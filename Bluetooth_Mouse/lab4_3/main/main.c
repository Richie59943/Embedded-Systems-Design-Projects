/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"

#include "esp_hidd_prf_api.h"
#include "esp_bt_defs.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "driver/gpio.h"
#include "hid_dev.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"


/**
 * 
 * Brief:
 * This example Implemented BLE HID device profile related functions, in which the HID device
 * has 4 Reports (1 is mouse, 2 is keyboard and LED, 3 is Consumer Devices, 4 is Vendor devices).
 * Users can choose different reports according to their own application scenarios.
 * BLE HID profile inheritance and USB HID class.
 */

/**
 * Note:
 * 1. Win10 does not support vendor report , So SUPPORT_REPORT_VENDOR is always set to FALSE, it defines in hidd_le_prf_int.h
 * 2. Update connection parameters are not allowed during iPhone HID encryption, slave turns
 * off the ability to automatically update connection parameters during encryption.
 * 3. After our HID device is connected, the iPhones write 1 to the Report Characteristic Configuration Descriptor,
 * even if the HID encryption is not completed. This should actually be written 1 after the HID encryption is completed.
 * we modify the permissions of the Report Characteristic Configuration Descriptor to `ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE_ENCRYPTED`.
 * if you got `GATT_INSUF_ENCRYPTION` error, please ignore.
 */

#define HID_DEMO_TAG "HID_DEMO"


static uint16_t hid_conn_id = 1;
static bool sec_conn = false;
//static bool send_volum_up = false;
int  move_mouse_right;
int  move_mouse_left;

#define CHAR_DECLARATION_SIZE   (sizeof(uint8_t))

//creating a static function where we read accel from x and return it as a decimal value 
static int16_t read_accel_x(i2c_master_dev_handle_t dev_handle)
{
	//these are our commands that we need to send to our sensor 
	uint8_t ACCEL_X1 = 0x0B;
	uint8_t ACCEL_X0 = 0x0C;

	//this is where we are going to store our high and low bytes that these commands get us
	uint8_t high_bytes = 0;
	uint8_t low_bytes = 0;

	//this is where we are going to store the full decimal value 
	int16_t ACCEL_X =0;

	//these are the commands that will send and receive our data for the icm sensor 
	i2c_master_transmit_receive(dev_handle,&ACCEL_X1,1,&high_bytes,1,-1);
	i2c_master_transmit_receive(dev_handle,&ACCEL_X0,1,&low_bytes,1,-1);

	ACCEL_X= (int16_t)((high_bytes << 8) | low_bytes);
	return ACCEL_X;
}

//creating a function to read accel from y and returning it as a decimal value
static int16_t read_accel_y(i2c_master_dev_handle_t dev_handle)
{
	//these are commands that we need to send to our sensor
	uint8_t ACCEL_Y1 = 0x0D;
	uint8_t ACCEL_Y0 = 0x0E;

	//this is where we will hold low and high bytes 
	uint8_t high_bytes = 0;
	uint8_t low_bytes = 0;

	//going to holw our final value 
	int16_t ACCEL_Y = 0;

	//commands to send and receive our commands 
	i2c_master_transmit_receive(dev_handle,&ACCEL_Y1,1,&high_bytes,1,-1);
	i2c_master_transmit_receive(dev_handle,&ACCEL_Y0,1,&low_bytes,1,-1);

	//holding our final vlaue this is a mask since we need ot shift by 8 becaus eit is our top 8 bytes
	ACCEL_Y= (int16_t)((high_bytes << 8) | low_bytes);

	return ACCEL_Y;
}






static void hidd_event_callback(esp_hidd_cb_event_t event, esp_hidd_cb_param_t *param);

#define HIDD_DEVICE_NAME            "HID"
static uint8_t hidd_service_uuid128[] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    //first uuid, 16bit, [12],[13] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x12, 0x18, 0x00, 0x00,
};

static esp_ble_adv_data_t hidd_adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = ESP_BLE_GAP_CONN_ITVL_MS(7.5), //slave connection min interval
    .max_interval = ESP_BLE_GAP_CONN_ITVL_MS(20), //slave connection max interval
    .appearance = 0x03c0,       //HID Generic,
    .manufacturer_len = 0,
    .p_manufacturer_data =  NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(hidd_service_uuid128),
    .p_service_uuid = hidd_service_uuid128,
    .flag = 0x6,
};

