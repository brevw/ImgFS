/*
 * @file imgfs_server_services.c
 * @brief ImgFS server part, bridge between HTTP server layer and ImgFS library
 *
 * @author Konstantinos Prasopoulos
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h> // uint16_t
#include <stdbool.h>
#include <vips/vips.h>
#include <pthread.h>
#include <stdbool.h>

#include "error.h"
#include "http_prot.h"
#include "util.h" // atouint16
#include "imgfs.h"
#include "http_net.h"
#include "imgfs_server_service.h"

// Main in-memory structure for imgFS
static struct imgfs_file fs_file;
static uint16_t server_port;
static pthread_mutex_t mutex;

#define URI_ROOT "/imgfs"
#define STARTING_VALID_PORT 1024

/**********************************************************************
 * Sends error message.
 ********************************************************************** */
static int reply_error_msg(int connection, int error)
{
#define ERR_MSG_SIZE 256
    char err_msg[ERR_MSG_SIZE]; // enough for any reasonable err_msg
    if (snprintf(err_msg, ERR_MSG_SIZE, "Error: %s\n", ERR_MSG(error)) < 0) {
        fprintf(stderr, "reply_error_msg(): sprintf() failed...\n");
        return ERR_RUNTIME;
    }
    return http_reply(connection, "500 Internal Server Error", "",
                      err_msg, strlen(err_msg));
}

/**********************************************************************
 * Sends 302 OK message.
 ********************************************************************** */
static int reply_302_msg(int connection)
{
    char location[ERR_MSG_SIZE];
    if (snprintf(location, ERR_MSG_SIZE, "Location: http://localhost:%d/" BASE_FILE HTTP_LINE_DELIM,
                 server_port) < 0) {
        fprintf(stderr, "reply_302_msg(): sprintf() failed...\n");
        return ERR_RUNTIME;
    }
    return http_reply(connection, "302 Found", location, "", 0);
}

static int handle_list_call(int connection)
{
    char* json_string;
    if (pthread_mutex_lock(&mutex)) {
        return ERR_THREADING;
    }
    int res = do_list(&fs_file, JSON, &json_string);
    if (pthread_mutex_unlock(&mutex)) {
        return ERR_THREADING;
    }
    if (res != ERR_NONE) {
        return reply_error_msg(connection, res);
    }
    int reply = http_reply(connection, HTTP_OK, "Content-Type: application/json" HTTP_LINE_DELIM, json_string, strlen(json_string));
    free(json_string);
    return reply;
}

static int handle_read_call(struct http_message* msg, int connection)
{
    char res_string[15];
    int res = 0;
    char img_id[MAX_IMG_ID + 1];
    int ret = http_get_var(&msg->uri, "res", res_string, 15);
    if (ret == 0) ret = ERR_NOT_ENOUGH_ARGUMENTS;
    if (ret <= 0) {
        return reply_error_msg(connection, ret);
    }
    res = resolution_atoi(res_string);
    if (res == -1) {
        return reply_error_msg(connection, ERR_RESOLUTIONS);
    }
    ret = http_get_var(&msg->uri, "img_id", img_id, MAX_IMG_ID + 1);
    if (ret == 0) ret = ERR_NOT_ENOUGH_ARGUMENTS;
    if (ret <= 0) {
        return reply_error_msg(connection, ret);
    }

    char* image_buffer = NULL;
    uint32_t image_size = 0;
    if (pthread_mutex_lock(&mutex)) {
        return ERR_THREADING;
    }
    ret = do_read(img_id, res, &image_buffer, &image_size, &fs_file);
    if (pthread_mutex_unlock(&mutex)) {
        free(image_buffer);
        return ERR_THREADING;
    }
    if (ret != ERR_NONE) {
        free(image_buffer);
        return reply_error_msg(connection, ret);
    }
    ret = http_reply(connection, HTTP_OK, "Content-Type: image/jpeg" HTTP_LINE_DELIM, image_buffer, (size_t) image_size);
    free(image_buffer);
    return ret;
}

static int handle_delete_call(struct http_message* msg, int connection)
{
    char img_id[MAX_IMG_ID + 1];
    int ret = http_get_var(&msg->uri, "img_id", img_id, MAX_IMG_ID + 1);
    if (ret == 0) ret = ERR_NOT_ENOUGH_ARGUMENTS;
    if (ret <= 0) {
        return reply_error_msg(connection, ret);
    }
    if (pthread_mutex_lock(&mutex)) {
        return ERR_THREADING;
    }
    ret = do_delete(img_id, &fs_file);
    if (pthread_mutex_unlock(&mutex)) {
        return ERR_THREADING;
    }
    if (ret != ERR_NONE) {
        return reply_error_msg(connection, ret);
    }
    ret = reply_302_msg(connection);
    return ret;

}

