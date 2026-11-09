#!/bin/bash

# Clarity Wokwi Test Automation
# Cross-platform automated integration testing for WSL2, macOS, and Linux
# Uses push button automation scenarios for complete automation without manual interaction

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
TIMEOUT=120

# Cross-platform detection
detect_platform() {
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        if grep -qEi "(Microsoft|WSL)" /proc/version 2>/dev/null; then
            echo "wsl2"
        else
            echo "linux"
        fi
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        echo "macos"
    else
        echo "unknown"
    fi
}

PLATFORM=$(detect_platform)

# Platform-specific wokwi-cli detection
if [[ "$PLATFORM" == "wsl2" ]]; then
    # WSL2: Check for wokwi-cli in common locations
    WOKWI_CLI_CANDIDATES=(
        "/home/$(whoami)/bin/wokwi-cli"
        "/usr/local/bin/wokwi-cli" 
        "wokwi-cli"
    )
elif [[ "$PLATFORM" == "macos" ]]; then
    # macOS: Check for wokwi-cli in common locations
    WOKWI_CLI_CANDIDATES=(
        "/Users/$(whoami)/bin/wokwi-cli"
        "/Users/$(whoami)/.wokwi/bin/wokwi-cli"
        "/usr/local/bin/wokwi-cli"
        "wokwi-cli"
    )
else
    # Default fallback
    WOKWI_CLI_CANDIDATES=("wokwi-cli")
fi

# Find working wokwi-cli
WOKWI_CLI=""
for candidate in "${WOKWI_CLI_CANDIDATES[@]}"; do
    if command -v "$candidate" &> /dev/null; then
        WOKWI_CLI="$candidate"
        break
    fi
done

# Use environment variable override if set
WOKWI_CLI=${WOKWI_CLI:-$WOKWI_CLI}

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Helper functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_step() {
    echo -e "${PURPLE}[STEP]${NC} $1"
}

show_usage() {
    echo "Clarity Wokwi Test Automation"
    echo
    echo "USAGE:"
    echo "  $0 [--help|-h|help]"
    echo
    echo "This script runs the fully automated Clarity integration test"
    echo "using push button simulation for complete automation."
    echo
    echo "Cross-platform support: WSL2, macOS, Linux"
    echo "Hardware: ESP32 with 1.28\" round display simulation"
}

cleanup() {
    log_info "Cleaning up..."
    if [[ -n "$WOKWI_PID" ]]; then
        kill $WOKWI_PID 2>/dev/null || true
        wait $WOKWI_PID 2>/dev/null || true
    fi
    
    # No file cleanup needed since we don't swap files anymore
    cd "${SCRIPT_DIR}" 2>/dev/null || true
}

# Set up cleanup trap
trap cleanup EXIT

