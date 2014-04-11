#include <stdlib.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/flash.h>

#define F_CPU 4000000 // 4Mhz

// constants
#define FLASH_RAND_OPERATION_ADDRESS ((uint32_t)0x08003800)
#define FLASH_PAGE_SIZE 0x800
#define FLASH_WRONG_DATA_WRITTEN 0x80
#define FLASH_RAND_BUFFER_SIZE 4
#define RESULT_OK 0

const uint8_t LEDS[4] = {GPIO0, GPIO1, GPIO2, GPIO3};
const uint8_t BUTTONS[4] = {GPIO4, GPIO5, GPIO6, GPIO7};

// non-constant state information
uint8_t button_pressed;
uint8_t level_sequence[30];
uint8_t level;
uint8_t input_mode;
uint32_t loop_count;
uint8_t current_step;
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
        return flash_status;
    }

    flash_program_word(current_address, data);
    flash_status = flash_get_status_flags();
    if(flash_status != FLASH_SR_EOP) {
        return flash_status;
    }

    return 0;
}

static void setup_clock(void) {
    // set clock to 4MHz, the lowest it can go
    rcc_clock_setup_in_hsi_out_4mhz();
}

static void setup_gpio(void) {
    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_GPIOB);

    // set up pins for the 4 leds
    gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, 
        GPIO0 | GPIO1 | GPIO2 | GPIO3);

    // set up pins for the 4 buttons, with an internal pulldown resistor
    // (pushing the button will cause the pin to go HIGH)
    gpio_mode_setup(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_PULLDOWN,
        GPIO4 | GPIO5 | GPIO6 | GPIO7);

    // set up the pin for the FAIL led
    gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,
        GPIO1);

    // set up the pin for the timer output (for the little buzzer)
    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_OTYPE_PP, GPIO10);
    gpio_set_output_options(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_LOW, GPIO10);

    // GPIO_AF2 == TIM1, CHANNEL 3 for PA10 (see on page 31 of the datasheet)
    gpio_set_af(GPIOA, GPIO_AF2, GPIO10);
}

static void random_seed(void) {
    uint32_t data = flash_read_word(FLASH_RAND_OPERATION_ADDRESS);
    srand(data);
    data = rand();
    
    uint32_t result = flash_program_data(FLASH_RAND_OPERATION_ADDRESS, data);

    if(result != 0) {
        error = 1;
        gpio_toggle(GPIOB, GPIO1);
    }
}

int main(void) {
    setup_clock();
    setup_gpio();
    random_seed();


    while(1) {
        int led = rand() % 4;
        gpio_toggle(GPIOA, (1 << led));
        delay(1000);
        gpio_toggle(GPIOB, GPIO1);
        gpio_toggle(GPIOA, (1 << led));
        delay(1000);
    }

    return 0;
}
