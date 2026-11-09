#pragma once

/**
 * @file definitions/constants.h
 * @brief Centralized constants to replace magic strings and values
 *
 * This file contains various categories of constants used throughout the application
 * to improve maintainability, reduce magic strings, and ensure consistency.
 */

/*============================================================================*/
/*                           CONSTANT NAMESPACES                             */
/*============================================================================*/

/**
 * @brief Available themes for the application
 */
namespace Themes {
    static constexpr const char* DAY = "Day";
    static constexpr const char* NIGHT = "Night";
    static constexpr const char* ERROR = "Error";
}

/**
 * @brief Panel names used throughout the system
 */
namespace PanelNames {
    static constexpr const char* OIL = "OemOilPanel";
    static constexpr const char* CONFIG = "ConfigPanel";
    static constexpr const char* ERROR = "ErrorPanel";
    static constexpr const char* KEY = "KeyPanel";
    static constexpr const char* LOCK = "LockPanel";
    static constexpr const char* SPLASH = "SplashPanel";
}

/**
 * @brief Trigger IDs for the interrupt system
 */
namespace TriggerIds {
    static constexpr const char* KEY_PRESENT = "key_present";
    static constexpr const char* KEY_NOT_PRESENT = "key_not_present";
    static constexpr const char* LOCK = "lock";
    static constexpr const char* LIGHTS = "lights";
    static constexpr const char* ERROR = "error";
    static constexpr const char* SHORT_PRESS = "short_press";
    static constexpr const char* LONG_PRESS = "long_press";
}

/*============================================================================*/
/*                            DATA CONSTANTS                                 */
/*============================================================================*/

namespace DataConstants {
    namespace ErrorInfo {
        static constexpr size_t MAX_MESSAGE_LENGTH = 128;
    }
}

/*============================================================================*/
/*                              UI CONSTANTS                                 */
/*============================================================================*/

namespace UIConstants {
    static constexpr const char* APP_NAME = "Clarity";
    static constexpr const char* GAUGE_LOW_LABEL = "L";
    static constexpr const char* GAUGE_HIGH_LABEL = "H";
}

namespace UIStrings {
    // ========== Config Panel Action Types ==========
    namespace ActionTypes {
        static constexpr const char* ENTER_SECTION = "enter_section";
        static constexpr const char* TOGGLE_BOOLEAN = "toggle_boolean";
        static constexpr const char* SHOW_OPTIONS = "show_options";
        static constexpr const char* SET_CONFIG_VALUE = "set_config_value";
        static constexpr const char* BACK = "back";
        static constexpr const char* NONE = "none";
        static constexpr const char* PANEL_EXIT = "panel_exit";
        static constexpr const char* PANEL_LOAD = "panel_load";
        static constexpr const char* SUBMENU = "submenu";
    }

    // ========== Button Action Strings ==========
    namespace ButtonActionStrings {
        static constexpr const char* SHORT_PRESS = "SHORT_PRESS";
        static constexpr const char* LONG_PRESS = "LONG_PRESS";
        static constexpr const char* NONE = "NONE";
        static constexpr const char* SHORT = "SHORT";
        static constexpr const char* LONG = "LONG";
    }

    // ========== Menu Labels ==========
    namespace MenuLabels {
        static constexpr const char* EXIT = "Exit";
        static constexpr const char* BACK = "Back";
        static constexpr const char* CONFIGURATION = "Configuration";
    }

    // ========== Configuration Keys ==========
    namespace ConfigKeys {
        static constexpr const char* STYLE_MANAGER_THEME = "style_manager.theme";
    }

    // ========== Default Hint Text ==========
    namespace HintText {
        static constexpr const char* SHORT_LONG_PRESS = "Short: Next | Long: Select";
    }

    // ========== Configuration UI Labels ==========
    namespace ConfigLabels {
        static constexpr const char* DEFAULT_PANEL = "Default Panel";
        static constexpr const char* UPDATE_RATE = "Update Rate";
        static constexpr const char* SHOW_SPLASH = "Show Splash";
        static constexpr const char* THEME = "Theme";
        static constexpr const char* BRIGHTNESS = "Brightness";
        static constexpr const char* TEMPERATURE_UNIT = "Temperature Unit";
        static constexpr const char* PRESSURE_UNIT = "Pressure Unit";
        static constexpr const char* UPDATE_RATE_MS = "Update Rate (ms)";
        static constexpr const char* CALIBRATION_OFFSET = "Calibration Offset";
        static constexpr const char* CALIBRATION_SCALE = "Calibration Scale";
        static constexpr const char* DEBOUNCE_MS = "Debounce (ms)";
        static constexpr const char* LONG_PRESS_MS = "Long Press (ms)";
    }

