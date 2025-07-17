#pragma once

#ifdef __cplusplus
extern "C"{
#endif

int copy_dir(const char *dst_dir, const char *src_dir, json_t *jn_values);

#ifdef __cplusplus
}
#endif
