#ifndef LCD_H
#define LCD_H

#include "driver/i2c_master.h"

class LCD {
	public:
	LCD();
	void init();
	void clear();
	void setCursor(int  row, int  col); // this will allow us to controll where we place our str
	void printstr(const char *str); // this is where we grab our string and derefernce
	void setRGB(uint8_t r, uint8_t g, uint8_t b);//helps us control the RGB 					




	private:
	void writeCommand(uint8_t command); // takes in our command from DFRobot data sheet to write
	void writeChar(char character);
	void writeRGBRegister(uint8_t reg, uint8_t value);
	
	i2c_master_bus_handle_t bus_handle; // this is us creating a bus
	i2c_master_dev_handle_t lcd_handle; // this is us creating a handle for LCD
	i2c_master_dev_handle_t rgb_handle; // this is us creating a handle for RGB
};

#endif



