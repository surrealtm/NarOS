#pragma once

#include "base.h"

struct File_Node;

/**
 * Creates a new file node under the given path. If the path already exists, the
 * file is truncated. A pointer to the created file is returned.
 * Use `os_file_exists` to verify that the file creation has succeeded.
 */
struct File_Node *os_file_create(Str8 path);

/**
 * Returns a pointer to a file node for the given path.
 * If the path does not exist currently, a pointer to a sentinel node is returned.
 * Use `os_file_exists` to determine the status of the file.
 */
struct File_Node *os_file_find(Str8 path);

/**
 * Returns true if the node refers to a file that actually exists in the current file
 * system.
 */
b8 os_file_exists(const struct File_Node *node);
