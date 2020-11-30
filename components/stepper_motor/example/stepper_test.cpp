#include <Stepper.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

extern "C" void app_main(void) {
    int stepsPerRevolution = 4095;
    gpio_num_t pin_1 = GPIO_NUM_27;
    gpio_num_t pin_2 = GPIO_NUM_26;
    gpio_num_t pin_3 = GPIO_NUM_25;
    gpio_num_t pin_4 = GPIO_NUM_33;
    Stepper myStepper(stepsPerRevolution, pin_1, pin_2, pin_3, pin_4);

    myStepper.setSpeed(10);
    while(1) {
        ESP_LOGI("Main", "Clockwise");
        myStepper.step(1000);
        vTaskDelay(500 / portTICK_PERIOD_MS); 

        ESP_LOGI("Main", "Counter Clockwise");
        myStepper.step(1000 * -1);
        vTaskDelay(500 / portTICK_PERIOD_MS); 
    }
}