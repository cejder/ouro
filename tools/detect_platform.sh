#!/usr/bin/env bash

# Detect platform and return appropriate platform string
# Returns "generic" if no special platform detected

# Check for MacBook (Air, Pro, etc.)
if command -v system_profiler >/dev/null 2>&1; then
    MODEL_NAME=$(system_profiler SPHardwareDataType 2>/dev/null | grep "Model Name:" | awk -F': ' '{print $2}')
    if echo "$MODEL_NAME" | grep -qi "MacBook"; then
        echo "macbookair"
        exit 0
    fi
fi

# Fallback: check hostname for MacBook
if command -v sysctl >/dev/null 2>&1; then
    if sysctl kern.hostname 2>/dev/null | grep -qi "MacBook"; then
        echo "macbookair"
        exit 0
    fi
fi

# No special platform detected
echo "generic"
exit 0
