#include "json.h"

#include <stdio.h>

static void print_string(const char *text)
{
    const unsigned char *cursor =(const unsigned char *)text;

    putchar('"');

    while (*cursor != '\0')
    {
        switch (*cursor)
        {
            case '"':
                printf("\\\"");
                break;

            case '\\':
                printf("\\\\");
                break;

            case '\b':
                printf("\\b");
                break;

            case '\f':
                printf("\\f");
                break;

            case '\n':
                printf("\\n");
                break;

            case '\r':
                printf("\\r");
                break;

            case '\t':
                printf("\\t");
                break;

            default:
                if (*cursor < 0x20)
                {
                    printf("\\u%04x", *cursor);
                }
                else
                {
                    putchar(*cursor);
                }

                break;
        }

        cursor++;
    }

    putchar('"');
}

void json_print(const struct json_value_t *value)
{
    if (!value) return;

    switch (value->type)
    {
        case JSON_NULL:
            printf("null");
            break;

        case JSON_BOOL:
            if (value->data.boolean)
            {
                printf("true");
            }
            else
            {
                printf("false");
            }

            break;

        case JSON_NUMBER:
            printf("%g", value->data.number);
            break;

        case JSON_STRING:
            print_string(value->data.string);
            break;

        case JSON_ARRAY:
        {
            size_t i;

            putchar('[');

            for (i = 0; i < value->count; i++)
            {
                if (i > 0) printf(", ");
                json_print(value->data.array[i]);
            }

            putchar(']');
            break;
        }

        case JSON_OBJECT:
        {
            size_t i;

            putchar('{');

            for (i = 0; i < value->count; i++)
            {
                if (i > 0) printf(", ");
                print_string(value->data.object[i].key);

                printf(": ");
                json_print(value->data.object[i].value);
            }

            putchar('}');
            break;
        }
    }
}
