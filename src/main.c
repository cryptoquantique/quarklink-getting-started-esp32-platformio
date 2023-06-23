#include <freertos/FreeRTOS.h>
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_https_ota.h"

#include "quarklink.h"
#include "led_strip.h"

#include "mqtt_client.h"
#include "esp_ota_ops.h"

/* LED Strip */
#define LED_STRIP_BLINK_GPIO  8 // GPIO assignment
#define LED_STRIP_LED_NUMBERS 1 // LED numbers in the strip
#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000) // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;

static const char *TAG = "wifi station";
static const char *IEF_TAG = "IEF";
static const char *QL_TAG = "QL";

quarklink_context_t quarklink;

// #
// # MQTT Example Configuration
// #
const char *CONFIG_BROKER_URI = "mqtts://mqtt.eclipseprojects.io:8883";
// CONFIG_BROKER_CERTIFICATE_OVERRIDE=""
int CONFIG_BROKER_BIN_SIZE_TO_SEND = 20000;
// # end of MQTT Example Configuration

#if CONFIG_BROKER_CERTIFICATE_OVERRIDDEN == 1
static const uint8_t mqtt_eclipseprojects_io_pem_start[]  = "-----BEGIN CERTIFICATE-----\n" CONFIG_BROKER_CERTIFICATE_OVERRIDE "\n-----END CERTIFICATE-----";
#else
extern const uint8_t mqtt_eclipseprojects_io_pem_start[]   asm("_binary_mqtt_eclipseprojects_io_pem_start");
#endif
extern const uint8_t mqtt_eclipseprojects_io_pem_end[]   asm("_binary_mqtt_eclipseprojects_io_pem_end");

//
// Note: this function is for testing purposes only publishing part of the active partition
//       (to be checked against the original binary)
//
static void send_binary(esp_mqtt_client_handle_t client)
{
    esp_partition_mmap_handle_t out_handle;
    const void *binary_address;
    const esp_partition_t *partition = esp_ota_get_running_partition();
    esp_partition_mmap(partition, 0, partition->size, ESP_PARTITION_MMAP_DATA, &binary_address, &out_handle);
    // sending only the configured portion of the partition (if it's less than the partition size)
    int binary_size = MIN(CONFIG_BROKER_BIN_SIZE_TO_SEND, partition->size);
    int msg_id = esp_mqtt_client_publish(client, "/topic/binary", binary_address, binary_size, 0, 0);
    ESP_LOGI(TAG, "binary sent with msg_id=%d", msg_id);
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
        ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        if (strncmp(event->data, "send binary please", event->data_len) == 0) {
            ESP_LOGI(TAG, "Sending the binary");
            send_binary(client);
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
            ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
            ESP_LOGI(TAG, "Last captured errno : %d (%s)",  event->error_handle->esp_transport_sock_errno,
                     strerror(event->error_handle->esp_transport_sock_errno));
        } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
            ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
        } else {
            ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt_app_start(void)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = "mqtts://aqjjhlxf7iuc2-ats.iot.eu-west-2.amazonaws.com:8883",
            .verification.certificate = quarklink.iotHubRootCert
        },
    };

    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}


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
    wifi_config_t wifi_config;   // PUT IT BACK AFTER REMOVING THE REST OF HARD_CODED CLAUDIA
    
    // // HARD_CODED CLAUDIA
    // wifi_config_t wifi_config = {
    //     .sta = {
    //         .ssid = "vodafone4BAA80",
    //         .password = "Pr2YgfcAfmM9fYCn",
    //         /* Setting a password implies station will connect to all security modes including WEP/WPA.
    //          * However these modes are deprecated and not advisable to be used. Incase your Access point
    //          * doesn't support WPA2, these mode can be enabled by commenting below line */
	//      .threshold.authmode = WIFI_AUTH_WPA2_PSK,
    //     },
    // }; //HARD_CODED CLAUDIA


    // /* Load existing configuration and prompt user */
    esp_wifi_get_config(WIFI_IF_STA, &wifi_config); // PUT IT BACK AFTER REMOVING THE REST OF HARD_CODED CLAUDIA

    // ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) ); //HARD_CODED CLAUDIA
    // ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) ); //HARD_CODED CLAUDIA

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
        ESP_LOGI(TAG, "connected to ap SSID:%s",
                 wifi_config.sta.ssid);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s",
                 wifi_config.sta.ssid);
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
        ESP_LOGE(QL_TAG, "%s", message);
    }
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void getting_started_task(void *pvParameter) {

    quarklink_return_t ql_ret;
    quarklink_return_t ql_status = QUARKLINK_ERROR;
    while (1) {
        /* get status */
        ESP_LOGI(QL_TAG, "Get status");
        ql_status = quarklink_status(&quarklink);
        switch (ql_status) {
            case QUARKLINK_STATUS_ENROLLED:
                ESP_LOGI(QL_TAG, "Enrolled");
                break;
            case QUARKLINK_STATUS_FWUPDATE_REQUIRED:
                ESP_LOGI(QL_TAG, "Firmware Update required");
                break;
            case QUARKLINK_STATUS_NOT_ENROLLED:
                ESP_LOGI(QL_TAG, "Not enrolled");
                break;
            case QUARKLINK_STATUS_CERTIFICATE_EXPIRED:
                ESP_LOGI(QL_TAG, "Certificate expired");
                break;
            case QUARKLINK_STATUS_REVOKED:
                ESP_LOGI(QL_TAG, "Device revoked");
                break;
            default:
                error_loop("Error during status request");
        }

        if (ql_status == QUARKLINK_STATUS_NOT_ENROLLED ||
            ql_status == QUARKLINK_STATUS_CERTIFICATE_EXPIRED ||
            ql_status == QUARKLINK_STATUS_REVOKED) {
            /* enroll */
            ESP_LOGI(QL_TAG, "Enrol to %s", quarklink.endpoint);
            ql_ret = quarklink_enrol(&quarklink);
            switch (ql_ret) {
                case QUARKLINK_SUCCESS:
                    ESP_LOGI(QL_TAG, "Successfully enrolled!");
                    //mqtt_app_start();
                    break;
                case QUARKLINK_DEVICE_DOES_NOT_EXIST:
                    ESP_LOGW(QL_TAG, "Device does not exist");
                    break;
                case QUARKLINK_DEVICE_REVOKED:
                    ESP_LOGW(QL_TAG, "Device revoked");
                    break;
                case QUARKLINK_CACERTS_ERROR:
                default:
                    error_loop("Error during enrol");
            }
        }
   
        if (ql_status == QUARKLINK_STATUS_FWUPDATE_REQUIRED) {
            /* firmware update */
            ESP_LOGI(QL_TAG, "Get firmware update");
            ql_ret = quarklink_firmwareUpdate(&quarklink, NULL);
            switch (ql_ret) {
                case QUARKLINK_FWUPDATE_UPDATED:
                    ESP_LOGI(QL_TAG, "Firmware updated. Rebooting...");
                    esp_restart();
                    break;
                case QUARKLINK_FWUPDATE_NO_UPDATE:
                    ESP_LOGI(QL_TAG, "No firmware update");
                    break;
                case QUARKLINK_FWUPDATE_WRONG_SIGNATURE:
                    ESP_LOGI(QL_TAG, "Wrong firmware signature");
                    break;
                case QUARKLINK_FWUPDATE_MISSING_SIGNATURE:
                    ESP_LOGI(QL_TAG, "Missing required firmware signature");
                    break;
                case QUARKLINK_FWUPDATE_ERROR:
                default:
                    error_loop("error while updating firmware");
            }
        }
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }

}

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
    led_strip_set_pixel(led_strip, 0, 0, 0, 255);
    led_strip_refresh(led_strip);
}


