#include "util.h"
#include "error.h"
#include "tcp-test-util.h"
#include "socket_layer.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUF_MAX_SIZE 32


static void close_free_all(int socket, char* file_buff){
  free(file_buff);
  if (close(socket) == -1){
    perror("close() in close_socket()");
  }
}

int main(int argc, char *argv[])
{
  //remove first argument
  --argc;
  ++argv;

  uint16_t port_number = 0;
  char buf[BUF_MAX_SIZE + DELIMITER_SIZE + 1];
  char* file_buf = NULL;
  int listener_socket = 0;
  int client_socket = 0;
  if (argc < 1) handle_error(ERR_NOT_ENOUGH_ARGUMENTS, NULL);
  if (argc != 1) handle_error(ERR_INVALID_COMMAND, NULL);

  //extract port number and load file to buffer
  port_number = atouint16(argv[0]);
  if (port_number == 0) handle_error(ERR_INVALID_ARGUMENT, NULL);
  listener_socket = tcp_server_init(port_number);
  if (listener_socket < 0) handle_error(listener_socket, NULL);
  printf_styled(STYLE_RED, "Server "); 
  printf_styled(STYLE_DEFAULT, "started on port %d\n", port_number);

  uint32_t file_size = 0;

  // infinite loop so that server never stops accepting requests
  while(true){
    zero_init_var(buf);
    file_buf = NULL;

    // create client socket 
    client_socket = tcp_accept(listener_socket);
    if (client_socket < 0){
      close_free_all(client_socket, file_buf);
      continue;
    }
    printf_styled(STYLE_RED, "Waiting "); printf_styled(STYLE_DEFAULT, "for a size...\n");
    
    // receive size of the file
    int res = tcp_read_till_delim(client_socket, buf, BUF_MAX_SIZE);
    if (res < 0){
      close_free_all(client_socket, file_buf);
      continue;
    }
    // convert the size of the file from string to uint32
    file_size = atouint32(buf);
    if (file_size == 0) {
      close_free_all(client_socket, file_buf);
      continue;
    };
    printf_styled(STYLE_RED, "Received "); printf_styled(STYLE_DEFAULT, "a size: %d --> accepted\n", file_size);

    printf_styled(STYLE_RED, "About "); printf_styled(STYLE_DEFAULT, "to receive file of %d bytes\n", file_size);
    // sending to the client if it is a small or a big file
    if (file_size > SMALL_FILE_MAX_SIZE){
      if (tcp_send_with_delim(client_socket, BIG_FILE, BIG_FILE_SIZE) < 0){
        close_free_all(client_socket, file_buf);
        continue;
      }
    } else {
      if (tcp_send_with_delim(client_socket, SMALL_FILE, SMALL_FILE_SIZE) < 0){
        close_free_all(client_socket, file_buf);
        continue;
      }
    }


    // receiving all the file
    file_buf = malloc(file_size + 1);
    if (file_buf == NULL){
      close_free_all(client_socket, file_buf);
      continue;
    }
    file_buf[file_size] = (char) 0;
    if (tcp_read(client_socket, file_buf, file_size) != file_size){
      close_free_all(client_socket, file_buf);
      continue;
    }
    
    // send application layer ACK
    tcp_send_with_delim(client_socket, ACCEPTED_FILE, strlen(ACCEPTED_FILE));
    printf_styled(STYLE_RED, "Received a file:\n");
    printf_styled(STYLE_DEFAULT, "%s\n", file_buf);
    


    close_free_all(client_socket, file_buf);
    
  }
}    
