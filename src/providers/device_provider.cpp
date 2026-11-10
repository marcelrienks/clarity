#include "providers/device_provider.h"
#include "providers/gpio_provider.h"
#include "providers/lvgl_display_provider.h"
#include <esp32-hal-log.h>

// ========== Constructors and Destructor ==========

/**
 * @brief Configure hardware components for ESP32 + GC9A01 display
 * @details Sets up SPI bus, display panel, and PWM backlight with optimized settings
 */
DeviceProvider::DeviceProvider()
{
    log_v("DeviceProvider() constructor called");

    // Configure SPI bus for high-speed display communication
    {
        auto cfg = bus_instance_.config();
        cfg.spi_host = SPI;                // Use SPI2_HOST
        cfg.spi_mode = 0;                  // SPI mode 0 (CPOL=0, CPHA=0)
        cfg.freq_write = 80000000;         // 80MHz write frequency for fast rendering
        cfg.freq_read = 20000000;          // 20MHz read frequency (slower for stability)
        cfg.spi_3wire = true;              // 3-wire SPI (no MISO)
        cfg.use_lock = true;               // Thread-safe SPI access
        cfg.dma_channel = SPI_DMA_CH_AUTO; // Automatic DMA channel allocation
        cfg.pin_sclk = SCLK;
        cfg.pin_mosi = MOSI;
        cfg.pin_miso = MISO;
        cfg.pin_dc = DC;

        bus_instance_.config(cfg);
        panel_instance_.setBus(&bus_instance_);
    }

    // Configure GC9A01 display panel parameters
    {
        auto cfg = panel_instance_.config();

        cfg.pin_cs = CS;
        cfg.pin_rst = RST;
        cfg.pin_busy = -1; // Not used on GC9A01

        cfg.memory_width = SCREEN_WIDTH;
        cfg.memory_height = SCREEN_HEIGHT;
        cfg.panel_width = SCREEN_WIDTH;
        cfg.panel_height = SCREEN_HEIGHT;
        cfg.offset_x = SCREEN_OFFSET_X;
        cfg.offset_y = SCREEN_OFFSET_Y;
        cfg.offset_rotation = 0;
        cfg.dummy_read_pixel = 8;
        cfg.dummy_read_bits = 1;
        cfg.readable = false; // Write-only for performance
        cfg.rgb_order = SCREEN_RGB_ORDER;
        cfg.dlen_16bit = false;
        cfg.bus_shared = false;

#ifdef INVERT
        cfg.invert = true; // Color inversion for Waveshare 1.28" display
#endif

        panel_instance_.config(cfg);
    }

    // Configure PWM backlight control
    {
        auto cfg = light_instance_.config();

        cfg.pin_bl = BL;
        cfg.invert = false;  // Normal PWM polarity
        cfg.freq = 44100;    // PWM frequency
        cfg.pwm_channel = 1; // PWM channel allocation

        light_instance_.config(cfg);
        panel_instance_.setLight(&light_instance_);
    }

    // Link panel to LovyanGFX device
    setPanel(&panel_instance_);
}

// ========== Public Interface Methods ==========

/**
 * @brief Initialize display hardware and LVGL graphics subsystem
 * @details Performs complete hardware initialization, LVGL setup, and display buffer configuration
 */
void DeviceProvider::Prepare()
{
    log_v("prepare() called");
    log_i("init...");

    // Initialize LovyanGFX display hardware
    init();
    initDMA();      // Enable DMA for fast pixel transfers
    startWrite();   // Begin SPI transaction
    setRotation(0); // Set display orientation

#ifdef WOKWI_EMULATOR
    // WOKWI EMULATOR WORKAROUND:
    // Enable horizontal mirroring to work around wokwi emulator limitation
    // The emulator uses a square ILI9341 display instead of the round GC9A01 target display
    // and renders the image horizontally inverted, also need to correct RGB/BGR order
    writeCommand(0x36); // Memory Access Control register
    writeData(0x48);    // Set MX bit (0x40) + BGR bit (0x08) for horizontal mirror with correct colors
#endif

    // Set default brightness
    setBrightness(SCREEN_DEFAULT_BRIGHTNESS);

    // Initialize LVGL graphics library
    lv_init();
    log_i("Display configuration...");

    // Create LVGL display object with dual buffer configuration
    lv_display_t *display = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_user_data(display, this); // Store DeviceProvider reference
    lv_display_set_flush_cb(display, DeviceProvider::DisplayFlushCallback);
    lv_display_set_buffers(display, lv_buffer_[0], lv_buffer_[1], LV_BUFFER_SIZE, LV_DISPLAY_RENDER_MODE_PARTIAL);

    // Get reference to the active screen for component rendering
    screen_ = lv_screen_active();

    log_i("DeviceProvider hardware initialization completed successfully");
}

/**
 * @brief Get the active LVGL screen for component rendering
 */
lv_obj_t *DeviceProvider::GetScreen() const
{
    return screen_;
}

/**
 * @brief Check if display provider is fully initialized and ready
 */
bool DeviceProvider::IsReady() const
{
    return screen_ != nullptr;
}

// ========== Static Methods ==========

/**
 * @brief LVGL display flush callback for rendering pixels to GC9A01 hardware
 * @details Handles byte swapping, SPI transaction management, and DMA transfer
 */
void DeviceProvider::DisplayFlushCallback(lv_display_t *display, const lv_area_t *area, unsigned char *data)
{
    // Retrieve DeviceProvider instance from LVGL display user data
    DeviceProvider *device_instance = (DeviceProvider *)lv_display_get_user_data(display);

    uint32_t width = lv_area_get_width(area);
    uint32_t height = lv_area_get_height(area);

    // Perform RGB565 byte swapping required by SPI transmission
    lv_draw_sw_rgb565_swap(data, width * height);

    // Manage SPI transaction state for optimal performance
    if (device_instance->getStartCount() == 0)
    {
        device_instance->endWrite();
    }

    // Transfer pixel data to display using DMA for performance
    device_instance->pushImageDMA(area->x1, area->y1, area->x2 - area->x1 + 1, area->y2 - area->y1 + 1,
                                  (uint16_t *)data);

    // Signal LVGL that flush operation is complete
    lv_disp_flush_ready(display);
}