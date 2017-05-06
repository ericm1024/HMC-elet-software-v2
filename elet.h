#ifndef ELET_H
#define ELET_H

// this file contains definitions needed by both the client and server

#ifdef ARDUINO_ARCH_AVR
#define ELET_HAVE_ARDUINO
#else
#define ELET_HAVE_UNIX
#endif

enum valve {
        OX_ON_OFF = 0,
        FIRST_VALVE = OX_ON_OFF,
        OX_BLEED,
        OX_FLOW,
        N2_PURGE,
        N2_ON_OFF,
        FUEL_FLOW,
        FUEL_ON_OFF,
        NR_VALVES,
};

struct valve_properties {
        // the name of this valve
        const char *name;

        // name used in commands
        const char *short_name;

        // the io pin we use to control this valve
        const uint8_t pin;

        // is this valve a flow control valve?
        const bool is_flow;
};

// 6, 12, 8, 2, 9, 11, 7
static const struct valve_properties valve_properties[] = {
        [OX_ON_OFF] = {
                .name = "oxygen on/off",
                .short_name = "oxoo",
                .pin = 11,
                .is_flow = false
        },
        [OX_BLEED] = {
                .name = "oxygen bleed",
                .short_name = "oxbl",
                .pin = 9,
                .is_flow = false,
        },
        [OX_FLOW] = {
                .name = "oxygen flow control",
                .short_name = "oxfl",
                .pin = 2,
                .is_flow = true
        },
        [N2_PURGE] = {
                .name = "nitrogen purge",
                .short_name = "n2pr",
                .pin = 7,
                .is_flow = false
        },
        [N2_ON_OFF] = {
                .name = "nitrogen on/off",
                .short_name = "n2oo",
                .pin = 8,
                .is_flow = false
        },
        [FUEL_FLOW] = {
                .name = "fuel flow control",
                .short_name = "fufl",
                .pin = 6,
                .is_flow = true
        },
        [FUEL_ON_OFF] = {
                .name = "fuel on/off",
                .short_name = "fuoo",
                .pin = 12,
                .is_flow = false
        }
};

static inline const char *
valve_name(const enum valve v)
{
        return valve_properties[v].name;
}

static inline int
valve_pin(const enum valve v)
{
        return valve_properties[v].pin;
}

static inline bool
valve_is_flow(const enum valve v)
{
        return valve_properties[v].is_flow;
}

static inline enum valve
next_valve(enum valve v)
{
        return (enum valve)((int)v + 1);
}

enum pressure_sensor {
        PS_OXYGEN,
        FIRST_PSENSOR = PS_OXYGEN,
        PS_FUEL,
        NR_PSENSORS
};

struct pressure_sensor_properties {
        // name of this sensor
        const char *name;

        // pin we use to analogRead from this sensor
        const uint8_t pin;

        // calibration data
        const float slope;
        const float offset;
};

static const struct pressure_sensor_properties pressure_sensor_properties[] = {
        [PS_OXYGEN] = {
                .name = "oxygen (yellow)",
                .pin = 1,
                .slope = 1.222,
                .offset = -250.0
        },
        [PS_FUEL] = {
                .name = "fuel (blue)",
                .pin = 2,
                .slope = 1.222,
                .offset = -250.0
        }
};

static inline enum pressure_sensor
next_pressure_sensor(enum pressure_sensor p)
{
        return (enum pressure_sensor)((int)p + 1);
}

struct pressure_reading {
        int digital;
        float analog;
};

enum thermocouple {
        TC_OXYGEN,
        FIRST_THERMOCOUPLE = TC_OXYGEN,
        TC_WATER,
        NR_THERMOCOUPLES
};

struct thermocouple_properties {
        // name of this thermocouple
        const char *name;

        // pins
        const int8_t clk_pin;
        const int8_t cs_pin;
        const int8_t do_pin;
};

static struct thermocouple_properties thermocouple_properties[] = {
        [TC_OXYGEN] = {
                .name = "oxygen",
                .clk_pin = 43,
                .cs_pin = 42, // blue heat shrink
                .do_pin = 44,
        },
        [TC_WATER] = {
                .name = "water",
                .clk_pin = 43,
                .cs_pin = 45, // yellow heat shrink
                .do_pin = 44,
        }
};

static inline enum thermocouple
next_thermocouple(const enum thermocouple tc)
{
        return (enum thermocouple)((int)tc + 1);
}

struct load_cell_properties {
        // data out pin from the load cell board
        const int8_t dout_pin;

        // clock pin from the load cell board
        const int8_t clk_pin;

        // load cell calibration data load cell reading in lbf is calculated
        // as load_cell.read() * slope + offset, i.e. as a linear function
        // of the raw reading.
        float slope;
        float offset;
};

static struct load_cell_properties load_cell_props = {
        .dout_pin = 39,
        .clk_pin = 38,

        // from slack message from Mike on 12 April 2017
        .slope = 5.6234*10e-5,
        .offset = -471.15,
};

