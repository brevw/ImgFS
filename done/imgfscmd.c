/**
 * @file imgfscmd.c
 * @brief imgFS command line interpreter for imgFS core commands.
 *
 * Image Filesystem Command Line Tool
 *
 * @author Mia Primorac
 */

#include "error.h"
#include "imgfs.h"
#include "imgfscmd_functions.h"
#include "util.h"   // for _unused

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <vips/vips.h>

typedef int (*command)(int, char* []);
struct command_mapping {
    const char* const name;
    command func;
};
static const struct command_mapping commands[] = {
    {"list", do_list_cmd},
    {"create", do_create_cmd},
    {"help", help},
    {"delete", do_delete_cmd},
    {"insert", do_insert_cmd},
    {"read", do_read_cmd}
};
static size_t COMMANDS_SIZE = (sizeof(commands) / sizeof(commands[0]));

/*******************************************************************************
 * MAIN
 */
int main(int argc, char* argv[])
{
    if (VIPS_INIT(argv[0])) {
        return ERR_IMGLIB;
    }
    int ret = ERR_NONE;

    if (argc < 2) {
        ret = ERR_NOT_ENOUGH_ARGUMENTS;
    } else {
        int found_cmd = false;
        const char* const target_cmd = argv[1];
        const struct command_mapping* command = &commands[0];
        const struct command_mapping* const endCommand = commands + COMMANDS_SIZE;
        for (; !found_cmd && command < endCommand; ++command) {
            if (strcmp(target_cmd, command->name) == 0) {
                ret = command->func(argc - 2, argv + 2);
                found_cmd = true;
            }
        }
        if (!found_cmd) ret = ERR_INVALID_COMMAND;

        argc--; argv++; // skips command call name
    }

    if (ret) {
        fprintf(stderr, "ERROR: %s\n", ERR_MSG(ret));
        help(argc, argv);
    }

    vips_shutdown();
    return ret;
}

