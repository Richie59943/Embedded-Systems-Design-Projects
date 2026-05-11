#include "lcd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
extern "C" void app_main(void)
{
    LCD lcd;

    lcd.init();
    lcd.setRGB(255,255,255);
vTaskDelay(5);    
    lcd.clear();
        lcd.setCursor(0, 0);
        lcd.printstr("Hello CSE121!");

        lcd.setCursor(0, 1);
        lcd.printstr("Ramirez");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}



