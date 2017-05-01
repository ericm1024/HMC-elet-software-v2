#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/ip.h>

#include "../elet.h"

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

static uint32_t process_packet(const uint8_t *pkt, int logfd)
{
        struct packet_header *hdr = (struct packet_header *)pkt;
        uint32_t seq = hdr->seq;

        if (hdr->type == PT_DATA) {
                struct data_packet *dpkt = (struct data_packet *)pkt;
                if (hdr->len != sizeof *dpkt) {
                        fprintf(stderr, "%s: bad data header len %hu\n",
                                __func__, hdr->len);
                        goto die_bad_packet;
                }

                // data, seq, solenoid states, ox pwm state, fuel pwm state,
                // last ignition status, state, igniter good, ps1, ps2, t1,
                // t2, thrust
                dprintf(logfd,
                        "data,%u,%x,%u,%u,%x,%x,%d,%f,%f,%f,%f,%u\n",
                        seq,
                        dpkt->vlv_states,
                        dpkt->vlv_pwm_ox,
                        dpkt->vlv_pwm_fuel,
                        dpkt->state & 0x7,
                        (dpkt->state & 0x18) >> 3,
                        (dpkt->state & 0x2) >> 6,
                        dpkt->pressures[0],
                        dpkt->pressures[1],
                        dpkt->temps[0],
                        dpkt->temps[1],
                        dpkt->thrust);

        } else if (hdr->type == PT_MESSAGE) {
                struct message_packet *mpkt = (struct message_packet *)pkt;
                if (hdr->len != sizeof *mpkt) {
                        fprintf(stderr, "%s: bad message header len %hu\n",
                                __func__, hdr->len);
                        goto die_bad_packet;
                }

                dprintf(logfd, "message,%u,%s\n", seq, mpkt->data);

        } else {
                fprintf(stderr, "%s: invalid packet type %x\n", __func__,
                        hdr->type);

                goto die_bad_packet;
        }

        return seq;

die_bad_packet:
        die("bad packet", EINVAL);
        return -1U;
}

static uint32_t process_command(const char *buf, size_t size,
                                uint32_t seq, int sd)
{
        uint32_t sent_seq = -1;
        struct req_packet pkt;

        memset(&pkt, 0, sizeof pkt);
        //pkt.hdr

        // start the engine
        if (strncmp(buf, "start-the-engine-everyone-is-safe", size) == 0) {


        // stop the engine
        } else if (strncmp(buf, "stop", size) == 0) {

        // valve manipulation
        } else if (strncmp(buf, "v ", 2) == 0) {
                buf += 2;
                
        } else {
                fprintf(stderr, "invalid command, resetting buffer\n");
        }

        return sent_seq;
}

