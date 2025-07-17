/***************************************************************************
 *  Función copiada de http://creativeandcritical.net/str-replace-c
 *  Description:
 *      Replaces in the string str all the occurrences of the source string old
 *      with the destination string new.
 *      The lengths of the strings old and new may differ.
 *      The string new may be of any length, but the string old must be of non-zero length -
 *      the penalty for providing an empty string for the old parameter is an infinite loop.
 *      In addition, none of the three parameters may be NULL.
 *
 *  Returns:
 *      The post-replacement string, or NULL if memory for the new string could not be allocated.
 *      Does not modify the original string.
 *      The memory for the returned post-replacement string
 *      may be deallocated with the standard library function free when it is no longer required.
 *
 *  Dependencies:
 *  For this function to compile, you will need to also
 *      #include the following files: <string.h>, <stdlib.h> and <stddef.h>.
 *  Portability:
 *      Portable to C compilers implementing the ISO C Standard,
 *      any version i.e. from C89/C90 onwards.
 *  Author:
 *      Me (Laird Shaw).
 *  Licence:
 *      Public domain.
 *  Attribution:    Optional. If you choose to indicate attribution when using this function,
 *      feel free to link to this page.
 ***************************************************************************/
#pragma once

#ifdef __cplusplus
extern "C"{
#endif

// WARNING use **free()** to deallocated the returned string.
char *replace_string(const char *str, const char *old, const char *snew);

#ifdef __cplusplus
}
#endif
