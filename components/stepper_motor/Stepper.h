// ensure this library description is only included once
#ifndef Stepper_h
#define Stepper_h

#include "driver/gpio.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>


// library interface description
class Stepper {
  public:
    // constructors:
    Stepper(int number_of_steps, gpio_num_t motor_pin_1, gpio_num_t motor_pin_2,
                                 gpio_num_t motor_pin_3, gpio_num_t motor_pin_4);

    // speed setter method:
    void setSpeed(long whatSpeed);

    // mover method:
    void step(int number_of_steps);
    void stop();

  private:
    void stepMotor(int this_step);

    int direction;            // Direction of rotation
    unsigned long step_delay; // delay between steps, in ms, based on speed
    int number_of_steps;      // total number of steps this motor can take
    int step_number;          // which step the motor is on

    // motor pin numbers:
    gpio_num_t motor_pin_1;
    gpio_num_t motor_pin_2;
    gpio_num_t motor_pin_3;
    gpio_num_t motor_pin_4;

    unsigned long last_step_time; // time stamp in us of when the last step was taken
};

#endif

