#include <stdlib.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/flash.h>

#define F_CPU 4000000 // 4Mhz

// constants
#define NOTE_E3                         165
#define NOTE_CS3                        139
#define NOTE_A3                         220
#define NOTE_E2                         82
#define NOTE_C3                         131
#define NOTE_C2                         65
#define NOTE_A5                         880

#define ERROR_TONE                      NOTE_C2
#define WON_TONE                        NOTE_A5
#define MAX_LEVELS                      20
#define NEXT_GAME_PAUSE_DURATION        800
#define INCREASE_SPEED_LEVELS           3
#define INCREASE_SPEED_AMOUNT           20
#define PAUSE_DURATION                  250
#define INITIAL_TONE_DURATION           500
#define MAX_SPEED                       200
#define MAX_LOOPS                       (F_CPU * 4) / 64
#define TIM3_PRESCALER                  4

#define FLASH_RAND_OPERATION_ADDRESS    ((uint32_t)0x08003c00)
#define FLASH_PAGE_SIZE                 0x400
#define RESULT_OK                       0

const uint8_t LEDS[4] = {GPIO0, GPIO1, GPIO2, GPIO3};
const uint16_t BUTTONS[4] = {GPIO4, GPIO5, GPIO6, GPIO10};
const uint16_t TONE_FOR_BUTTON[4] = {NOTE_E3, NOTE_CS3, NOTE_A3, NOTE_E2};

// non-constant state information
int8_t button_pressed;
uint8_t level_sequence[50];
uint8_t level;
uint8_t input_mode;
uint32_t loop_count;
int8_t current_step;
uint32_t tone_duration;
uint32_t randint;
uint8_t error = 0;

static void delay(uint32_t time) {
    volatile uint32_t loops = ((F_CPU / 1000) * time) / 16;
    volatile uint32_t i;
    for(i = 0; i < loops; i++) {
        __asm__("nop");
    }
}

static uint32_t flash_read_word(uint32_t start_address) {
    uint32_t *memory_ptr = (uint32_t*)start_address;
    return *(memory_ptr);
}

static uint32_t flash_program_data(uint32_t start_address, uint32_t data) {
    uint32_t current_address = start_address;
    uint32_t page_address = start_address;
    volatile uint32_t flash_status = 0;

    // calculate current page address
    if(start_address % FLASH_PAGE_SIZE) {
        page_address -= (start_address % FLASH_PAGE_SIZE);
    }

    flash_unlock();

    // erase the page
    flash_erase_page(page_address);
    flash_status = flash_get_status_flags();
    if(flash_status != FLASH_SR_EOP) {
        flash_lock();
        return flash_status;
    }

    flash_program_word(current_address, data);
    flash_status = flash_get_status_flags();
    if(flash_status != FLASH_SR_EOP) {
        flash_lock();
        return flash_status;
    }

    flash_lock();

    return 0;
}


static void reset_game(void) {
    tone_duration = INITIAL_TONE_DURATION;
    level = 1;
    input_mode = 0;
    loop_count = 0;
}

static void tone_off(void) {
    timer_disable_counter(TIM3);
}

static void tone(uint16_t frequency, int32_t millis) {
    timer_disable_oc_output(TIM3, TIM_OC2);
    frequency += 100;

    int val = ((F_CPU / TIM3_PRESCALER) / (frequency * 2.0)) - 1;

    timer_set_period(TIM3, val); 
    timer_enable_oc_output(TIM3, TIM_OC2);
    timer_enable_counter(TIM3);
    if(millis > -1) {
        delay(millis);
        tone_off();
    }
}

static void game_over(void) {
    reset_game();

    gpio_toggle(GPIOB, GPIO1);
    tone(ERROR_TONE, 1000);
    delay(NEXT_GAME_PAUSE_DURATION * 2);
    gpio_toggle(GPIOB, GPIO1);
    delay(NEXT_GAME_PAUSE_DURATION * 2);
}

static void game_won(void) {
    reset_game();
    delay(200);
    uint8_t i, a;
    for(i = 0; i < 8; i++) {
        for(a = 0; a < 4; a++) {
            gpio_set(GPIOA, LEDS[a]);
            tone(WON_TONE, 100);
            delay(50);
            gpio_clear(GPIOA, LEDS[a]);
        }
    }

    delay(NEXT_GAME_PAUSE_DURATION);
}

