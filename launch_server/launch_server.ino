#include "elet_arduino.h"

static EthernetServer server(ELET_NET_PORT);

static enum system_state sys_state = SS_READY;

static uint32_t pkt_seq = 0;

// we read a packet in parts, since it might take some time to transmit, so
// we record the partial packet here.
struct rx_state {
        struct req_packet pkt;
        uint8_t *ptr = NULL;
        size_t nread;
};

static struct rx_state rx_state;

static void server_eth_setup()
{
        byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
        IPAddress ip((ELET_NET_ADDR & (0xffUL << 24)) >> 24,
                     (ELET_NET_ADDR & (0xffUL << 16)) >> 16,
                     (ELET_NET_ADDR & (0xffUL << 8)) >> 8,
                      ELET_NET_ADDR & (0xffUL));

        Ethernet.begin(mac, ip);
        server.begin();
}

static void reset_rx_state()
{
        memset(&rx_state.pkt, 0, sizeof rx_state.pkt);
        rx_state.ptr = (uint8_t*)&rx_state.pkt;
        rx_state.nread = 0;
}

void setup()
{
        Serial.begin(9600);

        server_eth_setup();
        setup_all_valves();
        setup_igniter();
        reset_rx_state();

        Serial.println("#################################################################");
        Serial.println("######################### LAUNCH SERVER #########################");
        Serial.println("#################################################################");

        // XXX: what do we do when we don't want a serial console?
}

// I don't really know what to do when the client dies, but we're gonna call
// this function.
static void
handle_dead_client(EthernetClient *client)
{
        Serial.println("handling dead client");
        
        // XXX: do we want to call stop if the client is dead?
        client->flush();
        client->stop();
        reset_rx_state();
        pkt_seq = 0;
}

static void send_packet(EthernetClient *client, const void *_pkt, unsigned len)
{
        unsigned sent = 0;
        unsigned remaining = len;
        const uint8_t *pkt = (const uint8_t *)_pkt;

        /*
        while (client->connected() && remaining != 0) {
                unsigned ret = client->write(pkt + sent, remaining);
                remaining -= ret;
                sent += ret;
        }
        */
        while (remaining != 0) {
                unsigned ret = server.write(pkt + sent, remaining);
                remaining -= ret;
                sent += ret;
        }

        // oops, we failed to transmit an entire packet because the client
        // died. Try to do something sensible.
        if (remaining != 0) {
                Serial.println("Failed to send entire packet");
                handle_dead_client(client);
        }
}

static unsigned long last_fire_start;
static unsigned long fire_timeout;

static void stop_engine()
{
        Serial.println("stopping engine");

        // close the oxygen line
        // XXX: do we want any delays here?
        close_valve(OX_FLOW);
        close_valve(OX_ON_OFF);

        // close the fuel line
        close_valve(FUEL_FLOW);
        close_valve(FUEL_ON_OFF);

        // purge the engine with nitrogen
        open_valve(N2_PURGE);
        delay(5000);
        close_valve(N2_PURGE);

        // bleed the oxygen system
        delay(5000);
        open_valve(OX_BLEED);
        delay(10000);
        close_valve(OX_BLEED);
        delay(5000);

        // XXX: some other shit here?
}

static enum ignition_status start_engine()
{
        for (enum valve v = FIRST_VALVE; v < NR_VALVES; v = next_valve(v))
                if (valve_states[v] != 0)
                        close_valve(v);

        Serial.println("starting engine");

        enum ignition_status status = IGN_NUM_STATUSES;

        int continuity;

        // test the ignition sensor to make sure its present
        continuity = analogRead(sys_igniter.ignition_sense);
        if (continuity == 0) {
                status = IGN_FAIL_NO_ISENSE_WIRE;
                goto out;
        }

        continuity = igniter_test_continuity();
        if (continuity == 0) {
                last_ign_status = IGN_FAIL_BAD_IGNITER;
                goto out;
        }

        // open the flow control valves a tiny bit
        // XXX: chage these values
        open_valve_to(OX_FLOW, 110);
        open_valve_to(FUEL_FLOW, 110);

        // pressurize the fuel
        open_valve(N2_ON_OFF);
        delay(100);

        // open the fuel and oxygen feed valves
        open_valve(OX_ON_OFF);
        open_valve(FUEL_ON_OFF);
        delay(500);

        // fire the igniter
        digitalWrite(sys_igniter.igniter_fire_ctl_be_careful, HIGH);
        delay(1000); // XXX: change me
        digitalWrite(sys_igniter.igniter_fire_ctl_be_careful, LOW);

        // wait for the ignition sense wire to melt
        delay(3000); // XXX: change me
        continuity = analogRead(sys_igniter.ignition_sense);
        if (continuity == 0) {
                // open the flow control valves to full bore
                open_valve(OX_FLOW);
                open_valve(FUEL_FLOW);
                status = IGN_SUCCESS;
                last_fire_start = millis();
        } else {
                // uh-oh: ignition failed
                stop_engine();
                status = IGN_FAIL_NO_IGNITION;
        }

out:
        last_ign_status = status;
        return status;
}