    // ========== Config Component UI Text ==========
    namespace ConfigUI {
        static constexpr const char* CURRENT_LABEL_PREFIX = "Current: ";
        static constexpr const char* SELECTED_MENU_PREFIX = "> ";
        static constexpr const char* UNSELECTED_MENU_PREFIX = "  ";
        static constexpr const char* UNIT_SEPARATOR = " ";
        static constexpr const char* ACTION_TYPE_NONE = "none";
        static constexpr const char* EMPTY_PARAM = "";
    }

    // ========== Error Component UI Text ==========
    namespace ErrorUI {
        static constexpr const char* SINGLE_ERROR_COUNT = "1/1";
        static constexpr const char* DEFAULT_ERROR_LEVEL = "ERROR";
        static constexpr const char* DEFAULT_ERROR_SOURCE = "System";
        static constexpr const char* LOADING_ERRORS_MESSAGE = "Loading errors...";
        static constexpr const char* LOADING_NAVIGATION = "Loading...";
        static constexpr const char* NAVIGATION_INSTRUCTIONS = "short: next, long: exit";

        // Error level text
        static constexpr const char* LEVEL_CRITICAL = "CRIT";
        static constexpr const char* LEVEL_ERROR = "ERR";
        static constexpr const char* LEVEL_WARNING = "WARN";
        static constexpr const char* LEVEL_UNKNOWN = "UNKN";
    }
}

/*============================================================================*/
/*                            STORAGE CONSTANTS                              */
/*============================================================================*/

namespace StorageConstants {
    // ========== NVS Storage Configuration ==========
    namespace NVS {
        static constexpr const char* CONFIG_KEY = "config";
        static constexpr const char* META_NAMESPACE = "config_meta";
        static constexpr const char* SECTION_PREFIX = "cfg_";
        static constexpr const char* MIGRATION_FLAG = "migration_v1";
        static constexpr int MAX_NAMESPACE_LEN = 15;
    }
}

/*============================================================================*/
/*                       CONFIGURATION CONSTANTS                             */
/*============================================================================*/

namespace ConfigConstants {
    // Section Names
    namespace Sections {
        static constexpr const char* SYSTEM = "system";
        static constexpr const char* STYLE_MANAGER = "style_manager";
        static constexpr const char* OIL_PRESSURE_SENSOR = "oil_pressure_sensor";
        static constexpr const char* OIL_TEMPERATURE_SENSOR = "oil_temperature_sensor";
        static constexpr const char* BUTTON_SENSOR = "button_sensor";
        static constexpr const char* SPLASH_PANEL = "splash_panel";
    }

    // Item Keys
    namespace Items {
        static constexpr const char* DEFAULT_PANEL = "default_panel";
        static constexpr const char* UPDATE_RATE = "update_rate";
        static constexpr const char* SHOW_SPLASH = "show_splash";
        static constexpr const char* THEME = "theme";
        static constexpr const char* DURATION = "duration";
        static constexpr const char* UNIT = "unit";
        static constexpr const char* OFFSET = "offset";
        static constexpr const char* SCALE = "scale";
        static constexpr const char* BRIGHTNESS = "brightness";
        static constexpr const char* DEBOUNCE_MS = "debounce_ms";
        static constexpr const char* LONG_PRESS_MS = "long_press_ms";
    }

    // Configuration Keys (full dotted paths)
    namespace Keys {
        static constexpr const char* SYSTEM_THEME = "system.theme";
        static constexpr const char* SYSTEM_UPDATE_RATE = "system.update_rate";
        static constexpr const char* SYSTEM_DEFAULT_PANEL = "system.default_panel";
        static constexpr const char* SYSTEM_SHOW_SPLASH = "system.show_splash";

        // Button sensor configuration keys
        static constexpr const char* BUTTON_DEBOUNCE_MS = "button_sensor.debounce_ms";
        static constexpr const char* BUTTON_LONG_PRESS_MS = "button_sensor.long_press_ms";

        // Oil pressure sensor configuration keys
        static constexpr const char* OIL_PRESSURE_UNIT = "oil_pressure_sensor.unit";
        static constexpr const char* OIL_PRESSURE_UPDATE_RATE = "oil_pressure_sensor.update_rate";
        static constexpr const char* OIL_PRESSURE_OFFSET = "oil_pressure_sensor.offset";
        static constexpr const char* OIL_PRESSURE_SCALE = "oil_pressure_sensor.scale";

