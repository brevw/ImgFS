#pragma once
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#define DELIMITER "<EOF>"
#define DELIMITER_SIZE (sizeof(DELIMITER) - 1)
#define BIG_FILE "Big file"
#define BIG_FILE_SIZE (sizeof(BIG_FILE) - 1)
#define SMALL_FILE "Small file"
#define SMALL_FILE_SIZE (sizeof(SMALL_FILE) - 1)
#define SMALL_FILE_MAX_SIZE 1024
#define ERR_FILE_TOO_BIG -9999

#define ACCEPTED_FILE "Accepted"
typedef enum {
    STYLE_DEFAULT,
    STYLE_RED,
    STYLE_GREEN
} TextStyle;

static const char *style_codes[] = {
    "\033[0m",   // Default
    "\033[31m",  // Red
    "\033[32m",  // Green
};

int tcp_read_till_delim(int socket, void* buf, int max_buf_len);
int tcp_send_with_delim(int socket, const void* buf, int size);

void handle_error(int err, void* buf);

void printf_styled(TextStyle style, const char* format, ...);
