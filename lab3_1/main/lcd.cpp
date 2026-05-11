#include "lcd.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_check.h"

#define I2C_PORT I2C_NUM_0
#define SDA_PIN GPIO_NUM_7
#define SCL_PIN GPIO_NUM_8
#define LCD_ADDR 0x3E
#define CLEAR 0x01
#define RGB_ADDR 0x2D
#define REG_RED 0x01
#define REG_GREEN 0x02
#define REG_BLUE 0x03

LCD::LCD()
{
    bus_handle = NULL;
    lcd_handle = NULL;
    rgb_handle = NULL;
}

void LCD::init()
{
    i2c_master_bus_config_t bus_config = {};

    bus_config.i2c_port = I2C_PORT;
    bus_config.sda_io_num = SDA_PIN;
    bus_config.scl_io_num = SCL_PIN;
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = true;

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));

    i2c_device_config_t lcd_config = {};

    lcd_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    lcd_config.device_address = LCD_ADDR;
    lcd_config.scl_speed_hz = 100000;


    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &lcd_config, &lcd_handle));


    i2c_device_config_t RGB_config = {};

    RGB_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    RGB_config.device_address = RGB_ADDR;
    RGB_config.scl_speed_hz = 100000;

    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &RGB_config, &rgb_handle));

    vTaskDelay(pdMS_TO_TICKS(50));

    writeCommand(0x38);
    vTaskDelay(pdMS_TO_TICKS(5));

    writeCommand(0x39);
    vTaskDelay(pdMS_TO_TICKS(5));

    writeCommand(0x14);
    writeCommand(0x70);
    writeCommand(0x56);
    writeCommand(0x6C);

    vTaskDelay(pdMS_TO_TICKS(200));

    writeCommand(0x38);
    writeCommand(0x0C);
    writeCommand(0x01);

    vTaskDelay(pdMS_TO_TICKS(5));
    
    setRGB(255,255,255);
}

void LCD::writeRGBRegister(uint8_t reg, uint8_t value)
{
	uint8_t data[2];
	data[0] = reg;
	data[1] = value;

	ESP_ERROR_CHECK(i2c_master_transmit(rgb_handle,data, sizeof(data),pdMS_TO_TICKS(1000)));

}
void LCD::setRGB(uint8_t r, uint8_t g, uint8_t b)
{
	writeRGBRegister(REG_RED, r);
	writeRGBRegister(REG_GREEN,g);
	writeRGBRegister(REG_BLUE, b);
}


void LCD::writeCommand(uint8_t command)
{
    uint8_t data[2];

    data[0] = 0x80;
    data[1] = command;

    ESP_ERROR_CHECK(i2c_master_transmit(lcd_handle, data, sizeof(data), pdMS_TO_TICKS(1000)));
}

void LCD::writeChar(char character)
{
    uint8_t data[2];

    data[0] = 0x40;
    data[1] = character;

    ESP_ERROR_CHECK(i2c_master_transmit(lcd_handle, data, sizeof(data),pdMS_TO_TICKS( 1000)));
}

void LCD::clear()
{
    writeCommand(CLEAR);
    vTaskDelay(pdMS_TO_TICKS(10));
}

void LCD::setCursor(int col, int row)
{
    uint8_t address;

    if (row == 0) {
        address = 0x80;
    } else {
        address = 0xC0;
    }

    address = address + col;

    writeCommand(address);
}

void LCD::printstr(const char *str)
{
    int i = 0;

    while (str[i] != '\0') {
        writeChar(str[i]);
        i++;
    }
}



