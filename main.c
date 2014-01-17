/*
 Application Name: Adon
 Description: A simon clone
 Author: Adam Olsen
 Email: arolsen@gmail.com
 Date: Jan 14, 2014
 Version: 0.5
 License: Apache v2

 This is written for the Amtel ATtiny84 chip.  A schematic and a board for this 
 can be found in eagle/ directory.
*/

#define F_CPU 8000000UL
#include <avr/eeprom.h>
#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>

// musical notes
#define NOTE_E3 165
#define NOTE_CS3 139
#define NOTE_A3 220
#define NOTE_E2 82
#define NOTE_C3 131
#define NOTE_A5 880

// define led pins
#define LED1 PA3
#define LED2 PA2
#define LED3 PA1
#define LED4 PA0

// define button pins
#define BUTTON1 PA7
#define BUTTON2 PA6
#define BUTTON3 PA5
#define BUTTON4 PA4

// game mode DIP switch
#define GAMEMODE1 PB1
#define GAMEMODE2 PB0

// various game constants
#define ERROR_TONE NOTE_C3
#define WON_TONE NOTE_A5
#define MAX_LEVELS 50
#define NEXT_GAME_PAUSE_DURATION 800
#define INCREASE_SPEED_LEVELS 3
#define INCREASE_SPEED_AMOUNT 20
#define PAUSE_DURATION 200
#define INITIAL_TONE_DURATION 500
#define MAX_SPEED 200
#define MAX_LOOPS 200000UL

const unsigned int TONES_FOR_BUTTON[5] = {NOTE_E3, NOTE_CS3, NOTE_A3, NOTE_E2,
    NOTE_A3};
const uint8_t LEDS[4] = {LED1, LED2, LED3, LED4};
const uint8_t BUTTONS[4] = {BUTTON1, BUTTON2, BUTTON3, BUTTON4};

uint8_t game_mode;
uint8_t button_pressed[4];
uint8_t level_sequence[10];
uint8_t level;
uint8_t input_mode;
unsigned long loop_count;
uint8_t current_step;
unsigned int tone_duration;
unsigned int randint;

void delay(long millis);
void no_tone(void);
void tone(int frequency, long delay);
void led_on(uint8_t led);
void led_off(uint8_t led);
void setup_gamemode(void);
void setup_level(void);
void reset_game(void);
void game_over(void);
void game_won(void);
void button_press(uint8_t index);
void button_release(uint8_t index);
void check_button_press(uint8_t index);

int main(void) {
    // do initial set up of the timer - set it up to toggle OC0A output (which
    // is PB2), and set it to ctc mode.
    TCCR0A = _BV(WGM01) | _BV(COM0A0);   

    // set PB2 as an output
    DDRB = _BV(PB2); 

    // set LED pins as outputs
    DDRA = _BV(LED1) | _BV(LED2) | _BV(LED3) | _BV(LED4);
    
    // enable pullup resistors on the button pins
    PORTA |= _BV(BUTTON1) | _BV(BUTTON2) | _BV(BUTTON3) | _BV(BUTTON4);

    // enable pullup resistors on the game mode dip switches
    PORTB |= _BV(GAMEMODE1) | _BV(GAMEMODE2);

    // get a random number to seed with
    srand(eeprom_read_word(&randint));

    // write the random seed number for next time ;-)
    eeprom_write_word(&randint, rand());

    // initialize variables
    level = 1;
    game_mode = 0;
    input_mode = 0;
    current_step = -1;
    tone_duration = INITIAL_TONE_DURATION;
    for(uint8_t i = 0; i < 4; i++) button_pressed[i] = 0;

    // loop forever
    while(1) { 
        if(!input_mode) {
            setup_level();
            input_mode = 1;
        }
        else {
            // check for button presses
            for(uint8_t i = 0; i < 4; i++) {
                check_button_press(i);
            }

            if(loop_count > MAX_LOOPS) {
                game_over();
            }

            loop_count++;
        }
    }
}

void delay(long millis) {
    for(uint8_t i = 0; i < millis; i++) _delay_ms(1);
}

void no_tone(void) {
    PORTB &= ~_BV(PB2);
    TCCR0B = 0x00;                       // turn timer off completely
}

void tone(int frequency, long delay_millis) {
    OCR0A = ((F_CPU / 1024.0) / (frequency * 2.0)) - 1;
    TCCR0B = _BV(CS02) | _BV(CS00);      // set prescaler to 1024 

    if(delay_millis > -1) {
        delay(delay_millis);
        no_tone();
    }
}

