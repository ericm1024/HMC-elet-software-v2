#!/usr/bin/env python

import csv
import sys

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

state_names = ["ready", "firing", "safing", "num states (shouldn't happen)"]

messages = []
data = []

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

        if data[0] == "message":
            messages.append({"time", int(line[1]),
                             "msg", line[3]})
            
for i,line in enumerate(data):
    if i == 0:
        continue

    last_line = data[i-1]

    ts = line["time"]/1000.0
    # print out any and all valve state changes
    vs = line["valve states"]
    lvs = last_line["valve states"]
    for i in range(nr_valves):
        name = valve_names[i]
        pwm = -1
        last_pwm = -1
        if is_pwm(name):
            pwm = line["ox pwm"] if name == "oxfl" else line["fuel pwm"]
            last_pwm = last_line["ox pwm"] if name == "oxfl" \
                       else last_line["fuel pwm"]

        on = (vs & (1 << i)) != 0
        if (vs & (1 << i)) != (lvs & (1 << i)):
            if is_pwm(name):
                print ts, name.upper(), "ON" if on else "OFF", pwm

            else:
                print ts, name.upper(), "ON" if on else "OFF"

        # if it's a flow control valve, we want to see pwm changes as well
        if is_pwm(name) and on and pwm != last_pwm and last_pwm != 0:
            print ts, name.upper(), "pwm", last_pwm, "to", pwm

    # print out state changes
    ss = line["state"]
    lss = last_line["state"]
    if ss != lss:
        print ts, "state changed from",state_names[lss],"to",\
            state_names[ss]

    # print out if ignition status changed
    istat = line["ign stat"]
    listat = last_line["ign stat"]
    if istat != listat:
        print ts, "ignition status:", ign_stats[istat]

for m in messages:
    print m["time"], m["msg"]
