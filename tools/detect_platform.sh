#!/usr/bin/env bash

# Detect platform and return appropriate platform string
# Returns "generic" if no special platform detected

# Check for MacBook (Air, Pro, etc.)
if command -v sysctl >/dev/null 2>&1; then
    if sysctl hw.model 2>/dev/null | grep -q "MacBook"; then
        echo "macbookair"
        exit 0
    fi
fi

# No special platform detected
echo "generic"
exit 0
