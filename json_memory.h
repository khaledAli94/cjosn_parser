#ifndef JSON_MEMORY_H
#define JSON_MEMORY_H

#include "json.h"

#ifdef __cplusplus
extern "C" {
#endif

struct json_value_t *json_memory_new_value(enum json_type_t type);
int json_memory_array_append(struct json_value_t *array, struct json_value_t *item);
int json_memory_object_append(struct json_value_t *object, char *key, struct json_value_t *value);

#ifdef __cplusplus
}
#endif

#endif
