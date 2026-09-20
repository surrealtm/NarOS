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

if [[ "${1:-}" == "--run" ]]; then
    RUN_QEMU=true
fi

if [[ "${1:-}" == "--debug" ]]; then
    DEBUG_QEMU=true
fi

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

#
# Prepare the work tree
#
mkdir -p ${BUILD_DIR}

#
# Build the kernel
#
KERNEL_DIR=${SOURCE_DIR}kernel/
COMPILER_OPTIONS="-m32 -ffreestanding -fno-stack-protector -fno-pie -fno-pic -c -I${INCLUDE_DIR}"
if [[ ${DEBUG_QEMU} == true ]]; then
    COMPILER_OPTIONS="${COMPILER_OPTIONS} -g -Og"
else
    COMPILER_OPTIONS="${COMPILER_OPTIONS} -O3"
fi
LINKER_OPTIONS="-m elf_i386 -Ttext 0x1000 -e kernel_entry_point"

echo " + Compiling the kernel with options: ${COMPILER_OPTIONS}"

nasm ${KERNEL_DIR}kernel_main.asm -f elf -o ${BUILD_DIR}kernel_main.o
gcc ${COMPILER_OPTIONS} ${KERNEL_DIR}kernel.c -o ${BUILD_DIR}kernel.o
ld ${LINKER_OPTIONS} ${BUILD_DIR}kernel_main.o ${BUILD_DIR}kernel.o -o ${BUILD_DIR}kernel.elf # This elf file is used for debugging
objcopy -O binary ${BUILD_DIR}kernel.elf ${BUILD_DIR}kernel.bin

#
# Build the boot loader
#
BOOT_LOADER_DIR=${SOURCE_DIR}/boot_loader/

echo " + Compiling the boot loader"

nasm ${BOOT_LOADER_DIR}boot_loader.asm -f bin -o ${BUILD_DIR}boot_loader.bin

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