void led_on(uint8_t led) {
    PORTA |= _BV(led);
}

void led_off(uint8_t led) {
    PORTA &= ~_BV(led);
}

/*
 * Checks the game mode dip switch, and sets the game type accordingly.  There
 * are four different game modes:
 *
 * 1. Progressing steps, not increasing speed (easy, PB0 and PB1 off)
 * 2  Progressing steps, increasing speed (medium, PB0 on PB1 off)
 * 3. Random steps, not increasing speed (hard, PB0 off and PB1 on)
 * 4. Random steps, increasing speed (super hard, PB0 and PB1 on)
 */
void setup_gamemode(void) {
    if(!bit_is_clear(PINB, PB0) && !bit_is_clear(PINB, PB1)) 
        game_mode = 0;
    else if(bit_is_clear(PINB, PB0) && !bit_is_clear(PINB, PB1)) 
        game_mode = 1;
    else if(!bit_is_clear(PINB, PB0) && bit_is_clear(PINB, PB1))
        game_mode = 2;
    else if(bit_is_clear(PINB, PB0) && bit_is_clear(PINB, PB1)) 
        game_mode = 3;
    else
        game_mode = 0;
}

void setup_level(void) {
    loop_count = 0;
    setup_gamemode();

    uint8_t increase_speed = (game_mode == 1 || game_mode == 3);

    if(increase_speed && level % INCREASE_SPEED_LEVELS == 0) {
        int mult = level / INCREASE_SPEED_LEVELS;
        tone_duration -= INCREASE_SPEED_AMOUNT * mult;
        if(tone_duration < MAX_SPEED) {
            tone_duration = MAX_SPEED;
        }
    }

    current_step = -1;
    uint8_t start_sequence = 0;
    if(game_mode < 2) {
        start_sequence = level - 1;
    }

    for(uint8_t i = 0; i < level; i++) {
        uint8_t step;

        if(start_sequence > 0 && game_mode < 2) {
            if(start_sequence == i) {
                step = rand() % 4;
            }
            else {
                step = level_sequence[i];
            }
        }
        else {
            step = rand() % 4;
        }

        level_sequence[i] = step;
        led_on(LEDS[step]);
        tone(TONES_FOR_BUTTON[step], tone_duration);
        led_off(LEDS[step]);
        delay(PAUSE_DURATION);
    }
}

void reset_game(void) {
    tone_duration = INITIAL_TONE_DURATION;
    level = 1;
    input_mode = 0;
    loop_count = 0;
}

void game_over(void) {
    reset_game();
    for(uint8_t i = 0; i < 4; i++) {
        led_on(LEDS[i]);
    }
    tone(ERROR_TONE, 1000);
    delay(NEXT_GAME_PAUSE_DURATION * 2);
    for(uint8_t i = 0; i < 4; i++) {
        led_off(LEDS[i]);
    }
    delay(NEXT_GAME_PAUSE_DURATION * 2);
}

void game_won(void) {
    reset_game();
    delay(200);
    for(uint8_t i = 0; i < 8; i++) {
        for(uint8_t a = 0; a < 4; a++) {
            led_on(LEDS[a]);
            tone(WON_TONE, 100);
            delay(50);
            led_off(LEDS[a]);
        }
    }

    delay(NEXT_GAME_PAUSE_DURATION);
}

void button_press(uint8_t index) {
    // turn LED on for this button
    led_on(LEDS[index]);

    // play tone for this button
    tone(TONES_FOR_BUTTON[index], -1);
}

void button_release(uint8_t index) {
    // turn LED off for this button
    led_off(LEDS[index]);

    // turn tone off for this button
    no_tone();
    
    current_step += 1;
    if(index != level_sequence[current_step]) {
        game_over();
        return;
    }

    if(current_step + 1 >= level) {
        input_mode = 0;
        level ++;

        if(level > MAX_LEVELS) {
            game_won();
            return;
        }

        delay(NEXT_GAME_PAUSE_DURATION);
    }
}

void check_button_press(uint8_t index) {
    if(bit_is_clear(PINA, BUTTONS[index])) {
        delay(50);
        if(bit_is_clear(PINA, BUTTONS[index])) {
            button_pressed[index] = 1;
            button_press(index);
        }
    }
    
    if(button_pressed[index] && !bit_is_clear(PINA, BUTTONS[index])) {
        delay(50);
        if(!bit_is_clear(PINA, BUTTONS[index])) {
            button_pressed[index] = 0;
            button_release(index);
        }
    }
}
