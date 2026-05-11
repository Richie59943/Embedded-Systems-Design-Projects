#include "temphum.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdint.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_check.h"

#define I2C_MASTER_SCL_IO         GPIO_NUM_8
#define I2C_MASTER_SDA_IO         GPIO_NUM_7
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

static void create_i2c_bus(i2c_master_bus_handle_t *bus_handle)
{
    i2c_master_bus_config_t bus_config = {};
       bus_config.i2c_port = I2C_MASTER_NUM,
         bus_config.sda_io_num = I2C_MASTER_SDA_IO,
         bus_config.scl_io_num = I2C_MASTER_SCL_IO,
         bus_config.clk_source = I2C_CLK_SRC_DEFAULT,
         bus_config.glitch_ignore_cnt = 7,
         bus_config.flags.enable_internal_pullup = true;
  
	ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, bus_handle)); 
}

static void SHTC3_add_device(i2c_master_bus_handle_t bus_handle, i2c_master_dev_handle_t *shtc3_handle)
{
    i2c_device_config_t sensor_config = {};

       sensor_config.dev_addr_length = I2C_ADDR_BIT_LEN_7,
        sensor_config.device_address = SHTC3_SENSOR_ADDR,
        sensor_config.scl_speed_hz = I2C_MASTER_FREQ_HZ; 

      ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &sensor_config,shtc3_handle));
}




extern "C" void app_main(void)
{

	i2c_master_bus_handle_t bus_handle;
	i2c_master_dev_handle_t shtc3_handle;
	create_i2c_bus(&bus_handle);
	LCD lcd;

    lcd.init(bus_handle);
    lcd.setRGB(255,255,255);
vTaskDelay(5);    
    lcd.clear();
SHTC3_add_device(bus_handle,&shtc3_handle);
	uint8_t data[6];

    uint16_t raw_temp;
    uint16_t raw_humidity;

    uint8_t temp_crc;
    uint8_t humidity_crc;

    float temp_c;
    float temp_f;
    float humidity;

    
    while (1) {
        ESP_ERROR_CHECK(shtc3_write_cmd(shtc3_handle, SHTC3_WAKEUP_CMD, sizeof(SHTC3_WAKEUP_CMD)));
        vTaskDelay(pdMS_TO_TICKS(10));

        ESP_ERROR_CHECK(shtc3_write_cmd(shtc3_handle, SHTC3_MEASURE_CMD, sizeof(SHTC3_MEASURE_CMD)));
        vTaskDelay(pdMS_TO_TICKS(20));

        ESP_ERROR_CHECK(shtc3_read_data(shtc3_handle, data, 6));

        ESP_ERROR_CHECK(shtc3_write_cmd(shtc3_handle, SHTC3_SLEEP_CMD, sizeof(SHTC3_SLEEP_CMD)));

        temp_crc = shtc3_crc8(data[0], data[1]);
        humidity_crc = shtc3_crc8(data[3], data[4]);

        if ((temp_crc != data[2]) || (humidity_crc != data[5])) {
            printf("Sensor read failed\n");
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        raw_temp = ((uint16_t)data[0] << 8) | data[1];
        raw_humidity = ((uint16_t)data[3] << 8) | data[4];

        temp_c = -45.0f + (175.0f * (float)raw_temp / 65536.0f);
        temp_f = (temp_c * 9.0f / 5.0f) + 32.0f;
        humidity = 100.0f * (float)raw_humidity / 65536.0f;

        printf("Temperature is %.0fC (or %.0fF) with a %.0f%% humidity\n",
               temp_c, temp_f, humidity);

        vTaskDelay(pdMS_TO_TICKS(1000));
    

	// these lines are allwoing us to change from float to a character string aka a buffer or array
	char temp[64];
    snprintf(temp, sizeof temp, "Temp: %.0fC" , temp_c);
    char hum[64];
    snprintf(hum, sizeof hum, "Hum : %.0f%%", humidity);


        lcd.setCursor(0, 0);
        lcd.printstr(temp);

        lcd.setCursor(0, 1);
        lcd.printstr(hum);

    
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}



