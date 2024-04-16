#include <TM1637Display.h>

// Define the connection pins for the displays
#define CLK1 A1
#define DIO1 A0
#define CLK2 A3
#define DIO2 A2
// Define the pins for the switches
#define SWITCH_PIN1 A4
#define SWITCH_PIN2 A5
// Define the pin for the pause button
#define PAUSE_BUTTON_PIN 7

// Create display objects
TM1637Display display1(CLK1, DIO1);
TM1637Display display2(CLK2, DIO2);

// Timer countdown values
unsigned long timePlayer1 = 600; // 10 minutes in seconds
unsigned long timePlayer2 = 600; // 10 minutes in seconds

// Timer control
bool timer1Running = false;
bool timer2Running = true; // Player 2 starts the game
bool paused = false;       // Pause state

unsigned long lastMillis; // Last update time

void setup() {
  pinMode(SWITCH_PIN1, INPUT_PULLUP);
  pinMode(SWITCH_PIN2, INPUT_PULLUP);
  pinMode(PAUSE_BUTTON_PIN, INPUT_PULLUP);

  // Initialize the displays
  display1.setBrightness(0x0f);
  display2.setBrightness(0x0f);
  display1.clear();
  display2.clear();

  // Start the clock
  lastMillis = millis();
}

void loop() {
  // Read the state of the pause button
  bool pauseButtonState = digitalRead(PAUSE_BUTTON_PIN) == LOW;
  static bool lastPauseButtonState = HIGH;

  if (pauseButtonState && lastPauseButtonState != pauseButtonState) {
    paused = !paused; // Toggle the paused state
    delay(50); // Debounce delay
  }
  lastPauseButtonState = pauseButtonState;

  // Read the state of the switches
  bool switchState1 = digitalRead(SWITCH_PIN1) == LOW;
  bool switchState2 = digitalRead(SWITCH_PIN2) == LOW;

  // Handle switch presses
  if (switchState1 && timer1Running) {
    timer1Running = false;
    timer2Running = true;
  } else if (switchState2 && timer2Running) {
    timer2Running = false;
    timer1Running = true;
  }

  // Update the timers
  unsigned long currentMillis = millis();
  if (!paused && currentMillis - lastMillis >= 1000) {
    if (timer1Running && timePlayer1 > 0) {
      timePlayer1--;
    }
    if (timer2Running && timePlayer2 > 0) {
      timePlayer2--;
    }
    lastMillis = currentMillis;
  }

  // Update the displays
  display1.showNumberDecEx(timePlayer1, 0b01000000, true); // Colon on for display 1
  display2.showNumberDecEx(timePlayer2, 0b01000000, true); // Colon on for display 2
}
