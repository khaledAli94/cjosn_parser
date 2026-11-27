#include <stdio.h>
#include <json.h>
#include <dtree.h>

extern unsigned char json_start[];
extern unsigned char json_end[];

int main(void)
{
    size_t length = (size_t)(json_end - json_start);

    struct json_value_t *root = json_parse((const char *)json_start, length);

    if (root == NULL)
    {
        fprintf(stderr, "JSON parsing failed\n");

        return 1;
    }

    json_print(root);
    // build_dtnodes(root);

    putchar('\n');

    json_free(root);
    return 0;
}