# Main execution
main() {
    echo -e "${CYAN}========================================${NC}"
    echo -e "${CYAN} Clarity Wokwi Test Automation         ${NC}"
    echo -e "${CYAN}========================================${NC}"
    echo
    echo "Platform: $PLATFORM"
    echo "Wokwi CLI: $WOKWI_CLI"
    echo
    
    # Handle help requests
    if [[ "$1" == "help" || "$1" == "-h" || "$1" == "--help" ]]; then
        show_usage
        exit 0
    fi
    
    log_info "Starting Clarity Automated Integration Test"
    log_info "Test Environment: Wokwi Emulator with Push Button Automation"
    echo
    echo "Hardware mapping:"
    echo "  - trigger_btn1 (Blue):   GPIO 25 - Key Present"
    echo "  - trigger_btn2 (Yellow): GPIO 26 - Key Not Present" 
    echo "  - trigger_btn3 (Orange): GPIO 27 - Lock Engaged"
    echo "  - trigger_btn4 (Purple): GPIO 33 - Lights On"
    echo "  - btn1 (Green):          GPIO 32 - Action Button"
    echo "  - btn2 (Red):            GPIO 34 - Debug Error Button"
    echo
    
    # Verify files exist
    if [[ ! -f "${SCRIPT_DIR}/diagram_automated.json" ]]; then
        log_error "Automated diagram not found: ${SCRIPT_DIR}/diagram_automated.json"
        exit 1
    fi
    
    SCENARIOS=("${SCRIPT_DIR}"/scenario_automated_*.yaml)
    if [[ ! -f "${SCENARIOS[0]}" ]]; then
        log_error "No automation scenarios found: ${SCRIPT_DIR}/scenario_automated_*.yaml"
        exit 1
    fi
    
    # Check that we have either wokwi.toml or wokwi_automated.toml for base config
    if [[ ! -f "${SCRIPT_DIR}/wokwi.toml" ]] && [[ ! -f "${SCRIPT_DIR}/wokwi_automated.toml" ]]; then
        log_error "No Wokwi configuration found (wokwi.toml or wokwi_automated.toml)"
        exit 1
    fi
    
    if [[ ! -f "${PROJECT_ROOT}/.pio/build/test-wokwi/firmware.bin" ]]; then
        log_error "Firmware not built. Run: pio run -e test-wokwi"
        exit 1
    fi

    # Scenarios named *_release.yaml run on the test-wokwi-release firmware
    for scenario in "${SCENARIOS[@]}"; do
        if [[ "$scenario" == *_release.yaml ]] && [[ ! -f "${PROJECT_ROOT}/.pio/build/test-wokwi-release/firmware.bin" ]]; then
            log_error "Firmware not built for $(basename "$scenario"). Run: pio run -e test-wokwi-release"
            exit 1
        fi
    done
    
    # Check if wokwi-cli is available
    if ! command -v $WOKWI_CLI &> /dev/null; then
        log_error "wokwi-cli not found. Please install Wokwi CLI"
        log_info "Visit: https://github.com/wokwi/wokwi-cli"
        exit 1
    fi
    
    log_step "Using automated diagram with push buttons instead of DIP switches"
    log_step "Running fully automated test scenario..."
    
    # Change to test directory for relative paths in wokwi.toml
    cd "${SCRIPT_DIR}"
    
    # Run the automated test with explicit parameters (no file swapping needed)
    # Wokwi caps a session at 5 minutes of wall-clock time, so the scenario is split into
    # parts that each boot the firmware fresh. Every part must pass.
    local failed=()
    for scenario in "${SCENARIOS[@]}"; do
        local name
        name="$(basename "$scenario")"
        local elf_path="${PROJECT_ROOT}/.pio/build/test-wokwi/firmware.bin"
        if [[ "$name" == *_release.yaml ]]; then
            elf_path="${PROJECT_ROOT}/.pio/build/test-wokwi-release/firmware.bin"
        fi
        log_info "Executing: $WOKWI_CLI --elf $elf_path --diagram-file diagram_automated.json --scenario $name --timeout ${TIMEOUT}000"
        local started=$SECONDS
        if $WOKWI_CLI --elf "$elf_path" --diagram-file diagram_automated.json --scenario "$name" --timeout ${TIMEOUT}000; then
            log_success "$name passed in $((SECONDS - started))s"
        else
            log_error "$name FAILED after $((SECONDS - started))s"
            failed+=("$name")
        fi
    done

    if [[ ${#failed[@]} -eq 0 ]]; then
        log_success "✅ Automated integration test PASSED!"
        log_success "All trigger systems validated successfully"
        echo
        log_info "Test Summary:"
        echo "  ✅ Key Present trigger (GPIO 25)"
        echo "  ✅ Key Not Present trigger (GPIO 26)" 
        echo "  ✅ Lock Engaged trigger (GPIO 27)"
        echo "  ✅ Lights On trigger (GPIO 33)"
        echo "  ✅ Short press action button"
        echo "  ✅ Long press action button"
        echo "  ✅ Error trigger button"
        echo "  ✅ System restoration"
        echo
        log_success "🎉 Full automation achieved! No manual interaction required."
        exit 0
    else
        log_error "❌ Automated integration test FAILED! Failed scenarios: ${failed[*]}"
        log_warning "Check the automation scenario output above for details"
        exit 1
    fi
}

main "$@"