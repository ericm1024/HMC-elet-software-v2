#ifndef ELET_ARDUINO_H
#define ELET_ARDUINO_H

// this file contains arduino-specific code. The generic stuff is in
// elet.h. The files are separated so that we can compile the generic
// code on the client, e.g. my laptop.

#include <Ethernet2.h>
#include <Adafruit_MAX31855.h>
#include <Q2HX711.h>

#include "elet.h"

        
// 0 == closed, 1 = open for solenoids, 1-255 is PWM for flow ctls
static uint8_t valve_states[NR_VALVES] = {0};

static inline void
close_valve(enum valve v)
{
        if (valve_is_flow(v))
                analogWrite(valve_properties[v].pin, 0);
        else
                digitalWrite(valve_properties[v].pin, 0);

        valve_states[v] = 0;
}

static inline void
open_valve(enum valve v)
{
        if (valve_is_flow(v)) {
                analogWrite(valve_properties[v].pin, 255);
                valve_states[v] = 255;
        } else {
                digitalWrite(valve_properties[v].pin, 1);
                valve_states[v] = 1;
        }
}

static inline void
open_valve_to(enum valve v, uint8_t val)
{
        if (valve_is_flow(v)) {
                analogWrite(valve_properties[v].pin, val);
                valve_states[v] = val;
        }
}

static inline void
setup_all_valves()
{
        for (enum valve v = FIRST_VALVE; v < NR_VALVES; v = next_valve(v)) {
                pinMode(valve_properties[v].pin, OUTPUT);
                valve_states[v] = 0;
                close_valve(v);
        }
}

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

static Adafruit_MAX31855 thermocouples[NR_THERMOCOUPLES] = {
        [TC_OXYGEN] = {thermocouple_properties[TC_OXYGEN].clk_pin,
                       thermocouple_properties[TC_OXYGEN].cs_pin,
                       thermocouple_properties[TC_OXYGEN].do_pin},
        [TC_WATER] = {thermocouple_properties[TC_WATER].clk_pin,
                      thermocouple_properties[TC_WATER].cs_pin,
                      thermocouple_properties[TC_WATER].do_pin}
};

static inline float
read_thermocouple_f(const enum thermocouple tc)
{
        return thermocouples[tc].readFarenheit();
}

static inline void
setup_igniter()
{
        pinMode(sys_igniter.igniter_cont_ctl, OUTPUT);
        pinMode(sys_igniter.igniter_fire_ctl_be_careful, OUTPUT);
}

static Q2HX711 load_cell(load_cell_props.dout_pin, load_cell_props.clk_pin);

static inline long read_load_cell()
{
        return load_cell.read();
}

static inline float read_load_cell_calibrated()
{
        return read_load_cell()*load_cell_props.slope
                + load_cell_props.offset;
}

static enum ignition_status last_ign_status = IGN_NUM_STATUSES;

// this is a helper so server_tx_all_data() can call it
static inline int __attribute__((warn_unused_result))
igniter_test_continuity()
{
        int continuity;

        // turn on the igniter continuity sense circuit and see if any
        // current is flowing through. If not, it means we have a bad
        // igniter
        digitalWrite(sys_igniter.igniter_cont_ctl, HIGH);

        // let thing settle for a bit (XXX: not sure why we really need
        // this delay, but things don't work without it...)
        delay(100);
        continuity = analogRead(sys_igniter.igniter_cont_sense);

        // turn off that circuit before we do anything else        
        digitalWrite(sys_igniter.igniter_cont_ctl, LOW);

        return continuity;
}

// fire the igniter for a test. This differs from a real ignition in that
// no valves are actuated
static inline enum ignition_status __attribute__((warn_unused_result))
igniter_test_fire()
{
        int continuity;
        enum ignition_status ret;

        // first check continuity across the ignition sense wire. If it's not
        // there, we have no way to know if the engine fired, so we don't
        // want to even try.
        continuity = analogRead(sys_igniter.ignition_sense);
        if (continuity == 0) {
                ret = IGN_FAIL_NO_ISENSE_WIRE;
                goto out;
        }

        continuity = igniter_test_continuity();

        Serial.print("igniter continuity was ");
        Serial.println(continuity);

        // okay we have a bad igniter: no current is flowing through it
        if (continuity == 0) {
                ret = IGN_FAIL_BAD_IGNITER;
                goto out;
        }

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
                ret = IGN_SUCCESS;
        else
                ret = IGN_FAIL_NO_IGNITION;

out:
        last_ign_status = ret;
        return ret;
}

#define FUEL_DRAIN_TIME_SECONDS 120

#endif // ELET_ARDUINO_H
