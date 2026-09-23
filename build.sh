#!/bin/bash

set -e

BUILD_START=$(date +%s%N)

#
# Set up common variables
#
INCLUDE_DIR=include/
SOURCE_DIR=src/
BUILD_DIR=build/
IMAGE_NAME=naros.bin
RUN_QEMU=false
DEBUG_QEMU=false
CHECK_HEADERS=false
C_COMPILER="gcc"

print_help() {
    cat <<EOF
Usage: $0 [OPTIONS]

Build the NarOS kernel image.

Options:
  --run            Run the built image with QEMU
  --debug          Debug the built image with GDB and QEMU
  --check-headers  Check that every header is self-contained
  --gcc            Build using GCC (default)
  --clang          Build using Clang
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
        C_COMPILER="gcc"
        ;;

    --clang)
        C_COMPILER="clang"
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

#
# Log the build type
#
if [[ ${DEBUG_QEMU} == true ]]; then
    echo "Debugging with QEMU..."
elif [[ ${RUN_QEMU} == true ]]; then
    echo "Running with QEMU..."
else
    echo "Making release build..."
fi

echo "Using C Compiler: ${C_COMPILER}"

#
# Prepare the work tree
#
mkdir -p ${BUILD_DIR}

#
# Build the kernel
#
KERNEL_DIR=${SOURCE_DIR}kernel/

KERNEL_ASSEMBLER_OPTIONS="-f elf"
COMPILER_OPTIONS="-std=c99 -pedantic -Wall -Wextra -Werror -m32 -mno-sse -mno-sse2 -mno-mmx -ffreestanding -fno-stack-protector -fno-pie -fno-pic -fno-builtin -I${INCLUDE_DIR}"
if [[ ${DEBUG_QEMU} == true ]]; then
    COMPILER_OPTIONS="${COMPILER_OPTIONS} -g -O0"
else
    COMPILER_OPTIONS="${COMPILER_OPTIONS} -O3"
fi
LINKER_OPTIONS="-m elf_i386 -nostdlib -Ttext 0x1000 -e kernel_main"

echo " + Compiling the kernel with options: ${COMPILER_OPTIONS}"

nasm ${KERNEL_DIR}kernel_main.asm ${KERNEL_ASSEMBLER_OPTIONS} -o ${BUILD_DIR}kernel_main.o
nasm ${KERNEL_DIR}interrupt.asm ${KERNEL_ASSEMBLER_OPTIONS} -o ${BUILD_DIR}interrupt.o
${C_COMPILER} ${COMPILER_OPTIONS} ${KERNEL_DIR}kernel.c -c -o ${BUILD_DIR}kernel.o
ld ${LINKER_OPTIONS} ${BUILD_DIR}kernel_main.o ${BUILD_DIR}interrupt.o ${BUILD_DIR}kernel.o -o ${BUILD_DIR}kernel.elf # This elf file is used for debugging
objcopy -O binary ${BUILD_DIR}kernel.elf ${BUILD_DIR}kernel.bin

#
# Check that each header in the `include` directory is self-contained
#
if [[ ${CHECK_HEADERS} == true ]]; then
    echo " + Checking the individual headers"
    while IFS= read -r -d '' HEADER; do
        printf '#include "%s"\n' "${HEADER}" |
            "${C_COMPILER}" ${COMPILER_OPTIONS} "-I${INCLUDE_DIR}" -x c -fsyntax-only -
    done < <(find "${INCLUDE_DIR}" -type f -name "*.h" -print0)
fi

#
# Build the boot loader
#
BOOT_LOADER_DIR=${SOURCE_DIR}/boot_loader/
BOOT_LOADER_ASSEMBLER_OPTIONS="-f bin"
KERNEL_SIZE_IN_BYTES=$(wc -c <"${BUILD_DIR}kernel.bin")
KERNEL_SECTOR_COUNT=$(((KERNEL_SIZE_IN_BYTES + 511) / 512))

echo " + Compiling the boot loader"
echo "   + Kernel is ${KERNEL_SIZE_IN_BYTES} bytes large and occupies ${KERNEL_SECTOR_COUNT} sectors..."
nasm ${BOOT_LOADER_DIR}boot_loader.asm ${BOOT_LOADER_ASSEMBLER_OPTIONS} "-DKERNEL_SECTOR_COUNT=${KERNEL_SECTOR_COUNT}" -o ${BUILD_DIR}boot_loader.bin

#
# Assemble the final image
#
echo " + Assembling the final image"
cat ${BUILD_DIR}boot_loader.bin ${BUILD_DIR}kernel.bin >${BUILD_DIR}${IMAGE_NAME}

#
# Report Metrics
#
BUILD_END=$(date +%s%N)
BUILD_DURATION=$((BUILD_END - BUILD_START))
echo "Build took $((BUILD_DURATION / 1000000)) ms."

#
# Run the final image using qemu
#
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
