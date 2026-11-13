# Test Scenarios

**Related Documentation:**
- **[Requirements](requirements.md)** - Complete trigger scenarios and panel behaviors
- **[Architecture](architecture.md)** - System components and interaction patterns
- **[Interrupt Architecture](interrupts.md)** - Trigger priority and processing logic

## Primary Integration Test Scenario

This scenario demonstrates the complete system functionality following a realistic user interaction flow:

### 1. System Startup
- **System Start** → Splash panel loads and animates with day theme
- **Splash Animation Completes** → Oil panel loads automatically
- **Oil Panel Initial State**: Pressure and Temperature scales animate to preset start values (demonstrating gauge functionality)

### 2. Real-time Sensor Interaction
- **Note**: Manual sensor interaction is skipped in automated testing due to limitations with analog potentiometer simulation
- **Pressure/Temperature Readings**: Initial sensor values are logged during startup verification
- **Oil Panel**: Gauges display preset values for automated testing consistency

### 3. Theme Change (Non-Blocking Style Trigger)
- **Lights Trigger Activates** → Night theme applied instantly to Oil Panel
- **Oil Panel**: Continues displaying with night theme without panel reload
- **Lights Trigger**: STYLE type trigger, always maintains and doesn't block restoration

### 4. Panel Loading Trigger (IMPORTANT Priority)
- **Lock Trigger Activates** → Lock panel loads (IMPORTANT priority)
- **Lights Trigger**: Still active, night theme maintained
- **Lock Panel**: Displays lock status with night theme

### 5. Higher Priority Key Trigger Override
- **Key Not Present Trigger Activates** → Key panel loads with red key icon (CRITICAL priority)
- **Key Trigger**: Higher priority than lock, overrides lock panel immediately
- **Key Panel**: Red key icon displayed with night theme

### 6. Same Priority Key State Transition
- **Key Present Trigger Activates** → Key panel reloads with green key icon (CRITICAL priority)
- **Key Panel Update**: Panel reloads to show green key icon (key present state)
- **Key Not Present Trigger**: Automatically deactivates when key present activates

### 7. Key State Reversal and Panel Persistence
- **Note**: This step demonstrates persistence behavior - no active trigger changes occur
- **Key Present Trigger**: Remains active from previous step
- **Key Panel**: Continues displaying green key icon (demonstrates state persistence)
- **Implementation**: Includes GPIO stabilization delay for reliable automation

### 8. Priority-Based Restoration Chain
- **Key Present Trigger Deactivates** → Key Not Present automatically activates (mutually exclusive key states)
- **Key Panel Reloads**: Shows red key icon as Key Not Present takes over
- **Key State Logic**: Both key triggers are mutually exclusive by sensor design - when one deactivates, the other activates
- **Lock Panel**: Remains in background due to Key Not Present having CRITICAL priority

### 9. Error System Integration
- **Debug Error Trigger Activates** → Error panel loads with error reporting (CRITICAL priority)
- **Error Trigger**: Highest priority, overrides all other panels
- **Error Panel**: Displays error interface with night theme
- **Implementation**: Uses button press simulation with proper timing (500ms press duration)

### 10. Error Navigation and Interaction
- **Short Press Action Button** → Navigate through error interface
- **Error Panel**: Handles short press action for error cycling
- **Button Timing**: 800ms press duration within ActionHandler's SHORT_PRESS range (500-1500ms)
- **Error Navigation**: Demonstrates error panel interaction capabilities

### 11. Error Resolution and Priority Restoration
- **Long Press Action Button** → Clear all errors and exit Error panel
- **Error Trigger**: Deactivates after clearing error queue
- **Key Panel**: Loads (Key Not Present trigger still active with CRITICAL priority)
- **Button Timing**: 2000ms press duration ensures LONG_PRESS detection (>1500ms)

### 12. Complete Trigger Chain Restoration
- **Complex Restoration Sequence**: Multi-step process to clear all active triggers
- **Step 1**: Activate Key Present to clear Key Not Present (mutually exclusive states)
- **Step 2**: Deactivate Key Present, then deactivate Lock trigger
- **Step 3**: Key Not Present automatically reactivates (sensor design), requires explicit deactivation
- **Final Result**: Oil panel loads when no triggers remain active
- **Implementation**: Includes GPIO stabilization delays and restoration logic verification

### 13. Theme Restoration to Day Mode
- **Lights Trigger Deactivates** → Day theme applied to Oil Panel
- **Oil Panel**: Continues displaying with day theme without panel reload
- **Theme Change**: Instant theme transition, no interruption to panel operation

### 14. Configuration Access via Button Action
- **Long Press Action Button** → Config panel loads with main preference settings
- **Config Panel**: Main menu displayed with day theme, showing available options
- **Panel Transition**: Oil panel → Config panel via universal long press function

### 15. Configuration Navigation to the Display section
- **Short Press Repeatedly** → Navigate through the main menu: Button, Display, Oil Press., Oil Temp., Splash Scr., System, Exit
- **Display**: Option highlighted when navigation reaches it

### 16. Display section access
- **Long Press Action Button** → Display section menu displayed
- **Config Panel**: Lists Brightness and Back. There is no theme setting: the theme follows the lights signal only (Q-05)

### 17. Theme follows the lights signal, also inside Config
- **Lights Trigger Deactivates while Config is open** → Day theme applied
- **Lights Trigger Activates** → Night theme applied
- **Config Panel**: Keeps its own layout; only the palette changes

### 18. (no longer used)
Former theme selection and application steps. The theme is not configurable.

### 19. Configuration Exit Navigation
- **Short Press Repeatedly** → Navigate through options until Exit reached
- **Config Panel**: Cycles through main menu options until Exit option highlighted
- **Exit Option**: Final option in main configuration menu

### 20. Return to Oil Panel with Applied Settings
- **Long Press Action Button** → Oil panel loads, themed by the lights signal (night while the lights are on)
- **Config Exit**: Executes exit function, loads Oil panel as default user panel
- **Oil Panel Final State**: Displays with the current lights-driven theme, Pressure and Temperature scales animate to preset values

## Individual Scenarios

### Startup
- Default: Splash (day theme) → Oil panel
- With sensors: Needles animate to sensor values
- With interrupts: Skip oil panel, load interrupt-driven panel
- With interrupts: Set theme before loading any panels

### Key Interrupts
- **Present**: Green icon, CRITICAL priority over oil panel
- **Not Present**: Red icon, CRITICAL priority over oil panel

### Lock Interrupt
- Takes IMPORTANT priority over oil panel
- Lower priority than key interrupts

### Theme Switching
- Light sensor controls day/night theme
- No panel reload on theme change
- Persists across panel switches
- Always maintains (never blocks restoration)

## Testing Notes
- **Panel priorities**: Key (CRITICAL) > Lock (IMPORTANT) > Oil (user-driven)
- **Theme changes**: Instant (no reload), always allow restoration
- **Button interrupts**: Universal functions work with all panels
- **Error handling**: CRITICAL priority, overrides all other panels
- **Restoration logic**: Smart restoration with priority-based override
- **Invalid states**: Handled gracefully with logging