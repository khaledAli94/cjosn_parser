#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

extern unsigned char json_start[];
extern unsigned char json_end[];

enum json_type_t{
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
};

// Forward declaration
struct json_value_t;

// Optimized Object Entry: Key (string) + Value (struct)
// This ensures Keys and Values are neighbors in cache.
struct json_entry_t{
    const char* key;
    size_t key_len;
    struct json_value_t* value; // Pointer to value to allow recursive structures in flat array
};

struct json_value_t {
    enum json_type_t type;
    union {
        int boolean;
        double number;
        struct {
            const char* val;
            size_t len;
        } string;
        struct {
            struct json_value_t* items; // Contiguous array of children
            size_t count;
        } array;
        struct {
            struct json_entry_t* entries;      // Contiguous array of key-value pairs
            size_t count;
        } object;
    } data;
};

// Internal allocator state
struct json_arena_t{
    struct json_value_t* nodes;      // Pointer to available node slots
    struct json_entry_t* entries;    // Pointer to available entry slots
    char* strings;         // Pointer to available string buffer
    
    // Counters to ensure we don't overflow (for debugging/safety)
    size_t nodes_rem;
    size_t entries_rem;
    size_t strings_rem;
};

struct scan_status_t{
    size_t nodes;
    size_t entries;
    size_t string_bytes;
    int err_line;
    int err_col;
    const char* err_msg;
};

static void skip_whitespace(const char **cursor, const char* end) {
    while (*cursor < end) {
        char c = **cursor;
        if (isspace(c)) {
            (*cursor)++;
        } else if (c == '/' && *cursor + 1 < end) {
            if (*(*cursor + 1) == '/') { // Single line
                *cursor += 2;
                while (*cursor < end && **cursor != '\n') (*cursor)++;
            } else if (*(*cursor + 1) == '*') { // Multi line
                *cursor += 2;
                while (*cursor + 1 < end && !(**cursor == '*' && *(*cursor + 1) == '/')) {
                    (*cursor)++;
                }
                if (*cursor + 1 < end) *cursor += 2;
            } else {
                return; // Not a comment
            }
        } else {
            return;
        }
    }
}

// Helper to simulate string parsing and count bytes
static bool scan_string(const char** cursor, const char* end, size_t* out_len) {
    (*cursor)++; // skip / quote 
    size_t len = 0;
    while (*cursor < end) {
        char c = **cursor;
        if (c == '"') {
            (*cursor)++;
            *out_len += len + 1; // +1 for null terminator
            return true;
        }
        if (c == '\\') {
            (*cursor)++;
            if (*cursor >= end) return false;
            // Handle escape (simplified count)
            if (**cursor == 'u') *cursor += 4; // unicode
            len++;
        } else {
            len++;
        }
        (*cursor)++;
    }
    return false;
}

// Recursive function for Pass 1 would technically require stack space. 
// To keep it flat and fast, we just iterate tokens and validate hierarchy loosely 
// or use a recursive function that returns counts. 
// Here, we present the recursive logic which is cleaner to read.
static bool pass1_analyze(const char** cursor, const char* end, struct scan_status_t *stats) {
    skip_whitespace(cursor, end);
    if (*cursor >= end)
		return false;

    char c = **cursor;
    stats->nodes++; // Every value needs a node

    if (c == '{') { //0x7b
        (*cursor)++;
        bool first = true;
        while (true) {
            skip_whitespace(cursor, end);
            if (*cursor >= end)
				return false;

            if (**cursor == '}') { //0x7d
                (*cursor)++;
                return true;
            }
            if (!first) {
                if (**cursor != ',')
					return false;
                (*cursor)++;
                skip_whitespace(cursor, end);
            }
            
            // Key
            if (**cursor != '"')
				return false; // 0x22

            if (!scan_string(cursor, end, &stats->string_bytes))
				return false;
            
            skip_whitespace(cursor, end);
            if (**cursor != ':')
				return false; // 0x3a
            (*cursor)++;
            
            stats->entries++; // Record an object entry
            
            // Value
            if (!pass1_analyze(cursor, end, stats))
				return false;

            first = false;
        }
    } else if (c == '[') {
        (*cursor)++;
        bool first = true;
        while (true) {
            skip_whitespace(cursor, end);
            if (*cursor >= end)
				return false;

            if (**cursor == ']') {
                (*cursor)++;
                return true;
            }
            if (!first) {
                if (**cursor != ',')
					return false;

                (*cursor)++;
            }
            if (!pass1_analyze(cursor, end, stats))
				return false;

            first = false;
        }
    } else if (c == '"') {
        return scan_string(cursor, end, &stats->string_bytes);
    } else if (c == '-' || (c >= '0' && c <= '9')) {
        // Number (simplified skip)
        while (*cursor < end && (isdigit(**cursor) || **cursor == '.' || **cursor == '-' || **cursor == 'e' || **cursor == 'E' || **cursor == '+')) {
            (*cursor)++;
        }
        return true;
    } else if (strncmp(*cursor, "true", 4) == 0) {
        *cursor += 4; return true;
    } else if (strncmp(*cursor, "false", 5) == 0) {
        *cursor += 5; return true;
    } else if (strncmp(*cursor, "null", 4) == 0) {
        *cursor += 4; return true;
    }

    return false;
}

