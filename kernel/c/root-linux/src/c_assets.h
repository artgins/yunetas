/****************************************************************************
 *          C_ASSETS.H
 *          Assets GClass.
 *
 *          Binary assets of treedb nodes: the bytes on disk, the metadata
 *          in the treedb.
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
 *  C_ASSETS keeps the bytes in a directory it owns, next to the treedb of
 *  the same realm, and one node per asset in the treedb. The consumer's
 *  own columns (`foto`, `qr`, ...) become fkeys to that topic, so an asset
 *  is linked, listed, graphed and cascade-deleted like any other node.
 *
 *  The asset id is the sha256 of the content, so the same bytes stored
 *  twice are one asset, a reload creates nothing, and a served url can be
 *  cached for ever. Replacing an asset is a new id plus a relinked node:
 *  the history of the node then says which photo it carried, and when.
 *
 *  Two ways out to a browser, and the SERVICE decides which:
 *
 *      - a signed url that a web server checks by itself (fast, cached),
 *        when `public_url` and `sign_secret` are both configured;
 *      - the bytes inline in the answer, when they are not.
 *
 *  So a node with no web server in front of it still shows its images, and
 *  the caller has one code path either way. See `get-asset`.
 */

/*
 *  WHAT IT ACCEPTS, AND WHAT THAT COSTS.
 *
 *  Images, pdf, video and audio -- `allowed_content_types` says which, and
 *  the default carries all four families. Two of them are deliberate
 *  omissions: `image/svg+xml`, because an svg served from the app's own
 *  origin runs script, and anything the host has no business serving.
 *
 *  An asset is hashed and written WHOLE: there is no streaming path, so
 *  `max_size` (128M by default) is a MEMORY limit as much as a policy one.
 *  A `put-asset` costs the worst: the base64 arrives inside the kw and is
 *  then decoded, so one call peaks at roughly 2.3x the file. `import-assets`
 *  only pays the file itself. Raise `max_size` for big media only after
 *  checking the yuno's own `MEM_MAX_BLOCK` -- a single base64 string above
 *  it is refused by the allocator, not by this gclass.
 *
 *  THE TOPIC IS THE HOST'S, NOT THIS GCLASS'S.
 *
 *  C_ASSETS never creates it: the fkeys of an asset point at the host's
 *  own topics, so only the host can write those hooks. What C_ASSETS does
 *  is REFUSE TO WORK when the topic it was pointed at cannot hold what it
 *  is about to write -- a blob on disk whose row failed to be written is
 *  a file nothing can ever find again.
 *
 *  Declare it in the host's `treedb_schema`, verbatim except for the
 *  hooks, which name the host's own topics and columns:
 *
 *      {
 *          'id': 'assets',
 *          'pkey': 'id',
 *          'system_flag': 'sf_string_key',
 *          'topic_version': '1',
 *          'cols': {
 *              'id':            {'header':'Id','fillspace':32,'type':'string',
 *                                'flag':['persistent','required']},
 *              'content_type':  {'header':'Type','fillspace':12,'type':'string',
 *                                'flag':['persistent']},
 *              'size':          {'header':'Size','fillspace':8,'type':'integer',
 *                                'flag':['persistent']},
 *              't':             {'header':'Time','fillspace':20,'type':'integer',
 *                                'flag':['persistent','time','now']},
 *              'original_name': {'header':'Name','fillspace':20,'type':'string',
 *                                'flag':['persistent']},
 *              'source_path':   {'header':'Source','fillspace':30,'type':'string',
 *                                'flag':['persistent']},
 *              'uploaded_by':   {'header':'By','fillspace':20,'type':'string',
 *                                'flag':['persistent']},
 *
 *              # One hook per column of the host that links an asset. A hook
 *              # maps CHILD TOPIC -> the fkey column on it, so one hook cannot
 *              # serve two columns of the same topic: `foto` and `qr` need one
 *              # each.
 *              'as_foto':  {'header':'Photo of','type':'dict','flag':['hook'],
 *                           'hook':{'devices':'foto'}},
 *              'as_qr':    {'header':'Qr of','type':'dict','flag':['hook'],
 *                           'hook':{'devices':'qr'}}
 *          }
 *      }
 *
 *  and on the host's side the column stops being a path and becomes a
 *  link:  'foto': {'type':'string','flag':['fkey']}.
 *
 *  Serving the bytes through a web server (`public_url` + `sign_secret`)
 *  needs this, and nothing else, in front of the blob directory:
 *
 *      location /assets/ {
 *          secure_link      $arg_s,$arg_e;
 *          secure_link_md5  "$secure_link_expires$uri <sign_secret>";
 *          if ($secure_link = "")  { return 403; }
 *          if ($secure_link = "0") { return 410; }
 *          alias <store_path>/;
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
