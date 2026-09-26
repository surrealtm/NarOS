#!/bin/bash

set -e

BUILD_START=$(date +%s%N)

# ----------------------------------------------------------------------------------------------------------------
# Common Variables
# ----------------------------------------------------------------------------------------------------------------
INCLUDE_DIR=include/
SOURCE_DIR=src/
BUILD_DIR=build/
IMAGE_NAME=naros.bin
RUN_QEMU=false
DEBUG_QEMU=false
CHECK_HEADERS=false
COMPILER="clang"
ASSEMBLER="nasm"

# ----------------------------------------------------------------------------------------------------------------
# Argument Parsing
# ----------------------------------------------------------------------------------------------------------------
print_help() {
    cat <<EOF
Usage: $0 [OPTIONS]

Build the NarOS kernel image.

Options:
  --run            Run the built image with QEMU
  --debug          Debug the built image with GDB and QEMU
  --check-headers  Check that every header is self-contained
  --gcc            Build using GCC
  --clang          Build using Clang (default)
  --help           Display this help and exit
EOF
}

for ARGUMENT in "$@"; do
    case "${ARGUMENT}" in
    --run)
        RUN_QEMU=true
        ;;

    --debug)
        DEBUG_QEMU=true
        ;;

    --check-headers)
        CHECK_HEADERS=true
        ;;

    --gcc)
        COMPILER="gcc"
        ;;

    --clang)
        COMPILER="clang"
        ;;

    --help)
        print_help
        exit 0
        ;;

    *)
        echo " -- Unknown Build Argument '${ARGUMENT}'" >&2
        echo >&2
        print_help >&2
        exit 1
        ;;
    esac
done

# ----------------------------------------------------------------------------------------------------------------
# Log Build Configuration
# ----------------------------------------------------------------------------------------------------------------
if [[ ${DEBUG_QEMU} == true ]]; then
    echo "Debugging with QEMU..."
elif [[ ${RUN_QEMU} == true ]]; then
    echo "Running with QEMU..."
else
    echo "Making release build..."
fi

echo "Using C Compiler: ${COMPILER}"

# ----------------------------------------------------------------------------------------------------------------
# Prepare Build
# ----------------------------------------------------------------------------------------------------------------
mkdir -p ${BUILD_DIR}

# ----------------------------------------------------------------------------------------------------------------
# Compilation Helpers
# ----------------------------------------------------------------------------------------------------------------
object_file_path() {
    local source="$1"
    local name="${source//\//_}"
    name="${name}.o"
    printf '%s%s\n' "${BUILD_DIR}" "${name}"
}

# ----------------------------------------------------------------------------------------------------------------
# Build the Kernel
# ----------------------------------------------------------------------------------------------------------------
KERNEL_LINKER_OPTIONS="-m elf_i386 -nostdlib -Ttext 0x1000 -e kernel_main"
KERNEL_ASSEMBLER_OPTIONS="-f elf"
KERNEL_COMPILER_OPTIONS="-std=c99 -pedantic -Wall -Wextra -Werror -m32 -mno-sse -mno-sse2 -mno-mmx -ffreestanding -fno-stack-protector -fno-pie -fno-pic -fno-builtin -I${INCLUDE_DIR} -I${SOURCE_DIR}"
if [[ ${DEBUG_QEMU} == true ]]; then
    KERNEL_COMPILER_OPTIONS="${KERNEL_COMPILER_OPTIONS} -g -O0"
else
    KERNEL_COMPILER_OPTIONS="${KERNEL_COMPILER_OPTIONS} -O3"
fi

echo " + Compiling the kernel with options: ${KERNEL_COMPILER_OPTIONS}"

KERNEL_C_SOURCE_FILES=(
    "acpi/acpi.c"
    "base/base.c"
    "ctrl/ctrl.c"
    "display/display.c"
    "entry_point/entry_point.c"
    "input/input.c"
    "interrupt/interrupt.c"
    "math/math.c"
    "port/port.c"
)

KERNEL_ASM_SOURCE_FILES=(
    "entry_point/entry_point.asm"
    "interrupt/interrupt.asm"
)

KERNEL_OBJECT_FILES=""

