#include "bmp280.h"

#include <stdio.h>
#include <logger.h>

#define BMP280_DEFAULT_ADDR 0x76
#define BMP280_ALT_ADDR     0x77
#define BMP280_CALIB_BASE   0x88
#define BMP280_CALIB_END    0xA2
#define BMP280_CHIP_ID      0x58
#define BMP280_ID_REG       0xD0
#define BMP280_RST_REG      0xE0
#define BMP280_STATUS_REG   0xF3
#define BMP280_MEASURE_REG  0xF4
#define BMP280_CONFIG_REG   0xF5
#define BMP280_PMSB_REG     0xF7
#define BMP280_PLSB_REG     0xF8
#define BMP280_PXLSB_REG    0xF9
#define BMP280_TMSB_REG     0xFA
#define BMP280_TLSB_REG     0xFB
#define BMP280_TXLSB_REG    0xFC

#define BMP280_OSRS_P_BIT   2
#define BMP280_OSRS_T_BIT   5
#define BMP280_FILTER_BIT   2
#define BMP280_STANDBY_BIT  5
#define BMP280_BUSY_BIT     3
#define BMP280_BUSY_MASK    (1 << BMP280_BUSY_BIT)

#define BMP280_T1 (((uint16_t)m_trim_params[1]) << 8 | m_trim_params[0])
#define BMP280_T2 (((int16_t)m_trim_params[3]) << 8 | m_trim_params[2])
#define BMP280_T3 (((int16_t)m_trim_params[5]) << 8 | m_trim_params[4])
#define BMP280_P1 (((uint16_t)m_trim_params[7]) << 8 | m_trim_params[6])
#define BMP280_P2 (((int16_t)m_trim_params[9]) << 8 | m_trim_params[8])
#define BMP280_P3 (((int16_t)m_trim_params[11]) << 8 | m_trim_params[10])
#define BMP280_P4 (((int16_t)m_trim_params[13]) << 8 | m_trim_params[12])
#define BMP280_P5 (((int16_t)m_trim_params[15]) << 8 | m_trim_params[14])
#define BMP280_P6 (((int16_t)m_trim_params[17]) << 8 | m_trim_params[16])
#define BMP280_P7 (((int16_t)m_trim_params[19]) << 8 | m_trim_params[18])
#define BMP280_P8 (((int16_t)m_trim_params[21]) << 8 | m_trim_params[20])
#define BMP280_P9 (((int16_t)m_trim_params[23]) << 8 | m_trim_params[22])

int64_t measurement_delay_us(bmp280::standby standby_val) {
    switch(standby_val) {
    case bmp280::standby::half_ms:
        return     -750;
    case bmp280::standby::sixteenth_s:
        return   -31250;
    case bmp280::standby::eighth_s:
        return   -62500;
    case bmp280::standby::quarter_s:
        return  -125000;
    case bmp280::standby::half_s:
        return  -250000;
    case bmp280::standby::one_s:
        return  -500000;
    case bmp280::standby::two_s:
        return -1000000;
    case bmp280::standby::four_s:
        return -2000000;
    default:
        return -750;
    }
}

int64_t bmp280::retrieve_measurement_callback(alarm_id_t alarm, void* user_data) {
    bmp280* sensor = (bmp280*)user_data;
    if(sensor->busy()) {
        return measurement_delay_us(sensor->m_standby);
    }
    sensor->read_raw_data();
    sensor->m_alarm = 0;
    if(sensor->m_mode == bmp280::mode::forced) {
        sensor->m_mode = bmp280::mode::sleep;
    }
    return 0;
}

bmp280::bmp280(bool default_addr, i2c_inst_t *instance, uint32_t baud, uint8_t sda_pin, uint8_t scl_pin)
    : m_addr(default_addr ? BMP280_DEFAULT_ADDR : BMP280_ALT_ADDR)
    , m_i2c(instance)
    , m_alarm(0)
    , m_trim_params{0}
    , m_tfine(0)
    , m_temperature(0)
    , m_pressure(0)
    , m_id(0)
    , m_has_data(false)
{
    trace1("bmp280 constructor entered...\n");
    if(gpio_get_function(sda_pin) != GPIO_FUNC_I2C) {
        trace1("setting up i2c\n");
        i2c_init(m_i2c, baud);
        gpio_set_function(sda_pin, GPIO_FUNC_I2C);
        gpio_set_function(scl_pin, GPIO_FUNC_I2C);
        gpio_pull_up(sda_pin);
        gpio_pull_up(scl_pin);
    }
    trace1("Reading id... ");
    int rc = read(BMP280_ID_REG, {&m_id, 1});
    trace_cont("rc = %d\n", rc);
    if(m_id != 0x58) {
        error("bmp280: ID mismatch - read 0x%02x, expected 0x%02x\n", m_id, BMP280_CHIP_ID);
    }
    trace1("Reading calibration data... ");
    rc = read(BMP280_CALIB_BASE, {m_trim_params, sizeof(m_trim_params)});
    trace_cont("rc = %d\n", rc);
    trace1("bmp280 constructor exited.\n");
}

