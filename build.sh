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
# Build the boot loader
#
BOOT_LOADER_DIR=${SOURCE_DIR}/boot_loader/
nasm ${BOOT_LOADER_DIR}boot_loader.asm -f bin -o ${BUILD_DIR}boot_loader.bin

#
# Assemble the final image
#
cp ${BUILD_DIR}boot_loader.bin ${BUILD_DIR}${IMAGE_NAME}

#
# Run the final image using qemu
#
if [[ ${RUN_QEMU} == true ]]; then
    qemu-system-i386 -fda ${BUILD_DIR}${IMAGE_NAME}
fi
