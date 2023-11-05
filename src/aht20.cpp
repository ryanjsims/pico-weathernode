#include "aht20.h"

#include <pico/time.h>
#include <string.h>

#include <stdio.h>
#include "logger.h"
#include "crc8.h"

#define AHT20_I2C_ADDR      0x38
#define AHT20_I2C_STATUS    0x71
#define AHT20_I2C_INIT      0xBE
#define AHT20_I2C_INIT1     0x08
#define AHT20_I2C_INIT2     0x00
#define AHT20_I2C_MEASURE   0xAC
#define AHT20_I2C_MEASURE1  0x33
#define AHT20_I2C_MEASURE2  0x00
#define AHT20_I2C_RESET     0xBA
#define AHT20_CAL_BIT       (1 << 3)
#define AHT20_BUSY_BIT      (1 << 7)

int64_t aht20::retrieve_measurement_callback(alarm_id_t alarm, void* user_data) {
    trace1("aht20::retrieve_measurement_callback entered\n");
    aht20* sensor = (aht20*)user_data;
    // Read the status byte
    sensor->read(1);
    if(sensor->busy()) {
        // Measurement not complete, continue waiting for 5ms before trying again
        return -5000;
    }
    // Read the entire measurement and clear the alarm if the CRC check passes
    sensor->read(7);
    uint8_t crc = crc8(sensor->m_rbuffer, 6);
    if(crc != sensor->m_rbuffer[6]) {
        warn("CRC check failed:\n    Provided   %02x\n    Calculated %02x\n", sensor->m_rbuffer[6], crc);
        return -1000;
    }

    sensor->m_alarm = 0;
    return 0;
}

int64_t aht20::scheduled_write_callback(alarm_id_t alarm, void* user_data) {
    trace1("aht20::scheduled_write_callback entered\n");
    aht20* sensor = (aht20*)user_data;
    sensor->write(sensor->m_wlen);
    sensor->m_alarm = 0;
    return 0;
}

