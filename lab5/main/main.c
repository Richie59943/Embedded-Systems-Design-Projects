/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include <stdint.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"

static const char *TAG = "lab5";
#define TRIGGER_PIN 2
#define ECHO_PIN 3
#define I2C_MASTER_SCL_IO         8
#define I2C_MASTER_SDA_IO         7
#define I2C_MASTER_NUM            I2C_NUM_0
#define I2C_MASTER_FREQ_HZ        100000
#define I2C_MASTER_TIMEOUT_MS     1000

#define SHTC3_SENSOR_ADDR         0x70

static const uint8_t SHTC3_WAKEUP_CMD[2]  = {0x35, 0x17};
static const uint8_t SHTC3_SLEEP_CMD[2]   = {0xB0, 0x98};
static const uint8_t SHTC3_MEASURE_CMD[2] = {0x78, 0x66};

static esp_err_t shtc3_write_cmd(i2c_master_dev_handle_t dev_handle, const uint8_t *buf, size_t len)
{
    return i2c_master_transmit(
        dev_handle,
        buf,
        len,
        pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS)
    );
}

static esp_err_t shtc3_read_data(i2c_master_dev_handle_t dev_handle, uint8_t *data, size_t len)
{
    return i2c_master_receive(
        dev_handle,
        data,
        len,
        pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS)
    );
}

static uint8_t shtc3_crc8(uint8_t msb, uint8_t lsb)
{
    uint8_t crc = 0xFF;
    uint8_t bytes[2];
    int i;
    int j;

    bytes[0] = msb;
    bytes[1] = lsb;

    for (i = 0; i < 2; i++) {
        crc = crc ^ bytes[i];

        for (j = 0; j < 8; j++) {
            if ((crc & 0x80) != 0) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc = (crc << 1);
            }
        }
    }

    return crc;
}

static void i2c_master_init(i2c_master_bus_handle_t *bus_handle, i2c_master_dev_handle_t *dev_handle)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SHTC3_SENSOR_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, bus_handle));
    ESP_ERROR_CHECK(i2c_master_bus_add_device(*bus_handle, &dev_config, dev_handle));
}


//adding our ultrasonic sensor logic
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
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;

    uint8_t data[6];

    uint16_t raw_temp;
    uint16_t raw_humidity;

    uint8_t temp_crc;
    uint8_t humidity_crc;

    float temp_c;
    float temp_f;
    float humidity;

    i2c_master_init(&bus_handle, &dev_handle);
    ESP_LOGI(TAG, "I2C initialized successfully");
	setup_gpio();	
    while (1) {
        ESP_ERROR_CHECK(shtc3_write_cmd(dev_handle, SHTC3_WAKEUP_CMD, sizeof(SHTC3_WAKEUP_CMD)));
        vTaskDelay(pdMS_TO_TICKS(10));

        ESP_ERROR_CHECK(shtc3_write_cmd(dev_handle, SHTC3_MEASURE_CMD, sizeof(SHTC3_MEASURE_CMD)));
        vTaskDelay(pdMS_TO_TICKS(20));

        ESP_ERROR_CHECK(shtc3_read_data(dev_handle, data, 6));

        ESP_ERROR_CHECK(shtc3_write_cmd(dev_handle, SHTC3_SLEEP_CMD, sizeof(SHTC3_SLEEP_CMD)));

        temp_crc = shtc3_crc8(data[0], data[1]);
        humidity_crc = shtc3_crc8(data[3], data[4]);

        if ((temp_crc != data[2]) || (humidity_crc != data[5])) {
            printf("Sensor read failed\n");
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        raw_temp = ((uint16_t)data[0] << 8) | data[1];
        //raw_humidity = ((uint16_t)data[3] << 8) | data[4];

        temp_c = -45.0f + (175.0f * (float)raw_temp / 65536.0f);
        //temp_f = (temp_c * 9.0f / 5.0f) + 32.0f;
        //humidity = 100.0f * (float)raw_humidity / 65536.0f;

	int64_t time_measured = measure_echo_time();
	float  speed_of_sound = (331.3 + (0.606 * (temp_c)));
	speed_of_sound = speed_of_sound / 10000.0;
	float distance = ((time_measured * speed_of_sound) / 2.0);

        printf("Distance: %0.2fcm at %0.2fC\n",distance,
               temp_c);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


