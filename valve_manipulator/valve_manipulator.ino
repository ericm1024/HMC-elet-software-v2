#include "elet_arduino.h"

char buf[64];
int i = 0;
float mass = 0;
long count = 0;

void setup() {
  setup_all_valves();

  Serial.begin(9600);

  Serial.println("#########################################");
  Serial.println("####### valve mainpulator (SPRAY) #######");
  Serial.println("#########################################");
  Serial.println("");
}

void serialEvent() {
  int c;
  while ((c = Serial.read()) != -1) {
    // echo what the user typed
    Serial.print((char)c);
    
    // end of command: parse it and do something
    if (c == '\r') {
      Serial.print((char)'\n');
      int j = 0;
      enum valve which_valve;

      if (strcmp(buf, "off") == 0) {
        for (enum valve v = FIRST_VALVE; v < NR_VALVES; v = next_valve(v))
          close_valve(v);
      } else {
  
        // try to match a valve name
        bool found_valid_name = false;
        for (enum valve v = FIRST_VALVE; v < NR_VALVES; v = next_valve(v)) {
          const char *sname = valve_properties[v].short_name;
          size_t len = strlen(sname);
  
          // do we match this valve name?
          if (strncmp(buf + j, sname, len) == 0) {
            found_valid_name = true;
            which_valve = v;
            j += len;
  
            // after the valve name comes another space
            if (buf[j++] != ' ')
              goto invalid;

            break;
          }
        }
  
        if (!found_valid_name)
          goto invalid;
  
        // match either 'on' or 'off'
        bool on = false;
        if (strcmp(buf + j, "on") == 0) {
          j += 3;
          on = true;
        } else if (strcmp(buf + j, "off") == 0) {
          j += 4;
          on = false;
        } else {
          goto invalid;
        }
 
        // we should be at the end of a command now
        if (buf[j] != 0)
          goto invalid;
  
        // do what the command asked for
        if (on)
          open_valve(which_valve);
        else
          close_valve(which_valve);
      }

      i = 0;
      memset(buf, 0, sizeof buf);
    } else if (i == (sizeof buf) - 1) {
      Serial.println("buffer overflow. resetting to beginning");
      i = 0;
      memset(buf, 0, sizeof buf);
    } else {
      buf[i++] = c;
    }
  }
  return;
  
invalid:
  Serial.println("Invalid command, resetting buffer");
  i = 0;
  memset(buf, 0, sizeof buf);
}

void loop() {}
