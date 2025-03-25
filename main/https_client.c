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
    static char *output_buffer;  
    static int output_len;       
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
         
            if (output_len == 0 && evt->user_data) {
                memset(evt->user_data, 0, MAX_HTTP_OUTPUT_BUFFER);
            }
            /*
             *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
             *  However, event handler can also be used in case chunked encoding is used.
             */
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // If user_data buffer is configured, copy the response into the buffer
                int copy_len = 0;
                if (evt->user_data) {
                    // The last byte in evt->user_data is kept for the NULL character in case of out-of-bound access.
                    copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
                    if (copy_len) {
                        memcpy(evt->user_data + output_len, evt->data, copy_len);
                    }
                } else {
                    int content_len = esp_http_client_get_content_length(evt->client);
                    if (output_buffer == NULL) {
                        // We initialize output_buffer with 0 because it is used by strlen() and similar functions therefore should be null terminated.
                        output_buffer = (char *) calloc(content_len + 1, sizeof(char));
                        output_len = 0;
                        if (output_buffer == NULL) {
                            ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                            return ESP_FAIL;
                        }
                    }
                    copy_len = MIN(evt->data_len, (content_len - output_len));
                    if (copy_len) {
                        memcpy(output_buffer + output_len, evt->data, copy_len);
                    }
                }
                output_len += copy_len;
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            if (output_buffer != NULL) {
                ESP_LOGI(TAG, "----------------------------------------");
                //ESP_LOG_BUFFER_HEX(TAG, output_buffer, output_len);
                free(output_buffer);
                output_buffer = NULL;
            } else {
                ESP_LOGW(TAG, "No response buffer allocated!");
            }
            output_len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
                ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
            esp_http_client_set_header(evt->client, "From", "user@example.com");
            esp_http_client_set_header(evt->client, "Accept", "text/html");
            esp_http_client_set_redirection(evt->client);
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


void send_sensor_data(int ina_out, float power, float voltage){

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "power", power);
    cJSON_AddNumberToObject(root, "voltage", voltage);

    // Convert JSON object to string
    char *post_data = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);  // Free cJSON object

    char url[256];
    char* time_str = print_time();
    const char* base_path = (ina_out == 1) ? "powerOut" : "powerIn";
    snprintf(url, sizeof(url), 
             "https://windturbinemonitor-b1b52-default-rtdb.europe-west1.firebasedatabase.app/Data/sensor_data/%s/%s.json?auth=t3kgf6oyJS006l10Hrn81t2HG7ELUcsOSLGFpBun", 
             base_path, time_str);

    // Configure HTTP client
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = _http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Set HTTP method to PUT (or POST if you want to append data)
    esp_http_client_set_method(client, HTTP_METHOD_PUT);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    // Perform the request
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %" PRId64,
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "Error performing HTTP request: %s", esp_err_to_name(err));
    }

    // Cleanup
    esp_http_client_cleanup(client);
    free(post_data);  // Free allocated JSON string
}
#endif 



void http_test_task(void *pvParameters)
{

#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
    ESP_LOGI(TAG, "Start https");
    //GET test
    //https_with_url();

    //POST data
    sensor_data_params_t *params = (sensor_data_params_t *)pvParameters;
    send_sensor_data(params->ina_out, params->power, params->voltage);
    free(pvParameters);
#endif

    ESP_LOGI(TAG, "Finish http example");
    #if !CONFIG_IDF_TARGET_LINUX
    vTaskDelete(NULL);
#endif
}

