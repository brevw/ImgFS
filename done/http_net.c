/*
 * @file http_net.c
 * @brief HTTP server layer for CS-202 project
 *
 * @author Konstantinos Prasopoulos
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <limits.h>
#include <pthread.h>

#include "http_prot.h"
#include "http_net.h"
#include "socket_layer.h"
#include "util.h"
#include "error.h"

static int passive_socket = -1;
static EventCallback cb;

#define MK_OUR_ERR(X) \
static int our_ ## X = X

MK_OUR_ERR(ERR_NONE);
MK_OUR_ERR(ERR_INVALID_ARGUMENT);
MK_OUR_ERR(ERR_OUT_OF_MEMORY);
MK_OUR_ERR(ERR_IO);
MK_OUR_ERR(ERR_RUNTIME);

static const size_t HTTP_HDR_END_DELIM_SIZE = sizeof(HTTP_HDR_END_DELIM) - 1;
static const size_t HTTP_LINE_DELIM_SIZE    = sizeof(HTTP_LINE_DELIM) - 1;
static const size_t HTTP_PROTOCOL_ID_SIZE   = sizeof(HTTP_PROTOCOL_ID) - 1;
static const size_t HTTP_HDR_KV_DELIM_SIZE  = sizeof(HTTP_HDR_KV_DELIM) - 1;
static const char CONTENT_LENGTH[]          = "Content-Length";
static const size_t CONTENT_LENGTH_SIZE     = sizeof(CONTENT_LENGTH) - 1;

#define PARSING_BUF_MAX 20


// close the buffer and the socker.
// if error is one of our standard errors return a pointer to it else return a pointer to ERR_IO
static int* free_close_all(char* buf, int* active_socket, int err)
{
    free(buf);
    if (close(*active_socket) == -1) {
        perror("close() in free_close_all()");
        free(active_socket);
        return &our_ERR_IO;
    }
    free(active_socket);
    switch (err) {
    case ERR_NONE:
        return &our_ERR_NONE;
    case ERR_INVALID_ARGUMENT:
        return &our_ERR_INVALID_ARGUMENT;
    case ERR_OUT_OF_MEMORY:
        return &our_ERR_OUT_OF_MEMORY;
    case ERR_RUNTIME:
        return &our_ERR_RUNTIME;
    default:
        return &our_ERR_IO;
    }
}

/*******************************************************************
 * Handle connection
 */
static void *handle_connection(void *arg)
{
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT );
    sigaddset(&mask, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    if (arg == NULL) return &our_ERR_INVALID_ARGUMENT;
    int* active_socket = (int*)arg;
    char* rcvbuf = malloc(MAX_HEADER_SIZE);
    if (rcvbuf == NULL) {
        return free_close_all(rcvbuf, active_socket, ERR_OUT_OF_MEMORY);
    }

    ssize_t ret = 0;
    char* where_to_read = rcvbuf;
    size_t read_bytes = 0;
    bool extended_message = false;
    struct http_message msg;
    int content_len = 0;
    size_t how_much_to_read = 0;

    while(true) {
        how_much_to_read = !extended_message ? MAX_HEADER_SIZE - read_bytes : (size_t) content_len - msg.body.len;
        ret = tcp_read(*active_socket, where_to_read, how_much_to_read);
        // case of an error or socket is closed
        if (ret <= 0) {
            if (ret < 0) fprintf(stderr, "handle_connection: tcp_read error\n");
            else fprintf(stderr, "handle_connection: Connection closed by client\n");
            return free_close_all(rcvbuf, active_socket, (int) ret);
        }
        read_bytes += (size_t) ret;
        where_to_read += ret;

        int parse_result = http_parse_message(rcvbuf, read_bytes, &msg, &content_len);
        if (parse_result < 0) {
            fprintf(stderr, "handle_connection: http_parse_message error\n");
            return free_close_all(rcvbuf, active_socket, parse_result);
        }
        if (parse_result == 0 && !extended_message && content_len > 0 && read_bytes < MAX_HEADER_SIZE + (size_t) content_len) {
            char* temp = realloc(rcvbuf, MAX_HEADER_SIZE + (size_t) content_len);
            if (temp == NULL) {
                fprintf(stderr, "handle_connection: Out of memory during realloc\n");
                return free_close_all(rcvbuf, active_socket, ERR_OUT_OF_MEMORY);
            }
            rcvbuf = temp;
            where_to_read = rcvbuf + read_bytes;
            extended_message = true;
            // go back and read till the end of the body
        }

        // check if we exceeded the header size constrain
        if (parse_result == 0 && content_len == 0 && read_bytes == MAX_HEADER_SIZE) {
            fprintf(stderr, "handle_connection: Header size exceeded MAX_HEADER_SIZE\n");
            return free_close_all(rcvbuf, active_socket, ERR_IO);
        }

        if (parse_result > 0) {
            int callback_result = cb(&msg, *active_socket);
            if (callback_result < 0) {
                fprintf(stderr, "handle_connection: EventCallback error\n");
                return free_close_all(rcvbuf, active_socket, callback_result);
            }

            // reset variables and garbage collect
            read_bytes = 0;
            extended_message = false;
            content_len = 0;
            free(rcvbuf);
            rcvbuf = malloc(MAX_HEADER_SIZE);
            if (rcvbuf == NULL) {
                fprintf(stderr, "handle_connection: Out of memory during reset\n");
                return free_close_all(rcvbuf, active_socket, ERR_OUT_OF_MEMORY);
            }
            where_to_read = rcvbuf;
        }

    }
    // should never be reached
    return free_close_all(rcvbuf, active_socket, ERR_NONE);
}


