#include "error.h"
#include "socket_layer.h"
#include "tcp-test-util.h"
#include "util.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_FILE_SIZE 2048
#define LOCAL_IP "127.0.0.1"
static int MAX_SERVER_RESPONSE = MAX(strlen(SMALL_FILE), strlen(BIG_FILE));

int tcp_client_init(uint16_t port) {
  int socket_fd;
  struct sockaddr_in server_addr;
  socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd == -1) {
    perror("socket() in tcp_client_init()");
    return ERR_IO;
  }
  zero_init_var(server_addr);
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  if (inet_pton(AF_INET, LOCAL_IP, (struct in_addr*) &server_addr.sin_addr) <= 0){
    perror("inet_pton() in tcp_client_init()");
    if (close(socket_fd) == -1){
        perror("close() in tcp_client_init");
    }
    return ERR_IO;
  }
  if (connect(socket_fd, (struct sockaddr*) &server_addr, sizeof(server_addr)) == -1) {
    perror("connect() in tcp_server_init()");
    if (close(socket_fd) == -1) {
      perror("close() in tcp_server_init()");
    }
    return ERR_IO;
  }

  return socket_fd;
}

int file_size_bounded(FILE *file, int bound) {
  M_REQUIRE_NON_NULL(file);
  if (fseek(file, 0, SEEK_END)) {
    return ERR_IO;
  }
  size_t size = (size_t)file->_offset;
  if (size <= (size_t)bound)
    return (int)size;
  else
    return ERR_FILE_TOO_BIG;
}

int main(int argc, char *argv[]) {
  // remove first argument
  --argc;
  ++argv;

  uint16_t port_number = 0;
  char* file_buf = NULL;
  FILE* file = NULL;
  int file_size = 0;
  char* file_name = NULL;
  int server_socket = 0;
  if (argc < 2)
    handle_error(ERR_NOT_ENOUGH_ARGUMENTS, file_buf);
  if (argc != 2)
    handle_error(ERR_INVALID_COMMAND, file_buf);

  // extract port number and load file to buffer
  port_number = atouint16(argv[0]);
  if (port_number == 0) 
    handle_error(ERR_INVALID_ARGUMENT, file_buf);
  file_name = argv[1];
  file = fopen(file_name, "rb");
  if (file == NULL)
    handle_error(ERR_IO, file_buf);
  file_size = file_size_bounded(file, MAX_FILE_SIZE);
  if (file_size < 0)
    handle_error(file_size, file_buf);
  if (fseek(file, 0, SEEK_SET))
    handle_error(ERR_IO, file_buf);
  file_buf = malloc((size_t)file_size);
  if (file_buf == NULL)
    handle_error(ERR_OUT_OF_MEMORY, file_buf);
  if (fread(file_buf, (size_t)file_size, 1, file) != 1)
    handle_error(ERR_IO, file_buf);

  // open socket connection
  server_socket = tcp_client_init(port_number);
  if (server_socket < 0) handle_error(server_socket, file_buf);

  printf_styled(STYLE_RED, "Talking "); 
  printf_styled(STYLE_DEFAULT, "to %d\n", port_number);
  int res;


  // sending file size to the server 
  printf_styled(STYLE_RED, "Sending "); 
  printf_styled(STYLE_DEFAULT, "size %d:\n", file_size);
  char size_buf[32 + 1]; // we suppose that number can be incoded on 32 bits
  sprintf(size_buf, "%d", file_size);
  res = tcp_send_with_delim(server_socket, size_buf, (int) strlen(size_buf));
  if (res != ERR_NONE){
    handle_error(res, file_buf);
  }

  // read server response
  char respones_buff[MAX_SERVER_RESPONSE + DELIMITER_SIZE + 1];
  res = tcp_read_till_delim(server_socket, respones_buff, MAX_SERVER_RESPONSE);
  if (res != ERR_NONE){
    handle_error(res, file_buf);
  }
  printf_styled(STYLE_RED, "Server "); 
  printf_styled(STYLE_DEFAULT, "responded: \"");
  printf_styled(STYLE_GREEN, "%s", respones_buff);
  printf_styled(STYLE_DEFAULT, "\"\n");


  // sending the entire file
  printf_styled(STYLE_RED, "Sending ");
  printf_styled(STYLE_DEFAULT, "%s:\n", file_name);
  if (tcp_send(server_socket, file_buf,(size_t) file_size) != file_size) handle_error(ERR_IO, file_buf);

  // if we reach this step that means that file was Sent 
  char ACK_buff[sizeof(ACCEPTED_FILE)-1 + DELIMITER_SIZE + 1];
  res = tcp_read_till_delim(server_socket, ACK_buff, sizeof(ACCEPTED_FILE) - 1);
  if (res != ERR_NONE){
    handle_error(res, file_buf);
  }
  printf_styled(STYLE_RED, "%s\n", ACK_buff);
  printf_styled(STYLE_RED, "Done\n");

  // close socket and exit
  if (close(server_socket) == -1) handle_error(ERR_IO, file_buf);

  handle_error(ERR_NONE, file_buf);
}

