/*
 * @file imgfs_server.c
 * @brief ImgFS server part, main
 *
 * @author Konstantinos Prasopoulos
 */

#include "error.h"
#include "util.h"
#include "imgfs.h"
#include "socket_layer.h"
#include "http_net.h"
#include "imgfs_server_service.h"

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h> // abort()

/********************************************************************/
static void signal_handler(int sig_num _unused)
{
    server_shutdown();
    exit(0);
}

/********************************************************************/
static void set_signal_handler(void)
{
    struct sigaction action;
    if (sigemptyset(&action.sa_mask) == -1) {
        perror("sigemptyset() in set_signal_handler()");
        abort();
    }
    action.sa_handler = signal_handler;
    action.sa_flags   = 0;
    if ((sigaction(SIGINT,  &action, NULL) < 0) ||
        (sigaction(SIGTERM,  &action, NULL) < 0)) {
        perror("sigaction() in set_signal_handler()");
        abort();
    }
}

/********************************************************************/

int main (int argc, char *argv[])
{
    set_signal_handler();
    int ret = server_startup(argc, argv);
    if (ret < 0) {
        return ret;
    }
    while ((ret = http_receive()) ==  ERR_NONE);
    server_shutdown();

    fprintf(stderr, "http_receive() failed\n");
    fprintf(stderr, "%s\n", ERR_MSG(ret));

    return ret;
}