static int handle_insert_call(struct http_message* msg, int connection)
{
    char name[MAX_IMG_ID + 1];
    int ret = http_get_var(&msg->uri, "name", name, MAX_IMG_ID + 1);
    if (ret == 0) ret = ERR_NOT_ENOUGH_ARGUMENTS;
    if (ret <= 0) {
        return reply_error_msg(connection, ret);
    }
    size_t image_size = msg->body.len;
    char* image_buffer = malloc(image_size);
    if (image_buffer == NULL) {
        return reply_error_msg(connection, ERR_OUT_OF_MEMORY);
    }
    memcpy(image_buffer, msg->body.val, image_size);
    if (pthread_mutex_lock(&mutex)) {
        free(image_buffer);
        return ERR_THREADING;
    }
    ret = do_insert(image_buffer, image_size, name, &fs_file);
    if (pthread_mutex_unlock(&mutex)) {
        free(image_buffer);
        return ERR_THREADING;
    }
    free(image_buffer);
    if (ret != ERR_NONE) {
        return reply_error_msg(connection, ret);
    }
    ret = reply_302_msg(connection);
    return ret;
}

/**********************************************************************
 * Simple handling of http message.
 ********************************************************************** */
int handle_http_message(struct http_message* msg, int connection)
{
    M_REQUIRE_NON_NULL(msg);
    if (http_match_verb(&msg->uri, "/") || http_match_uri(msg, "/index.html")) {
        return http_serve_file(connection, BASE_FILE);
    }
    debug_printf("handle_http_message() on connection %d. URI: %.*s\n",
                 connection,
                 (int) msg->uri.len, msg->uri.val);
    if (http_match_uri(msg, URI_ROOT "/list")) {
        return handle_list_call(connection);
    } else if (http_match_uri(msg, URI_ROOT "/read")) {
        return handle_read_call(msg, connection);
    } else if (http_match_uri(msg, URI_ROOT "/delete")) {
        return handle_delete_call(msg, connection);
    } else if (http_match_uri(msg, URI_ROOT "/insert") &&  http_match_verb(&msg->method, "POST")) {
        return handle_insert_call(msg, connection);
    } else {
        return reply_error_msg(connection, ERR_INVALID_COMMAND);
    }
}

static void close_all_and_free(bool destroy_mutex, bool close_imgfs_file){
    fprintf(stderr, "Shutting down...\n");
    http_close();
    vips_shutdown();
    if (close_imgfs_file) do_close(&fs_file);
    if (destroy_mutex) pthread_mutex_destroy(&mutex);
}

/********************************************************************//**
 * Startup function. Create imgFS file and load in-memory structure.
 * Pass the imgFS file name as argv[1] and optionnaly port number as argv[2]
 ********************************************************************** */
int server_startup (int argc, char **argv)
{
    if (VIPS_INIT(argv[0])) {
        close_all_and_free(false, false);
        return ERR_IMGLIB;
    }
    if (pthread_mutex_init(&mutex, NULL)) {
        close_all_and_free(false, false);
        return ERR_THREADING;
    }
    if (argc < 2){
        close_all_and_free(true, false);
        return ERR_NOT_ENOUGH_ARGUMENTS;
    } 
    if (argc > 3){
        close_all_and_free(true, false);
        return ERR_INVALID_COMMAND;
    } 
    int ret = do_open(argv[1], "rb+", &fs_file);
    if (ret != ERR_NONE) {
        close_all_and_free(true, false);
        return ret;
    }
    print_header(&fs_file.header);
    if (argc == 3) {
        server_port = atouint16(argv[2]);
        // case where server_port overflows or is not in the range of valid port numbers
        if (server_port < STARTING_VALID_PORT) {
            close_all_and_free(true, true);
            return ERR_INVALID_ARGUMENT;
        }
    } else {
        server_port = DEFAULT_LISTENING_PORT;
    }
    ret = http_init(server_port, handle_http_message);
    if (ret < 0) {
        close_all_and_free(true, true);
        return ret;
    }
    printf("ImgFS server started on http://localhost:%u\n", server_port);

    return 0;


}

/********************************************************************//**
 * Shutdown function. Free the structures and close the file.
 ********************************************************************** */
void server_shutdown (void)
{
    fprintf(stderr, "Shutting down...\n");
    http_close();
    do_close(&fs_file);
    vips_shutdown();
    pthread_mutex_destroy(&mutex);
}
