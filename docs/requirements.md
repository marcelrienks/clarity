# Product Requirements Document - Clarity Digital Gauge System

**Related Documentation:**
- **[Architecture](architecture.md)** - Complete system architecture and component relationships
- **[Hardware](hardware.md)** - Target hardware specifications and GPIO mappings
- **[Standards](standards.md)** - Coding and naming conventions
- **[Scenarios](scenarios.md)** - Integration test scenarios

## 1. Product Overview

### 1.1 Product Name
Clarity - ESP32-based Digital Automotive Gauge System

### 1.2 Product Description
Clarity is a digital gauge system designed for automotive engine monitoring, built on the ESP32 platform with a 1.28" round display (240x240 GC9A01 driver). The system provides real-time monitoring of engine parameters, security status, and system configuration through an intuitive visual interface.

### 1.3 Target Hardware
- **Microcontroller**: ESP32-WROOM-32 (NodeMCU-32S development board)
- **Display**: 1.28" round LCD with GC9A01 driver (240x240 resolution)
- **Interface**: SPI2_HOST for display communication
- **Memory**: 4MB flash with custom partitioning for OTA updates
- **Input**: Single push button on GPIO 32
- **Sensors**: Digital (GPIO) and Analog (ADC) inputs

## 2. Functional Requirements

### 2.1 Architecture Pattern

#### 2.1.1 MVP (Model-View-Presenter) Implementation
- **Models (Sensors)**: Hardware abstraction for data acquisition with change detection
  - Read GPIO and ADC values with state tracking
  - Provide clean data interfaces with unit conversion
  - Report readings in injected units of measure
  - Implement change detection for trigger system integration
  - Single ownership model - each GPIO has exactly one sensor instance
  
- **Views (Components)**: UI element rendering
  - LVGL-based visual components
  - No business logic
  - Responsible for display scales and visual representation
  - Interrupt-driven panels (Key, Lock) are display-only without sensors
  
- **Presenters (Panels)**: Business logic and orchestration
  - Coordinate components for display
  - Implement lifecycle: init → load → update
  - Handle panel-specific input behaviors via IActionService
  - Interrupt-driven panels receive state from interrupt system, don't read sensors
  - Data panels (Oil) create their own data sensors

#### 2.1.2 Dependency Injection
- All system components use dependency injection for testability and loose coupling
- **Multi-factory architecture**: ProviderFactory creates hardware providers, ManagerFactory creates managers (with provider dependency), PanelFactory creates panels, ComponentFactory creates UI components
- **Interface-based factories**: IProviderFactory interface enables mock provider injection for testing
- **Generic factory support**: Template-based IFactory<T> for type-safe component creation
- **Dependency resolution**: Managers and panels receive injected dependencies via constructor
- **Service container pattern**: Centralized manager initialization with dependency wiring
- **Comprehensive interface design**: 
  - **Factories**: IProviderFactory, IManagerFactory, IFactory<T>
  - **Providers**: IGpioProvider, IDisplayProvider, IDeviceProvider  
  - **Components**: IPanel, IComponent, ISensor, IHandler, IActionService, IManager
  - **Services**: ILogger, IPreferenceService (future extensions)

### 2.2 Panel System

#### 2.2.1 Available Panels with Button Behaviors

1. **Splash Panel**: Startup animation with user control
   - **Auto-transition**: To default panel after animation completion
   - **Short Press**: Skip animation, load default panel immediately
   - **Long Press**: Load config panel for system settings
   - **Display**: Animated startup sequence with branding
   
2. **Oil Panel** (OemOilPanel): Primary monitoring display
   - **Primary Function**: Dual gauge monitoring (pressure/temperature)
   - **Display**: 240x240px gauges positioned side-by-side with OEM styling
   - **Animation**: Continuous gauge needle animation reflecting sensor data
   - **Units**: Configurable units of measure with delta-based smooth updates
   - **Short Press**: No action (reserved for future functionality)
   - **Long Press**: Load config panel for system configuration
   - **Sensors**: Creates own OilPressureSensor and OilTemperatureSensor internally
   
3. **Key Panel**: Security key status indicator (Display-only)
   - **Visual States**: Green icon (key present) / Red icon (key not present)
   - **Interrupt Source**: Activated by KeyPresentSensor/KeyNotPresentSensor via TriggerHandler
   - **Priority**: CRITICAL - overrides all other panels when active
   - **Short Press**: No action (status display only)
   - **Long Press**: Load config panel
   - **Sensors**: None - display-only, receives state from TriggerHandler that owns GPIO sensors
   
4. **Lock Panel**: Vehicle security status indicator (Display-only)
   - **Visual States**: Lock engaged/disengaged indication
   - **Interrupt Source**: Activated by LockSensor via TriggerHandler
   - **Priority**: IMPORTANT - overrides user panels but lower than key panels
   - **Short Press**: No action (status display only)
   - **Long Press**: Load config panel
   - **Sensors**: None - display-only, receives state from TriggerHandler that owns GPIO sensors
   
