#!/bin/bash

set -e

TEST_START=$(date +%s%N)

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

for FILEPATH in "${TEST_SOURCE_FILES[@]}"; do
    echo " + Testing ${FILEPATH}..."
    EXECUTABLE_FILE=$(executable_file_path ${FILEPATH})
    ${COMPILER} "${TEST_COMPILER_OPTIONS[@]}" ${TEST_SOURCE_DIR}${FILEPATH} -o ${EXECUTABLE_FILE}
    ${EXECUTABLE_FILE}
done
