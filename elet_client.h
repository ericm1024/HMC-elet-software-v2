#ifndef ELET_CLIENT_H
#define ELET_CLIENT_H

#include "elet.h"

// exit, possibly with a message
static void die(const char *reason, int err)
{
        if (reason) {
                fprintf(stderr, "%s: %s\n", reason, strerror(err));
                exit(1);
        } else {
                exit(0);
        }
}

static ssize_t
read_until_size_or_err(int socket, void *buf, size_t size)
{
        ssize_t ret = 0;
        size_t offset = 0;

        while (size > 0) {
                ret = read(socket, (uint8_t*)buf + offset, size);
                if (ret <= 0)
                        return ret < 0 ? -errno : 0;

                offset += ret;
                size -= ret;
        }

        return offset;
}

static ssize_t
write_until_size_or_err(int socket, const void *buf, size_t size)
{
        ssize_t ret = 0;
        size_t offset = 0;

        while (size > 0) {
                ret = write(socket, (uint8_t*)buf + offset, size);
                if (ret <= 0)
                        return ret < 0 ? -errno : 0;

                offset += ret;
                size -= ret;
        }

        return offset;
}

struct elet_client_ops {
        int (*on_data_rx)(struct data_packet *, void *);
        int (*on_msg_rx)(struct message_packet *, void *);
};

static uint32_t last_tx_seq = 1;
static uint32_t last_rx_seq = 0;

static void
__generic_on_data_rx(struct data_packet *pkt)
{
        // data, seq, solenoid states, ox pwm state, fuel pwm state
        // last ignition status, state, igniter good, ps1, ps2, t1, t2, thrust
        printf("data, %u, %x, %u, %u, %x, %x, %d, %f, %f, %f, %f, %u\n",
               pkt->hdr.seq,
               pkt->vlv_states,
               pkt->vlv_pwm_ox,
               pkt->vlv_pwm_fuel,
               pkt->state & 0x7,
               (pkt->state & 0x18) >> 3,
               (pkt->state & 0x2) >> 6,
               pkt->pressures[0],
               pkt->pressures[1],
               pkt->temps[0],
               pkt->temps[1],
               pkt->thrust);

        last_rx_seq = pkt->hdr.seq;
}

static void
__generic_on_message_rx(struct message_packet *pkt)
{
        printf("message,%s\n", pkt->data);
}

// returns what the function in ops function, otherwise 0
static int
client_rx_packet(int socket, struct elet_client_ops *ops, void *thunk)
{
        struct packet_header hdr;
        ssize_t ret;
	
        ret = read_until_size_or_err(socket, buf, ret);
        if (ret < 0)
                die("failed to read packet header", ret);

        if (hdr.type == PT_DATA) {
                struct data_packet pkt;

                memset(&pkt, 0, sizeof pkt);

                // XXX: the following is gross and assumes that we
                // alligned our struct correctly, which I think I did...
                size_t offset = sizeof hdr;
                size_t size = (sizeof pkt) - (sizeof hdr);
                
                memcpy(&pkt.hdr, &hdr, sizeof hdr);
                ret = read_until_size_or_err(socket,
                                             (uint8_t *)&pkt + offset,
                                             size);
                if (ret < 0)
                        die("failed to read data packet body", -ret);

                __generic_on_data_rx(&pkt);

                if (ops->on_data_rx)
                        return ops->on_data_rx(&pkt, thunk);
                else
                        return 0;
                
        } else if (hdr.type == PT_MESSAGE) {
                struct message_packet pkt;
                memset(&pkt, 0, sizeof pkt);
                
                // XXX: see above
                size_t offset = sizeof hdr;
                size_t size = (sizeof pkt) - (sizeof hdr);

                memcpy(&pkt.hdr, &hdr, sizeof hdr);
                ret = read_until_size_or_err(socket,
                                             (uint8_t*)&pkt + offset,
                                             size);
                if (ret < 0)
                        die("failed to read message packet body", -ret);

                __generic_on_msg_rx(&pkt);

                if (ops->on_msg_rx)
                        return ops->on_msg_rx(&pkt, thunk);
                else
                        return 0;

        } else
                die("invalid packet type");

        return 0;
}

static void
client_send_req(int socket, uint8_t cmd, uint32_t arg)
{
        struct req_packet pkt;

        memset(&pkt, 0, sizeof pkt);

        pkt.header.len = sizeof pkt;
        pkt.header.type = PT_REQ;
        pkt.header.seq = last_tx_seq++;

        pkt.cmd = cmd;
        pkt.arg = arg;

        ssize_t ret = write_until_size_or_err(socket, &pkt, sizeof pkt);
        if (ret < 0)
                die("failed to send req to server", -ret);
}

static int
client_wait_for_seq(struct data_packet *pkt, void *thunk)
{
        (void)pkt;
        (void)thunk;

        return last_rx_seq == last_tx_seq ? 0 : 1;
}

static void
__client_wait_for_cmd(int socket)
{
        struct elet_client_ops ops = {
                .on_data_rx = client_wait_for_seq,
                .on_msg_rx = NULL
        };

        int ret;

        do {
                ret = client_rx_packet(socket, &ops, NULL);
        } while (ret != 0);
}

static void
client_mod_valve(int socket, const enum valve v, uint8_t state)
{
        uint32_t arg = ((uint32_t)v & 0xff) | ((uint32_t)state << 8);

        if (!valve_is_flow(v) & state > 2)
                die("client_mod_valve: bad state", EINVAL);

        client_send_req(socket, REQ_MOD_VALVE, arg);

        __client_wait_for_cmd(socket);
}

static void
client_open_valve(int socket, enum valve v)
{
        if (valve_is_flow(v))
                client_mod_valve(socket, v, 255);
        else
                client_mod_valve(socket, v, 1);
}

static void
client_close_valve(int socket, enum valve v)
{
        client_mod_valve(socket, v, 0);

}

static void
client_open_valve_pwm(int socket, enum valve v, uint8_t pwm)
{
        if (!valve_is_flow(v))
                die("client_open_valve_pwm: bad valve", EINVAL);

        client_mod_valve(socket, v, pwm);
}

#endif // ELET_CLIENT_H