struct igniter {
        // this is a GPIO pin that controlls current to the igniter
        // continuity sense circuit. Turn this pin on then analogRead from
        // the ADC pin igniter_cont_sense to see if a working igniter is
        // present
        const uint8_t igniter_cont_ctl;

        // This is an ADC pin that detects continuity across the igniter. It
        // will read 0 if the igniter is not present or if igniter_cont_ctl
        // is off
        const uint8_t igniter_cont_sense;

        // this is a GPIO pin that controlls current to the igniter firing
        // circuit. writing to this pin will fire the igniter.
        //
        // *BE CAREFUL WITH THIS PIN*
        const uint8_t igniter_fire_ctl_be_careful;

        // this is an ADC pin that senses continuity across the ignition
        // sense wire. This is a thin wire across the mouth of the engine.
        // If the engine fires successfully, this wire should melt or be
        // blown away, and this pin will read zero. Otherwise this pin
        // should read non-zero
        const uint8_t ignition_sense;
};

static struct igniter sys_igniter = {
        .igniter_cont_ctl = 25,
        .igniter_cont_sense = 4,
        .igniter_fire_ctl_be_careful = 5,
        .ignition_sense = 5
};

enum ignition_status {
        IGN_SUCCESS,
        IGN_FAIL_NO_ISENSE_WIRE,
        IGN_FAIL_BAD_IGNITER,
        IGN_FAIL_NO_IGNITION,
        IGN_NUM_STATUSES
};

static inline const char *
ignition_status_to_str(const enum ignition_status st)
{
        static const char *map[] = {
        [IGN_SUCCESS] = "success",
        [IGN_FAIL_NO_ISENSE_WIRE] =
                "failed: no ignition sense wire present",
        [IGN_FAIL_BAD_IGNITER] =
                "failed: no continuity across igniter",
        [IGN_FAIL_NO_IGNITION] =
                "failed: no ignition"
        };

        if (st < IGN_SUCCESS || st >= IGN_NUM_STATUSES)
                return "bad ignition status";
        else
                return map[st];
}

// Current state of the entire system. Our state diagram is
//
//
//     SS_DEPRESS 
//         ^
//   (7,8) |        (2)
//         |     ________
//  (1)    v    /        v
//  --->  SS_READY       SS_FIRE
//          ^   ^________/    /
//     (5,6) \      (3)      / (4)
//            v             v
//               SS_SAFING
//
// The states have the following semantics and rules:
//
//   state       notes
//  -----------------------------------------------------------------------
//   SS_READY    This state is the only safe state. In this state it is safe
//               to approach the engine, but care should still be exercised
//               in case of other software bugs.
//
//               Valves can be be arbitrarily actuated in this state with
//               the REQ_MOD_VALVE command, but this should be done with
//               extreme caution, and only in the case of other software
//               bugs.
//
//   SS_FIRE     In this state the engine is firing or attempting to do so.
//               No one shall approach the engine when it is in this state.
//
//   SS_SAFING   In this state the system is shutting the engine off, purging
//               the engine with nitrogen, and de-pressurizing the oxygen
//               line. It is not safe to approach the engine in this state,
//               but this state should be brief.
//
//   SS_DEPRESS  In this state we're depressurizing and emptying the fuel
//               tank. This state is only entered manually, via a REQ_CMD_DEPRESS
//
// The state machine has the following transition table:
//
//   #    prev       next        reason
//  -----------------------------------------------------------------------
//   1    n/a        SS_READY    This is the default state on startup.
//
//   2    SS_READY   SS_FIRE     This state transition is triggered by a
//                               REQ_CMD_START command from the client to the
//                               system. This state transition represents a
//                               request to start the engine
//
//   3    SS_FIRE    SS_READY    This transition happens when the system is
//                               told to fire, but there is no continuity
//                               across the ingiter, so no ignition is
//                               attempted.
//
//   4    SS_FIRE    SS_SAFING   This transition happens when either the burn
//                               time runs out or someone issues an explicit
//                               REQ_CMD_STOP.
//
//   5    SS_SAFING  SS_READY    This transition happens once automated
//                               safing is complete.
//
//   6    SS_READY   SS_SAFING   This transition happens when someone sends
//                               a REQ_CMD_STOP while we're in the ready
//                               state. We might want to do this in the case
//                               of a power failure or something else weird.
//
//   7    SS_READY   SS_DEPRESS  This transitions happens when the arduino
//                               gets a REQ_CMD_DEPRESS command
//   
//   8    SS_DEPRESS SS_READY    This transition when the depressurization
//                               finishes.
enum system_state {
        SS_READY,
        SS_FIRE,
        SS_SAFING,
        SS_DEPRESS,
        SS_NUM_STATES
};

// network bullshittery begins here, continue at your own risk

// packet types
#define PT_DATA ((uint8_t)1)
#define PT_REQ ((uint8_t)2)
#define PT_MESSAGE ((uint8_t)3)
#define PT_HELLO ((uint8_t)4)

// this header is at the start of every packet we send over the wire.
// Packet parsing code should first parse the length and packet type out of
// this header, then parse the rest of the packet based on the type. Code
// should yell when it does not recognize the packet type.
struct packet_header {
        // length of this packet in bytes
        uint16_t len;

