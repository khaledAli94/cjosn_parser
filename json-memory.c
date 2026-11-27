#include "json_memory.h"

#include <stdint.h>
#include <stdlib.h>

struct json_value_t *json_memory_new_value(enum json_type_t type)
{
    struct json_value_t *value = calloc(1, sizeof(*value));

    if (!value) return NULL;

    value->type = type;
    return value;
}

int json_memory_array_append(struct json_value_t *array, struct json_value_t *item)
{
    if (!array || !item) return 0;
    if (array->type != JSON_ARRAY) return 0;


    if (array->count >= SIZE_MAX / sizeof(*array->data.array)) return 0;
    
    struct json_value_t **grown = realloc(array->data.array, (array->count + 1)*sizeof(*grown));

    if (!grown) return 0;

    array->data.array = grown;
    array->data.array[array->count] = item;
    array->count++;

    return 1;
}

int json_memory_object_append(struct json_value_t *object, char *key, struct json_value_t *value)
{
    if (!object || !key || !value) return 0;
    if (object->type != JSON_OBJECT) return 0;

    if (object->count >= SIZE_MAX / sizeof(*object->data.object)) return 0;

    struct json_entry_t *grown = realloc(object->data.object, (object->count + 1)*sizeof(*grown));

    if (!grown) return 0;

    object->data.object = grown;
    object->data.object[object->count].key = key;
    object->data.object[object->count].value = value;
    object->count++;

    return 1;
}
