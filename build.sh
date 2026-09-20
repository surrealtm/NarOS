#!/bin/bash

set -e

#
# Set up common variables
#
SOURCE_DIR=src/
BUILD_DIR=build/
IMAGE_NAME=naros.bin
RUN_QEMU=false

if [[ "${1:-}" == "--run" ]]; then
    RUN_QEMU=true
fi

#
# Prepare the work tree
#
mkdir -p ${BUILD_DIR}

#
# Build the kernel
#
KERNEL_DIR=${SOURCE_DIR}/kernel/
COMPILER_OPTIONS="-m32 -ffreestanding -fno-pie -fno-pic -c -O3"
LINKER_OPTIONS="-m elf_i386 -Ttext 0x1000 --oformat binary -e kernel_entry_point"
nasm ${KERNEL_DIR}kernel_main.asm -f elf -o ${BUILD_DIR}kernel_main.o
gcc ${COMPILER_OPTIONS} ${KERNEL_DIR}kernel.c -o ${BUILD_DIR}kernel.o
ld ${LINKER_OPTIONS} ${BUILD_DIR}kernel_main.o ${BUILD_DIR}kernel.o -o ${BUILD_DIR}kernel.bin

#
# Build the boot loader
#
BOOT_LOADER_DIR=${SOURCE_DIR}/boot_loader/
nasm ${BOOT_LOADER_DIR}boot_loader.asm -f bin -o ${BUILD_DIR}boot_loader.bin

#
# Assemble the final image
#
cat ${BUILD_DIR}boot_loader.bin ${BUILD_DIR}kernel.bin >${BUILD_DIR}${IMAGE_NAME}

#
# Run the final image using qemu
#
if [[ ${RUN_QEMU} == true ]]; then
    qemu-system-i386 -drive format=raw,file=${BUILD_DIR}${IMAGE_NAME}
fi
