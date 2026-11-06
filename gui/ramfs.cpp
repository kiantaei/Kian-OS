// Kian OS GUI - RAM filesystem implementation (fixed node pool + heap data).
#include "ramfs.h"
#include "util.h"

static RNode POOL[128];      // POD static storage, no constructors run
static int POOL_N = 0;
static RNode *ROOT = 0;
static RNode *TRASH = 0;

static RNode *alloc_node() {
    for (int i = 0; i < 128; i++) {
        if (POOL_N < 128) return &POOL[POOL_N++];
        break;
    }
    return 0;
}

static void node_init(RNode *n, const char *name, int type, RNode *parent) {
    int i = 0;
    for (; i < 63 && name[i] != '\0'; i++) n->name[i] = name[i];
    n->name[i] = '\0';
    n->type = type;
    n->parent = parent;
    n->nchild = 0;
    n->data = 0;
    n->size = 0;
    n->cap = 0;
    n->trashed = false;
    for (int k = 0; k < 16; k++) n->children[k] = 0;
}

void ramfs_init() {
    POOL_N = 0;
    ROOT = alloc_node();
    node_init(ROOT, "/", RF_DIR, 0);
    TRASH = alloc_node();
    node_init(TRASH, ".Trash", RF_DIR, ROOT);
    ROOT->children[ROOT->nchild++] = TRASH;

    RNode *docs = ramfs_mkdir(ROOT, "Documents");
    RNode *desk = ramfs_mkdir(ROOT, "Desktop");
    ramfs_mkdir(ROOT, "Downloads");
    ramfs_mkdir(ROOT, "Pictures");
    if (docs) {
        ramfs_create(docs, "readme.txt",
            "Kian OS 0.3.0 'Horizon'\n"
            "Rust kernel + C console + C++ GUI.\n\n"
            "This file lives in the RAM filesystem:\n"
            "it is NOT persistent and is lost on reboot.\n");
    }
    if (desk) {
        ramfs_create(desk, "welcome.txt",
            "Welcome to Kian OS!\n\n"
            "Shortcuts: Win = Start menu, Win+R = Run,\n"
            "Ctrl+Alt+Q = exit to console, ESC closes windows.\n");
    }
}

RNode *ramfs_root() { return ROOT; }
RNode *ramfs_trash() { return TRASH; }

RNode *ramfs_mkdir(RNode *parent, const char *name) {
    if (!parent || parent->type != RF_DIR || parent->nchild >= 16) return 0;
    if (ramfs_find(parent, name)) return 0;
    RNode *n = alloc_node();
    if (!n) return 0;
    node_init(n, name, RF_DIR, parent);
    parent->children[parent->nchild++] = n;
    return n;
}

RNode *ramfs_create(RNode *parent, const char *name, const char *content) {
    if (!parent || parent->type != RF_DIR || parent->nchild >= 16) return 0;
    if (ramfs_find(parent, name)) return 0;
    RNode *n = alloc_node();
    if (!n) return 0;
    node_init(n, name, RF_FILE, parent);
    n->cap = 256;
    n->data = (char *)kian_alloc((size_t)n->cap, 16);
    if (!n->data) return 0;
    n->data[0] = '\0';
    n->size = 0;
    if (content && !ramfs_write(n, content, (int)strlen(content))) return 0;
    parent->children[parent->nchild++] = n;
    return n;
}

RNode *ramfs_find(RNode *dir, const char *name) {
    if (!dir || dir->type != RF_DIR) return 0;
    for (int i = 0; i < dir->nchild; i++) {
        if (strcmp(dir->children[i]->name, name) == 0) return dir->children[i];
    }
    return 0;
}

RNode *ramfs_resolve(const char *path) {
    if (!ROOT || !path || path[0] != '/') return 0;
    RNode *cur = ROOT;
    int i = 0;
    while (path[i] != '\0') {
        while (path[i] == '/') i++;
        if (path[i] == '\0') break;
        char part[64];
        int j = 0;
        while (path[i] != '\0' && path[i] != '/' && j < 63) part[j++] = path[i++];
        part[j] = '\0';
        if (strcmp(part, ".") == 0) continue;
        if (strcmp(part, "..") == 0) {
            if (cur->parent) cur = cur->parent;
            continue;
        }
        cur = ramfs_find(cur, part);
        if (!cur) return 0;
    }
    return cur;
}

static int cmp_node(const void *a, const void *b) {
    // dirs first, then case-insensitive-ish name compare
    RNode *const *na = (RNode *const *)a;
    RNode *const *nb = (RNode *const *)b;
    if ((*na)->type != (*nb)->type) return ((*na)->type == RF_DIR) ? -1 : 1;
    return strcmp((*na)->name, (*nb)->name);
}

