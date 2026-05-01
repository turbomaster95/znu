#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#define MAX_BOOT_ARGS 64

struct boot_arg {
    char *key;
    char *value; // Will be NULL if it's just a flag (e.g., "verbose")
};

static struct boot_arg boot_args[MAX_BOOT_ARGS];
static size_t boot_arg_count = 0;

void parse_cmdline(char *cmdline) {
    if (!cmdline || *cmdline == '\0') return;

    char *ptr = cmdline;
    while (*ptr && boot_arg_count < MAX_BOOT_ARGS) {
        // Skip spaces
        while (*ptr == ' ') ptr++;
        if (!*ptr) break;

        // Start of a new argument
        boot_args[boot_arg_count].key = ptr;
        boot_args[boot_arg_count].value = NULL;

        // Find end of token or '='
        while (*ptr && *ptr != ' ' && *ptr != '=') ptr++;

        if (*ptr == '=') {
            *ptr = '\0'; // Null terminate the key
            ptr++;
            boot_args[boot_arg_count].value = ptr; // Value starts here
            
            // Find end of value
            while (*ptr && *ptr != ' ') ptr++;
        }

        if (*ptr == ' ') {
            *ptr = '\0'; // Null terminate the value or flag
            ptr++;
        }

        boot_arg_count++;
    }
}

bool boot_get_flag(const char *key) {
    for (size_t i = 0; i < boot_arg_count; i++) {
        if (strcmp(boot_args[i].key, key) == 0) return true;
    }
    return false;
}

const char* boot_get_value(const char *key) {
    for (size_t i = 0; i < boot_arg_count; i++) {
        if (strcmp(boot_args[i].key, key) == 0) {
            return boot_args[i].value;
        }
    }
    return NULL;
}