// HARD CODED CLAUDIA
/* Quarklinik Details. */
char *CQ_ENDPOINT = "cqtest.quarklink-staging.io";
int CQ_PORT = 6000;
char *CQ_GATEWAY_CERT = // CA_CERT for cqtest
    "-----BEGIN CERTIFICATE-----\n"\
    "MIIBXjCCAQSgAwIBAgIIF1CZM8uRvrgwCgYIKoZIzj0EAwIwEjEQMA4GA1UEAxMH\n"\
    "T0VNUm9vdDAgFw0yMzAzMjgxMzQ1MDhaGA8yMDUzMDMyMDEzNDUwOFowEjEQMA4G\n"\
    "A1UEAxMHT0VNUm9vdDBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABA4x1u8ICV/T\n"\
    "u2bQjY2O1HxqgM56WGENWqO+vZ1c3bk2ApNHTZ5r/MKwyP67DG5SbAFlCzt0m2Ab\n"\
    "yW/bmjt8bkejQjBAMA4GA1UdDwEB/wQEAwIChDAPBgNVHRMBAf8EBTADAQH/MB0G\n"\
    "A1UdDgQWBBQrDtBEyyVI9/bcRyuh3jjEs7JpijAKBggqhkjOPQQDAgNIADBFAiBi\n"\
    "KYsy0UjyLLWOZSbMLYjfCYIUC645HrUNObLNmPxnRwIhAItZM1y4aRvEm0xjKSP9\n"\
    "VOHBMBILDCC8OvfSjuP2phoU\n"\
    "-----END CERTIFICATE-----\n";


void app_main(void)
{

    printf("\n quarklink-getting_started esp32-c3 mqtt WIP Hard coded 0.1.0 BLUE LED*\n");
    set_led(); // esp32-c3 RGB LED

    /* quarklink init */
    ESP_LOGI(QL_TAG, "Initialising QuarkLink");
    //quarklink_return_t ql_ret = quarklink_init(&quarklink, CQ_ENDPOINT, 6000, CQ_GATEWAY_CERT); //HARD_CODED CLAUDIA
    quarklink_return_t ql_ret = quarklink_init(&quarklink, "", 6000, ""); // PUT IT BACK AFTER REMOVING THE REST OF HARD_CODED CLAUDIA
    //TODO calling quarklink init will initialise the key at the first boot. init will be called again to update URL, PORT, ROOTCA
    ql_ret = quarklink_loadStoredContext(&quarklink); // PUT IT BACK AFTER REMOVING THE REST OF HARD_CODED CLAUDIA
    if (ql_ret != QUARKLINK_SUCCESS) { // PUT IT BACK AFTER REMOVING THE REST OF HARD_CODED CLAUDIA
        printf("ql_ret %d\n", ql_ret); // PUT IT BACK AFTER REMOVING THE REST OF HARD_CODED CLAUDIA
    } // PUT IT BACK AFTER REMOVING THE REST OF HARD_CODED CLAUDIA
    ESP_LOGI(QL_TAG, "Device ID: %s", quarklink.deviceID);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    xTaskCreate(&getting_started_task, "getting_started_task", 1024 * 8, NULL, 5, NULL);
}