5. **Error Panel**: System error management with navigation
   - **Primary Function**: Display and manage system errors with user interaction
   - **Display**: Scrollable error list with severity color-coding (Critical/Error/Warning)
   - **Navigation**: Cycle through multiple errors for review
   - **Short Press**: Cycle to next error in queue (marks current error as viewed)
   - **Long Press**: Clear all acknowledged/viewed errors
   - **Auto-Restoration**: Automatically restore previous panel when all errors viewed
   - **Priority**: CRITICAL - highest priority, overrides all other panels
   - **State Management**: Tracks viewed errors and auto-restoration conditions
   
6. **Config Panel**: System configuration with hierarchical navigation
   - **Architecture**: Multi-level state machine with menu hierarchy
   - **Navigation**: Main Menu → Submenus → Option Selection → Application
   - **Short Press**: Navigate through current menu level options
   - **Long Press**: Select highlighted option / Enter submenu / Apply setting
   - **Menu Structure**:
     - **Main Menu**: one entry per section (Button, Display, Oil Press., Oil Temp., Splash Scr., System), then Exit
     - **Display Submenu**: Brightness, Back (no theme setting: the theme follows the lights signal only)
     - **Panel Submenu**: Default panel selection options
   - **Integration**: PreferenceManager (persistence), PanelManager (defaults)
   - **State Persistence**: Menu state maintained during navigation

#### 2.2.2 Panel Switching and Universal Button System

**Automatic Panel Switching**:
- **Priority-Based**: CRITICAL (Key/Error) > IMPORTANT (Lock) > User-driven (Oil)
- **Interrupt-Driven**: GPIO state changes automatically load appropriate panels
- **Restoration**: Return to user-driven panel when all non-maintaining interrupts inactive

**Universal Button System**:
- **IActionService Interface**: All panels implement universal button functions
- **Function Injection**: Panel's short/long press functions injected into QUEUED interrupts
- **Dynamic Updates**: Button interrupts updated when panels switch
- **Context Management**: Panel context passed to button functions for execution

**Panel Lifecycle Management**:
- **Seamless Transitions**: Proper cleanup and initialization during switching
- **Memory Safety**: Panel creation/destruction managed by PanelManager
- **State Preservation**: Theme and configuration state persists across switches

### 2.3 Interrupt System

#### 2.3.1 Critical Constraint
**INTERRUPTS CAN ONLY BE CHECKED DURING SYSTEM IDLE TIME**
- No interrupt processing during LVGL operations
- InterruptManager coordinates TriggerHandler and ActionHandler
- Priority-based coordination across both handlers
- Panel manager automatically manages idle/busy states
- Individual panels can update state during animations

#### 2.3.1b Critical Memory Constraint
**HEAP CORRUPTION PREVENTION REQUIREMENT**
- `std::function` usage causes heap fragmentation on ESP32
- Lambda captures create heap objects leading to system crashes
- All interrupt callbacks MUST use static function pointers
- No dynamic allocations during interrupt processing
- Memory-efficient design is mandatory for system stability

#### 2.3.2 Architecture Overview
**COORDINATED INTERRUPT SYSTEM**

For detailed architecture, see: **[Architecture Document](architecture.md)**

**Key Components**:
1. **InterruptManager**: Coordinates TriggerHandler and ActionHandler with Trigger/Action architecture
2. **TriggerHandler**: GPIO state monitoring with dual activate/deactivate functions and priority-based override logic
3. **ActionHandler**: Button event processing with press duration detection and event queueing
4. **Smart Restoration**: PanelManager tracks last user panel and handles intelligent restoration
5. **Memory Safety**: Direct singleton calls eliminate context parameters (estimated memory usage - see section 3.1.2)
6. **Priority Override**: CRITICAL > IMPORTANT > NORMAL with sophisticated blocking logic

**Critical Processing Model**:
- **Action Evaluation**: Happens on EVERY main loop iteration to detect button events
- **Trigger Evaluation**: Only happens during UI IDLE state
- **All Execution**: Only happens during UI IDLE state (Triggers processed before Actions)

#### 2.3.3 Triggers (State-Based Interrupts)

**Trigger Architecture**:
Triggers monitor GPIO state transitions and execute dual functions based on state changes:

**Change-Based Evaluation**:
- Triggers only activate when sensor states actually change
- Dual functions: activate on HIGH state, deactivate on LOW state
- Each function executes exactly once per state transition  
- No repeated execution for maintained states
- Eliminates performance issues from redundant operations

**Priority and Type-Based Execution**:
- **Activation Logic**: 
  - If no higher-priority active triggers: Execute activateFunc(), set isActive = true
  - If higher-priority trigger active: Only set isActive = true (suppress function execution)
- **Deactivation Logic**:
  - Always execute deactivateFunc(), set isActive = false
  - Find highest-priority active trigger of same TriggerType
  - If found, execute that trigger's activateFunc() for type-based restoration

**Trigger Priority Hierarchy**:
- **CRITICAL Priority**: 
  - Key Present (PANEL type)
  - Key Not Present (PANEL type)
  - Error Occurred (PANEL type)