        // type of this message. One of the PT_* constants.
        uint8_t type;
        uint8_t _pad1;

        // The sequence number of this packet. This field exists so that we
        // can provide an ordering between sent and recived packets. For
        // example, when someone issues a PT_REQ packet with a REQ_CMD_START
        // command (i.e. "start the damn engine"), we want to know when that
        // command was processed so we can look at the result of the last
        // ignition in subsequent data packets. However, we don't want to
        // examine the result-of-last-ignition field until we know for sure
        // that the command has been processed. This seems like it would be
        // easy---just wait for the command to send and read the value out
        // of the next data packet. Howevver, but due to packet buffering
        // on both ends, "just wait" is not a viable option as the next
        // data packet may have been sent before we sent our request. Thus
        // we need a sequence number.
        //
        // For packets sent from the server (arduino) to the client (i.e.
        // PT_DATA and PT_MESSAGE), this number is the sequence number of
        // the last command processed. For packets sent from the client to
        // the server (i.e. PT_REQ), this is a sequence number for the
        // message. The server does not look at it at all, it just pastes
        // it into all subsequent response headers until a new PT_REQ packet
        // comes in, then starts using that value.
        //
        // Thus it is suggested that the client start with seq = 0 and
        // increment it by one on every PT_REQ sent to the server. Then the
        // client should wait until it sees a data packet with seq == seq
        // of the last command
        uint32_t seq;

        // this field is the 32 bit millisecond timestamp returned
        // by millis() at the time this data was gathered. This field is
        // ignored by the server.
        uint32_t timestamp;
};

// this packet is sent from the arduino to the client. It contains the
// state of the entire system. Be careful when you touch this struct,
// it's carefully designed to use no padding
struct data_packet {
        struct packet_header header;

        // the state of each valve as a bitmap. Index into this bitmap with
        // integer interpretation of the values of `enum valve`. 0 means
        // the valve is closed, 1 means the valve is open. For pwm valves,
        // consult the next two fields to find out how open they are.
        //
        // NB: right now we only have 6 valves. If we get more than 8, we
        // need to change this to a uint16_t
        uint8_t vlv_states;
        uint8_t vlv_pwm_ox;
        uint8_t vlv_pwm_fuel;

        // system state:
        // * bits 0-2 are the result of the last ignition, packed into 3 bits
        //   aka an enum ignition_status. (one extra bit in case we add more
        //   ignition statuses)
        //
        // * bits 3-5 are the system state packed into 3 bits aka an enum
        //   system_state (again, one extra bit in case we add states)
        //
        // * bit 6 is the "igniter good" bit. If we have continuity across
        //   the igniter, this bit is 1, otherwise it is zero. This bit is
        //   only valid when our system_state is SS_READY, otherwise it is
        //   zero.
        //
        // * bit 7 is reserved/always zero. If a client sees this bit as
        //   non-zero, it should yell loudly.
        uint8_t state;

        // data from all of our sensors. IEEE floats.
        uint16_t pressures[NR_PSENSORS];
        float temps[NR_THERMOCOUPLES];

        // thrust from the load cell in lbf.
        uint32_t thrust;
};

// stop the engine. No arguments
#define REQ_CMD_STOP ((uint8_t)0)

// start the engine. Argument is the length of the burn in seconds. Must
// be between 2 and 120
#define REQ_CMD_START ((uint8_t)1)

#define REQ_CMD_START_MIN_BURN_TIME 2
#define REQ_CMD_START_MAX_BURN_TIME 120

// do something with a valve. Bits 0-7 of `arg` are which valve (`enum valve`
// squashed into 8 bits), bits 8-15 are value. 0 means closed, 1 means
// open for solenoid, and 1-255 mean open to that PWM value for flow ctl
// valves. If bits 0-7 of arg are 0xff, the rest of the bits are ignored and
// all valves are turned off.
//
// This command is only valid in the SS_DEBUG state.
#define REQ_MOD_VALVE ((uint8_t)2)

#define REQ_CMD_DEPRESS_MIN_TIMEOUT 15
#define REQ_CMD_DEPRESS_MAX_TIMEOUT 120

// depressurize the fuel pressure vessel. Argument is drain time in seconds
#define REQ_CMD_DEPRESS ((uint8_t)3)

// this packet is sent from the client to the arduino to tell it to do things
struct req_packet {
        struct packet_header header;

        // the command for this request. one of the REQ_CMD_* constants.
        uint8_t cmd;
        uint8_t _pad1[3];

        // argument See REQ_CMD_* for details
        uint32_t arg;
};

// this packet is sent from the arduino to the client to share diagnostic
// information in string formats. This packet is sent sparingly due to its
// space overhead.
struct message_packet {
        struct packet_header header;

        // ascii null-terminated string. All bytes after null are undefined.
        uint8_t data[256];
};

struct hello_packet {
        struct packet_header header;
};

#define ELET_NET_ADDR ((192UL << 24) | (168UL << 16) | (1UL << 8) | 100UL)

// I am 13 years old
#define ELET_NET_PORT 420

#endif // ELET_H
