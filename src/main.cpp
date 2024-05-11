/*
LED Board Firmwawre
Author: Owen Grimm

This program controls 6 connected NeoPixel lights according to the following standard:
  - 2 LEDs permanently on, red if port side, green if starboard side (see below)
  - 2 LEDs strobing red as beacon lights
  - 2 LEDs strobing white as anti-collision lights

Whether lights for the port or starboard side are running is determined by a jumper
connecting pins PA3 and PA2 on the board. If the jumper is present (ie. PA3 & PA2 are
connected) **at startup**, then the program will display lights appropriate for the
starboard side. If the jumper is not present at startup, then the board will display
lights appropriate for the port side.

This program makes several tradeoffs in the sake of keeping the program size small
enough to fit in the ATtinys202's 8KB flash memory. If you see any strange design choices,
this is most likely why.

ASSUMPTIONS:
 - Timing of the strobes is not critical
   - This program does not use any high-accuracy timing strategies, also the neopixel
     library has some interrupt disabling functions to preserve comm timing that throws
     off some of the Arduino timing functions.
*/

#include <Arduino.h>
#include <tinyNeoPixel_Static.h>

// ==================== CONNECTION CONFIGURATION ==================== //

// Port/Starboard config pins, MUST BE ON GPIO PORT A
#define DIR_JUMP_PIN_O_BITMASK  PIN2_bm
#define DIR_JUMP_PIN_I_BITMASK  PIN3_bm
#define DIR_JUMP_PIN_I_
#define DIR_JUMP_PIN_I_CONTROL  PORTA_PIN3CTRL  // Ensure correct port and pin

// Neopixel connection
#define NEOPIXEL_PIN PIN_PA1
#define NEOPIXEL_CNT 6

// Neopixel transmission
// How many data bytes need to be sent per neopixel (most likely 3)
#define NEOPIXEL_BYTES_PER_PIXEL 3

// Indices (0-based) of the different light types from the start of the NeoPixel chain
#define NAV_LIGHT_A_IDX 1   // Index of the 1st navigation light 
#define NAV_LIGHT_B_IDX 2   // Index of the 2nd navigation light
#define BEA_LIGHT_A_IDX 3   // Index of the 1st beacon light
#define BEA_LIGHT_B_IDX 4   // Index of the 2nd beacon light
#define COL_LIGHT_A_IDX 5   // Index of the 1st anti-collision light
#define COL_LIGHT_B_IDX 0   // Index of the 2nd anti-collision light

// Pixel format info
#define PIXEL_WIDTH 3
#define R_OFFSET 1
#define B_OFFSET 2
#define G_OFFSET 0

// ==================== MAGIC NUMBERS ==================== //

// for setting and clearing global interrupt bit
#define GLOBAL_INTERRUPT_BITMASK      0x80
#define GLOBAL_INTERRUPT_BITMASK_INV  ~GLOBAL_INTERRUPT_BITMASK

// ==================== LIGHTING CONFIGURATION ==================== //
// The total strobe times of the beacon lights and anti-collision lights
// should be coprime to maximize the time before they synchronize.

// To control the beacon and anti-collision blinking without using too much 
// flash memory, a very simple control scheme is used. First, an interval of
// time (INCREMENT_DUR) is chosen to which the lights will synchronize. Each
// blinking light has a duration (in increments) for which it should stay on and
// stay off. Each time `INCREMENT_DUR` ms have passed, the duration each light
// has been in its current state (on/off) is incremented. When the light has 
// been on/off for enough increments, its state is toggled and the its duration
// in counter state is reset.

// Light cycle duration configuration
// this is which value the 16-bit counter should roll over at. with 20MHz clock
// and 1024 prescale, counter will count up once every (1024 / 20,000,000)s.
// Therefore, 0x0000 -> 0s increments, 0xFFFF -> 3.35544s. 
// To find the overflow value for a given time between 0 and 3.355 seconds, use
// the formula: overflow val = desired time * (20,000,000 / 1024)
// eg:
//      100ms -> overflow val = 1953
#define INCREMENT_OVERFLOW 1953 // ~100ms

// Color of port side nav lights (0x00rrggbb)
#define NAV_LIGHT_P_COLOR 0x00FF0000
// Color of starboard side nav lights (0x00rrggbb)
#define NAV_LIGHT_S_COLOR 0x0000FF00

// Color of beacon lights (0x00rrggbb)
#define BEA_LIGHT_COLOR   0x00FF0000

// Beacon strobe timings
#define BEA_LIGHT_INCR_HI 12  // Duration beacon lights will be on each strobe (in increments)
#define BEA_LIGHT_INCR_LO 12  // Duration beacon lights will be off each strobe (in increments)

// Color of anti-collision lights (0x00rrggbb)
#define COL_LIGHT_COLOR   0x00FFFFFF

// Color of off lights -- should always be 0x00000000
#define OFF_COLOR         0x00000000

// Anti-collision strobe timings
#define COL_LIGHT_INCR_HI 10  // Duration anti-collision lights will be on each strobe (in increments)
#define COL_LIGHT_INCR_LO 11  // Duration anti-collision lights will be off each strobe (in increments)

// ==================== END CONFIGURATION ==================== //

// Enum for direction
enum Side { PORT, STBD };

