#include <json.h>
#include "json_memory.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

struct json_parser_t {
    const char *input;
    size_t length;
    size_t position;
};

static void parser_skip_whitespace(struct json_parser_t *parser);
static int parser_peek(struct json_parser_t *parser);
static int parser_consume(struct json_parser_t *parser, char expected);
static int parser_consume_literal(struct json_parser_t *parser, const char *literal);
static char *parse_string_contents(struct json_parser_t *parser);
static struct json_value_t *parse_string(struct json_parser_t *parser);
static struct json_value_t *parse_number(struct json_parser_t *parser);
static struct json_value_t *parse_literal(struct json_parser_t *parser);
static struct json_value_t *parse_value(struct json_parser_t *parser);
static struct json_value_t *parse_array(struct json_parser_t *parser);
static struct json_value_t *parse_object(struct json_parser_t *parser);

static void parser_skip_whitespace(struct json_parser_t *parser)
{
    while (parser->position < parser->length)
    {
        char c = parser->input[parser->position];

        if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
        {
            break;
        }

        parser->position++;
    }
}

/* advance */
static int parser_peek(struct json_parser_t *parser)
{
    if (parser->position >= parser->length)
    {
        return -1;
    }

    return (unsigned char)parser->input[parser->position];
}

static int parser_consume(struct json_parser_t *parser, char expected)
{
    if (parser_peek(parser) != expected)
    {
        return 0;
    }

    parser->position++;
    return 1;
}

static int parser_consume_literal(struct json_parser_t *parser, const char *literal)
{
    size_t length = strlen(literal);

    if (parser->position + length > parser->length)
    {
        return 0;
    }

    if (memcmp(parser->input + parser->position, literal, length) != 0)
    {
        return 0;
    }

    parser->position += length;
    return 1;
}

static char *parse_string_contents(struct json_parser_t *parser)
{
    char *buffer = NULL;
    size_t length = 0;
    size_t capacity = 0;

    if (!parser_consume(parser, '"'))
    {
        return NULL;
    }

    while (parser->position < parser->length)
    {
        unsigned char c = (unsigned char)parser->input[parser->position++];

        if (c == '"')
        {
            char *result = realloc(buffer, length + 1);

            if (!result)
            {
                free(buffer);
                return NULL;
            }

            result[length] = '\0';
            return result;
        }

        if (c < 0x20)
        {
            free(buffer);
            return NULL;
        }

        if (c == '\\')
        {
            if (parser->position >= parser->length)
            {
                free(buffer);
                return NULL;
            }

            c = (unsigned char)
                parser->input[parser->position++];

            switch (c)
            {
                case '"':
                    c = '"';
                    break;

                case '\\':
                    c = '\\';
                    break;

                case '/':
                    c = '/';
                    break;

                case 'b':
                    c = '\b';
                    break;

                case 'f':
                    c = '\f';
                    break;

                case 'n':
                    c = '\n';
                    break;

                case 'r':
                    c = '\r';
                    break;

                case 't':
                    c = '\t';
                    break;

                default:
                    free(buffer);
                    return NULL;
            }
        }

        if (length + 1 >= capacity)
        {
            size_t new_capacity;

            if (capacity == 0)
            {
                new_capacity = 32;
            }
            else
            {
                new_capacity = capacity * 2;
            }

            char *grown = realloc(buffer, new_capacity);

            if (grown == NULL)
            {
                free(buffer);
                return NULL;
            }

            buffer = grown;
            capacity = new_capacity;
        }

        buffer[length++] = (char)c;
    }

    free(buffer);
    return NULL;
}

static struct json_value_t *parse_string(struct json_parser_t *parser)
{
    char *text = parse_string_contents(parser);

    if (text == NULL)
    {
        return NULL;
    }

    struct json_value_t *value = json_memory_new_value(JSON_STRING);

    if (value == NULL)
    {
        free(text);
        return NULL;
    }

    value->data.string = text;
    return value;
}

static struct json_value_t *parse_number(struct json_parser_t *parser)
{
    size_t start = parser->position;

    int c = parser_peek(parser);

    if (c == '-')
    {
        parser->position++;

        if (parser->position >= parser->length)
        {
            return NULL;
        }
    }

    c = parser_peek(parser);

    if (c == '0')
    {
        parser->position++;

        c = parser_peek(parser);

        if (c >= '0' && c <= '9')
        {
            return NULL;
        }
    }
    else
    {
        c = parser_peek(parser);

        if (c < '1' || c > '9')
        {
            return NULL;
        }

        while (parser->position < parser->length)
        {
            c = parser_peek(parser);

            if (c < '0' || c > '9')
            {
                break;
            }

            parser->position++;
        }
    }

    c = parser_peek(parser);

    if (c == '.')
    {
        parser->position++;

        c = parser_peek(parser);

        if (c < '0' || c > '9')
        {
            return NULL;
        }

        while (parser->position < parser->length)
        {
            c = parser_peek(parser);

            if (c < '0' || c > '9')
            {
                break;
            }

            parser->position++;
        }
    }

