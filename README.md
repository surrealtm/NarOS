# NarOS

Welcome to the **N**ot **A** **R**eal **OS** project.
This repository contains a very simple "Operating System" for educational purposes.

## Getting Started

### Installing dependencies

Before you can build and run the kernel, please install the following system dependencies:

1. `GCC`
2. `NASM`
3. `QEMU` (only required for running the image virtually)

### Building and running

To build the kernel, execute the following script in the root directory of this repository:

```(bash)
./build.sh
```

If you want to directly run the image using `QEMU`, you can pass an argument:

```(bash)
./build.sh --run
```

If you want to flash the image onto a USB drive (to run the image on real hardware), use:

```(bash)
./build.sh
./flash_to_disk
```

The `flash_to_disk` script will query which block device you want to flash the image to.
**Note**: The drive is expected to be using the `FAT` formatting.
