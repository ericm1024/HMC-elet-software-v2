#include "elet_arduino.h"

static EthernetServer server(ELET_NET_PORT);

static enum system_state sys_state = SS_READY;
static unsigned long state_start_ms = 0;

static uint32_t pkt_seq = 0;

static struct data_packet data_pkt;

// we read a packet in parts, since it might take some time to transmit, so
// we record the partial packet here.
struct rx_state {
        uint8_t buf[sizeof (struct req_packet)];
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
        rx_state.nread = 0;
}

void setup()
{
        Serial.begin(9600);

        server_eth_setup();
        setup_all_valves();
        setup_igniter();
        reset_rx_state();

        memset(&data_pkt, 0, sizeof data_pkt);
        data_pkt.header.len = sizeof data_pkt;
        data_pkt.header.type = PT_DATA;

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

        Serial.println("finished handling dead client");
}

static void
update_sys_state(enum system_state ss)
{
        sys_state = ss;
        state_start_ms = millis();

        Serial.print("updating system state to ");
        Serial.println(sys_state);
}

static void send_packet(EthernetClient *client, const void *_pkt, unsigned len)
{
        unsigned sent = 0;
        unsigned remaining = len;
        const uint8_t *pkt = (const uint8_t *)_pkt;

        while (remaining != 0) {
                unsigned ret = client->write(pkt + sent, remaining);
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


// close n2 on off
// close fuel on off
// open nitrogen purge
//
// wait 1 second
// close ox on off
// close ox flow
//
// wait 1 seconds
// open ox bleed
//
// wait 3 seconds
// close nitrogen purge
//
// wait 7 seconds
// close ox bleed
// close fuel flow
static void continue_safing(unsigned long time)
{
        static unsigned long state_start = 0;
        static int safing_state = 0;

        unsigned long time_this_state = state_start == 0 ? time
                : time - state_start;

        switch (safing_state) {
        case 0:
                close_valve(N2_ON_OFF);
                close_valve(FUEL_FLOW);
                open_valve(N2_PURGE);
                goto out_next_safing_state;

        case 1:
                if (time_this_state < 1000)
                        return;
                close_valve(OX_ON_OFF);
                close_valve(OX_FLOW);
                goto out_next_safing_state;

        case 2:
                if (time_this_state < 1000)
                        return;
                open_valve(OX_BLEED);
                goto out_next_safing_state;

        case 3:
                if (time_this_state < 3000)
                        return;
                close_valve(N2_PURGE);
                goto out_next_safing_state;

        case 4:
                if (time_this_state < 7000)
                        return;
                close_valve(OX_BLEED);
                close_valve(FUEL_FLOW);
                goto out_end_state;

        default:
                Serial.println("got weird safing_state");
                goto out_end_state;
        }

out_next_safing_state:
        ++safing_state;
        state_start = time;
        return;
        
out_end_state:
        state_start = 0;
        safing_state = 0;
        update_sys_state(SS_READY);
}

static unsigned long fire_timeout;

// test igniter
//
// open n2 on off
// wait 5 seconds
// 
// open ox flow
// open nitrogen purge
// 
// wait 1 second
// open ox on off
//
// wait 1 second
// open fuel on-off
//
// wait 1 second
// open fuel flow
//
// wait 1 second
// close nitrogen purge
//
// wait 1 second
// fire igniter
//
// wait 1 second
// test success
// open ox flow
// open fuel flow
static void continue_fire(unsigned long time)
{
        static unsigned long state_start = 0;
        static int fire_state = 0;
        enum system_state next_state = SS_NUM_STATES;
        int continuity;

        unsigned long time_this_state = state_start == 0 ? time
                : time - state_start;

        switch (fire_state) {
        // step 0: close all valves, test the igniter
        case 0:
                for (enum valve v = FIRST_VALVE; v < NR_VALVES; v = next_valve(v))
                        if (valve_states[v] != 0)
                                close_valve(v);

                // test the ignition sensor to make sure its present
                /*
                continuity = analogRead(sys_igniter.ignition_sense);
                if (continuity == 0) {
                        last_ign_status = IGN_FAIL_NO_ISENSE_WIRE;
                        next_state = SS_READY;
                        goto out_end_state;
                }
                */

                continuity = igniter_test_continuity();

                if (continuity == 0) {
                        last_ign_status = IGN_FAIL_BAD_IGNITER;
                        next_state = SS_READY;
                        goto out_end_state;
                }
                break;

        case 1:
                open_valve(N2_ON_OFF);
                break;
                
        case 2:
                if (time_this_state < 5000)
                        return;

                open_valve_to(OX_FLOW, 119);
                open_valve(N2_PURGE);
                break;

        case 3:
                if (time_this_state < 1000)
                        return;

                open_valve(OX_ON_OFF);
                break;

        case 4:
                if (time_this_state < 1000)
                        return;

                open_valve(FUEL_ON_OFF);
                break;

        case 5:
                if (time_this_state < 1000)
                        return;

                open_valve_to(FUEL_FLOW, 110);
                break;

        case 6:
                if (time_this_state < 1000)
                        return;

                close_valve(N2_PURGE);

        case 7:
                if (time_this_state < 1000)
                        return;

                // fire the igniter. Don't give up the CPU here because
                // we really want to turn this off quickly
                digitalWrite(sys_igniter.igniter_fire_ctl_be_careful, HIGH);
                delay(100);
                digitalWrite(sys_igniter.igniter_fire_ctl_be_careful, LOW);
                break;

        case 8:
                // "it doesn't have to be precise, it's all bullshit anyways"
                // - Mike Chaffee, our lord and savior Jeezaus
                if (time_this_state < 200)
                        return;

                /*
                continuity = analogRead(sys_igniter.ignition_sense);
                if (continuity == 0) {
                        // open the flow control valves to full bore
                        open_valve(OX_FLOW);
                        open_valve(FUEL_FLOW);
                        last_ign_status = IGN_SUCCESS;
                        goto out_next_fire_state;
                } else {
                        // uh-oh: ignition failed
                        next_state = SS_SAFING;
                        last_ign_status = IGN_FAIL_NO_IGNITION;
                        goto out_end_state;
                }
                */

                open_valve(OX_FLOW);
                open_valve(FUEL_FLOW);
                last_ign_status = IGN_SUCCESS;
                goto out_next_fire_state;

        case 9:
                if (time_this_state < fire_timeout)
                        return;
                next_state = SS_SAFING;
                goto out_end_state;

        default:
                Serial.println("got weird fire_state");
                goto out_end_state;
        }

out_next_fire_state:
        ++fire_state;
        state_start = time;
        return;
        
out_end_state:
        state_start = 0;
        fire_state = 0;
        update_sys_state(next_state);
}

static unsigned long depress_timeout;

static void continue_depress(unsigned long time)
{
        static unsigned long state_start = 0;
        static int depress_state = 0;

        unsigned long time_this_state = state_start == 0 ? time
                : time - state_start;

        switch (depress_state) {
        // step 0: open the fuel system
        case 0:
                open_valve(N2_ON_OFF);
                open_valve(FUEL_ON_OFF);
                open_valve(FUEL_FLOW);
                goto out_next_depress_state;

        // step 1: stop the nitrogen feed system after the specified timeout
        case 1:
                if (time_this_state < depress_timeout)
                        return;

                close_valve(N2_ON_OFF);
                goto out_next_depress_state;

        // step 2: keep the fuel valves open for 30 seconds, then close then
        // and start a nitrogen purge
        case 2:
                if (time_this_state < 30000)
                        return;

                close_valve(FUEL_ON_OFF);
                close_valve(FUEL_FLOW);
                open_valve(N2_PURGE);
                goto out_next_depress_state;

        // step 3: end the nitrogen purge after 5 seconds.
        case 3:
                if (time_this_state < 5000)
                        return;

                close_valve(N2_PURGE);
                goto out_end_state;
        }

out_next_depress_state:
        ++depress_state;
        state_start = time;
        return;
        
out_end_state:
        state_start = 0;
        depress_state = 0;
        update_sys_state(SS_READY);
}

// this function is the meat of the arduino code. Here he handle a REQ
// packet from the client.
static bool handle_req_packet(struct req_packet *pkt, EthernetClient *client)
{
        uint8_t valve;
        uint8_t val;

        Serial.println("handle_req_packet");
        
        switch (pkt->cmd) {
        case REQ_CMD_STOP:
                // make sure we're actually in the right state
                if (sys_state != SS_FIRE && sys_state != SS_READY)
                        goto the_default_is_to_yell;

                update_sys_state(SS_SAFING);
                break;

        case REQ_CMD_START:
                if (sys_state != SS_READY)
                        goto the_default_is_to_yell;

                if (pkt->arg < REQ_CMD_START_MIN_BURN_TIME
                    || pkt->arg > REQ_CMD_START_MAX_BURN_TIME)
                        goto the_default_is_to_yell;

                update_sys_state(SS_FIRE);
                fire_timeout = pkt->arg * 1000UL;
                break;

        case REQ_MOD_VALVE:
                if (sys_state != SS_READY)
                        goto the_default_is_to_yell;

                valve = pkt->arg & 0xff;
                val = (pkt->arg & 0xff00) >> 8;

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
                } else {
                        goto the_default_is_to_yell;
                }
                break;

        case REQ_CMD_DEPRESS:
                if (sys_state != SS_READY)
                        goto the_default_is_to_yell;

                if (pkt->arg < REQ_CMD_DEPRESS_MIN_TIMEOUT
                    || pkt->arg > REQ_CMD_DEPRESS_MAX_TIMEOUT)
                        goto the_default_is_to_yell;

                update_sys_state(SS_DEPRESS);
                depress_timeout = pkt->arg * 1000UL;
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
                mpkt.header.timestamp = millis();
                strcpy((char*)mpkt.data, "processed bad command");

                send_packet(client, &mpkt, sizeof mpkt);
                return false;
        }

        return true;
}

static void rx_continue(EthernetClient *client)
{
        struct packet_header *hdr = (struct packet_header *)rx_state.buf;
        uint16_t hsize = sizeof *hdr;
        uint16_t avail = client->available();
        uint16_t toread = min((sizeof rx_state.buf) - rx_state.nread,
                              avail);

        // XXX: don't call this function in this case
        if (avail == 0)
                return;

        int ret = client->read(rx_state.buf + rx_state.nread, toread);
        if (ret == -1) {
                handle_dead_client(client);
                return;
        }

        rx_state.nread += ret;

        // we got at least a header, so validate it
        if (rx_state.nread >= hsize) {          
                uint16_t len = hdr->len;
                uint8_t type = hdr->type;

                // the client sent us some bullshit, so close the
                // connection
                if ((type != PT_REQ && type != PT_HELLO) ||
                    (len != sizeof(struct req_packet)
                     && len != sizeof(struct hello_packet))) {
                        handle_dead_client(client);
                        return;
                }

                // we got a whole packet -- sick! let's process it
                if (rx_state.nread == len) {                  
                        // XXX: packet CRCs
                        // validate_pkt_ctc(&rx_state.pkt);

                        // PT_HELLO packets have nothing in them, just update
                        // the last seq
                        if (type == PT_HELLO) {
                                pkt_seq = hdr->seq;
                        } else {
                                bool success = handle_req_packet((struct req_packet *)rx_state.buf, client);
                                if (success)
                                        pkt_seq = hdr->seq;
                        }
                        reset_rx_state();

                // too much!!
                } else if (rx_state.nread > len)
                        handle_dead_client(client);
        }
}

static void gather_all_data()
{
        // fill in header
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

        data_pkt.state = state;

        // fill in pressure sensor data
        for (enum pressure_sensor ps = FIRST_PSENSOR; ps < NR_PSENSORS;
             ps = next_pressure_sensor(ps)) {
                struct pressure_reading rd = read_pressure(ps);
                data_pkt.pressures[ps] = rd.digital;
        }

        // fill in thermocouple readings. The OX thermo is borked, so don't
        // bother
        data_pkt.temps[TC_WATER] = 0.0;
        data_pkt.temps[TC_OXYGEN] = 0.0;

        data_pkt.thrust = read_load_cell();
}

static EthernetClient client;
static int loop_count = 0;

void loop()
{
        gather_all_data();

        // only re-try grabbing a client after a while, since it's
        // expensive
        if (++loop_count == 32) {

                loop_count = 0;

                // only get new clinents in the SS_READY state because
                // server.available() can be an expensive operation and
                // we have other time-critical code
                if (!client && sys_state == SS_READY) {
                        client = server.available();
                }
        }

        if (client.connected()) {
                // receive and possibly process an incoming packet
                rx_continue(&client);
        
                // transmit data from all sensors
                send_packet(&client, &data_pkt, sizeof data_pkt);
        } else if (client) {
                handle_dead_client(&client);
        }
       
        unsigned long now = millis();
        switch (sys_state) {
        case SS_FIRE:
                continue_fire(now);
                break;
                
        case SS_SAFING:
                continue_safing(now);
                break;

        case SS_DEPRESS:
                continue_depress(now);
                break;

        default:
                break;
        }
}
