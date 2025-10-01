#ifndef SERVO_H
#define SERVO_H

#include <stdint.h>
#include <stdbool.h>

#define PWM_FREQUENCY_HZ    50
#define PWM_FRAME_US        20000

#define SERVO_MIN_US        1000
#define SERVO_MAX_US        2000
#define SERVO_NEUTRAL_US    1500

#define SERVO_ABSOLUTE_MIN  500
#define SERVO_ABSOLUTE_MAX  2500

#define MAX_SERVO_CHANNELS  8
#define MAX_GPIO_PIN        27

#define SOCKET_PATH         "/tmp/piservod.sock"
#define SOCKET_BACKLOG      5
#define SOCKET_BUFFER_SIZE  256

typedef struct {
    uint8_t  gpio;
    uint8_t  enabled;
    int16_t  min_us;
    int16_t  max_us;
    int16_t  pulse_us;
} ServoChannel;

typedef struct {
    ServoChannel channels[MAX_SERVO_CHANNELS];
    uint8_t      num_channels;
    bool         running;
    int          listen_fd;
} ServoController;

// Returns false if no channel or gpio invalid
bool servo_init(ServoChannel *channel, uint8_t gpio);
void servo_close(ServoChannel *channel);
void servo_enable(ServoChannel *channel);
void servo_disable(ServoChannel *channel);
// Returns false if range had to be clamped
bool servo_set_range(ServoChannel *channel, int16_t min_us, int16_t max_us);
// Returns false if pulse had to be clamped
bool servo_set_pulse(ServoChannel *channel, int16_t pulse_us);

#endif /* SERVO_H */
