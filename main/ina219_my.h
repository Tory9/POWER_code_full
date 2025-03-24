#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>
#include <esp_log.h>
#include <assert.h>
#include <math.h>
#include <lib8tion.h>
#include <esp_idf_lib_helpers.h>
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_err.h>
#include <ina219.h>
#include "esp_mac.h"

#define I2C_PORT 0
#define I2C_ADDR 0x40
#define CONFIG_EXAMPLE_SHUNT_RESISTOR_MILLI_OHM 100

#define CONFIG_EXAMPLE_I2C_MASTER_SDA 5
#define CONFIG_EXAMPLE_I2C_MASTER_SCL 4

typedef struct power_data_t {
    float bus_voltage;
    float shunt_voltage;
    float current;
    float power;
} power_data;


ina219_t ina_start();
power_data get_power_data(ina219_t *dev);
