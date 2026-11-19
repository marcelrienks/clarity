#include "panels/oem_oil_panel.h"

#include <math.h>
#include <variant>

#include "definitions/constants.h"
#include "ui/gauge_specs.h"
#include "utilities/logging.h"
#include "utilities/unit_converter.h"

namespace
{
int32_t ClampValue(int32_t value, int32_t min_val, int32_t max_val)
{
    if (value < min_val)
    {
        return min_val;
    }
    if (value > max_val)
    {
        return max_val;
    }
    return value;
}
} // namespace

OemOilPanel::OemOilPanel(IGpioProvider &gpio, IConfigurationManager &configuration)
    : configuration_(configuration), pressure_sensor_(&gpio), temperature_sensor_(&gpio),
      pressure_gauge_(PRESSURE_GAUGE), temperature_gauge_(TEMPERATURE_GAUGE)
{
}

void OemOilPanel::Build(lv_obj_t *screen, Theme &theme)
{
    theme_ = &theme;

    pressure_sensor_.Init();
    temperature_sensor_.Init();
    ApplySensorSettings();

    pressure_gauge_.SetDeadbandPercent(
        QueryDeadbandPercent(config_constants::keys::OIL_PRESSURE_DEADBAND_PERCENT, "pressure"));
    temperature_gauge_.SetDeadbandPercent(
        QueryDeadbandPercent(config_constants::keys::OIL_TEMPERATURE_DEADBAND_PERCENT, "temperature"));

    pressure_gauge_.Build(screen, theme);
    temperature_gauge_.Build(screen, theme);

    // Built with the applied theme's colours: Update() restyles only when it changes
    last_theme_ = theme.Active();
    passes_ = 0;
}

void OemOilPanel::OnExit()
{
    // The gauges' objects and animations are deleted with the screen
    theme_ = nullptr;
}

void OemOilPanel::Update(uint32_t now)
{
    (void)now;
    if (!screen_)
    {
        return;
    }

    // The shared styles follow a theme change by themselves (Theme::Apply()); the needle, icon and pivot
    // colours are set locally
    if (theme_->Active() != last_theme_)
    {
        last_theme_ = theme_->Active();
        pressure_gauge_.Restyle(theme_->Colors());
        temperature_gauge_.Restyle(theme_->Colors());
    }

    // Apply sensor settings periodically instead of every pass; this still catches changes quickly enough
    if (++passes_ % SETTINGS_INTERVAL_PASSES == 0)
    {
        ApplySensorSettings();
    }

    // A gauge that is still animating ignores readings, so skip reading its sensor
    if (!pressure_gauge_.Animating())
    {
        pressure_gauge_.AnimateTo(static_cast<float>(ReadPressure()));
    }
    if (!temperature_gauge_.Animating())
    {
        temperature_gauge_.AnimateTo(static_cast<float>(ReadTemperature()));
    }
}

clarity::core::NavIntent OemOilPanel::OnButton(clarity::core::ButtonEvent event)
{
    if (event != clarity::core::ButtonEvent::Long)
    {
        // No action on short press - monitoring panel
        log_v("OemOilPanel short press - no action");
        return clarity::core::NavIntent::None;
    }

    log_i("OemOilPanel long press - loading config panel");
    return clarity::core::NavIntent::OpenConfig;
}

// ========== Readings ==========

int32_t OemOilPanel::ReadPressure()
{
    return MapPressureValue(std::get<int32_t>(pressure_sensor_.GetReading()));
}

int32_t OemOilPanel::ReadTemperature()
{
    return MapTemperatureValue(std::get<int32_t>(temperature_sensor_.GetReading()));
}

