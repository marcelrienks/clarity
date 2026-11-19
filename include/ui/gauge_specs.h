#pragma once

#include "definitions/constants.h"
#include "ui/gauge.h"
#include <icons/oil_can_regular.h>
#include <icons/oil_temp_regular.h>

/// The pressure gauge (left): 0.0 to 6.0 bar in 0.1 bar units, danger at 0.5 bar and below
inline constexpr GaugeSpec PRESSURE_GAUGE{&oil_can_regular,
                                          sensor_constants::PRESSURE_DISPLAY_SCALE_MIN,
                                          sensor_constants::PRESSURE_DISPLAY_SCALE_MAX,
                                          5,
                                          false,
                                          210,
                                          120,
                                          false,
                                          -55};

/// The temperature gauge (right): 0 to 120 degrees Celsius, danger at 100 and above. The scale runs from the
/// far end, so a cold engine reads at the bottom
inline constexpr GaugeSpec TEMPERATURE_GAUGE{&oil_temp_regular,
                                             sensor_constants::TEMPERATURE_DISPLAY_SCALE_MIN,
                                             sensor_constants::TEMPERATURE_DISPLAY_SCALE_MAX,
                                             100,
                                             true,
                                             30,
                                             120,
                                             true,
                                             55};
