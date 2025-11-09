#!/usr/bin/env bash
# Script to setup and download dependencies
# Usage: ./setup_deps.sh [--force] /path/to/third_party_folder

FORCE=false
if [ "$1" = "--force" ]; then
    FORCE=true
    shift
fi

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 [--force] /path/to/third_party_folder"
    exit 1
fi

THIRD_PARTY_FOLDER="$1"

mkdir -p "$THIRD_PARTY_FOLDER"
SUCCESS_COLOR="\033[0;32m"
CLEANUP_CMD_COLOR="\033[0;34m"
LOG_COLOR="\033[0;33m"
ERROR_COLOR="\033[0;31m"
NC="\033[0m"

install_library() {
    local name="$1"
    local repo_url="$2"
    local delete_git="$3"
    local cleanup_cmd="$4"

    if [ -d "${THIRD_PARTY_FOLDER}/$name" ] && [ "$(ls -A "${THIRD_PARTY_FOLDER}/$name" 2>/dev/null)" ]; then
        if [ "$FORCE" = false ]; then
            echo -e "${LOG_COLOR}$name already exists, skipping${NC}"
            return 0
        else
            echo -e "${LOG_COLOR}Force flag set, removing existing $name${NC}"
            rm -rf "${THIRD_PARTY_FOLDER}/$name"
        fi
    fi

    git clone --depth 1 "$repo_url" "${THIRD_PARTY_FOLDER}/$name" >/dev/null 2>&1 || {
        echo -e "${ERROR_COLOR}ERROR: Failed to install $name${NC}" >&2
        return 1
    }

    if [[ "$delete_git" == "true" ]]; then
        echo -e "${LOG_COLOR}Deleting .git directory ${THIRD_PARTY_FOLDER}/$name/.git${NC}" >&2
    fi
    rm -rf "${THIRD_PARTY_FOLDER}/$name/.git"

    if [ -n "$cleanup_cmd" ]; then
        echo -e "${LOG_COLOR}Running cleanup command: ${CLEANUP_CMD_COLOR}${cleanup_cmd}${NC}" >&2
        eval "$cleanup_cmd" || {
            echo -e "${ERROR_COLOR}WARNING: Cleanup command failed for $name${NC}" >&2
        }
    fi

    echo -e "${SUCCESS_COLOR}SUCCESS: Installed $name${NC}"
    return 0
}

# name|repo_url|delete_git|cleanup_cmd
declare -a libs=(
    "raylib|https://github.com/raysan5/raylib|true|rm -rf ${THIRD_PARTY_FOLDER}/raylib/examples ${THIRD_PARTY_FOLDER}/raylib/projects"
    "Unity|https://github.com/ThrowTheSwitch/Unity|true|"
    "tinycthread|https://github.com/tinycthread/tinycthread|true|if [[ \$OSTYPE == darwin* ]]; then sed -i '' \"s/cmake_minimum_required(VERSION [0-9.]*)/cmake_minimum_required(VERSION 3.10)/\" \${THIRD_PARTY_FOLDER}/tinycthread/CMakeLists.txt; else sed -i \"s/cmake_minimum_required(VERSION [0-9.]*)/cmake_minimum_required(VERSION 3.10)/\" \${THIRD_PARTY_FOLDER}/tinycthread/CMakeLists.txt; fi"
    "glm|https://github.com/g-truc/glm|true|"
    "mimalloc|https://github.com/microsoft/mimalloc|true|"
)

if [ ${#libs[@]} -eq 0 ]; then
    echo -e "${SUCCESS_COLOR}No dependencies to install${NC}"
    exit 0
fi

failed_installs=()

for lib in "${libs[@]}"; do
    IFS='|' read -r name repo_url delete_git cleanup_cmd <<< "$lib"
    if ! install_library "$name" "$repo_url" "$delete_git" "$cleanup_cmd"; then
        failed_installs+=("$name")
    fi
done

if [ ${#failed_installs[@]} -gt 0 ]; then
    echo -e "${ERROR_COLOR}Some installations failed: ${failed_installs[*]}${NC}"
    exit 1
else
    echo -e "${SUCCESS_COLOR}All third-party libraries successfully installed${NC}"
    exit 0
fi
