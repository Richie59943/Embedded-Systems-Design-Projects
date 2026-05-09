#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"

static const char *TAG = "LAB4";
static void get_direction (i2c_master_dev_handle_t dev_handle)
{
	//addresses for the icm to know what to use
	uint8_t ACCEL_X1 = 0x0B; 
	uint8_t ACCEL_X0 = 0x0C;
	uint8_t ACCEL_Y1 = 0x0D;
	uint8_t ACCEL_Y0 = 0x0E;

	//low and hight bytes for X
	uint8_t high_bytes_X = 0;
	uint8_t low_bytes_X = 0;
	
	//low and high bytes for Y
	uint8_t high_bytes_Y = 0;
	uint8_t low_bytes_Y =0;
	//combines both low and high bytes
	int16_t ACCEL_X;
	int16_t ACCEL_Y;

	//transmits and receives at the same time (handle, what we want to send,size of that,where we want to send it, size of that, error return)
	i2c_master_transmit_receive(dev_handle,&ACCEL_X1,1,&high_bytes_X,1, -1);
	i2c_master_transmit_receive(dev_handle,&ACCEL_X0,1,&low_bytes_X,1,-1);
  	
	i2c_master_transmit_receive(dev_handle,&ACCEL_Y1,1,&high_bytes_Y,1,-1);
	i2c_master_transmit_receive(dev_handle,&ACCEL_Y0,1,&low_bytes_Y,1,-1);

	ACCEL_Y = (int16_t)((high_bytes_Y << 8) | low_bytes_Y);
	ACCEL_X = (int16_t)((high_bytes_X << 8) | low_bytes_X);	
		if(ACCEL_Y > 100 && ACCEL_X < 100 && ACCEL_X > -100)
	{
		ESP_LOGI(TAG,"UP y %d", ACCEL_Y);
	};
	
	if(ACCEL_Y < -100 && ACCEL_X < 100 && ACCEL_X > -100)
	{
		ESP_LOGI(TAG,"DOWN y %d", ACCEL_Y);
	};
	if(ACCEL_X > 100 && ACCEL_Y < 100 && ACCEL_Y > -100)
	{
	ESP_LOGI(TAG,"LEFT x %d", ACCEL_X);
	};
	if(ACCEL_X < -100 && ACCEL_Y <100 && ACCEL_Y > -100)
	{
		ESP_LOGI(TAG,"RIGHT x %d",ACCEL_X);
	};

	if(ACCEL_X > 100 && ACCEL_Y > 100)
	{
		ESP_LOGI(TAG,"UP LEFT");
	};

	if(ACCEL_X < -100 && ACCEL_Y > 100)
	{	
		ESP_LOGI(TAG,"UP RIGHT");
	};

	if(ACCEL_X < -100 && ACCEL_Y < -100)
	{	
		ESP_LOGI(TAG,"DOWN RIGHT");
	};

		if(ACCEL_X < -100 && ACCEL_Y < -100)
	{	
		ESP_LOGI(TAG,"DOWN RIGHT");
	};

	if(ACCEL_X > 100 && ACCEL_Y < -100)
	{	
		ESP_LOGI(TAG,"DOWN LEFT");
	};

	if(ACCEL_X < -100 && ACCEL_Y > 100)
	{	
		ESP_LOGI(TAG,"UP RIGHT");
	};

	if(ACCEL_X < -100 && ACCEL_Y > 100)
	{	
		ESP_LOGI(TAG,"UP RIGHT");
	};
}

void app_main (void) {

uint8_t PWR_MGMTO[2];
PWR_MGMTO[0]= 0x1F;
PWR_MGMTO[1]=0x03;


	
	
	
i2c_master_bus_config_t bus_config = {
	.clk_source = I2C_CLK_SRC_DEFAULT,
	.i2c_port = I2C_NUM_0,
	.scl_io_num = GPIO_NUM_8,
	.sda_io_num = GPIO_NUM_7,
	.glitch_ignore_cnt = 7,
	.flags.enable_internal_pullup = true,
};

i2c_master_bus_handle_t bus_handle;
ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config,&bus_handle));





i2c_device_config_t ICM_DEV = {
	.dev_addr_length = I2C_ADDR_BIT_LEN_7,
	.device_address = 0x68,
	.scl_speed_hz = 100000,
};

i2c_master_dev_handle_t dev_handle;
ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &ICM_DEV, &dev_handle));


i2c_master_transmit(dev_handle,PWR_MGMTO,2,-1);
while(1)
{

	get_direction(dev_handle);
	vTaskDelay(pdMS_TO_TICKS(1000));
}
};
