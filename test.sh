#!/bin/bash

set -e

TEST_START=$(date +%s%N)

# ----------------------------------------------------------------------------------------------------------------
# Color Codes
# ----------------------------------------------------------------------------------------------------------------
RED='\033[0;31m'
GREEN='\033[0;32m'
RESET='\033[0m'

# ----------------------------------------------------------------------------------------------------------------
# Common Variables
# ----------------------------------------------------------------------------------------------------------------
KERNEL_INCLUDE_DIR=include/
KERNEL_SOURCE_DIR=src/
TEST_SOURCE_DIR=tests/
BUILD_DIR=build/tests/
COMPILER="clang"

# ----------------------------------------------------------------------------------------------------------------
# Prepare Build
# ----------------------------------------------------------------------------------------------------------------
mkdir -p ${BUILD_DIR}

# ----------------------------------------------------------------------------------------------------------------
# Compilation Helpers
# ----------------------------------------------------------------------------------------------------------------
executable_file_path() {
    local source="$1"
    local name="${source//\//_}"
    name="${name}.out"
    printf '%s%s\n' "${BUILD_DIR}" "${name}"
}

invoke_test() {
    local executable="$1"
    if ${executable}; then
        echo "0"
    else
        echo "$?"
    fi
}

# ----------------------------------------------------------------------------------------------------------------
# Compile and execute the tests
# ----------------------------------------------------------------------------------------------------------------
TEST_COMPILER_OPTIONS=(
    -std=c99
    -pedantic
    -Wall
    -Wextra
    -Werror
    -m32
    -mno-sse
    -mno-sse2
    -mno-mmx
    -I${KERNEL_INCLUDE_DIR}
    -I${KERNEL_SOURCE_DIR}
)

TEST_SOURCE_FILES=(
    "hello_world/hello_world.c"
)

echo "Running the test suite..."

for FILEPATH in "${TEST_SOURCE_FILES[@]}"; do
    printf " + Running ${FILEPATH}..."

    EXECUTABLE_FILE=$(executable_file_path ${FILEPATH})
    ${COMPILER} "${TEST_COMPILER_OPTIONS[@]}" ${TEST_SOURCE_DIR}${FILEPATH} -o ${EXECUTABLE_FILE}
    FAILED_TESTS=$(invoke_test ${EXECUTABLE_FILE})

    if [ "${FAILED_TESTS}" -eq 0 ]; then
        printf " ${GREEN}passed${RESET}"
    else
        printf " ${RED}${FAILED_TESTS} failure(s)${RESET}"
    fi

    printf "\n"
done

# ----------------------------------------------------------------------------------------------------------------
# Report Metrics
# ----------------------------------------------------------------------------------------------------------------
TEST_END=$(date +%s%N)
TEST_DURATION=$((BUILD_END - BUILD_START))
echo "Test run took $((TEST_DURATION / 1000000)) ms."