aht20::aht20(i2c_inst_t *instance, uint32_t baud, uint8_t sda_pin, uint8_t scl_pin)
    : m_i2c(instance)
    , m_wbuffer{0}
    , m_rbuffer{0}
    , m_wlen(0)
    , m_alarm(0)
    , m_busy_until(nil_time)
{
    m_baud = i2c_init(m_i2c, baud);
    gpio_set_function(sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(scl_pin, GPIO_FUNC_I2C);
    gpio_pull_up(sda_pin);
    gpio_pull_up(scl_pin);
    update_us_since_boot(&m_busy_until, 40000);
}

aht20::status aht20::init() {
    trace1("aht20::init entered\n");
    m_wbuffer[0] = AHT20_I2C_INIT;
    m_wbuffer[1] = AHT20_I2C_INIT1;
    m_wbuffer[2] = AHT20_I2C_INIT2;
    if(!time_reached(m_busy_until)) {
        m_wlen = 3;
        m_alarm = add_alarm_at(m_busy_until, aht20::scheduled_write_callback, this, true);
        m_busy_until = delayed_by_ms(m_busy_until, 10);
        trace1("aht20::init exited ERR_OK\n");
        return aht20::status::ERR_OK;
    }
    int rc = write(3);
    if(PICO_ERROR_GENERIC != rc) {
        m_busy_until = make_timeout_time_ms(10);
        trace1("aht20::init exited ERR_OK\n");
        return aht20::status::ERR_OK;
    }
    trace1("aht20::init exited ERR_FAIL\n");
    return aht20::status::ERR_FAIL;
}

aht20::status aht20::update_status() {
    trace1("aht20::update_status entered\n");
    if(!time_reached(m_busy_until)) {
        trace1("aht20::update_status exited ERR_BUSY\n");
        return aht20::status::ERR_BUSY;
    }
    int rc = 0;
    if(m_alarm <= 0) {
        // If no measurement is currently in progress, write the status command
        // Otherwise, just read the status byte since the measure command exposes that
        m_wbuffer[0] = AHT20_I2C_STATUS;
        rc = write(1);
    }
    if(rc != PICO_ERROR_GENERIC) {
        rc = read(1);
        trace("Read status 0x%02x\n", m_rbuffer[0]);
    }
    trace("aht20::update_status exited %s\n", rc != PICO_ERROR_GENERIC ? "ERR_OK" : "ERR_FAIL");
    return rc != PICO_ERROR_GENERIC ? aht20::status::ERR_OK : aht20::status::ERR_FAIL;
}

aht20::status aht20::measure() {
    trace1("aht20::measure entered\n");
    if(!time_reached(m_busy_until) || m_alarm > 0) {
        trace1("aht20::measure exited ERR_BUSY\n");
        return aht20::status::ERR_BUSY;
    }
    if(update_status() != aht20::status::ERR_OK) {
        trace1("aht20::measure exited ERR_FAIL\n");
        return aht20::status::ERR_FAIL;
    }
    if(!calibrated()) {
        bool status = init() == aht20::status::ERR_OK;
        trace("aht20::measure exited %s\n", status ? "ERR_BUSY" : "ERR_FAIL");
        return status ? aht20::status::ERR_BUSY : aht20::status::ERR_FAIL;
    }
    m_wbuffer[0] = AHT20_I2C_MEASURE;
    m_wbuffer[1] = AHT20_I2C_MEASURE1;
    m_wbuffer[2] = AHT20_I2C_MEASURE2;
    int rc = write(3);
    if(rc != PICO_ERROR_GENERIC) {
        m_alarm = add_alarm_in_ms(80, aht20::retrieve_measurement_callback, this, false);
    }
    bool status = rc != PICO_ERROR_GENERIC && m_alarm > 0;
    trace("aht20::measure exited %s\n", status ? "ERR_OK" : "ERR_FAIL");
    return status ? aht20::status::ERR_OK : aht20::status::ERR_FAIL;
}

aht20::status aht20::reset() {
    trace1("aht20::reset entered\n");
    if(!time_reached(m_busy_until)) {
        return aht20::status::ERR_BUSY;
    }
    if(m_alarm > 0) {
        // Cancel any current measurement alarm
        cancel_alarm(m_alarm);
        m_alarm = 0;
    }
    m_wbuffer[0] = AHT20_I2C_RESET;
    int rc = write(1);
    if(rc != PICO_ERROR_GENERIC) {
        memset(m_rbuffer, 0, sizeof(m_rbuffer));
        m_busy_until = make_timeout_time_ms(20);
    }
    trace("aht20::reset exited %s\n", rc != PICO_ERROR_GENERIC ? "ERR_OK" : "ERR_FAIL");
    return rc != PICO_ERROR_GENERIC ? aht20::status::ERR_OK : aht20::status::ERR_FAIL;
}

bool aht20::calibrated() const {
    trace1("aht20::calibrated\n");
    return (m_rbuffer[0] & AHT20_CAL_BIT) != 0;
}

bool aht20::busy() const {
    trace1("aht20::busy\n");
    return (m_rbuffer[0] & AHT20_BUSY_BIT) != 0;
}

bool aht20::has_data() const {
    trace1("aht20::has_data\n");
    return raw_humidity() || raw_temperature();
}

percentage_t aht20::humidity() const {
    trace1("aht20::humidity\n");
    return (float)raw_humidity() / (1 << 20) * 100.0f;
}

celsius_t aht20::temperature() const {
    trace1("aht20::temperature\n");
    return (float)raw_temperature() / (1 << 20) * 200.0f - 50.0f;
}

fahrenheit_t aht20::temperature_f() const {
    trace1("aht20::temperature_f\n");
    return temperature() * 1.8f + 32;
}

uint32_t aht20::raw_humidity() const {
    trace1("aht20::raw_humidity\n");
    return m_rbuffer[1] << 12 | m_rbuffer[2] << 4 | m_rbuffer[3] >> 4;
}

uint32_t aht20::raw_temperature() const {
    trace1("aht20::raw_temperature\n");
    return (m_rbuffer[3] & 0x0F) << 16 | m_rbuffer[4] << 8 | m_rbuffer[5];
}

int aht20::read(uint8_t count) {
    trace("aht20::read count %d m_i2c %p\n", count, m_i2c);
    if(count > sizeof(m_rbuffer)) {
        return PICO_ERROR_GENERIC;
    }
    int rc = i2c_read_blocking(m_i2c, AHT20_I2C_ADDR, m_rbuffer, count, false);
#if LOG_LEVEL <= LOG_LEVEL_TRACE
    trace1("Read data:\n");
    for(int i = 0; i < rc; i++) {
        trace_cont("0x%02x ", m_rbuffer[i]);
    }
    trace_cont1("\n");
#endif
    return rc;
}

int aht20::write(uint8_t count) {
    trace("aht20::write count %d m_i2c %p\n", count, m_i2c);
    if(count > sizeof(m_wbuffer)) {
        return PICO_ERROR_GENERIC;
    }
    int rc = i2c_write_blocking(m_i2c, AHT20_I2C_ADDR, m_wbuffer, count, false);
#if LOG_LEVEL <= LOG_LEVEL_TRACE
    trace1("Wrote data:\n");
    for(int i = 0; i < rc; i++) {
        trace_cont("0x%02x ", m_wbuffer[i]);
    }
    trace_cont1("\n");
#endif
    return rc;
}