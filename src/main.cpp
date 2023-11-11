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

#define INDOOR_I2C_SDA_PIN 2
#define INDOOR_I2C_SCL_PIN 3

int main() {
    bi_decl(bi_2pins_with_func(PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C));
    bi_decl(bi_2pins_with_func(INDOOR_I2C_SDA_PIN, INDOOR_I2C_SCL_PIN, GPIO_FUNC_I2C));
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

    aht20 outdoor_sensor(i2c_default, 100 * 1000, PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN);
    aht20 indoor_sensor(&i2c1_inst, 100 * 1000, INDOOR_I2C_SDA_PIN, INDOOR_I2C_SCL_PIN);
    aht20::status rc;
    int reconnection_count = -1;
    while(true) {
        link_status = check_network_connection(WIFI_SSID, WIFI_PASSWORD);
        if(link_status == CYW43_LINK_UP && client.state() == sio_client::client_state::disconnected) {
            if(reconnection_count < 0) {
                client.open();
            } else {
                info("Reconnecting client (%d previous reconnect(s))\n", reconnection_count);
                client.reconnect();
            }
            reconnection_count++;
        }
        debug1("Starting measurements...\n");
        rc = outdoor_sensor.measure();
        if(rc == aht20::status::ERR_FAIL) {
            error1("Failed to read from outdoor sensor!\n");
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        } else {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        }
        rc = indoor_sensor.measure();
        if(rc == aht20::status::ERR_FAIL) {
            error1("Failed to read from indoor sensor!\n");
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        } else {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        }
        outdoor_sensor.update_status();
        indoor_sensor.update_status();
        packet_args args;
        if(outdoor_sensor.has_data()) {
            args.outTemp = outdoor_sensor.temperature();
            args.outHumidity = outdoor_sensor.humidity();
            info("Outdoors: %.2f%%RH %.2f°F\n", outdoor_sensor.humidity(), outdoor_sensor.temperature_f());
        }
        if(indoor_sensor.has_data()) {
            args.inTemp = indoor_sensor.temperature();
            args.inHumidity = indoor_sensor.humidity();
            info("Indoors:  %.2f%%RH %.2f°F\n", indoor_sensor.humidity(), indoor_sensor.temperature_f());
        }

        if(client.socket()->connected() && (args.outTemp || args.inTemp)) {
            client.socket()->emit("weather_event", create_packet(args));
        }
        sleep_ms(2500);

    }
    return 0;
}