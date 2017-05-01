#ifndef ELET_SERVER_H
#define ELET_SERVER_H

#include <Ethernet.h>

#include "elet_arduino.h"

// the state of the whole system
static enum system_state sys_state = SS_READY;

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
EthernetServer server(420);

static void
server_eth_setup()
{
        byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
        IPAddress ip(192, 168, 1, 100);

        Ethernet.begin(mac, ip);
        server.begin();
}

static uint32_t pkt_seq = 0;

// we read a packet in parts, since it might take some time to transmit, so
// we record the partial packet here.
struct rx_state {
        struct req_packet pkt;
        uint8_t *ptr = NULL;
        int nread;
};

static struct rx_state rx_state;

static void reset_rx_state()
{
        memset(&rx_state.pkt, 0, sizeof rx_state.pkt);
        rx_state.ptr = &rx_state.pkt;
        rx_state.nread = 0;
}

// I don't really know what to do when the client dies, but we're gonna call
// this function.
static void
handle_dead_client(EthernetClient *client)
{
        // XXX: do we want to call stop if the client is dead?
        client.flush();
        client->stop();
        reset_rx_state();
        pkt_seq = 0;
}

// send some diagnostic shenanigans to the client
static void
server_send_diag(EthernetClient *client, const char *msg)
{
        
}

// this function is the meat of the arduino code. Here he handle a REQ
// packet from the client.
static void
server_handle_req_packet()
{
        struct req_packet *pkt = &server_state.pkt;

        switch (pkt->cmd) {
        case REQ_CMD_STOP:
                // make sure we're actually in the right state
                if (sys_state != SS_FIRE)
                        
                break;

        case REQ_CMD_START:
                break;

        case REQ_MOD_VALVE:
                break;

        default:
                // XXX: the client sent us a command we don't know about.
                // Send a message back and give them the bird
        }
}

static void
server_rx_continue(EthernetClient *client)
{
        if (rx_state.ptr == NULL)
                reset_rx_state();
        
        uint16_t size = sizeof rx_state.pkt;
        uint16_t hsize = sizeof rx_state.pkt.hdr;
        
        while (client->available()) {
                char c = client->read();
                if (c == -1)
                        break;

                *rx_state.ptr++ = (uint8_t)c;
                rx_state.nread++;

                // we got the whole header, so validate it
                if (rx_state.nread == hsize) {
                        uint16_t len = rx_state.pkt.hdr.len;
                        uint8_t type = rx_state.pkt.hdr.type;

                        // the client sent us some bullshit, so close the
                        // connection
                        if (len != size || type != PT_REQ) {
                                handle_dead_client(client);
                                return;
                        }

                // we got a whole packet -- sick! let's process it
                } else if (rx_state.nread == size) {
                        server_handle_req_packet(&rx_state.pkt);
                        reset_rx_state(client);
                }
        }
}
        
// see the comments in struct data_packet to see what's going on here
static void
server_tx_all_data(EthernetClient *client)
{
        struct data_packet pkt;

        memset(&pkt, 0, sizeof pkt);
        
        pkt.hdr.len = sizeof pkt;
        pkt.hdr.type = PT_DATA;
        pkt.hdr.seq = pkt_seq;

        uint8_t vlv_states = 0;
        for (enum valve v = FIRST_VALVE; v < NR_VALVES; v = next_valve(v)) {
                if (valve_states[v] > 0)
                        vlv_states |= 1 << v;
        }
        pkt.vlv_states = vlv_states;
        pkt.vlv_pwm_ox = vlv_states[OX_FLOW];
        pkt.vlv_pwm_fuel = vlv_states[FUEL_FLOW];

        uint8_t state = 0;
        state |= (uint8_t)last_ign_status & 0x7;
        state |= ((uint8_t)sys_state & 0x7) << 3;

        if (sys_state == SS_READY) {
                int continuity = igniter_test_continuity();
                state |= continuity > 0 ? ((uint8_t)1 << 7) : 0;
        }
        pkt.state = state;

        for (enum pressure_sensor ps = FIRST_PSENSOR; ps < NR_PSENSORS;
             ps = next_pressure_sensor(ps)) {
                struct pressure_reading rd = read_pressure(ps);
                pkt.pressure[ps] = rd.analog;
        }

        for (enum thermocouple tc = FIRST_THERMOCOUPLE; tc < NR_THERMOCOUPLES;
             tc = next_thermocouple(tc)) {
                pkt.temps = 0.0; // XXX: fix thermocuple circuitry
        }

        pkt.thrust = read_load_cell();

        unsigned sent = 0;
        unsigned remaining = sizeof pkt;
        while (client->connected() && remaining != 0) {
                unsigned ret = client->write((uint8_t*)&pkt + sent,
                                             remaining);
                remaining -= ret;
                sent += ret;
        }

        // oops, we failed to transmit an entire packet because the client
        // died. Try to do something sensible.
        if (remaining != 0)
                handle_dead_client(client);
}

// top-level function to be called from loop()
static void
server_do_tx_rx()
{
        EthernetClient client = server.available();
        if (!client)
                return;

        // uhh not really sure what to do here
        if (!client.connected()) {
                handle_dead_client(&client);
                return;
        }

        server_rx_continue(&client);
        server_tx_all_data(&client);
}

#endif // ELET_SERVER_H
