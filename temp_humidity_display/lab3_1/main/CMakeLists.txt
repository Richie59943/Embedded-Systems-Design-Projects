idf_component_register(SRCS "main.cpp" "lcd.cpp" 
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_driver_i2c esp_driver_gpio)