/*******************************************************************
 * Init connection
 */
int http_init(uint16_t port, EventCallback callback)
{
    passive_socket = tcp_server_init(port);
    cb = callback;
    return passive_socket;
}

/*******************************************************************
 * Close connection
 */
void http_close(void)
{
    if (passive_socket > 0) {
        if (close(passive_socket) == -1)
            perror("close() in http_close()");
        else
            passive_socket = -1;
    }
}

/*******************************************************************
 * Receive content
 */
int http_receive(void)
{
    int active_socket_ = tcp_accept(passive_socket);
    // valid socket case
    if (active_socket_ >= 0) {
        int* active_socket = malloc(sizeof(int));
        if (active_socket == NULL) {
            return ERR_OUT_OF_MEMORY;
        }
        *active_socket = active_socket_;
        pthread_attr_t thread_attr;
        pthread_t thread;
        if (pthread_attr_init(&thread_attr)) {
            free(active_socket);
            return ERR_THREADING;
        }
        if (pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED)) {
            free(active_socket);
            pthread_attr_destroy(&thread_attr);
            return ERR_THREADING;
        }
        if (pthread_create(&thread, &thread_attr, handle_connection, active_socket)) {
            free(active_socket);
            pthread_attr_destroy(&thread_attr);
            return ERR_THREADING;
        }
        if (pthread_attr_destroy(&thread_attr)) {
            free(active_socket);
            return ERR_THREADING;
        }
        return ERR_NONE;
    } else {
        return ERR_IO;
    }
}

/*******************************************************************
 * Serve a file content over HTTP
 */
int http_serve_file(int connection, const char* filename)
{
    M_REQUIRE_NON_NULL(filename);

    // open file
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "http_serve_file(): Failed to open file \"%s\"\n", filename);
        return http_reply(connection, "404 Not Found", "", "", 0);
    }

    // get its size
    fseek(file, 0, SEEK_END);
    const long pos = ftell(file);
    if (pos < 0) {
        fprintf(stderr, "http_serve_file(): Failed to tell file size of \"%s\"\n",
                filename);
        fclose(file);
        return ERR_IO;
    }
    rewind(file);
    const size_t file_size = (size_t) pos;

    // read file content
    char* const buffer = calloc(file_size + 1, 1);
    if (buffer == NULL) {
        fprintf(stderr, "http_serve_file(): Failed to allocate memory to serve \"%s\"\n", filename);
        fclose(file);
        return ERR_IO;
    }

    const size_t bytes_read = fread(buffer, 1, file_size, file);
    if (bytes_read != file_size) {
        fprintf(stderr, "http_serve_file(): Failed to read \"%s\"\n", filename);
        fclose(file);
        return ERR_IO;
    }

    // send the file
    const int  ret = http_reply(connection, HTTP_OK,
                                "Content-Type: text/html; charset=utf-8" HTTP_LINE_DELIM,
                                buffer, file_size);

    // garbage collecting
    fclose(file);
    free(buffer);
    return ret;
}

// return nbr of digits of a number and (0) in case of an error 
static size_t nbr_of_digits (size_t n)
{
    char buf[PARSING_BUF_MAX + 1];
    if (snprintf(buf, PARSING_BUF_MAX + 1, "%zu", n) <= 0){
        return 0;
    }
    return strlen(buf);
}

/*******************************************************************
 * Create and send HTTP reply
 */
int http_reply(int connection, const char* status, const char* headers, const char *body, size_t body_len)
{
    if ((body == NULL && body_len != 0) || headers == NULL || status == NULL) {
        return ERR_INVALID_COMMAND;
    }
    // case of empty body
    if (body == NULL) {
        body = "";
    }

    // compute number of digits of body_len and no errors occured 
    size_t body_len_digits = nbr_of_digits(body_len);
    if (body_len_digits == 0){
        return ERR_IO;
    }

    size_t http_response_len = HTTP_PROTOCOL_ID_SIZE + strlen(status) + HTTP_LINE_DELIM_SIZE+ strlen(headers) +
                               CONTENT_LENGTH_SIZE + HTTP_HDR_KV_DELIM_SIZE + body_len_digits +
                               HTTP_HDR_END_DELIM_SIZE + body_len;
    size_t http_response_without_body_len = http_response_len - body_len;
    if (http_response_len > INT_MAX) {
        return ERR_IO;
    }
    char* http_response = malloc(http_response_len + 1);
    if (http_response == NULL) {
        return ERR_OUT_OF_MEMORY;
    }
    int joint_len = snprintf(http_response, http_response_without_body_len + 1, "%s%s%s%s%s%s%zu%s",
                             HTTP_PROTOCOL_ID, status, HTTP_LINE_DELIM, headers, CONTENT_LENGTH,
                             HTTP_HDR_KV_DELIM, body_len, HTTP_HDR_END_DELIM);
    if ((size_t) joint_len != http_response_without_body_len) {
        free(http_response);
        return ERR_IO;
    }
    memcpy(http_response + http_response_without_body_len, body, body_len);


    ssize_t ret = tcp_send(connection, http_response, http_response_len);
    if (ret < 0 || (size_t) ret != http_response_len) {
        free(http_response);
        return ERR_IO;
    }
    free(http_response);
    return ERR_NONE;
}