void bmp280::init() {
    trace1("bmp280::init entered...\n");
    set_oversampling(bmp280::precision::X2, bmp280::precision::X16);
    set_filtering(bmp280::filter::X4);
    set_standby(bmp280::standby::sixteenth_s);
    set_mode(bmp280::mode::normal);
    trace1("bmp280::init exited.\n");
}

bmp280::status bmp280::measure() {
    trace1("bmp280::measure entered...\n");
    if(m_mode != bmp280::mode::normal && m_alarm == 0) {
        set_mode(bmp280::mode::forced);
        m_alarm = add_alarm_in_us(measurement_delay_us(m_standby), retrieve_measurement_callback, this, true);
        trace("bmp280::measure setting alarm %08x\n", m_alarm);
        return m_alarm >= 0 ? bmp280::status::ERR_BUSY : bmp280::status::ERR_FAIL;
    }
    if(m_mode != bmp280::mode::normal && m_alarm > 0) {
        trace1("bmp280::measure alarm is set.\n");
        return bmp280::status::ERR_BUSY;
    }
    read_raw_data();
    trace1("bmp280::measure exiting.\n");
    return bmp280::status::ERR_OK;
}

bool bmp280::busy() {
    trace1("bmp280::busy entered...\n");
    uint8_t status;
    read(BMP280_STATUS_REG, {&status, 1});
    bool to_return = (BMP280_BUSY_MASK & status) != 0;
    trace("bmp280::busy returning %d\n", to_return);
    return to_return;
}

bool bmp280::has_data() const {
    return m_has_data;
}

uint8_t bmp280::chip_id() const {
    return m_id;
}

celsius_t bmp280::temperature() {
    trace1("bmp280::temperature entered...\n");
    if(m_needs_conversion) {
        convert();
    }
    celsius_t to_return = (celsius_t)(m_temperature) / 100.0f;
    trace("bmp280::temperature returning %.2f\n", to_return);
    return to_return;
}

mbar_t bmp280::pressure() {
    trace1("bmp280::pressure entered...\n");
    if(m_needs_conversion) {
        convert();
    }
    mbar_t to_return = (mbar_t)(m_pressure / 256.0f) / 100.0f;
    trace("bmp280::pressure returning %.2f\n", to_return);
    m_has_data = false;
    return to_return;
}

void bmp280::set_oversampling(bmp280::precision temperature, bmp280::precision pressure) {
    trace1("bmp280::set_oversampling entered...\n");
    uint8_t measure_reg;
    read(BMP280_MEASURE_REG, {&measure_reg, 1});
    // Mask off osrs_t and osrs_p bits
    measure_reg &= 0x03;
    measure_reg |= ((uint8_t)temperature << BMP280_OSRS_T_BIT) | ((uint8_t)pressure << BMP280_OSRS_P_BIT);
    write(BMP280_MEASURE_REG, {&measure_reg, 1});
    trace1("bmp280::set_oversampling exiting.\n");
}

void bmp280::set_mode(bmp280::mode new_mode) {
    trace1("bmp280::set_mode entered...\n");
    uint8_t measure_reg;
    read(BMP280_MEASURE_REG, {&measure_reg, 1});
    // Mask off mode bits
    measure_reg &= 0xFC;
    measure_reg |= (uint8_t)new_mode;
    write(BMP280_MEASURE_REG, {&measure_reg, 1});
    m_mode = new_mode;
    trace1("bmp280::set_mode exiting.\n");
}

void bmp280::set_filtering(bmp280::filter coeff) {
    trace1("bmp280::set_filtering entered...\n");
    uint8_t config_reg;
    read(BMP280_CONFIG_REG, {&config_reg, 1});
    // Mask off filter coeff bits
    config_reg &= 0xE3; 
    config_reg |= (uint8_t)coeff << BMP280_FILTER_BIT;
    write(BMP280_CONFIG_REG, {&config_reg, 1});
    trace1("bmp280::set_filtering exiting.\n");
}