static void sort_nodes(RNode **arr, int n) {
    // small insertion sort (qsort pulls in libc)
    for (int i = 1; i < n; i++) {
        RNode *key = arr[i];
        int j = i - 1;
        while (j >= 0 && cmp_node(&arr[j], &key) > 0) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

int ramfs_list(RNode *dir, RNode **out, int max) {
    if (!dir || dir->type != RF_DIR) return 0;
    int n = 0;
    for (int i = 0; i < dir->nchild && n < max; i++) {
        // the Trash node itself shows its (trashed) children
        if (!dir->children[i]->trashed || dir == TRASH) out[n++] = dir->children[i];
    }
    sort_nodes(out, n);
    return n;
}

bool ramfs_write(RNode *f, const char *data, int len) {
    if (!f || f->type != RF_FILE) return false;
    if (len + 1 > f->cap) {
        int nc = f->cap;
        while (nc < len + 1) nc *= 2;
        char *nd = (char *)kian_alloc((size_t)nc, 16);
        if (!nd) return false;
        if (f->data) {
            memcpy(nd, f->data, (size_t)f->size);
            kian_free(f->data);
        }
        f->data = nd;
        f->cap = nc;
    }
    memcpy(f->data, data, (size_t)len);
    f->data[len] = '\0';
    f->size = len;
    return true;
}

bool ramfs_append(RNode *f, const char *s) {
    if (!f || f->type != RF_FILE) return false;
    int len = (int)strlen(s);
    if (f->size + len + 1 > f->cap) {
        // grow then append
        int nc = f->cap;
        while (nc < f->size + len + 1) nc *= 2;
        char *nd = (char *)kian_alloc((size_t)nc, 16);
        if (!nd) return false;
        if (f->data) memcpy(nd, f->data, (size_t)f->size);
        if (f->data) kian_free(f->data);
        f->data = nd;
        f->cap = nc;
    }
    memcpy(f->data + f->size, s, (size_t)len);
    f->size += len;
    f->data[f->size] = '\0';
    return true;
}

const char *ramfs_read(RNode *f) { return (f && f->data) ? f->data : ""; }
int ramfs_size(RNode *f) { return f ? f->size : 0; }

bool ramfs_trash_node(RNode *n) {
    if (!n || !n->parent || n == ROOT || n == TRASH || n->trashed) return false;
    RNode *old = n->parent;
    if (TRASH->nchild >= 16) return ramfs_delete(n); // trash full -> really delete
    for (int i = 0; i < old->nchild; i++) {
        if (old->children[i] == n) {
            for (int j = i; j < old->nchild - 1; j++) old->children[j] = old->children[j + 1];
            old->nchild--;
            break;
        }
    }
    n->parent = TRASH;
    n->trashed = true;
    TRASH->children[TRASH->nchild++] = n;
    return true;
}

bool ramfs_delete(RNode *n) {
    if (!n || n == ROOT || n == TRASH) return false;
    if (n->type == RF_DIR && n->nchild > 0) return false; // must be empty
    if (n->parent) {
        for (int i = 0; i < n->parent->nchild; i++) {
            if (n->parent->children[i] == n) {
                for (int j = i; j < n->parent->nchild - 1; j++) {
                    n->parent->children[j] = n->parent->children[j + 1];
                }
                n->parent->nchild--;
                break;
            }
        }
    }
    if (n->data) {
        kian_free(n->data);
        n->data = 0;
    }
    n->name[0] = '\0';
    return true;
}

bool ramfs_rename(RNode *n, const char *newname) {
    if (!n || !newname || newname[0] == '\0') return false;
    if (n->parent && ramfs_find(n->parent, newname)) return false;
    int i = 0;
    for (; i < 63 && newname[i] != '\0'; i++) n->name[i] = newname[i];
    n->name[i] = '\0';
    return true;
}

const char *ramfs_path(RNode *n, char *out, int max) {
    if (!n || !out || max < 2) return "";
    // build by walking up, then reverse
    char tmp[256];
    int tl = 0;
    RNode *cur = n;
    while (cur && cur != ROOT && tl < 200) {
        int nl = (int)strlen(cur->name);
        if (tl + nl + 2 >= 256) break;
        for (int i = nl - 1; i >= 0; i--) tmp[tl++] = cur->name[i];
        tmp[tl++] = '/';
        cur = cur->parent;
    }
    int ol = 0;
    out[ol++] = '/';
    for (int i = tl - 1; i >= 0 && ol < max - 1; i--) out[ol++] = tmp[i];
    out[ol] = '\0';
    if (ol > 1 && out[ol - 1] == '/') out[ol - 1] = '\0';
    return out;
}

bool ramfs_is_dir(RNode *n) { return n && n->type == RF_DIR; }
