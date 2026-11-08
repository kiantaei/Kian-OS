// Kian OS GUI - RAM filesystem: a tree of nodes, pool-allocated.
// HONEST: everything lives in RAM and is LOST on reboot.
#pragma once
#include <kian_abi.h>

enum { RF_FILE = 1, RF_DIR = 2 };

struct RNode {
    char name[64];
    int type;                 // RF_FILE / RF_DIR
    RNode *parent;
    RNode *children[16];
    int nchild;
    char *data;               // file content (kian_alloc'ed), 0 for dirs
    int size, cap;
    bool trashed;             // moved to Trash
};

void ramfs_init();            // pre-fills Documents/readme.txt, Desktop, Downloads + welcome file
RNode *ramfs_root();
RNode *ramfs_trash();

RNode *ramfs_mkdir(RNode *parent, const char *name);        // 0 if full/exists
RNode *ramfs_create(RNode *parent, const char *name, const char *content);
RNode *ramfs_find(RNode *dir, const char *name);            // child by name
RNode *ramfs_resolve(const char *path);                     // absolute, '/'-separated
int    ramfs_list(RNode *dir, RNode **out, int max);        // dirs first, then a..z
bool   ramfs_write(RNode *f, const char *data, int len);    // replace content
bool   ramfs_append(RNode *f, const char *s);
const char *ramfs_read(RNode *f);
int    ramfs_size(RNode *f);
bool   ramfs_trash_node(RNode *n);                          // move into Trash
bool   ramfs_delete(RNode *n);                              // really remove
bool   ramfs_rename(RNode *n, const char *newname);
bool   ramfs_move(RNode *n, RNode *newparent);          // reparent (no cycle)
const char *ramfs_path(RNode *n, char *out, int max);       // absolute path
bool   ramfs_is_dir(RNode *n);
