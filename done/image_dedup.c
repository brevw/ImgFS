#include "image_dedup.h"
#include "error.h"
#include "imgfs.h"
#include <openssl/sha.h>
#include <string.h>
#include <stdbool.h>

int do_name_and_content_dedup(struct imgfs_file *imgfs_file, uint32_t index)
{
    M_REQUIRE_NON_NULL(imgfs_file);
    if (index >= imgfs_file->header.max_files) {
        return ERR_IMAGE_NOT_FOUND;
    }

    const struct img_metadata* metadata = imgfs_file->metadata;
    struct img_metadata* const target_metadata = imgfs_file->metadata + index;
    const struct img_metadata* const metadata_last = metadata + imgfs_file->header.max_files;
    bool found_duplicate = false;
    //find metadata corresponding the original duplicate
    for(; metadata < metadata_last && !found_duplicate; ++metadata) {
        if(target_metadata != metadata && metadata->is_valid == NON_EMPTY) {
            if (strncmp(metadata->img_id, target_metadata->img_id, MAX_IMG_ID) == 0) {
                return ERR_DUPLICATE_ID;
            }
            if (strncmp((const char*)metadata->SHA, (const char*)target_metadata->SHA, SHA256_DIGEST_LENGTH) == 0) {
                found_duplicate = true;
                //assigning offset and size of the original to the duplicate
                target_metadata->offset[THUMB_RES] = metadata->offset[THUMB_RES];
                target_metadata->offset[SMALL_RES] = metadata->offset[SMALL_RES];
                target_metadata->offset[ORIG_RES ] = metadata->offset[ORIG_RES ];
                target_metadata->size[THUMB_RES] = metadata->size[THUMB_RES];
                target_metadata->size[SMALL_RES] = metadata->size[SMALL_RES];
                target_metadata->size[ORIG_RES ] = metadata->size[ORIG_RES ];
            }

        }
    }

    if (!found_duplicate) {
        target_metadata->offset[ORIG_RES] = 0;
    }
    return ERR_NONE;
}
