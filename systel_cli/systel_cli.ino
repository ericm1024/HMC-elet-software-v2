#include "elet.h"

char buf[512];
size_t idx = 0;
size_t sz = sizeof buf;

void clear_buffer()
{
  memset(buf, 0, sizeof buf);
  idx = 0;
  sz = sizeof buf;
}

/*
 * Supported commands:
 * 
 * set all|<name> closed|open|<value>
 * 
 * read all|<name>
 * 
 * list valves|sensors
 * 
 * h
 */
void process_command()
{
  size_t i = 0;

  // process a help command
  if (strcmp(buf, "h") == 0) {
    Serial.println("Valve control:      set all|<name> <value>");
    Serial.println("Sensor reading:     read all|<name>");
    Serial.println("List device names:  list");
    Serial.println("Print help:         h");

  // process a valve set command ('set' followed by a space)
  } else if (strncmp(buf, "set ", 4) == 0) {
    i += 4;

    enum valve which_valve = NR_VALVES;

    // then either 'all' followed by a space
    if (strncmp(buf + i, "all ", 4) == 0) {
      i += 4;

    // or the short name of a valve
    } else {
      bool found_valid_name = false;
      for (enum valve v = FIRST_VALVE; v < NR_VALVES; v = next_valve(v)) {
        const char *sname = valve_properties[v].short_name;
        size_t len = strlen(sname);

        // do we match this valve name?
        if (strncmp(buf + i, sname, len)) {
          found_valid_name = true;
          which_valve = v;

          // after the valve name comes another space
          if (buf[i++] != '')
            goto invalid;
        }
      }

      if (!found_valid_name)
        goto invalid;
    }

    // okay, we found out which valve(s) we're opening, now find out
    // how much we're opening them. This is a number, 0-255. Anything
    // non-zero means "open" for solenoid valves.

  // process a sensor reading command ('read' followed by a space)
  } else if (strncmp(buf, "read ", 5) == 0) {

  // process a device list command (just the string 'list'
  } else if (strcmp(buf, "list") == 0) {
    
  } else {
    // otherwise, we didn't recognize this command
    goto invalid;
  }

invalid:
  Serial.println("invalid command. Type h for help");
}

void setup() {
  // put your setup code here, to run once:
  setup_all_valves();
  Serial.begin(9600);
  Serial.setTimeout(1000UL*60UL*60UL);

  Serial.println("Welcome to the ELET system cli. Type 'h' for help");
  clear_buffer();
}

// if someone types something on serial, move to the next bin
void serialEvent() {
  char c;

  while ((c = Serial.read()) != -1) {
    if (sz == 0) {
      Serial.println("buffer overflow! emptying it now");
      clear_buffer();
    }

    if (c == '\n') {
      process_command();
      clear_buffer();
    }

    buf[++idx] = c;
    --sz;
  }
}

void loop() {
  // put your main code here, to run repeatedly:
}
