# **Changelog**

## v7.2.1 -- 07/Apr/2026
    - Documentation: add Test Suite page, fix glossary warnings, improve gobj-js and lib-yui READMEs
    - Remove obsolete defconfig and REVIEW.md
    - Fix duplicate measure_times declarations in yev_loop.h

## v7.2.0 -- 04/Apr/2026
    - Fully static glibc binaries (CONFIG_FULLY_STATIC): GCC and Clang, with custom
      static resolver (yuneta_getaddrinfo) and NSS replacements (static_getpwuid, etc.)
    - mbedTLS support as alternative TLS backend (~3x smaller static binaries vs OpenSSL)
    - Fix mbedTLS bad_record_mac: accumulate TLS records before writing
    - Add TRACE_TLS trace level and mbedTLS debug callback for TLS diagnostics
    - JS kernel restructured: gobj-js (7.1.x) and lib-yui (7.1.x) published to npm
    - Replace bootstrap-table+jQuery with Tabulator in gui_treedb
    - Vite 8 build for lib-yui (ES/CJS/UMD/IIFE bundles)
    - MQTT 5.0: will properties, user properties, topic alias, subscription identifiers
    - Fix MQTT QoS 2 infinite loop and flow control (receive-maximum, keepalive)
    - OAuth2 BFF (auth_bff yuno) with PKCE, httpOnly cookies, security hardening
    - TreeDB: compound link improvements, undo/redo history sync, new tr2search/treedb_list utils
    - G6 graph visualization: C_G6_NODES_TREE and C_YUI_JSON_GRAPH GClasses
    - Fix c_watchfs: memory leak, event name mismatch (EV_FS_CHANGED), buffer bugs
    - Fix c_fs: memory leak in destroy_subdir_watch
    - Fix XSS vulnerabilities in gui_treedb webapp
    - Kconfig: add CONFIG_C_PROT_MQTT, organize protocol modules submenu
    - Remove deprecated musl compiler option

## v7.0.1 -- 29/Mar/2026
    - Release 7.0.1
    - JS kernel (yunetas npm package) published as v0.3.0
    - Updated and documented .deb packaging (packages/)

## v7.0.0 -- 28/Sep/2025
    - Publish first 7.0.0 for production

## v7.0.0-b17 -- 26/Sep/2025
    - fix remote console (controlcenter) blocked when paste text

## v7.0.0-b15 -- 22/Sep/2025
    - fix yuneta_agent: wrong assignment of ips to public service

## v7.0.0-b14 -- 11/Sep/2025
    - improve .deb
    - yuno-skeleton to /yuneta/bin and skeletons to /yuneta/bin/skeletons
    - check inherited files only for daemons

## v7.0.0-b12 -- 7/Sep/2025
    - now you can select openresty or nginx in .deb

## v7.0.0-b10 -- 2/Sep/2025
    - jwt in remote connection

## v7.0.0-b9 -- 2/Sep/2025
    - Remote control (controlcenter) ok

## v7.0.0-b8 -- 29/Aug/2025
    - GObj: fix bug with rename events

## v7.0.0-b7 -- 29/Aug/2025
    - Fixed: avoid that yunos (fork child) inherit the socket/file descriptors from agent.
