/* HTTP GET Example using plain POSIX sockets

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "sdkconfig.h"
#include <stdio.h>

// includes for the temp sensor 
#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_check.h"

//defining our I2C Communication
#define I2C_MASTER_SCL_IO GPIO_NUM_8
#define I2C_MASTER_SCA_IO GPIO_NUM_7
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 100000
#define I2C_MASTER_TIMEOUT_MS 1000

//this is the address of our sensor on board our esp32c3
#define SHTC3_SENSOR_ADDR 0x70

//SHTC3 Commnads
static const uint8_t  sleep_cmd[2] = {0xB0,0x98};
static const uint8_t  wake_cmd[2] = {0x35,0x17};
static const uint8_t measure_cmd[2] = {0x78,0x66};

 static esp_err_t shtc3_write_cmd(i2c_master_dev_handle_t dev_handle,const uint8_t *buff, size_t len)
{
	return i2c_master_transmit(
			dev_handle,
			buff,
			len,
			pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS)
			);
}

 static esp_err_t shtc3_read_cmd(i2c_master_dev_handle_t dev_handle, uint8_t *buff, size_t len)
{
	return i2c_master_receive(dev_handle,
			buff,
			len,
			pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS)
			);
}

//this funciton is doing the check sum becuase out shtc3 sends 2 bytes of data then 1 of crc then 2 again an 1 checsome 
//the msb and lsb are the two btes of eother hum or temp 
static uint8_t shtc3_crc8(uint8_t msb, uint8_t lsb)
{
	uint8_t crc = 0xFF; //starts off at this value the datasheet says so 
	uint8_t bytes[2]; //array that can hold 2 bytes
	int i;
	int j;

	bytes[0] = msb;
	bytes[1] = lsb;

	for(i = 0; i < 2; i++)
	{
		crc = crc ^ bytes[i]; //xor
		for(j = 0; j < 8; j++) //each bytes has 8 bits so checsk each bit
		{
		if((crc & 0x80) != 0) // this is checking if the left most crc is 1 
		{
			crc = (crc << 1) ^ 0x31;
		}
		else {
			crc = (crc << 1);
		}
		}
	}
	return crc;
}



//this function is going to help up set up our i2c bus
static void setup_i2c_bus(i2c_master_bus_handle_t *bus_handle)
{
	i2c_master_bus_config_t i2c_master_config = {
		.clk_source = I2C_CLK_SRC_DEFAULT,
		.i2c_port = I2C_MASTER_NUM,
		.scl_io_num = I2C_MASTER_SCL_IO,
		.sda_io_num = I2C_MASTER_SCA_IO,
		.glitch_ignore_cnt = 7,
		.flags.enable_internal_pullup = true,
	};

	ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_master_config, bus_handle));

}


//this function is going to set up our device aka the SHTC3

static void set_dev_config(i2c_master_dev_handle_t *dev_handle,i2c_master_bus_handle_t bus_handle) // we create dev handle with a pointer becuase we are trying to modify it and we no longer have a pointer to bus_handle because we just want the value 
{
	i2c_device_config_t dev_config = {
	.dev_addr_length = I2C_ADDR_BIT_LEN_7,
	.device_address = SHTC3_SENSOR_ADDR,
	.scl_speed_hz = I2C_MASTER_FREQ_HZ,
	};

	ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config, dev_handle));
}





/* Constants that aren't configurable in menuconfig */
#define WEB_SERVER "172.20.10.10"
#define WEB_PORT "1234"
#define WEB_PATH "/"

#define WEB_SERVER_GET "172.20.10.10"
#define WEB_PORT_GET "1234"
#define WEB_PATH_GET "/location"

//defining paths for wttr request
#define WEB_SERVER_WTTR "wttr.in"
#define WEB_PORT_WTTR "80"
#define WEB_PATH_WTTR "/"



static const char *TAG = "example";

static const char *REQUEST = "GET " WEB_PATH_GET " HTTP/1.0\r\n"
    "Host: "WEB_SERVER":"WEB_PORT"\r\n"
    "User-Agent: esp-idf/1.0 esp32\r\n"
    "\r\n";