static void setup_level(void) {
    loop_count = 0;
    
    if(level > 1 && (level % INCREASE_SPEED_LEVELS == 0)) {
        uint8_t mult = level / INCREASE_SPEED_LEVELS;
        tone_duration -= INCREASE_SPEED_AMOUNT * mult;
        if(tone_duration < MAX_SPEED) {
            tone_duration = MAX_SPEED;
        }
    }

    current_step = -1;
    uint8_t start_sequence = 0;
    uint8_t i = 0;
    start_sequence = level - 1;

    for(i = 0; i < level; i++) {
        volatile uint8_t step;

        if(start_sequence > 0) {
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
        gpio_set(GPIOA, LEDS[step]);
        tone(TONE_FOR_BUTTON[step], tone_duration);
        gpio_clear(GPIOA, LEDS[step]);
        delay(PAUSE_DURATION);
    }
}

static void button_press(uint8_t index) {
    gpio_set(GPIOA, LEDS[index]);
    tone(TONE_FOR_BUTTON[index], -1);
    loop_count = 0;
}

static void button_release(uint8_t index) {
    gpio_clear(GPIOA, LEDS[index]);

    tone_off();

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

static void check_button_press(uint8_t index) {
    if(gpio_get(GPIOA, BUTTONS[index])) {
        if(button_pressed > -1) return;

        delay(50); // wait for debounce
        if(gpio_get(GPIOA, BUTTONS[index])) {
            button_pressed = index;
            button_press(index);
        }
    }

    if(button_pressed == index &&
        !gpio_get(GPIOA, BUTTONS[index])) {
        
        delay(50);
        if(!gpio_get(GPIOA, BUTTONS[index])) {
            button_pressed = -1;
            button_release(index);
        }
    }
}

static void clock_setup(void) {
    // set clock to 4MHz, the lowest it can go
    rcc_clock_setup_in_hsi_out_4mhz();
}

static void gpio_setup(void) {
    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_GPIOB);

    // set up pins for the 4 leds
    gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, 
        GPIO0 | GPIO1 | GPIO2 | GPIO3);

    // set up pins for the 4 buttons, with an internal pulldown resistor
    // (pushing the button will cause the pin to go HIGH)
    gpio_mode_setup(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_PULLDOWN,
        GPIO4 | GPIO5 | GPIO6 | GPIO10);

    // set up the pin for the FAIL led
    gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,
        GPIO1);

    // set up the pin for the timer output (for the little buzzer)
    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_OTYPE_PP, GPIO7);
    gpio_set_output_options(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_HIGH, GPIO7);

    // GPIO_AF2 == TIM3, CHANNEL 2 for PA7 (see on page 31 of the datasheet)
    gpio_set_af(GPIOA, GPIO_AF1, GPIO7);
}

static void timer_setup(void) {
    rcc_periph_clock_enable(RCC_TIM3);
    timer_reset(TIM3);
    timer_set_mode(TIM3, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
    timer_set_prescaler(TIM3, TIM3_PRESCALER);
    timer_enable_preload(TIM3);
    timer_continuous_mode(TIM3);
    timer_enable_preload_complementry_enable_bits(TIM3);
    timer_disable_oc_output(TIM3, TIM_OC2);
    timer_set_oc_mode(TIM3, TIM_OC2, TIM_OCM_TOGGLE);
}

static void random_seed(void) {
    uint32_t data = flash_read_word(FLASH_RAND_OPERATION_ADDRESS);
    srand(data);
    data = rand();
    srand(data);
    data = rand();
    
    uint32_t result = flash_program_data(FLASH_RAND_OPERATION_ADDRESS, data);

    if(result != 0) {
        error = 1;
        gpio_toggle(GPIOA, GPIO9);
        delay(1000);
        gpio_toggle(GPIOA, GPIO9);
    }
}

int main(void) {
    clock_setup();
    gpio_setup();
    timer_setup();
    random_seed();

    // set up the game
    level = 1;
    input_mode = 0;
    current_step = -1;
    button_pressed = -1;
    tone_duration = INITIAL_TONE_DURATION;
    uint16_t i;

    delay(2000); // delay 2 seconds before starting the game

    while(1) {
        if(!input_mode) {
            setup_level();
            input_mode = 1;
        }
        else {
            for(i = 0; i < 4; i++) {
                check_button_press(i);
            }

            if(loop_count > MAX_LOOPS) {
                game_over();
            }

            loop_count ++;
        }
    }

    return 0;
}