- **IMPORTANT Priority**:
  - Lock (PANEL type)
- **NORMAL Priority**:
  - Lights (STYLE type)
- **Actions** (separate from triggers):
  - Short Press (no priority - event-based)
  - Long Press (no priority - event-based)

**Trigger Interaction Rules and Scenarios**:

**Scenario 1: Lock Active + Key Present Becomes Active**
- Initial: Lock trigger active, Lock panel showing
- Key Present sensor goes HIGH → Key Present trigger attempts activation
- Key Present (CRITICAL) has higher priority than Lock (IMPORTANT)
- Result: Key Present activateFunc() executes, Key panel displays
- Both triggers remain active (Lock.isActive = true, KeyPresent.isActive = true)

**Scenario 2: Lock Active + Key Present Active, Then Key Present Deactivates**
- Initial: Both Lock and Key Present active, Key panel showing (last activated)
- Key Present sensor goes LOW → Key Present trigger deactivates
- System finds active same-type triggers (PANEL type): Lock is found
- Result: Lock's activateFunc() executes, Lock panel displays
- Final state: Lock.isActive = true, KeyPresent.isActive = false

**Scenario 3: Multiple Triggers Activate in Sequence**
- Step 1: Lights sensor HIGH → Lights trigger activates (NORMAL/STYLE), night theme applied
- Step 2: Lock sensor HIGH → Lock trigger activates (IMPORTANT/PANEL), Lock panel displays
- Step 3: Key Present sensor HIGH → Key Present activates (CRITICAL/PANEL), Key panel displays
- All three triggers remain active with their respective states set

**Scenario 4: Cascading Deactivation with Type-Based Restoration**
- Initial: Key Present, Lock, and Error all active (all PANEL type), Error panel showing
- Error clears → Error trigger deactivates
- System finds highest priority active PANEL trigger: Key Present (CRITICAL)
- Key Present's activateFunc() executes, Key panel displays
- If Key Present then deactivates → Lock's activateFunc() executes (next highest PANEL)
- If Lock then deactivates → No PANEL triggers active, restore to last user panel

**Scenario 5: Different Type Triggers (No Interaction)**
- Initial: Lights trigger active (NORMAL/STYLE), night theme applied
- Lock trigger activates (IMPORTANT/PANEL) → Lock panel displays
- Lights remains active, theme unchanged
- Lock deactivates → No other PANEL triggers, restore to last user panel
- Lights still active, theme remains night mode

**Scenario 6: Priority Suppression During Activation**
- Initial: Key Present active (CRITICAL/PANEL), Key panel showing
- Lock sensor goes HIGH → Lock trigger attempts activation
- Lock (IMPORTANT) has lower priority than active Key Present (CRITICAL)
- Result: Lock.isActive set to true, but activateFunc() NOT executed
- Key panel remains displayed (no visual change)

**Scenario 7: Error Override Behavior**
- Any state + Error occurs → Error trigger activates (CRITICAL/PANEL)
- Error panel displays regardless of other active triggers
- When error clears, system finds next highest priority PANEL trigger
- Executes that trigger's activateFunc() or restores user panel if none

**Scenario 8: Complex Multi-Trigger Interaction**
- Initial: User viewing Oil panel
- Step 1: Lock goes HIGH → Lock activates, Lock panel shows
- Step 2: Key Present goes HIGH → Key Present activates, Key panel shows (Lock still active)
- Step 3: Error occurs → Error activates, Error panel shows (Lock, Key Present still active)
- Step 4: Error clears → Error deactivates, Key Present reactivates (highest PANEL), Key panel shows
- Step 5: Key Present goes LOW → Key Present deactivates, Lock reactivates, Lock panel shows
- Step 6: Lock goes LOW → Lock deactivates, restore to Oil panel (last user panel)

**Scenario 9: Key Present vs Key Not Present (Mutual Exclusion)**
- These are physically exclusive states (same key, different GPIO pins)
- Only one can be active at a time based on physical key position
- Both are CRITICAL priority (Priority::CRITICAL = 2), so no priority conflict
- In practice, one deactivates as the other activates due to physical key state
- Priority system handles this naturally without special "same priority" logic

**Trigger State Truth Table**:

| Active Triggers | Priority Levels | Current Panel | Explanation |
|----------------|-----------------|---------------|-------------|
| None | - | Last User Panel | Default state, user-driven navigation |
| Lock only | IMPORTANT | Lock Panel | Single trigger active |
| Key Present only | CRITICAL | Key Panel | Single trigger active |
| Lock + Key Present | IMP + CRIT | Key Panel | Higher priority (Key) displays |
| Lock + Lights | IMP + NORM | Lock Panel | Different types, PANEL wins |
| Key + Lock + Error | CRIT + IMP + CRIT | Error Panel | Last CRITICAL activation executed |
| All triggers | All levels | Highest Priority Panel | Highest priority trigger's panel displays |

**Deactivation Restoration Chain**:

