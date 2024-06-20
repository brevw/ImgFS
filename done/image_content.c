#include "image_content.h"
#include "error.h"
#include "util.h"
#include <stdlib.h>
#include <vips/vips.h>

//
static void free_all(void* buffer_in, void* buffer_out, VipsImage* image_in, VipsImage* image_out)
{
    free(buffer_in);
    free(buffer_out);
    g_object_unref(VIPS_OBJECT(image_in));
    g_object_unref(VIPS_OBJECT(image_out));
}

int lazily_resize(int resolution, struct imgfs_file *imgfs_file, size_t index)
{
    M_REQUIRE_NON_NULL(imgfs_file);
    // cache header and metadata
    struct imgfs_header* const header   = &imgfs_file->header;
    struct img_metadata* const metadata = imgfs_file->metadata + index;
    // buffers to load image from disk and to store resized image
    void* buffer_in = NULL;
    void* buffer_out = NULL;
    size_t buffer_out_len = 0;
    // internal representation of image and resized image in vips
    VipsImage* image_in = NULL;
    VipsImage* image_out_resized = NULL;

    if (!(0 <= resolution && resolution < NB_RES)) {
        return ERR_RESOLUTIONS;
    }
    if (index >= header->max_files || metadata->is_valid == EMPTY) {
        return ERR_INVALID_IMGID;
    }
    if (resolution == ORIG_RES || metadata->size[resolution] != 0) {
        return ERR_NONE;
    }

    uint32_t orig_res_size = metadata->size[ORIG_RES];
    buffer_in = malloc(orig_res_size);
    if (buffer_in == NULL) {
        return ERR_OUT_OF_MEMORY;
    }
    if (fseek(imgfs_file->file, (long) metadata->offset[ORIG_RES], SEEK_SET) ||
        fread(buffer_in, orig_res_size, 1, imgfs_file->file) != 1) {
        free_all(buffer_in, buffer_out, image_in, image_out_resized);
        return ERR_IO;
    }

    if (vips_jpegload_buffer(buffer_in, orig_res_size, &image_in, NULL) == -1) {
        free_all(buffer_in, buffer_out, image_in, image_out_resized);
        return ERR_IMGLIB;
    }
    if (vips_thumbnail_image(image_in, &image_out_resized, header->resized_res[2 * resolution],
                             "height", header->resized_res[2 * resolution + 1], NULL) == -1) {
        free_all(buffer_in, buffer_out, image_in, image_out_resized);
        return ERR_IMGLIB;
    }

    if (vips_jpegsave_buffer(image_out_resized, &buffer_out, &buffer_out_len, NULL) == -1) {
        free_all(buffer_in, buffer_out, image_in, image_out_resized);
        return ERR_IMGLIB;
    }
    if (fseek(imgfs_file->file, 0, SEEK_END)) {
        free_all(buffer_in, buffer_out, image_in, image_out_resized);
        return ERR_IO;
    }
    uint64_t offset = (uint64_t)imgfs_file->file->_offset;

    if (fwrite(buffer_out, buffer_out_len, 1, imgfs_file->file) != 1) {
        free_all(buffer_in, buffer_out, image_in, image_out_resized);
        return ERR_IO;
    }
    free_all(buffer_in, buffer_out, image_in, image_out_resized);
    metadata->offset[resolution] = offset;
    metadata->size[resolution] = (uint32_t) buffer_out_len;
    if (fseek(imgfs_file->file, (signed) (sizeof(struct imgfs_header) + index * sizeof(struct img_metadata)), SEEK_SET) ||
        fwrite(metadata, sizeof(struct img_metadata), 1, imgfs_file->file) != 1) {
        return ERR_IO;
    }
    return ERR_NONE;

}


int get_resolution(uint32_t *height, uint32_t *width,
                   const char *image_buffer, size_t image_size)
{
    M_REQUIRE_NON_NULL(height);
    M_REQUIRE_NON_NULL(width);
    M_REQUIRE_NON_NULL(image_buffer);

    VipsImage* original = NULL;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
    const int err = vips_jpegload_buffer((void*) image_buffer, image_size,
                                         &original, NULL);
#pragma GCC diagnostic pop
    if (err != ERR_NONE) return ERR_IMGLIB;

    *height = (uint32_t) vips_image_get_height(original);
    *width  = (uint32_t) vips_image_get_width (original);

    g_object_unref(VIPS_OBJECT(original));
    return ERR_NONE;
}
