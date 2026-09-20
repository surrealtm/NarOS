#!/bin/bash

set -e

IMAGE="build/naros.bin"

if [[ ! -f "${IMAGE}" ]]; then
    echo "The image ${IMAGE} does not exist. Please run $(./build.sh) first."
    exit 1
fi

echo "Available Drives:"
echo
lsblk -d -o NAME,SIZE,MODEL,TRAN
echo

read -rp "Enter the device to write to (e.g. /dev/sdb): " DRIVE

if [[ ! -b "${DRIVE}" ]]; then
    echo "The specified path ${DRIVE} is not a valid block device."
    exit 1
fi

echo
echo "You selected:"
echo ${DRIVE}
echo

read -rp "WARNING: This will erase '${DRIVE}'. Continue? [y/N] " CONFIRM

if [[ "${CONFIRM}" != "y" && "${CONFIRM}" != "Y" ]]; then
    echo "Aborted."
    exit 0
fi

echo
echo "Flashing ${IMAGE} to ${DRIVE}..."

sudo dd if="${IMAGE}" of="${DRIVE}" bs=4M status=progress conv=fsync

echo
echo "Syncing..."
sync

echo "Done. You may now remove the USB drive."