static esp_ble_adv_params_t hidd_adv_params = {
    .adv_int_min        = ESP_BLE_GAP_ADV_ITVL_MS(20),
    .adv_int_max        = ESP_BLE_GAP_ADV_ITVL_MS(30),
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    //.peer_addr            =
    //.peer_addr_type       =
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};


static void hidd_event_callback(esp_hidd_cb_event_t event, esp_hidd_cb_param_t *param)
{
    switch(event) {
        case ESP_HIDD_EVENT_REG_FINISH: {
            if (param->init_finish.state == ESP_HIDD_INIT_OK) {
                //esp_bd_addr_t rand_addr = {0x04,0x11,0x11,0x11,0x11,0x05};
                esp_ble_gap_set_device_name(HIDD_DEVICE_NAME);
                esp_ble_gap_config_adv_data(&hidd_adv_data);

            }
            break;
        }
        case ESP_BAT_EVENT_REG: {
            break;
        }
        case ESP_HIDD_EVENT_DEINIT_FINISH:
	     break;
		case ESP_HIDD_EVENT_BLE_CONNECT: {
            ESP_LOGI(HID_DEMO_TAG, "ESP_HIDD_EVENT_BLE_CONNECT");
            hid_conn_id = param->connect.conn_id;
            break;
        }
        case ESP_HIDD_EVENT_BLE_DISCONNECT: {
            sec_conn = false;
            ESP_LOGI(HID_DEMO_TAG, "ESP_HIDD_EVENT_BLE_DISCONNECT");
            esp_ble_gap_start_advertising(&hidd_adv_params);
            break;
        }
        case ESP_HIDD_EVENT_BLE_VENDOR_REPORT_WRITE_EVT: {
            ESP_LOGI(HID_DEMO_TAG, "%s, ESP_HIDD_EVENT_BLE_VENDOR_REPORT_WRITE_EVT", __func__);
            ESP_LOG_BUFFER_HEX(HID_DEMO_TAG, param->vendor_write.data, param->vendor_write.length);
            break;
        }
        case ESP_HIDD_EVENT_BLE_LED_REPORT_WRITE_EVT: {
            ESP_LOGI(HID_DEMO_TAG, "ESP_HIDD_EVENT_BLE_LED_REPORT_WRITE_EVT");
            ESP_LOG_BUFFER_HEX(HID_DEMO_TAG, param->led_write.data, param->led_write.length);
            break;
        }
        default:
            break;
    }
    return;
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&hidd_adv_params);
        break;
     case ESP_GAP_BLE_SEC_REQ_EVT:
        for(int i = 0; i < ESP_BD_ADDR_LEN; i++) {
             ESP_LOGD(HID_DEMO_TAG, "%x:",param->ble_security.ble_req.bd_addr[i]);
        }
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
	 break;
     case ESP_GAP_BLE_AUTH_CMPL_EVT:
        esp_bd_addr_t bd_addr;
        memcpy(bd_addr, param->ble_security.auth_cmpl.bd_addr, sizeof(esp_bd_addr_t));
        ESP_LOGI(HID_DEMO_TAG, "remote BD_ADDR: %08x%04x",\
                (bd_addr[0] << 24) + (bd_addr[1] << 16) + (bd_addr[2] << 8) + bd_addr[3],
                (bd_addr[4] << 8) + bd_addr[5]);
        ESP_LOGI(HID_DEMO_TAG, "address type = %d", param->ble_security.auth_cmpl.addr_type);
        ESP_LOGI(HID_DEMO_TAG, "pair status = %s",param->ble_security.auth_cmpl.success ? "success" : "fail");
        if (param->ble_security.auth_cmpl.success) {
            sec_conn = true;
            ESP_LOGI(HID_DEMO_TAG, "secure connection established.");
        } else {
            ESP_LOGE(HID_DEMO_TAG, "pairing failed, reason = 0x%x",
                     param->ble_security.auth_cmpl.fail_reason);
        }
        break;
    default:
        break;
    }
}

