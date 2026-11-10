#include "providers/lvgl_display_provider.h"
#include "managers/style_manager.h"
#include <esp32-hal-log.h>

// ========== Constructors and Destructor ==========

/**
 * @brief Constructs the LVGL display provider with the main screen reference
 * @param mainScreen Pointer to the main LVGL screen object
 *
 * Initializes the display provider with a reference to the main screen.
 * This provides abstraction for LVGL display operations and allows for
 * consistent display management across the application.
 */
LvglDisplayProvider::LvglDisplayProvider(lv_obj_t *main_screen) : main_screen_(main_screen)
{
    log_v("LvglDisplayProvider() constructor called");
}

// ========== IDisplayProvider Implementation ==========

/**
 * @brief Creates a new LVGL screen object
 * @return Pointer to the newly created screen object
 *
 * Creates a new top-level LVGL screen for panels to render their UI.
 * The screen is created without a parent, making it a root object
 * suitable for screen transitions.
 */
lv_obj_t *LvglDisplayProvider::CreateScreen()
{
    return lv_obj_create(nullptr);
}

/**
 * @brief Creates an LVGL label widget
 * @param parent Parent object for the label
 * @return Pointer to the newly created label object
 *
 * Creates a text label widget for displaying strings on the UI.
 * The label is created as a child of the specified parent object.
 */
lv_obj_t *LvglDisplayProvider::CreateLabel(lv_obj_t *parent)
{
    return lv_label_create(parent);
}

/**
 * @brief Creates a generic LVGL object
 * @param parent Parent object for the new object
 * @return Pointer to the newly created object
 *
 * Creates a generic LVGL container object. Used as a base for
 * custom widgets and layout containers.
 */
lv_obj_t *LvglDisplayProvider::CreateObject(lv_obj_t *parent)
{
    return lv_obj_create(parent);
}

/**
 * @brief Creates an LVGL arc widget
 * @param parent Parent object for the arc
 * @return Pointer to the newly created arc object
 *
 * Creates an arc widget for displaying circular gauges and progress
 * indicators. Commonly used for sensor value visualization.
 */
lv_obj_t *LvglDisplayProvider::CreateArc(lv_obj_t *parent)
{
    return lv_arc_create(parent);
}

/**
 * @brief Creates an LVGL scale widget
 * @param parent Parent object for the scale
 * @return Pointer to the newly created scale object
 *
 * Creates a scale widget for displaying measurement scales with tick marks.
 * Falls back to arc widget in unit testing environment where scale widget
 * may not be available.
 */
lv_obj_t *LvglDisplayProvider::CreateScale(lv_obj_t *parent)
{
    return lv_scale_create(parent);
}

/**
 * @brief Creates an LVGL image widget
 * @param parent Parent object for the image
 * @return Pointer to the newly created image object
 *
 * Creates an image widget for displaying graphics and icons.
 * Handles API differences between LVGL versions for compatibility.
 */
lv_obj_t *LvglDisplayProvider::CreateImage(lv_obj_t *parent)
{
    return lv_image_create(parent);
}
