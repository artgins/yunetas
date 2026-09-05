/****************************************************************************
 *          C_ASSETS.H
 *          Assets GClass.
 *
 *          The way OUT of the bytes a treedb keeps for its 'file' columns.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <gobj.h>

#ifdef __cplusplus
extern "C"{
#endif

/*
 *  A treedb node often owns something that is NOT json: a photo, a plan, a
 *  signed pdf. Those bytes cannot live inside the treedb -- it is held in
 *  memory and timeranger2 rewrites the whole record on every update, so a
 *  40 KB photo would be rewritten every time a node changes its state.
 *
 *  STORING them is treedb's own business (kernel/c/timeranger2/tr_treedb.h,
 *  DESIGN-treedb-files.md): a column flagged ['fkey','file'] holds an fkey
 *  into the system topic `__assets__`, the index of the bytes, and the bytes
 *  live under `<treedb dir>/.blobs/ab/cd/<sha256>.<ext>`. The record's
 *  write path takes the bytes beside the record (`__files__`), hashes them,
 *  checks size and type ON THE BYTES, and links. `import-assets` and
 *  `gc-assets` are commands of C_NODE.
 *
 *  C_ASSETS is the other half: PUBLISHING the bytes to a browser, which is
 *  not storing them. Two ways out, and the SERVICE decides which:
 *
 *      - a signed url that a web server checks by itself (fast, cached),
 *        when `public_url` and `sign_secret` are both configured;
 *      - the bytes inline in the answer, when they are not.
 *
 *  So a node with no web server in front of it still shows its images, and
 *  the caller has one code path either way. See `get-asset`: it takes the
 *  asset ID and nothing else -- the id is the sha256 of the content, so a
 *  served url means the same bytes for ever and can be cached for ever.
 *
 *  Serving the bytes through a web server (`public_url` + `sign_secret`)
 *  needs this, and nothing else, in front of the treedb's `.blobs/`:
 *
 *  NOT `/assets/`: a Vite SPA on the same vhost already owns that
 *  prefix for its content-hashed bundles, so the two locations would
 *  fight over it. Pick a prefix the app does not use.
 *
 *      location /media/ {
 *          secure_link      $arg_s,$arg_e;
 *          secure_link_md5  "$secure_link_expires$uri <sign_secret>";
 *          if ($secure_link = "")  { return 403; }
 *          if ($secure_link = "0") { return 410; }
 *          alias <store>/<realm>/treedb_<name>/.blobs/;
 *          expires max;    # the name IS the hash: it can never go stale
 *          access_log off;
 *      }
 */

/***************************************************************
 *              FSM
 ***************************************************************/
/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DECLARE_GCLASS(C_ASSETS);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_assets(void);

#ifdef __cplusplus
}
#endif
