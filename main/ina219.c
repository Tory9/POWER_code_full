#include "ina219_my.h"
const static char *TAG = "INA219_example";

ina219_t ina_start(){
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
    return dev;

}

power_data get_power_data(ina219_t *dev) {
    power_data data;

    ESP_ERROR_CHECK(ina219_get_bus_voltage(dev, &data.bus_voltage));
    ESP_ERROR_CHECK(ina219_get_shunt_voltage(dev, &data.shunt_voltage));
    ESP_ERROR_CHECK(ina219_get_current(dev, &data.current));
    ESP_ERROR_CHECK(ina219_get_power(dev, &data.power));

    data.bus_voltage = roundf(data.bus_voltage * 1000) / 1000;
    data.power = roundf(data.power * 1000 * 1000) / 1000;
    data.current = roundf(data.current * 1000 * 1000) / 1000;
    data.shunt_voltage = roundf(data.shunt_voltage * 1000 * 1000) / 1000;

    return data;
}



