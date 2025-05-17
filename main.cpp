#include "mbed.h"

// Pin configuration for 7-segment display shield (NUCLEO-F401RE)
DigitalOut latch(PB_5);    // D4 -> LCHCLK (Latch pin)
DigitalOut clk(PA_8);      // D7 -> SFTCLK (Shift clock)
DigitalOut data(PA_9);     // D8 -> SDI (Serial data input)
DigitalIn  button1(PA_1);  // S1 -> A1 (Reset button, active low)
DigitalIn  button3(PB_0);  // S3 -> A3 (Mode toggle button, active low)
AnalogIn   pot(PA_0);      // Potentiometer connected to A0

// Segment patterns for digits 0–9 on a common-anode 7-segment display (active low)
const uint8_t SegMap[10] = {
    0xC0, 0xF9, 0xA4, 0xB0, 0x99,
    0x92, 0x82, 0xF8, 0x80, 0x90
};

// Digit enable lines for a 4-digit display (active low)
const uint8_t SegSlct[4] = { 0xF1, 0xF2, 0xF4, 0xF8 };

// Shared variables for state management
volatile int ttlseconds = 0;
volatile bool Display = true;
Ticker tickseconds;
Ticker tickrefresh;
volatile int Digit = 0;

// Timer interrupt: increments elapsed time in seconds
void tick() {
    ttlseconds++;
    if (ttlseconds >= 6000) ttlseconds = 0;  // Roll over at 99 minutes 59 seconds
}

// Timer interrupt: flag to update display
void refreshISR() {
    Display = true;
}

// Send segment and digit data to the 74HC595 shift registers
void outputToDisplay(uint8_t segments, uint8_t digitSelect) {
    latch = 0;
    // Send segment bits (MSB first)
    for (int i = 7; i >= 0; --i) {
        data = (segments >> i) & 0x1;
        clk = 0; clk = 1;
    }
    // Send digit selection bits
    for (int i = 7; i >= 0; --i) {
        data = (digitSelect >> i) & 0x1;
        clk = 0; clk = 1;
    }
    latch = 1;
}

int main() {
    // Enable internal pull-ups for buttons
    button1.mode(PullUp);
    button3.mode(PullUp);

    // Initialize periodic timers
    tickseconds.attach(&tick, 1.0);           // Tick every 1 second
    tickrefresh.attach(&refreshISR, 0.002);  // Refresh display every 2 milliseconds

    // Start in time display mode
    bool Voltagemode = false;
    int Prevb1 = 1, prevb3 = 1;

    while (true) {
        // --- Reset time on button S1 press ---
        int b1 = button1.read();
        if (b1 == 0 && Prevb1 == 1) {
            ttlseconds = 0;
        }
        Prevb1 = b1;

        // --- Toggle to voltage mode while holding button S3 ---
        int b3 = button3.read();
        Voltagemode = (b3 == 0);  // Active only while button is pressed
        prevb3 = b3;

        // --- Display update logic ---
        if (Display) {
            Display = false;

            uint8_t Byte = 0xFF, Byte2 = 0xFF;

            if (!Voltagemode) {
                // Show elapsed time in MM:SS format
                int seconds = ttlseconds % 60;
                int minutes = ttlseconds / 60;

                switch (Digit) {
                    case 0: Byte = SegMap[minutes / 10]; Byte2 = SegSlct[0]; break;
                    case 1: Byte = SegMap[minutes % 10] & 0x7F; Byte2 = SegSlct[1]; break; // Add colon/dot
                    case 2: Byte = SegMap[seconds / 10]; Byte2 = SegSlct[2]; break;
                    case 3: Byte = SegMap[seconds % 10]; Byte2 = SegSlct[3]; break;
                }
            } else {
                // Show voltage from potentiometer in format X.XXX V
                float volts = pot.read() * 3.3f;
                int millivolts = (int)(volts * 1000.0f);
                if (millivolts > 9999) millivolts = 9999;

                int inte = millivolts / 1000;
                int frac = millivolts % 1000;

                switch (Digit) {
                    case 0: Byte = SegMap[inte] & 0x7F; Byte2 = SegSlct[0]; break; // Show decimal point
                    case 1: Byte = SegMap[frac / 100]; Byte2 = SegSlct[1]; break;
                    case 2: Byte = SegMap[(frac % 100) / 10]; Byte2 = SegSlct[2]; break;
                    case 3: Byte = SegMap[frac % 10]; Byte2 = SegSlct[3]; break;
                }
            }

            outputToDisplay(Byte, Byte2);
            Digit = (Digit + 1) % 4;
        }

        // Optional sleep for power saving
        // ThisThread::sleep_for(1ms);
    }
}