/**
 * @file imgfscmd_functions.c
 * @brief imgFS command line interpreter for imgFS core commands.
 *
 * @author Mia Primorac
 */

#include "error.h"
#include "imgfs.h"
#include "imgfscmd_functions.h"
#include "util.h"   // for _unused

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>

// default values
static const uint32_t default_max_files = 128;
static const uint16_t default_thumb_res =  64;
static const uint16_t default_small_res = 256;

// max values
static const uint16_t MAX_THUMB_RES = 128;
static const uint16_t MAX_SMALL_RES = 512;

//resolution_suffix values
#define EXTENSION_STRING ".jpg"


/********************************************************************
 * Creates a name by joining the image's id and resolution.
 */
static void create_name(const char* img_id, int resolution, char** new_name)
{
    const char* const resolution_suffix = (resolution == ORIG_RES ) ? "_orig"  :
                                          (resolution == SMALL_RES) ? "_small" :
                                          (resolution == THUMB_RES) ? "_thumb" :
                                          "";

    size_t name_size = strlen(img_id) + strlen(resolution_suffix) + strlen(EXTENSION_STRING);
    char* const new_filename = malloc(name_size + 1);
    if (new_filename != NULL) {
        snprintf(new_filename, name_size + 1, "%s%s%s", img_id, resolution_suffix, EXTENSION_STRING);
    }
    *new_name = new_filename;
}

static int write_disk_image(const char *filename, const char *image_buffer, uint32_t image_size)
{
    FILE* file = fopen(filename, "wb");
    if (file == NULL) {
        return ERR_IO;
    }
    if (fwrite(image_buffer, image_size, 1, file) != 1) {
        fclose(file);
        return ERR_IO;
    }
    fclose(file);
    return ERR_NONE;
}

static int read_disk_image(const char *path, char **image_buffer, uint32_t *image_size)
{
    char* buffer_out = NULL;
    long buffer_out_size = 0;
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        return ERR_IO;
    }
    if (fseek(file, 0, SEEK_END) == -1 ||
        (buffer_out_size = ftell(file)) == -1) {
        fclose(file);
        return ERR_IO;
    }

    buffer_out = malloc((size_t)buffer_out_size);
    if (buffer_out == NULL) {
        fclose(file);
        return ERR_OUT_OF_MEMORY;
    }
    if (fseek(file, 0, SEEK_SET) == -1 ||
        fread(buffer_out, (size_t) buffer_out_size, 1, file) != 1) {
        free(buffer_out);
        fclose(file);
        return ERR_IO;
    }
    *image_buffer = buffer_out;
    *image_size   = (uint32_t) buffer_out_size;
    fclose(file);
    return ERR_NONE;
}

/**********************************************************************
 * Displays some explanations.
 ********************************************************************** */
int help(int useless _unused, char** useless_too _unused)
{
    printf("imgfscmd [COMMAND] [ARGUMENTS]\n"
           "  help: displays this help.\n"
           "  list <imgFS_filename>: list imgFS content.\n"
           "  create <imgFS_filename> [options]: create a new imgFS.\n"
           "      options are:\n"
           "          -max_files <MAX_FILES>: maximum number of files.\n"
           "                                  default value is %u\n"
           "                                  maximum value is 4294967295\n"
           "          -thumb_res <X_RES> <Y_RES>: resolution for thumbnail images.\n"
           "                                  default value is %ux%u\n"
           "                                  maximum value is %ux%u\n"
           "          -small_res <X_RES> <Y_RES>: resolution for small images.\n"
           "                                  default value is %ux%u\n"
           "                                  maximum value is %ux%u\n"
           "  read   <imgFS_filename> <imgID> [original|orig|thumbnail|thumb|small]:\n"
           "      read an image from the imgFS and save it to a file.\n"
           "      default resolution is \"original\".\n"
           "  insert <imgFS_filename> <imgID> <filename>: insert a new image in the imgFS.\n"
           "  delete <imgFS_filename> <imgID>: delete image imgID from imgFS.\n",
           default_max_files, default_thumb_res, default_thumb_res, MAX_THUMB_RES, MAX_THUMB_RES,
           default_small_res, default_small_res, MAX_SMALL_RES, MAX_SMALL_RES);
    return ERR_NONE;
}

/**********************************************************************
 * Opens imgFS file and calls do_list().
 ********************************************************************** */
int do_list_cmd(int argc, char** argv)
{
    M_REQUIRE_NON_NULL(argv);
    if (argc <= 0) return ERR_NOT_ENOUGH_ARGUMENTS;
    if (argc != 1) return ERR_INVALID_COMMAND;
    struct imgfs_file file;
    const int err = do_open(argv[0], "rb", &file);
    if (err != ERR_NONE) return err;
    const int listed = do_list(&file, STDOUT, NULL);
    do_close(&file);
    return listed;
}