//request for the wttr.in 
static const char *WTTR_REQUEST = "GET " WEB_PATH_WTTR "HTTP/1.0\r\n"
    "Host: "WEB_SERVER_WTTR":"WEB_PORT_WTTR"\r\n"
    "User-Agent: esp-idf/1.0 esp32 curl\r\n"
  "\r\n";

char request_buffer[600]; // our buffer 
char wttr_request[100];
int length_of_mssg = 0; // this will hold the return value (size of buffer)
char output[400];
char temp[64];
char hum[64];

char http_responce_copy[1000]; //this is where we will keep a copy of out http responce in order to take out the body
int responce_index =0; // hold out place in for loop 
//

void get_location_formatted(char *location, char *new_formatted_location)
{
  int i;
  //takes in our location 
  for(i =0; i < strlen(location);i++)
  {
    //iterates throuhg untill it finds the place
    if(location[i] == ' ')
    {
      //in our new array we place the + instead of the " "
      new_formatted_location[i] = '+';
    }else{
      // if not we just keep passing from one array into another 
      new_formatted_location[i] = location[i];
    }
   
  }
  //we add the \o in order for it to be a actuall c string 
  new_formatted_location[i] = '\0';

}

void get_outdoor_temp(char *new_formatted_location, char *outdoor_temp, size_t outdoor_temp_size)
{
  char http_responce_copy_temp[500];
  char get_request_temp[900];
  int responce_index_temp =0;
  snprintf(get_request_temp, sizeof(get_request_temp),"GET /%s?format=%%t HTTP/1.0\r\n"
           "Host: "WEB_SERVER_WTTR":"WEB_PORT_WTTR"\r\n"
           "User-Agent: esp-idf/1.0 esp32 curl\r\n"
           "\r\n",new_formatted_location);


//http server code 
   const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;
    struct in_addr *addr;
    int s, r;
    char recv_buf[64];

   
        int err = getaddrinfo(WEB_SERVER_WTTR, WEB_PORT_WTTR, &hints, &res);

        if(err != 0 || res == NULL) {
            ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
                    }

        /* Code to print the resolved IP.

           Note: inet_ntoa is non-reentrant, look at ipaddr_ntoa_r for "real" code */
        addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
        ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

        s = socket(res->ai_family, res->ai_socktype, 0);
        if(s < 0) {
            ESP_LOGE(TAG, "... Failed to allocate socket.");
            freeaddrinfo(res);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            return;
        }
        ESP_LOGI(TAG, "... allocated socket");

        if(connect(s, res->ai_addr, res->ai_addrlen) != 0) {
            ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);
            close(s);
            freeaddrinfo(res);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            return;
        }

        ESP_LOGI(TAG, "... connected");
        freeaddrinfo(res);

        if (write(s,get_request_temp, strlen(get_request_temp)) < 0) {
            ESP_LOGE(TAG, "... socket send failed");
            close(s);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            return;

        }
        ESP_LOGI(TAG, "... socket send success");

        struct timeval receiving_timeout;
        receiving_timeout.tv_sec = 5;
        receiving_timeout.tv_usec = 0;
        if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout,
                sizeof(receiving_timeout)) < 0) {
            ESP_LOGE(TAG, "... failed to set socket receiving timeout");
            close(s);
            vTaskDelay(4000 / portTICK_PERIOD_MS);

        }
        ESP_LOGI(TAG, "... set socket receiving timeout success");

        /* Read HTTP response */
        do {
            bzero(recv_buf, sizeof(recv_buf));
            r = read(s, recv_buf, sizeof(recv_buf)-1);
            for(int i = 0; i < r; i++) {
                //we are going to store the http request the entire thing
                http_responce_copy_temp[responce_index_temp] = recv_buf[i];

                putchar(recv_buf[i]);
                responce_index_temp++; //going to help us move along our http request 
            }
                 } while(r > 0);


    
  http_responce_copy_temp[responce_index_temp] = '\0';
  
  char *body_temp = strstr(http_responce_copy_temp,"\r\n\r\n");

  if(body_temp != NULL)
  {
    body_temp = body_temp + 4;

    snprintf(outdoor_temp,outdoor_temp_size, "%s",body_temp);

    printf("Outside Temp: %s\n", outdoor_temp);

  }
  
}




