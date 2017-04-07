
#ifndef VALVE_PINS_H
#define VALVE_PINS_H

#include "Adafruit_MAX31855.h"

enum valve {
        OX_ON_OFF = 0,
        FIRST_VALVE = OX_ON_OFF,
        OX_BLEED,
        OX_FLOW,
        N2_PURGE,
        N2_ON_OFF,
        FUEL_FLOW,
        NR_VALVES,
};

struct valve_properties {
        // the name of this valve
        const char *name;

        // name used in commands
        const char *short_name;

        // the io pin we use to control this valve
        const byte pin;

        // is this valve a flow control valve?
        const bool is_flow;
};

static const struct valve_properties valve_properties[] {
        [OX_ON_OFF] = {
                .name = "oxygen on/off",
                .short_name = "oxoo",
                .pin = 6,
                .is_flow = false
        },
        [OX_BLEED] = {
                .name = "oxygen bleed",
                .short_name = "oxbl",
                .pin = 7,
                .is_flow = false,
        },
        [OX_FLOW] = {
                .name = "oxygen flow control",
                .short_name = "oxfl",
                .pin = 8,
                .is_flow = true
        },
        [N2_PURGE] = {
                .name = "nitrogen purge",
                .short_name = "n2pr",
                .pin = 10,
                .is_flow = false
        },
        [N2_ON_OFF] = {
                .name = "nitrogen on/off",
                .short_name = "n2oo",
                .pin = 9,
                .is_flow = false
        },
        [FUEL_FLOW] = {
                .name = "fuel flow control",
                .short_name = "fufl",
                .pin = 11,
                .is_flow = true
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

static inline void
setup_all_valves()
{
        for (enum valve v = FIRST_VALVE; v < NR_VALVES; v = next_valve(v))
                pinMode(valve_properties[v].pin, OUTPUT);
}

static inline void
close_valve(enum valve v)
{
        if (valve_is_flow(v))
                analogWrite(valve_properties[v].pin, 0);
        else
                digitalWrite(valve_properties[v].pin, 0);
}

static inline void
open_valve(enum valve v)
{
        if (valve_is_flow(v))
                analogWrite(valve_properties[v].pin, 255);
        else
                digitalWrite(valve_properties[v].pin, 1);
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
        byte pin;

        // calibration data
        float slope;
        float offset;
};

static const struct pressure_sensor_properties pressure_sensor_properties[] {
        [PS_OXYGEN] = {
                name : "oxygen (yellow)",
                pin : 1,
                slope : 1.222,
                offset : -250.0
        },
        [PS_FUEL] = {
                name : "fuel (blue)",
                pin : 2,
                slope : 1.222,
                offset : -250.0
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

static inline struct pressure_reading
read_pressure(enum pressure_sensor p)
{
        struct pressure_reading r;
        const struct pressure_sensor_properties *props =
                pressure_sensor_properties + (int)p;

        r.digital = analogRead(props->pin);
        r.analog = props->slope * r.digital + props->offset;
        
        return r;
}

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
        int8_t clk_pin;
        int8_t cs_pin;
        int8_t do_pin;
        
        Adafruit_MAX31855 chip;
};

static struct thermocouple_properties thermocouple_properties[] {
        [TC_OXYGEN] = {
                name : "oxygen",
                clk_pin : 48,
                cs_pin : 50,
                do_pin : 52,
                chip : {thermocouple_properties[TC_OXYGEN].clk_pin,
                        thermocouple_properties[TC_OXYGEN].cs_pin,
                        thermocouple_properties[TC_OXYGEN].do_pin}
        },
        [TC_WATER] = {
                name : "water",
                clk_pin : 48,
                cs_pin : 46, // XXX: double check this
                do_pin : 52,
                chip : {thermocouple_properties[TC_WATER].clk_pin,
                        thermocouple_properties[TC_WATER].cs_pin,
                        thermocouple_properties[TC_WATER].do_pin}
        },
};

static inline double
read_thermocouple_f(const enum thermocouple tc)
{
        return thermocouple_properties[tc].chip.readFarenheit();
}

struct igniter {
        // this is a GPIO pin that controlls current to the igniter
        // continuity sense circuit. Turn this pin on then analogRead from
        // the ADC pin igniter_cont_sense to see if a working igniter is
        // present
        byte igniter_cont_ctl;

        // This is an ADC pin that detects continuity across the igniter. It
        // will read 0 if the igniter is not present or if igniter_cont_ctl
        // is off
        byte igniter_cont_sense;

        // this is a GPIO pin that controlls current to the igniter firing
        // circuit. writing to this pin will fire the igniter.
        //
        // *BE CAREFUL WITH THIS PIN*
        byte igniter_fire_ctl_be_careful;

        // this is an ADC pin that senses continuity across the ignition
        // sense wire. This is a thin wire across the mouth of the engine.
        // If the engine fires successfully, this wire should melt or be
        // blown away, and this pin will read zero. Otherwise this pin
        // should read non-zero
        byte ignition_sense;
};

static struct igniter sys_igniter = {
        .igniter_cont_ctl = 2,
        .igniter_cont_sense = 0,
        .igniter_fire_ctl_be_careful = 3,
        .ignition_sense = 1
};

static inline void
setup_igniter()
{
        pinMode(sys_igniter.igniter_cont_ctl, OUTPUT);
        pinMode(sys_igniter.igniter_fire_ctl_be_careful, OUTPUT);
}

enum ignition_status {
        IGN_SUCCESS,
        IGN_FAIL_NO_ISENSE_WIRE,
        IGN_FAIL_BAD_IGNITER,
        IGN_FAIL_NO_IGNITION,
        IGN_NUM_STATUSES
};

// Current state of the entire system. Our state diagram is
//
//                  (2)
//               ________
//  (1)         /        v
//  --->  SS_READY       SS_FIRE
//          ^   ^________/    /
//       (5) \      (3)      / (4)
//            \             v
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
enum system_state {
        SS_READY,
        SS_FIRE,
        SS_SAFING,
        SS_NUM_STATES
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

// fire the igniter for a test. This differs from a real ignition in that
// no valves are actuated
static inline enum ignition_status __attribute__((warn_unused_result))
igniter_test_fire()
{
        int continuity;

        // first check continuity across the ignition sense wire. If it's not
        // there, we have no way to know if the engine fired, so we don't
        // want to even try.
        continuity = analogRead(sys_igniter.ignition_sense);
        if (continuity == 0)
                return IGN_FAIL_NO_ISENSE_WIRE;

        // turn on the igniter continuity sense circuit and see if any
        // current is flowing through. If not, it means we have a bad
        // igniter
        digitalWrite(sys_igniter.igniter_cont_ctl, HIGH);

        // let thing settle for a bit (XXX: not sure why we really need
        // this delay, but things don't work without it...
        delay(50);
        continuity = analogRead(sys_igniter.igniter_cont_sense);

        // turn off that circuit before we do anything else
        digitalWrite(sys_igniter.igniter_cont_ctl, LOW);

        Serial.print("igniter continuity was ");
        Serial.println(continuity);

        // okay we have a bad igniter: no current is flowing through it
        if (continuity == 0)
                return IGN_FAIL_BAD_IGNITER;

        delay(3000);

        // okay here goes: fire the igniter
        digitalWrite(sys_igniter.igniter_fire_ctl_be_careful, HIGH);

        // wait a hot second to make sure it fires
        delay(1000);

        // turn that circuit off
        digitalWrite(sys_igniter.igniter_fire_ctl_be_careful, LOW);

        // add artificial delay to allow the user to remove the ignition
        // sense wire; remember, this is just a test function
        delay(5000);
        
        // now see if we detected ignition
        continuity = analogRead(sys_igniter.ignition_sense);
        if (continuity == 0)
                return IGN_SUCCESS;
        else
                return IGN_FAIL_NO_IGNITION;
}

// network bullshittery begins here, continue at your own risk

// packet types
#define PT_DATA ((uint8_t)1)
#define PT_REQ ((uint8_t)2)
#define PT_MESSAGE ((uint8_t)3)

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
        float pressures[NR_PSENSORS];
        float temps[NR_THERMOCOUPLES];

        // thrust from the load cell. Undetermines format
        uint32_t thrust;
};

// stop the engine. No arguments
#define REQ_CMD_STOP ((uint8_t)0)

// start the engine. Argument is the length of the burn in seconds. Must
// be between 10 and 120
#define REQ_CMD_START ((uint8_t)1)

// do something with a valve. Bits 0-7 of `arg` are which valve (`enum valve`
// squashed into 8 bits), bits 8-15 are value. 0 means closed, 1 means
// open for solenoid, and 1-255 mean open to that PWM value for flow ctl
// valves. This command is only valid in the SS_DEBUG state.
#define REQ_MOD_VALVE ((uint8_t)2)

// XXX: possible other commands:
// * modify purge timings

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

#endif // ELET_H
