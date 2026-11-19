#pragma once

#define LGFX_USE_V1

// Third-party header: keep project warning flags from firing inside it
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#include <LovyanGFX.hpp>
#pragma GCC diagnostic pop
#include "definitions/constants.h"
#include <lvgl.h>

// Screen - use constants from centralized location
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 240
#define SCREEN_OFFSET_X 0
#define SCREEN_OFFSET_Y 0
#define SCREEN_RGB_ORDER false
#define SCREEN_DEFAULT_BRIGHTNESS 100

// Display
#define SPI SPI2_HOST

// Pins - use constants from centralized location
#define SCLK 18
#define MOSI 23
#define MISO (-1)
#define DC 16
#define CS 22
#define RST 4
#define BL 3
#define BUZZER (-1)

/**
 * @class DeviceProvider
 * @brief Hardware abstraction provider for ESP32 with GC9A01 display
 *
 * @details This class provides the concrete implementation of the display provider interface,
 * managing the physical display hardware and LVGL integration. It handles the SPI communication
 * with the GC9A01 display controller and provides display buffer management.
 *
 * @hardware_target ESP32-WROOM-32 with NodeMCU-32S development board
 * @display_target 1.28" round GC9A01 240x240 display (Waveshare compatible)
 * @interface SPI2_HOST with hardware-defined pins
 * @buffer_strategy Dual 60-line buffers for smooth rendering
 *
 * @design_pattern Dependency Injectable - managed through service container
 * @thread_safety LVGL display callbacks are thread-safe
 * @memory_usage ~57.6KB for dual display buffers (240*60*2*2 bytes)
 *
 * @context This is the main hardware interface that all panels and components
 * render to. The display is 240x240 pixels with a round form factor.
 */
class DeviceProvider : public lgfx::LGFX_Device
{
  public:
    // ========== Constructors and Destructor ==========
    DeviceProvider();
    DeviceProvider(const DeviceProvider &) = delete;
    DeviceProvider &operator=(const DeviceProvider &) = delete;
    ~DeviceProvider() override = default;

    // ========== Public Interface Methods ==========
    void Prepare();
    lv_obj_t *GetScreen() const;
    bool IsReady() const;

  private:
    // ========== Static Methods ==========
    static void DisplayFlushCallback(lv_display_t *display, const lv_area_t *area, unsigned char *data);

    // ========== Private Data Members ==========
    lv_obj_t *screen_{};

    lgfx::Panel_GC9A01 panel_instance_; // Waveshare Round 1.28inch LCD Display Module
    lgfx::Light_PWM light_instance_;
    lgfx::Bus_SPI bus_instance_;

    // Two partial-render buffers of 60 lines each (a quarter of the 240 line screen).
    // RGB565 is 2 bytes per pixel: 240 * 60 * 2 = 28.8KB per buffer, ~57.6KB in total.
    static constexpr unsigned int BUFFER_LINE_COUNT = 60;
    static constexpr unsigned int BYTES_PER_PIXEL = 2; // LV_COLOR_FORMAT_RGB565
    static constexpr unsigned int LV_BUFFER_SIZE = (SCREEN_WIDTH * BUFFER_LINE_COUNT * BYTES_PER_PIXEL);
    uint8_t lv_buffer_[2][LV_BUFFER_SIZE]{};
};