#include <stdio.h>

extern unsigned char json_start[];
extern unsigned char json_end[];

int main(void)
{
    size_t length = (size_t)(json_end - json_start);

    return 0;
}
