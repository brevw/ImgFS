#include "http_prot.h"
#include "util.h"
#include "error.h"
#include <stddef.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdbool.h>

#ifdef IN_CS202_UNIT_TEST
#define static_unless_test
#else
#define static_unless_test static
#endif

#define PARSING_BUF_MAX 20

// given an http_string parse it to size_t
// int         : case no error
// ERR_RUNTIME : case of an overflow or invalid http_string (error while parsing)
static_unless_test int parse_int(struct http_string* string)
{
    char buf[PARSING_BUF_MAX+1];
    int buf_size = PARSING_BUF_MAX;
    if (string->len > INT_MAX || (int)string->len > buf_size) {
        return ERR_RUNTIME;
    }
    strncpy(buf, string->val, string->len);
    buf[string->len] = '\0';

    int parsed = atoi(buf);
    return parsed <= 0 ? ERR_RUNTIME : parsed;
}

// given a key (C string) return
// int         : index of the header inside headers array
// ERR_RUNTIME : in case key not present inside the headers array
static_unless_test int get_value_from_key(const char* key, struct http_message* out)
{
    size_t i = 0;
    struct http_header* headers = out->headers;
    bool found_value = false;

    for(; i < out->num_headers && !found_value; ++i) {
        if (http_match_verb(&headers[i].key, key)) {
            found_value = true;
        }
    }
    --i;

    if (found_value) {
        return (int) i;
    } else {
        return ERR_RUNTIME;
    }
}

static_unless_test const char* get_next_token(const char* message, const char* delimiter, struct http_string* output)
{
    size_t delimiter_len = strlen(delimiter);
    char* pattern_pos = strstr(message, delimiter);
    if (pattern_pos == NULL) {
        return NULL;
    }
    if (output != NULL) {
        output->val = message;
        output->len = (size_t) (pattern_pos - message);
    }
    return pattern_pos + delimiter_len;
}

static_unless_test const char* http_parse_headers(const char* header_start, struct http_message* output)
{
    size_t HTTP_LINE_DELIM_len = strlen(HTTP_LINE_DELIM);
    const char* parse_from = header_start;
    bool too_many_headers = false;
    size_t header_count = 0;

    while(!too_many_headers && parse_from != NULL && strncmp(parse_from, HTTP_LINE_DELIM, HTTP_LINE_DELIM_len) != 0) {
        if (header_count >= MAX_HEADERS) {
            too_many_headers = true;
        } else {
            parse_from = get_next_token(parse_from, HTTP_HDR_KV_DELIM, &output->headers[header_count].key);
            if (parse_from != NULL) {
                parse_from = get_next_token(parse_from, HTTP_LINE_DELIM, &output->headers[header_count].value);
                if (parse_from != NULL) {
                    ++header_count;
                }
            }
        }
    }
    if (parse_from == NULL || too_many_headers) {
        return NULL;
    }

    parse_from += HTTP_LINE_DELIM_len;
    output->num_headers = header_count;
    return parse_from;
}



int http_match_uri(const struct http_message *message, const char *target_uri)
{
    M_REQUIRE_NON_NULL(message);
    M_REQUIRE_NON_NULL(target_uri);

    const struct http_string* uri = &message->uri;
    size_t target_uri_len = strlen(target_uri);
    if (target_uri_len > uri->len) {
        return false;
    }
    return strncmp(uri->val, target_uri, target_uri_len) == 0 ? true : false;
}

int http_match_verb(const struct http_string *method, const char *verb)
{
    M_REQUIRE_NON_NULL(method);
    M_REQUIRE_NON_NULL(verb);
    size_t verb_len = strlen(verb);

    if (verb_len != method->len) {
        return false;
    }
    return strncmp(method->val, verb, verb_len) == 0 ? true : false;
}

int http_get_var(const struct http_string *url, const char *name, char *out, size_t out_len)
{
    M_REQUIRE_NON_NULL(url);
    M_REQUIRE_NON_NULL(name);
    M_REQUIRE_NON_NULL(out);

    size_t name_len = strlen(name);

    // early exit in this edge case
    if (name_len > url->len) {
        return 0;
    }

    size_t pattern_buf_len = name_len + 1; // +1 to append "="
    char pattern_buf[pattern_buf_len + 1];

    if ((size_t) snprintf(pattern_buf, pattern_buf_len + 1, "%s=", name) != pattern_buf_len) {
        return ERR_RUNTIME;
    }

    // locate the param
    const char* start_pos_param = strnstr(url->val, pattern_buf, url->len);
    if (start_pos_param == NULL) {
        return 0;
    }

    // check param is located right after '&' or '?'
    if(start_pos_param != url->val && *(start_pos_param - 1) != '?' && *(start_pos_param - 1) != '&') {
        return 0;
    }
    // check param is located after '?'
    if (strnstr(url->val, "?", (size_t) (start_pos_param - url->val)) == NULL){
        return 0;
    }

    const char* start_pos_value = start_pos_param + name_len + 1;
    const char* stop_pos_value  = strnstr(start_pos_value, "&", ( url->len - (size_t)(start_pos_value - url->val) ));
    // case value of the param located at the end of the url without '&' after it
    if (stop_pos_value == NULL) {
        stop_pos_value = url->val + url->len;
    }

    size_t value_len = (size_t) (stop_pos_value - start_pos_value);
    // value_len too big to fit into an int
    if (value_len > INT_MAX) {
        return ERR_RUNTIME;
    }
    // provided buffer too small (can overflow)
    if (value_len + 1 > out_len) {
        return ERR_RUNTIME;
    }

    strncpy(out, start_pos_value, value_len);
    out[value_len] = '\0';

    return (int) value_len;
}

int http_parse_message(const char *stream, size_t bytes_received, struct http_message *out, int *content_len)
{
    M_REQUIRE_NON_NULL(stream);
    M_REQUIRE_NON_NULL(out);
    M_REQUIRE_NON_NULL(content_len);

    // incomplete headers
    if (strstr(stream, HTTP_HDR_END_DELIM) == NULL) {
        return 0;
    }

    const char* parse_from = stream;
    parse_from = get_next_token(parse_from, " ", &out->method);
    if (parse_from == NULL) {
        return 0;
    }

    parse_from = get_next_token(parse_from, " ", &out->uri);
    if (parse_from == NULL) {
        return 0;
    }

    struct http_string protocol_id;
    parse_from = get_next_token(parse_from, HTTP_LINE_DELIM, &protocol_id);
    if (parse_from == NULL || !http_match_verb(&protocol_id, "HTTP/1.1")) {
        return 0;
    }

    parse_from = http_parse_headers(parse_from, out);
    if (parse_from == NULL) {
        return ERR_RUNTIME;
    }

    int index = get_value_from_key("Content-Length", out);
    if (index < 0 || http_match_verb(&out->headers[index].value, "0") == 1) { // case Content-Length is 0 or not present
        out->body.val = parse_from;
        out->body.len = 0;
        return 1;
    }

    int content_length = parse_int(&out->headers[index].value);
    if (content_length < 0) {
        return ERR_RUNTIME;
    }
    *content_len  = content_length;

    size_t body_received_len = bytes_received - (size_t)(parse_from - stream);
    if (body_received_len > INT_MAX) {
        return ERR_RUNTIME;
    }
    // body not fully received
    out->body.val = parse_from;
    out->body.len = body_received_len;

    if ((int) body_received_len < content_length) {
        return 0;
    }
    return 1;
}