for FILEPATH in "${KERNEL_ASM_SOURCE_FILES[@]}"; do
    OBJECT_FILE=$(object_file_path ${FILEPATH})
    ${ASSEMBLER} ${KERNEL_ASSEMBLER_OPTIONS} ${SOURCE_DIR}${FILEPATH} -o ${OBJECT_FILE}
    KERNEL_OBJECT_FILES="${KERNEL_OBJECT_FILES} ${OBJECT_FILE}"
done

for FILEPATH in "${KERNEL_C_SOURCE_FILES[@]}"; do
    OBJECT_FILE=$(object_file_path ${FILEPATH})
    ${COMPILER} ${KERNEL_COMPILER_OPTIONS} ${SOURCE_DIR}${FILEPATH} -c -o ${OBJECT_FILE}
    KERNEL_OBJECT_FILES="${KERNEL_OBJECT_FILES} ${OBJECT_FILE}"
done

ld ${KERNEL_LINKER_OPTIONS} ${KERNEL_OBJECT_FILES} -o ${BUILD_DIR}kernel.elf # This elf file is used for debugging
objcopy -O binary ${BUILD_DIR}kernel.elf ${BUILD_DIR}kernel.bin

# ----------------------------------------------------------------------------------------------------------------
# Build the Boot Loader
# ----------------------------------------------------------------------------------------------------------------
BOOT_LOADER_ASSEMBLER_OPTIONS="-f bin"
KERNEL_SIZE_IN_BYTES=$(wc -c <"${BUILD_DIR}kernel.bin")
KERNEL_SECTOR_COUNT=$(((KERNEL_SIZE_IN_BYTES + 511) / 512))

echo " + Compiling the boot loader with options: ${BOOT_LOADER_ASSEMBLER_OPTIONS}"
echo "   + Kernel is ${KERNEL_SIZE_IN_BYTES} bytes large and occupies ${KERNEL_SECTOR_COUNT} sectors..."
${ASSEMBLER} ${SOURCE_DIR}boot_loader/boot_loader.asm ${BOOT_LOADER_ASSEMBLER_OPTIONS} "-DKERNEL_SECTOR_COUNT=${KERNEL_SECTOR_COUNT}" -o ${BUILD_DIR}boot_loader.bin

# ----------------------------------------------------------------------------------------------------------------
# Assemble Final Image
# ----------------------------------------------------------------------------------------------------------------
echo " + Assembling the final image"
cat ${BUILD_DIR}boot_loader.bin ${BUILD_DIR}kernel.bin >${BUILD_DIR}${IMAGE_NAME}

# ----------------------------------------------------------------------------------------------------------------
# Report Metrics
# ----------------------------------------------------------------------------------------------------------------
BUILD_END=$(date +%s%N)
BUILD_DURATION=$((BUILD_END - BUILD_START))
echo "Build took $((BUILD_DURATION / 1000000)) ms."

# ----------------------------------------------------------------------------------------------------------------
# Quality Checks
# ----------------------------------------------------------------------------------------------------------------
if [[ ${CHECK_HEADERS} == true ]]; then
    echo " + Checking the individual headers"
    while IFS= read -r -d '' HEADER; do
        printf '#include "%s"\n' "${HEADER}" |
            "${COMPILER}" ${KERNEL_COMPILER_OPTIONS} "-I${INCLUDE_DIR}" -x c -fsyntax-only -
    done < <(find "${INCLUDE_DIR}" -type f -name "*.h" -print0)
fi

# ----------------------------------------------------------------------------------------------------------------
# Launch QEMU
# ----------------------------------------------------------------------------------------------------------------
if [[ ${DEBUG_QEMU} == true ]]; then
    echo " + Launching QEMU debugging..."
    $TERMINAL -e gdb \
        -ex "set confirm off" \
        -ex "file ${BUILD_DIR}kernel.elf" \
        -ex "target remote localhost:1234" &
    qemu-system-i386 -drive format=raw,file=${BUILD_DIR}${IMAGE_NAME} -S -s
elif [[ ${RUN_QEMU} == true ]]; then
    echo " + Launching QEMU run..."
    qemu-system-i386 -drive format=raw,file=${BUILD_DIR}${IMAGE_NAME}
fi
