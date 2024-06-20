#include "error.h"
#include "image_content.h"
#include "imgfs.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int do_read(const char *img_id, int resolution, char **image_buffer, uint32_t *image_size, struct imgfs_file *imgfs_file)
{
    M_REQUIRE_NON_NULL(img_id);
    M_REQUIRE_NON_NULL(image_buffer);
    M_REQUIRE_NON_NULL(image_size);
    M_REQUIRE_NON_NULL(imgfs_file);
    const struct imgfs_header* const header = &imgfs_file->header;
    int ret = ERR_NONE;

    bool image_found                     = false;
    uint32_t metadata_index              = 0;
    struct img_metadata* metadata        = &imgfs_file->metadata[0];
    for(; !image_found && metadata_index < header->max_files; ++metadata, ++metadata_index) {
        if (metadata->is_valid == NON_EMPTY && strcmp(metadata->img_id, img_id) == 0) {
            image_found = true;
        }
    }
    --metadata;
    --metadata_index;

    if (!image_found) {
        return ERR_IMAGE_NOT_FOUND;
    }

    if ((metadata->size[resolution] == 0 || metadata->offset[resolution] == 0) && resolution != ORIG_RES) {
        ret = lazily_resize(resolution, imgfs_file, metadata_index);
        if (ret != ERR_NONE) {
            return ret;
        }
    }
    char* buffer_out = malloc(metadata->size[resolution]);
    if (buffer_out == NULL) {
        return ERR_OUT_OF_MEMORY;
    }
    if (fseek(imgfs_file->file, (signed) metadata->offset[resolution], SEEK_SET) ||
        fread(buffer_out, metadata->size[resolution], 1, imgfs_file->file) != 1) {
        free(buffer_out);
        return ERR_IO;
    }
    *image_buffer = buffer_out;
    *image_size   = metadata->size[resolution];

    return ret;
}