        // Oil temperature sensor configuration keys
        static constexpr const char* OIL_TEMPERATURE_UNIT = "oil_temperature_sensor.unit";
        static constexpr const char* OIL_TEMPERATURE_UPDATE_RATE = "oil_temperature_sensor.update_rate";
        static constexpr const char* OIL_TEMPERATURE_OFFSET = "oil_temperature_sensor.offset";
        static constexpr const char* OIL_TEMPERATURE_SCALE = "oil_temperature_sensor.scale";

        // Oil pressure component configuration keys (display behavior)
        static constexpr const char* OIL_PRESSURE_DEADBAND_PERCENT = "oil_pressure_component.deadband_percent";

        // Oil temperature component configuration keys (display behavior)
        static constexpr const char* OIL_TEMPERATURE_DEADBAND_PERCENT = "oil_temperature_component.deadband_percent";

        // Style manager configuration keys
        static constexpr const char* STYLE_MANAGER_THEME = "style_manager.theme";
        static constexpr const char* STYLE_MANAGER_BRIGHTNESS = "style_manager.brightness";

        // Splash panel configuration keys
        static constexpr const char* SPLASH_PANEL_DURATION = "splash_panel.duration";
    }

    // Panel Names
    namespace Panels {
        static constexpr const char* OEM_OIL_PANEL = "OemOilPanel";
    }

    // Config Item Types
    namespace Types {
        static constexpr const char* SELECTION = "Selection";
        static constexpr const char* BOOLEAN = "Boolean";
        static constexpr const char* INTEGER = "Integer";
        static constexpr const char* FLOAT = "Float";
        static constexpr const char* STRING = "String";

        // Type names for internal logic
        static constexpr const char* UNSET = "unset";
        static constexpr const char* INTEGER_INTERNAL = "integer";
        static constexpr const char* FLOAT_INTERNAL = "float";
        static constexpr const char* BOOLEAN_INTERNAL = "boolean";
        static constexpr const char* STRING_INTERNAL = "string";
        static constexpr const char* UNKNOWN = "unknown";
    }

    // Boolean Values
    namespace BooleanValues {
        static constexpr const char* TRUE_STRING = "true";
        static constexpr const char* FALSE_STRING = "false";
        static constexpr const char* TRUE_NUMERIC = "1";
        static constexpr const char* EMPTY_STRING = "";
    }

    // Units
    namespace Units {
        static constexpr const char* MILLISECONDS = "ms";
        static constexpr const char* PSI = "psi";
        static constexpr const char* BAR = "bar";
        static constexpr const char* PERCENT = "%";

        // Unit strings for logic comparisons
        static constexpr const char* PSI_UPPER = "PSI";
        static constexpr const char* KPA_UPPER = "kPa";
        static constexpr const char* BAR_UPPER = "Bar";
        static constexpr const char* FAHRENHEIT = "F";
        static constexpr const char* CELSIUS = "C";
    }

    // Default Values
    namespace Defaults {
        static constexpr const char* DEFAULT_PRESSURE_UNIT = "Bar";
        static constexpr const char* DEFAULT_TEMPERATURE_UNIT = "C";
        static constexpr int DEFAULT_UPDATE_RATE = 500;
        static constexpr float DEFAULT_CALIBRATION_OFFSET = 0.0f;
        static constexpr float DEFAULT_CALIBRATION_SCALE = 1.0f;
        static constexpr int DEFAULT_BRIGHTNESS = 80;
        static constexpr int DEFAULT_DEBOUNCE_MS = 400;
        static constexpr int DEFAULT_LONG_PRESS_MS = 1500;
    }

    // Section Display Names
    namespace SectionNames {
        static constexpr const char* SYSTEM = "System";
        static constexpr const char* STYLE_MANAGER = "Display";
        static constexpr const char* SPLASH_PANEL = "Splash Scr.";
        static constexpr const char* BUTTON_SENSOR = "Button";
        static constexpr const char* OIL_PRESSURE_SENSOR = "Oil Press.";
        static constexpr const char* OIL_TEMPERATURE_SENSOR = "Oil Temp.";
    }

    // Configuration Option Values (metadata options)
    namespace Options {
        // Pressure unit options
        static constexpr const char* PRESSURE_UNITS = "Bar,PSI,kPa";

        // Temperature unit options
        static constexpr const char* TEMPERATURE_UNITS = "C,F";

        // Theme options
        static constexpr const char* THEMES = "Day,Night";

