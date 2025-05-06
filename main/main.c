#include "https_client.h"
#include "mppt.h"
#include "ina219_my.h"
#include <driver/gpio.h>
#include <driver/ledc.h>
#include "interrupts.h"
#include "mirf.h"


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

    ESP_LOGI(TAG, "Starting the loop");
    pwm();
    init_interrupts();

    while (1) {

        if (mppt_flag){
            mppt_flag = false;
            power_data data_out = get_power_data(&dev_out);
            power_data data_in = get_power_data(&dev_in);

            // int DUTY_CYCLE = mppt(data_out.power, data_out.bus_voltage);
            int DUTY_CYCLE = mppt(data_in.power, data_in.bus_voltage);
            if (print_flag) {
                print_flag = false;

                printf("OUTPUT___ VBUS: %.04f V, VSHUNT: %.04f mV, IBUS: %.04f mA, PBUS: %.04f mW, DUTY: %d\n",
                    data_out.bus_voltage, data_out.shunt_voltage, data_out.current, data_out.power, DUTY_CYCLE);

                printf("INPUT ___ VBUS: %.04f V, VSHUNT: %.04f mV, IBUS: %.04f mA, PBUS: %.04f mW, DUTY: %d\n",
                    data_in.bus_voltage, data_in.shunt_voltage, data_in.current, data_in.power, DUTY_CYCLE);
            }


            if (http_flag) {
                http_flag = false;

                sensor_data_params_t *params_out = malloc(sizeof(sensor_data_params_t));
                if (params_out == NULL) {
                    ESP_LOGE(TAG, "Failed to allocate memory for params_out");
                    continue;
                }
                params_out->ina_out = 1;
                params_out->power = data_out.power;
                params_out->voltage = data_out.bus_voltage;
                params_out->current = data_out.current;


                xTaskCreate(&http_test_task, "http_test_task", 8192, (void *)params_out, 2, NULL);


                sensor_data_params_t *params_in = malloc(sizeof(sensor_data_params_t));
                if (params_in == NULL) {
                    ESP_LOGE(TAG, "Failed to allocate memory for params_in");
                    continue;
                }
                params_in->ina_out = 0;
                params_in->power = data_in.power;
                params_in->voltage = data_in.bus_voltage;
                params_in->current = data_in.current;
                

                xTaskCreatePinnedToCore(&http_test_task, "http_test_task", 8192, (void *)params_in, 5, NULL, 1);

            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));   // Prevent task from hogging the CPU and triggering watchdog

    }
}

    
    static void duty_sweep_task(void *pv)
    {
        ina219_t dev_out = ina_start(I2C_ADDR_OUT);
        ina219_t dev_in  = ina_start(I2C_ADDR_IN);
    
        pwm();
        init_interrupts();
    
        const int DUTY_MIN   = 30;
        const int DUTY_MAX   = 230;
        const int N_SAMPLES  = 1;
        const int SETTLE_MS  = 40;
        const int BETWEEN_MS = 20;
    
        ESP_LOGI(TAG,
                 "===== STARTING DUTY SWEEP  (%d → %d, %d samples/step) =====",
                 DUTY_MIN, DUTY_MAX, N_SAMPLES);
    
        for (int d = DUTY_MIN; d <= DUTY_MAX; ++d) {
    
            /* --- apply duty ------------------------------------------------ */
            ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE,
                                     LEDC_CHANNEL_0,
                                     d, 0);
    
            vTaskDelay(pdMS_TO_TICKS(SETTLE_MS));
    
    
            /* --- take N_SAMPLES readings ----------------------------------- */
            for (int k = 0; k < N_SAMPLES; ++k) {
                power_data in  = get_power_data(&dev_in);
                power_data out = get_power_data(&dev_out);
    
                // printf("[D=%3d]  "
                //     "Vin=%6.3f V  Iin=%6.1f mA  Pin=%7.1f mW  |  "
                //     "Vout=%6.3f V Iout=%6.1f mA Pout=%7.1f mW\n",
                //     d,
                //     in.bus_voltage,   in.current,   in.power,
                //     out.bus_voltage,  out.current,  out.power);
        
                printf("%3d,"
                        "%6.3f, %6.1f, %7.1f, "
                        "%6.3f, %6.1f, %7.1f \n",
                        d,
                        in.bus_voltage,   in.current,   in.power,
                        out.bus_voltage,  out.current,  out.power);
    
                vTaskDelay(pdMS_TO_TICKS(BETWEEN_MS));
            }
    
                         // exits this task gracefully
    }
    ESP_LOGI(TAG, "===== SWEEP COMPLETE — deleting task =====");
        vTaskDelete(NULL);   
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

    xTaskCreatePinnedToCore(task, "test", configMINIMAL_STACK_SIZE * 8, NULL, 5, NULL, 1);
    // xTaskCreate(duty_sweep_task,
    //     "duty_sweep",
    //     4096,            // stack
    //     NULL,
    //     5,               // prio
    //     NULL);

    #if CONFIG_RECEIVER
        ESP_LOGI(TAG, "Starting nRF24L01 receiver only");
        xTaskCreatePinnedToCore(&receiver, "RECEIVER", 1024*15, NULL, 8, NULL, 0);
    #else
        ESP_LOGW(TAG, "CONFIG_RECEIVER is not set — nothing to do!");
    #endif
}
