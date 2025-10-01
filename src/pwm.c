#define _GNU_SOURCE

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include "pwm.h"
#include "gpio.h"

static int timer_fd = -1;

/**
 * Sleep for a specified number of microseconds using high-resolution timer
 */
static void sleep_us(uint32_t microseconds) {
  struct timespec ts;

  ts.tv_sec = microseconds / 1000000;
  ts.tv_nsec = (microseconds % 1000000) * 1000;

  clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);
}

/**
 * Initialize PWM system
 */
bool pwm_init(ServoController *controller) {
  if (!controller) {
    return false;
  }

  // Do not leak timer if init is called twice
  if (timer_fd >= 0) {
    close(timer_fd);
  }

  // Create timer file descriptor
  timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
  if (timer_fd < 0) {
    fprintf(stderr, "Error: Could not create timerfd: %s\n", strerror(errno));
    return false;
  }

  // Configure for 20ms periodic timer
  struct itimerspec timer_spec = {
    .it_interval = {.tv_sec = 0, .tv_nsec = PWM_FRAME_US * 1000},
    .it_value = {.tv_sec = 0, .tv_nsec = PWM_FRAME_US * 1000}
  };

  if (timerfd_settime(timer_fd, 0, &timer_spec, NULL) < 0) {
    fprintf(stderr, "Error: Could not set timerfd: %s\n", strerror(errno));
    close(timer_fd);
    timer_fd = -1;

    return false;
  }

  // Try to set real-time priority for better timing accuracy
  struct sched_param sp;
  sp.sched_priority = 99;

  if (sched_setscheduler(0, SCHED_FIFO, &sp) != 0) {
    fprintf(
      stderr,
      "Warning: Could not set real-time priority: %s\n",
      strerror(errno)
    );
    fprintf(stderr, "Run with 'sudo' or 'chrt -f 99' for better timing.\n");
  }

  return true;
}

/**
 * Run one PWM frame (20ms cycle)
 */
void pwm_run_frame(ServoController *controller) {
  if (!controller || timer_fd < 0) {
    return;
  }

  // Wait for timer expiration (blocks until 20ms frame boundary)
  uint64_t expirations;
  ssize_t bytes_read = read(timer_fd, &expirations, sizeof(expirations));

  if (bytes_read < 0) {
    fprintf(stderr, "Error: timerfd read failed: %s\n", strerror(errno));
    return;
  }

  // Check for frame overruns
  if (expirations > 1) {
    fprintf(stderr, "Warning: Missed %lu PWM frames\n", expirations - 1);
  }

  // Step 1: Set all enabled channels HIGH
  for (uint8_t i = 0; i < controller->num_channels; i++) {
    ServoChannel *ch = &controller->channels[i];
    if (ch->enabled && ch->gpio <= MAX_GPIO_PIN) {
      gpio_set(ch->gpio);
    }
  }

  // Step 2: Clear channels one by one as their pulse width expires
  // Sort channels by pulse_us for efficient timing
  uint8_t sorted[MAX_SERVO_CHANNELS];
  for (uint8_t i = 0; i < controller->num_channels; i++) {
    sorted[i] = i;
  }

  // Simple bubble sort by pulse_us (good enough for 8 channels) and avoids
  // pulling in stdlib.
  for (uint8_t i = 0; i < controller->num_channels - 1; i++) {
    for (uint8_t j = 0; j < controller->num_channels - i - 1; j++) {
      if (
        controller->channels[sorted[j]].pulse_us >
        controller->channels[sorted[j + 1]].pulse_us
      ) {
        uint8_t temp = sorted[j];
        sorted[j] = sorted[j + 1];
        sorted[j + 1] = temp;
      }
    }
  }

  // Step 3: Process each channel in sorted order
  uint32_t prev_pulse_us = 0;
  for (uint8_t i = 0; i < controller->num_channels; i++) {
    ServoChannel *ch = &controller->channels[sorted[i]];

    if (!ch->enabled) {
      continue;
    }

    // Sleep for the delta between this pulse and the previous
    uint32_t delta_us = ch->pulse_us - prev_pulse_us;
    if (delta_us > 0) {
      sleep_us(delta_us);
    }

    gpio_clear(ch->gpio);

    prev_pulse_us = ch->pulse_us;
  }

  // Note: No manual sleep needed - timerfd handles frame timing
  // Next call to pwm_run_frame() will block until the next 20ms boundary
}

/**
 * Cleanup PWM resources
 */
void pwm_cleanup(void) {
  if (timer_fd >= 0) {
    close(timer_fd);
    timer_fd = -1;
  }

  // Reset scheduler priority
  struct sched_param sp;
  sp.sched_priority = 0;

  sched_setscheduler(0, SCHED_OTHER, &sp);
}
