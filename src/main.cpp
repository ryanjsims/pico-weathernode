#include <stdio.h>
#include <pico/stdlib.h>
#include <pico/binary_info.h>
#include <pico/cyw43_arch.h>

#include "aht20.h"
#include "logger.h"

int main() {
    bi_decl(bi_2pins_with_func(PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C));
    stdio_init_all();
    sleep_ms(1000);
    if(cyw43_arch_init_with_country(CYW43_COUNTRY_USA)) {
        error1("Wi-Fi init failed\n");
        return -1;
    }

    aht20 sensor(i2c_default, 100 * 1000, PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN);
    aht20::status rc;
    while(true) {
        debug1("Starting measurement...\n");
        rc = sensor.measure();
        if(rc == aht20::status::ERR_FAIL) {
            error1("Failed to write to sensor!\n");
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        } else {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        }
        sensor.update_status();
        if(sensor.has_data()) {
            info("%.2f%%RH %.2f°F\n", sensor.humidity(), sensor.temperature_f());
            sleep_ms(1000);
        } else {
            debug("sensor.has_data() == %d\tsensor.busy() == %d\n", sensor.has_data(), sensor.busy());
            sleep_ms(80);
        }
    }
    return 0;
}