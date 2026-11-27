#include "json.h"

#include <stdlib.h>

void json_free(struct json_value_t *value)
{
    if (value == NULL)
    {
        return;
    }

    switch (value->type)
    {
        case JSON_STRING:
            free(value->data.string);
            break;

        case JSON_ARRAY:
        {
            size_t i;

            for (i = 0; i < value->count; i++)
            {
                json_free(value->data.array[i]);
            }

            free(value->data.array);
            break;
        }

        case JSON_OBJECT:
        {
            size_t i;

            for (i = 0; i < value->count; i++)
            {
                free(value->data.object[i].key);
                json_free(value->data.object[i].value);
            }

            free(value->data.object);
            break;
        }

        case JSON_NULL:
        case JSON_BOOL:
        case JSON_NUMBER:
            break;
    }

    free(value);
}
