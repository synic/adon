#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>

static void clock_setup(void) {
    rcc_clock_setup_in_hsi_8mhz_out_48mhz();
}

static void timer_setup(void) {
    rcc_periph_clock_enable(RCC_TIM3);                  

    timer_reset(TIM3);
    timer_set_mode(TIM3, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
    int prescaler = (uint16_t) ((48000000 / 2) / 21000000) - 1;

    timer_set_prescaler(TIM3, prescaler);
    timer_enable_preload(TIM3);
    timer_continuous_mode(TIM3);
    timer_set_period(TIM3, 665);
    timer_disable_oc_output(TIM3, TIM_OC2);
    timer_set_oc_mode(TIM3, TIM_OC2, TIM_OCM_PWM1);
    timer_set_oc_value(TIM3, TIM_OC2, 500);
    timer_enable_oc_output(TIM3, TIM_OC2);
    timer_enable_preload_complementry_enable_bits(TIM3);
    timer_enable_counter(TIM3);
}

static void gpio_setup(void) {
	rcc_periph_clock_enable(RCC_GPIOA);
    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_OTYPE_PP, GPIO7);
    gpio_set_output_options(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_HIGH, GPIO7);
    gpio_set_af(GPIOA, GPIO_AF1, GPIO7);
}

int main(void) {
    clock_setup();
	gpio_setup();
    timer_setup();

	volatile int i;
    volatile int brightness = 0;
    volatile int up = 1;

    while (1) {
        if (up) {
            brightness++;
        } else {
            brightness--;
        }

        if (brightness >= 700) {
            up = 0;
        } else if (brightness <= 0) {
            up = 1;
        }

        timer_set_oc_value(TIM3, TIM_OC2, brightness);

        for(i = 0; i < 2500; i++);
    }

	return 0;
}