/// Raw reading in the configured unit to the 0.1 bar display scale (0 to 60). The unit mapping moves out in
/// phase 8 (D-08, D-09)
int32_t OemOilPanel::MapPressureValue(int32_t sensor_value)
{
    std::string unit = config_constants::defaults::DEFAULT_PRESSURE_UNIT;
    if (auto value = configuration_.QueryConfig<std::string>(OilPressureSensor::CONFIG_UNIT))
    {
        unit = *value;
    }

    // Convert to Bar if needed
    float bar_value = static_cast<float>(sensor_value);
    if (unit == config_constants::units::PSI_UPPER)
    {
        bar_value = unit_converter::PsiToBar(bar_value);
    }
    else if (unit == config_constants::units::KPA_UPPER)
    {
        bar_value = unit_converter::KpaToBar(bar_value);
    }

    // Bar range (1 to 10) to the display scale (0 to 60)
    const int32_t bar = ClampValue(static_cast<int32_t>(bar_value), 1, sensor_constants::PRESSURE_MAX_BAR);
    return ((bar - 1) * 60) / (sensor_constants::PRESSURE_MAX_BAR - 1);
}

/// Raw reading in the configured unit to degrees Celsius, one to one on the display scale (0 to 120)
int32_t OemOilPanel::MapTemperatureValue(int32_t sensor_value)
{
    std::string unit = config_constants::defaults::DEFAULT_TEMPERATURE_UNIT;
    if (auto value = configuration_.QueryConfig<std::string>(OilTemperatureSensor::CONFIG_UNIT))
    {
        unit = *value;
    }

    float celsius = static_cast<float>(sensor_value);
    if (unit == config_constants::units::FAHRENHEIT)
    {
        celsius = unit_converter::FahrenheitToCelsius(celsius);
    }

    return ClampValue(static_cast<int32_t>(celsius), sensor_constants::TEMPERATURE_MIN_CELSIUS,
                      sensor_constants::TEMPERATURE_MAX_CELSIUS);
}

// ========== Configuration ==========

/// Applies the update rate and units from the configuration to the sensors, when they changed
void OemOilPanel::ApplySensorSettings()
{
    int update_rate = 500;
    if (auto rate = configuration_.QueryConfig<int>(config_constants::keys::SYSTEM_UPDATE_RATE))
    {
        update_rate = *rate;
    }

    std::string pressure_unit = config_constants::defaults::DEFAULT_PRESSURE_UNIT;
    if (auto unit = configuration_.QueryConfig<std::string>(OilPressureSensor::CONFIG_UNIT))
    {
        pressure_unit = *unit;
    }

    std::string temperature_unit = config_constants::defaults::DEFAULT_TEMPERATURE_UNIT;
    if (auto unit = configuration_.QueryConfig<std::string>(OilTemperatureSensor::CONFIG_UNIT))
    {
        temperature_unit = *unit;
    }

    const bool rate_changed = last_update_rate_ != update_rate;
    const bool pressure_changed = last_pressure_unit_ != pressure_unit;
    const bool temperature_changed = last_temperature_unit_ != temperature_unit;

    if (rate_changed)
    {
        pressure_sensor_.SetUpdateRate(update_rate);
        temperature_sensor_.SetUpdateRate(update_rate);
        last_update_rate_ = update_rate;
    }
    if (pressure_changed)
    {
        pressure_sensor_.SetTargetUnit(pressure_unit);
        last_pressure_unit_ = pressure_unit;
    }
    if (temperature_changed)
    {
        temperature_sensor_.SetTargetUnit(temperature_unit);
        last_temperature_unit_ = temperature_unit;
    }

    if (rate_changed || pressure_changed || temperature_changed)
    {
        log_t("Sensor settings updated - Rate: %dms, Pressure: %s, Temp: %s", update_rate, pressure_unit.c_str(),
              temperature_unit.c_str());
    }
}

int32_t OemOilPanel::QueryDeadbandPercent(const char *key, [[maybe_unused]] const char *name)
{
    if (auto percent = configuration_.QueryConfig<int>(key))
    {
        log_i("Using configured %s deadband: %d%%", name, *percent);
        return *percent;
    }
    log_i("No %s deadband config found, using default: %d%%", name, DEFAULT_DEADBAND_PERCENT);
    return DEFAULT_DEADBAND_PERCENT;
}
