#include "file_mount.h"

#define MAX_FILE_MOUNTS 2

typedef struct File_Mount {
    struct Block_Device *physical_device;
} File_Mount;

static File_Mount dummy_file_mount;
static File_Mount file_mounts[MAX_FILE_MOUNTS];
static u32 file_mount_count;

b8 file_mount_is_valid(const File_Mount *file_mount) {
    return file_mount->physical_device != null;
}

File_Mount *file_mount_create(struct Block_Device *device) {
    if (file_mount_count >= MAX_FILE_MOUNTS) {
        return &dummy_file_mount;
    }

    File_Mount *file_mount = &file_mounts[file_mount_count];
    file_mount->physical_device = device;
    ++file_mount_count;
    return file_mount;
}
