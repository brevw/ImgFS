#include "error.h"
#include "imgfs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define PRINT_ITEMS_WRITTEN(written) printf("%zu item(s) written\n", (written))

int do_create(const char *imgfs_filename, struct imgfs_file *imgfs_file)
{
    M_REQUIRE_NON_NULL(imgfs_filename);
    M_REQUIRE_NON_NULL(imgfs_file);
    struct imgfs_header *header = &imgfs_file->header;
    //init relevant header fields
    strncpy(header->name, CAT_TXT, sizeof(header->name) - 1);
    header->name[sizeof(header->name) - 1] = (char) 0;
    header->version   = 0;
    header->nb_files  = 0;
    header->unused_32 = 0;
    header->unused_64 = 0;
    imgfs_file->metadata = calloc(header->max_files, sizeof(struct img_metadata));
    if (imgfs_file->metadata == NULL) {
        imgfs_file->file = NULL;
        return ERR_OUT_OF_MEMORY;
    }
    imgfs_file->file = fopen(imgfs_filename, "wb");
    if (imgfs_file->file == NULL) {
        return ERR_IO;
    }

    size_t written = 0;
    size_t temp_written = 0;
    //writing file to disk
    if ((temp_written = fwrite(header, sizeof(struct imgfs_header), 1, imgfs_file->file)) != 1) {
        return ERR_IO;
    }
    written += temp_written;
    if ((temp_written = fwrite(imgfs_file->metadata, sizeof(struct img_metadata),
                               header->max_files, imgfs_file->file)) != header->max_files) {
        PRINT_ITEMS_WRITTEN(written);
        return ERR_IO;
    }
    written += temp_written;
    PRINT_ITEMS_WRITTEN(written);
    return ERR_NONE;

}
