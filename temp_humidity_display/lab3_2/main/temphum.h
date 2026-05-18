#ifndef TEMPHUM_H
#define TEMPHUM_H

#include "driver/i2c_master.h"

class LCD {
	public:
	LCD();
	void init(i2c_master_bus_handle_t shared_bus); // we no longer have lcd.cpp make the bus we have our main make it becuase of our SHTC3 code which makes it so we have to pass it in 
	void clear();
	void writeCommand(uint8_t command);
	void writeChar(char character);
	void setCursor(int col, int row);
	void printstr( const char *str);
	void setRGB(uint8_t r, uint8_t g, uint8_t b);
	void writeRGBRegister(uint8_t reg, uint8_t value);


	private:

	i2c_master_bus_handle_t bus_handle; // this is us creating a bus
	i2c_master_dev_handle_t lcd_handle; // this is us creating a handle for LCD
	i2c_master_dev_handle_t rgb_handle;
	
};

#endif



