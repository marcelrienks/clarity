#pragma once

#include "interfaces/i_display_provider.h"
#include <lvgl.h>

/**
 * @class LvglDisplayProvider
 * @brief LVGL hardware implementation of display provider interface
 *
 * @details This class provides the concrete implementation of IDisplayProvider
 * interface, handling all LVGL display operations and object creation. It
 * manages LVGL objects, screens, and provides a clean abstraction layer for
 * UI components to interact with the LVGL graphics library.
 *
 * @display_abstraction Bridge between IDisplayProvider interface and LVGL API
 * @dependency_injection Injectable into panels and components for testability
 * @lvgl_integration Direct LVGL API wrapper with safety checks
 *
 * @object_management:
 * - Screen creation and management
 * - LVGL object creation (labels, arcs, scales, images, lines)
 * - Object deletion and cleanup
 * - Event callback registration
 *
 * @screen_lifecycle:
 * - createScreen(): Create new LVGL screen objects
 * - loadScreen(): Make screen active and visible
 * - getMainScreen(): Access to primary display screen
 *
 * @safety_features:
 * - Initialization state tracking
 * - Null pointer checks on object operations
 * - LVGL thread safety compliance
 *
 * @performance_considerations:
 * - Direct LVGL API calls for minimal overhead
 * - Efficient object creation patterns
 * - Memory management delegated to LVGL
 *
 * @context This is the primary display provider implementation used
 * throughout the system. It wraps LVGL operations in a clean interface
 * that can be dependency-injected into UI components for testing.
 */
class LvglDisplayProvider : public IDisplayProvider
{
  private:
    lv_obj_t *main_screen_;

  public:
    explicit LvglDisplayProvider(lv_obj_t *main_screen);

    lv_obj_t *CreateScreen() override;

    lv_obj_t *CreateLabel(lv_obj_t *parent) override;

    lv_obj_t *CreateObject(lv_obj_t *parent) override;

    lv_obj_t *CreateArc(lv_obj_t *parent) override;

    lv_obj_t *CreateScale(lv_obj_t *parent) override;

    lv_obj_t *CreateImage(lv_obj_t *parent) override;
};