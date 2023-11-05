#pragma once

#include <stdint.h>
#include <hardware/i2c.h>
#include <hardware/gpio.h>

typedef float celsius_t;
typedef float fahrenheit_t;
typedef float percentage_t;

class aht20 {
public:
    enum class status {
        ERR_OK,
        ERR_FAIL,
        ERR_BUSY
    };

    aht20(i2c_inst_t *instance, uint32_t baud, uint8_t sda_pin, uint8_t scl_pin);

    status init();
    status update_status();
    status measure();
    status reset();

    bool calibrated() const;
    bool busy() const;
    bool has_data() const;

    percentage_t humidity() const;
    celsius_t temperature() const;
    fahrenheit_t temperature_f() const;

    uint32_t raw_humidity() const;
    uint32_t raw_temperature() const;
private:
    i2c_inst_t *m_i2c;
    uint8_t m_rbuffer[7];
    uint8_t m_wbuffer[3];
    uint8_t m_wlen;
    uint32_t m_baud;
    alarm_id_t m_alarm;
    absolute_time_t m_busy_until;

    int read(uint8_t);
    int write(uint8_t);
    static int64_t retrieve_measurement_callback(alarm_id_t, void*);
    static int64_t scheduled_write_callback(alarm_id_t, void*);
};