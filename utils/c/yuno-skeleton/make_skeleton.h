#pragma once

#ifdef __cplusplus
extern "C"{
#endif

/*
 *  base - directorio raiz de los skeletons
 *  file - fichero json de configuración de skeletons
 *  skeleton - skeleton a usar para renderizar la salida
 *
 *  El resto de datos se sacan preguntando al usuario.
 *  El yunorole, yunoname, y rootname tienen varias variantes (YUNOROLE, Yunorole, etc)
 *  que se implementan internamente.
 *  Eso implica que hay campos obligatorios en las plantillas json (yunorole, yunoname, rootname),
 *  el resto puede ser modificado por el usario.
 *
 */
int make_skeleton(const char *base, const char *file, const char *skeleton);
int list_skeletons(const char *base, const char *file);


#ifdef __cplusplus
}
#endif
