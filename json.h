#ifndef JSON_H
#define JSON_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum json_type_t {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
};

struct json_value_t;

struct json_entry_t {
    const char *key;
    struct json_value_t *value;
};

struct json_value_t {
    enum json_type_t type;
    size_t count;

    union {
        int boolean;
        double number;
        char *string;
        struct json_value_t **array;
        struct json_entry_t *object;
    } data;
};

struct json_value_t *json_parse(const char *input, size_t length);
void json_free(struct json_value_t *value);
void json_print(const struct json_value_t *value);

#ifdef __cplusplus
}
#endif

#endif