void bmp280::set_standby(bmp280::standby time) {
    trace1("bmp280::set_standby entered...\n");
    uint8_t config_reg;
    read(BMP280_CONFIG_REG, {&config_reg, 1});
    // Mask off standby time bits
    config_reg &= 0x1F; 
    config_reg |= (uint8_t)time << BMP280_STANDBY_BIT;
    write(BMP280_CONFIG_REG, {&config_reg, 1});
    m_standby = time;
    trace1("bmp280::set_standby exiting.\n");
}

void bmp280::read_raw_data() {
    trace1("bmp280::read_raw_data entered...\n");
    uint8_t data[6];
    read(BMP280_PMSB_REG, {data, sizeof(data)});
    m_raw_pressure = (((uint32_t)data[0]) << 12) | ((uint16_t)data[1]) << 4 | data[2] >> 4;
    m_raw_temperature = (((uint32_t)data[3]) << 12) | ((uint16_t)data[4]) << 4 | data[5] >> 4;
    m_needs_conversion = true;
    m_has_data = true;
    trace1("bmp280::read_raw_data exiting.\n");
}

int bmp280::read(uint8_t addr, std::span<uint8_t> buffer) {
    trace("bmp280::read 0x%02x %d entered...\n", addr, buffer.size());
    int rc = i2c_write_blocking(m_i2c, m_addr, &addr, 1, true);
    if(rc == PICO_ERROR_GENERIC) {
        error("Read address 0x%02x failed!\n", addr);
        return rc;
    }
    rc = i2c_read_blocking(m_i2c, m_addr, buffer.data(), buffer.size(), false);
    trace("bmp280::read exiting (rc = %d)\n", rc);
    return rc;
}

int bmp280::write(uint8_t addr, std::span<uint8_t> buffer) {
    trace("bmp280::write 0x%02x %d entered...\n", addr, buffer.size());
    // BMP280 does not auto increment register address on write,
    // so we have to set up the addresses ourselves
    uint8_t transfer[2 * buffer.size()];
    for(uint8_t i = 0; i < buffer.size(); i++) {
        transfer[2 * i] = addr + i;
        transfer[2 * i + 1] = buffer[i];
    }
    int rc = i2c_write_blocking(m_i2c, m_addr, transfer, 2 * buffer.size(), false);
    trace("bmp280::write exiting (rc = %d)\n", rc);
    return rc == PICO_ERROR_GENERIC ? rc : rc / 2;
}

int32_t bmp280::calc_temp() {
    trace1("bmp280::calc_temp entered...\n");
    // Taken from bosch bmp280 datasheet
    int32_t var1, var2;
    var1 = ((((m_raw_temperature >> 3) - ((int32_t)BMP280_T1 << 1))) * ((int32_t)BMP280_T2)) >> 11;
    var2 = (((((m_raw_temperature >> 4) - ((int32_t)BMP280_T1)) * ((m_raw_temperature >> 4) - ((int32_t)BMP280_T1))) >> 12) * ((int32_t)BMP280_T3)) >> 14;
    m_tfine = var1 + var2;
    m_temperature = (m_tfine * 5 + 128) >> 8;
    trace("bmp280::calc_temp exiting. m_tfine = %d, m_temperature = %d\n", m_tfine, m_temperature);
    return m_temperature;
}

uint32_t bmp280::calc_pressure() {
    trace1("bmp280::calc_pressure entered...\n");
    // Taken from bosch bmp280 datasheet
    int64_t var1, var2, pressure;
    var1 = ((int64_t)m_tfine) - 128000;
    var2 = var1 * var1 * (int64_t)BMP280_P6;
    var2 = var2 + ((var1 * (int64_t)BMP280_P5) << 17);
    var2 = var2 + (((int64_t)BMP280_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)BMP280_P3) >> 8) + ((var1 * (int64_t)BMP280_P2) << 12);
    var1 = (((1ll << 47) + var1)) * ((int64_t)BMP280_P1) >> 33;
    if(var1 == 0) {
        warn1("bmp280::calc_pressure exiting since var1 is zero.\n");
        return 0;
    }
    pressure = 1048576 - m_raw_pressure;
    pressure = (((pressure << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)BMP280_P9) * (pressure >> 13) * (pressure >> 13)) >> 25;
    var2 = (((int64_t)BMP280_P8) * pressure) >> 19;
    pressure = ((pressure + var1 + var2) >> 8) + (((int64_t)BMP280_P7) << 4);
    m_pressure = (uint32_t)pressure;
    trace("bmp280::calc_pressure exiting. m_pressure = %d\n", m_pressure);
    return m_pressure;
}

void bmp280::convert() {
    trace1("bmp280::convert entered...\n");
    calc_temp();
    calc_pressure();
    m_needs_conversion = false;
    trace1("bmp280::convert exiting.\n");
}