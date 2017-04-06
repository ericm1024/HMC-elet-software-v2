
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
        IGN_NUM_STATUSES,
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

#endif // VALVE_PINS_H
