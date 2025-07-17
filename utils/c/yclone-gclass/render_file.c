/****************************************************************************
 *          MAKE_SKELETON.C
 *          Make a skeleton
 *
 *          Copyright (c) 2015 Niyamaka.
 *          All Rights Reserved.
 *
 *
 *  sudo apt-get install libpcre3-dev
 *
 ****************************************************************************/
#define _POSIX_SOURCE
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>
#include <pcre.h>
#include <errno.h>
#include <jansson.h>
#include <00_replace_string.h>
#include "render_file.h"

/***************************************************************************
 *      Constants
 ***************************************************************************/

/***************************************************************************
 *  Busca en str las {{clave}} y sustituye la clave con el valor
 *  de dicha clave en el dict jn_values
 ***************************************************************************/
int render_string(char *rendered_str, int rendered_str_size, char *str, json_t *jn_values)
{
    snprintf(rendered_str, rendered_str_size, "%s", str);

    const char *key; json_t *v;
    json_object_foreach(jn_values, key, v) {
        const char *value = json_string_value(v);
        if(!value)
            value = "";
        char * new_value = replace_string(rendered_str, key, value);
        snprintf(rendered_str, rendered_str_size, "%s", new_value);
        free(new_value);
    }

    return 0;
}

/***************************************************************************
 *  Lee el fichero src_path línea a línea, render la línea,
 *  y sálvala en dst_path
 ***************************************************************************/
int render_file(char *dst_path, char *src_path, json_t *jn_values)
{
    FILE *f = fopen(src_path, "r");
    if(!f) {
        fprintf(stderr, "ERROR Cannot open file %s\n", src_path);
        exit(-1);
    }

    if(access(dst_path, 0)==0) {
        fprintf(stderr, "ERROR File %s ALREADY EXISTS\n", dst_path);
        exit(-1);
    }

    FILE *fout = fopen(dst_path, "w");
    if(!fout) {
        printf("ERROR: cannot create '%s', %s\n", dst_path, strerror(errno));
        exit(-1);
    }
    printf("Creating filename: %s\n", dst_path);
    char line[8*1024];
    char rendered[8*1024];
    while(fgets(line, sizeof(line), f)) {
        render_string(rendered, sizeof(rendered), line, jn_values);
        fputs(rendered, fout);
    }
    fclose(f);
    fclose(fout);
    return 0;
}

