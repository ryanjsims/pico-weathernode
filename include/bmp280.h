#pragma once

#include <stdint.h>
#include <hardware/i2c.h>
#include <hardware/gpio.h>

#include <span>

#include "units.h"

class bmp280 {
public:
    enum class status {
        ERR_OK,
        ERR_FAIL,
        ERR_BUSY
    };
    enum class precision : uint8_t {
        // Sampling turned off, output set to 0x80000
        OFF = 0b000,
        // Sampling on, 16 bit resolution
        X1  = 0b001,
        // Sampling on, 17 bit resolution
        X2  = 0b010,
        // Sampling on, 18 bit resolution
        X4  = 0b011,
        // Sampling on, 19 bit resolution
        X8  = 0b100,
        // Sampling on, 20 bit resolution
        X16 = 0b101,
    };
    enum class filter : uint8_t {
        // Filtering turned off
        OFF = 0b000,
        // Filtering on
        X2  = 0b001,
        X4  = 0b010,
        X8  = 0b011,
        X16  = 0b100,
    };
    enum class standby : uint8_t {
        half_ms     = 0b000,
        sixteenth_s = 0b001,
        eighth_s    = 0b010,
        quarter_s   = 0b011,
        half_s      = 0b100,
        one_s       = 0b101,
        two_s       = 0b110,
        four_s      = 0b111,
    };
    enum class mode : uint8_t {
        sleep  = 0b00,
        forced = 0b01,
        normal = 0b11
    };
    bmp280(bool default_addr = true, i2c_inst_t *instance = PICO_DEFAULT_I2C_INSTANCE, uint32_t baud = 100000, uint8_t sda_pin = PICO_DEFAULT_I2C_SDA_PIN, uint8_t scl_pin = PICO_DEFAULT_I2C_SCL_PIN);

    void init();
    status measure();
    bool busy();
    bool has_data() const;
    uint8_t chip_id() const;

    celsius_t temperature();
    mbar_t pressure();

    // Default sampling: temperature x2, pressure x16
    void set_oversampling(precision temperature, precision pressure);
    void set_filtering(filter coeff);
    void set_standby(standby time);
    void set_mode(mode new_mode);

private:
    i2c_inst_t *m_i2c;
    uint8_t m_addr, m_id;
    alarm_id_t m_alarm;
    uint8_t m_trim_params[26];
    int32_t m_tfine, m_temperature;
    uint32_t m_pressure, m_raw_temperature, m_raw_pressure;
    bool m_needs_conversion, m_has_data;
    mode m_mode;
    standby m_standby;

    int read(uint8_t addr, std::span<uint8_t> buffer);
    int write(uint8_t addr, std::span<uint8_t> data);

    int32_t calc_temp();
    uint32_t calc_pressure();
    void convert();
    void read_raw_data();
    static int64_t retrieve_measurement_callback(alarm_id_t, void*);
};