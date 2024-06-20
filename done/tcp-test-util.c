#include "tcp-test-util.h"
#include "error.h"
#include "socket_layer.h"
#include "util.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

void handle_error(int err, void *buf) {
  free(buf);
  if (err == ERR_NONE)
    exit(EXIT_SUCCESS);
  else
    exit(EXIT_FAILURE);
}

// has exact same use as printf except we need to specify another
// argument which is the color depending on the TextStyle enum
void printf_styled(TextStyle style, const char *format, ...) __attribute__((format(printf, 2, 3)));
void printf_styled(TextStyle style, const char *format, ...) {
  va_list args;
  va_start(args, format);
  printf("%s", style_codes[style]);
  vprintf(format, args);
  printf("\033[0m");
  va_end(args);
}

// a wrapper over tcp_read that reads until we reach a the constant
// DELIMITER specified in the header file
// on success the DELIMITED at the end of the buf will be deleted
// - buf : should be of size content_size + DELIMITER_SIZE
// - max_buf_len : content_size without including the DELIMITER
// returns (ERR_IO) in case of an error and (ERR_NONE) in case of success
int tcp_read_till_delim(int socket, void *buf, int max_buf_len) {
  ssize_t ret = 0;
  char *where_to_read = (char *)buf;
  int read_bytes = 0;
  max_buf_len += DELIMITER_SIZE;

  while (read_bytes < max_buf_len) {
    ret = tcp_read(socket, buf, (size_t) max_buf_len - (size_t) read_bytes);
    if (ret <= 0)
      return ERR_IO;
    read_bytes += ret;
    where_to_read += ret;
    // if we delimited found delete it at the end
    if (strncmp((char *)MAX((char *)buf, where_to_read - DELIMITER_SIZE),
                DELIMITER, DELIMITER_SIZE) == 0) {
      where_to_read -= DELIMITER_SIZE;
      *where_to_read = (char)0;
      return ERR_NONE;
    }
  }
  return ERR_IO;
}

// awrapper over tcp_send which will add the DELIMITER at the
// end of the sequence before sending it through tcp_send
int tcp_send_with_delim(int socket, const void *buf, int size) {
  int bytes_sent = 0;
  int new_buf_size = size + (int) DELIMITER_SIZE;
  char *new_buf = malloc((size_t)new_buf_size);

  if (new_buf == NULL)
    return ERR_INVALID_ARGUMENT;
  strncpy(new_buf, buf, (size_t)size);
  strncpy(new_buf + size, DELIMITER, DELIMITER_SIZE);
  int ret = 0;
  while (bytes_sent < new_buf_size) {
    ret = (int)tcp_send(socket, new_buf, (size_t)new_buf_size - (size_t) bytes_sent);
    if (ret <= 0) {
      free(new_buf);
      return ERR_IO;
    }
    bytes_sent += ret;
  }
  free(new_buf);
  return ERR_NONE;
}
