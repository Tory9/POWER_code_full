#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"


extern volatile bool print_flag;
extern volatile bool http_flag;
extern volatile bool mppt_flag;

void init_interrupts(void);
