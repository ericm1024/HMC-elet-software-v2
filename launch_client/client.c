#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/ip.h>

#include "../elet.h"

// exit, possibly with a message
static void __attribute__((noreturn)) die(const char *reason, int err)
{
        if (reason) {
                fprintf(stderr, "%s: %s\n", reason, strerror(err));
                exit(1);
        } else {
                exit(0);
        }
}

static uint32_t process_packet(const uint8_t *pkt, int logfd,
                               enum system_state *sys_state)
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

                // we expect this bit to be zero
                if (dpkt->state & 0x80)
                        fprintf(stderr, "BAD: corrupted bit in dpkt->state\n", EIO);
                
                *sys_state = (enum system_state)
                        ((dpkt->state & (0x7 << 3)) >> 3);

                // data, timestamp, seq, solenoid states, ox pwm state, fuel pwm state,
                // last ignition status, state, igniter good, ps1, ps2, t1,
                // t2, thrust.
                // 
                // See comments in struct data_packet for bit twiddling
                // explanation.
                dprintf(logfd,
                        "data, %u, %u, 0x%x, %u, %u, 0x%x, 0x%x, %d, %hu, %hu, %f, %f, %f\n",
                        dpkt->header.timestamp,
                        seq,
                        dpkt->vlv_states,
                        dpkt->vlv_pwm_ox,
                        dpkt->vlv_pwm_fuel,
                        dpkt->state & 0x7,
                        (dpkt->state & (0x7 << 3)) >> 3, 
                        (dpkt->state & (0x1 << 6)) >> 6,
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

                dprintf(logfd, "message, %u, %u, %s\n",
                        mpkt->header.timestamp, seq, mpkt->data);

        } else {
                fprintf(stderr, "%s: invalid packet type %x\n", __func__,
                        hdr->type);

                goto die_bad_packet;
        }

        return seq;

die_bad_packet:
        die("bad packet", EINVAL);
}

static uint32_t process_command(const char *buf, size_t size,
                                uint32_t last_seq,
                                enum system_state sys_state, int sd)
{
        uint32_t sent_seq = last_seq + 1;
        struct req_packet pkt;

        memset(&pkt, 0, sizeof pkt);
        pkt.header.len = sizeof pkt;
        pkt.header.type = PT_REQ;
        pkt.header.seq = sent_seq;

        // start the engine
        const char *cmd = "start-the-damn-engine";
        size_t len = strlen(cmd);
        if (strncmp(buf, cmd, len) == 0) {
                buf += len;

                // parse the burn-time argument
                char *end = NULL;
                errno = 0;
                long t = strtol(buf, &end, 10);
                if (errno)
                        goto bad_command;

                // the burn time argument should take up the rest of the
                // command, so yell if we don't find a null byte at the end
                if (*end != '\0')
                        goto bad_command;

                // make sure we're with our defined min/max burn time
                if (t < REQ_CMD_START_MIN_BURN_TIME
                    || t > REQ_CMD_START_MAX_BURN_TIME)
                        goto bad_command;

                // make sure we can fit into a uint32_t, even if the
                // above constants get borked
                if (t < 0 || t > 0xffffffffLL)
                        goto bad_command;

                pkt.cmd = REQ_CMD_START;

                // this cast is safe becasuse of the above check
                pkt.arg = (uint32_t)t;

                fprintf(stderr, "starting the engine\n");

                goto send_pkt;

        // stop the engine
        } else if (strcmp(buf, "stop") == 0) {
                pkt.cmd = REQ_CMD_STOP;

                fprintf(stderr, "stopping the engine\n");

                goto send_pkt;

        // valve manipulation
        } else if (strncmp(buf, "v ", 2) == 0) {

                if (sys_state != SS_READY) {
                        fprintf(stderr,
                                "%s: attempt to actuate valve while engine is running\n",
                                __func__);
                        goto bad_command;
                }
                
                buf += 2;
                pkt.cmd = REQ_MOD_VALVE;

                if (strcmp(buf, "off") == 0) {
                        pkt.arg = 0xff;
                } else {
                        enum valve which_valve = NR_VALVES;

                        // try to match a valve name
                        bool found_valid_name = false;
                        for (enum valve v = FIRST_VALVE; v < NR_VALVES;
                             v = next_valve(v)) {
                                const char *sname =
                                        valve_properties[v].short_name;
                                size_t len = strlen(sname);
  
                                // do we match this valve name?
                                if (strncmp(buf, sname, len) == 0) {
                                        found_valid_name = true;
                                        which_valve = v;
                                        buf += len;
  
                                        // another space
                                        if (*buf++ != ' ')
                                                goto bad_command;

                                        break;
                                }
                        }
  
                        if (!found_valid_name)
                                goto bad_command;
  
                        // match either 'on' or 'off'
                        bool on = false;
                        if (strcmp(buf, "on") == 0) {
                                buf += 3;
                                on = true;
                        } else if (strcmp(buf, "off") == 0) {
                                buf += 4;
                                on = false;
                        } else {
                                goto bad_command;
                        }
 
                        // we should be at the end of a command now
                        if (*buf != '\0')
                                goto bad_command;

                        // fill in the arg field of the packet
                        uint32_t arg = 0;

                        arg |= (uint32_t)which_valve & 0xff;

                        // do what the command asked for
                        if (on)
                                arg |= valve_is_flow(which_valve) ? 0xff00
                                        : 0x100;
                        // else turn the valve off, which means set bits
                        // 8-15 to 0, which they already are
                        
                        pkt.arg = arg;
                }

                fprintf(stderr, "modding a valve\n");
                
                goto send_pkt;
        }
        
bad_command:
        fprintf(stderr, "%s: bad command, resetting buffer\n", __func__);
        return -1U;

send_pkt:

