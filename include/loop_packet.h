#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include "units.h"

struct packet_args {
    std::optional<celsius_t> outTemp = {};
    std::optional<celsius_t> inTemp = {};
    std::optional<mbar_t> barometer = {};
    std::optional<mbar_t> pressure = {};
    std::optional<kmph_t> windSpeed = {};
    std::optional<degree_compass_t> windDir = {};
    std::optional<kmph_t> windGust = {};
    std::optional<degree_compass_t> windGustDir = {};
    std::optional<percentage_t> outHumidity = {};
    std::optional<percentage_t> inHumidity = {};
    std::optional<watt_m2_t> radiation = {};
    std::optional<uv_index_t> UV = {};
    std::optional<cm_t> rain = {};
    std::optional<int> txBatteryStatus = {};
    std::optional<int> windBatteryStatus = {};
    std::optional<int> rainBatteryStatus = {};
    std::optional<int> outTempBatteryStatus = {};
    std::optional<int> inTempBatteryStatus = {};
    std::optional<volt_t> consBatteryVoltage = {};
    std::optional<volt_t> heatingVoltage = {};
    std::optional<volt_t> supplyVoltage = {};
    std::optional<volt_t> referenceVoltage = {};
    std::optional<percentage_t> rxCheckPercent = {};
};

nlohmann::json create_packet(packet_args args) {
    nlohmann::json packet = {};

    packet["outTemp"] = nullptr;
    packet["inTemp"] = nullptr;
    packet["barometer"] = nullptr;
    packet["pressure"] = nullptr;
    packet["windSpeed"] = nullptr;
    packet["windDir"] = nullptr;
    packet["windGust"] = nullptr;
    packet["windGustDir"] = nullptr;
    packet["outHumidity"] = nullptr;
    packet["inHumidity"] = nullptr;
    packet["radiation"] = nullptr;
    packet["UV"] = nullptr;
    packet["rain"] = nullptr;
    packet["txBatteryStatus"] = nullptr;
    packet["windBatteryStatus"] = nullptr;
    packet["rainBatteryStatus"] = nullptr;
    packet["outTempBatteryStatus"] = nullptr;
    packet["inTempBatteryStatus"] = nullptr;
    packet["consBatteryVoltage"] = nullptr;
    packet["heatingVoltage"] = nullptr;
    packet["supplyVoltage"] = nullptr;
    packet["referenceVoltage"] = nullptr;
    packet["rxCheckPercent"] = nullptr;

    if(args.outTemp.has_value())
        packet["outTemp"] = *(args.outTemp);
    if(args.inTemp.has_value())
        packet["inTemp"] = *(args.inTemp);
    if(args.barometer.has_value())
        packet["barometer"] = *(args.barometer);
    if(args.pressure.has_value())
        packet["pressure"] = *(args.pressure);
    if(args.windSpeed.has_value())
        packet["windSpeed"] = *(args.windSpeed);
    if(args.windDir.has_value())
        packet["windDir"] = *(args.windDir);
    if(args.windGust.has_value())
        packet["windGust"] = *(args.windGust);
    if(args.windGustDir.has_value())
        packet["windGustDir"] = *(args.windGustDir);
    if(args.outHumidity.has_value())
        packet["outHumidity"] = *(args.outHumidity);
    if(args.inHumidity.has_value())
        packet["inHumidity"] = *(args.inHumidity);
    if(args.radiation.has_value())
        packet["radiation"] = *(args.radiation);
    if(args.UV.has_value())
        packet["UV"] = *(args.UV);
    if(args.rain.has_value())
        packet["rain"] = *(args.rain);
    if(args.txBatteryStatus.has_value())
        packet["txBatteryStatus"] = *(args.txBatteryStatus);
    if(args.windBatteryStatus.has_value())
        packet["windBatteryStatus"] = *(args.windBatteryStatus);
    if(args.rainBatteryStatus.has_value())
        packet["rainBatteryStatus"] = *(args.rainBatteryStatus);
    if(args.outTempBatteryStatus.has_value())
        packet["outTempBatteryStatus"] = *(args.outTempBatteryStatus);
    if(args.inTempBatteryStatus.has_value())
        packet["inTempBatteryStatus"] = *(args.inTempBatteryStatus);
    if(args.consBatteryVoltage.has_value())
        packet["consBatteryVoltage"] = *(args.consBatteryVoltage);
    if(args.heatingVoltage.has_value())
        packet["heatingVoltage"] = *(args.heatingVoltage);
    if(args.supplyVoltage.has_value())
        packet["supplyVoltage"] = *(args.supplyVoltage);
    if(args.referenceVoltage.has_value())
        packet["referenceVoltage"] = *(args.referenceVoltage);
    if(args.rxCheckPercent.has_value())
        packet["rxCheckPercent"] = *(args.rxCheckPercent);
    return packet;
}
