#include "error.h"
#include "imgfs.h"     //calls to do_lists, print_header, etc,..
#include "util.h"     //call to TO_BE_IMPLEMENTED()

#include <json-c/json.h>
#include <json-c/json_object.h>
#include <string.h>

int do_list(const struct imgfs_file *imgfs_file, enum do_list_mode output_mode, char **json)
{
    M_REQUIRE_NON_NULL(imgfs_file);
    switch(output_mode) {
    case STDOUT:
        print_header(&imgfs_file->header);
        if (imgfs_file->header.nb_files == 0) {
            puts("<< empty imgFS >>");
        } else {
            const struct img_metadata* start      = imgfs_file->metadata;
            const struct img_metadata* const  end = start + imgfs_file->header.max_files;
            for (; start < end; ++start) {
                if (start->is_valid == NON_EMPTY) {
                    print_metadata(start);
                }
            }
        }
        return ERR_NONE;
    case JSON: {
        M_REQUIRE_NON_NULL(json);

        struct json_object* json_array = json_object_new_array();
        if (json_array == NULL) {
            return ERR_RUNTIME;
        }
        const struct img_metadata* start      = imgfs_file->metadata;
        const struct img_metadata* const  end = start + imgfs_file->header.max_files;
        struct json_object* json_temp = NULL;
        for (; start < end; ++start) {
            if (start->is_valid == NON_EMPTY) {
                json_temp = json_object_new_string(start->img_id);
                if (json_temp == NULL || json_object_array_add(json_array, json_temp) == -1) {
                    json_object_put(json_temp);
                    json_object_put(json_array);
                    return ERR_RUNTIME;
                }
            }
        }
        struct json_object* json_obj = json_object_new_object();
        if (json_obj == NULL) {
            json_object_put(json_array);
            return ERR_RUNTIME;
        }
        if (json_object_object_add(json_obj, "Images", json_array) == -1) {
            json_object_put(json_array);
            json_object_put(json_obj);
            return ERR_RUNTIME;
        }
        *json = strdup(json_object_to_json_string(json_obj));

        json_object_put(json_obj);
        if (*json == NULL) {
            return ERR_OUT_OF_MEMORY;
        }

        return ERR_NONE;
    }
    case NB_DO_LIST_MODES:
        //do nothing
        return ERR_NONE;
    }
}
