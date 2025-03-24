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

#define DUTY_STEP 1

int mppt(double P, double V);