    c = parser_peek(parser);

    if (c == 'e' || c == 'E')
    {
        parser->position++;

        c = parser_peek(parser);

        if (c == '+' || c == '-')
        {
            parser->position++;
        }

        c = parser_peek(parser);

        if (c < '0' || c > '9')
        {
            return NULL;
        }

        while (parser->position < parser->length)
        {
            c = parser_peek(parser);

            if (c < '0' || c > '9')
            {
                break;
            }

            parser->position++;
        }
    }

    char *end = NULL;

    double number = strtod(parser->input + start, &end);

    if (!end) return NULL;

    if (end == parser->input + start)
    {
        return NULL;
    }

    if (end > parser->input + parser->length)
    {
        return NULL;
    }

    parser->position = (size_t)(end - parser->input);

    struct json_value_t *value = json_memory_new_value(JSON_NUMBER);

    if (!value) return NULL;

    value->data.number = number;
    return value;
}

static struct json_value_t *parse_literal(struct json_parser_t *parser)
{
    struct json_value_t *value;

    if (parser_consume_literal(parser, "true"))
    {
        value = json_memory_new_value(JSON_BOOL);

        if (!value) return NULL;

        value->data.boolean = 1;
        return value;
    }

    if (parser_consume_literal(parser, "false"))
    {
        value = json_memory_new_value(JSON_BOOL);

        if (!value) return NULL;

        value->data.boolean = 0;
        return value;
    }

    if (parser_consume_literal(parser, "null"))
    {
        return json_memory_new_value(JSON_NULL);
    }

    return NULL;
}

static struct json_value_t *parse_array(struct json_parser_t *parser)
{
    if (!parser_consume(parser, '['))
    {
        return NULL;
    }

    struct json_value_t *array = json_memory_new_value(JSON_ARRAY);

    if (array == NULL)
    {
        return NULL;
    }

    parser_skip_whitespace(parser);

    if (parser_consume(parser, ']'))
    {
        return array;
    }

    for (;;)
    {
        struct json_value_t *item = parse_value(parser);

        if (item == NULL)
        {
            json_free(array);
            return NULL;
        }

        if (!json_memory_array_append(array, item))
        {
            json_free(item);
            json_free(array);
            return NULL;
        }

        parser_skip_whitespace(parser);

        if (parser_consume(parser, ']'))
        {
            return array;
        }

        if (!parser_consume(parser, ','))
        {
            json_free(array);
            return NULL;
        }

        parser_skip_whitespace(parser);

        if (parser_peek(parser) == ']')
        {
            json_free(array);
            return NULL;
        }
    }
}

static struct json_value_t *parse_object(struct json_parser_t *parser)
{
    if (!parser_consume(parser, '{'))
    {
        return NULL;
    }

    struct json_value_t *object = json_memory_new_value(JSON_OBJECT);

    if (!object) return NULL;

    parser_skip_whitespace(parser);

    if (parser_consume(parser, '}'))
    {
        return object;
    }

    for (;;)
    {
        parser_skip_whitespace(parser);

        char *key = parse_string_contents(parser);

        if (!key)
        {
            json_free(object);
            return NULL;
        }

        parser_skip_whitespace(parser);

        if (!parser_consume(parser, ':'))
        {
            free(key);
            json_free(object);
            return NULL;
        }

        struct json_value_t *value = parse_value(parser);

        if (value == NULL)
        {
            free(key);
            json_free(object);
            return NULL;
        }

        if (!json_memory_object_append(object, key, value))
        {
            free(key);
            json_free(value);
            json_free(object);
            return NULL;
        }

        parser_skip_whitespace(parser);

        if (parser_consume(parser, '}'))
        {
            return object;
        }

        if (!parser_consume(parser, ','))
        {
            json_free(object);
            return NULL;
        }

        parser_skip_whitespace(parser);

        if (parser_peek(parser) == '}')
        {
            json_free(object);
            return NULL;
        }
    }
}

static struct json_value_t *parse_value(struct json_parser_t *parser)
{
    parser_skip_whitespace(parser);

    int c = parser_peek(parser);

    switch (c)
    {
        case '{':
            return parse_object(parser);

        case '[':
            return parse_array(parser);

        case '"':
            return parse_string(parser);

        case '-':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            return parse_number(parser);

        case 't':
        case 'f':
        case 'n':
            return parse_literal(parser);

        default:
            return NULL;
    }
}

struct json_value_t *json_parse(const char *input, size_t length)
{
    if (!input) return NULL;

    struct json_parser_t parser;

    parser.input = input;
    parser.length = length;
    parser.position = 0;

    parser_skip_whitespace(&parser);

    struct json_value_t *root = parse_value(&parser);

    if (!root) return NULL;

    parser_skip_whitespace(&parser);

    if (parser.position != parser.length)
    {
        json_free(root);
        return NULL;
    }

    return root;
}
