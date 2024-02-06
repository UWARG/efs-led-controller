# UWARG Lighting Board Firmware

This repository containst a small program written to control the navigation,
beacon, and anti-collision lights of UWARG's LED lighting boards. It is written
with flash memory limitations in mind, as it is intended to run on an ATtiny202
microcontroller with only 2KB of flash memory. Because of this, this program
features a pared down version of the Adafruit NeoPixel library.

Physically, the lighting of the drone is provided by four lighting control
boards, one on each motor. Each of these boards hosts an ATtiny202 which will 
run this program.

To enable different lighting for port and starboard lighting boards, the program
detects the presence of a jumper between pins PA2 and PA3 (by default). If a jumper
is present, this program will display starboard-side lighting, otherwise it will display
port-side lighting.

# Changing Lighting
To change the color, strobe frequncy, and duty cycle of the lighting, edit the
definitions present in `main.cpp`:
```cpp
// Light cycle duration configuration
#define INCREMENT_DUR 100     // Duration of a lighting increment in ms

// Color of port side nav lights (0x00rrggbb)
#define NAV_LIGHT_P_COLOR       0x00FF0000
// Color of starboard side nav lights (0x00rrggbb)
#define NAV_LIGHT_S_COLOR       0x0000FF00

// Color of beacon lights (0x00rrggbb)
#define BEA_LIGHT_COLOR         0x00FF0000

// Beacon strobe timings
#define BEA_LIGHT_INCR_HI 12  // Duration beacon lights will be on each strobe (in increments)
#define BEA_LIGHT_INCR_LO 12  // Duration beacon lights will be off each strobe (in increments)

// Color of anti-collision lights (0x00rrggbb)
#define COL_LIGHT_COLOR       0x00FFFFFF

// Color of off lights -- should always be 0x00000000
#define OFF_COLOR             0x00000000

// Anti-collision strobe timings
#define COL_LIGHT_INCR_HI 10  // Duration anti-collision lights will be on each strobe (in increments)
#define COL_LIGHT_INCR_LO 11  // Duration anti-collision lights will be off each strobe (in increments)
```