/*
In Pass 2, we need to ensure Contiguous Memory for array/object children. Since we have a single linear allocator, we use a "Scan-Ahead" strategy: when we encounter 
a container ({ or [), we scan it locally to count its immediate children. We then advance the linear allocator by that count to reserve the memory block 
contiguously, and finally fill it recursively.
*/
// Helper to decode string into arena
static const char* parse_string_text(const char** cursor, struct json_arena_t *arena, size_t* out_len) {
    (*cursor)++; // skip quote
    char* start = arena->strings;
    while (**cursor != '"') {
        char c = *(*cursor)++;
        if (c == '\\') {
            c = *(*cursor)++;
            // Simplified escape handling for brevity
            switch(c) {
                case 'n': *arena->strings++ = '\n'; break;
                case 't': *arena->strings++ = '\t'; break;
                // ... other escapes ...
                default:  *arena->strings++ = c; break;
            }
        } else {
            *arena->strings++ = c;
        }
    }
    (*cursor)++; // skip closing quote
    *arena->strings++ = '\0';
    *out_len = (size_t)(arena->strings - start - 1);
    return start;
}

static struct json_value_t *pass2_build(const char** cursor, const char* end, struct json_arena_t *arena) {
    skip_whitespace(cursor, end);
    
    // Allocate slot for current node
    struct json_value_t * node = arena->nodes++; 
    
    char c = **cursor;
    
    if (c == '{') {
        node->type = JSON_OBJECT;
        const char* saved = *cursor + 1; // Save position inside {
        
        // 1. Mini-pass: Count immediate children
        size_t count = 0;
        skip_whitespace(&saved, end);
        if (*saved != '}') {
            count++;
            while (true) {
                // Skip Key
                while (*saved != ':') saved++; 
                saved++; // Skip :
                
                // Skip Value (Balance brackets/braces)
                int depth = 0;
                while (*saved) {
                    if (*saved == '{' || *saved == '[') depth++;
                    else if (*saved == '}' || *saved == ']') {
                        if (depth == 0) break;
                        depth--;
                    }
                    else if (*saved == ',' && depth == 0) break;
                    saved++;
                }
                
                if (*saved == '}') break;
                if (*saved == ',') { count++; saved++; }
                skip_whitespace(&saved, end);
            }
        }
        
        // 2. Reserve contiguous memory
        node->data.object.count = count;
        node->data.object.entries = (count > 0) ? arena->entries : NULL;
        arena->entries += count;
        
        // 3. Fill
        (*cursor)++; // Skip {
        for (size_t i = 0; i < count; i++) {
            skip_whitespace(cursor, end);
            
            // Parse Key
            size_t klen;
            node->data.object.entries[i].key = parse_string_text(cursor, arena, &klen);
            node->data.object.entries[i].key_len = klen;
            
            skip_whitespace(cursor, end);
            (*cursor)++; // Skip :
            
            // Recursively Parse Value
            node->data.object.entries[i].value = pass2_build(cursor, end, arena);
            
            skip_whitespace(cursor, end);
            if (**cursor == ',') (*cursor)++;
        }
        skip_whitespace(cursor, end);
        if (**cursor == '}') (*cursor)++;
        
    } else if (c == '[') {
        node->type = JSON_ARRAY;
        const char* saved = *cursor + 1;
        
        // 1. Mini-pass: Count items
        size_t count = 0;
        skip_whitespace(&saved, end);
        if (*saved != ']') {
            count++;
            int depth = 0;
            while (*saved) {
                 if (*saved == '{' || *saved == '[') depth++;
                 else if (*saved == '}' || *saved == ']') {
                     if (depth == 0 && *saved == ']') break;
                     depth--;
                 }
                 else if (*saved == ',' && depth == 0) count++;
                 saved++;
            }
        }
        
        // 2. Reserve contiguous memory
        node->data.array.count = count;
        node->data.array.items = (count > 0) ? arena->nodes : NULL; 
        // Note: We don't increment arena->nodes here by `count`.
        // The recursive calls in the loop below will consume the node slots sequentially!
        // This works because we call pass2_build exactly `count` times immediately.
        
        // 3. Fill
        (*cursor)++; // Skip [
        for (size_t i = 0; i < count; i++) {
            // We do NOT store pointers to children in the array struct.
            // We store the START pointer (items). The children are contiguous.
            // But wait, pass2_build returns a pointer. 
            // In a contiguous array of structs, we don't need pointers.
            // Correct approach for Array of Structs:
            
            // Standard JSON array is usually array of pointers or array of variants.
            // Our json_value_t  has a pointer `items`. 
            // Since `pass2_build` consumes a slot from `arena->nodes` immediately,
            // calling it `count` times will place them linearly in memory.
            
            // However, pass2_build returns a pointer to the slot used.
            // We need to verify the slots are indeed contiguous. 
            // Since `arena->nodes` is a linear bump pointer, recursive calls
            // will layout [Child1][Child1_Children...][Child2]...
            // This is NOT contiguous Child1, Child2.
            
            // CORRECTION for Contiguous Requirements:
            // To have Child1, Child2 contiguous, we cannot do deep recursion inline.
            // We must reserve the slots for Child1...ChildN, THEN recurse to fill them.
            
            struct json_value_t *slot = &node->data.array.items[i]; // This logic assumes items was alloc'd
            // But we didn't alloc items yet?
        }
        
        // REVISED Logic for Array:
        // We want `items` to be an array of `json_value_t `. 
        // We reserve `count` slots now.
        struct json_value_t * children_start = arena->nodes;
        arena->nodes += count;
        node->data.array.items = children_start;
        
        for (size_t i = 0; i < count; i++) {
             skip_whitespace(cursor, end);
             // We cannot call pass2_build normally because it allocs a new node.
             // We need a helper that fills an EXISTING node.
             // Let's refactor slightly: split alloc and fill.
        }
    } 
    // ... Primitives (Bool, Null, Number, String) ...
    else if (c == '"') {
        node->type = JSON_STRING;
        node->data.string.val = parse_string_text(cursor, arena, &node->data.string.len);
    } else {
        // ... (Number/Bool parsing similar to Pass 1 but filling data) ...
        if (isdigit(c) || c == '-') {
            node->type = JSON_NUMBER;
            char* next;
            node->data.number = strtod(*cursor, &next);
            *cursor = next;
        } 
        // ...
    }
    
