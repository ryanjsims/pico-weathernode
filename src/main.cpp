#include <stdio.h>
#include <pico/stdlib.h>
#include <pico/binary_info.h>
#include <pico/cyw43_arch.h>

#include <optional>

#include "aht20.h"
#include "logger.h"
#include "loop_packet.h"
#include "wifi_utils.h"

#include "sio_client.h"

int main() {
    bi_decl(bi_2pins_with_func(PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C));
    stdio_init_all();
    sleep_ms(1000);
    if(cyw43_arch_init_with_country(CYW43_COUNTRY_USA)) {
        error1("Wi-Fi init failed\n");
        return -1;
    }

    cyw43_arch_enable_sta_mode();
    info("Connecting to WiFi SSID %s...\n", WIFI_SSID);

    netif_set_status_callback(netif_default, netif_status_callback);

    int link_status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);

    sio_client client(WEEWX_URL, {});

    client.on_open([&client](){
        info1("User open callback\n");
        client.connect();
    });

    aht20 sensor(i2c_default, 100 * 1000, PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN);
    aht20::status rc;
    while(true) {
        link_status = check_network_connection(WIFI_SSID, WIFI_PASSWORD);
        if(link_status == CYW43_LINK_UP && client.state() == sio_client::client_state::disconnected) {
            client.open();
        }
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
            percentage_t humidity = sensor.humidity();
            fahrenheit_t temperature = sensor.temperature_f();
            info("%.2f%%RH %.2f°F\n", humidity, temperature);
            if(client.socket()->connected()) {
                packet_args args;
                args.outTemp = sensor.temperature();
                args.outHumidity = humidity;
                client.socket()->emit("weather_event", create_packet(args));
            }
            sleep_ms(1000);
        } else {
            debug("sensor.has_data() == %d\tsensor.busy() == %d\n", sensor.has_data(), sensor.busy());
            sleep_ms(80);
        }
    }
    return 0;
}