void get_location_from_server(char *location, size_t location_size)
//static void http_get_task(void *pvParameters)
{
  const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;
    struct in_addr *addr;
    int s, r;
    char recv_buf[64];

   
        int err = getaddrinfo(WEB_SERVER, WEB_PORT, &hints, &res);

        if(err != 0 || res == NULL) {
            ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
                    }

        /* Code to print the resolved IP.

           Note: inet_ntoa is non-reentrant, look at ipaddr_ntoa_r for "real" code */
        addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
        ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

        s = socket(res->ai_family, res->ai_socktype, 0);
        if(s < 0) {
            ESP_LOGE(TAG, "... Failed to allocate socket.");
            freeaddrinfo(res);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        ESP_LOGI(TAG, "... allocated socket");

        if(connect(s, res->ai_addr, res->ai_addrlen) != 0) {
            ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);
            close(s);
            freeaddrinfo(res);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
        }

        ESP_LOGI(TAG, "... connected");
        freeaddrinfo(res);

        if (write(s,REQUEST , strlen(REQUEST)) < 0) {
            ESP_LOGE(TAG, "... socket send failed");
            close(s);
            vTaskDelay(4000 / portTICK_PERIOD_MS);

        }
        ESP_LOGI(TAG, "... socket send success");

        struct timeval receiving_timeout;
        receiving_timeout.tv_sec = 5;
        receiving_timeout.tv_usec = 0;
        if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout,
                sizeof(receiving_timeout)) < 0) {
            ESP_LOGE(TAG, "... failed to set socket receiving timeout");
            close(s);
            vTaskDelay(4000 / portTICK_PERIOD_MS);

        }
        ESP_LOGI(TAG, "... set socket receiving timeout success");

        /* Read HTTP response */
        do {
            bzero(recv_buf, sizeof(recv_buf));
            r = read(s, recv_buf, sizeof(recv_buf)-1);
            for(int i = 0; i < r; i++) {
                //we are going to store the http request the entire thing
                http_responce_copy[responce_index] = recv_buf[i];

                putchar(recv_buf[i]);
                responce_index++; //going to help us move along our http request 
            }
                 } while(r > 0);


    http_responce_copy[responce_index] = '\0'; // so that it is a proper C string

  char  *body_start = strstr(http_responce_copy,"\r\n\r\n");

      if(body_start != NULL)
    {
      body_start = body_start + 4;

      snprintf(location, location_size,"%s",body_start); // this is going to put the body into the location array
         }
 printf("Body: %s\n", body_start);


        ESP_LOGI(TAG, "... done reading from socket. Last read return=%d errno=%d.", r, errno);
        close(s);
        for(int countdown = 10; countdown >= 0; countdown--) {
            ESP_LOGI(TAG, "%d... ", countdown);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        ESP_LOGI(TAG, "Starting again!");
    
}