    return node;
}

// Helper: Fill a pre-allocated node
static void fill_node(struct json_value_t *node, const char **cursor, const char *end, struct json_arena_t *arena);

static void fill_node(struct json_value_t *node, const char **cursor, const char *end, struct json_arena_t *arena) {
    skip_whitespace(cursor, end);
    char c = **cursor;

    if (c == '{') {
        node->type = JSON_OBJECT;
        const char* saved = *cursor + 1;
        // Count keys
        size_t count = 0;
        skip_whitespace(&saved, end);
        if (*saved != '}') {
            count++;
            while(1) {
                // Simplified skip logic
                while(*saved != ':' && *saved) saved++; 
                if(*saved) saved++;
                int d=0;
                while(*saved && (d>0 || (*saved!=',' && *saved!='}'))) {
                    if(*saved=='{'||*saved=='[') d++;
                    if(*saved=='}'||*saved==']') d--;
                    saved++;
                }
                if(*saved=='}') break;
                count++; saved++;
            }
        }
        
        node->data.object.count = count;
        node->data.object.entries = (count > 0) ? arena->entries : NULL;
        arena->entries += count;
        
        (*cursor)++;
        for(size_t i=0; i<count; i++) {
            skip_whitespace(cursor, end);
            size_t klen;
            node->data.object.entries[i].key = parse_string_text(cursor, arena, &klen);
            node->data.object.entries[i].key_len = klen;
            skip_whitespace(cursor, end);
            (*cursor)++; // :
            
            // Allocate the value node immediately after keys? 
            // Strategy: Allocate a new node from the pool for the value.
            struct json_value_t *val_node = arena->nodes++;
            node->data.object.entries[i].value = val_node;
            fill_node(val_node, cursor, end, arena);
            
            skip_whitespace(cursor, end);
            if(**cursor == ',') (*cursor)++;
        }
        skip_whitespace(cursor, end);
        if(**cursor == '}') (*cursor)++;
    }
    else if (c == '[') {
        node->type = JSON_ARRAY;
        const char* saved = *cursor + 1;
        size_t count = 0;
        // ... (Similar counting logic as Object) ...
        // Simplified for brevity:
        skip_whitespace(&saved, end);
        if (*saved != ']') {
             count = 1;
             int d=0;
             while(*saved && (d>0 || *saved!=']')) {
                 if(*saved=='{'||*saved=='[') d++;
                 if(*saved=='}'||*saved==']') d--;
                 if(d==0 && *saved==',') count++;
                 saved++;
             }
        }

        node->data.array.count = count;
        // Contiguous Allocation: Reserve `count` nodes sequentially
        node->data.array.items = (count > 0) ? arena->nodes : NULL;
        arena->nodes += count;
        
        (*cursor)++;
        for(size_t i=0; i<count; i++) {
            // Fill the pre-reserved slots
            fill_node(&node->data.array.items[i], cursor, end, arena);
            
            skip_whitespace(cursor, end);
            if(**cursor == ',') (*cursor)++;
        }
        skip_whitespace(cursor, end);
        if(**cursor == ']') (*cursor)++;
    }
    else if (c == '"') {
        node->type = JSON_STRING;
        node->data.string.val = parse_string_text(cursor, arena, &node->data.string.len);
    }
    else if (isdigit(c) || c=='-') {
        node->type = JSON_NUMBER;
        char* next;
        node->data.number = strtod(*cursor, &next);
        *cursor = next;
    }
    else if (strncmp(*cursor, "true", 4)==0) {
        node->type = JSON_BOOL; node->data.boolean = 1; *cursor+=4;
    }
    else if (strncmp(*cursor, "false", 5)==0) {
        node->type = JSON_BOOL; node->data.boolean = 0; *cursor+=5;
    }
    else if (strncmp(*cursor, "null", 4)==0) {
        node->type = JSON_NULL; *cursor+=4;
    }
}

