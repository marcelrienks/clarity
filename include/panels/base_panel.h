#pragma once

#include "definitions/types.h"
#include "interfaces/i_display_provider.h"
#include "interfaces/i_gpio_provider.h"
#include "interfaces/i_panel.h"
#include "interfaces/i_panel_manager.h"
#include "interfaces/i_style_manager.h"
/**
 * @class BasePanel
 * @brief Base class providing common panel functionality and lifecycle management
 *
 * @details This class eliminates 200+ lines of duplicated boilerplate code across
 * all panels by centralizing common patterns:
 * - Constructor with standard dependency injection
 * - Screen creation and destruction
 * - Provider validation and error handling
 * - Theme application and styling
 * - LVGL event callbacks and lifecycle
 * - Manager service injection
 * - Standard logging patterns
 *
 * @design_pattern Template Method Pattern - defines common algorithms, derived classes implement specifics
 * @simplification_impact Reduces panel implementations by 60-80% for simple panels
 * @memory_management Handles screen lifecycle and component cleanup consistently
 *
 * @virtual_methods_to_implement:
 * - CreateContent(): Create and render panel-specific UI components
 * - UpdateContent(): Update components with fresh data
 * - GetPanelName(): Return panel name constant for logging/identification
 *
 * @optional_overrides:
 * - HandleShortPress(): Panel-specific short button press behavior
 * - HandleLongPress(): Panel-specific long button press behavior
 * - PostLoad(): Additional setup after content creation
 * - CustomInit(): Additional initialization before screen creation
 *
 * @example
 * @code
 * class KeyPanel : public BasePanel {
 * protected:
 *     void CreateContent() override {
 *         key_component_ = componentFactory_->CreateKeyComponent(style_manager_);
 *         key_component_->Render(screen_, center_location_, display_provider_);
 *     }
 *
 *     void UpdateContent() override {
 *         // Update key status if needed
 *     }
 *
 *     const char* GetPanelName() const override { return panel_names::KEY; }
 * };
 * @endcode
 */
class BasePanel : public IPanel
{
  public:
    // ========== Constructors and Destructor ==========
    /**
     * @brief Construct BasePanel with required service dependencies
     * @param gpio GPIO provider for hardware access
     * @param display Display provider for screen management
     * @param styleManager Style service for theming
     */
    BasePanel(IGpioProvider *gpio, IDisplayProvider *display, IStyleManager *style_manager,
              IPanelManager *panel_manager = nullptr);
    BasePanel(const BasePanel &) = delete;
    BasePanel &operator=(const BasePanel &) = delete;
    /**
     * @brief Virtual destructor ensuring proper cleanup of LVGL screen
     */
    virtual ~BasePanel();

    // ========== Public Interface Methods ==========
    /**
     * @brief Initialize panel - validate providers and setup screen
     * @details Template method - calls CustomInit() for derived class setup
     */
    void Init() final;

    /**
     * @brief Load panel content and display on screen
     * @details Template method - calls CreateContent() and PostLoad() hooks
     */
    void Load() final;

    /**
     * @brief Update panel with fresh data
     * @details Template method - calls UpdateContent() and resets UI state
     */
    void Update() final;

    /**
     * @brief Inject panel and style service dependencies
     * @param panelManager Panel management service
     * @param styleManager Style service for theming
     */

    /**
     * @brief Get function pointer for short button press handling
     * @return Pointer to static wrapper function
     */
    void (*GetShortPressFunction())(void *panel_context) final;

    /**
     * @brief Get function pointer for long button press handling
     * @return Pointer to static wrapper function
     */
    void (*GetLongPressFunction())(void *panel_context) final;

    /**
     * @brief Get panel instance pointer for button handler context
     * @return Pointer to this panel instance
     */
    // Old GetPanelContext method removed - no longer needed

  protected:
    // ========== Protected Methods ==========
    /**
     * @brief Create panel-specific UI content - must be implemented by derived classes
     * @details Called during Load() to create and render panel components
     */
    virtual void CreateContent() = 0;

    /**
     * @brief Update panel content with fresh data - must be implemented by derived classes
     * @details Called during Update() to refresh component data
     */
    virtual void UpdateContent() = 0;

    /**
     * @brief Get panel name constant for logging and identification
     * @return Panel name constant (e.g., panel_names::OIL)
     */
    virtual const char *GetPanelName() const = 0;

    /**
     * @brief Optional custom initialization hook for derived classes
     * @details Called during Init() before screen setup
     */
    virtual void CustomInit()
    {
    }

    /**
     * @brief Optional post-load setup hook for derived classes
     * @details Called during Load() after CreateContent()
     */
    virtual void PostLoad()
    {
    }

    /**
     * @brief Handle short button press - override for panel-specific behavior
     * @details Default implementation does nothing
     */
    virtual void HandleShortPress()
    {
    }

    /**
     * @brief Handle long button press - override for panel-specific behavior
     * @details Default implementation does nothing
     */
    virtual void HandleLongPress()
    {
    }

    // ========== Protected Data Members ==========
    IGpioProvider *gpio_provider_;
    IDisplayProvider *display_provider_;
    IStyleManager *style_manager_;
    IPanelManager *panel_manager_;
    ComponentLocation center_location_;
    lv_obj_t *screen_ = nullptr;

  private:
    // ========== Static Methods ==========
    /**
     * @brief LVGL callback for screen load completion
     * @param event LVGL event object containing panel context
     */
    static void ShowPanelCompletionCallback(lv_event_t *event);

    /**
     * @brief Static wrapper for short button press handling
     * @param panelContext Pointer to BasePanel instance
     */
    static void BasePanelShortPress(void *panel_context);

    /**
     * @brief Static wrapper for long button press handling
     * @param panelContext Pointer to BasePanel instance
     */
    static void BasePanelLongPress(void *panel_context);

    // ========== Private Methods ==========
    /**
     * @brief Validate required providers are non-null
     * @details Reports critical error if display or GPIO provider missing
     */
    void ValidateProviders();

    /**
     * @brief Create LVGL screen and apply initial theme
     */
    void SetupScreen();

    /**
     * @brief Load screen and ensure current theme is applied
     */
    void ApplyThemeAndLoadScreen();
};