#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>



#define TRIGGER_PIN 8
#define ECHO_PIN 7
#define MAX_DISTANCE 400



void setup_gpio(void)
{
	gpio_config_t trig_config = {
		.pin_bit_mask = 1ULL << TRIGGER_PIN,
		.mode = GPIO_MODE_OUTPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE
	};

	gpio_config(&trig_config);
	
	gpio_config_t echo_config = {
		.pin_bit_mask = 1ULL << ECHO_PIN,
		.mode = GPIO_MODE_INPUT,
       		.pull_up_en = GPIO_PULLUP_DISABLE,
        	.pull_down_en = GPIO_PULLDOWN_DISABLE,
        	.intr_type = GPIO_INTR_DISABLE,
	};

	gpio_config(&echo_config);
}


void send_trigger_pulse(void)
{
	gpio_set_level(TRIGGER_PIN,0);
	esp_rom_delay_us(2);

	gpio_set_level(TRIGGER_PIN,1);
	esp_rom_delay_us(10);

	gpio_set_level(TRIGGER_PIN,0);
}


int64_t measure_echo_time(void)
{
    int64_t start_time;
    int64_t end_time;
    int64_t timeout_start;

    send_trigger_pulse();

    timeout_start = esp_timer_get_time();

    while (gpio_get_level(ECHO_PIN) == 0)
    {
        if (esp_timer_get_time() - timeout_start > 30000)
        {
            return -1;
        }
    }

    start_time = esp_timer_get_time();

    while (gpio_get_level(ECHO_PIN) == 1)
    {
        if (esp_timer_get_time() - start_time > 30000)
        {
            return -1;
        }
    }

    end_time = esp_timer_get_time();

    return end_time - start_time;
}




void app_main(void)
{

		
	//we call our setup function 
	setup_gpio();

	while(1)
	{
		int64_t duration_us = measure_echo_time();

		float total_distance = ((duration_us * 0.0343) / 2.0);
		printf("Echo time: %.2f cm\n", total_distance);
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

	




	
