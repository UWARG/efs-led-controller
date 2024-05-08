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

# Flashing

Flashing this firmware onto the ATtiny202 is accomplished using [jtag2updi](https://github.com/ElTangas/jtag2updi), a tool that uses an Arduino Uno or Mega to as an intermediate device to convert JTAG programming signals to UPDI (used by ATtiny MCUs for single-wire programming).

## Setup

### Running jtag2updi
Before flashing the board, the jtag2updi Arduino sketch must be flashed and running on an Arduino Uno or Mega. Instructions for this can be found in [jtag2updi's readme](https://github.com/ElTangas/jtag2updi?tab=readme-ov-file#building-with-arduino-ide), and are also outlined below:

1. Download or clone the jtag2updi repository, unzipping if necessary.

![image](https://github.com/UWARG/efs-led-controller/assets/12762677/06fb1621-5e67-41bb-b3d7-9c5953ef8c6a)

3. Open the `source/jtag2updi.ino` sketch file in the repository with Arduino IDE (File -> Open / Ctrl+O)
4. Connect the Arduino Uno/Mega that will host the jtag2updi sketch, verify the correct COM port & board are selected, and upload the sketch (Button on top left / Sketch -> Upload / Ctrl+U)

### Wiring
Before wiring the Arduino and lighting board for flashing, ensure both are unplugged and disconnected from power.

The diagram below shows the complete wiring setup needed to flash the lighting board:
```     
            +----------------+                   +----------------+
       +----| RST        +5V |-------------------| +5V            |
       |    |            GND |-------------------| GND            |
     +-+-+  |                |                   |                |
     |100|  |                |                   |                |
     |ohm|  |                |      +------+     |                |
     +-+-+  |            OUT |------+ 4.7k +-----| PA0            |
       |    |                |      +------+     | Lighting Board |
       +----| +5V            |                   +----------------+
            |    UNO/MEGA    |
            +----------------+

        OUT: Digital Pin  6 for UNO
             Digital Pin 18 for MEGA
```

The pinouts for each header are shown below, make sure you are using the correct pinout for the correct header using the J1 and J2 silkscreen markings as a guide.

![image](https://github.com/UWARG/efs-led-controller/assets/12762677/c9f7ea50-dd3f-492e-ad0f-02290f2324ef)
![image](https://github.com/UWARG/efs-led-controller/assets/12762677/91c9ffbf-502b-45aa-ac4c-b58adfdc9351)

The 100 ohm resistor between RST and GND on the Arduino is present to override the Arduino's behavior of resetting when a USB serial connection is made, which would otherwise prevent the jtag2updi sketch from working properly. Note, this wiring must be completed AFTER flashing the jtag2updi sketch onto the Arduino, as the 100 ohm resistor between RST and GND on the Arduino interferes with programming.

After double checking the wiring and plugging in the Arduino, you can proceed to flashing the lighting board with PlatformIO.

## Flashing with PlatformIO & VS Code
To flash the lighting board, clone this repository and open it with VS Code. Ensure that the PlatformIO extension for VS Code is installed and working (the UWARG laptop should have it installed if you want to skip the hassle). Open the cloned repository and wait for PlatformIO to initialize. Ensure you are on the latest main branch by clicking the git status badge on the bottom left of the screen, and selecting remote/main in the resulting popup.

![image](https://github.com/UWARG/efs-led-controller/assets/12762677/352fac83-aeb3-4f6a-a811-eb0b102f1ac4)

Once PlatformIO is initialized, click on the PlatformIO icon in the primary side bar.

![image](https://github.com/UWARG/efs-led-controller/assets/12762677/063414e0-7123-4c98-983d-dba73b2b17d2)

From there, in the sidebar, expand the ATtiny202 folder, and click on the Upload option. PlatformIO will then open a new terminal window and begin flashing the lighting board PCB using jtag2updi. If a progress bar saying "Verifying Flash Memory 100%" appears (or something along those lines), the board has been flashed successfully.
