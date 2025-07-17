#pragma once

#ifdef __cplusplus
extern "C"{
#endif

int clone_tree_dir(const char *destination_path, const char *source_path, json_t *toreplace);

#ifdef __cplusplus
}
#endif
