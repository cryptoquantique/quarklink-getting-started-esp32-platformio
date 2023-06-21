// Include FreeRTOS for delay
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_chip_info.h" // for chip info
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include "esp_flash.h"

#include "esp_wifi.h"

#include <string.h>
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "driver/uart.h"
#include "driver/gpio.h"

#include "mbedtls/base64.h"

#include "quarklink.h"
#include "led_strip.h"

quarklink_context_t quarklink;

/** UART 
 * \brief The protocol adopted for serial communication is described in the README.
 * 
 */

/* The request type */
typedef enum {
    QUARKLINK_ROOT_CA   = 1,
    QUARKLINK_ENDPOINT  = 2,
    QUARKLINK_PORT      = 3,
    WIFI_SSID           = 4,
    WIFI_PASSWORD       = 5,
    DEVICE_ID           = 6,
} ief_data_t;

/* The IEF data messages */
static const char* IEF_QL_ROOT_CA    = "ql_rootCA";
static const char* IEF_QL_ENDPOINT   = "ql_endpoint";
static const char* IEF_QL_PORT       = "ql_port";
static const char* IEF_WIFI_SSID     = "wifi_ssid";
static const char* IEF_WIFI_PASSWORD = "wifi_password";
static const char* IEF_DEVICE_ID     = "deviceID";

/* UART pin configuration - leave the pins unchanged as we are using the USB serial */
#define IEF_UART_TX     (UART_PIN_NO_CHANGE)
#define IEF_UART_RX     (UART_PIN_NO_CHANGE)
#define IEF_UART_RTS    (UART_PIN_NO_CHANGE)
#define IEF_UART_CTS    (UART_PIN_NO_CHANGE)

/* UART configuration */
#define IEF_UART_PORT_NUM   0           // USB is on UART0
#define IEF_UART_BAUD_RATE  115200      // Use 115200 as default baudrate
#define UART_BUF_SIZE       1024        // The largest data we receive is the quarklink root CA certificate, encoded in base64 (~700 bytes)

/* Global buffer used for uart communications */
static char uartBuffer[UART_BUF_SIZE];
/*************************************/

/** WI-FI
 * \brief Most of the wi-fi stuff is taken from the example at https://github.com/espressif/esp-idf/tree/master/examples/wifi/getting_started/station
 * We could simplify it if we don't want to support all of the configurations.
 */
#if CONFIG_ESP_WPA3_SAE_PWE_HUNT_AND_PECK
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define EXAMPLE_H2E_IDENTIFIER ""
#elif CONFIG_ESP_WPA3_SAE_PWE_HASH_TO_ELEMENT
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HASH_TO_ELEMENT
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#elif CONFIG_ESP_WPA3_SAE_PWE_BOTH
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#endif
#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif


/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "wifi station";

static int s_retry_num = 0;

/** IEF
 * \brief Other IEF stuff
 * 
 */

/* Task size */
#define IEF_TASK_STACK_SIZE (1024 * 8)

// Delay between status requests, in ms
#define IEF_STATUS_REQUEST_INTERVAL 10000

// Tag for logs
static const char *IEF_TAG = "IEF";
static const char *WIFI_TAG = "WiFi";

/*************************************/

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < 10) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
    else if (event_base == ESP_HTTPS_OTA_EVENT) {
        switch (event_id) {
            case ESP_HTTPS_OTA_START:
                ESP_LOGI(IEF_TAG, "OTA started");
                break;
            case ESP_HTTPS_OTA_CONNECTED:
                ESP_LOGI(IEF_TAG, "Connected to server");
                break;
            case ESP_HTTPS_OTA_GET_IMG_DESC:
                ESP_LOGI(IEF_TAG, "Reading Image Description");
                break;
            case ESP_HTTPS_OTA_VERIFY_CHIP_ID:
                ESP_LOGI(IEF_TAG, "Verifying chip id of new image: %d", *(esp_chip_id_t *)event_data);
                break;
            case ESP_HTTPS_OTA_DECRYPT_CB:
                ESP_LOGI(IEF_TAG, "Callback to decrypt function");
                break;
            case ESP_HTTPS_OTA_WRITE_FLASH:
                ESP_LOGD(IEF_TAG, "Writing to flash: %d written", *(int *)event_data);
                break;
            case ESP_HTTPS_OTA_UPDATE_BOOT_PARTITION:
                ESP_LOGI(IEF_TAG, "Boot partition updated. Next Partition: %d", *(esp_partition_subtype_t *)event_data);
                break;
            case ESP_HTTPS_OTA_FINISH:
                ESP_LOGI(IEF_TAG, "OTA finish");
                break;
            case ESP_HTTPS_OTA_ABORT:
                ESP_LOGI(IEF_TAG, "OTA abort");
                break;
        }
    }
}

