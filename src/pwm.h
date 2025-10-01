#ifndef PWM_H
#define PWM_H

#include "servo.h"

bool pwm_init(ServoController *controller);
void pwm_run_frame(ServoController *controller);
void pwm_cleanup(void);

#endif /* PWM_H */
