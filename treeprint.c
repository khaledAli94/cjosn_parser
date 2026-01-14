#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

extern uint8_t json_start[];
extern uint8_t json_end[];


typedef struct Node {
    const char *name;
    struct Node *left;
    struct Node *right;
} Node;

Node *make(const char *name, Node *left, Node *right) {
    Node *n = malloc(sizeof(Node));
    n->name = name;
    n->left = left;
    n->right = right;
    return n;
}

void print_ascii_children(const Node *node, const char *prefix, bool is_last) {
    if (!node)
        return;

    printf("%s%s%s\n",
           prefix,
           is_last ? "└── " : "├── ",
           node->name);

    char new_prefix[256];
    snprintf(new_prefix, sizeof(new_prefix),
             "%s%s",
             prefix,
             is_last ? "    " : "│   ");

    int count = 0;
    if (node->left)  count++;
    if (node->right) count++;

    if (node->left)
        print_ascii_children(node->left, new_prefix, count == 1);

    if (node->right)
        print_ascii_children(node->right, new_prefix, true);
}

void print_ascii_tree(const Node *root) {
    if (!root)
        return;

    // Print root WITHOUT prefix
    printf("%s\n", root->name);

    // Print children with formatting
    int count = 0;
    if (root->left)  count++;
    if (root->right) count++;

    if (root->left)
        print_ascii_children(root->left, "", count == 1);

    if (root->right)
        print_ascii_children(root->right, "", true);
}

int main(int argc, char *argv[]){
    Node *root =
        make("root",
            make("left",
                make("left.left", NULL, NULL),
                NULL),
            make("right", NULL, NULL));

    print_ascii_tree(root);

    return 0;
}
