
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

        // the io pin we use to control this valve
        const byte pin;

        // is this valve a flow control valve?
        const bool is_flow;
};

static const struct valve_properties valve_properties[] {
        [OX_ON_OFF] = {
                .name = "oxygen on/off",
                .pin = 6,
                .is_flow = false
        },
        [OX_BLEED] = {
                .name = "oxygen bleed",
                .pin = 7,
                .is_flow = false,
        },
        [OX_FLOW] = {
                .name = "oxygen flow control",
                .pin = 8,
                .is_flow = true
        },
        [N2_PURGE] = {
                .name = "nitrogen purge",
                .pin = 10,
                .is_flow = false
        },
        [N2_ON_OFF] = {
                .name = "nitrogen on/off",
                .pin = 9,
                .is_flow = false
        },
        [FUEL_FLOW] = {
                .name = "fuel flow control",
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
                pinMode(valve_properties[v].pin, OUT);
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
        FIRST_PSENSOR = OXYGEN,
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
                .name = "oxygen (yellow)",
                .pin = -1,
                .slope = 0.0,
                .offset = 0.0
        },
        [PS_FUEL] = {
                .name = "fuel (blue)",
                .pin = -1,
                .slope = 0.0,
                .offset = 0.0
        }
};

static inline enum valve
next_pressure_sensor(enum pressure_sensor p)
{
        return (enum pressure_sensor)((int)p + 1);
}

static inline int
read_pressure_sensor_raw(enum pressure_sensor p)
{
        return analogRead(pressure_sensor_properties[p].pin);
}

enum thermocouple {
        TC_OXYGEN,
        FIRST_THERMOCOUPLE = OXYGEN,
        TC_WATER,
        NR_THERMOCOUPLES
};

struct thermocouple_properties {
        // name of this thermocouple
        const char *name;

        // pins
        byte clk_pin;
        byte cs_pin;
        byte do_pin;
        
        Adafruit_MAX31855 chip(clk_pin, cs_pin, do_pin);
};

static struct thermocouple_properties thermocouple_properties[] {
        [TC_OXYGEN] = {
                .name = "oxygen",
                .clk_pin = -1,
                .cs_pin = -1,
                .do_pin = -1
        },
        [TC_WATER] = {
                .name = "water",
                .clk_pin = -1,
                .cs_pin = -1,
                .do_pin = -1
        },
};

#endif // VALVE_PINS_H
