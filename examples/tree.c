#include <stdio.h>

#include "../cgc.h"

typedef struct _node {
    int val;
    struct _node *left;
    struct _node *right;
} Node;

Node *node_new(int val) {
    Node *node = (Node *)gcmalloc(sizeof(Node));
    node->val = val;
    node->left = NULL;
    node->right = NULL;
    return node;
}

void dfs(Node *node) {
    if (!node) return;
    printf("val = %d\n", node->val);
    dfs(node->left);
    dfs(node->right);
}

int main() {
    gcinit();

    Node *root = node_new(1);
    root->left = node_new(2);
    root->right = node_new(3);
    root->left->left = node_new(4);
    root->left->right = node_new(5);
    root->right->left = node_new(6);
    root->right->right = node_new(7);

    dfs(root);

    return 0;
}