// this function is the meat of the arduino code. Here he handle a REQ
// packet from the client.
static void handle_req_packet(struct req_packet *pkt, EthernetClient *client)
{
        uint8_t valve;
        uint8_t val;

        Serial.println("handle_req_packet");
        
        switch (pkt->cmd) {
        case REQ_CMD_STOP:
                
                // make sure we're actually in the right state
                if (sys_state != SS_FIRE)
                        goto the_default_is_to_yell;

                sys_state = SS_SAFING;
                stop_engine();
                sys_state = SS_READY;
                ++pkt_seq;
                break;

        case REQ_CMD_START:
                if (sys_state != SS_READY)
                        goto the_default_is_to_yell;

                if (pkt->arg < REQ_CMD_START_MIN_BURN_TIME
                    || pkt->arg > REQ_CMD_START_MAX_BURN_TIME)
                        goto the_default_is_to_yell;

                sys_state = SS_FIRE;
                fire_timeout = pkt->arg * 1000UL;
                start_engine();
                ++pkt_seq;
                break;

        case REQ_MOD_VALVE:
                if (sys_state != SS_READY)
                        goto the_default_is_to_yell;

                valve = pkt->arg & 0xff;
                val = (pkt->arg & 0xff00) >> 8;

                Serial.print("Got REQ_MOD_VALVE: valve=");
                Serial.print(valve);
                Serial.print(", val=");
                Serial.println(val);

                if (valve == 0xff) {
                        for (enum valve v = FIRST_VALVE; v < NR_VALVES;
                             v = next_valve(v))
                                close_valve(v);

                } else if (valve < NR_VALVES) {
                        enum valve v = (enum valve)valve;
                        if (!valve_is_flow(v) && val != 0 && val != 1)
                                goto the_default_is_to_yell;

                        if (valve_is_flow(v)) {
                                open_valve_to(v, val);
                        } else {
                                if (val == 1)
                                        open_valve(v);
                                else
                                        close_valve(v);
                        }
                }
                ++pkt_seq;
                break;

        the_default_is_to_yell:
        default:
                ;

                // XXX: the client sent us a command we don't know about.
                // Send a message back and give them the bird
                struct message_packet mpkt;
                memset(&mpkt.header, 0, sizeof mpkt.header);

                mpkt.header.len = sizeof mpkt;
                mpkt.header.type = PT_MESSAGE;
                mpkt.header.seq = pkt_seq;
                strcpy((char*)mpkt.data, "processed bad command");

                send_packet(client, &mpkt, sizeof mpkt);
        }
}

static void rx_continue(EthernetClient *client)
{
        uint16_t size = sizeof rx_state.pkt;
        uint16_t hsize = sizeof rx_state.pkt.header;
        
        while (client->available()) {
                char c = client->read();
                if (c == -1)
                        break;

                *rx_state.ptr++ = (uint8_t)c;
                rx_state.nread++;

                // we got the whole header, so validate it
                if (rx_state.nread == hsize) {
                        uint16_t len = rx_state.pkt.header.len;
                        uint8_t type = rx_state.pkt.header.type;

                        // the client sent us some bullshit, so close the
                        // connection
                        if (len != size || type != PT_REQ) {
                                Serial.println("got bad header len or type");
                                handle_dead_client(client);
                                return;
                        }

                // we got a whole packet -- sick! let's process it
                } else if (rx_state.nread == size) {
                        // XXX: packet CRCs
                        // validate_pkt_ctc(&rx_state.pkt);
                        handle_req_packet(&rx_state.pkt, client);
                        reset_rx_state();

                        // process at most one packet at a time, so
                        // return here
                        return;
                }
        }
}

static struct data_packet data_pkt;

static void gather_all_data()
{
        memset(&data_pkt, 0, sizeof data_pkt);

        // fill in header
        data_pkt.header.len = sizeof data_pkt;
        data_pkt.header.type = PT_DATA;
        data_pkt.header.seq = pkt_seq;
        data_pkt.header.timestamp = millis();

        // fill in valve states
        uint8_t vlv_states = 0;
        for (enum valve v = FIRST_VALVE; v < NR_VALVES; v = next_valve(v))
                if (valve_states[v] > 0)
                        vlv_states |= 1 << v;

        data_pkt.vlv_states = vlv_states;
        data_pkt.vlv_pwm_ox = valve_states[OX_FLOW];
        data_pkt.vlv_pwm_fuel = valve_states[FUEL_FLOW];

        // fill in system state
        uint8_t state = 0;
        state |= (uint8_t)last_ign_status & 0x7;
        state |= ((uint8_t)sys_state & 0x7) << 3;

        // we only guarentee a valid igniter state in the SS_READY
        if (sys_state == SS_READY) {
                int continuity = igniter_test_continuity();
                state |= continuity > 0 ? ((uint8_t)1 << 6) : 0;
        }
        data_pkt.state = state;

        // fill in pressure sensor data
        for (enum pressure_sensor ps = FIRST_PSENSOR; ps < NR_PSENSORS;
             ps = next_pressure_sensor(ps)) {
                struct pressure_reading rd = read_pressure(ps);
                data_pkt.pressures[ps] = rd.digital;
        }

        // fill in thermocouple readings
        for (enum thermocouple tc = FIRST_THERMOCOUPLE;
             tc < NR_THERMOCOUPLES;
             tc = next_thermocouple(tc))
                data_pkt.temps[tc] = read_thermocouple_f(tc);

        data_pkt.thrust = read_load_cell_calibrated();
}

static EthernetClient client;
static unsigned long loop_count = 0;

void loop()
{
        ++loop_count;

        if (loop_count % 20 == 0 && !client)
                client = server.available();

        gather_all_data();

        //Serial.println("gathered all data");

        if (client.connected()) {
                Serial.println("got connected client");
          
                // receive and possibly process an incoming packet
                rx_continue(&client);
        }

        // transmit data from all sensors
        send_packet(&client, &data_pkt, sizeof data_pkt);

        if (sys_state == SS_FIRE && millis() > last_fire_start + fire_timeout) {
                stop_engine();
                sys_state = SS_READY;       
        }
}
