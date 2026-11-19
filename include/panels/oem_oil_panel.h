#pragma once

#include <cstdint>
#include <string>

#include "clarity/core/types.h"
#include "definitions/constants.h"
#include "interfaces/i_configuration_manager.h"
#include "interfaces/i_gpio_provider.h"
#include "sensors/oil_pressure_sensor.h"
#include "sensors/oil_temperature_sensor.h"
#include "ui/gauge.h"
#include "ui/panel.h"

/**
 * @class OemOilPanel
 * @brief Main oil monitoring dashboard: a pressure gauge and a temperature gauge
 *
 * @details A long-lived Panel (A-05) with two Gauge widgets (A-09) drawn over each other, each on its own
 * 240 px scale so they show on either side of the screen. The panel owns the two oil sensors until phase 8
 * moves sensing out: every Update() reads each sensor (unless that gauge's needle is still moving), maps the
 * reading to the gauge's scale units and hands it to the gauge, which applies the dead-band and the animation
 * rules (`clarity::core::GaugeMotion`).
 *
 * A theme change recolours the needles, icons and pivots in Update(); the shared styles follow by themselves.
 */
class OemOilPanel : public Panel
{
  public:
    OemOilPanel(IGpioProvider &gpio, IConfigurationManager &configuration);

    static constexpr const char *NAME = panel_names::OIL;

    void Update(uint32_t now) override;

    /// Long press opens Config; short press does nothing
    clarity::core::NavIntent OnButton(clarity::core::ButtonEvent event) override;

  protected:
    void Build(lv_obj_t *screen, Theme &theme) override;
    void OnExit() override;

  private:
    static constexpr uint32_t SETTINGS_INTERVAL_PASSES = 100;
    static constexpr int32_t DEFAULT_DEADBAND_PERCENT = Gauge::DEFAULT_DEADBAND_PERCENT;

    int32_t ReadPressure();
    int32_t ReadTemperature();
    int32_t MapPressureValue(int32_t sensor_value);
    int32_t MapTemperatureValue(int32_t sensor_value);
    void ApplySensorSettings();
    int32_t QueryDeadbandPercent(const char *key, const char *name);

    IConfigurationManager &configuration_;
    Theme *theme_ = nullptr; ///< Valid between Build() and OnExit()

    OilPressureSensor pressure_sensor_;
    OilTemperatureSensor temperature_sensor_;
    Gauge pressure_gauge_;
    Gauge temperature_gauge_;

    clarity::core::ThemeId last_theme_ = clarity::core::ThemeId::Day; ///< Theme the gauges were coloured for
    uint32_t passes_ = 0;
    int last_update_rate_ = -1;
    std::string last_pressure_unit_;
    std::string last_temperature_unit_;
};