        // Panel options
        static constexpr const char* PANEL_OPTIONS = "OemOilPanel,ConfigPanel";

        // Update rate options (milliseconds)
        static constexpr const char* UPDATE_RATES = "250,500,1000,2000";
        static constexpr const char* SYSTEM_UPDATE_RATES = "100,250,500,750,1000,1500,2000";

        // Calibration offset options
        static constexpr const char* CALIBRATION_OFFSETS = "-5.0,-2.0,-1.0,-0.5,0.0,0.5,1.0,2.0,5.0";

        // Calibration scale options
        static constexpr const char* CALIBRATION_SCALES = "0.9,0.95,1.0,1.05,1.1";

        // Brightness options (percentage)
        static constexpr const char* BRIGHTNESS_LEVELS = "0,10,20,30,40,50,60,70,80,90,100";

        // Button timing options
        static constexpr const char* DEBOUNCE_OPTIONS = "200,400,600";
        static constexpr const char* LONG_PRESS_OPTIONS = "1000,1500,2000,3000";
    }
}

/*============================================================================*/
/*                            TIMING CONSTANTS                               */
/*============================================================================*/

namespace TimingConstants {
    // Splash panel specific
    namespace Splash {
        static constexpr uint32_t DISPLAY_TIME_MS = 500;
        static constexpr uint32_t DELAY_TIME_MS = 200;
        static constexpr const char* DEFAULT_DURATION = "1500";
        static constexpr const char* DURATION_OPTIONS = "1500,1750,2000,2500";
        static constexpr const char* DURATION_LABEL = "Duration";
        static constexpr const char* DURATION_UNIT = "ms";
    }
}

/*============================================================================*/
/*                            SENSOR CONSTANTS                               */
/*============================================================================*/

namespace SensorConstants {
    // Base pressure values (Bar)
    static constexpr int32_t PRESSURE_MIN_BAR = 0;  // 0 Bar minimum
    static constexpr int32_t PRESSURE_MAX_BAR = 10; // 10 Bar maximum

    // Component Display Scale Constants
    // These define the internal scale ranges used by gauge components
    static constexpr int32_t PRESSURE_DISPLAY_SCALE_MIN = 0;   // Pressure gauge minimum scale value
    static constexpr int32_t PRESSURE_DISPLAY_SCALE_MAX = 60;  // Pressure gauge maximum scale value (represents 6.0 Bar)
    static constexpr int32_t TEMPERATURE_DISPLAY_SCALE_MIN = 0; // Temperature gauge minimum scale value
    static constexpr int32_t TEMPERATURE_DISPLAY_SCALE_MAX = 120; // Temperature gauge maximum scale value (120°C)

    // Base temperature values (Celsius)
    static constexpr int32_t TEMPERATURE_MIN_CELSIUS = 0;   // 0°C minimum
    static constexpr int32_t TEMPERATURE_MAX_CELSIUS = 120; // 120°C maximum for oil temp
}
/*============================================================================*/
/*                           HARDWARE CONSTANTS                              */
/*============================================================================*/

namespace HardwareConstants {
    // ========== Display Hardware Constants ==========
    namespace Display {
        static constexpr int SCREEN_WIDTH = 240;
        static constexpr int SCREEN_HEIGHT = 240;
        static constexpr int SCREEN_OFFSET_X = 0;
        static constexpr int SCREEN_OFFSET_Y = 0;
        static constexpr bool SCREEN_RGB_ORDER = false;
        static constexpr int SCREEN_DEFAULT_BRIGHTNESS = 100;
        static constexpr unsigned int BUFFER_LINE_COUNT = 40;
        static constexpr unsigned int LV_BUFFER_SIZE = (SCREEN_WIDTH * BUFFER_LINE_COUNT * sizeof(uint16_t));
    }

    // ========== SPI Configuration Constants ==========
    namespace SPI {
        static constexpr int SPI_HOST_VALUE = 2; // SPI2_HOST value
        static constexpr int SCLK_PIN = 18;
        static constexpr int MOSI_PIN = 23;
        static constexpr int MISO_PIN = -1;
        static constexpr int DC_PIN = 16;
        static constexpr int CS_PIN = 22;
        static constexpr int RST_PIN = 4;
        static constexpr int BL_PIN = 3;
        static constexpr int BUZZER_PIN = -1;
    }

    // ========== Trigger IDs for logic comparisons ==========
    namespace TriggerIDs {
        static constexpr const char* KEY_PRESENT = "key_present";
    }
}
