#include <string.h>

#include "servo.h"
#include "gpio.h"

/**
 * Initialize a servo channel with safe defaults
 */
bool servo_init(ServoChannel *channel, uint8_t gpio) {
  if (
    !channel ||
    gpio > MAX_GPIO_PIN
  ) {
    return false;
  }

  memset(channel, 0, sizeof(ServoChannel));

  channel->gpio = gpio;
  channel->enabled = 0;
  channel->min_us = SERVO_MIN_US;
  channel->max_us = SERVO_MAX_US;
  channel->pulse_us = SERVO_NEUTRAL_US;

  // Configure GPIO as output and set low
  gpio_set_output(channel->gpio);
  gpio_clear(channel->gpio);

  return true;
}

/**
 * Close a servo channel and reset GPIO to safe state
 */
void servo_close(ServoChannel *channel) {
  if (!channel) {
    return;
  }

  // Disable channel and set GPIO as input
  servo_disable(channel);
  gpio_set_input(channel->gpio);
}

/**
 * Enable a servo channel
 */
void servo_enable(ServoChannel *channel) {
  if (!channel) {
    return;
  }

  channel->enabled = 1;
}

/**
 * Disable a servo channel
 */
void servo_disable(ServoChannel *channel) {
  if (!channel) {
    return;
  }

  channel->enabled = 0;
  gpio_clear(channel->gpio);
}

/**
 * Set servo pulse range with validation
 *
 * @return false if values had to be clamped to absolute limits
 */
bool servo_set_range(ServoChannel *channel, int16_t min_us, int16_t max_us) {
  if (
    !channel ||
    min_us >= max_us
  ) {
    return false;
  }

  bool clamped = false;

  // Clamp to absolute hardware limits
  if (min_us < SERVO_ABSOLUTE_MIN) {
    min_us = SERVO_ABSOLUTE_MIN;
    clamped = true;
  }
  if (max_us > SERVO_ABSOLUTE_MAX) {
    max_us = SERVO_ABSOLUTE_MAX;
    clamped = true;
  }

  // Set the new range
  channel->min_us = min_us;
  channel->max_us = max_us;

  // Re-clamp current pulse to new range
  if (channel->pulse_us < min_us) {
    channel->pulse_us = min_us;
    clamped = true;
  }
  if (channel->pulse_us > max_us) {
    channel->pulse_us = max_us;
    clamped = true;
  }

  return !clamped;
}

/**
 * Set servo pulse width with automatic clamping
 *
 * @return false if value had to be clamped
 */
bool servo_set_pulse(ServoChannel *channel, int16_t pulse_us) {
  if (!channel) {
    return false;
  }

  int16_t original = pulse_us;

  // Clamp to channel-specific range
  if (pulse_us < channel->min_us) {
    pulse_us = channel->min_us;
  }
  if (pulse_us > channel->max_us) {
    pulse_us = channel->max_us;
  }

  // Clamp to absolute safety limits
  if (pulse_us < SERVO_ABSOLUTE_MIN) {
    pulse_us = SERVO_ABSOLUTE_MIN;
  }
  if (pulse_us > SERVO_ABSOLUTE_MAX) {
    pulse_us = SERVO_ABSOLUTE_MAX;
  }

  channel->pulse_us = pulse_us;

  return (pulse_us == original);
}
