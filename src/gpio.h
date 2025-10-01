#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>
#include <stdbool.h>

/* BCM2835 GPIO register offsets (in 32-bit words) */
#define GPFSEL0   0   // Function select 0
#define GPFSEL1   1
#define GPFSEL2   2
#define GPFSEL3   3
#define GPFSEL4   4
#define GPFSEL5   5

#define GPSET0    7   // Set bits (output high)
#define GPSET1    8

#define GPCLR0    10  // Clear bits (output low)
#define GPCLR1    11

#define GPLEV0    13  // Pin level (read input)
#define GPLEV1    14

/* GPIO function select modes */
#define GPIO_FSEL_INPUT   0b000
#define GPIO_FSEL_OUTPUT  0b001
#define GPIO_FSEL_ALT0    0b100
#define GPIO_FSEL_ALT1    0b101
#define GPIO_FSEL_ALT2    0b110
#define GPIO_FSEL_ALT3    0b111
#define GPIO_FSEL_ALT4    0b011
#define GPIO_FSEL_ALT5    0b010

#define BLOCK_SIZE (4*1024)

bool gpio_init(void);
void gpio_cleanup(void);

void gpio_set_output(uint8_t pin);
void gpio_set_input(uint8_t pin);
void gpio_set(uint8_t pin);
void gpio_clear(uint8_t pin);

uint8_t gpio_read(uint8_t pin);

#endif /* GPIO_H */
