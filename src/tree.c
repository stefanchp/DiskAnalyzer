#include "tree.h"
#include <stdlib.h>
#include <string.h>

TreeNode *create_node(const char *name, NodeType type) 
{
    TreeNode *n = (TreeNode *)calloc(1, sizeof(TreeNode));
    if (!n) return NULL;

    n->name = strdup(name ? name : "");
    if (!n->name) 
    {
        free(n);
        return NULL;
    }
    n->type = type;
    n->size = 0;
    
    return n;
}

void add_child(TreeNode *parent, TreeNode *child) 
{
    if (!parent || !child) return;
    child->parent = parent;
    child->next = parent->children;
    parent->children = child;
}

void free_tree(TreeNode *root) 
{
    if (!root) return;

    TreeNode *c = root->children;
    while (c) 
    {
        TreeNode *next = c->next;
        free_tree(c);
        c = next;
    }
    free(root->name);
    free(root);
}