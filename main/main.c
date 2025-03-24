#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ina219.h>
#include <string.h>
#include <esp_log.h>
#include <assert.h>
#include <math.h>
#include <lib8tion.h>
#include <esp_idf_lib_helpers.h>
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_err.h>

#include "esp_http_client_example.h"


#define I2C_PORT 0
#define I2C_ADDR 0x40
#define CONFIG_EXAMPLE_SHUNT_RESISTOR_MILLI_OHM 100
#define DUTY_STEP 1
#define CONFIG_EXAMPLE_I2C_MASTER_SDA 5
#define CONFIG_EXAMPLE_I2C_MASTER_SCL 4

int DUTY_CYCLE = 127;
double P_prev = 1;
double V_prev = 1;


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

void mppt(double P, double V){


    if (P > P_prev) {
        //printf("power UP\n");
        if (V > V_prev) {
            DUTY_CYCLE+= DUTY_STEP;
            //printf("volatge UP\n");
        } else if(V < V_prev){
            DUTY_CYCLE-= DUTY_STEP;
            //printf("volatage DOWN\n");
        }
    } else {
        //printf("power DOWN\n");
        if (V > V_prev) {
            DUTY_CYCLE-= DUTY_STEP;
            //printf("volatge UP\n");
        } else if(V < V_prev){
            DUTY_CYCLE+= DUTY_STEP;
            //printf("volatge DOWN\n");
        }
    }

    // Ensure duty cycle remains in valid range
    if (V >= 14) DUTY_CYCLE = DUTY_CYCLE - 5;
    if (DUTY_CYCLE < 30) DUTY_CYCLE = 30;
    if (DUTY_CYCLE > 230) DUTY_CYCLE = 230;

    ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, DUTY_CYCLE, 0);

    V_prev = V;
    P_prev = P;
    //printf("DUTYYYYY CYCLE: %d , PBUS: %.04f mW \n",  DUTY_CYCLE , P_prev);

}

void task(void *pvParameters) {
    ina219_t dev;
    memset(&dev, 0, sizeof(ina219_t));

    assert(CONFIG_EXAMPLE_SHUNT_RESISTOR_MILLI_OHM > 0);
    ESP_ERROR_CHECK(ina219_init_desc(&dev, I2C_ADDR, I2C_PORT, CONFIG_EXAMPLE_I2C_MASTER_SDA, CONFIG_EXAMPLE_I2C_MASTER_SCL));
    ESP_LOGI(TAG, "Initializing INA219");
    ESP_ERROR_CHECK(ina219_init(&dev));

    ESP_LOGI(TAG, "Configuring INA219");
    ESP_ERROR_CHECK(ina219_configure(&dev, INA219_BUS_RANGE_16V, INA219_GAIN_0_125,
            INA219_RES_12BIT_1S, INA219_RES_12BIT_1S, INA219_MODE_CONT_SHUNT_BUS));

    ESP_LOGI(TAG, "Calibrating INA219");
    ESP_ERROR_CHECK(ina219_calibrate(&dev, (float)CONFIG_EXAMPLE_SHUNT_RESISTOR_MILLI_OHM / 1000.0f));

    float bus_voltage, shunt_voltage, current, power;
    int counter = 0;  // Counter for controlling print rate

    ESP_LOGI(TAG, "Starting the loop");
    pwm();

    while (1) {
        ESP_ERROR_CHECK(ina219_get_bus_voltage(&dev, &bus_voltage));
        ESP_ERROR_CHECK(ina219_get_shunt_voltage(&dev, &shunt_voltage));
        ESP_ERROR_CHECK(ina219_get_current(&dev, &current));
        ESP_ERROR_CHECK(ina219_get_power(&dev, &power));

        float rounded_power = roundf(power * 1000 * 1000) / 1000;
        float rounded_voltage = roundf(bus_voltage * 1000) / 1000;
        mppt(rounded_power, rounded_voltage);

        if (counter % 8 == 0) {  // Print only every 200ms (8 * 25ms)
            printf("VBUS: %.04f V, VSHUNT: %.04f mV, IBUS: %.04f mA, PBUS: %.04f mW, DUTY: %d\n",
                bus_voltage, shunt_voltage * 1000, current * 1000, power * 1000, DUTY_CYCLE);
        }

        if (counter % 1000 == 0){
            sensor_data_params_t *params = malloc(sizeof(sensor_data_params_t));
            params->ina_out = 1;
            params->power = power * 1000;
            params->voltage = bus_voltage;
            counter = 0;
            
            xTaskCreate(&http_test_task, "http_test_task", 8192, (void *)params, 5, NULL);
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