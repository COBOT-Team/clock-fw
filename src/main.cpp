#include <Arduino.h>
#include <TM1637Display.h>

//                                                                                                //
// ====================================== Pin Definitions ======================================= //
//                                                                                                //

const int PAUSE_PIN = 7;
const int LIM_PIN[] = {A5, A4};

TM1637Display displays[] = {TM1637Display(A3, A2), TM1637Display(A1, A0)};

//                                                                                                //
// ========================================= Constants ========================================== //
//                                                                                                //

enum Color {
    WHITE = 0,
    BLACK = 1,
};

const unsigned long BLINK_INTERVAL = 500;
const unsigned long SERIAL_REPORT_INTERVAL = 1000;

//                                                                                                //
// ========================================= Game State ========================================= //
//                                                                                                //

unsigned long time_left[2] = {0, 0};
unsigned long last_time_update = 0;
Color current = WHITE;

bool button_states[2] = {false, false};
bool pause_was_pressed = false;
unsigned long paused_time = 0;

unsigned long last_serial_report = 0;

bool game_running = false;
bool paused = false;

unsigned long initial_time = 0;
unsigned long time_increment = 0;

char input_string[128];
size_t input_string_idx = 0;

//                                                                                                //
// ===================================== Utility Functions ====================================== //
//                                                                                                //

/**
 * Update the measured time for both colors. Should be called regularly, especially before making
 * any state changes or sending updates.
 *
 * @param[in] current_time The current time in milliseconds.
 */
void update_time(unsigned long current_time) {
    if (last_time_update == 0) {
        last_time_update = current_time;
        return;
    }

    if (paused) {
        last_time_update = current_time;
        return;
    }

    unsigned long elapsed = current_time - last_time_update;
    last_time_update = current_time;
    if (elapsed > time_left[current]) {
        time_left[current] = 0;
        game_running = false;
        return;
    }

    time_left[current] -= elapsed;
}

/**
 * Update the time remaining on both displays.
 */
void display_time() {
    for (int i = 0; i < 2; ++i) {
        const int mins = time_left[i] / 60000;
        const int secs = (time_left[i] / 1000) % 60;
        displays[i].showNumberDecEx(mins * 100 + secs, 0b01000000, true);
    }
}

/**
 * Clear both displays.
 */
void clear_displays() {
    for (auto& display : displays) {
        display.clear();
    }
}

/**
 * Send a serial update with the current state.
 *
 * @param[in] current_time The current time in milliseconds.
 */
void send_serial_update(unsigned long current_time) {
    last_serial_report = (current_time / SERIAL_REPORT_INTERVAL) * SERIAL_REPORT_INTERVAL;
    Serial.print("upd w ");
    Serial.print(time_left[WHITE]);
    Serial.print(" b ");
    Serial.print(time_left[BLACK]);
    Serial.print(" t ");
    Serial.print(current == WHITE ? "white" : "black");
    Serial.print(" p ");
    Serial.print(paused ? "true" : "false");
    Serial.print(" a ");
    Serial.print(game_running ? "true" : "false");
    Serial.println();
}

/**
 * Send a serial update with the current button states.
 */
void send_button_update() {
    Serial.print("btn w ");
    Serial.print(button_states[WHITE] ? "true" : "false");
    Serial.print(" b ");
    Serial.print(button_states[BLACK] ? "true" : "false");
    Serial.println();
}

/**
 * Update the button states.
 */
void update_button_states() {
    const bool w = digitalRead(LIM_PIN[WHITE]) == LOW;
    const bool b = digitalRead(LIM_PIN[BLACK]) == LOW;
    const bool updated = w != button_states[WHITE] || b != button_states[BLACK];

    button_states[WHITE] = w;
    button_states[BLACK] = b;
    if (updated) send_button_update();
}

/**
 * Parse and handle a command recieved over serial.
 *
 * @param[in] command The full command received over serial.
 * @param[in] current_time The current time in milliseconds.
 */
void handle_command(String command, unsigned long current_time) {
    if (command.startsWith("rst")) {
        unsigned long base, inc;
        int res = sscanf(command.c_str(), "rst %lu %lu", &base, &inc);
        switch (res) {
            case 2:
                time_left[WHITE] = base;
                time_left[BLACK] = base;
                time_increment = inc;
                current = WHITE;
                game_running = true;
                Serial.println("ack");
                send_serial_update(current_time);
                return;
            default:
                Serial.println("rej Argument error");
                return;
        }
    }

    if (command.startsWith("set")) {
        char w[8];
        char b[8];
        int res = sscanf(command.c_str(), "set w %s b %s", w, b);
        switch (res) {
            case 2:
                if (strcmp(w, "-") != 0) {
                    time_left[WHITE] = atol(w);
                }
                if (strcmp(b, "-") != 0) {
                    time_left[BLACK] = atol(b);
                }
                Serial.println("ack");
                send_serial_update(current_time);
                return;
            default:
                Serial.println("rej Argument error");
                return;
        }
    }

    if (command.startsWith("pause")) {
        paused = true;
        Serial.println("ack");
        send_serial_update(current_time);
        return;
    }

    if (command.startsWith("resume")) {
        paused = false;
        Serial.println("ack");
        send_serial_update(current_time);
        return;
    }
}

//                                                                                                //
// ==================================== Arduino Control Flow ==================================== //
//                                                                                                //

void setup() {
    Serial.begin(9600);
    Serial.setTimeout(1000);

    pinMode(PAUSE_PIN, INPUT_PULLUP);
    pinMode(LIM_PIN[WHITE], INPUT_PULLUP);
    pinMode(LIM_PIN[BLACK], INPUT_PULLUP);

    for (auto& display : displays) {
        display.setBrightness(1);
        display.clear();
    }
}

void loop() {
    unsigned long current_time = millis();
    update_time(current_time);

    // Check for new serial commands.
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n') {
            input_string[input_string_idx] = '\0';
            handle_command(String(input_string), current_time);
            input_string_idx = 0;
        } else {
            input_string[input_string_idx++] = c;
        }
    }

    // Check for button presses.
    bool pause_pressed = digitalRead(PAUSE_PIN) == LOW;
    if (game_running && current_time - paused_time > 200) {
        if (pause_pressed) {
            if (!pause_was_pressed) {
                pause_was_pressed = true;
                paused = !paused;
                paused_time = current_time;
                send_serial_update(current_time);
                return;
            }
        } else
            pause_was_pressed = false;
    }
    update_button_states();
    if (button_states[current]) {
        time_left[current] += time_increment;
        current = (Color)(1 - current);
        send_serial_update(current_time);
        delay(100);
        return;
    }

    // Update the displays.
    if (paused || !game_running) {
        const int cycle = (current_time / BLINK_INTERVAL) % 2;
        if (cycle == 0)
            display_time();
        else
            clear_displays();
    }

    // Send regular serial updates.
    if (current_time - last_serial_report >= SERIAL_REPORT_INTERVAL) {
        send_button_update();
        send_serial_update(current_time);
        display_time();
    }
}