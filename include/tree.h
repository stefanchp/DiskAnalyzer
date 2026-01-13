#ifndef TREE_H
#define TREE_H
#include <stdint.h>

typedef enum {
    NODE_FILE = 0,
    NODE_DIR  = 1
} NodeType;

typedef struct TreeNode {
    char *name;                 
    uint64_t size;             
    NodeType type;
    struct TreeNode *parent;  
    struct TreeNode *children;
    struct TreeNode *next;
} TreeNode;

TreeNode *create_node(const char *name, NodeType type);
void add_child(TreeNode *parent, TreeNode *child);
void free_tree(TreeNode *root);

#endif