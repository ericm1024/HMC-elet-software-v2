#!/usr/bin/env python

import csv
import sys
import matplotlib.pyplot as plt
import numpy

fname = "run.log"

if len(sys.argv) > 2:
    print "usage ./post.py [fname]"
    exit(1)

if len(sys.argv) == 2:
    fname = sys.argv[1]

# from elet.h, `enum valve` definition
valve_names = ["oxoo", "oxbl", "oxfl", "n2pr", "n2oo", "fufl", "fuoo"]
nr_valves = len(valve_names)
is_pwm = lambda x: x == "oxfl" or x == "fufl"

# from elet.h `enum ignition_status` definition
ign_stats = ["success",
             "failed: no ignition sense wire present",
             "failed: no continuity across igniter",
             "failed: no ignition",
             "no ignition attempted"]

state_names = ["ready", "firing", "safing", "fuel depressurization",
               "num states (shouldn't happen)"]

messages = []
data = []
times = []
ox_pressure = []
fuel_pressure = []
load = []

with open(fname, 'rb') as fd:
    reader = csv.reader(fd)
    for line in reader:
        if line[0] == "data":
            data.append({"time" : int(line[1]),
                         "valve states" : int(line[3], 16),
                         "ox pwm" : int(line[4]),
                         "fuel pwm" : int(line[5]),
                         "ign stat" : int(line[6], 16),
                         "state" : int(line[7], 16)})
            # 9, 10
            times.append(int(line[1])/1000.0)
            ox_pressure.append(int(line[9]))
            fuel_pressure.append(int(line[10]))
            load.append(int(line[-1]))

        if data[0] == "message":
            messages.append({"time", int(line[1]),
                             "msg", line[3]})
            
plt.subplot(2, 1, 1)
plt.plot(times, ox_pressure, times, fuel_pressure)
plt.title("Pressure")
plt.xlabel("time (seconds)")
plt.ylabel("Pressure (digital)")

plt.subplot(2, 1, 2)
plt.plot(times, load)
plt.title("load cell")
plt.xlabel("time (seconds)")
plt.ylabel("load (bullshit)")

plt.show()