static void http_post_task(void *pvParameters)                                                                                          
{
  char outdoor_temp[100]; // this is going to store our outside temp 
  char location[100]; // yhis is going to hold out location 

  char new_formatted_location[100];

i2c_master_dev_handle_t dev_handle =
    (i2c_master_dev_handle_t) pvParameters;

    uint8_t data[6];

    uint16_t raw_temp;
    uint16_t raw_humidity;

    uint8_t temp_crc;
    uint8_t humidity_crc;

    float temp_c;
    float temp_f;
    float humidity;


    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;
    struct in_addr *addr;
    int s, r;
    char recv_buf[64];

    while(1) {
      
    get_location_from_server(location,sizeof(location));
    printf("Server Location: %s\n", location);

    get_location_formatted(location,new_formatted_location);
    
    printf("Formated location: %s\n",new_formatted_location);


    get_outdoor_temp(new_formatted_location,outdoor_temp,sizeof(outdoor_temp));
    

     ESP_ERROR_CHECK(shtc3_write_cmd(dev_handle,wake_cmd,sizeof(wake_cmd)));
    vTaskDelay(pdMS_TO_TICKS(10));

    ESP_ERROR_CHECK(shtc3_write_cmd(dev_handle,measure_cmd, sizeof(measure_cmd)));
    vTaskDelay(pdMS_TO_TICKS(20));

    ESP_ERROR_CHECK(shtc3_read_cmd(dev_handle,data, 6));
    vTaskDelay(pdMS_TO_TICKS(10));

    ESP_ERROR_CHECK(shtc3_write_cmd(dev_handle,sleep_cmd,sizeof(sleep_cmd)));
    vTaskDelay(pdMS_TO_TICKS(20));



    temp_crc = shtc3_crc8(data[0], data[1]);
        humidity_crc = shtc3_crc8(data[3], data[4]);

        if ((temp_crc != data[2]) || (humidity_crc != data[5])) 
	{
            printf("Sensor read failed\n");
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
	}
 

        raw_temp = ((uint16_t)data[0] << 8) | data[1];
        raw_humidity = ((uint16_t)data[3] << 8) | data[4];

        temp_c = -45.0f + (175.0f * (float)raw_temp / 65536.0f);
        temp_f = (temp_c * 9.0f / 5.0f) + 32.0f;
        humidity = 100.0f * (float)raw_humidity / 65536.0f;

      snprintf(output,sizeof(output),"\nLocation: %s\nOutdoor Temp: %s\nESP32 Sensor Temp: %.2f C\nESP32 Sensor Humidity: %.2f%%",
               location,
               outdoor_temp,
               temp_c,
               humidity);
    


snprintf(request_buffer,
         sizeof(request_buffer),
         "POST " WEB_PATH " HTTP/1.0\r\n"
         "Host: " WEB_SERVER ":" WEB_PORT "\r\n"
         "Content-Type: text/plain\r\n"
         "Content-Length: %zu\r\n"
         "User-Agent: esp-idf/1.0 esp32 curl\r\n"
         "\r\n"
         "%s",
         strlen(output),
         output);



        


    

  
  


        int err = getaddrinfo(WEB_SERVER, WEB_PORT, &hints, &res);

        if(err != 0 || res == NULL) {
            ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        /* Code to print the resolved IP.

           Note: inet_ntoa is non-reentrant, look at ipaddr_ntoa_r for "real" code */
        addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
        ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

        s = socket(res->ai_family, res->ai_socktype, 0);
        if(s < 0) {
            ESP_LOGE(TAG, "... Failed to allocate socket.");
            freeaddrinfo(res);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... allocated socket");

        if(connect(s, res->ai_addr, res->ai_addrlen) != 0) {
            ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);
            close(s);
            freeaddrinfo(res);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }

        ESP_LOGI(TAG, "... connected");
        freeaddrinfo(res);

        if (write(s,request_buffer , strlen(request_buffer)) < 0) {
            ESP_LOGE(TAG, "... socket send failed");
            close(s);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... socket send success");

        struct timeval receiving_timeout;
        receiving_timeout.tv_sec = 5;
        receiving_timeout.tv_usec = 0;
        if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout,
                sizeof(receiving_timeout)) < 0) {
            ESP_LOGE(TAG, "... failed to set socket receiving timeout");
            close(s);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... set socket receiving timeout success");

        /* Read HTTP response */
        do {
            bzero(recv_buf, sizeof(recv_buf));
            r = read(s, recv_buf, sizeof(recv_buf)-1);
            for(int i = 0; i < r; i++) {
                putchar(recv_buf[i]);
            }
        } while(r > 0);
      

        ESP_LOGI(TAG, "... done reading from socket. Last read return=%d errno=%d.", r, errno);
        close(s);
        for(int countdown = 10; countdown >= 0; countdown--) {
            ESP_LOGI(TAG, "%d... ", countdown);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        ESP_LOGI(TAG, "Starting again!");
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK( nvs_flash_init() );
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

	i2c_master_bus_handle_t bus_handle;

	i2c_master_dev_handle_t dev_handle;
  
  setup_i2c_bus(&bus_handle);
  set_dev_config(&dev_handle,bus_handle);

  xTaskCreate(&http_post_task, "http_post_task", 4096, dev_handle, 5, NULL);
  
}
