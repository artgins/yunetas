/****************************************************************************
 *              gbmem.c
 *
 *              Block Memory Core
 *
 *
 * TODO find out some good memory mananger
 *
 *  Originally from yuneta V6:
 *  Code inspired in zmalloc.c (MAWK)
 *  copyright 1991,1993, Michael D. Brennan
 *  Mawk is distributed without warranty under the terms of
 *  the GNU General Public License, version 2, 1991.
 *
 *   ZBLOCKS of sizes 1, 2, 3, ... 128
 *   are stored on the linked linear lists in
 *   pool[0], pool[1], pool[2],..., pool[127]
 *
 *   Para minimum size of 128 y pool size de 32 (pool[32]) ->
 *        sizes 128, 256, 384, 512, ... 2048,...,4096
 *   Para minimum size of 16 y pool size de 256 (pool[256]) ->
 *        sizes 16, 32, 48, 64,... 2048,...,4096
 *
 *
 *              Copyright (c) 1996-2023 Niyamaka.
 *              Copyright (c) 2024, ArtGins.
 *              All Rights Reserved.
 ****************************************************************************/