// Send a IEF start message, return -1 error, 0 success
static int send_ief_start() {
    static const char IEF_START_MESSAGE[] = "IEF DATA\n";

    // Send the message
    int ret = uart_write_bytes(IEF_UART_PORT_NUM, IEF_START_MESSAGE, sizeof(IEF_START_MESSAGE));
    if (ret != sizeof(IEF_START_MESSAGE))
        return -1;
    else 
        return 0;
}

/**
 * \brief Send a UART IEF data message. The function formats the packet according to the protocol defined in this file.
 * 
 * \param reqType       the request type
 * \param payload       the payload to send as a null-terminated string, NULL if no payload
 * \param buffer        the buffer that will contain the response as a null-terminated string
 * \param bufsize       the size of buffer
 * \return              the length of the response, -1 if failed
 */
static int send_ief_data(ief_data_t reqType, const char *payload, char *buffer, size_t bufsize) {
    if (send_ief_start() != 0) {
        return -1;
    }

    switch (reqType) {
        case QUARKLINK_ROOT_CA: {
            strcpy(uartBuffer, IEF_QL_ROOT_CA);
            break;
        }
        case QUARKLINK_ENDPOINT: {
            strcpy(uartBuffer, IEF_QL_ENDPOINT);
            break;
        }
        case QUARKLINK_PORT: {
            strcpy(uartBuffer, IEF_QL_PORT);
            break;
        }
        case WIFI_SSID: {
            strcpy(uartBuffer, IEF_WIFI_SSID);
            break;
        }
        case WIFI_PASSWORD: {
            strcpy(uartBuffer, IEF_WIFI_PASSWORD);
            break;
        }
        case DEVICE_ID: {
            strcpy(uartBuffer, IEF_DEVICE_ID);
            strcat(uartBuffer, ":");
            strcat(uartBuffer, payload);
            break;
        }
    }

    // Add new line
    strcat(uartBuffer, "\n");

    // Send the packet
    uart_write_bytes(IEF_UART_PORT_NUM, uartBuffer, strlen(uartBuffer));

    // Wait for the response
    int len = 0;
    while (!len) {
        len = uart_read_bytes(IEF_UART_PORT_NUM, buffer, bufsize, 100 / portTICK_PERIOD_MS);
        // TODO add a timeout ?

    }
    // Replace \n with end of string
    buffer[len] = '\0';
    return len;
}

/**
 * Format the packet and send a request of type reqType over UART, saves the response in buffer.
 * return length of response, -1 if error
*/
static int send_ief_request(ief_data_t reqType, char *buffer, size_t bufsize) {
    return send_ief_data(reqType, NULL, buffer, bufsize);
}

/* Read user's response. Ignore every char after the 1st */
char read_user_response() {
    int len = 0;
    while (!len) {
        len = uart_read_bytes(IEF_UART_PORT_NUM, uartBuffer, sizeof(UART_BUF_SIZE), 100 / portTICK_PERIOD_MS);
    }
    uartBuffer[len] = '\0';
    ESP_LOGD(IEF_TAG, "Read: %s", uartBuffer);
    return (char) uartBuffer[0];
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config;

    /* Load existing configuration and prompt user */
    esp_wifi_get_config(WIFI_IF_STA, &wifi_config);

    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 wifi_config.sta.ssid, wifi_config.sta.password);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 wifi_config.sta.ssid, wifi_config.sta.password);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}