void hid_demo_task(void *pvParameters)
{
    i2c_master_dev_handle_t dev_handle = (i2c_master_dev_handle_t) pvParameters;

    //these are some thresholds we are creating these let us know when we go from a little to ALOT
    int X_BIT_THRESHOLD = 150;
    int X_LOT_THRESHOLD = 1200;

    int Y_BIT_THRESHOLD = 150;
    int Y_LOT_THRESHOLD = 1200;

    // this is the step size of our mouse is going to take 
    int bit_speed = 2;
    int lot_speed = 4;

    //this is our accel multiplier
    int accel_multiplier = 1;

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    // while loop
    while (1) {

        vTaskDelay(50 / portTICK_PERIOD_MS);
	
	// this is checking if we have a bluetooth connection established
        if (sec_conn) {
	
		//getting the decimal value from our read function
            int16_t x_value = read_accel_x(dev_handle);
            int16_t y_value = read_accel_y(dev_handle);

	    //our deltas
            int x_delta = 0;
            int y_delta = 0;
		


	    // left positive up postive 
	    //these are just checking if our values are grater or less than our thressholds in order to know by how muhc we move our mouse
            if (x_value > X_LOT_THRESHOLD) {
                x_delta = -lot_speed;
            }
            else if (x_value > X_BIT_THRESHOLD) {
                x_delta = -bit_speed;
            }
            else if (x_value < -X_LOT_THRESHOLD) {
                x_delta = lot_speed;
            }
            else if (x_value < -X_BIT_THRESHOLD) {
                x_delta = bit_speed;
            }

            if (y_value > Y_LOT_THRESHOLD) {
                y_delta = -lot_speed;
            }
            else if (y_value > Y_BIT_THRESHOLD) {
                y_delta = -bit_speed;
            }
            else if (y_value < -Y_LOT_THRESHOLD) {
                y_delta = lot_speed;
            }
            else if (y_value < -Y_BIT_THRESHOLD) {
                y_delta = bit_speed;
            }

	    //this is checing if the board is being tilted anyway
            if (x_delta != 0 || y_delta != 0) {
                if (accel_multiplier < 3) { //keeps the multiplier from getting bigger
                    accel_multiplier++;
                }
            }
            else {
                accel_multiplier = 1;
            }

            x_delta = x_delta * accel_multiplier;
            y_delta = y_delta * accel_multiplier;
	    
            if (x_delta != 0 || y_delta != 0) {
                esp_hidd_send_mouse_value(hid_conn_id, 0, x_delta, y_delta);
            }

	  	    if(x_delta == 0 && y_delta == 0)
	    {
		    esp_hidd_send_mouse_value(hid_conn_id,1,0,0);
		    ESP_LOGI(HID_DEMO_TAG,"MOUSE IS BEING PRESSED!");
	    }
	  

            ESP_LOGI(HID_DEMO_TAG, "X=%d Y=%d x_delta=%d y_delta=%d a=%d",
                     x_value, y_value, x_delta, y_delta, accel_multiplier);
        }
    }
}
void app_main(void)
{

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





    esp_err_t ret;

    // Initialize NVS.
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(HID_DEMO_TAG, "%s initialize controller failed", __func__);
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(HID_DEMO_TAG, "%s enable controller failed", __func__);
        return;
    }

    esp_bluedroid_config_t cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    ret = esp_bluedroid_init_with_cfg(&cfg);
    if (ret) {
        ESP_LOGE(HID_DEMO_TAG, "%s init bluedroid failed", __func__);
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(HID_DEMO_TAG, "%s init bluedroid failed", __func__);
        return;
    }

    if((ret = esp_hidd_profile_init()) != ESP_OK) {
        ESP_LOGE(HID_DEMO_TAG, "%s init bluedroid failed", __func__);
    }

    ///register the callback function to the gap module
    esp_ble_gap_register_callback(gap_event_handler);
    esp_hidd_register_callbacks(hidd_event_callback);

    /* set the security iocap & auth_req & key size & init key response key parameters to the stack*/
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_BOND;     //bonding with peer device after authentication
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;           //set the IO capability to No output No input
    uint8_t key_size = 16;      //the key size should be 7~16 bytes
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
    /* If your BLE device act as a Slave, the init_key means you hope which types of key of the master should distribute to you,
    and the response key means which key you can distribute to the Master;
    If your BLE device act as a master, the response key means you hope which types of key of the slave should distribute to you,
    and the init key means which key you can distribute to the slave. */
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));

    xTaskCreate(&hid_demo_task, "hid_task", 4096, dev_handle, 5, NULL);
}
