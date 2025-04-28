#include "interrupts.h"

volatile bool print_flag = false;
volatile bool http_flag = false;
volatile bool mppt_flag = false;

void print_timer_callback(void* arg) {
    print_flag = true;
}

void http_timer_callback(void* arg) {
    http_flag = true;
}

void mppt_timer_callback(void* arg) {
    mppt_flag = true;
}

const esp_timer_create_args_t print_timer_args = {
    .callback = print_timer_callback,
    .arg = NULL,
    .dispatch_method = ESP_TIMER_TASK, 
    .name = "print_timer"
};

const esp_timer_create_args_t http_timer_args = {
    .callback = http_timer_callback,
    .arg = NULL,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "http_timer"
};

const esp_timer_create_args_t mppt_timer_args = {
    .callback = mppt_timer_callback,
    .arg = NULL,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "mppt_timer"
};


esp_timer_handle_t print_timer;
esp_timer_handle_t http_timer;
esp_timer_handle_t mppt_timer;


void init_interrupts(void) {
    ESP_ERROR_CHECK(esp_timer_create(&print_timer_args, &print_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(print_timer, 200 * 1000)); 

    ESP_ERROR_CHECK(esp_timer_create(&http_timer_args, &http_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(http_timer, 50 * 1000 * 1000));

    ESP_ERROR_CHECK(esp_timer_create(&mppt_timer_args, &mppt_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(mppt_timer, 200 * 1000));
}
