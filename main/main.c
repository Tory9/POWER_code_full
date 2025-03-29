#include "https_client.h"
#include "mppt.h"
#include "ina219_my.h"
#include <driver/gpio.h>
#include <driver/ledc.h>


const static char *TAG = "INA219_example";

void pwm(){
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 312500,
        .clk_cfg = LEDC_AUTO_CLK
    };

    ledc_timer_config(&timer);
    ledc_channel_config_t channel = {
        .gpio_num = 12,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0
    };

    ledc_channel_config(&channel);
    ledc_fade_func_install(0);
}



void task(void *pvParameters) {
    ina219_t dev_out = ina_start(I2C_ADDR_OUT);
    ina219_t dev_in = ina_start(I2C_ADDR_IN);

    int counter = 0;  // Counter for controlling print rate

    ESP_LOGI(TAG, "Starting the loop");
    pwm();

    while (1) {
        power_data data_out = get_power_data(&dev_out);
        power_data data_in = get_power_data(&dev_in);

        int DUTY_CYCLE = mppt(data_out.power, data_out.bus_voltage);

        if (counter % 8 == 0) {  // Print only every 200ms (8 * 25ms)
            printf("OUTPUT___ VBUS: %.04f V, VSHUNT: %.04f mV, IBUS: %.04f mA, PBUS: %.04f mW, DUTY: %d\n",
                data_out.bus_voltage, data_out.shunt_voltage, data_out.current, data_out.power, DUTY_CYCLE);
            
            printf("INPUT ___ VBUS: %.04f V, VSHUNT: %.04f mV, IBUS: %.04f mA, PBUS: %.04f mW, DUTY: %d\n",
                data_in.bus_voltage, data_in.shunt_voltage, data_in.current, data_in.power, DUTY_CYCLE);
            
        }

        if (counter % 2000 == 0){
            sensor_data_params_t *params_out = malloc(sizeof(sensor_data_params_t));
            if (params_out == NULL) {
                ESP_LOGE(TAG, "Failed to allocate memory for params_out");
                continue;
            }
            params_out->ina_out = 1;
            params_out->power = data_out.power;
            params_out->voltage = data_out.bus_voltage;


            xTaskCreate(&http_test_task, "http_test_task", 8192, (void *)params_out, 5, NULL);


            sensor_data_params_t *params_in = malloc(sizeof(sensor_data_params_t));
            if (params_in == NULL) {
                ESP_LOGE(TAG, "Failed to allocate memory for params_in");
                continue;
            }
            params_in->ina_out = 0;
            params_in->power = data_in.power;
            params_in->voltage = data_in.bus_voltage;
            counter = 0;
            

            xTaskCreate(&http_test_task, "http_test_task", 8192, (void *)params_in, 5, NULL);
        }

        counter++;
        vTaskDelay(pdMS_TO_TICKS(25));  // Sample every 25ms (40Hz)
    }
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(example_connect());
    ESP_LOGI(TAG, "Connected to AP, begin http example");

    initialize_sntp();
    wait_for_time_sync();

    ESP_ERROR_CHECK(i2cdev_init()); 

    xTaskCreate(task, "test", configMINIMAL_STACK_SIZE * 8, NULL, 5, NULL);
}