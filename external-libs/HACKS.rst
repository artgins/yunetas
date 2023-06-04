Hacks of External libraries
===========================

Para encontrar el código modificado, comparar los tar.gz originales con los de gines.tar.gz

Hackeo de libuv:
================

No me aceptaron al parche de añadir uv_accept(), así que me toca hackear libuv.

Despues de uv_accept():

libuv-?.?.?/include/uv.h
------------------------

UV_EXTERN int uv_not_accept(uv_stream_t* server);

libuv-?.?.?/src/unix/stream.c
-----------------------------

int uv_not_accept(uv_stream_t* server) {
  if (server->accepted_fd == -1)
    return -EAGAIN;
  uv__close(server->accepted_fd);
  server->accepted_fd = -1;
  return 0;
}

Hackeo de pcre2
===============
Hay que añadir a pcre2posix.h

    #pragma once    /* GMS */