/**********************************************************************
 * Prepares and calls do_create command.
********************************************************************** */
int do_create_cmd(int argc, char** argv)
{
    puts("Create");
    M_REQUIRE_NON_NULL(argv);
    char* imgfs_filename;
    struct imgfs_file imgfs_file;
    if (argc < 1) {
        return ERR_NOT_ENOUGH_ARGUMENTS;
    }
    imgfs_filename = argv[0];

    uint32_t max_files = default_max_files;
    uint16_t thumb_res_width = default_thumb_res;
    uint16_t thumb_res_height = default_thumb_res;
    uint16_t small_res_width = default_small_res;
    uint16_t small_res_height = default_small_res;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-max_files") == 0) {
            if (i + 1 >= argc) {
                return ERR_NOT_ENOUGH_ARGUMENTS;
            }
            max_files = atouint32(argv[++i]);
            if (max_files == 0) {
                return ERR_MAX_FILES;
            }
        } else if (strcmp(argv[i], "-thumb_res") == 0) {
            if (i + 2 >= argc) {
                return ERR_NOT_ENOUGH_ARGUMENTS;
            }
            thumb_res_width = atouint16(argv[++i]);
            thumb_res_height = atouint16(argv[++i]);
            if (thumb_res_width == 0 || thumb_res_width > MAX_THUMB_RES ||
                thumb_res_height == 0 || thumb_res_height > MAX_THUMB_RES) {
                return ERR_RESOLUTIONS;
            }
        } else if (strcmp(argv[i], "-small_res") == 0) {
            if (i + 2 >= argc) {
                return ERR_NOT_ENOUGH_ARGUMENTS;
            }
            small_res_width = atouint16(argv[++i]);
            small_res_height = atouint16(argv[++i]);
            if (small_res_width == 0 || small_res_width > MAX_SMALL_RES ||
                small_res_height == 0 || small_res_height > MAX_SMALL_RES) {
                return ERR_RESOLUTIONS;
            }
        } else {
            return ERR_INVALID_ARGUMENT;
        }
    }
    imgfs_file.header.max_files = max_files;
    imgfs_file.header.resized_res[0] = thumb_res_width;
    imgfs_file.header.resized_res[1] = thumb_res_height;
    imgfs_file.header.resized_res[2] = small_res_width;
    imgfs_file.header.resized_res[3] = small_res_height;
    int ret = do_create(imgfs_filename, &imgfs_file);
    do_close(&imgfs_file);
    return ret;
}

/**********************************************************************
 * Deletes an image from the imgFS.
 */
int do_delete_cmd(int argc, char** argv)
{
    M_REQUIRE_NON_NULL(argv);
    struct imgfs_file imgfs_file;
    int ret = ERR_NONE;
    char* img_id = NULL;
    if (argc < 2) {
        return ERR_NOT_ENOUGH_ARGUMENTS;
    }
    if (argc != 2) {
        return ERR_INVALID_COMMAND;
    }
    char* imgfs_filename = argv[0];
    img_id = argv[1];
    size_t img_id_size = strlen(img_id);
    if (img_id_size == 0 || img_id_size > MAX_IMG_ID) {
        return ERR_INVALID_IMGID;
    }
    ret = do_open(imgfs_filename, "rb+", &imgfs_file);
    if (ret != ERR_NONE) return ret;
    ret = do_delete(img_id, &imgfs_file);
    do_close(&imgfs_file);
    return ret;
}


int do_read_cmd(int argc, char **argv)
{
    M_REQUIRE_NON_NULL(argv);
    if (argc != 2 && argc != 3) return ERR_NOT_ENOUGH_ARGUMENTS;

    const char * const img_id = argv[1];

    const int resolution = (argc == 3) ? resolution_atoi(argv[2]) : ORIG_RES;
    if (resolution == -1) return ERR_RESOLUTIONS;

    struct imgfs_file myfile;
    zero_init_var(myfile);
    int error = do_open(argv[0], "rb+", &myfile);
    if (error != ERR_NONE) return error;

    char *image_buffer = NULL;
    uint32_t image_size = 0;
    error = do_read(img_id, resolution, &image_buffer, &image_size, &myfile);
    do_close(&myfile);
    if (error != ERR_NONE) {
        return error;
    }

    // Extracting to a separate image file.
    char* tmp_name = NULL;
    create_name(img_id, resolution, &tmp_name);
    if (tmp_name == NULL) return ERR_OUT_OF_MEMORY;
    error = write_disk_image(tmp_name, image_buffer, image_size);
    free(tmp_name);
    free(image_buffer);

    return error;
}

int do_insert_cmd(int argc, char **argv)
{
    M_REQUIRE_NON_NULL(argv);
    if (argc != 3) return ERR_NOT_ENOUGH_ARGUMENTS;

    struct imgfs_file myfile;
    zero_init_var(myfile);
    int error = do_open(argv[0], "rb+", &myfile);
    if (error != ERR_NONE) return error;

    char *image_buffer = NULL;
    uint32_t image_size;

    // Reads image from the disk.
    error = read_disk_image (argv[2], &image_buffer, &image_size);
    if (error != ERR_NONE) {
        do_close(&myfile);
        return error;
    }

    error = do_insert(image_buffer, image_size, argv[1], &myfile);
    free(image_buffer);
    do_close(&myfile);
    return error;
}


