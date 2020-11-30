#include "Stepper.h"

unsigned long IRAM_ATTR micros()
{
  return (unsigned long)(esp_timer_get_time());
}

/*
 *   constructor for four-pin version
 *   Sets which wires should control the motor.
 */
Stepper::Stepper(int number_of_steps, gpio_num_t motor_pin_1, gpio_num_t motor_pin_2,
                 gpio_num_t motor_pin_3, gpio_num_t motor_pin_4)
{
  this->step_number = 0;                   // which step the motor is on
  this->direction = 0;                     // motor direction
  this->last_step_time = 0;                // time stamp in us of the last step taken
  this->number_of_steps = number_of_steps; // total number of steps for this motor

  // Arduino pins for the motor control connection:
  this->motor_pin_1 = motor_pin_1;
  this->motor_pin_2 = motor_pin_2;
  this->motor_pin_3 = motor_pin_3;
  this->motor_pin_4 = motor_pin_4;

  // setup the pins on the microcontroller:
  gpio_set_direction(this->motor_pin_1, GPIO_MODE_OUTPUT);
  gpio_set_direction(this->motor_pin_2, GPIO_MODE_OUTPUT);
  gpio_set_direction(this->motor_pin_3, GPIO_MODE_OUTPUT);
  gpio_set_direction(this->motor_pin_4, GPIO_MODE_OUTPUT);
}

/*
 * Sets the speed in revs per minute
 */
void Stepper::setSpeed(long whatSpeed)
{
  if (whatSpeed > 16) {
    whatSpeed = 16;
  }
  this->step_delay = 60L * 1000L * 1000L / this->number_of_steps / whatSpeed;
  ESP_LOGD("Stepper", "Step delay set to: %lu", this->step_delay);
}

/*
 * Moves the motor steps_to_move steps.  If the number is negative,
 * the motor moves in the reverse direction.
 */
void Stepper::step(int steps_to_move)
{
  int steps_left = abs(steps_to_move); // how many steps to take

  // determine direction based on whether steps_to_mode is + or -:
  if (steps_to_move > 0) {
    this->direction = 1;
  }
  if (steps_to_move < 0) {
    this->direction = 0;
  }
  ESP_LOGD("Stepper", "Direction %d", this->direction);

  // decrement the number of steps, moving one step each time:
  while (steps_left > 0) {

    unsigned long now = micros();
    // move only if the appropriate delay has passed:
    if (now - this->last_step_time >= this->step_delay)
    {
      // get the timeStamp of when you stepped:
      this->last_step_time = now;
      // increment or decrement the step number,
      // depending on direction:
      if (this->direction == 1) {
        this->step_number++;
        if (this->step_number > 7) {
          this->step_number = 0;
        }
      }
      else {
        this->step_number--;
        if (this->step_number < 0) {
          this->step_number = 7;
        }
      }
      // decrement the steps left:
      steps_left--;
      // step the motor to step number 0, 1, ..., {3 or 10}
      stepMotor(this->step_number);
    }
  }
}

void Stepper::stop()
{
  stepMotor(10); // go to default
}

/*
 * Moves the motor forward or backwards.
 */
void Stepper::stepMotor(int thisStep)
{
  switch (thisStep)
  {
  case 0: // 0001
    gpio_set_level(motor_pin_1, 0);
    gpio_set_level(motor_pin_2, 0);
    gpio_set_level(motor_pin_3, 0);
    gpio_set_level(motor_pin_4, 1);
    break;
  case 1: // 0011
    gpio_set_level(motor_pin_1, 0);
    gpio_set_level(motor_pin_2, 0);
    gpio_set_level(motor_pin_3, 1);
    gpio_set_level(motor_pin_4, 1);
    break;
  case 2: //0010
    gpio_set_level(motor_pin_1, 0);
    gpio_set_level(motor_pin_2, 0);
    gpio_set_level(motor_pin_3, 1);
    gpio_set_level(motor_pin_4, 0);
    break;
  case 3: //0110
    gpio_set_level(motor_pin_1, 0);
    gpio_set_level(motor_pin_2, 1);
    gpio_set_level(motor_pin_3, 1);
    gpio_set_level(motor_pin_4, 0);
    break;
  case 4: // 0100
    gpio_set_level(motor_pin_1, 0);
    gpio_set_level(motor_pin_2, 1);
    gpio_set_level(motor_pin_3, 0);
    gpio_set_level(motor_pin_4, 0);
    break;
  case 5: //1100
    gpio_set_level(motor_pin_1, 1);
    gpio_set_level(motor_pin_2, 1);
    gpio_set_level(motor_pin_3, 0);
    gpio_set_level(motor_pin_4, 0);
    break;
  case 6: //1000
    gpio_set_level(motor_pin_1, 1);
    gpio_set_level(motor_pin_2, 0);
    gpio_set_level(motor_pin_3, 0);
    gpio_set_level(motor_pin_4, 0);
    break;
  case 7: // 1001
    gpio_set_level(motor_pin_1, 1);
    gpio_set_level(motor_pin_2, 0);
    gpio_set_level(motor_pin_3, 0);
    gpio_set_level(motor_pin_4, 1);
    break;
  default: //0000
    gpio_set_level(motor_pin_1, 0);
    gpio_set_level(motor_pin_2, 0);
    gpio_set_level(motor_pin_3, 0);
    gpio_set_level(motor_pin_4, 0);
    break;
  }
}
