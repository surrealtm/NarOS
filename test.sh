#!/bin/bash

set -e

TEST_START=$(date +%s%N)

# ----------------------------------------------------------------------------------------------------------------
# Common Variables
# ----------------------------------------------------------------------------------------------------------------
KERNEL_INCLUDE_DIR=include/
KERNEL_SOURCE_DIR=src/
TEST_SOURCE_DIR=tests/
TEST_BUILD_DIR=build/tests/
COMPILER="clang"
KERNEL_LIBRARY=${TEST_BUILD_DIR}libkernel.a

# ----------------------------------------------------------------------------------------------------------------
# Prepare Build
# ----------------------------------------------------------------------------------------------------------------
mkdir -p ${TEST_BUILD_DIR}

# ----------------------------------------------------------------------------------------------------------------
# Compilation Helpers
# ----------------------------------------------------------------------------------------------------------------
EXIT_CODE=0

binary_file_path() {
    local source="$1"
    local name="${source}"
    name="${name//\//_}"
    name="${name}.o"
    printf '%s%s\n' "${TEST_BUILD_DIR}" "${name}"
}

invoke_test() {
    local executable="$1"
    ${executable} || EXIT_CODE=1 # This little helper prevents the bash script from exiting when a single test fails
}

# ----------------------------------------------------------------------------------------------------------------
# Compile tested parts of the kernel as library
# ----------------------------------------------------------------------------------------------------------------
KERNEL_COMPILER_OPTIONS=(
    -std=c99
    -pedantic
    -Wall
    -Wextra
    -Werror
    -m32
    -mno-sse
    -mno-sse2
    -mno-mmx
    -g
    -O0
    -I${KERNEL_INCLUDE_DIR}
    -I${KERNEL_SOURCE_DIR}
)

KERNEL_C_SOURCE_FILES=(
    "alloc/alloc.c"
    "base/base.c"
)

KERNEL_OBJECT_FILES=""

for FILEPATH in "${KERNEL_C_SOURCE_FILES[@]}"; do
    OBJECT_FILE=$(binary_file_path ${FILEPATH})
    ${COMPILER} ${KERNEL_COMPILER_OPTIONS[@]} ${KERNEL_SOURCE_DIR}${FILEPATH} -c -o ${OBJECT_FILE}
    KERNEL_OBJECT_FILES="${KERNEL_OBJECT_FILES} ${OBJECT_FILE}"
done

ar rcs ${KERNEL_LIBRARY} ${KERNEL_OBJECT_FILES}

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
    -g
    -O0
    -I${KERNEL_INCLUDE_DIR}
    -I${KERNEL_SOURCE_DIR}
    -I${TEST_SOURCE_DIR}
)

TEST_SOURCE_FILES=(
    "hello_world/hello_world.c"
    "alloc/stack.c"
)

TEST_HANDLER_FILE="test.c"

echo "Running the test suite..."

for FILEPATH in "${TEST_SOURCE_FILES[@]}"; do
    EXECUTABLE_FILE=$(binary_file_path ${FILEPATH})
    ${COMPILER} "${TEST_COMPILER_OPTIONS[@]}" ${TEST_SOURCE_DIR}${FILEPATH} ${TEST_SOURCE_DIR}${TEST_HANDLER_FILE} ${KERNEL_LIBRARY} -o ${EXECUTABLE_FILE}
    invoke_test ${EXECUTABLE_FILE}
done

# ----------------------------------------------------------------------------------------------------------------
# Report Metrics
# ----------------------------------------------------------------------------------------------------------------
TEST_END=$(date +%s%N)
TEST_DURATION=$((BUILD_END - BUILD_START))
echo "Test run took $((TEST_DURATION / 1000000)) ms."

exit ${EXIT_CODE}
