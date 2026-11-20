#include "file_node.h"

typedef struct File_Node {
    b8 exists;
} File_Node;

static File_Node null_node = { .exists = false };

File_Node *os_file_create(const Str8 path) {
    (void) path;
    return &null_node;
}

File_Node *os_file_find(const Str8 path) {
    (void) path;
    return &null_node;
}

b8 os_file_exists(const File_Node *node) {
    return node != null && node->exists;
}
