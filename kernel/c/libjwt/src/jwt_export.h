/* Copyright (C) 2024-2025 maClara, LLC <info@maclara-llc.com>
   This file is part of the JWT C Library

   SPDX-License-Identifier:  MPL-2.0
   This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef JWT_EXPORT_H
#define JWT_EXPORT_H

/* Version macros for LibJWT */
#define JWT_VERSION_MAJOR	3
#define JWT_VERSION_MINOR	2
#define JWT_VERSION_MICRO	1

#define JWT_VERSION_STRING	"3.2.1"

/* Whether the system supports "long long". This is primarily to sync
 * with jansson's json_int_t when working with JSON integers. */
#define JWT_USES_LONG_LONG	1

#ifdef JWT_STATIC_DEFINE
#  define JWT_EXPORT
#  define JWT_NO_EXPORT
#else
#  ifndef JWT_EXPORT
#    ifdef jwt_EXPORTS
        /* We are building this library */
#      define JWT_EXPORT __attribute__((visibility("default")))
#    else
        /* We are using this library */
#      define JWT_EXPORT __attribute__((visibility("default")))
#    endif
#  endif

#  ifndef JWT_NO_EXPORT
#    define JWT_NO_EXPORT __attribute__((visibility("hidden")))
#  endif
#endif

#ifndef JWT_CONSTRUCTOR
#  define JWT_CONSTRUCTOR __attribute__ ((__constructor__))
#endif

#ifndef JWT_DEPRECATED
#  define JWT_DEPRECATED __attribute__ ((__deprecated__))
#endif

#ifndef JWT_DEPRECATED_EXPORT
#  define JWT_DEPRECATED_EXPORT JWT_EXPORT JWT_DEPRECATED
#endif

#ifndef JWT_DEPRECATED_NO_EXPORT
#  define JWT_DEPRECATED_NO_EXPORT JWT_NO_EXPORT JWT_DEPRECATED
#endif

/* NOLINTNEXTLINE(readability-avoid-unconditional-preprocessor-if) */
#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef JWT_NO_DEPRECATED
#    define JWT_NO_DEPRECATED
#  endif
#endif

#endif /* JWT_EXPORT_H */
