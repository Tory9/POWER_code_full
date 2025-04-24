#include "https_client.h"

static const char *TAG = "HTTP_CLIENT";

extern const char howsmyssl_com_root_cert_pem_start[] asm("_binary_howsmyssl_com_root_cert_pem_start");
extern const char howsmyssl_com_root_cert_pem_end[]   asm("_binary_howsmyssl_com_root_cert_pem_end");

extern const char postman_root_cert_pem_start[] asm("_binary_postman_root_cert_pem_start");
extern const char postman_root_cert_pem_end[]   asm("_binary_postman_root_cert_pem_end");

char* print_time() {
    static char strftime_buf[64];  

    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    // Add 1 hour
    timeinfo.tm_hour += 1;

    // Normalize time structure after modification
    mktime(&timeinfo);

    // Format: YYYY-MM-DD HH:MM:SS
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d_%H:%M:%S", &timeinfo);
    return strftime_buf;
}

void initialize_sntp() {
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}

void wait_for_time_sync() {
    time_t now = 0;
    struct tm timeinfo = { 0 };
    const int retry_count = 10;
    int retries = 0;

    while (timeinfo.tm_year < (2020 - 1900) && ++retries < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d)", retries);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    http_buffer_ctx_t *ctx = (http_buffer_ctx_t *)evt->user_data;

    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);

            if (!esp_http_client_is_chunked_response(evt->client)) {
                int copy_len = MIN(evt->data_len, ctx->max_length - ctx->length);
                if (copy_len > 0) {
                    memcpy(ctx->buffer + ctx->length, evt->data, copy_len);
                    ctx->length += copy_len;
                }
            }
            break;

        case HTTP_EVENT_ON_FINISH:
            ctx->buffer[ctx->length] = '\0'; // Null-terminate
            ESP_LOGI(TAG, "Response: %s", ctx->buffer);
            break;

        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;

        default:
            break;
    }

    return ESP_OK;
}


#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
static void https_with_url(void)
{
    esp_http_client_config_t config = {
        .url = "https://windturbinemonitor-b1b52-default-rtdb.europe-west1.firebasedatabase.app/Data/sensor_data.json?auth=t3kgf6oyJS006l10Hrn81t2HG7ELUcsOSLGFpBun",
        .event_handler = _http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %"PRId64,
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}


void send_sensor_data(int ina_out, float power, float voltage, float current){

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "power", power);
    cJSON_AddNumberToObject(root, "voltage", voltage);
    cJSON_AddNumberToObject(root, "current", current);

    char *post_data = cJSON_PrintUnformatted(root);
    cJSON_Delete(root); 

    char url[256];
    char* time_str = print_time();
    const char* base_path = (ina_out == 1) ? "powerOut" : "powerIn";
    snprintf(url, sizeof(url), 
             "https://windturbinemonitor-b1b52-default-rtdb.europe-west1.firebasedatabase.app/Data/sensor_data/%s/%s.json?auth=t3kgf6oyJS006l10Hrn81t2HG7ELUcsOSLGFpBun", 
             base_path, time_str);


    http_buffer_ctx_t *ctx = malloc(sizeof(http_buffer_ctx_t));
    if (!ctx) {
        ESP_LOGE(TAG, "Failed to allocate memory for ctx");
        return;
    }

    ctx->buffer = malloc(MAX_HTTP_OUTPUT_BUFFER);
    ctx->length = 0;
    ctx->max_length = MAX_HTTP_OUTPUT_BUFFER - 1;

    if (!ctx->buffer) {
        ESP_LOGE(TAG, "Failed to allocate memory for response buffer");
        free(ctx);
        return;
    }


    esp_http_client_config_t config = {
        .url = url,
        .event_handler = _http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .user_data = ctx
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);


    esp_http_client_set_method(client, HTTP_METHOD_PUT);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %" PRId64,
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "Error performing HTTP request: %s", esp_err_to_name(err));
    }


    esp_http_client_cleanup(client);
    free(post_data);
    free(ctx->buffer);
    free(ctx);
}
void send_pic_data(uint8_t buf[32]){

    char url[256];
    char* time_str = print_time();
    snprintf(url, sizeof(url), 
             "https://windturbinemonitor-b1b52-default-rtdb.europe-west1.firebasedatabase.app/Data/lamp/%s.json?auth=t3kgf6oyJS006l10Hrn81t2HG7ELUcsOSLGFpBun", 
             time_str);

    uint8_t lamp_state = buf[0] - '0';  // '1'→1, '0'→0, etc

    char json_buf[16];
    int len = snprintf(json_buf, sizeof(json_buf), "\"%u\"", lamp_state);
    if (len < 0 || len >= sizeof(json_buf)) {
         ESP_LOGE(TAG, "json_buf overflow");
            return;
        }
    

    http_buffer_ctx_t *ctx = malloc(sizeof(http_buffer_ctx_t));
    if (!ctx) {
        ESP_LOGE(TAG, "Failed to allocate memory for ctx");
        return;
    }

    ctx->buffer = malloc(MAX_HTTP_OUTPUT_BUFFER);
    ctx->length = 0;
    ctx->max_length = MAX_HTTP_OUTPUT_BUFFER - 1;

    if (!ctx->buffer) {
        ESP_LOGE(TAG, "Failed to allocate memory for response buffer");
        free(ctx);
        return;
    }


    esp_http_client_config_t config = {
        .url = url,
        .event_handler = _http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .user_data = ctx
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);


    esp_http_client_set_method(client, HTTP_METHOD_PUT);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_buf, strlen(json_buf));

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %" PRId64,
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "Error performing HTTP request: %s", esp_err_to_name(err));
    }


    esp_http_client_cleanup(client);
    free(ctx->buffer);
    free(ctx);

}
#endif 



void http_test_task(void *pvParameters)
{

#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
    ESP_LOGI(TAG, "Start https");
    sensor_data_params_t *params = (sensor_data_params_t *)pvParameters;
    send_sensor_data(params->ina_out, params->power, params->voltage, params->current);
    free(pvParameters);
#endif

    ESP_LOGI(TAG, "Finish http example");
    #if !CONFIG_IDF_TARGET_LINUX
    vTaskDelete(NULL);
#endif
}

