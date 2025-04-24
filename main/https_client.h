#include <string.h>
#include <sys/param.h>
#include <stdlib.h>
#include <ctype.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "protocol_examples_utils.h"
#include "esp_tls.h"
#include "cJSON.h"
#include "esp_system.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_http_client.h"
#include "esp_mac.h"

#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 2048

typedef struct {
    int ina_out;
    float power;
    float voltage;
    float current;
} sensor_data_params_t;

typedef struct {
    char *buffer;
    int length;
    int max_length;
} http_buffer_ctx_t;

char* print_time();
void initialize_sntp();
void wait_for_time_sync();
void send_sensor_data(int ina_out, float power, float voltage, float current);
void http_test_task(void *pvParameters);
void send_pic_data(uint8_t buf[32]);