void error_loop(const char *message) {
    if (message != NULL) {
        ESP_LOGE(TAG, "%s", message);
    }
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void fota_demo_task(void *pvParameter) {

    quarklink_return_t ql_ret;

    quarklink_return_t ql_status = QUARKLINK_ERROR;
    while (1) {
        /* get status */
        ESP_LOGI(TAG, "Get status");
        ql_status = quarklink_status(&quarklink);
        switch (ql_status) {
            case QUARKLINK_STATUS_ENROLLED:
                ESP_LOGI(TAG, "Enrolled");
                break;
            case QUARKLINK_STATUS_FWUPDATE_REQUIRED:
                ESP_LOGI(TAG, "Firmware Update required");
                break;
            case QUARKLINK_STATUS_NOT_ENROLLED:
                ESP_LOGI(TAG, "Not enrolled");
                break;
            case QUARKLINK_STATUS_CERTIFICATE_EXPIRED:
                ESP_LOGI(TAG, "Certificate expired");
                break;
            case QUARKLINK_STATUS_REVOKED:
                ESP_LOGI(TAG, "Device revoked");
                break;
            default:
                error_loop("Error during status request");
        }

        if (ql_status == QUARKLINK_STATUS_NOT_ENROLLED ||
            ql_status == QUARKLINK_STATUS_CERTIFICATE_EXPIRED ||
            ql_status == QUARKLINK_STATUS_REVOKED) {
            /* enroll */
            ESP_LOGI(TAG, "Enrol to %s", quarklink.endpoint);
            ql_ret = quarklink_enrol(&quarklink);
            switch (ql_ret) {
                case QUARKLINK_SUCCESS:
                    ESP_LOGI(TAG, "Successfully enrolled!");
                    break;
                case QUARKLINK_DEVICE_DOES_NOT_EXIST:
                    ESP_LOGW(TAG, "Device does not exist");
                    break;
                case QUARKLINK_DEVICE_REVOKED:
                    ESP_LOGW(TAG, "Device revoked");
                    break;
                case QUARKLINK_CACERTS_ERROR:
                default:
                    error_loop("Error during enrol");
            }
        }
   
        if (ql_status == QUARKLINK_STATUS_FWUPDATE_REQUIRED) {
            /* firmware update */
            ESP_LOGI(TAG, "Get firmware update");
            ql_ret = quarklink_firmwareUpdate(&quarklink, NULL);
            switch (ql_ret) {
                case QUARKLINK_FWUPDATE_UPDATED:
                    ESP_LOGI(TAG, "Firmware updated. Rebooting...");
                    esp_restart();
                    break;
                case QUARKLINK_FWUPDATE_NO_UPDATE:
                    ESP_LOGI(TAG, "No firmware update");
                    break;
                case QUARKLINK_FWUPDATE_WRONG_SIGNATURE:
                    ESP_LOGI(TAG, "Wrong firmware signature");
                    break;
                case QUARKLINK_FWUPDATE_MISSING_SIGNATURE:
                    ESP_LOGI(TAG, "Missing required firmware signature");
                    break;
                case QUARKLINK_FWUPDATE_ERROR:
                default:
                    error_loop("error while updating firmware");
            }
        }

        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }

}

// GPIO assignment
#define LED_STRIP_BLINK_GPIO  8

// LED numbers in the strip
#define LED_STRIP_LED_NUMBERS 1

// 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000)


void set_led(void){

    // LED Strip object handle
    led_strip_handle_t led_strip;

    // LED strip general initialization, according to your led board design
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_BLINK_GPIO,   // The GPIO that connected to the LED strip's data line
        .max_leds = LED_STRIP_LED_NUMBERS,        // The number of LEDs in the strip,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB, // Pixel format of your LED strip
        .led_model = LED_MODEL_WS2812,            // LED strip model
        .flags.invert_out = false,                // whether to invert the output signal
    };

    // LED strip backend configuration: RMT
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,        // different clock source can lead to different power consumption
        .resolution_hz = LED_STRIP_RMT_RES_HZ, // RMT counter clock frequency
        .flags.with_dma = false,               // DMA feature is available on ESP target like ESP32-S3
    };
    led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    

    led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);

    led_strip_set_pixel(led_strip, 0, 0, 0, 255);
    led_strip_refresh(led_strip);
}

void app_main(void)
{

    printf("**** New Version esp32-ql-binary-simple-demo esp32 c3 debug TEST 4.03 Secure ***\n");

    set_led();

    /* quarklink init */
    ESP_LOGI(TAG, "Initialising QuarkLink");
    quarklink_return_t ql_ret = quarklink_init(&quarklink, "", 6000, "");
    // TODO calling quarklink init will initialise the key at the first boot. init will be called again to update URL, PORT, ROOTCA
    ql_ret = quarklink_loadStoredContext(&quarklink);
    if (ql_ret != QUARKLINK_SUCCESS) {
        printf("ql_ret %d\n", ql_ret);
    }
    ESP_LOGI(TAG, "Device ID: %s", quarklink.deviceID);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    xTaskCreate(&fota_demo_task, "fota_demo_task", 1024 * 8, NULL, 5, NULL);
}