| Deactivating Trigger | Other Active PANEL Triggers | Result |
|---------------------|----------------------------|---------|
| Error | Key Present, Lock | Key Panel (highest priority) |
| Key Present | Lock | Lock Panel (next in chain) |
| Lock | None | User Panel (fallback) |
| Key Present | Error, Lock | Error Panel (if Error is CRITICAL) |
| Any PANEL | Different type triggers | User Panel (no same-type active) |

**Key Rules Summary**:
1. **Higher Priority Blocks**: Lower priority triggers can't execute activation when higher priority active
2. **Type-Based Restoration**: Only same-type triggers participate in restoration chain
3. **State Persistence**: Triggers remain active even when their functions are suppressed
4. **User Panel Fallback**: When no PANEL triggers active, restore to last user-initiated panel
5. **Type Isolation**: STYLE triggers (Lights) don't affect PANEL trigger restoration chains
6. **Same Priority Resolution**: With current 3-level priority system (CRITICAL=2, IMPORTANT=1, NORMAL=0), same priority conflicts are rare
7. **Activation Execution**: Only one activation function executes at a time (highest priority non-blocked trigger)

**Interrupt Structure**:

For complete interrupt structure details, see: **[Architecture Document](architecture.md#coordinated-interrupt-architecture)**

**Memory Safety Requirements**:
- Static function pointers with void* context parameters
- No heap allocation during interrupt processing
- ESP32-optimized memory patterns

**Key Architecture Elements**:
- **Smart Panel Restoration**: PanelManager tracks last user-driven panel, restores when triggers deactivate
- **Sensor Ownership Model**: TriggerHandler owns GPIO sensors, ActionHandler owns ButtonSensor, Panels create data sensors
- **Priority System**: CRITICAL > IMPORTANT > NORMAL with smart override logic
- **Processing Model**: Actions evaluated continuously, both handlers execute only during UI idle

For complete architectural details including restoration logic, sensor patterns, and processing flows, see:
- **[Architecture Document](architecture.md)** - Complete system architecture
- **[Application Flow Diagram](diagrams/application-flow.md)** - Runtime processing details

**Multi-Trigger Scenarios**: Comprehensive trigger interaction scenarios and priority rules are documented above in the "Trigger Interaction Rules and Scenarios" section.

**Key Sensor Architecture**: See **[Architecture Document](architecture.md#sensor-architecture)** for complete split sensor design and implementation requirements.

#### 2.3.4 Actions (Event-Based Interrupts)

**Action Architecture (v4.0)**: Handles button events with press duration detection and single execution.

For complete implementation details, see: **[Architecture Document](architecture.md#coordinated-interrupt-architecture)**

**Universal Panel Button Functions**: All panels implement short/long press functions called via Actions.

**Universal Panel Button Implementation**:
Every panel implements the IActionService interface providing consistent button behavior across the system.

**Button Hardware Configuration**:
- **GPIO**: Single button on GPIO 32 with INPUT_PULLDOWN configuration
- **Timing**: Short press (50ms-2000ms), Long press (2000ms-5000ms)
- **Debouncing**: 50ms debounce time to prevent false triggers
- **Processing**: Action-based interrupts with event flag processing

**Button Integration Flow (v4.0)**:
1. **Button Press Detection**: ButtonSensor detects press type and duration
2. **Action Interrupt Processing**: ActionHandler receives button events and triggers action interrupts  
3. **Action Evaluation**: Action interrupts evaluate for pending button events
4. **Function Execution**: Current panel's function executed via action interrupt
5. **State Update**: Action interrupt flags cleared after successful execution

For complete input system details, see: **[Hardware Documentation](hardware.md)** - Input System Architecture section

#### 2.3.5 Errors

**Error Architecture (v4.0)**:
Errors are integrated into the unified interrupt system as Triggers with CRITICAL priority:

**Error Trigger Implementation**:
- `error_occurred` trigger registered with CRITICAL priority and dual functions
- ErrorManager integration through trigger evaluation function
- Automatic panel loading when errors are present via activate function
- Restoration handling via deactivate function when errors cleared
- Priority precedence over all other triggers

**Error Trigger Registration Example**:
```cpp
// Error trigger managed by TriggerHandler
Trigger errorTrigger = {
    .id = "error_occurred",
    .priority = Priority::CRITICAL,
    .type = TriggerType::PANEL,
    .activateFunc = []() { PanelManager::Instance().LoadPanel(PanelType::ERROR); },
    .deactivateFunc = []() { PanelManager::Instance().CheckRestoration(); },
    .sensor = &errorSensor,
    .isActive = false
};
```

**Error Levels**:
- **WARNING**: Non-critical issues (auto-dismiss after 10 seconds)
- **ERROR**: Significant issues affecting features
- **CRITICAL**: Issues requiring immediate attention

**Error Service Architecture**:
- **Global ErrorManager**: Singleton service for error collection and management
- **Bounded Error Queue**: Max 10 errors with FIFO replacement when full
- **Error Trigger Integration**: ErrorManager state checked by error trigger evaluation
- **No Try-Catch**: Preserve Arduino crash reporting in main loops

**Error Lifecycle**:
1. **Error Occurrence**: Component reports error to ErrorManager
2. **Trigger Evaluation**: Error trigger evaluates true when errors pending
3. **Panel Loading**: Error trigger activateFunc executes, loads ErrorPanel with CRITICAL priority
4. **User Interaction**: User acknowledges/dismisses errors via panel interactions
5. **Trigger Deactivation**: When no errors remain, trigger sensor state changes to INACTIVE
6. **Panel Restoration**: deactivateFunc calls CheckRestoration() for smart panel restoration

#### 2.3.6 Handler Architecture

**Handler Architecture**:
The system uses separate structures for Triggers (state-based GPIO monitoring) and Actions (event-based button processing), both managed through the IHandler interface.

For complete structure definitions and implementation details, see: **[Architecture Document](architecture.md#coordinated-interrupt-architecture)**

**Handler Implementations**:
- **TriggerHandler**: Implements IHandler, manages Triggers for GPIO state monitoring with priority override logic
- **ActionHandler**: Implements IHandler, manages Actions for button event processing with timing detection
- **Registration**: Separate Trigger and Action struct registration
- **Coordinated Processing**: InterruptManager coordinates both handlers with smart restoration

For complete implementation details, see: **[Architecture Document](architecture.md#coordinated-interrupt-architecture)**

#### 2.3.7 Registration Pattern

Triggers and Actions are registered at startup as separate struct instances:

```cpp
// Register Trigger with TriggerHandler
Trigger keyPresentTrigger = {
    .id = "key_present",
    .priority = Priority::CRITICAL,
    .type = TriggerType::PANEL,
    .activateFunc = []() { PanelManager::Instance().LoadPanel(PanelType::KEY); },
    .deactivateFunc = []() { PanelManager::Instance().CheckRestoration(); },
    .sensor = &keyPresentSensor,
    .isActive = false
};

// Register Action with ActionHandler  
Action shortPressAction = {
    .id = "short_press",
    .executeFunc = []() { PanelManager::Instance().HandleShortPress(); },
    .hasTriggered = false,
    .pressType = ButtonPress::SHORT
};
```

**Registered Interrupts**:
- **Triggers**: key_present, key_not_present, lock_state, lights_state, error_occurred
- **Actions**: short_press, long_press

This approach provides:
- Clear separation between state-based (Triggers) and event-based (Actions) interrupts
- Priority-based override logic for sophisticated panel management  
- Smart restoration through PanelManager with last user panel tracking
- Direct singleton calls eliminate context parameters and reduce memory usage
- Dual functions for Triggers group related activate/deactivate logic together

#### 2.3.8 Evaluation Process

**Polymorphic Handler Processing**:
InterruptManager coordinates both handlers through the unified IHandler interface:

```cpp
void InterruptManager::CheckAllInterrupts() {
    triggerHandler_->Process();   // IHandler* - processes Triggers
    actionHandler_->Process();    // IHandler* - processes Actions  
}
```

**Handler-Specific Processing**:
1. **TriggerHandler::Process()** (GPIO state monitoring):
   - Evaluates Triggers for state changes
   - Executes activate/deactivate functions based on state
   - Applies priority-based override logic
   
2. **ActionHandler::Process()** (button event processing):
   - Evaluates Actions for button events
   - Executes pending action functions
   - Handles timing detection and event flags
   
3. **InterruptManager coordination**:
   - Ensures Triggers process before Actions when both pending
   - Coordinates smart restoration through PanelManager

**Architecture Benefits**:
- **Polymorphic Processing**: InterruptManager treats both handlers identically
- **Clear Separation**: Triggers (state-based) vs Actions (event-based) distinction
- **Processing Order**: Triggers execute before Actions when both pending
- **Testability**: Single IHandler interface simplifies mocking
- **Smart Override**: Priority-based blocking logic for sophisticated panel management

#### 2.3.9 Benefits of Change-Based Architecture
- **Performance Optimization**: Triggers execute once per change instead of repeatedly
- **CPU Efficiency**: Eliminates unnecessary repeated operations during steady states
- **Memory Stability**: No fragmentation from repeated object creation/destruction (memory usage estimates provided below)
- **Predictable Behavior**: Clear semantics for when triggers execute
- **System Stability**: Priority override logic prevents conflicting panel switches
- **Consistent Implementation**: All Triggers follow identical change-detection pattern
- **Easier Debugging**: State changes and priority blocking are explicit and trackable
- **Reduced Power Consumption**: Less CPU usage during idle periods
- **Smart Panel Management**: Override logic provides sophisticated user experience

#### 2.3.10 Performance Requirements
**Target Performance Metrics**:
- **Theme Setting Frequency**: Maximum 2 executions per second (only on actual changes)
- **Trigger Processing Time**: Consistent regardless of maintained trigger states
- **CPU Usage During Idle**: Minimal processing overhead when no state changes occur
- **Memory Usage**: Stable without fragmentation (see section 3.1.2 for detailed estimates)

**Change Detection Implementation Requirements**:

**BaseSensor Pattern (Mandatory)**:
All sensors must inherit from BaseSensor base class for consistent change detection:
```cpp
class BaseSensor {
protected:
    bool initialized_ = false;
    
    template<typename T>
    bool DetectChange(T currentValue, T& previousValue) {
        if (!initialized_) {
            previousValue = currentValue;
            initialized_ = true;
            return false; // No change on first read
        }
        
        bool changed = (currentValue != previousValue);
        previousValue = currentValue;
        return changed;
    }
};
```

**Change Detection Rules**:
1. **First Read Initialization**: No change detected on first sensor read
2. **State Persistence**: Previous state maintained between calls
3. **Atomic Updates**: State update and change detection must be atomic
4. **Thread Safety**: Change detection must be safe for single-threaded ESP32

**Change Detection Performance**:
- **Sensor State Reads**: Execute only during interrupt evaluation cycles
- **Change Comparison**: Simple boolean comparison per sensor
- **Initialization Overhead**: One-time cost during sensor initialization
- **Scalability**: Performance scales linearly with number of sensors, not trigger frequency
- **Phase-Based Processing**: Single evaluation per trigger per cycle prevents corruption

#### 2.3.11 Priority-Based Restoration Implementation
**Blocking Trigger Detection**:
```cpp
// Check if any blocking panel-loading interrupts are active
bool HasBlockingPanelInterruptActive() const {
    for (const auto& interrupt : polledInterrupts_) {
        if (interrupt.active &&
            (interrupt.priority == CRITICAL || interrupt.priority == IMPORTANT) &&
            interrupt.effect == InterruptEffect::LOAD_PANEL &&
            !interrupt.data.panel.maintainWhenInactive) {
            return true;
        }
    }
    return false;
}
```

**Restoration Decision Logic**:
When a panel-loading trigger deactivates:
1. Check if any blocking panel-loading interrupts (CRITICAL/IMPORTANT) are active
2. If yes: Execute the highest priority blocking interrupt's activation function
3. If no: Restore the user-driven panel
4. SET_THEME interrupts (lights) never participate in blocking logic

**Example Flow**:
- Key trigger deactivates while lock trigger is active
- System detects lock (IMPORTANT) blocks restoration
- Lock trigger's execution function is called
- Lock panel is loaded instead of restoring user panel

### 2.4 Development and Debug System

#### 2.4.1 Debug Error Trigger System
**Purpose**: Enable testing and validation of error handling system during development.

**Hardware Configuration**:
- **GPIO 34**: Input-only pin for debug error trigger (development builds only)
- **External Circuit**: Requires external pull-down resistor (GPIO 34 has no internal pull resistors)
- **Activation**: HIGH signal triggers debug error generation

**Debug Error Integration**:
- **Conditional Compilation**: Only active in CLARITY_DEBUG builds
- **Error Generation**: Creates test errors for system validation and error panel testing
- **Interrupt Registration**: debug_error_trigger interrupt with CRITICAL priority
- **Error Types**: Configurable error levels (WARNING/ERROR/CRITICAL) for comprehensive testing

**Development Features**:
- **Error Panel Testing**: Validate error cycling, acknowledgment, and auto-restoration
- **Priority Testing**: Verify error interrupts override other panels correctly
- **System Integration**: Test complete error workflow from detection to restoration
- **Hardware Validation**: Verify GPIO interrupt handling and debouncing

**Production Builds**: Debug error system completely excluded from release builds for memory optimization.

### 2.5 Sensor System

#### 2.5.1 Sensor Architecture

**Sensor Class Hierarchy**:
All sensors inherit from `ISensor` interface and `BaseSensor` for change detection:

```cpp
class ISensor {
public:
    virtual ~ISensor() = default;
    virtual void Init() = 0;
    virtual Reading GetReading() = 0;
};

class BaseSensor {
protected:
    bool initialized_ = false;
    template<typename T> bool DetectChange(T currentValue, T& previousValue);
};
```

#### 2.5.2 Sensor Types and Implementations

**Digital Sensors**:
- **KeyPresentSensor** (GPIO 25): Key present detection
  - Independent class with isolated change tracking
  - Methods: `GetKeyPresentState()`, `HasStateChanged()`
  - Destructor with DetachInterrupt() for cleanup
  
- **KeyNotPresentSensor** (GPIO 26): Key not present detection  
  - Independent class with isolated change tracking
  - Methods: `GetKeyNotPresentState()`, `HasStateChanged()`
  - Destructor with DetachInterrupt() for cleanup
  
- **LockSensor** (GPIO 27): Vehicle lock status
  - Change detection via BaseSensor inheritance
  - Methods: `GetLockState()`, `HasLockStateChanged()`
  - No interrupt attachment - no destructor needed
  
- **LightsSensor** (GPIO 33): Day/night theme switching
  - Change detection for NORMAL priority triggers
  - Methods: `GetLightsState()`, `HasStateChanged()`
  - No interrupt attachment - no destructor needed

- **ButtonSensor** (GPIO 32): User input button for action interrupts
  - Debouncing and timing logic
  - Owned by ActionHandler

**Analog Sensors**: ADC-based continuous measurements
- **OilPressureSensor**: Pressure monitoring with unit conversion
- **OilTemperatureSensor**: Temperature monitoring

#### 2.5.3 Unit Management
- Units of measure injected via SetTargetUnit() method
- Sensors implement GetSupportedUnits() for validation
- Sensors report readings in requested units via ConvertReading()
- Panels control visual scales and ranges independently
- OilPressureSensor supports Bar, PSI, kPa units

### 2.6 Theme System

#### 2.6.1 Available Themes
- **Day Theme**: White/bright colors for daylight visibility
- **Night Theme**: Red/dark colors for night vision

#### 2.6.2 Theme Switching
- Driven only by the dash-light signal: lights on is Night, lights off is Day. There is no manual theme setting
- Instant switching without panel reload
- The error panel temporarily applies the Error theme; the lights-driven theme returns afterwards

## 3. Non-Functional Requirements

### 3.1 Memory Management Requirements

#### 3.1.1 ESP32 Memory Constraints
**Critical Limitation**: ESP32-WROOM-32 has 320KB total RAM, with ~250KB available after system overhead and OTA partitioning
- All architecture decisions must account for memory constraints
- Heap fragmentation prevention is mandatory
- Dynamic allocation minimization required
- Memory-efficient patterns must be used throughout

#### 3.1.2 Memory Usage Analysis (Post-Optimization Results)

**✅ Optimization Completed**: All 10 optimization steps implemented with measured results

**Flash Memory Optimization Results**:
- **Total Flash Reduction**: 30.7KB saved across all optimizations
- **Error Handling**: 15KB saved (largest single optimization - fixed-size buffers)
- **Hardware Providers**: 3KB saved (GPIO hot path optimization)
- **Component System**: 2.5KB saved (hot path logging removal)
- **Sensor Architecture**: 2.1KB saved (eliminated double reads, logging optimization)
- **Panel System**: 1.8KB saved (animation state optimization)
- **Other optimizations**: 6.3KB combined savings

**Memory Pattern Improvements**:
- **Zero hot path allocations**: Eliminated `std::string` and `std::to_string` in critical paths
- **Heap fragmentation prevention**: Fixed-size char buffers replace dynamic strings
- **String lifetime safety**: Async operation fixes prevent use-after-free crashes
- **Stack allocation preference**: Moved temporary data from heap to stack

**Performance Critical Path Results**:
- **Main Loop Optimization**: Removed 5+ log_v calls → ~25% cycle time reduction
- **Interrupt Processing**: Cached UI idle checks → ~40% processing improvement
- **Sensor Reading**: Fixed double ADC reads → ~60% speed increase
- **Component Rendering**: Hot path logging removal → reduced flash + CPU overhead

**Current Memory Configuration**:
- **ESP32 Available RAM**: 320KB total
- **LVGL Buffer Configuration**: Dual-buffer mode (120KB) - optimized and stable
- **Static Memory Design**: All critical structures use fixed-size arrays
- **Heap Usage**: Minimized through optimization - no critical path allocations
- **String Management**: Hybrid approach - `std::string` for safety, `const char*` for constants

**System Memory Efficiency**:
- **Interrupt System**: Static arrays, zero heap allocation during operation
- **Sensor Architecture**: Template-based change detection, minimal overhead
- **Error Handling**: Fixed 128-byte buffers instead of dynamic strings
- **Factory Pattern**: Optimized dependency injection with minimal overhead

#### 3.1.3 Static Callback Requirements with Centralized Restoration
**Mandatory Pattern**: All interrupt system callbacks must use static function pointers with centralized restoration
- Eliminates `std::function` heap allocation overhead
- Prevents lambda capture heap objects
- Provides predictable memory usage
- Prevents system crashes from heap fragmentation
- **Memory Optimization**: Streamlined callback architecture reduces overhead
- **Centralized Logic**: Restoration handled in InterruptManager reduces callback complexity

#### 3.1.4 LVGL Buffer Optimization
**Memory Management**: LVGL buffers optimized for performance and memory efficiency
**Optimization Requirements**:
- Single buffer mode for memory optimization
- Configurable buffer sizes for memory efficiency
- Dynamic buffer allocation based on panel complexity
- Memory-efficient rendering modes

#### 3.1.5 Pointer Management Strategy
**Required Ownership Pattern**:
- main.cpp owns all sensors via unique_ptr (transfer ownership)
- Panels receive raw pointers (non-owning references)
- Managers receive raw pointers (non-owning references)
- Factory methods return unique_ptr (clear ownership transfer)
- Avoid shared_ptr overhead for memory efficiency

### 3.2 Error Prevention Requirements

#### 3.2.1 Heap Corruption Prevention
**Critical Requirement**: System must prevent heap corruption that causes crashes
- **Root Cause**: `std::function` with lambda captures fragments ESP32 heap
- **Solution**: Static function pointers with context parameters exclusively
- **Enforcement**: Build-time verification of callback patterns
- **Testing**: Memory stress testing during LVGL operations

#### 3.2.2 Change Detection Corruption Prevention
**Problem**: Multiple calls to `HasStateChanged()` corrupt sensor state
**Requirements**:
- Separate change detection from state retrieval functions
- Single evaluation rule: each trigger evaluated once per cycle
- Phase-based processing to prevent multiple evaluations
- BaseSensor pattern for consistent change tracking

#### 3.2.3 Resource Conflict Prevention
**GPIO Resource Management**:
- Each GPIO pin has exactly one sensor instance
- Proper DetachInterrupt() calls in sensor destructors
- No sensor duplication between panels and handlers
- Build-time enforcement of single ownership model

### 3.3 Performance
- Smooth animations at 60 FPS
- Responsive button input (<100ms response time)
- Efficient memory usage within ESP32 constraints
- LVGL buffer optimized for ESP32 memory constraints

### 3.4 Reliability
- Graceful error handling without system crashes
- Automatic recovery from error states
- Hardware watchdog integration
- Core dump partition for debugging
- Heap corruption prevention through static callbacks
- Memory leak prevention through proper resource cleanup

### 3.5 Implementation Constraints

#### 3.5.1 ESP32-Specific Constraints
- **Single-Core Operation**: LVGL operations must not be interrupted by GPIO processing
- **Memory Fragmentation**: Minimize dynamic allocations during runtime
- **Resource Cleanup**: Proper GPIO interrupt detachment in destructors required
- **Timing Constraints**: Interrupt processing during idle time only

#### 3.5.2 Architectural Constraints
- **Sensor Independence**: Split sensor classes prevent initialization conflicts
- **Display-Only Panels**: Trigger panels must not create sensors
- **Single Ownership**: Each GPIO managed by exactly one sensor
- **Change Detection**: BaseSensor pattern mandatory for all sensors

### 3.6 Maintainability
- Modular architecture with clear separation of concerns
- Comprehensive logging system:
  - log_v: Method entry only
  - log_d: Complex activities within methods
  - log_i: Major functional operations
- Consistent coding standards (Google C++ Style Guide adapted)
- Memory-efficient patterns throughout codebase

### 3.7 Extensibility
- Easy addition of new panels
- Pluggable sensor architecture
- Configurable trigger mappings
- Theme system extensibility
- Memory-efficient extension patterns

## 4. Technical Constraints

### 4.1 Platform Limitations
- ESP32 memory constraints
- Single-core operation for reliability
- No dynamic memory allocation in critical paths
- LVGL integration requirements

### 4.2 Development Environment
- PlatformIO build system
- Wokwi emulator for local testing (square display limitation)
- Custom partition scheme for OTA updates
- Build environments: debug-local, debug-upload, release

## 5. User Interface Requirements

### 5.1 Display Specifications
- 240x240 pixel round display
- Full-color RGB support
- SPI interface for fast updates
- Hardware rotation support

### 5.2 Visual Design
- Clean, automotive-inspired interface
- High contrast for visibility
- Smooth gauge animations
- Clear status indicators

### 5.3 User Feedback
- Visual confirmation for all inputs
- Error states clearly indicated
- Loading/transition animations
- Status badges for pending errors

## 6. System Integration

### 6.1 Manager Services
- **PanelManager**: Panel lifecycle, switching, and restoration tracking
- **InterruptManager**: Orchestrates interrupt checking during LVGL idle time
  - **TriggerHandler**: GPIO state monitoring with dual functions and priority override logic
  - **ActionHandler**: Button event processing with timing detection and event flags
- **StyleManager**: Theme management (Day/Night)
- **PreferenceManager**: Persistent settings storage
- **ErrorManager**: Error collection with trigger system integration
- **ProviderFactory**: Implements IProviderFactory, creates all hardware providers
- **ManagerFactory**: Creates all managers with injected IProviderFactory dependency

### 6.2 Service Initialization
- Ordered initialization in main setup
- Dependency resolution
- Graceful degradation on initialization failure

## 7. Security and Safety

### 7.1 Security Features
- Key presence detection
- Lock status monitoring
- No storage of sensitive data
- No network connectivity (air-gapped)

### 7.2 Safety Considerations
- Fail-safe defaults for sensor failures
- Clear error indication
- No critical operations during animations
- Interrupt safety during LVGL operations

## 8. Future Enhancements

### 8.1 Planned Features
- Additional sensor support
- Network connectivity for telemetry
- Mobile app integration
- Advanced configuration options

### 8.2 Extensibility Points
- Plugin architecture for custom panels
- Sensor calibration system
- Theme marketplace
- OTA update mechanism

## 9. Acceptance Criteria

### 9.1 Core Functionality
- All panels load and display correctly
- Trigger system responds within 100ms
- Button input works reliably
- Error handling prevents crashes

### 9.2 User Experience
- Smooth transitions between panels
- Responsive to user input
- Clear visual feedback
- Intuitive navigation

### 9.3 System Stability
- 24-hour continuous operation without crashes
- Graceful handling of sensor disconnection
- Memory usage remains stable
- No LVGL conflicts with interrupt system

