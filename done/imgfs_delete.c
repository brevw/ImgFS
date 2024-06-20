#include "error.h"
#include "imgfs.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

int do_delete(const char *img_id, struct imgfs_file *imgfs_file)
{
    M_REQUIRE_NON_NULL(img_id);
    M_REQUIRE_NON_NULL(imgfs_file);

    int found_image = false;
    uint32_t index_image = 0;
    struct imgfs_header* header = &imgfs_file->header;
    // case where DB is empty
    if (header->nb_files == 0) {
        return ERR_IMAGE_NOT_FOUND;
    }
    struct img_metadata* metadata = imgfs_file->metadata;
    struct img_metadata* const last_metadata = metadata + header->max_files;

    //finding the image with the same id
    for (; !found_image && metadata < last_metadata; ++metadata, ++index_image) {
        if(strcmp(metadata->img_id, img_id) == 0 && metadata->is_valid == NON_EMPTY) {
            found_image = true;
        }
    }
    --metadata;
    --index_image;

    if (!found_image) {
        return ERR_IMAGE_NOT_FOUND;
    }

    //invalidating image in memory then on disk
    metadata->is_valid = EMPTY;
    if (fseek(imgfs_file->file, sizeof(struct imgfs_header) + index_image * sizeof(struct img_metadata), SEEK_SET) ||
        fwrite(metadata, sizeof(struct img_metadata), 1, imgfs_file->file) != 1) {
        return ERR_IO;
    }

    //updating the header in memory then on disk
    ++header->version;
    --header->nb_files;
    if (fseek(imgfs_file->file, 0, SEEK_SET) ||
        fwrite(header, sizeof(struct imgfs_header), 1, imgfs_file->file) != 1) {
        return ERR_IO;
    }

    return ERR_NONE;
}
