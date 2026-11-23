
#include <csexp.h>

static int csexp_is_digit(uint8_t c) { return c >= (uint8_t)'0' && c <= (uint8_t)'9'; }

static csexp_error csexp_fail(csexp_result *result, csexp_error error, size_t offset, size_t used) {
    result->root = NULL;
    result->nodes_used = used;
    result->error_offset = offset;
    result->error = error;
    return error;
}

static csexp_error csexp_read_string(const uint8_t *input, size_t input_len, size_t *pos, const uint8_t **data, size_t *length, size_t *bad) {
    size_t n = 0;
    size_t start = *pos;

    if (*pos >= input_len || !csexp_is_digit(input[*pos])) {
        *bad = *pos;
        return CSEXP_ERR_INVALID_LENGTH;
    }

    if (input[*pos] == (uint8_t)'0' && *pos + 1 < input_len && csexp_is_digit(input[*pos + 1])) {
        *bad = *pos;
        return CSEXP_ERR_NONCANONICAL_LENGTH;
    }

    while (*pos < input_len && csexp_is_digit(input[*pos])) {
        size_t digit = (size_t)(input[*pos] - (uint8_t)'0');

        if (n > ((size_t)-1 - digit) / 10) {
            *bad = *pos;
            return CSEXP_ERR_LENGTH_OVERFLOW;
        }

        n = n * 10 + digit;
        (*pos)++;
    }

    if (*pos >= input_len || input[*pos] != (uint8_t)':') {
        *bad = *pos;
        return CSEXP_ERR_INVALID_LENGTH;
    }

    (*pos)++;

    if (n > input_len - *pos) {
        *bad = start;
        return CSEXP_ERR_TRUNCATED_ATOM;
    }

    *data = input + *pos;
    *length = n;
    *pos += n;

    return CSEXP_OK;
}

static csexp_error csexp_new_node(csexp_node *nodes, size_t cap, size_t *used, csexp_node **out) {
    csexp_node *node;

    if (*used >= cap) return CSEXP_ERR_NO_NODES;

    node = &nodes[(*used)++];

    node->type = CSEXP_ATOM;
    node->data = NULL;
    node->length = 0;
    node->hint = NULL;
    node->hint_length = 0;
    node->first_child = NULL;
    node->last_child = NULL;
    node->next = NULL;

    *out = node;
    return CSEXP_OK;
}

static void csexp_attach(csexp_node **root, csexp_node **stack, size_t depth, csexp_node *node) {
    csexp_node *parent;

    if (depth == 0) {
        *root = node;
        return;
    }

    parent = stack[depth - 1];

    if (parent->last_child != NULL) parent->last_child->next = node;
    else parent->first_child = node;

    parent->last_child = node;
}