struct json_value_t *parse_json(const char* input, size_t length, char* error_buffer) {
    const char* cursor = input;
    const char* end = input + length;
    struct scan_status_t stats = {0};

    // --- PASS 1: Calculate ---
    if (!pass1_analyze(&cursor, end, &stats)) {
        if (error_buffer) sprintf(error_buffer, "Syntax Error or Unexpected EOF");
        return NULL;
    }
    
    // --- Allocate ---
    // Memory Layout: [json_value_t  Nodes ... ] [Entry Arrays ... ] [Strings ... ]
    size_t total_size = (sizeof(struct json_value_t) * stats.nodes) + 
                        (sizeof(struct json_entry_t) * stats.entries) + 
                        stats.string_bytes;
                        
    // Use calloc to ensure zero-initialization (safer)
    void *memory = calloc(1, total_size);
    if (!memory) {
        if (error_buffer) sprintf(error_buffer, "Memory allocation failed");
        return NULL;
    }

    // Initialize Arena
    struct json_arena_t arena;
    arena.nodes = (struct json_value_t *)memory;
    arena.entries = (struct json_entry_t *)(arena.nodes + stats.nodes);
    arena.strings = (char *)(arena.entries + stats.entries);
    
    // Safety boundaries
    arena.nodes_rem = stats.nodes;
    
    // --- PASS 2: Allocate & Fill ---
    cursor = input;
    struct json_value_t *root = arena.nodes++; // Take first slot for root
    fill_node(root, &cursor, end, &arena);
    
    return root;
}

void print_json(struct json_value_t* val, int indent) {
    if (!val) 
		return;

    // Helper to print indentation
    for (int i = 0; i < indent; i++) printf("  ");

    switch (val->type) {
        case JSON_NULL:
            printf("null\n");
            break;
        case JSON_BOOL:
            printf("%s\n", val->data.boolean ? "true" : "false");
            break;
        case JSON_NUMBER:
            printf("%f\n", val->data.number);
            break;
        case JSON_STRING:
            printf("\"%.*s\"\n", (int)val->data.string.len, val->data.string.val);
            break;
        case JSON_ARRAY:
            printf("[\n");
            for (size_t i = 0; i < val->data.array.count; i++) {
                // Notice we access items contiguously using array indexing [i]
                // This validates your contiguous memory allocation logic in Pass 2
                print_json(&val->data.array.items[i], indent + 1);
            }
            for (int i = 0; i < indent; i++) printf("  ");
            printf("]\n");
            break;
        case JSON_OBJECT:
            printf("{\n");
            for (size_t i = 0; i < val->data.object.count; i++) {
                for (int j = 0; j < indent + 1; j++) printf("  ");
                // Validate Key
                printf("\"%.*s\": \n", (int)val->data.object.entries[i].key_len, val->data.object.entries[i].key);
                // Validate Value pointer
                print_json(val->data.object.entries[i].value, indent + 2);
            }
            for (int i = 0; i < indent; i++) printf("  ");
            printf("}\n");
            break;
    }
}

int main(int argc, char *argv[]) {
    // Calculate length of the embedded blob
    size_t len = json_end - json_start;
    char err_buf[256];
    
    struct json_value_t *root = parse_json((const char*)json_start , len, err_buf);

    if (!root) {
        fprintf(stderr, "Parsing Failed: %s\n", err_buf);
        return 1;
    }

    printf("--- Parsed Tree ---\n");
    print_json(root, 0);

    // Single free for the whole arena
    free(root);
    printf("\n--- Cleanup Done ---\n");

    return 0;
}