        // http://stackoverflow.com/q/8384388/3775803
        ;

        size_t sent = 0;
        size_t remaining = sizeof pkt;
        for (int i = 0; i < 1000; ++i) {
                ssize_t ret = write(sd, &pkt, remaining);
                if (ret == -1) {
                        // this socket is marked as non-blocking, so there's
                        // a very small chance that a write fails because
                        // we can't send data (the chance is only small
                        // because we're not sending a lot of data). In
                        // this case, sleep for a  millisecond and keep
                        // trying
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {

                                // this is a weird situation, so let's be
                                // chatty
                                fprintf(stderr,
                                        "write would block, retrying");

                                struct timespec ts;
                                memset(&ts, 0, sizeof ts);
                                ts.tv_nsec = 1000*1000;
                                int err = nanosleep(&ts, NULL);
                                if (err == -1)
                                        die("nanosleep", errno);
                                continue;
                        }

                        // otherwise this was a bad error
                        die("write", errno);
                }

                // normal case: the write succeeds--update our counters
                sent += ret;
                remaining -= ret;

                // yayyy we wrote the whole packet!
                if (remaining == 0)
                        return sent_seq;
        }

        // well shit, we got through 1000 tries and failed, this is bad
        die("failed to write to socket after 1000 tries, giving up", EIO);
}

static void say_hello(int sd)
{
        struct hello_packet pkt;
        memset(&pkt, 0, sizeof pkt);
        pkt.header.len = sizeof pkt;
        pkt.header.type = PT_HELLO;
        pkt.header.seq = 1;

        ssize_t ret = write(sd, &pkt, sizeof pkt);
        if (ret == -1)
                die("failed to say hello", errno);
        else if (ret != sizeof pkt)
                die("failed to write whole hello packet", EIO);
}

static int global_sd = -1;

void sigint_handler(int sig)
{
        (void)sig;

        close(global_sd);
        exit(0);
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

        // put the socket in a global so a signal handler can close it on
        // Ctrl-C
        global_sd = sd;
        if (signal(SIGINT, sigint_handler) == SIG_ERR)
                die("signal", errno);

        // define the server address
        memset(&addr, 0, sizeof addr);
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(ELET_NET_ADDR);
        addr.sin_port = htons(ELET_NET_PORT);

        // connect to the server (arduino)
        err = connect(sd, (struct sockaddr *)&addr, sizeof addr);
        if (err == -1)
                die("connect failed", errno);

        // send the hello packet so the sever picks us up
        say_hello(sd);

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

        uint32_t seq_acked = 0; 
        uint32_t seq_sent = 1; // the "hello" packet we sent has seq = 1

        // if we don't here from the server for 2 seconds, something bad
        // happened. (this won't necessarily catch death if someone is
        // typing things on the command line constantly.
        const int timeout_ms = -1; // XXX: change me

        // the state we think the arduino system is in;
        enum system_state sys_state = SS_READY;

        for (;;) {
                const short events = POLLIN;
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
                                // (but null-terminate it first, to be nice)
                                cmd_buf[i] = '\0';
                                uint32_t s = process_command(cmd_buf, i,
                                                             seq_sent,
                                                             sys_state, sd);
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
                                cmd_idx -= i;
                                cmd_space += i;

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
                                                                    logfd,
                                                                    &sys_state);

                                        // uh-oh, the arduino sent back
                                        // a seq number less than what
                                        // we've seen so far: this probably
                                        // means it reset. This is probs bad
                                        if (s < seq_acked || s == 0) {
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