int main(int argc, char **argv)
{
        int err, sd, flags, ret, logfd;
        struct sockaddr_in addr;

        (void)argc;
        (void)argv;

        // open a logfile
        logfd = open("run.log", O_CREAT|O_RDWR|O_APPEND, S_IRUSR|S_IWUSR);
        if (logfd == -1)
                die("failed to open logfd", errno);

        // grab a socket
        sd = socket(AF_INET, SOCK_STREAM, 0);
        if (sd == -1)
                die("socket failed", errno);

        // define the server address
        memset(&addr, 0, sizeof addr);
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl((192 << 24) | (168 << 16)
                                     | (1 << 8) | 100);
        addr.sin_port = htons(420);

        // connect to the server (arduino)
        err = connect(sd, (struct sockaddr *)&addr, sizeof addr);
        if (err == -1)
                die("connect failed", errno);

        // set socket as non-blocking
        flags = fcntl(sd, F_GETFL);
        if (flags == -1)
                die("fcntl(sd, F_GETFL) failed", errno);

        err = fcntl(sd, F_SETFL, flags|O_NONBLOCK);
        if (err == -1)
                die("fcntl(sd, F_SETFL) failed", errno);

        // set stdin as non-blocking
        flags = fcntl(STDIN_FILENO, F_GETFL);
        if (flags == -1)
                die("fcntl(stdin, F_GETFL) failed", errno);

        err = fcntl(STDIN_FILENO, F_SETFL, flags|O_NONBLOCK);
        if (err == -1)
                die("fcntl(stdin, F_SETFL) failed", errno);

        // setup buffers. one for reading from socket, one for reading from
        // command line
        const size_t bsize = 1024;
        uint8_t *pkt_buf = calloc(1, bsize);
        char *cmd_buf = calloc(1, bsize);
        if (!pkt_buf || !cmd_buf)
                die("calloc failed", ENOMEM);

        // buffer indexes/sizes. idx's are the index of the first freee byte
        // in the buffer; space's are the space left in each buffer.
        size_t pkt_idx = 0;
        size_t pkt_space = bsize;
        size_t cmd_idx = 0;
        size_t cmd_space = bsize;

        struct packet_header *hdr = NULL;

        uint32_t seq_acked = -1;
        uint32_t seq_sent = 0;

        // if we don't here from the server for 2 seconds, something bad
        // happened. (this won't necessarily catch death if someone is
        // typing things on the command line constantly.
        const int timeout_ms = 2000;

        for (;;) {
                const short events = POLLIN | POLLPRI;
                const short bad_revents = POLLERR | POLLHUP | POLLNVAL;

                struct pollfd fds[] = {
                        {
                                .fd = STDIN_FILENO,
                                .events = events,
                                .revents = 0
                        },
                        {
                                .fd = sd,
                                .events = events,
                                .revents = 0
                        }
                };
                
                ret = poll(fds, (sizeof fds)/(sizeof fds[0]), timeout_ms);
                if (ret == 0)
                        die("poll timeout exceeded!", EIO);

                // something happend on stdin!
                if (fds[0].revents & (events|bad_revents)) {
                        if (fds[0].revents & bad_revents)
                                die("something bad happened on stdin", EIO);

                        // probably will never happen, but it's possible
                        // if the user is a monkey
                        if (cmd_space == 0)
                                die("command buffer full", ENOMEM);

                        // otherwise we have data
                        ssize_t ret = read(STDIN_FILENO, cmd_buf + cmd_idx,
                                           cmd_space);
                        if (ret == -1)
                                // this should never be EAGAIN since we
                                // marked stdin as non-blocking
                                die("failed to read from stdin", errno);

                        cmd_idx += ret;
                        cmd_space -= ret;

                        // look for a newline and process a whole command
                        // if we got one
                        for (size_t i = cmd_idx - ret; i < cmd_idx; ++i) {
                                if (cmd_buf[i] != '\n')
                                        continue;

                                // okay we got a newline--process the command
                                uint32_t s = process_command(cmd_buf,
                                                             seq_sent + 1,
                                                             i, sd);
                                if (s != -1U)
                                        seq_sent = s;

                                // skip past the newline char so i now
                                // indexes the first byte of the next
                                // command, or more likely the first free
                                // byte 
                                ++i;

                                // move any remaining unprocessed command
                                // back to the beginning of the line
                                memmove(cmd_buf, cmd_buf + i, cmd_idx - i);

                                // zero the rest of the buffer
                                memset(cmd_buf + (cmd_idx - i), 0,
                                       bsize - (cmd_idx - i));

                                // reset indices
                                pkt_idx -= i;
                                pkt_space += i;

                                break;
                        }
                }

                // something happened on our socket!
                if (fds[1].revents & (events|bad_revents)) {
                        if (fds[1].revents & bad_revents)
                                die("something bad happened on socket", EIO);

                        // this shouldn't happen unless my code is buggy
                        if (cmd_space == 0)
                                die("packet buffer full", ENOMEM);

                        ssize_t ret = read(sd, pkt_buf + pkt_idx, pkt_space);
                        if (ret == -1)
                                die("failed to read from socket", errno);

                        pkt_idx += ret;
                        pkt_space -= ret;

                        if (pkt_idx >= sizeof *hdr)
                                // this technically violates strict aliasing,
                                // but we allocated pkt_buf with calloc,
                                // it's sufficiently alligned
                                hdr = (struct packet_header *)pkt_buf;

                        if (hdr) {
                                uint16_t len = hdr->len;

                                // we read this whole packet
                                if (pkt_idx >= len) {
                                        uint32_t s = process_packet(pkt_buf,
                                                                    logfd);

                                        // uh-oh, the arduino sent back
                                        // a seq number less than what
                                        // we've seen so far: this probably
                                        // means it reset. This is probs bad
                                        if (s < seq_acked) {
                                                fprintf(stderr,
                                                        "arduino reset!\n");
                                        }

                                        seq_acked = s;

                                        memmove(pkt_buf, pkt_buf + len,
                                                bsize - len);
                                        
                                        memset(pkt_buf + (pkt_idx - len), 0,
                                               bsize - (pkt_idx - len));

                                        hdr = NULL;
                                        pkt_idx -= len;
                                        pkt_space += len;
                                }
                        }
                }
        }
}