csexp_error csexp_parse(const uint8_t *input, size_t input_len, csexp_node *nodes, size_t node_cap, csexp_node **stack, size_t stack_cap, csexp_result *result) {
    size_t pos = 0;
    size_t used = 0;
    size_t depth = 0;
    csexp_node *root = NULL;

    if (result == NULL) return CSEXP_ERR_ARGUMENT;

    result->root = NULL;
    result->nodes_used = 0;
    result->error_offset = 0;
    result->error = CSEXP_OK;

    if (input == NULL || (node_cap != 0 && nodes == NULL) || (stack_cap != 0 && stack == NULL)) return csexp_fail(result, CSEXP_ERR_ARGUMENT, 0, 0);
    if (input_len == 0) return csexp_fail(result, CSEXP_ERR_EMPTY, 0, 0);

    while (pos < input_len) {
        uint8_t c = input[pos];
        csexp_node *node = NULL;
        csexp_error error;

        if (root != NULL && depth == 0) return csexp_fail(result, CSEXP_ERR_TRAILING_DATA, pos, used);

        if (c == (uint8_t)')') {
            if (depth == 0) return csexp_fail(result, CSEXP_ERR_UNEXPECTED_CLOSE, pos, used);
            depth--;
            pos++;
            continue;
        }

        if (c == (uint8_t)'(') {
            if (depth >= stack_cap) return csexp_fail(result, CSEXP_ERR_MAX_DEPTH, pos, used);

            error = csexp_new_node(nodes, node_cap, &used, &node);
            if (error != CSEXP_OK) return csexp_fail(result, error, pos, used);

            node->type = CSEXP_LIST;
            csexp_attach(&root, stack, depth, node);
            stack[depth++] = node;
            pos++;
            continue;
        }

        if (c == (uint8_t)'[') {
            const uint8_t *hint = NULL;
            const uint8_t *data = NULL;
            size_t hint_length = 0;
            size_t data_length = 0;
            size_t bad = pos;

            pos++;

            error = csexp_read_string(input, input_len, &pos, &hint, &hint_length, &bad);
            if (error != CSEXP_OK) return csexp_fail(result, CSEXP_ERR_INVALID_DISPLAY_HINT, bad, used);

            if (pos >= input_len || input[pos] != (uint8_t)']') return csexp_fail(result, CSEXP_ERR_INVALID_DISPLAY_HINT, pos, used);

            pos++;

            error = csexp_read_string(input, input_len, &pos, &data, &data_length, &bad);
            if (error != CSEXP_OK) return csexp_fail(result, error, bad, used);

            error = csexp_new_node(nodes, node_cap, &used, &node);
            if (error != CSEXP_OK) return csexp_fail(result, error, bad, used);

            node->data = data;
            node->length = data_length;
            node->hint = hint;
            node->hint_length = hint_length;

            csexp_attach(&root, stack, depth, node);
            continue;
        }

        if (csexp_is_digit(c)) {
            const uint8_t *data = NULL;
            size_t data_length = 0;
            size_t bad = pos;

            error = csexp_read_string(input, input_len, &pos, &data, &data_length, &bad);
            if (error != CSEXP_OK) return csexp_fail(result, error, bad, used);

            error = csexp_new_node(nodes, node_cap, &used, &node);
            if (error != CSEXP_OK) return csexp_fail(result, error, bad, used);

            node->data = data;
            node->length = data_length;

            csexp_attach(&root, stack, depth, node);
            continue;
        }

        return csexp_fail(result, CSEXP_ERR_UNEXPECTED_CHAR, pos, used);
    }

    if (depth != 0) return csexp_fail(result, CSEXP_ERR_UNCLOSED_LIST, input_len, used);
    if (root == NULL) return csexp_fail(result, CSEXP_ERR_EMPTY, 0, used);

    result->root = root;
    result->nodes_used = used;
    result->error_offset = input_len;
    result->error = CSEXP_OK;

    return CSEXP_OK;
}

const char *csexp_error_string(csexp_error error) {
    switch (error) {
        case CSEXP_OK: return "success";
        case CSEXP_ERR_ARGUMENT: return "invalid argument";
        case CSEXP_ERR_EMPTY: return "empty input";
        case CSEXP_ERR_UNEXPECTED_CHAR: return "unexpected character";
        case CSEXP_ERR_UNEXPECTED_CLOSE: return "unexpected closing parenthesis";
        case CSEXP_ERR_UNCLOSED_LIST: return "unclosed list";
        case CSEXP_ERR_INVALID_LENGTH: return "invalid string length";
        case CSEXP_ERR_NONCANONICAL_LENGTH: return "noncanonical string length";
        case CSEXP_ERR_LENGTH_OVERFLOW: return "string length overflow";
        case CSEXP_ERR_TRUNCATED_ATOM: return "truncated string";
        case CSEXP_ERR_TRAILING_DATA: return "trailing data";
        case CSEXP_ERR_NO_NODES: return "node pool exhausted";
        case CSEXP_ERR_MAX_DEPTH: return "maximum nesting depth exceeded";
        case CSEXP_ERR_INVALID_DISPLAY_HINT: return "invalid display hint";
        default: return "unknown CSEXPs error";
    }
}
