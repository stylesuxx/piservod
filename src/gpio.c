#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stddef.h>

#include "gpio.h"
#include "servo.h"

static volatile uint32_t *gpio_map = NULL;
static int gpio_fd = -1;

bool gpio_init(void) {
  if (gpio_map != NULL) {
    return true;
  }

  gpio_fd = open("/dev/gpiomem", O_RDWR | O_SYNC);
  if (gpio_fd < 0) {
    perror("Failed to open /dev/gpiomem");
    return false;
  }

  // Note: /dev/gpiomem already points to GPIO base, so no offset needed
  gpio_map = (volatile uint32_t *) mmap(
    NULL,
    BLOCK_SIZE,
    PROT_READ | PROT_WRITE,
    MAP_SHARED,
    gpio_fd,
    0  // /dev/gpiomem is pre-offset to GPIO registers
  );

  if (gpio_map == MAP_FAILED) {
    perror("mmap failed");
    close(gpio_fd);

    gpio_fd = -1;
    gpio_map = NULL;

    return false;
  }

  return true;
}

void gpio_cleanup(void) {
  if (gpio_map != NULL) {
    munmap((void *)gpio_map, BLOCK_SIZE);
    gpio_map = NULL;
  }

  if (gpio_fd >= 0) {
    close(gpio_fd);
    gpio_fd = -1;
  }
}

static void gpio_set_function(uint8_t pin, uint8_t function) {
  if (
    gpio_map == NULL ||
    pin > MAX_GPIO_PIN
  ) {
    return;
  }

  uint8_t reg_index = pin / 10;          // Which GPFSEL register (0-5)
  uint8_t bit_offset = (pin % 10) * 3;   // Bit position within register

  volatile uint32_t *reg = &gpio_map[GPFSEL0 + reg_index];
  uint32_t value = *reg;

  // Clear the 3 bits for this pin
  value &= ~(0b111 << bit_offset);

  // Set the new function
  value |= ((function & 0b111) << bit_offset);

  *reg = value;
}

void gpio_set_output(uint8_t pin) {
  gpio_set_function(pin, GPIO_FSEL_OUTPUT);
}

void gpio_set_input(uint8_t pin) {
  gpio_set_function(pin, GPIO_FSEL_INPUT);
}

void gpio_set(uint8_t pin) {
  if (
    gpio_map == NULL ||
    pin > MAX_GPIO_PIN
  ) {
    return;
  }

  uint8_t reg_index = (pin < 32) ? GPSET0 : GPSET1;
  uint8_t bit = pin % 32;

  gpio_map[reg_index] = (1 << bit);
}

void gpio_clear(uint8_t pin) {
  if (
    gpio_map == NULL ||
    pin > MAX_GPIO_PIN
  ) {
    return;
  }

  uint8_t reg_index = (pin < 32) ? GPCLR0 : GPCLR1;
  uint8_t bit = pin % 32;

  gpio_map[reg_index] = (1 << bit);
}

uint8_t gpio_read(uint8_t pin) {
  if (
    gpio_map == NULL ||
    pin > MAX_GPIO_PIN
  ) {
    return 0;
  }

  uint8_t reg_index = (pin < 32) ? GPLEV0 : GPLEV1;
  uint8_t bit = pin % 32;

  return (gpio_map[reg_index] & (1 << bit)) ? 1 : 0;
}