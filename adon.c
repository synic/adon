#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>

#define F_CPU 4000000

static void delay(uint32_t time) {
    volatile uint32_t loops = ((F_CPU / 1000) * time) / 16;
    volatile uint32_t i;
    for(i = 0; i < loops; i++) {
        __asm__("nop");
    }
}

static void setup_clock(void) {
    // set clock to 4MHz, the lowest it can go
    rcc_clock_setup_in_hsi_out_4mhz();
}

static void setup_gpio(void) {
    rcc_periph_clock_enable(RCC_GPIOA);

    // set up pins for the 4 leds
    gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, 
        GPIO0 | GPIO1 | GPIO2 | GPIO3);

    // set up pins for the 4 buttons, with an internal pulldown resistor
    // (pushing the button will cause the pin to go HIGH)
    gpio_mode_setup(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_PULLDOWN,
        GPIO4 | GPIO5 | GPIO6 | GPIO7);

    rcc_periph_clock_enable(RCC_GPIOB);

    // set up the pin for the FAIL led
    /*gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,
        GPIO1);

    // set up the pin for the timer output (for the little buzzer)
    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_OTYPE_PP, GPIO10);
    gpio_set_output_options(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_LOW, GPIO10);

    // GPIO_AF2 == TIM1, CHANNEL 3 for PA10 (see on page 31 of the datasheet)
    gpio_set_af(GPIOA, GPIO_AF2, GPIO10);*/
}

int main(void) {
    setup_clock();
    setup_gpio();

    while(1) {
        gpio_toggle(GPIOA, GPIO0);
        delay(1000);
    }

    return 0;
}
