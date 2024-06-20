#include "imgfs.h"
#include "image_content.h"
#include "image_dedup.h"
#include "error.h"
#include <openssl/sha.h> // for SHA256()
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

int do_insert(const char *image_buffer, size_t image_size, const char *img_id, struct imgfs_file *imgfs_file)
{
    M_REQUIRE_NON_NULL(image_buffer);
    M_REQUIRE_NON_NULL(img_id);
    M_REQUIRE_NON_NULL(imgfs_file);
    struct imgfs_header* header = &imgfs_file->header;
    int ret = ERR_NONE;

    if (header->nb_files >= header->max_files) {
        return ERR_IMGFS_FULL;
    }
    bool empty_spot_found         = false;
    uint32_t metadata_index       = 0;
    struct img_metadata* metadata = imgfs_file->metadata;
    for(; !empty_spot_found && metadata_index < header->max_files; ++metadata, ++metadata_index) {
        if (metadata->is_valid == EMPTY) {
            empty_spot_found = true;
        }
    }
    --metadata;
    --metadata_index;

    if (empty_spot_found) {
        SHA256((const unsigned char*)image_buffer, image_size, metadata->SHA);
        strncpy(metadata->img_id, img_id, MAX_IMG_ID + 1);
        ret = get_resolution(&metadata->orig_res[1], &metadata->orig_res[0], image_buffer, image_size);
        if (ret != ERR_NONE) {
            return ret;
        }
        ret = do_name_and_content_dedup(imgfs_file, metadata_index);
        if (ret != ERR_NONE) {
            return ret;
        }
        if (metadata->offset[ORIG_RES] == 0) {
            if (fseek(imgfs_file->file, 0, SEEK_END) == -1) {
                return ERR_IO;
            }
            uint64_t orig_res_offset = (uint64_t) imgfs_file->file->_offset;
            if (fwrite(image_buffer, image_size, 1, imgfs_file->file) != 1) {
                return ERR_IO;
            }
            metadata->offset[THUMB_RES] = 0;
            metadata->offset[SMALL_RES] = 0;
            metadata->offset[ORIG_RES ] = orig_res_offset;
            metadata->size[THUMB_RES] = 0;
            metadata->size[SMALL_RES] = 0;
            metadata->size[ORIG_RES ] = (uint32_t) image_size;
        }
        metadata->is_valid = NON_EMPTY;
        header->nb_files += 1;
        header->version += 1;

        if (fseek(imgfs_file->file, 0, SEEK_SET) ||
            fwrite(header, sizeof(struct imgfs_header), 1, imgfs_file->file) != 1) {
            return ERR_IO;
        }
        if (fseek(imgfs_file->file, sizeof(struct imgfs_header) + metadata_index*sizeof(struct img_metadata), SEEK_SET) == -1 ||
            fwrite(metadata, sizeof(struct img_metadata), 1, imgfs_file->file) != 1) {
            return ERR_IO;
        }
    }

    return ret;
}