// NeoPixel control object
// Since we are doing some strange stuff to bypass calls to malloc and free,
// do NOT call the destructor on this before the microcontroller is shutting down,
// otherwise memory corruption will occur.
// Size of this array is dependent on the transmission format of the connected LEDS
byte pixels[NEOPIXEL_CNT * NEOPIXEL_BYTES_PER_PIXEL] = { };  // Pointer to raw neopixel LED data
tinyNeoPixel flight_lights(NEOPIXEL_CNT, NEOPIXEL_PIN, NEO_GRB, pixels);
// SlimAdafruit_NeoPixel flight_lights(NEOPIXEL_CNT, pixels, NEOPIXEL_PIN, NEO_GRB | NEO_KHZ800);

// To store which side of the craft we are on
Side board_side;

// Counters to keep track of how long each light has been in it's current state
uint8_t bea_dur_in_state = 0;
uint8_t col_dur_in_state = 0;

// States of each light (false -> OFF, true -> ON)
bool bea_state = false;
bool col_state = false;

// tracker to count how many increments have passed without lighting updates
// should never climb above 1 but who knows
volatile int increment_updates_needed = 0;

void setup() {
  // Checking if starboard jumper is present
  // Doing some register level manipulation of GPIO to avoid costly Arduino calls
  PORTA_DIRSET = DIR_JUMP_PIN_O_BITMASK;      // Setting PA2 to output mode
  PORTA_OUTCLR = DIR_JUMP_PIN_O_BITMASK;      // Setting PA2 to output LOW
  PORTA_DIRCLR = DIR_JUMP_PIN_I_BITMASK;      // Setting PA3 to input mode
  PORTA_PIN3CTRL = PORT_PULLUPEN_bm;  // Enabling pullup on PA3

  // If `DIR_JUMP_PIN_I` is connected to gnd, we're on starboard, otherwise port
  board_side = PORTA_IN & PIN3_bm ? Side::PORT : Side::STBD;

  // Assigning colors based on board side
  uint32_t nav_color = board_side == Side::PORT ? NAV_LIGHT_P_COLOR : NAV_LIGHT_S_COLOR;

  pinMode(NEOPIXEL_PIN, OUTPUT);

  flight_lights.setPixelColor(NAV_LIGHT_A_IDX, nav_color);
  flight_lights.setPixelColor(NAV_LIGHT_B_IDX, nav_color);
  flight_lights.show();

  // Setting up timer a 0 to generate an interrupt every now and then
  // please note that PWM will NOT work since MegaTinyCore would typically use
  // tca0 for PWM
  takeOverTCA0();
  TCA0_SINGLE_PER  = 0xFFFF;
  TCA0_SINGLE_CTRLB = 0x00;  // periodic interrupt mode

  // div1024 prescale, enabling clock
  // means we will have (1024/20MHz) * 2^16 = 3.355s maximum interval period
  // see 20.5.1 in datasheet
  TCA0_SINGLE_CTRLA = 0b00000011;

  TCA0_SINGLE_INTCTRL = 0b00000001; // enable overflow interrupt

}

void loop() {
  while (increment_updates_needed > 0) {
    bea_dur_in_state++;
    col_dur_in_state++;

    // If we've been in the current state as long as we should be
    if (bea_dur_in_state >= (bea_state ? BEA_LIGHT_INCR_HI : BEA_LIGHT_INCR_LO)) {
      // Switching to other state
      bea_state = !bea_state;

      // Special case when beacon is should be in next state for zero increments
      // must immediately switch back to prior state
      if (0 == bea_state ? BEA_LIGHT_INCR_HI : BEA_LIGHT_INCR_LO)
        bea_state = !bea_state;
      
      // Changing color of pixel based on new state
      if (!bea_state) {
        flight_lights.setPixelColor(BEA_LIGHT_A_IDX, BEA_LIGHT_COLOR);
        flight_lights.setPixelColor(BEA_LIGHT_B_IDX, BEA_LIGHT_COLOR);
      }
      else {
        flight_lights.setPixelColor(BEA_LIGHT_A_IDX, OFF_COLOR);
        flight_lights.setPixelColor(BEA_LIGHT_B_IDX, OFF_COLOR);
      }
      flight_lights.show();

      // Reseting duration in current state
      bea_dur_in_state = 0;
    }

    // If we've been in the current state as long as we should be
    if (col_dur_in_state >= (col_state ? COL_LIGHT_INCR_HI : COL_LIGHT_INCR_LO)) {
      // Switching to other state
      col_state = !col_state;
      
      // Special case when beacon is should be in next state for zero increments
      // must immediately switch back to prior state
      if (0 == col_state ? COL_LIGHT_INCR_HI : COL_LIGHT_INCR_LO)
        col_state = !col_state;

      // Changing color of pixel based on new state
      if (!col_state) {
        flight_lights.setPixelColor(COL_LIGHT_A_IDX, COL_LIGHT_COLOR);
        flight_lights.setPixelColor(COL_LIGHT_B_IDX, COL_LIGHT_COLOR);
      } else {
        flight_lights.setPixelColor(COL_LIGHT_A_IDX, OFF_COLOR);
        flight_lights.setPixelColor(COL_LIGHT_B_IDX, OFF_COLOR);
      }
      flight_lights.show();

      // Reseting duration in current state
      col_dur_in_state = 0;
    }

    // disabling interrupts to avoid race conditions
    SREG &= GLOBAL_INTERRUPT_BITMASK_INV; // setting global interrupt bit to 0
    increment_updates_needed -= 1;
    SREG |= GLOBAL_INTERRUPT_BITMASK;     // setting global interrupt bit to 1
  }
}

// todo: add 1 to increment
ISR(TCA0_OVF_vect) {
  increment_updates_needed += 1;

  // manually clearing interrupt flag, not done in hardware
  TCA0_SINGLE_INTFLAGS &= 0b11111110;
}
