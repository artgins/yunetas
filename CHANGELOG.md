# **Changelog**

## Unreleased

Mostly the JS layer (`yunos-js` 0.15.1 -> 0.22.16, `gobj-ui` 7.23.26) in four
parts, plus one thing the C side was missing: a tranger topic could be listed
key by key and never PRUNED.

Two things learn to act on a SET: the connections table of `gui_treedb`,
because a pasted deploy centre is two hundred rows, and the treedb GRAPH, which
could be rearranged one node at a time and no other way. Its camera toolbar
also stops promising what it does not do.

And the URL learns to hold a POSITION. Three separate places threw one away: a
reload on a deep tab route answered with another tab's default, an action route
came back to the route a view is declared at rather than the one you were
looking at, and both consoles were deciding this in their own `c_app.js` --
which is how one of them could be wrong while the other was right, and nothing
said so. The deciding is one shared piece now.

And the JSON viewer learns to READ the same document three ways. A tree is the
right shape for finding one value in a large document, and the wrong one for
reading it as it is written or for seeing its shape — so `C_YUI_JSON` now also
shows the raw text of what it holds, and a graph of it.

And last, the three G6 graphs learn to be OPERATED by a finger. They have
always drawn correctly on a telephone; what could not be done on one was
anything else — the zoom was two buttons because G6 gives it to the wheel, the
context menu had no door at all, the controls a finger has to land on were
sized for a pixel, and the treedb graph's multi-selection hung off a key a
telephone does not have. And the plainest thing of all could not be done
either, which is why it was found last: a finger could not MOVE a node.

Detail in those repos' CHANGELOGs.

### Added

- **The cards of the node viewers can be MOVED** (`gobj-ui` 7.23.24), in the
    JSON graph and the gobj tree alike. The position is deliberately not kept:
    both are rebuilt from their source on every refresh, fold and layout
    change, and neither is a document of its own to save it to -- pulling two
    cards apart to read the lines between them is worth having even for one
    session. The finger's half of it needed the mounts to refuse the browser's
    gestures: G6 puts `touch-action: none` on its CANVAS and nothing on the
    html nodes over it, so a drag that began on a card was a page scroll that
    died after ~20px. Same defect the treedb graph paid for at 7.23.14.

- **A camera ANCHOR in every node viewer** (`gobj-ui` 7.23.19, made to work in
    7.23.25 -- it had no visible state and never centred anything: the class it
    used was styled only for the G6 plugin toolbar, `graph.focusElement()` does
    not move these graphs, and a camera move issued inside G6's click dispatch
    is swallowed): pick one
    element from the toolbar's crosshairs and every zoom leaves it in the
    MIDDLE. `C_YUI_JSON_GRAPH`, `C_YUI_GOBJ_TREE_JS` and `C_G6_NODES_TREE`,
    from the same button. A graph that FITS on screen is unreadable at the
    zoom that makes it fit -- one topic's schema fits at 37%, where every card
    is grey texture -- so the useful view is always a fraction of the document,
    and which fraction was nobody's decision: `1:1` translated to the layout's
    origin, a corner with nothing in it. Three states (`off`, `arming`, `on`)
    because two could not say what a press does, and an armed anchor takes the
    next node click before selection, ports or popover. A **zoom** re-centres
    and a **pan** does not -- `aftertransform` fires for both, so the zoom
    LEVEL is what tells them apart. The target is remembered by identity (the
    `path`, the `full_name`), never by node id, because ids are generated per
    build. The viewers also open at ACTUAL SIZE now, centred on the anchor or
    the root, and a JSON card stops listing the containers it already draws as
    cards -- what made an array of N dicts an N-row card beside N cards.

- **The JSON viewer remembers which of its three views you read in**
    (`gobj-ui` 7.23.17). It opened on the tree every time, however many times
    you switched to the graph. The choice is kept in `localStorage` under one
    key for the whole library, because which view someone reads JSON in is a
    habit of the PERSON and not a property of the document. Precedence is host,
    then memory, then the tree -- which is why the `view_mode` attr no longer
    declares `"tree"` as its default: as a default and as a host's explicit
    choice it was the same string, so nothing could tell *"show me the tree"*
    from *"I have no opinion"*, and a memory that cannot see the difference has
    to lose to both. Only a view the READER picks is remembered.

- **A tranger key can be deleted** — `C_TRANGER` gains `delete-key`
    (`topic_name`, `key`, `force`). `tranger2_delete_key()` has been in the
    timeranger2 API all along (master-only, and it propagates the delete to the
    in-process subscribers and to the `rt_by_disk` followers), but the only way
    to reach it was `delete-node` on a topic that belongs to a TREEDB. A plain
    tranger topic had no path at all: `list-keys` could show a key born of a
    port scan or a typo, and nothing could remove it — it stayed in the topic,
    and in every view derived from the topic's keys, for ever.

    The command refuses a key that is not there (`tranger2_delete_key()`
    answers 0 for one that never existed, so a bare wrapper would report a
    delete that deleted nothing), and refuses a key that still holds records
    unless `force=1` — naming the record count in the refusal, which is what
    tells the operator what forcing would cost. Same `delete` authz as
    `delete-topic`.

    `gui_treedb`'s Keys picker grows the matching button: a third action on
    each key row, next to Rows and Live. It asks first, with the topic, the key
    and the record count in the question, and closes that key's open cards
    before the delete — a Rows card holds a server iterator on the key and a
    Live card a realtime feed, and both would otherwise be left pointing at
    something that no longer exists.

- **The JSON viewer shows the same document three ways** (`gobj-ui` 7.20.0,
    7.21.0, 7.22.0). The third is a GRAPH — it hosts the `C_YUI_JSON_GRAPH` child that
    already drew JSON as a hierarchy, so what is new is not the drawing but
    that you no longer leave the viewer to get it. The switch became one button
    per view: three views do not fit a toggle, because a cycling button cannot
    be aimed. Two layout facts came out of it, both only a browser could tell
    you — a canvas pushes no height, so the graph body needs a DEFINITE height
    and not a minimum (a percentage height does not resolve against a box sized
    by a minimum: G6 came up 1061x2); and at 390px the toolbar's search box was
    taking 294 of 320 visible pixels, which put every button off the edge.

    The graph then got the two facilities the tree already had (7.22.0): a find
    box that highlights matching rows, outlines their cards and says how many
    matched without moving the camera, and expand/collapse that folds every card
    but the root and marks each cut with a count. Its highlight is baked into
    the card's markup and not set as a G6 node state — the key shape of an
    `html` node is a DOM element, and G6 paints no state style on it. Each
    non-leaf card then got its own fold handle (7.23.0), because a graph you can
    only open whole or close whole is not navigable — drawn as the same filled
    chip the gobj tree uses (7.23.1), after the bare glyph it shipped with
    turned out to be two pixels of ink at the zoom that fits a document on a
    phone. The graph also picks its layout now (7.23.2) — vertical tree, dagre
    top-down, dagre left-right — and the treedb topic's JSON popups became
    movable, maximisable WINDOWS on a laptop, a document being something you
    read while looking at the table it came from. And its camera stopped using
    a different picture from the treedb graph's for the same two buttons
    (7.23.3) — one action, one drawing, zoom readout included. Every graph's
    camera is built in one module now (7.23.4), because unifying two files and
    leaving the third is how they drifted in the first place — and the offline
    demo finally shows the treedb graph too (7.23.5), which is the one the
    other two borrow that camera FROM and the one nobody could see there. Same
    drawings in the same PLACES, last (7.23.6, 7.23.7): the GLOBAL fold leads
    the toolbar ahead of the find box, and the PER-NODE one sits on the right of
    each card's own header — two controls, two jobs, two places. And both graph
    find boxes gained the clear (✕) the tree viewer's always had (7.23.8),
    reported on a phone, where there is no keyboard shortcut to fall back on.

- **The JSON viewer shows the same document as raw text** (`gobj-ui` 7.20.0).
    `C_YUI_JSON` had one way to read a document — the lazy tree — and a tree is
    the wrong shape for some of what people do with JSON: read a command answer
    as it is written, take a slab of it into a ticket, find a string with the
    browser's own Ctrl+F. A toolbar switch turns it into a
    `JSON.stringify(…, 4)` dump of the working document; the `view_mode` attr
    (`"tree"` | `"text"`) lets a host open straight into it, and
    `EV_SET_VIEW_MODE {mode}` moves it at runtime, toggling when no mode is
    given.

    Nothing there is lazy: it prints what the client currently holds,
    `__collapsed__` sentinels included, because that is honestly what it has.
    Over 2M characters the dump is cut and the cut is announced. Search and
    expand/collapse hide with the tree — they act on tree rows and have nothing
    to act on here — while copy stays. Long lines scroll sideways inside the
    viewer instead of wrapping: in a raw dump the indentation IS the structure,
    and a wrapped line restarts at column 0 and lies about the depth of
    everything under it.

- **Connections acts on many, and each gesture is one write** (`yunos-js`
    0.16.0, 0.17.0). The browse column gets its header checkbox — three
    states, covering what the filter leaves on screen, counted over services
    so it cannot read "all" while half a connection is unticked. And
    connecting several gets its own dialog, opened on the connect INTENT of
    every connection: ticking everything is one click, disconnecting a
    handful is the same box, and its count says what Apply will CHANGE rather
    than what is ticked. Neither borrows the row checkbox, which means
    *browse* and cannot mean two things. Both apply in a single write
    (`EV_SET_CONNS_BROWSE`, `EV_SET_CONNS_ENABLED`), so the app root
    reconciles the transports once instead of once per connection.

- **Several graph nodes can be selected, and they move together**
    (`gobj-ui` 7.16.0). In edition mode, **shift+click** adds a node to the
    selection or takes it out, **shift+drag on the canvas** is a rubber band,
    and dragging any selected node moves the **whole set** — as one undo,
    because G6 batches it. The group move costs nothing because of where the
    selection is kept: G6's `selected` element state IS the selection, which is
    what `drag-element` asks the graph for. The ring had to be painted into the
    card's own html, an html node drawing no state style — the same trap that
    kept the amber highlight invisible until 7.3.0, so turning the rubber band
    on and nothing else would have selected correctly and shown nothing.

- **And the keys that selection needed** (`gobj-ui` 7.17.0). **Esc** clears it,
    **ctrl/cmd+A** takes every node, **Delete** deletes it — behind the same
    confirmation the per-node icon shows, from the same function, with the
    children about to be UNLINKED and the parents about to be detached **summed
    over the set**: these views delete with `force`, so twelve cards can detach
    eleven children. The keys reach the graph only while it has FOCUS, G6
    giving its canvas a `tabIndex`, which is what keeps ctrl+A typed in the
    find box a selection of the text. They arrive as `EV_KEY_DOWN` and the
    action decides, the two full-screen keys included — they used to call the
    plugin straight from the callback.

- **The two decisions a runtime-opened tab costs its url, in one place**
    (`gobj-ui` 7.19.0, `yunos-js` 0.18.0). Both consoles have a workspace whose
    tabs the operator opens, and both answered the same two questions in their
    own `c_app.js` — which is exactly why one could have the cold-load one
    WRONG while the other had it right, and nothing said so. The deciding is
    `yui_tab_routes.js` now (`yui_tab_split_subpath`, `yui_tab_position_plan`),
    tests and all; the wiring stays in the hosts, which are each right about
    when their own tabs become real.

- **Zoom to the selection** (`gobj-ui` 7.18.0). `fit` gives back the whole
    graph; there is now the same action for the part being worked on, as a
    button next to it — edition only, disabled while nothing is selected, and
    wearing the `fit` brackets with a marked object inside so the two read as
    one action at two scopes. `fitView()` has no subset form, so the bounds are
    measured off the elements and the zoom clamped to the graph's own
    `zoomRange`. New host key: `zoom to selection`.

- **The G6 graphs, operable on a touch screen** (`gobj-ui` 7.23.9). Measured in
    a real touch context rather than read off the CSS, and two of the five are
    facts about G6 that nothing in its documentation says:

    - **Pinch to zoom, in all three graphs.** `zoom-canvas` binds the WHEEL and
      nothing else, and a telephone has no wheel. G6 does ship a pinch
      recogniser, but asking for it (`trigger: ['pinch']`) REPLACES the wheel —
      its `bindEvents` is an `if/else` — and its `PinchHandler` keeps its
      instance and its callback list in STATICS, so on a page with two graphs
      the second registers against the first one's emitter: pinching graph A
      zooms both and pinching graph B does nothing. Recognised per graph
      instead, over a `zoom-canvas` that keeps the wheel — the same shape the
      `drag-canvas` replacement already had, so every graph gets it with no
      change to its behaviors list.

      It reads the NATIVE touch events, not G6's forwarded pointer stream:
      **`@antv/g` re-issues pointer ids in the middle of a two-finger gesture**
      (measured: a pinch that started on ids 2 and 3 finished on id 1), so
      anything keyed on `pointerId` loses a finger halfway and reads the
      gesture as a fraction of what it was. `event.touches` needs no
      bookkeeping — it IS the list of fingers down, restated on every event.

    - **A long press opens the context menu.** It never could, on any platform:
      **G6 does not read the DOM's `contextmenu` event at all.** Its
      `BehaviorController` synthesises the event from `pointerdown` with
      `button === 2`, so the menu was a right click and only a right click,
      whatever the browser does with a long press. The press re-emits G6's own
      forwarded event under the name the plugin listens for, so `getItems(e)`
      sees exactly what a right click gives it — the port under the finger
      included, which is what tells the port menu from the node one.

    - **Touch targets**, all behind `(pointer: coarse)` so a mouse sees no
      change: resize handles were 8x8 and eight of them (now a 14px mark in a
      44px box, corners only — eight fingertip-sized boxes around a 90px node
      overlap into one blob, and a corner resizes both axes anyway);
      `node properties` and `delete node` were two 28px circles **4px apart**,
      one fingertip covering both with the destructive one underneath; a port's
      hit area was a flat `+4` in WORLD units, a different target at every zoom
      and 5 screen px at the 50% a telephone lands on after fit.

    - **The floating toolbars fold.** Drawn inside the canvas, one on each
      edge, the two of them took a third of a 356px telephone canvas and stood
      on top of the nodes. Under 480px of container they collapse behind one
      button. Measured on the CONTAINER and not the window: the same graph is a
      full page in one app and a card in a column in another.

    New host keys: `show toolbar`, `hide toolbar`.

- **Multi-selection reaches a finger** (`gobj-ui` 7.23.11, 7.23.12). Both of its
    gestures hang off Shift — shift+click adds a card, shift+drag draws the
    band — and a phone has no Shift; nor is there a spare gesture to give
    them, since G6 binds panning and the band to the same plain drag. So
    Shift becomes a MODE: the graph's edit toolbar carries a **selection
    mode** toggle (a dashed marquee, next to `+`), and while it is on a tap
    picks a card and a drag on the background draws the band, with panning
    standing aside. A button rather than a heuristic, so the toolbar says
    which of the two the graph is listening for; and not device-specific,
    because it spares a desktop reader the key just as well. New consumer
    key: `selection mode`.

    The toggle looks **pressed** rather than taking a colour of the toolbar's
    palette (`7.23.12`, new `set_pressed_state()`): every colour there names a
    KIND of action — blue creates, orange means pending changes, violet is
    undo/redo, red destroys — so painting a STATE with one of them put the same
    violet on two neighbouring buttons for two different reasons, and carried
    the whole state change in the hairline of an outline glyph.

### Fixed

- **A JSON card was a single anchor for every line leaving it** (`gobj-ui`
    7.23.21, refined in 7.23.22). Fourteen edges came out of one point, and which row a line
    belonged to was a guess the reader made from where it landed. Every
    container key now opens a G6 **port** and its line leaves from there --
    on the bottom edge, and exactly ON it, because an html node draws its HTML
    in a DOM layer above the canvas and a port fully inside the box is painted
    under the card. A container with no card of its own hands the port its row
    opened in the parent down to all its children, so the fourteen columns of
    `cols` leave the single `cols` port. `getPointPosition()` moves to
    `lib_graph.js`, shared with the treedb graph it came from: it is the same
    decision about where a hook's port sits. Each port then moved onto the
    LINE of its own row (7.23.22): spread along the bottom edge they were
    distinguishable but still not attached to anything, since the reader had
    to count dots, count rows and trust the two orders matched. And a line
    gained an ARROWHEAD and a port to arrive at, centred above the target
    card's title (7.23.23): with ports only on the source side a line said
    where it left from and nothing about where it went.

- **A JSON graph drew a pure collection as a node of its own** (`gobj-ui`
    7.23.20). `cols` is one key of the topic dict like `pkey` is, and the card
    the graph gave it held no data, said nothing its parent's row did not
    already say, and pushed everything below it one level down. Every key is a
    row again, containers included -- a container row shows what it IS
    (`cols: [14]`) and nothing more, since what it HOLDS is the cards, and
    that is the part which has to scale. A container with no scalars of its
    own now gets no card at all and folds from the chip on its row.

- **Highlighting a JSON value made it HARDER to read than not highlighting
    it** (`gobj-ui` 7.23.18). On the graph view's amber match chip an orange
    boolean measured **1.80:1** and a red number 2.98:1, in both themes. No
    chip colour could have fixed it: `#FF8C00` is mid-luminance, so it reaches
    at most **2.33:1 against any background that exists**, white included --
    the ceiling is a fact of the colour, not of the surface. Orange moves to
    `#8C4D00` and blue to `#4359C6`, and every value type now clears 4.5:1 on
    all five surfaces the viewer paints (two light cards, the match chip, two
    dark cards); the worst in the matrix is 4.66:1. The graph also stops
    deriving its dark colours by mixing the light ones with white -- which is
    how one document was green in the tree and a paler green in the graph --
    and carries the tree's two palettes instead, so all three views agree.

- **The JSON viewer read better in LIGHT than in dark** (`gobj-ui` 7.23.17),
    which is normally the other way round, and the measurement said why: the
    dark graph card mixed 30% of the group's tint into the SURFACE, so green
    string values sat on a green card. The light card is near-white and lets
    saturated dark text carry the colour; the dark one carried it twice. The
    `list` card was worse -- a yellow tint at 30% lands mid-luminance, a muddy
    olive where nothing contrasts with anything, and its purple values measured
    **1.60:1** against 11.48 for the same values on the light card. The surface
    now stays out of the hue on both themes and the group's colour lives in the
    border and header bar, where the light theme always put it; worst case goes
    to **4.74:1**. The search highlight moved with it: its amber failed at both
    ends once the values brightened, and a chip dark enough for the text is
    invisible against the card, so a match is now a LIGHT chip on both themes
    carrying light-surface text -- the same flip the header bar already does.

- **A container row would not say WHICH record it was** (`gobj-ui` 7.23.17).
    An array of dicts carrying an `id` is a list of records -- a topic's nodes,
    for instance -- and the row said only `2: {15}`, so telling record 2 from
    record 9 meant opening all fifteen fields of both. It now carries the id
    beside the size, open as well as closed: it is the row's label, and a label
    that vanishes on expand makes the row jump and costs you the name of the
    thing you just opened.

- **A table whose `fillspace` was a string printed a stack trace per cell**
    (`ycommand`, `ybatch`, `mqtt_tui`, `msg2db_list`, `treedb_list`).
    `fillspace` is a column WIDTH, a presentation hint, and a command that
    declared it as `"30"` instead of `30` -- which `json_pack("s:s", ...)`
    does the moment the field is written next to the other three, all of
    them strings -- made `kw_get_int()` log *"path MUST BE a json integer"*
    with `LOG_OPT_TRACE_STACK`. Three reads run per column (header,
    separator, and once per data cell), so a four-column answer with two
    rows emitted twelve stack traces and buried the answer, and every column
    fell back to the default width of 10, which is what truncated the ids
    the operator was reading. `ycli` had been passing `KW_WILD_NUMBER` here
    for exactly this reason and its four siblings never got it; all fifteen
    read sites now do, so a width is accepted as written, string or int.

- **The toolbar lost its scroll arrows once it shared a row** (`gobj-ui`
    7.23.16). A `yui_toolbar()` alone in its row works, so the defect hid
    until the treedb graph was reached through the navigation, where the
    pinned `GRAPH_BACK_TOPICS` link sits beside it. A flex item's
    `min-width: auto` resolves to its CONTENT minimum, and every
    `yui-horizontal-toolbar-section` is `flex-shrink: 0; white-space:
    nowrap` — so the toolbar refused to shrink, kept `width: 100%` of the
    row, was pushed out of it, and its right arrow (absolutely positioned at
    its right edge) landed outside the ancestor that clips the view. The
    arrow was drawn all along, off-screen, while the items it would have
    scrolled to were plainly cut off.

- **A group move that moved the whole graph** (`gobj-ui` 7.23.10). Three
    defects behind one report: selecting two cards in a treedb graph and
    dragging them moved everything, erratically, and the saved result was
    right anyway. (1) Edition's `drag-canvas` is given an `enable` so it
    stands aside for Shift, the marquee's key — and an `enable` REPLACES
    G6's default, which is the `targetType === 'canvas'` test that keeps
    panning off a node drag: both behaviours ran on the same gesture, so a
    150px drag moved the card 255px and every other card 105px, and a pan
    writes nothing, which is why saving and refreshing showed the right
    thing. (2) The history plugin was installed at one MOMENT — the arrival
    of the last topic of the load — and only if the graph was in edition
    right then, so reaching edition through the mode selector, which is the
    ordinary way in, left dead Undo/Redo buttons and a `history_pause()`
    nothing answered, while Save still lit on its own. It follows the mode
    now. (3) G6's `brush-select` rewrites the `selected` state of every
    element on every canvas click, behind the gclass that owns the selection
    and outside its history pause — a recorded command whose before and
    after are identical, so a click on the background lit Save on a graph
    nobody had touched.

- **The treedb graph's two selects speak the app's language** (`gobj-ui`
    7.23.13). The layout one and the operation-mode one rendered their raw
    names — `reading`, `edition`, `dagre`, `manual` — in every language and in
    every app: not a missing key, a missing call, since neither went through
    `t()` at all. The Spanish console of a production node said *"Modo de
    operación: reading"*, label translated and value not. Two things came with
    it: the option's `value` is set EXPLICITLY (an `<option>` with none answers
    with its own TEXT, so a translated label would have sent `"Edición"` to the
    FSM as the mode to enter), and the labels are literals rather than
    `t(name)`, because a consumer's `validate-locales.mjs` reads literals and a
    variable key is invisible to it. New consumer keys: `reading`, `operation`,
    `writing`, `edition`, `manual`, `dagre`, `antv-dagre`, `d3-force`,
    `force-atlas2`.

- **The demo's treedb chapter opens its edition mode** (`gobj-ui`
    test-app). It was mounted read-only because there was nothing behind it
    to write to — still true of the RECORDS, where the refusal is the honest
    answer twice over (the graph draws a created or deleted node from the
    treedb's own node events, which a backend living in one page does not
    send, so a write that answered yes would leave the graph still). It is
    not true of the ARRANGEMENT: moving cards, the selection mode, undo/redo
    and Save all end in one write, `update-node` on `__graphs__`, and that
    topic is the view's own bookkeeping. `C_DEMO_BACKEND` declares it and
    takes that write, so the public demo now shows the whole of edition —
    including the selection mode added in `7.23.11`.

- **A mode that could not move the camera** (`gobj-ui` 7.23.9). The treedb
    graph's `operation` mode left its `behaviors` list empty where `reading`
    and `writing` next door both fill it. On a desktop the toolbar still
    zoomed and nothing panned; on a telephone, where the gestures ARE the
    camera, the graph was a picture.

- **A view that talks to a backend has to be a SERVICE, and for two reasons**
    (`gobj-ui` test-app). Only one of them is visible, and that is the trap:
    `gobj_save_persistent_attrs()` refuses a gobj that is not a service and
    says so, but `C_IEVENT_CLI` routes an answer back with
    `gobj_find_service(gobj_name(src))` and simply finds nobody. The offline
    demo mounted the treedb graph as a pure child and WORKED — its in-page
    backend answered the `src` pointer it had been handed, which is what a
    backend in the same page has and a real one never does. So the chapter
    modelled a contract that does not exist. Both halves fixed together: the
    graph goes through `yui_mount_service_view()`, the way both real consumers
    mount it, and the demo backend resolves its destination by NAME and
    refuses, loudly, to answer a caller that is not registered.

- **Three framework contracts, each found the same way: by a loud error nobody
    had seen because nobody had built that shape yet** (`gobj-ui` 7.20.1,
    7.23.0, 7.23.2). They are gobj rules, not component details, which is why
    they are here and not only in that repo's changelog:

    - **`gobj_destroy()` destroys the children BEFORE calling `mt_destroy()`.**
      A hosted child torn down in `mt_destroy` is torn down after the framework
      has already destroyed it — *while it was still running*. Retire a hosted
      child in **`mt_stop`**, where everything is whole.
    - **`C_YUI_WINDOW.close_window()` calls `on_close` and THEN stops and
      destroys itself.** A host that also destroys the window destroys it
      twice. On the ✕ path the host drops its reference and keeps its hands
      off; it destroys the window only when IT initiates the close.
    - **Adding an output event to a gclass hosted as a CHILD is a BREAKING
      change.** The child subscribes its host to everything it publishes, so a
      new output event is a new *mandatory* declaration in every host's FSM.
      Six gclasses answered *"Event NOT DEFINED in state"* to a graph click
      because one viewer started republishing its child's event "so the host
      has one contract" — which is the framework backwards.

- **F5 on a deep Schemas route landed on somebody else's default**
    (`yunos-js` 0.17.6). Reloading on
    `#/schemas/node/<node>%1F<yuno>/treedb_authzs/__graphs__` answered with
    `.../treedb_system_schema/edit`. A node tab's route is registered when the
    node is OPENED, so on a cold load it does not exist yet: the shell
    resolves as far as the workspace home and hands the whole rest over as the
    subpath. The restore read all of it as the node id, matched nothing, and
    fell back to the first tab — which stamped its own default treedb, and
    that treedb its own default view. Every segment past the id was thrown
    away, which is why a bare node tab survived a reload and nothing deeper
    did.

- **An action route came back to the MOUNT, not to where you were**
    (`gobj-ui` 7.19.4). With `redirect: "back"` or `"none"` the shell restores
    the previous resting route, and it read that off
    `stages.main.active_route` — the route the view is DECLARED at. Under a
    `C_YUI_NODE` tree everything below is subpath the node owns, so switching
    the theme from a graph five levels down landed on the workspace root,
    position gone; the same held for `/preferences`, `/sitemap` and the
    dev-tools routes. It restores `current_route` now, the same route WITH its
    subpath. An app whose views are all declared routes never saw this, which
    is why it took a node tree to surface it.

- **Shift+clicking a card smeared a text selection across the graph**
    (`gobj-ui` 7.19.2 — 7.19.3). Shift+click is the browser's own
    extend-the-text-selection gesture, so the moment `7.16.0` gave it a meaning
    here, marking three nodes also painted their labels blue. The canvas is a
    canvas — except the popovers, which carry record data an operator copies
    out, and `7.19.2` took the readable one (`g6-node-detail`) down with the
    rest before a measurement caught it.

- **`1:1` was a dead button, and the minimap drifted in full screen**
    (`gobj-ui` 7.19.1). G6's toolbar calls `onClick` only when the PRESSED
    element carries `g6-toolbar-item` — which is why its own CSS makes the icon
    `<svg>` click-through. The text glyph `7.15.0` added had no such rule, so a
    real press landed on the `<span>` and nothing happened; it survived a live
    check because that check used `element.click()`, which dispatches where it
    is told rather than where a pointer is. The minimap is placed in PIXELS
    computed once from the canvas size, so growing the container left it
    halfway up the left edge, over the graph it explains: it is anchored in CSS
    now, inset, and follows the theme instead of being a white box on a
    near-black canvas.

- **After clicking a node, the graph's keys did nothing** (`gobj-ui` 7.18.1 —
    7.18.3). The keyboard reaches the graph through G6's canvas, the element
    carrying a `tabIndex`, and a card is a DOM element inside the container —
    so clicking one sent the focus to `<body>` and Escape, ctrl+A and Delete
    stopped working immediately after the click that had just selected
    something. Three releases, each closed by MEASURING rather than reasoning
    (a probe reporting `document.activeElement` and every keydown): the first
    focused the wrong canvas of the four a graph stacks, the second ran before
    the browser's own mousedown focus handling. The focus is restored on
    `focusout` and only when it goes nowhere — moving to a real element is the
    user leaving.

- **The graph toolbar's house never took you home** (`gobj-ui` 7.15.0). The
    button people press to get the treedb graph back runs `zoomTo(1)`: it sets
    the SCALE and leaves the camera where it was, so from a corner of a large
    graph it answered with the same corner at 100%. A house means *the initial
    extent* in a map and *the starting view* in an editor, never a scale, and
    it sat directly under `fit`, which is the control that really does give
    the graph back. The action is right, so only its name moves: **`1:1`**,
    written and not drawn, the way every editor that offers actual size labels
    it. With it, the zoom level is shown as a readout, groups are separated by
    gaps rather than one more hairline, and **both floating toolbars follow
    the theme** — they were pinned light in both, two bright islands over a
    dark canvas. New host keys: `actual size`, `zoom level`.

- **A header checkbox that could not be clicked** (`yunos-js` 0.16.1). Two
    defects on one click, both found by driving the deployed app. It was born
    **disabled**: Tabulator draws the header before the rows exist, and the
    first load was the one data path that never repainted it afterwards. And
    the click **cancelled itself**: `preventDefault()` on a checkbox makes the
    browser revert the tick when the dispatch ends, while the repaint the
    event triggers runs in a microtask BEFORE that — state written, table
    repainted, count updated, and then the revert landed last.

- **A proposed url built on the realm id** (`yunos-js` 0.17.1). The agent
    console's *For TreeDB* copy read the FQDN from one place only, the
    `__ssl_certificate__` config variable, and fell through to the realm id
    when it was absent — proposing `demo.hidrauliaconnect.es` for a backend
    whose certificate says `hidrauliaconnect.es`. The evidence was in the
    same config, written where it is USED (`crypto.ssl_certificate` of the
    gate), and is now read there too: the gate wearing the top port wins, a
    wildcard certificate names no host, and the realm id is the last resort.

- **The `yunos-js` CHANGELOG was in Spanish** from 0.13.15 to 0.15.1.
    Translated in place: this is a public repo, and those are English.

- **A finger could not move a node** (`gobj-ui` 7.23.14, 7.23.15). The
    plainest thing there is to do in the treedb graph's edition mode, and two
    separate defects were in front of it — the first one not in this code at
    all.

    **The browser was taking the gesture.** G6 puts `touch-action: none` on
    its canvas and **nothing** on its HTML nodes, which are ordinary DIVs
    layered over it. So a drag that began on a CARD was a page scroll as far
    as the browser was concerned: it let two `pointermove`s through, decided,
    and killed the pointer stream with a `pointercancel`. The node followed
    the finger for about 20px and stopped dead while the page slid underneath
    — which reads as a delta bug and is not one. The pointer log is what says
    so: it ends at `pointercancel` / `lostpointercapture` while the touch
    stream runs on to the end of the gesture. `.graph-container` refuses those
    gestures whole now, with `touch-action: auto` kept for the panels and the
    context menu, which a finger must still be able to scroll.

    **And the press meant two things at once.** The long press fired on a
    TIMER, 500ms in, while `drag-element` was already carrying the node: the
    menu opened over a card that then ran away underneath it. A timer cannot
    arbitrate a gesture, because at the moment it fires the gesture is not
    over. Nothing decides now until the finger moves or lets go — moved →
    drag, still and let go quickly → the element's own action, still and held
    past 500ms → the context menu. The rule is `classify_press()` in the new
    pure `press_arbiter.js`, with tests; its 10px slop is G6's own
    `dragstartDistanceThreshold`, so "still" means the same to the arbiter and
    to the drag.

    Three more things the same press was doing, all fixed with it: the `click`
    that `@antv/g` SYNTHESISES in its `onPointerUp` (it does not take `click`
    from the DOM either, so swallowing the DOM one never helped) went on to
    click the node the menu had just opened on; the browser's own menu opened
    on top, while the finger was still down; and each release of a PINCH
    looked like the end of a press.

    Deciding at the release costs exactly one thing — while the finger is
    down, nothing says that letting go would now give the menu rather than the
    node's own action — so `7.23.15` adds a 15ms haptic tick at the 500ms
    mark. It is a NOTICE and not the decision: a finger that buzzes and then
    carries the node away still gets its drag.

## 7.16.2

JS layer only (`gobj-js` 7.13.2 -> 7.13.5, `gobj-ui` 7.10.5 -> 7.14.3,
`yunos-js` 0.13.3 -> 0.15.1). Each repo carries the detail in its own
CHANGELOG.

### Fixed

- **A subscription filter that never filtered** (`gobj-js` 7.13.3).
    `gobj_publish_event()` read the answer of `kw_match_simple()` — a JS
    **boolean** — with the C runtime's `=== 0` guard, so a subscription whose
    filter did not match was published to anyway. A treedb view subscribes to
    `EV_TREEDB_NODE_DELETED` once per topic with a `{treedb_name, topic_name}`
    filter, so deleting one row reached the table five times and the four
    strays logged *"record not found"*. Measured on the wire first — one frame
    out, one answer in — which is what said the copies were made in the
    browser.

    The audit of the same trap across the runtime (7.13.4, 7.13.5) fixed four
    more sites: `mt_publication_pre_filter`, `mt_play`,
    `mt_subscription_added`, and the `rc_walk_by_tree`/`rc_walk_by_list`
    return, now normalized through a `walk_ret()` helper. The other half of
    the trap is the caller: an action or framework method that answers
    `undefined` (a bare `return;`) reads as *stop* to a `< 0` guard and as
    *nothing* to a `=== 0` one. 31 off-contract returns were corrected across
    the five JS repos, with regression tests for the publish path.

- **The schema editor's buttons and icons are the size of buttons**
    (`gobj-ui` 7.13.4 - 7.13.6). `is-small` had been applied to every control
    of `SCHEMA_CARD` and `TREEDB_CARD`, form fields included, and the card
    grid was 12rem where a treedb name needs 14. In the same round, the
    header-filter hairline that 7.13.1 and 7.13.2 both failed to land: the
    cause was neither the token nor the specificity but the **CSS import
    order** — our Tabulator fixes were emitted before the theme they fix.

### Added

- **Rows can be selected and removed in bulk, in any table** (`gobj-ui`
    7.11.0 - 7.13.0). A shared facility, `yui_table_select.js`
    (`yui_selection_column`, `yui_selection_settings`, `yui_selection_bar`,
    `yui_wire_selection`), instead of a per-table checkbox: the treedb topic
    table takes it behind an opt-in flag (`with_selection_bar`), and so do the
    tables of the SPAs. The bar reports what is selected and clears itself; a
    tri-state parent checkbox writes its children in ONE gesture (the
    `indeterminate` state is a property, so those formatters must return DOM
    nodes, not markup).

- **A column drag can be undone** (`gobj-ui` 7.14.0, 7.14.1). Reordering a
    schema's columns raised *Version not raised* with no way back short of
    reloading the store. The remembered order is now reset only when the store
    is loaded, not on every write — which is what made the first Undo button
    invisible.

- **Connections and the Schemas picker read the same** (`yunos-js` 0.15.0,
    0.15.1). The two tables show a backend and the treedbs it exposes, and one
    is pasted **literally** into the other, but one was a tree and the other a
    flat table with a nested sub-table; the checkbox meant *open* in one and
    *select for deletion* in the other. Connections adopts the picker's shape:
    services are child rows, the checkbox means BROWSE in both with the three
    states on the connection row, and search, count and fold sit in the same
    place — the fold and the search are one wrapping unit, so a phone takes
    them to the next line together instead of leaving the fold stuck to the
    title. Removing several connections moved to its own dialog, with its own
    checkbox list, so marking rows to browse can never delete them.

## 7.16.1

### Fixed

- **The identity ack freed a record it does not own** (`c_ievent_cli`).
    `ac_identity_card_ack()` read the ievent stack record with
    `msg_iev_get_stack()` and then released it. That function returns
    `json_array_get(jn_stack, 0)`, a pointer the `kw` still owns — its own
    signature says so: *"Return is NOT YOURS!"*. The decref freed
    `__md_iev__.ievent_gate_stack[0]` while the array kept pointing at it, and
    the `KW_DECREF()` that follows in `ac_on_message()` walked the tree and
    freed it a second time.

    The second free reads a refcount out of released memory, so what happens
    next belongs to the allocator and not to the code: the block can still
    read `1` and be freed again, or it can have been reused and corrupted in
    silence. That is how one stray decref per connection survived since the
    gclass was imported. It is no longer silent on glibc 2.43 — the agent
    aborts with *"corrupted double-linked list"* while it processes the
    controlcenter's ack, seconds after start, and a run that gets past the ack
    dies later in `_int_malloc` instead.

    The path runs on every successful identity ack, which is every
    agent-to-controlcenter link and every citizen-yuno-to-agent link. This was
    the only site in the tree that treated the borrowed record as owned:
    `c_controlcenter` already pairs its `msg_iev_get_stack()` with an explicit
    `JSON_INCREF()`.

- **The watcher changed directory without looking** (`ydaemon`). `relauncher()`
    called `chdir(work_dir)` in the child and dropped the result, which the
    compiler had been reporting as `-Wunused-result` on every build. A failed
    `chdir` leaves the yuno running from whatever directory the watcher was in,
    and it said nothing about it. It now logs the path and the `errno`.

## 7.16.0

### Added

- **The daily report says which machine sent it** (`webstats`). The mail left
    every node with the same sender, the one persisted in `emailsender`'s
    `from` attr (`no-reply@artgins.com` here), so five nodes reporting into
    one mailbox were indistinguishable by sender — the hostname was in the
    subject and nowhere else, which no mail client sorts or filters by.

    `email_from` names it, and the config composes it: `json_config` resolves
    `(^^__hostname__^^)` inside a string, so
    `"C_WEBSTATS.email_from": "(^^__hostname__^^)@artgins.com"` reaches the
    yuno already resolved to `wattyzer@artgins.com`. The yuno stays out of it
    — no domain and no address shape built in — and any other variable works
    the same way. Empty leaves the sender to the email service, as before.

### Fixed

- **A probe that spells itself `/%2eenv` is still a probe** (`webstats`).
    The probe patterns were matched against the raw path, so a scanner that
    percent-encodes the interesting characters — `/%2eenv`, `/%2egit/%63onfig`,
    `/%2f%2eaws%2fcredentials` — shared no substring with `.env` or `/.git` and
    was counted as ordinary traffic. Eight such requests hid in one day of one
    node, and the packaged `fail2ban` filter is blind to them for the same
    reason: it matches the literals against the raw log line, and it has no
    way to decode first.

    The path is now percent-decoded before the patterns are applied, which is
    what it already means at the HTTP layer. Chasing the encodings from the
    pattern list instead is a game with no last move (`%2E`, `%2f`, and the
    combinations of both), so the patterns stay in their plain form.

    A malformed escape is copied verbatim, and so is `%00`: decoded it would
    end the string and hide the rest of the path from the match.

    `/cgi-bin` joins the default patterns, in `webstats` and in the packaged
    `fail2ban` filter together — they are one list in two files and drift
    between them is the bug. No node serves CGI, and what asks for it is not
    after a script: the requests are path traversal reaching for `/bin/sh`.

- **The agent's init script started a second web server, by hand**
    (`yuno_agent`). `yunos/c/yuno_agent/service/yuneta_agent` had drifted from
    the script the packagers generate, and it still carried a bare
    `/yuneta/bin/nginx/sbin/nginx` in its `start` case — no check for one
    already running, and plain nginx hardcoded while the openresty line sat
    commented out beside it.

    It is not a dead file: `CMakeLists.txt` installs `service/` into
    `/yuneta/agent`, which is where the `.deb`/`.rpm` postinst reads
    `yuneta_agent` from to seed `/etc/init.d/`. So a build on a source node
    overwrote the packaged script with the stale one — the same way a build
    overwrites the agent binaries.

    On a node serving with openresty, the result was a second web server of
    the wrong kind racing `yuneta-webserver.service` for ports 80 and 443
    every boot. It lost by eight seconds and left 21 `[emerg] bind() ...
    Address already in use` lines behind; had it won, it would have served
    the stock `server_name localhost` config for every vhost on the node.

    The copy here is now byte-identical to the one the `.deb` generates, which
    starts the web server with `systemctl start yuneta-webserver` — idempotent,
    and it picks nginx or openresty as the node chose.

    `ENTRY_POINT.md` §8.3.1 now says who owns the web server at boot, and
    §8.3 adds the `ulimit -l` the script has always set and the doc never
    showed — the memlock ceiling an io_uring ring is counted against.

- **The GeoJSON owes maplibre a real boolean** (`gobj-ui` 7.10.5).
    `devices2geojson` copied `device.connected` into the feature properties
    verbatim. Four style expressions test it with `['case', ...]`, and maplibre
    asserts a **strict** boolean there — so `null`, absent, or the `1`/`0` a
    backend may send all fail the assertion. Reproduced against maplibre's own
    expression engine.

    **The damage was not the console.** The failed assertion leaves the cluster
    accumulator NULL, and the cluster colour compares it against `point_count`
    — never equal, so the cluster paints **red as if a device were down**. The
    unclustered point and its label lose their colour expression too and fall
    back to the property default, black, instead of green or red. The map
    reported a state that was not true. `!!device.connected` also makes the
    `1`/`0` case work rather than merely stop erroring.

    In the same pass, `get_coordinates` read `device.settings.coordinates`
    while its own comment says `settings` may be null — a TypeError that would
    unwind out of `devices2geojson`.

- **The map asks for its source instead of guessing from the style**
    (`gobj-ui` 7.10.4). `C_YUI_MAP`'s refresh guarded itself with
    `map.isStyleLoaded()` and then called `map.getSource('devices').setData()`.
    The style is the wrong milestone: the source is added on the map `load`
    event, and `load` fires **one render frame after** `isStyleLoaded()` turns
    true. A refresh landing in that frame threw *"can't access property
    setData, map.getSource(...) is undefined"* — and the throw unwound through
    `gobj_publish_event`, aborting the publisher's own loop, so the caller
    stopped processing the rest of its batch.

    Measured against maplibre-gl 5.24.0 in Firefox, the window is exactly one
    frame, 10-42 ms, on every run — cold cache, warm cache and a
    `display:none` container alike.

    Testing the style was wrong in the other direction too: `style.loaded()`
    requires every tile manager to be loaded, so it returns to false while new
    tiles come in. Once the map was up, every refresh during a pan or a zoom
    was silently dropped and the devices stopped moving until the tiles
    settled.

    No in-repo consumer registers `C_YUI_MAP`, so no yuno changes here. The
    same fix ships on the frozen v1 line as **1.0.2** (npm dist-tag `legacy`),
    for estadodelaire and hidraulia, which is where it was found.

## 7.15.0

### Added

- **A delete says what it takes with it** (`gobj-ui` 7.10.3, `gui_treedb`
    0.13.3). The question was `are you sure` — the same words for one loose
    record and for one with six children hanging off it. And these views
    delete with **`force`**, which on a treedb node does not only remove it:
    its children are **UNLINKED** (they survive, loose) and it is cleaned off
    its parents. An operator could detach six records believing they had
    removed one.

    It names what is going and adds a line per thing at stake, each only when
    there IS something at stake — a loose record must not be dressed up as a
    dangerous one. Counted off the record the table already has, so asking
    costs no round trip; the counting is pure and tested, because a hook or
    fkey value arrives as a list of refs, a dict keyed by id or a single ref
    string, and a column can be BOTH — which counts on both sides, because the
    delete does both things.

    In the graph the node-delete popover carries the same lines, and the
    **unlink** popover carries the reassurance that is its whole point:
    *neither record is deleted*. Next to a delete button painted the same red,
    that is not obvious.

    Three traps, all found by running it, and all worth knowing before
    composing any message from keys: `yui_shell_confirm_*` renders its message
    **as an i18n key** (so a composed sentence can never be one — pass DOM);
    `createElement2` **trims text nodes** (so a `" "` separator vanishes and it
    reads "BorrarDeveloper" — space with CSS); and a **counted** word must
    carry no `i18n` attribute, because the dialog re-translates from the key
    alone, without the count, and puts the plural back over the singular.

- **`nodes` can answer a PAGE** (`c_node.c`). A treedb lives in memory, so
    walking it is not what costs: serializing every node, pushing it through a
    websocket and parsing it in a browser is. `nodes` takes `from` (1-based)
    and `limit`, and cuts the answer on the way out.

    The contract is deliberately the one `list-keys` of `C_TRANGER` already
    uses: with **no `limit`** the answer is the plain list it has always been,
    so every client written before this keeps working; asking for a page gets
    the envelope `get-page` uses, `{total_rows, pages, data}`, so a client
    pages nodes exactly as it pages records. Filtering happens BEFORE the cut,
    so `total_rows` is the size of the match and not of the topic, and a page
    past the end is empty while still reporting the true total.

    `KW_WILD_NUMBER` on both, because they arrive as strings through the
    agent's `command-yuno` forwarding, which does not coerce — and that half
    is pinned by the test too. New test: `tests/c/c_node_paged_nodes`
    (124/124 green). Documented in `YUNO_TREEDB.md` §5.3.

    **The SPA asks for pages** (`gobj-ui` 7.9.2, `gui_treedb` 0.12.2): each
    topic table pulls the page it is showing instead of the host pushing the
    whole topic down. The page size is generous on purpose (200), so a treedb
    that fits in one page behaves exactly as it did — paginator hidden, every
    filter seeing every row — and only a topic that does not fit pays for
    paging. Safe against a backend that cannot page: it answers the whole
    list, which the table reads as one page.

    The header filters and the search box work on the page that is loaded
    (`filterMode: "local"`), which is what the tranger browser's Rows card
    already does and for the same reason: the alternative is pushing every
    filter to the backend and changing what "search" means.

    Verified end to end against staging: `treedb_system_schema`'s `cols`
    arrives whole in one page of 200 (114 rows, paginator hidden), and at 50
    per page it really pages — 50, 50, 14, with different rows on each.

    Two traps worth keeping, both found by running it: the correlation id
    must be read **flat** off the command stack (`C_IEVENT_CLI` EXTRACTS
    `__md_command__` and pushes it AS the stack's `kw`), and whether there is
    more page must come from `getPageMax()`, not from the row count against
    the page size — 50 rows of a 50-row page reads as "it all fits" and it
    does not.

- **Edit a cell of a topic table in place** (`gobj-ui` 7.8.0, `gui_treedb`
    0.11.0). Changing one field meant opening the record form, changing it,
    saving and closing — four clicks for a word. A writable scalar is editable
    in the table now, in edition mode.

    Which cells: the schema decides (`writable` only, never the pkey —
    renaming what a record is KEYED by is not a field edit) and the type
    decides the rest. A hook holds children and an fkey IS a link, so both are
    edited by linking; a dict or a list is a document the form has an editor
    for; a date cell shows a formatted string over an epoch, so typing into it
    would write the string. Those stay with the form, one click away on the
    same row.

    **The write is a partial update with no `autolink`, and that is the whole
    safety of it.** `treedb_update_node()` merges (`json_object_update`), so
    the fields it does not carry are left alone; `autolink` wipes a node's
    links to rebuild them from the fkeys the record carries, and on a partial
    record it reads that as "no parents", detaches the node and answers
    success. So a cell edit travels as its own event and does not reuse the
    form's, which does send autolink and may — the form hands it the whole
    record with its fkeys in it. Verified against a live treedb: a role edited
    in place kept its parent, and the parent kept it.

    A refused write puts the topic back to what the treedb has: leaving the
    typed value on screen is tolerable for a form, which stays open on the
    values it failed with, while a cell edited in place would just look saved.

- **The agent console hands its backends to the TreeDB GUI**
    (`yunos-js`: `gui_agent` 0.9.0, `gui_treedb` 0.10.0). Copying a dozen
    `wss://host:port` rows between two tabs of one browser was the
    alternative. The Schemas picker copies the yunos it shows as gui_treedb
    connections; that app's Connections page pastes them (a clipboard read is
    not always allowed — Firefox refuses outright — so a refusal opens a box
    to paste into rather than failing).

    The picker already asked every yuno what services it runs, which is how it
    knows which ones hold a treedb, and threw the answer away one line after
    it arrived. What it lacked is the ENDPOINT, and one `view-config` per yuno
    gives it:

    - the **port**, from
      `global["<gate>.__json_config_variables__"].__top_url__`. A yuno without
      it exposes no top gate and can never be a treedb backend, so its absence
      is the filter — a node with 17 yunos contributes the 2 that are
      reachable.
    - the **host**, which is not in that url (the yuno binds `0.0.0.0`, its
      realm binds `127.0.0.1`): the filename of `__ssl_certificate__` — the
      FQDN the certificate is issued for — else the realm id. Neither is
      guaranteed, so the url is a proposal and the connections arrive
      **disabled**.
    - the **service**, which is the one NAMED like the yuno's role. Taking
      "the first top-service" instead made all nine backends of a real scan
      read `authz`, which is a connection the backend refuses.

- **A topic table you can read, and a graph you can search**
    (JS submodules: `gobj-ui` 7.3.0, `yunos-js` — `gui_treedb` 0.8.0).
    The treedb topic table had a single global search box while the read-only
    tranger browser sitting next to it had per-column filters, a column chooser
    and a CSV export: the richer table was the one that cannot write. It has
    all three now (`with_header_filters`, `with_columns_button`,
    `with_export_button`, all default on).

    The filter box is **not** put on every column. A hook holds children, a
    dict holds a subtree, and a date cell shows a formatted string over an
    epoch number — a text match against the raw value there is a box that lies,
    so those columns get none. A `boolean` gets a tristate tick, an `enum` the
    list of its own values, and an `fkey` a box that stringifies the value
    first, because *which rows point at X* is the question fkey columns exist
    to answer. Search and header filters are separate layers: clearing the
    search does not drop the column filters. The CSV carries what the table
    HOLDS — loaded rows, visible columns, both filters applied — and not the
    topic, which is a server-side dump this view cannot stream.

    The graph gets a find box. It matches the term against a node's **label**
    as well as its id — a topic keyed by `rowid` / `uuid` / `qualified` is
    keyed by a counter or a path while the name a human knows sits in a
    secondary key — and it **says how many** it found, because a graph that
    did not move looks the same whether nothing matched or the match was
    already on screen.

    Searching in the table now crosses the FSM (`EV_SEARCH`) instead of
    calling Tabulator straight from the DOM handler, where the `machine` trace
    could not see the one action a reader performs most often.

- **A legend for the topic colours, a minimap, and one click to take every
    service** (`gobj-ui` 7.4.5, `gui_treedb` 0.9.0). A node's port colour
    encodes the topic it links to — a deliberate, functional cue, and nothing
    on screen said what any colour meant. The graph gets a legend strip, and
    it is a strip rather than an overlay because it is read AGAINST the graph.
    Its entries are buttons: one focuses its topic and travels up as
    `EV_TOPIC_SELECTED`, so the host turns it into the URL and what you are
    looking at stays linkable.

    A minimap appears from 30 nodes on (`minimap_min_nodes`). One of a graph
    that already fits on screen is decoration, so it is not a preference to
    find and set. Its shapes are drawn by hand — a block in the topic's
    colour — because G6's minimap clones each element's KEY SHAPE and these
    are `html` nodes; at that scale a card is a rectangle anyway.

    In Connections, the **browse** column header selects every service of a
    connection at once. A yuno routinely exposes a dozen, and one click each
    was the only way there was; the header carries the state of the whole
    column (all / none / some) and flips it.

- **A treedb nobody has arranged opens laid out, not piled up**
    (`gobj-ui` 7.5.7, `gui_treedb` 0.9.2). `manual` means "leave every node
    where it was put", and where none was put it means a cascade —
    `get_default_ne_xy()` walks x and y together, so a first open was a
    diagonal pile of cards, 126 of them on `treedb_system_schema`, and the way
    out was knowing to pick a layout by hand. It opens in **dagre** unless most
    of its nodes carry a saved position; the pick is not persisted, so one
    dragged node makes `manual` right again by itself.

    And it opens FITTED — but never zoomed out past the point where a card
    carries no readable text. dagre spreads a schema treedb (one topic, a
    hundred columns) far enough that fitting lands near 0.2 zoom: technically
    the whole graph, and a hairline. It stops at a legible zoom and centres,
    and the minimap such a graph always has says where that part is.

    Two traps came out of this that are worth knowing beyond it, and both cost
    a wrong guess before the real value was pulled out:

    - **`gobj_write_attr()` writes the private field of the same name.** A
      `priv.x` therefore stops answering "did the HOST set attr x?" the moment
      anything resolves a default into it.
    - **After a graph is built, `record._geometry` is not evidence of anything
      a human did.** `get_node_graph_props()` returns the record's own
      `_geometry` when it has one — and a treedb hands back `{}` for a record
      nobody moved — and the `kw_get_int(..., KW_CREATE)` that follow write the
      invented cascade coordinates into it. Every node of every treedb read as
      "placed".


- **The record form says what is wrong, and says it in the app's language**
    (`gobj-ui` 7.7.1, `gui_treedb` 0.10.2). Pressing Save on a form with a bad
    field did **nothing at all**: the branch that refuses the save wrote
    `abort_close` and `warning` on the click's own kw, which nobody reads —
    that pair belongs to the close path. It calls `reportValidity()` now,
    which marks every bad field and puts the caret and the viewport on the
    first.

    The message a field did show was `input.validationMessage`: the BROWSER's
    sentence in the BROWSER's locale, so a Spanish form on an English Firefox
    read "Please fill out this field." The empty required field has its own
    key now; the rest still falls back to the browser, whose wording for a bad
    pattern or an out-of-range number is better than anything generic.

    And two dialogs of the editing flow were untranslatable: `yui_shell_confirm_*`
    renders its message as an i18n KEY, so the English sentences passed in were
    keys nobody had defined — they render as themselves, in every language, and
    **no locale validator can see a key that travels as data**. New keys for
    consumers: `this field is required`, `all changes will be lost`.
### Fixed

- **The amber highlight in the treedb graph had never appeared**
    (`gobj-ui` 7.3.0). `EV_FOCUS_TOPIC` has been setting G6's `active` element
    state since the topic-cards landing, and that state is defined as an amber
    `stroke` plus a `halo` — both properties of a node's KEY SHAPE. Every node
    in this graph is an `html` node, whose key shape is a DOM element, so
    neither property ever had anything to paint on: the topic focus centred the
    viewport and marked nothing. The highlight is drawn into the card's own
    html now, repainting only the cards whose state changes, and a theme switch
    carries it across — that path rebuilds every card and would otherwise clear
    what is on screen.

- **The row-count footer lied under a filter** (`gobj-ui` 7.2.4 + 7.2.5). It
    read "5 Filas" over four visible rows. `dataProcessed` fires on `setData`,
    not on a filter; and subscribing to `dataFiltered` was not enough either,
    because Tabulator dispatches that event from inside its own `filter()`,
    which only returns the surviving rows to the pipeline afterwards — so
    `getDataCount("active")` answers the pre-filter set there. The footer takes
    the rows the event hands over. It went unnoticed while the only filter was
    the global search box.

- **A filtered column had no room for its filter** (`gobj-ui` 7.2.2 + 7.2.3).
    The table lays out `fitDataFill`, which sizes a column to its DATA, so a
    `Role` column holding "root" came out narrower than the box it had just
    been given and its placeholder was cut to "filtrar c". Text and list
    filters carry a `minWidth` of 150 — what the placeholder needs in the
    LONGEST locale, which is the measure that decides, since a column width
    cannot follow the language.


- **The schema diagram keeps the size it is drawn at, and its arrows say who
    references whom** (JS submodules: `gobj-ui` 7.0.1, `yunos-js`). Selecting
    Diagram in `C_YUI_TREEDB_SCHEMA` drew the graph at its own scale and then,
    a frame later, zoomed it to the container: the reader saw one size and got
    another, and which one depended on how many topics the treedb had. The fit
    is gone — the canvas still follows the container when the view is shown,
    since G6 draws at 0x0 while hidden, but the camera no longer moves, so the
    scale and any pan/zoom survive.

    The arrowheads were also backwards. This view exists to draw a treedb the
    way its `.c` literal draws it in ASCII, and the `.c` lands the arrowhead on
    the parent's HOOK row: the reference belongs to the child's fkey and points
    up at its parent, which is what the `↖` in the `(↖)` / `[↖]` / `{↖}` marks
    the same view prints has always said. The edge stays declared parent ->
    child, because that is what ranks the parent first under left-to-right
    dagre; only the marker moved.

### Changed

- **`maplibre-gl` moves to `6.4.1`, and gobj-ui's peer floor moves with it**
    (`gobj-ui` 7.0.0, a dependency-only major — no component API moved).
    6.4.1 fixes `DOM.sanitize` leaving dangerous attributes behind when several
    of them sit next to each other: it iterated a live `NamedNodeMap` while
    removing from it, so every removal skipped the attribute right after it and
    an `ontoggle` could survive the scrub. A floor is the only thing that stops
    a consumer from resolving to a version without that fix, so the floor is
    what moves. `yunos/js/gui_treedb` declares maplibre itself and moves too.

    Not affected: the v1 line. `estadodelaire` and `hidraulia` are the two SPAs
    that actually mount a map in production and they are on `maplibre-gl`
    `^5.24.0`, which is the last 5.x there is — the fix is not backported, so
    the only way out of it is the migration to v2.

## 7.14.0

A schema stops being read by its rows. The order of the columns is part of
the schema and no longer the order the filesystem happens to hand the
projection back in; and the console that edits schemas stops showing the
three topics they are STORED in and shows the schema they ARE — and stops
losing your place in it.

The consoles also start saying WHICH machine you are looking at, in the two
places that never said it: a treedb tab named after a treedb that lives on
four backends, and a collapsed node hiding the yunos it has open.

### Added

- **The Schemas workspace of the agent console edits a schema AS A SCHEMA**
    (JS submodules: `gobj-ui` 6.3.0, `yunos-js`). Every schema a yuno holds
    lives in its `treedb_system_schema`, stored as data in three flat topics
    linked by fkeys — `treedbs` -> `topics` -> `cols`. That is the right
    storage and it was the whole screen: adding one column to one topic meant
    finding it in a table holding every column of every topic of every treedb
    the yuno has, composing the parent fkey by hand, and remembering to raise a
    `topic_version` that nothing asks about.

    The new `C_YUI_SCHEMA_EDITOR` puts the schema back together — treedb ->
    topics -> columns in declared order, reordered by dragging, flags as
    checkboxes that say what they do, the schema DRAWN from the records being
    edited, a check of what the treedb would refuse, an export as the C literal
    and an import shown as a plan before it runs.

    **Both versions travel with every write**, and the operator is asked to
    remember neither: `topic_version`, without which the persisted
    `topic_cols.json` masks the edit — the restart succeeds and nothing moved —
    and `schema_version`, which is what publishes the schema as a whole.
    Raising it is safe because `reconcile_treedb_schema()` compares
    `c_schema_version`, the version of the LITERAL, precisely so that an edit
    made there survives every start until a newer literal arrives.

    No C changed for this. It is listed here because the two JS submodules move
    with the release, and because the behaviour it depends on is the kernel's:
    `autolink` on a partial `update-node` rewrites a node's links from the fkey
    fields the record carries, finds none, and reads that as "no parents" — the
    editor learned it by detaching a topic from its treedb on a dev node, with
    the write answering success.

- **Both consoles say WHICH machine you are looking at** (JS submodules:
    `gobj-ui` 6.3.0, `yunos-js`). The same blind spot in two shapes, and the
    same cost: an edit made on the node you did not mean.

    A **gui_treedb tab** is labelled with the TREEDB name, and a treedb name is
    not unique across backends — two tabs reading `treedb_authzs` are two
    different machines. `C_YUI_TREEDB_TOPICS` takes a `source_url` and prints
    it in its toolbar, which is where it fits: a tab wide enough for
    `wss://host:1996` is a tab bar with room for one tab.

    In the **agent console** what is open in Statistics and Schemas is a YUNO,
    and the checkbox that says so is on a child row — invisible while the node
    is collapsed, which is how a node is read most of the time. The node row
    now carries the labels of its open yunos, read from the SELECTION and not
    from the loaded children, because it is asked exactly when the children are
    not on screen.

### Fixed

- **`system-schema` answered with a validator, not with the schema.** The
    command of `C_NODE` and `C_YUNO` returned `_treedb_create_topic_cols_desc()`
    — the descriptor a user column is validated against, which is DERIVED from
    the `cols` topic of `treedb_system_schema`: one topic of three, with the
    storage-only fields dropped and `value` renamed back to `id`. What it
    claims to answer is the meta-schema, and `C_NODE` already publishes the
    schema of its own treedb through `desc`/`descs`, so there was nothing else
    it could mean. It now returns the whole thing — `treedbs` -> `topics` ->
    `cols` plus the `schema_version` that says how a schema is stored — and it
    reports a parse failure instead of answering an empty success.

    The literal is handed over by the new `treedb_create_system_schema()`
    (`tr_treedb.h`), which is where the parse lived three times: the column
    descriptor and `c_treedb`'s materialization of the `__system__` treedb now
    go through it too.

- **The consoles stop losing your place** (JS submodules: `gobj-ui` 6.3.0,
    `yunos-js`). Two reports from the agent console's Schemas workspace, and
    the same shape twice:

    - A **strip of treedbs** dropped what each one had open. Open a topic,
        move to a sibling treedb, come back — its cards, with browser Back the
        only way to the table that was there. A `C_YUI_NODE` nav item pointed
        at the canonical route of its child; with the new `remember_position`
        it points at the tail last active under it. It stays a REAL position,
        which is why this belongs to the tree and not to a viewer restoring
        itself: clicking is a navigation like any other and nothing argues with
        the url.
    - **Shell tabs** had it too, and there the same trick is impossible: an
        item's route is where the tab's view is MOUNTED and what a deep link
        resolves to, so moving it would move the mount. The position is
        replayed when the tab is ENTERED AGAIN — never when you arrive at the
        root of the tab you were already in, which is the view's own way OUT of
        a topic. gui_agent already did this for its node tabs; gui_treedb does
        it now for its treedb tabs, with the decision in a tested function.

    - **The picker was ERASING the position** it should have replayed. A tab
        replays where it was left when it is entered again, and "again" was
        decided by whether the previous route was another tab — but the picker
        left that decision by its own early return, so the tab it had left
        behind stayed on record as the one we were in. Going to Select and back
        read exactly like walking up out of a topic: no replay, and the root
        recorded over the position. The tab we were in is now consumed at the
        one point every route passes through and written again only by the tab
        branch, so a route that forgets to clear it cannot exist.

    Also: a **yuno tab names its node** (`yuneta_agent · wattyzer`). Every node
    runs a `yuneta_agent`, so two tabs of two nodes read the same and there was
    no way to know which one you were typing into.

- **A table paints its columns in the order the schema declares them
    again.** Since 7.13.0 a treedb opens from its `__system__` projection, and
    the projection is a treedb like any other: its nodes load in the order
    `readdir()` returns the key directories, which on ext4 is hash order. That
    order became the order of `cols` in the rebuilt schema, then got FROZEN
    into each topic's `topic_cols.json` the next time `topic_version` rose, and
    from there into every table: `list-yunos` came back with `_channel_gobj`
    first and `id` third, in a different scramble on each node.

    Four things had to change for an order to survive:

    - **The order is stored.** `topics` and `cols` of `__system__` carry an
        `order`, stamped by the projector from the position the node occupies in
        the schema compiled in C. It is storage, not something a column may
        declare: `_treedb_create_topic_cols_desc()` keeps it out of the
        descriptor a user column answers to, and `get_treedb_schema()` takes it
        away again on the way out — in a schema the order IS the sequence of the
        dict, and a schema carrying both would hand every topic a column
        attribute nobody wrote.

    - **The rebuild sorts by it**, and falls back to where the schema compiled
        in C declares the node when the projection cannot place it — one
        projected before the index existed — so a store that has not been
        re-projected yet already comes back in order. `order` defaults to
        **9999**: a column created in `__system__` by hand says nothing about
        where it goes, and what says nothing goes last.

    - **The keys load sorted** (`find_keys_in_disk()`). `readdir()` order was
        never a contract: two replicas of the same store read it back
        differently, and so did the same store twice.

    - **A topic_cols.json that differs from the schema ONLY in the order is
        rewritten**, instead of waiting for a `topic_version` bump. The file is
        deliberately frozen until the version rises — that is what stops a
        change to WHAT a column declares from arriving unannounced — and a
        change to the order announces nothing new.

- **A phone stopped cutting the node list in half** (`yunos-js`). Tabulator
    ends a cell that does not fit with an ellipsis, and `fitColumns` leaves the
    status column about half of what *"Ejecutando Solo lectura"* needs. Name and
    status wrap now. Two things were needed: `variableHeight` on the columns, so
    the table grows the rows, and `height: auto` on those cells, because
    Tabulator measures once and writes that height into every cell — a column
    that gets narrower afterwards (a rotated phone, a resized window) wraps into
    a height nobody re-measured and loses the second line.

- **gui_treedb opens on the work.** Its rail is Topics / Graphs / Connections,
    the backends last: they are where you go when one has to be added or fixed,
    which is not what a session is spent doing. `/connections` is the same
    route — linkable, in the site map, and the landing is unchanged.

- **Meta-schema at `schema_version` 15** (`topics` topic_version 7, `cols` 9),
    which re-projects every store on the next start and rewrites every
    `topic_cols.json` with the order the schema declares.

## 7.13.2

A key that says where it belongs: the schema stored as data stops being
addressed by a counter, and starts being addressed by its own name.

### Changed

- **BREAKING (stored data): `topics` and `cols` of `__system__` are keyed by
    the QUALIFIED name, not by a rowid.** The id of a node is now the id of its
    parent, a dot, and its own name — `treedb_yunovatioscodb.yunos` for a topic,
    `treedb_yunovatioscodb.yunos.yuno_role` for a column.

    The collision it has to avoid is real: a topic name is unique only inside
    its treedb, so keyed by the bare name the second treedb of a store
    declaring `users` collided with the first and lost its schema. A rowid
    handed out from `tranger2_topic_size() + 1` avoided it and paid for it — it
    does not reproduce, so two projections of the same schema do not align;
    `find_col_id()` and `find_topic_node()` had to be linear scans over `value`
    because the key said nothing; and a rowid pkey has no update, so an editor
    saving a column appended a second one under the same name instead of
    changing it.

    **The separator is a dot and cannot be `^`.** That is the character an fkey
    reference is split on — `decode_parent_ref()` requires exactly
    `parent_topic^parent_id^hook` — so a column keyed `treedb^topic^col` makes
    every reference to itself undecodable.

- **A new `id` flag, `qualified`, beside `uuid` and `rowid`.** A create that
    sends no `id` gets one composed from the parent named in its fkey and the
    value of the topic's first secondary key (`build_qualified_id()` in
    `tr_treedb.c`). So an editor creates a column the way it creates any other
    record, and the rule lives in the store instead of in each client.
    Declarable by any schema; `tr_treedb.h` and `lib_treedb.js` carry the
    vocabulary.

- **Meta-schema at `schema_version` 14** (`topics` topic_version 6, `cols` 8),
    which re-projects every store on the next start.
    `migrate_schema_ids_to_qualified()` runs first and moves a projection made
    with rowid keys, node by node: it rewrites each under its qualified id with
    its content intact — an operator's addition is in there too and is not the
    projector's to drop — and deletes the old one, columns before their topic
    because deleting a parent only UNLINKS its children. It is the one delete
    the projector does, and it happens once per store.

- **An id that does not fit is refused, never trimmed.** Both composers built
    the qualified id with a bare `snprintf` into a buffer of one record key,
    and both halves can be a full key on their own. The id is the ADDRESS of
    the node, so a truncated one silently addresses something else — two long
    names sharing a prefix would land on the same record. They read the return
    value now, log, and answer nothing; every call site skips that topic or
    column. GCC saw only one of the two: `-Wformat-truncation` could bound the
    projector's buffers and not the ones fed by a json string.

- **Published: gobj-js `7.13.2` and gobj-ui `6.0.0`.** gobj-js ships ahead of
    the SDK release it belongs to, at the number the SDK will catch up to (the
    same way `7.6.7` did): `qualified` joins `treedb_field_types`, the list
    that turns a column FLAG into the `type` every form and table switches on.
    gobj-ui 6.0.0 is a dependency-only major — no API moved — that raises its
    peer floor to gobj-js `^7.13.2`, because on an older runtime the word is
    missing and the qualified paths fail quietly. The in-repo SPAs
    (`gui_agent`, `gui_treedb`) take both.

- **gobj-ui labels a `qualified` key by the secondary key too**
    (`treedb_node_label.js`), the same way it already did for `rowid` and
    `uuid`: the id names the record but names every ancestor with it, and the
    card wants the leaf. The pkey stays as the tooltip. The form treats a
    qualified pkey as a key the store hands out — never typed on create — and
    an existing row opens for **update**, which is what stops an edit from
    appending a twin.

## 7.13.1

A schema you can read: the descriptor learns to say what NAMES a record, and
the two treedb views stop showing storage where a schema was asked for.

### Added

- **A topic descriptor now carries `pkey2s`.** `tranger2_topic_desc()` cloned
    `topic_name`, `pkey`, `tkey`, `system_flag`, `topic_version` and `cols` — so
    a viewer of a treedb learned which column is the primary key and never which
    one is the SECONDARY key.

    That is the difference between identifying a record and naming it. A topic
    whose id column is flagged `rowid` or `uuid` keys its records by a value
    nobody reads, and the name a human knows them by lives in its `pkey2s`
    column: `treedb_system_schema` keys `topics` and `cols` by rowid and holds
    the topic/column name in `value`. Without `pkey2s` in the descriptor the
    only thing a GUI could print was the rowid, which is how the agent console's
    graph came to draw cards reading `181`, `225`, `193`.

    Additive and safe on both sides: `kw_clone_by_path()` skips a key a topic
    does not have, and the reader (gobj-ui 5.17.0) falls back to the id when the
    descriptor does not carry the field, which is what an older node answers.

- **`diff-schema`: what the stored schema says that the schema in C does not.**
    A treedb opens from its projection in `__system__`, the projector never
    deletes, and a re-projection publishes under `max(stored, literal) + 1`. So
    the three numbers of the `treedbs` node say that SOMETHING was published and
    never what: `schema_version` 24 over `c_schema_version` 23 is the shape of an
    operator edit and the shape of a plain re-projection alike. The new command
    of `C_TREEDB` tells the two apart.

        ycommand -c 'command-agent service=treedbs command=diff-schema \
            treedb_name=treedb_yuneta_agent'

    It answers one row per difference (`treedb`, `kind`, `topic`, `col`, `attr`,
    `stored`, `from_c`) plus a comment with the stored versions, the ones the
    running yuno carries and the count. `kind` is `changed`, `only_in_stored`
    (an operator addition, or a topic a later schema dropped and the upsert
    kept), `only_in_c` (declared and never projected) or `version`.

    Two rules keep the answer readable, and both are about the store rather than
    about schemas. A record carries EVERY column of its topic filled with the
    empty value of its type, so an attribute nobody wrote is stored as `""`,
    `{}`, `[]` or `0` and is not a difference — read as one, those defaults
    buried 6 real differences under 592. And the version stamps are not compared
    as content, since the projector raises them itself: only the anomaly is
    reported, a projection made from another release or a topic the
    re-projection never reached.

    The comparison is projection against projection: the schema from C is
    projected in memory through the same two builders the projector uses
    (`build_topic_projection()` / `build_col_projection()`, extracted from
    `upsert_treedb_schema()` for this), so no default the projector fills in can
    read as a difference nobody made, and the two paths cannot drift apart.
    `C_TREEDB` now keeps the schema each treedb was opened with, which is the
    other half of the comparison; a treedb opened from its projection alone has
    nothing to compare with and the command says so.

- **`yunos/js`: gui_agent asks it from the Schemas workspace.** A `Differences`
    button beside *Apply* asks each `C_TREEDB` service of the yuno and shows the
    answer as a table, with what the command says about the versions above it.
    A service that refuses, or that is too old to know the command, says so in
    the same dialog beside the ones that answered. Detail in that submodule's
    own `CHANGELOG.md`.

    Note it takes the `read` permission of the **`C_TREEDB`** service, which is
    not the `C_NODE` service of the treedb: an identity that reads a schema can
    still be refused here.

- **gobj-ui submodule 5.16.0 → 5.17.0: the treedb views become readable on a
    schema.** Two views, two different failures, one release.

    `C_YUI_TREEDB_SCHEMA` — the `/schema` landing of a treedb — drew one 40px
    circle per topic with its name underneath, which answered neither of the
    questions a schema is opened for: what a topic holds, and what links to
    what. It now draws what the `.c` literals draw in ASCII: one CARD per topic
    listing its fields in schema order, and one edge per hook, leaving the row
    that declares the hook and landing on the fkey row of the child it names.
    The marks are the notation of those literals (`{}` `[]` `()` `(↖)` `[↖]`
    `{↖}` `*` `#`), so the drawing and the source read the same. Both ends of
    an edge come from the declaration — `'hook': {'yunos': 'realm_id'}` names
    them — and a self-referent hook draws as the loop it is.

    `C_G6_NODES_TREE` — the RECORD graph — labelled every card with `record.id`,
    which on `treedb_system_schema` meant a fan of cards reading `181`, `225`,
    `193`. It now reads the pkey column's flags and, when the key is synthetic,
    labels by the secondary key, keeping the pkey as the tooltip. That is what
    the `pkey2s` above is for.

    The two answer different questions and the confusion between them is the
    whole story: the node graph draws RECORDS, so on a treedb whose records ARE
    schemas it draws a box per column — a correct picture of the storage and an
    unreadable picture of the schema.

    `yunos/js` follows: `gui_agent` and `gui_treedb` at `^5.17.0`.

### Changed

- **Every JSON file the framework writes is indented with FOUR spaces**, not
    two. `save_json_to_file()` is the one helper behind all of them —
    persistent attrs, configs, whatever a gclass saves — so the change reaches
    every file at once, and it puts the C side on the rule the rest of the
    project already follows: one level of structure is four characters.

    Only whitespace moves. Nothing reads these files by column, and jansson
    parses either shape, so an old file stays readable and is rewritten with
    the new indent the first time something saves it.

## 7.13.0

A schema stops being something only the C literal knows.

The `__system__` treedb holds it as DATA again — projected on every open,
reconciled by version, and able to rebuild the schema a treedb opens with — and
`gui_agent` grows the **Schemas** workspace that edits it: the treedbs of any
yuno of any node, over the one control-center session the console already has,
as tables or as a G6 graph. The node's own agent is one of the entries.

The other half is honesty about who may write. Only the MASTER of a treedb's
tranger can, and until now a replica **answered success** and lost the row at
the next reload; it refuses now, `treedb-info` says which one a yuno is, and
the editor opens read-only instead of turning every click into a toast.

And a family of small lies found by pulling one thread: every `authzs` command
answered with its permission list in the COMMENT field — which the client
refuses — four of them answered nothing at all, and the one helper behind them
leaked a whole `kw` per call.

### Fixed

- **Eleven full-path buffers were sized with `NAME_MAX`.** `NAME_MAX` is 255 and
    documents a FILENAME; these were filled with a whole path by `build_path()`
    or `yuneta_realm_file()`, so a long realm, role or config name truncates.
    Nothing was silent — `build_path()` logs the overflow — but the write
    failed.

    The one that started it: `dbsimple.c`'s `save_json()` used `NAME_MAX` for
    the same path `load_json()` reads with `PATH_MAX`, so a yuno could load its
    persistent attrs and fail to save them. Also `entry_point.c` (the file log
    handler), `c_resource2.c`, `c_agent.c` (audit file, yuno data dir, two temp
    files) and five callers each in `c_ycommand.c` and `c_cli.c`.
    `get_persist_filename()` keeps its `NAME_MAX` buffer: that one really is a
    filename.

- **`gobj_build_authzs_doc()` never released `kw`, and every `authzs` leaked
    one.** Its sibling `gobj_build_cmds_doc()` decrefs on all three of its
    return paths; this one decrefs on none of its four — while all fifteen
    callers `KW_INCREF(kw)` before calling, precisely because they believe it
    consumes one. Neither header said `// owned`, which is how the two helpers
    drifted apart.

    It now consumes `kw`, with a single exit: `authz` and `service` are
    BORROWED out of the kw and used by `authzs_list()` and by the error
    messages, so the decref cannot happen before the last of them.

    Measured, not argued: 20 `authzs` against a treedb service and an orderly
    `kill-yuno`, reading the `gbmem` audit at shutdown — **753 leaked blocks
    before, 0 after**. This is what the *"system memory not free"* line at the
    end of a yuno's log has been reporting.

- **`authzs` answered with the permission list in the COMMENT field, and
    `ycommand` refused it.** `msg_iev_build_response()` takes
    `(result, jn_comment, jn_schema, jn_data, kw)` and every one of them is a
    `json_t *`, so the compiler cannot tell them apart — but `jn_comment` is a
    STRING (`ycommand` reads it with `kw_get_str` and prints it with `%s`),
    while the authzs doc is a list. Every `authzs` command therefore logged

    ```
    ERROR: ... "function": "kw_get_str", "msg": "path MUST BE a json str", "path": "comment"
    ```

    with a full stack trace, and printed no permissions at all. `cmd_help` is
    NOT affected and was left alone: `gobj_build_cmds_doc()` returns a json
    string, which is exactly what a comment is.

    Eleven copies of the same three lines — `c_agent`, `c_authz`, `c_node`,
    `c_task`, `c_tranger`, `c_treedb`, `c_postgres`, `c_pepon`, `c_teston` and
    the two timer tests. `C_PROT_MQTT`'s `list-topics`, `list-clients`,
    `list-users` and `create-user` had it too, with the resource list in the
    same wrong slot.

- **The SKELETONS shipped the broken shape.** `gclass_service` and
    `yuno_citizen` both scaffolded the bare `return gobj_build_authzs_doc(...)`
    — which is where the four gclasses above got it. Fixed at the source, with
    the reason in the template so the next reader does not have to find this
    entry.

- **Four `cmd_authzs` answered nothing at all.** `C_IDP_KEYCLOAK`,
    `C_MQTT_BROKER`, `C_CONTROLCENTER` and `C_WEBSTATS` returned
    `gobj_build_authzs_doc()` raw, and `command_parser()` hands a handler's
    return value straight back with no wrapping. That is not a display
    problem: `msg_iev_build_response()` is also what sets `__md_iev__` back on
    the answer, so a bare doc has nothing to route it to the requester and the
    command **hung until the caller gave up** — verified live, 25 s with no
    answer against an unpatched yuno, correct output against the patched one.

### TreeDB

- **`C_NODE` answers `treedb-info`, and refuses writes on a replica.** Two
    halves of the same hole.

    The **master flag was not reachable** from the control plane: it is an
    `SDF_RD` attr of the tranger, absent from `services`, from `treedbs` and
    from the stats, so a client wanting to know whether a treedb can be edited
    had to fetch the entire `print-tranger` dump — megabytes to read one
    boolean. `treedb-info` answers `{treedb_name, master, schema_version, topics}`
    — the topics BY NAME, and the `__schema_version__` the treedb was built
    with, which is what tells a client whether the schema it is looking at is
    the one it knows. It is also
    per TREEDB, not per yuno: the same yuno is routinely master of its
    `treedb_system_schema` and a replica of a data treedb. The role comes from
    the config — only a yuno configured as master opens the store in exclusive
    mode, and one configured `master: false` never competes for the lock, so
    the start order does not change it.

    And **writing to a replica used to answer success.** `create-node`,
    `update-node`, `delete-node`, `link-nodes`, `unlink-nodes` and `import-db`
    built the node in the in-memory treedb and returned it; the append was
    never attempted, so `tranger2_append_record`'s own "NO master" guard never
    fired and **nothing was logged**; the row was gone at the next reload. They
    now answer

    ```
    ERROR -1: <yuno>: treedb '<name>' is READ-ONLY, this yuno is not the master of its tranger
    ```

    checked BEFORE the authz check, because on a replica nobody can write
    whoever they are, and a `-403` sends the operator after a permission that
    would not help. Reads are untouched — a replica exists to be read.

    Verified against two live yunos sharing one store, one master and one
    replica: the four write commands refused on the replica, reads kept
    working, writes on the master still persisted to disk. 123/123 ctest.

    Not covered, and deliberately: the snap commands (`shoot-snap`,
    `activate-snap`, `deactivate-snap`) also write, and their master semantics
    were not verified here.

### Documentation

- **The JavaScript side had no API reference — it had a listing.** Looking for
    `gobj_post_event()` in the docs found nothing, and the function had been in
    the JS runtime for releases. It *was* on the site, as one line inside a code
    block on `api/js/events.md`, with no signature and **no anchor**: the JS
    pages carried zero `(name)=` labels and the appendix index was C-only
    (2540 C functions, 0 JS symbols), so no JS name was reachable by search.
    The whole JS reference was 790 lines against 31 608 for C, written once in
    April and touched since only by the STE sweep and a link fix.

    The one line documenting `gobj_post_event()` was also **wrong**: it said
    *"next microtask"*, and the queue has drained with `setTimeout(…, 0)` — a
    macrotask, so the browser keeps its turns — since the JS side aligned with
    the C contract in gobj-js 7.10.0. `gobj_deliver_posted_events()`,
    `gobj_posted_events_size()`, the 10 000-message ceiling and the
    purge-on-destroy were all undocumented.

    Both packages now have a real reference, every symbol anchored and linked to
    its source: **gobj-js 255/255** and the **gobj-ui public surface 92/92**.
    New pages for what was missing entirely — the whole trace API
    (`gobj_set_gclass_trace()` and the silencing side, which is the tool the
    project calls an axiom for debugging), the `SDATACM`/`SDATAPM`/`SDATAAUTHZ`
    command descriptors, the `kw`/`kwid`/`msg_iev` families, the DOM and i18n
    helpers, the `jdb` database — and a first **gobj-ui section**: the shell
    API, the dialogs, the component gclasses, the period algebra, the theme and
    the dev panel.

- **`scripts/verify_js_api_coverage.py`** — the gate that stops this from
    happening again, the JS half of `verify_api_coverage.py`. It reads every
    export of both submodule packages, compares them against the `(js_<name>)=`
    anchors, and reports MISSING and STALE. `--write` generates
    `api/appendix_js_api_index.md`, which lists **all 439 symbols** with their
    signature, module and a source link — so a search finds a symbol whether or
    not it has a reference entry yet. The links are pinned to each submodule's
    **own tag**, read from its `package.json`: gobj-js is at 7.10.0 and gobj-ui
    at 5.11.0, and pinning either to the SDK tag points at a tag that repository
    does not have. The script warns when a submodule HEAD is not its tag,
    because then every `#L` anchor is a guess.

### Added

- **The `__system__` treedb holds a schema again.** Every `C_TREEDB` service
    builds a `__system__` treedb whose topics (`treedbs` → `topics` → `cols`)
    exist to hold a treedb schema **as data** — the V6 design that makes a
    schema listable and editable at runtime through the ordinary node commands.
    It has been created empty on every start and never filled: each of the 13
    stores on the dev machine had `treedbs/keys`, `topics/keys` and `cols/keys`
    with zero records, and nothing could have filled them, for two reasons in
    the meta-schema itself.

    Its `cols` topic had **lost the `value` column** while keeping
    `'pkey2s': 'value'`, a secondary index on a column that no longer existed;
    and `cols.id` had lost its **`rowid`** flag, so `treedb_create_node()`
    refused every column with *"Field 'id' required"*. Both were collateral of
    merging two descriptors that are not the same object: the **storage** schema
    of a column (keyed by rowid, name in `value`, because a column name is
    unique only inside its topic) and the **validator** for a user column (keyed
    by name). The diagram at the top of the file still documented the original
    shape — `id (rowid)`, `* value (2)` — which is what it was checked against.

    Both are restored, `_treedb_create_topic_cols_desc()` now *derives* the
    validator from the storage schema (renames `value` → `id`, drops the
    storage-only fields) instead of copying it, and `topics` gained
    `system_topic` so the flag survives the round trip. System schema 7 → 8.
    Since the three topics were empty everywhere, the regeneration the bump
    forces has nothing to lose.

    `open-treedb` projects a treedb's schema into `__system__` the first time it
    sees it, under the default `use_internal_schema=1` too — the C literal stays
    the source of truth, and the schema becomes visible. With
    `use_internal_schema=0` the projection is what the treedb opens with; the
    dead `return` that shadowed that path in `get_client_treedb_schema()` (it
    was in V6 as well) is gone.

    The `treedbs` node carries **two** version numbers, because one counter
    cannot serve two writers: `schema_version` is what the schema is worth to
    `treedb_open_db` and whoever edits the schema raises it, while
    `c_schema_version` records which literal the projection came from and only
    the projection writes it. Reconciliation compares against the second. With a
    single counter, the first edit made here — which has to raise the version to
    reach the treedb at all — would silently outrank every later release of the
    literal. A re-projection publishes under `max(stored, literal) + 1`, or the
    persisted schema file, sitting at the edited number, would keep masking it.
    System schema 8 → 9.

    Afterwards the two homes reconcile by **`schema_version`, strictly newer
    wins** — the same rule `treedb_open_db` already applies between that schema
    and the persisted schema file, so an edit made in `__system__` survives
    every start until a higher version arrives from C. Reconciling is an
    **upsert; nothing is ever deleted**. A column's `id` is a rowid handed out
    from the topic size, so re-creating columns renumbers all of them and can
    hand a retired number to a different column — and a delete is the one
    destructive primitive of the store: it drops the schema's own history, which
    is the reason to keep a schema in a treedb at all, and it refuses a
    snapshot-tagged node, so re-projection would fail outright on any store that
    has ever been snapped. An update appends a new version instead, and what a
    column used to declare stays readable with `instances`.

    And a write to those topics is a schema change, so it answers to the rules
    of a schema: a column is checked against the descriptor a user column
    answers to (the one `parse_schema_cols()` applies at open, so the failure
    lands on the writer instead of on the next open); `pkey` must be `id` and
    `system_flag` `sf_string_key`, which `treedb_open_db()` otherwise answers by
    silently dropping the topic; `pkey`/`tkey`/`system_flag` cannot change once
    the topic exists, because `topic_desc.json` is never rewritten and the
    change would be stored, shown by every reader, and ignored by the topic for
    good; and two columns with the same name in one topic are refused **when
    the column is linked**, which is when the clash becomes real.

    New test `tests/c/c_treedb_system_schema` covers the three steps: project;
    delete the schema file and re-open, and the topics and columns that come
    back are the ones that went in; then move the schema forward and check the
    projection updates while the existing columns keep their rowid. Docs:
    `YUNO_TREEDB.md` §3.11.

- **gobj-ui submodule 5.11.1 → 5.12.0: `C_YUI_TREEDB_TOPIC_WITH_FORM` shows the
    topic's schema.** A toolbar button (`with_schema_button`, on by default)
    opens that topic's `desc` — pkey, cols, types, flags and fkey targets — in
    the standardized adaptive dialog, on the lazy JSON viewer. The table shows
    the data; nothing showed the contract the data answers to, which is what
    you need in front of you when a value is refused or a link does not appear,
    and reaching it meant reading the backend's schema by hand.

    `register_c_yui_json()` became **idempotent** along the way, the courtesy
    `register_c_yui_form()` already had: the topic view auto-registers the JSON
    viewer for its dialog, and every consumer registers it explicitly *after*
    the topic view — an order that would otherwise trip *"GClass ALREADY
    created"* on boot.

    `yunos/js` takes it in `gui_treedb`, built and deployed. The JS API
    reference is repinned to the new tag with
    `scripts/verify_js_api_coverage.py --repin` (105 links, 2 line anchors) and
    its appendix index regenerated.

- **gobj-ui submodule 5.12.0 → 5.13.0: the JSON of a treedb cell is one click
    away.** A col that holds a JSON document — `dict`, `list`, `object`,
    `array`, `blob`, `template`, `coordinates`, `gbuffer` — has only ever shown
    the first 20 characters of `JSON.stringify()` in its cell, which for the
    fields carrying the actual configuration of a node is a preview of the
    opening brace. Reading the value meant opening the edit form: edition mode,
    a raw text editor, and a dialog whose purpose is to change the record.

    Clicking the cell now opens the whole value in the same adaptive dialog the
    schema button uses, on a hosted `C_YUI_JSON` — read-only, collapsed and
    searchable. The record is already in the table, so the dialog issues no
    command and touches no backend. The click crosses the machine
    (`EV_SHOW_CELL_JSON {row_id, col_id}`) like every other action of this view,
    and the kw carries the **identity** of the cell and never its value: the
    trace dumps the kw. A cell whose document is empty gets no link, and that
    absence is what makes the click a no-op.

    The preview is built as a DOM node (`JSON_CELL` / `JSON_CELL_ICON` /
    `JSON_CELL_PREVIEW`) instead of the bare string the formatter used to
    return, so record data is no longer parsed as markup on its way into the
    cell.

    Every v2 consumer took it and is deployed: `gui_treedb`
    (artgins.ytreedb.com), wattyzer (app.wattyzer.com, which was two releases
    behind and also gains the schema button), both yunovatios GUIs, and
    `gui_agent` on both control-center planes — that one mounts no treedb
    table, so what it takes is 5.12.0's idempotent `register_c_yui_json()`. The
    two v1 apps, estadodelaire and hidraulia, stay on the frozen `1.0.1` line
    by design.

    Three of those apps had to **define the keys the library now asks of their
    own i18next**, which their prebuild validators caught: `show json`
    everywhere, plus the JSON viewer's `too many rows; collapse some branches`
    in wattyzer, reachable there since the viewer entered the treedb stack. An
    undefined key renders as the key itself and never changes language — the
    failure that is invisible by construction.

### Fixed

- **`use_internal_schema` is gone: a treedb opens from its projection,
    always.** The flag chose between the C literal and the `__system__`
    projection, defaulted to 1, and was read-only in most yunos — so an edit
    made in `__system__` reached nothing anywhere until every yuno's config was
    changed one by one, which is the difference between a feature and a demo.
    It distinguishes nothing now: the projection is seeded from the literal and
    re-made whenever the literal **or the projector** moves ahead, so opening
    from it *is* opening from the literal until somebody edits it. The literal
    stays as the fallback for a projection that cannot be rebuilt into a valid
    schema. Removed from `C_TREEDB` and from the yunos of this tree; the project
    yunos that still pass it keep working untouched, because `command_parser`
    merges an unknown key and nobody reads it.

    Opening every treedb from its projection surfaced one more thing the flag
    had been hiding, caught by the mqtt ACL test: a column with **no**
    `default` is stored with an empty one, because the attribute is a blob and
    a blob with no value is `{}`. Left in the rebuilt schema that empty dict is
    a real default, and creating a record without that field handed `{}` to a
    string column — *"Value must be string"*, from a schema nobody wrote that
    way. It is dropped now, but only for a column that cannot hold a container:
    `[]` is a legitimate default for an array column, and mqtt_broker's
    `publish_acl`/`subscribe_acl` declare exactly that.

- **Two treedbs of one store could not both hold a schema.** `__system__` keyed
    its `topics` topic by the **bare topic name**, and a topic name is unique
    only inside its treedb: `users` is a topic of `authzs`, `mqtt_broker` and
    `controlcenter` alike. The second one to be projected got *"Node already
    exists"*, its topic node was never created, its hooks then failed with
    *"link topic not found"*, its rebuilt schema failed `parse_schema`, and
    `get_client_treedb_schema` fell back to the C literal — in silence. The
    local controlador's store holds five treedbs and its projection held one.

    `topics` is keyed by rowid now, with the name in the pkey2 `value`, exactly
    as `cols` has been since V6 — the same flaw, solved for columns and never
    for topics. The sweep that found it now holds all four schemas in **one**
    store, which is what used to break. Addressing a topic costs what
    addressing a column costs: its rowid, not its name. System schema 12 → 13.

- **A change to a schema now publishes itself.** Raising `topic_version` and
    `schema_version` is what makes a change visible — forget either and the
    change does nothing and says nothing, because `treedb_open_db` keeps the
    persisted schema file on a tie and tranger2 keeps `topic_cols.json` unless
    the incoming version is higher. Leaving that to whoever writes means every
    editor, script and console carries the rule; the author of this code got it
    wrong three times in a row while debugging, knowing it. So writing a `cols`
    or `topics` node of `__system__` raises the versions that publish it,
    walking up the fkeys to the column's topic and its treedb. A new column
    publishes when it is **linked** to its topic, which is when it becomes part
    of the schema — at create time it has no topic yet. The projector sets the
    versions itself and marks the tranger while it works, which is also what
    stops the rule from answering its own writes.

- **The projection of a schema was losing three column attributes, and a
    scalar `default`.** Measured across the 19 schemas in the tree and the
    project repos: `enum` (18 uses) and `template` (6) had no column in the
    meta-schema at all, and `pkey2s` had one that the projector never filled.
    The `enum` loss is the worst of the three because it is silent *and*
    disarming: the column keeps its `enum` **flag** while its enumeration
    disappears, so it declares an enumeration it no longer has and every value
    passes. On top of that, a column's `default` is declared `blob`, and a blob
    replaced anything that was not an array or an object with `{}` — so
    `'default': 'es'` was stored as an empty dict. A blob that discards a
    scalar is not a blob; it keeps what it is given now, on both the write and
    the read back.

    The cause of the missing attributes was a **list written by hand** in the
    projector: adding an attribute to the descriptor did not add it there. It
    now copies whatever the descriptor declares, and the test asserts fidelity
    the same way — reading the attribute list from the descriptor instead of
    repeating it — so the next attribute that gains no storage fails a test
    rather than changing every schema quietly.

    Fixing the projector was not enough to fix the stores it had already
    written: reconciliation compared only the C literal, so a projection made
    by an older SDK stayed frozen — and an older SDK is exactly the one whose
    projection is missing what it did not know how to store. The `treedbs` node
    now also records `system_schema_version`, the meta-schema version that
    produced it, and a projection made by an older one is re-made on the next
    start. Raising the meta-schema's own version is the migration lever, and it
    moves when the projector changes even if no field does — 11 → 12 was
    exactly that, because 11 could write a `topic_version` **lower** than the
    one already published, leaving a re-projection that fixed the columns in
    `__system__` and never reached the topic. A topic's version cannot go
    backwards now, for the same reason `schema_version` cannot.

    Verified end to end on a real schema: the local yunovatios controlador,
    degraded by the lossy projector, recovered `language`'s `["es","en"]` and
    its `"es"` default, and `cluster`'s `false` — a scalar boolean default that
    a blob used to replace with `{}`. System schema 9 → 12.

- **`close-treedb` on a playing yuno was a use-after-free anyone could
    trigger.** It destroys the treedb's `C_NODE` and its `C_TRANGER`, and an
    owner keeps raw handles no framework cleanup can reach: the service pointer,
    the `tranger` json_t read from it, copies of both on a hot path, and
    whatever else was opened on that same tranger — `db_history_co` opens its
    `msg2db_alarms` there. Freed underneath, the next record processed writes
    into released memory. Every in-tree consumer closes only from `mt_pause`, so
    the command now refuses while the yuno plays and points at the pair that
    does this correctly, `pause-yuno` + `play-yuno` (which reopens through the
    owner's own lifecycle, without restarting the process). `force=1` remains
    for a caller that opened the treedb and holds nothing of it.

- **A treedb `update-node` validated nothing, and answered success when it
    failed.** `treedb_update_node()` set each incoming field straight onto the
    node — no type, no `notnull`, no `enum` — so a column could end up holding
    what its own schema forbids, and `mt_update_node()` then dropped the return
    value and answered the collapsed view of the unchanged node, so a refusal
    read as a success all the way to the client. Updates now run the same
    normalization as creates, validate **every** field before touching the node
    (a refusal leaves nothing half-applied), and the failure reaches
    `cmd_update_node`, which already knew how to report it.

- **`enum` was decorative on writes.** A column's `enum` list was checked only
    when a *schema* was parsed (`check_desc_field`), never when a *node* was
    written: `normalize_node_field_value()`'s `enum` case only looked at whether
    the container was a string or an array. So the promise did not survive the
    first write, on any treedb. Checked now on both paths, for the supplied
    value only — an absent optional field is stored as `""` or `[]`, which no
    enum has to list. Audited before shipping: 122 enum columns across the local
    stores, 0 values outside their list.

- **`C_NODE`'s `mt_node_tree` released its options before reading them.**
    `JSON_DECREF(jn_options)` also nulls the variable, and the
    `with_metadata` lookup came two lines later — so the option never took
    effect, and every call logged *"kw must be list or dict"*. Reading a node
    tree with metadata was impossible, silently.

- `get_treedb_schema()` asked `gobj_node_tree()` whether a treedb had a schema
    in `__system__` yet, and a first open — the ordinary answer — logged *"Node
    not found"* as an error. It asks with a list now, which is silent when
    empty.

- Landing page: carded [yunomusica.com](https://yunomusica.com) as the second
    live example — a shipped SPA on `C_YUI_SHELL` + `C_YUI_NAV`, installable and
    offline, next to the shell demo that exists to show the shell.

- **gobj-ui submodule 5.11.0 → 5.11.1**, which exports
    `yui_shell_confirm_danger()` from the package root. `confirm_ok`,
    `confirm_yesno` and `confirm_yesnocancel` were all in the barrel and the
    destructive one was not, so the dialog the library's own comment tells you
    to use for *"this deletes an account"* — red button, safe answer last — was
    the single one that `import { … } from "@yuneta/gobj-ui"` resolved to
    `undefined`. Found while writing the gobj-ui reference above. A missing
    export fails at the call and not at the import, so it read as a bug in the
    caller, and the only consumer had reached for the deep import and moved on.
    Deep imports keep working, so nothing breaks.

- **Both JS packages track their `package-lock.json` now.** Each one ignored
    it, so a build of either was reproducible only by accident: every machine
    resolved its own tree from the ranges in `package.json`, and the tree behind
    a published `dist/` was whatever that disk held that day. Nothing recorded
    it. The lockfile is never published — npm keeps it out of the tarball, and
    it is in neither package's `files` — so this changes what a developer and a
    CI run get, not what a consumer gets. Both verified with a real `npm ci`
    from the committed lock in a clean directory: 76 packages for gobj-js, 251
    for gobj-ui.

    The staleness warning of `verify_js_api_coverage.py` had to change with it.
    It compared the submodule HEAD against its tag, so a lockfile commit made it
    fire for ever — and a warning that everybody learns to ignore is worse than
    no warning. It now diffs the **documented files** (`src/`, `index.js`)
    between the tag and the **working tree**, which is both quieter and
    stricter: a commit that cannot move a line number says nothing, and an
    uncommitted edit that can move one — which the old check missed entirely,
    because it compared two commits — now reports.

- **Every JS consumer refreshed, by a rule instead of by hand.** Eleven
    packages across nine repos moved their floors to `^5.11.1` for gobj-ui and
    `^7.10.0` for gobj-js, plus maplibre-gl `^6.3.0`, vite `^8.2.1` and vitest
    `^4.1.10` where they applied. The rule was to raise each floor to what npm
    calls **wanted** — the newest version the existing range already accepted —
    and never to **latest**, so a major cannot enter through a sweep.

    That rule is what protects the two v1 apps: estadodelaire and hidraulia keep
    gobj-ui at `1.0.1` (the frozen line), maplibre-gl at `5.24.0` (v6 is ESM-only
    and needs the worker wired) and vanilla-jsoneditor at `0.23.8`. All three
    are majors, and a major is a migration to plan, not a number to raise.

    The peer floors of gobj-ui are untouched for the mirror-image reason: a peer
    floor is a contract with every consumer, and `^6.1.0` already accepts
    maplibre 6.3.0. Raising it would force every consumer up and cost a
    republish, for nothing.

    Verified by build, not by the number in the file: all eleven install, the
    nine with a build script build, and the two libraries pass 39 and 256 tests.
    Worth knowing — **estadodelaire and hidraulia declare vitest and ship no test
    file**, so the build is the only gate their bump passed, and they are the two
    that cost the most to repair.

- **`--repin` for the JS docs.** A submodule bump moves a tag, and every
    hand-written `blob/<tag>/` link into that repository goes stale with it.
    `check_doc_line_refs.py --repin` cannot help — it matches
    `github.com/artgins/yunetas/blob/` only — so the 5.11.1 bump would have left
    104 links pointing at 5.11.0 with the pages looking correct. The JS script
    now retags them and recomputes each `#L` anchor from the symbol that the
    entry's own `(js_<name>)=` label names, which is more reliable than the link
    text: a heading is free to read `## C_TIMER` while the symbol is
    `register_c_timer()`.


- **The agent dropped every counter-driven answer of a client behind a
    controlcenter.** `kill-yuno`, `run-yuno`, `play-yuno` and `pause-yuno` do
    not answer when the command is parsed: they answer when it is DONE — the
    agent raises a `C_COUNTER` that waits for the killed yuno's channel to
    close, or for the launched one to connect back, and `ac_final_count()`
    sends the answer to the requester. It looked for that requester with
    `gobj_child_by_name(__input_side__, …)` only, and a command cascaded from a
    controlcenter arrives through the OUTBOUND `controlcenter` C_IEVENT_CLI —
    a top-level service, not an input-side child. So the lookup missed, the
    answer was dropped with *"requester channel child not found"* in the
    agent's log, and the caller waited forever for work that had actually been
    done.

    This is the same defect `ac_command_yuno_answer` / `ac_stats_yuno_answer`
    were fixed for in `d2d833109` (7.11.0); the deferred path was missed
    because it is reached only by the four lifecycle commands, and only from a
    SPA — a local `ycommand` requester ("input-N") IS an input-side child, so
    every CLI test passed. Same fallback here:
    `gobj_find_service(requester, FALSE)`.

    Found while giving gui_agent's Schemas workspace its Apply button, whose
    whole job is `kill-yuno` → `run-yuno play=0` → `play-yuno` chained on those
    answers. **Nodes need the new agent binary for it to work there.**

- **gobj-ui submodule 5.15.0 → 5.16.0, and the JS yunos with it: the treedb
    GRAPH learns read-only and reports its writes.** The whole write surface of
    `C_YUI_TREEDB_GRAPH` hangs off ONE operation mode — `edition` is the only
    one that draws the create / delete / link affordances — so `readonly` drops
    it from the mode select, and because the mode is a PERSISTED preference, a
    graph left in edition on a master comes back in `reading` on a replica. The
    five write events are refused as well: the G6 child raises them from its
    undo/redo history and from saving the node geometry, neither of which goes
    through the toolbar.

    It also publishes `EV_RECORD_WRITTEN`, the event the topics editor has had
    since 5.14.0, because a host that edits a SCHEMA is not finished when the
    record is written — the yuno still has to be restarted to re-read it.
    `__graphs__` is excluded on purpose: the graph writes that topic itself on
    every layout save, and reporting it would say the schema changed because
    somebody dragged a node.

    And a third thing, which only a SECOND view on the same treedb could
    surface: an updated node the table never loaded threw an unhandled
    rejection. Tabulator's `updateData()` rejects on a row it cannot find and
    nobody awaits it, so it arrived as a bare *"Update Error - Unable to find
    row"* naming neither gclass nor topic.

- **`yunos/js`: gui_treedb's Connections moved from the account menu to the
    rail.** The backends are where a session starts and both work entries depend
    on them, so they lead the rail — Connections / Topics / Graphs. The item
    declares a route and no target: `/connections` is already in `shell.routes`
    and `build_item_index()` fills it from there, so one place says which gclass
    the backends page is. The workspace picker (tab 0 of Topics/Graphs) is
    renamed **Select** in the same move, path included
    (`/<ws>/connections` → `/<ws>/select`): two different pages had carried the
    name *Connections*, and it only showed once both were on screen at once.
    Detail in that submodule's own `CHANGELOG.md`.

- **`yunos/js`: gui_agent reaches the AGENT's own treedbs.** The agent never
    appears in `list-yunos` — it is the daemon that answers it — so its treedbs
    (`treedb_yuneta_agent`, `treedb_system_schema`, `treedb_authzs`) were
    unreachable from the console. The Schemas picker offers it now under the
    sentinel yuno id `__agent__`, addressed with the agent's own `command-agent
    service=<treedb>`; on the `.ovh` plane the same row is **agent22**. Apply is
    disabled there — the agent is not a managed yuno and is restarted on the
    node. Detail in that submodule's own `CHANGELOG.md`.

- **`yunos/js`: a gui_agent tab remembers where you were inside it.** Open a
    topic in a Schemas tab, look at another tab, come back — and you landed on
    the treedb cards with the topic gone: a tab's nav item carries a FIXED
    route (its base), so clicking it always navigated to the root.

    The route cannot simply be made deeper — `yui_shell_set_submenu()`
    registers `item.route` in the shell's item index, so it is where the tab's
    view is MOUNTED and what a deep link resolves to. `C_APP` remembers the
    last position per tab and replays it when the tab is entered again, which
    it tells apart from walking UP inside the tab (the view's own *Topics*
    button) by looking at whether the previous route belonged to another tab.
    Detail in that submodule's own `CHANGELOG.md`.

- **`yunos/js`: gui_agent's Schemas workspace draws the treedb as a graph.**
    Under a treedb the subpath was `<topic>[/info]` or `schema`; it is now also
    `graph[/<topic>]`, and the third icon of every topic card goes there — the
    treedb's viewer hosts the graph beside the topic editor, on the SAME routing
    adapter, and swaps the two bodies by url. Mounted lazily (G6 is the heaviest
    thing the workspace draws, and it measures its canvas when first SHOWN), and
    opened with the `dagre` layout rather than the library's `manual`, which
    places nodes where the records say they are — a schema treedb carries no
    geometry, so it opened as one diagonal pile of 265 `cols`.

    The adapter had to learn the treedb LINK events: the graph subscribes to
    `EV_TREEDB_NODE_LINKED`/`UNLINKED` on its transport as soon as it loads a
    topic, and `gobj_subscribe_event` refuses an event that is not in the
    publisher's output list — so leaving them undeclared did not cost an edge
    that fails to redraw, it cost the SUBSCRIPTION. `gui_treedb` declares
    `EV_RECORD_WRITTEN`, which had been an FSM error on every write since
    gobj-ui 5.14.0. Detail in that submodule's own `CHANGELOG.md`.

- **gobj-ui submodule 5.13.0 → 5.14.1, and the JS yunos with it.** `5.14.0`
    publishes `EV_RECORD_WRITTEN` from `C_YUI_TREEDB_TOPICS`: the view refreshes
    itself from the treedb's own node events, which arrive for every writer and
    therefore say nothing a host can act on — a schema editor, where changing a
    column also has to raise the versions that publish it, cannot use them
    without answering its own writes in a loop. `5.14.1` teaches the treedb
    table the `rowid` type: a topic keyed by rowid — which is how the
    `__system__` treedb has stored a schema since this release — logged
    *"unhandled type 'rowid'"* once per cell over every render of `topics` and
    `cols`, the two topics a schema editor exists to edit.

- **`yunos/js`: gui_agent grows a Schemas workspace** — gobj-ui's treedb editor,
    unchanged, over a routing adapter that re-wraps each command as
    `command-agent` + `cmd2agent="command-yuno …"`, so one control-center
    session reaches the `treedb_system_schema` of any yuno of any node. It
    discovers which treedbs a yuno exposes, marks in the picker the yunos with
    none, carries `<treedb>/<topic>` in the URL, and applies a schema by
    restarting the owning yuno. Detail in that submodule's own `CHANGELOG.md`.

- **`yunos/js`: the two SPAs give their primary rail back to the work.** In
    `gui_agent` the rail is the four workspaces (Commands, Statistics,
    Terminal, Schemas) and the settings page moved to `/preferences` under the
    toolbar avatar; in `gui_treedb` the rail is Topics and Graphs, and what
    `/settings` held became two pages with their own names — `/connections`
    (the backends, where each workspace picker sends you) and `/preferences`
    (the live buffer). All of them stay ROUTES, so they are linkable, survive
    an F5 and appear in the site map.

    On the way, gui_agent's Schemas tab stopped routing its own url by hand:
    the treedbs of a yuno are now a **tree of nodes** (`C_YUI_NODE` at the
    tab's route, one `link` child per treedb), which is what makes the shape of
    the navigation a user choice — **Preferences → Navigation**: stacked
    strips, back to parent, or breadcrumb, applied live to the open tabs.
    Detail in that submodule's own `CHANGELOG.md`.


## 7.12.0-2

A packaging revision, not a new version: the tree under `kernel/`, `modules/`,
`utils/` and `yunos/` is the same one 7.12.0 was cut from. The packages are
rebuilt as `yuneta-agent-7.12.0-2` and attached to the existing 7.12.0 tag.

### Fixed

- **Nothing ever restarted the SECOND agent, on either distro.** An upgrade
    replaces both agent binaries, and only the main one is ever bounced: the
    init script's `stop_yunos()` stops just that one (start brings up both,
    stop takes down one), and no package scriptlet touches `yuneta_agent22` at
    all. *"Do not bounce both at once"* had quietly become *"never bounce the
    second one"* — it kept running its old inode for as long as the node stayed
    up. Found on **all five nodes at once**, four of them five days deep, and
    the code the spare was running was the version-comparison bug this release
    exists to fix. The escape hatch was the oldest thing on the node, which is
    the exact opposite of what a second agent is for.

    `install.sh` now refreshes it — but only **after** confirming the main
    agent is up *and* running the binary that was just installed. (That script
    is fetched from `main`, not shipped in the package, so it takes effect
    without a package revision; the `%pre` change below is what the `-2`
    packages carry.) If the main
    agent is unhealthy, the spare is the only way into the node and is left
    strictly alone, with the manual command printed. A spare that does not come
    back is reported without failing the install, because the node still has
    its main agent.

    The staleness test is the process, not the file: `readlink /proc/<pid>/exe`
    ending in `" (deleted)"`. `rpm -q`, `--version` and the installer's own
    sign-off all read the file, and all three said everything was fine.

- **An `.rpm` upgrade left the OLD agent running, and every check said it had
    worked.** dpkg stops the agent in `prerm` and starts it again in
    `postinst`, so a `.deb` upgrade lands on the new binary. rpm has no
    equivalent hook: `%preun` stops only on **uninstall** (`$1 == 0`), and
    `%post`'s `service yuneta_agent start` is a no-op against a process that is
    already running. So the new binary went to disk and the old process kept
    running on the unlinked inode.

    Nothing reported it. `rpm -q` said `7.12.0-1`, `yuneta_agent --version`
    said `7.12.0`, and the installer signed off with *"yuneta_agent and
    yuneta_agent22 are running"* — all three read the **file**. Only
    `readlink /proc/<pid>/exe`, ending in `" (deleted)"`, read the process.
    This is how 7.12.0 reached a node installed and not running: the release
    whose whole point was a fix in the agent.

    `%pre` now stops the main agent on an upgrade (`$1 == 2`), mirroring
    `prerm`. **Only the main one** — `yuneta_agent22` stays up, the same
    asymmetry the init script's `stop_yunos()` already has (start brings up
    both, stop takes down one): the second agent exists so each can recover the
    other, and an upgrade that bounced both at once would give that up for the
    seconds it matters most. The yunos are untouched — they outlive their agent
    and the new one adopts them back over the control channel, measured across
    five nodes as the same yunos with the same PIDs.

## 7.12.0

A minor for what was found, not for what was added.

The agent had been **demoting** yunos: a version comparison packed its segments
into an `int`, `1.9.0.0-2` overflowed into a negative number, and every
`deactivate-snap` re-appended the OLDER release as the primary — for eleven
days on a client node, with nothing in any log to say so. The comparison was
both the decision and the only guard, so when it lied there was nothing left to
notice. It is `version_cmp()` in the SDK now, comparing segment by segment,
tested against that node's real version chain; and the direction is checked on
its own, logged either way, and a release that does not move forward needs
`force=1`.

Two more of the same shape: a msg2db accepted records it could never load back
(4447 of them on that node, and 4447 log lines at every start), and the package
would unpack another machine's build over a node that compiles its own —
foreign glibc archives into `outputs/`, which a static link takes in silence and
the heap pays for at run time. Both refuse now, at the point where the mistake
is made.

### Fixed

- **The agent promoted the OLD binary, and kept doing it.** `get_n_v()`
    weighed each segment of a version by 1000 and accumulated into an `int`.
    A four-segment release with its revision — `1.9.0.0-2` — needs 10¹², so it
    overflowed and came out **negative**:

    ```
    1.7.1.0-2  ->   1,978,652,738
    1.9.0.0-2  ->    -317,314,558
    ```

    `promote_highest_release_yunos()` compares with that, so it read the older
    release as the newer one and **re-appended it as the primary**. Not a
    failure to promote: an active demotion, repeated at every restart. The
    store on a client node showed it plainly — `1.9.0.0` written at 13:46:25,
    `1.7.1.0` written back **two seconds later**, and again on 01/08, 04/08,
    05/08, 09/08 and 10/08. Eleven days of `deactivate-snap` bringing back the
    binary it was supposed to replace, with nothing in any log to say so,
    because a comparison that comes out negative says nothing.

    The function moved to the SDK as **`version_cmp()`** — it is a string
    utility, not agent business, and a `PRIVATE` in a gclass cannot be tested.
    It compares **segment by segment** and packs nothing into a number, which
    is what the JS (`version_tuple`) and Python sides of this project already
    did; the C side was the only one that accumulated, and the only one that
    broke. A wider accumulator would have moved the ceiling without removing
    it: weighing segments by 1000 also assumes every segment stays under 1000,
    so `1.2000.0` outranked `2.0.0`, and it right-aligned the segments, so
    `7.11` and `7.11.0` were **different versions**. Neither survives the
    rewrite. All 8 comparisons in `c_agent.c` go through it.

    Covered by `tests/c/helpers`, with that node's real version chain. The
    test was checked against the old implementation first — it fails **six**
    times there: the exact pair that cost the eleven days, the `1.2000.0` /
    `2.0.0` inversion, and the `7.11` = `7.11.0` equality.

- **A release that does not move forward is refused, and says so.** The
    comparison that picked a candidate was the only thing standing between a
    deploy and a downgrade, so when it was wrong there was nothing left to
    notice — eleven days of demotions and not one line about it.

    `cmd_find_new_yunos()` now checks the direction on its own and logs it
    **either way**: an info line naming `from` → `to` when a release moves
    forward, a **warning** when it does not, and it refuses to take it unless
    the caller passes `force=1`. `promote_highest_release_yunos()` gained the
    matching guard: a promotion that would go backwards cannot happen by
    construction, so if it ever does it is an **error**, not a silent
    `gobj_update_node()`.

### Changed

- **The package refuses a node that builds from source.** It carries libraries,
    headers and binaries built on another machine, and it lays them into the very
    tree `yunetas build` owns — `outputs/` and `outputs_ext/`. From then on fresh
    objects link against archives built for a **different glibc**: a dynamic link
    fails loudly, a **static** one resolves them silently and the binary corrupts
    its heap at run time. This is not hypothetical — it cost a client node a
    week, and it is why `libc_guard.cmake` exists.

    `preinst` / `%pre` now abort when `/yuneta/development/yunetas` holds
    `kernel/` and `.git`, and `install.sh` checks first so the refusal is not
    buried under apt's own error plus two failed fallbacks. Installing is still
    supported — it just has to be a decision:

    ```bash
    sudo YUNETAS_FORCE_OVER_SOURCE=1 apt install ./<package>.deb
    sudo touch /etc/yuneta/allow-package-over-source   # or make it stick
    ```

    The variable goes **after** `sudo`, which resets the environment; the marker
    file is there because that footgun is not obvious. Forcing prints what has
    to happen next: **rebuild everything, external libraries FIRST**. Rebuilding
    only the SDK leaves the packaged externals in place, which is the same
    mismatch by a shorter road.



- **A msg2db says once what it used to say thousands of times.** Every record
    whose `pkey2` value is empty is dropped at load, and each one wrote its own
    error line: a client node was printing **4447 identical lines at every
    start**, which buries everything else that log exists for.

    Now the first dropped record is logged **whole**, so it can still be
    diagnosed, and `msg2db_open_db()` reports the count when the topic finishes
    loading. Two lines instead of thousands, and neither of them silent — the
    number of records that did not load is stated, which is the part that
    matters.

### Fixed

- **BREAKING: a msg2db refuses a message whose `pkey2` value is empty.** The
    write side only asked whether the key was present, while the load side
    asks for a non-empty value — so a record carrying `"alarm": ""` was
    accepted, written to disk, and then dropped at every load for the life of
    the store. A client node was carrying 4447 of them, written across a
    fortnight and unreadable ever since.

    A store must not take what it cannot give back. The refusal belongs where
    the caller still holds the record and can be told, not in somebody else's
    log a restart later.

    ⚠️ **An app that writes those records will now see them refused**, with an
    error naming the record. That is the intended consequence: the mistake
    surfaces where it is made. Records already on disk are untouched, and the
    load side keeps its guard and its reporting for them.

- **`tr_msg2db.c` no longer keeps a file-static `json_t *` cache.**
    `topic_cols_desc` was a `PRIVATE json_t *` kept alive across open/close
    with an incref dance — the pattern CLAUDE.md forbids for library helpers,
    and the same variable that `tr_treedb.c` is cited as the fixed example of.
    Nothing outside `msg2db_open_db()` ever read it, so it is a local now,
    built per call and released before the function returns.

    ⚠️ This was **not** the leak it looked like: msg2db still leaks 8 tracked
    blocks per open/close cycle, measured and written down in `TODO.md`. The
    static was removed because it is forbidden, not because it was guilty.

## 7.11.0-3

A packaging revision, not a new version: the tree under `kernel/`, `modules/`,
`utils/` and `yunos/` is the same one 7.11.0 was cut from. The packages are
rebuilt as `yuneta-agent-7.11.0-3` and attached to the existing 7.11.0 tag.

One change, and it closes the difference the previous revision opened.

### Changed

- **`/etc/yuneta/webserver` is handled the same way in the `.deb` and the
    `.rpm`, and neither ships it.** 7.11.0-2 fixed the `.deb` by dropping the
    file from the package; the `.rpm` still shipped it as
    `%config(noreplace)`. Both worked, differently, which is the kind of
    asymmetry that is fine until the day it is not.

    They now follow the pattern this packaging already proved for
    `nginx.conf`: `preinst`/`%pre` saves the node's value to
    `/etc/yuneta/webserver.pkgsave`, and `postinst`/`%posttrans` puts it back
    if the transition removed it. A node that already chose keeps its choice;
    only a node that never had one gets the build default seeded.

    The save matters more than it looks: a file an upgrade no longer provides
    is a file the package manager removes, which is exactly how 7.9.1 deleted
    two nodes' `nginx.conf` while fixing the overwrite that preceded it.

## 7.11.0-2

A packaging revision, not a new version: the tree under `kernel/`, `modules/`,
`utils/` and `yunos/` is the same one 7.11.0 was cut from. The packages are
rebuilt as `yuneta-agent-7.11.0-2` and attached to the existing 7.11.0 tag.

Everything here comes from one install that failed on three nodes at once, and
from what that failure left behind.

### Fixed

- **A conffile question killed the install, and the install path cannot answer
    one.** `install.sh` is meant to be run as `curl ... | sudo sh`, which
    leaves dpkg with no stdin. It called apt with no `DEBIAN_FRONTEND` and no
    conffile policy, so the first question dpkg asked read EOF and took the
    whole install down — *"end of file on stdin at conffile prompt"* — with
    the package left half configured and re-running it hitting the same wall.

    It fired on three nodes because `/etc/logrotate.d/yuneta` had reached them
    by hand before it shipped in the package, so dpkg does not own it and asks
    what to do with it. `install.sh` now runs noninteractive with `confdef` +
    `confold`, and says afterwards when a file was kept. The `preinst` also
    adopts an unowned copy, so the question cannot come back.

    ⚠️ **The failure is worse than a failed install**, and that is the part to
    remember: dpkg had already unpacked and `prerm` had already stopped the
    agent and the web server. Three nodes were left with their web server
    down, their `nginx.conf` **deleted** — dpkg removes what the package no
    longer ships — and nothing to restart them. It was only recoverable
    because `preinst` had saved the configuration to `nginx.conf.pkgsave`.

- **The package no longer decides which web server a node runs.** It shipped
    `/etc/yuneta/webserver` with whatever the build machine chose, and the
    build machine has no opinion, so it shipped `nginx`. On 7.11.0 both
    openresty nodes came back serving from the wrong tree with a default
    configuration — one of them the company server, the other a client's.

    The file is node state, exactly like `nginx.conf`, which stopped being
    shipped in 7.9.1 for this same reason. It is not in the package any more;
    the scriptlet creates it only when it is absent.

- **The handover to the web server unit stops the old server for real.** It
    asked with `-s quit`, which is the graceful shutdown: it waits for the
    requests in flight, and on a node with websockets that wait does not end.
    Twenty seconds were not enough, the old master kept `:80` and `:443`, and
    the unit went into a restart loop that failed to bind 17 times. It now
    escalates to `-s stop`, and `yuneta-webserver` learned that verb.

- **`packages/deb/README.md` said a non-interactive install reboots the node.**
    It has not been true since the auto-reboot was removed. Reading that
    paragraph before installing on a client's node is a bad minute to have.

## 7.11.0

A minor for one reason: a public call changed its name and its signature, one
day after it got one.

7.10.0 added `gobj_post_message()` to C. The name was wrong and the shape was
wrong, and it took the compiler to say so — `gobj_post_event(dst, event, kw,
src)` already existed in `gobj-js` and in the ESP32 port, and the ESP32
component includes the Linux `gobj.h`, so the three-argument version did not
even build. Two implementations already agreed; C is the one that moved.
`gobj-js` then moved too, off a `setTimeout(…, 10)` and onto the same
contract.

The rest is the node's web server: it has its own systemd unit now instead of
being a side job of the init script, and on Rocky it needs a label to be
allowed to start at all.

### Fixed

- **The web server unit did not start on Rocky, and the node served nothing.**
    `/yuneta` is outside the SELinux policy, so everything under it is
    labelled `default_t`. The SysV script could exec that, because `initrc_t`
    may; systemd cannot. The unit shipped in 7.10.0-2 died with `203/EXEC`
    — *"Failed to locate executable /yuneta/bin/yuneta-webserver: Permission
    denied"* — retried four times, gave up, and `yunovatios-central` stayed
    two and a half hours with no web server at all.

    The `.rpm` now labels the wrapper `bin_t` (with `semanage fcontext` +
    `restorecon`, falling back to `chcon` where the policy tools are missing),
    and the scriptlet that already warned when the unit does not start now
    says where to look. Only the wrapper needs the label: it is the one file
    systemd execs, and what it execs in turn is reached from its own domain.

    Debian is untouched by this, which is exactly why it was not seen before
    shipping: the same revision on `yunovatios-controlador` came up fine.

### Changed

- **BREAKING: `gobj_post_message()` is now `gobj_post_event()`**, with
    `gobj_posted_events_size()` and `gobj_deliver_posted_events()` renamed to
    match.

    The call it adds to C already existed in JavaScript, and had for years:
    `gobj_post_event()` in `gobj-js`, under a comment that reads *"post_event,
    by now only in js"*. 7.10.0 shipped it into C under a third name for the
    same idea, which is the one thing a framework with two implementations
    cannot afford.

    The signature changed with the name, and the compiler is what said so:
    **the ESP32 port declares the same call too**, `gobj_post_event(dst, event,
    kw, src)` over an `esp_event` loop, and its component includes the Linux
    `gobj.h`, so the three-argument version did not build. Two implementations
    already agreed on four arguments, the same four as `gobj_send_event()` —
    because posting is sending, later. C is the one that had to move.

    So the destination is no longer always the caller. Lifetime is handled
    both ways instead: destroying the DESTINATION drops what it had pending,
    and destroying the SOURCE clears `src` and keeps the entry, because the
    destination still wants its event and gets it with `src == NULL`.

    The only caller is `webstats`, so nothing outside this tree breaks.

- **`gobj-js` aligned to the same contract** (package `7.10.0`). It deferred
    with `setTimeout(…, 10)` — one timer per event, and ten milliseconds
    standing in for "later", which is the very thing this call exists to stop
    writing. It now keeps a queue drained once per turn, a snapshot at a time,
    with the same lifetime rules, the same check at post time, the same
    ceiling and a trace line. Nothing called it, so the change breaks nobody.
    ESP32 still has its own contract, and what differs is in `TODO.md`.

## 7.10.0-2

A packaging revision, not a new version: the tree under `kernel/`, `modules/`,
`utils/` and `yunos/` is the same one 7.10.0 was cut from. The packages are
rebuilt as `yuneta-agent-7.10.0-2` and attached to the existing 7.10.0 tag.

Both changes are about the node's web server, and both came out of one daily
report that arrived empty.

### Fixed

- **The web server logs rotate to `.1` on every node, never to a date.**
    `/etc/logrotate.conf` on RHEL and Rocky carries a global `dateext`, so the
    same drop-in was giving `access.log.1` on Debian and `access.log-20260808`
    on Rocky.

    That is not cosmetic. The `webstats` yuno reads `<path>` and `<path>.1`:
    the day it reports is the day each LINE carries, and the previous day lives
    in the `.1` that `delaycompress` leaves uncompressed. With `dateext` there
    is no `.1`, so the yuno reads today's file alone and reports the previous
    day as empty, with nothing in any log to say why.

    Found by accident, which is the only reason it was found at all:
    `logrotate -f /etc/logrotate.d/yuneta` does not read `logrotate.conf`, so
    forcing a rotation by hand produced a `.1` and the difference showed
    itself.

### Changed

- **The node's web server is a systemd unit now, not a side job of the init
    script.** `/etc/init.d/yuneta_agent` ran nginx and let it daemonize, so
    nothing owned the process afterwards: a later `start` found nothing to look
    at and tried again, and the second master died with *"Address already in
    use"* while the first one kept serving.

    `yuneta-webserver.service` runs it with `daemon off`, so `$MAINPID` is the
    real master and stop and reload reach it. The unit calls
    `/yuneta/bin/yuneta-webserver`, which is the one place that reads
    `/etc/yuneta/webserver` to choose between nginx and openresty — that choice
    used to be repeated in three functions of the init script, and a unit file
    cannot branch on the content of a file. `systemctl reload` sends HUP, which
    reloads the configuration; reopening the log files is USR1 and stays in the
    `postrotate` of `/etc/logrotate.d/yuneta`, where logrotate does it itself.

    ⚠️ **The upgrade interrupts the node's web server briefly.** The unit
    cannot take `:80` and `:443` while the old daemonized master holds them, so
    the scriptlet asks that one to finish first and waits for it, then starts
    the unit and says so in the log if it did not come up.


## 7.10.0

A minor, not a patch: the framework gets a call it did not have.

`gobj_post_message()` names something every gclass already did and had no way
to say — *do this, but not on this stack*. Until now that was written as a
`C_TIMER0` of one millisecond, which is a time for something that is not a
time, and which cost the name of the event: every deferred continuation
arrived as `EV_TIMEOUT`, so the `machine` trace, which is the execution log of
a yuno, said "timeout" instead of what happened.

The call is not new to Yuneta. It existed in the first versions and was cut
when io_uring came in; what was missing was the wiring, not the design.

Nothing is removed and nothing changes shape: a gclass that does not call it
behaves exactly as before. `webstats` is the first consumer and the only yuno
that changed.

### Added

- **`gobj_post_message()`: an event a gobj sends to itself, delivered on the
    next cycle of the event loop.** It is the way for an action to leave the
    stack it is standing on — the usual case being a subscriber that must
    destroy or stop the publisher whose synchronous `gobj_publish_event()` is
    still on the stack below it.

    The call replaces the idiom of a `C_TIMER0` child armed with 1
    millisecond, which was never a time: it was "later", written as a
    duration. Saying it that way costs an io_uring timeout for something with
    nothing to wait for, needs a child gobj with its own start/stop, and
    throws away the name of the event — every deferred continuation arrived
    as `EV_TIMEOUT`, so the `machine` trace, which is the execution log of a
    yuno, said "timeout" instead of what happened, and a gclass with two
    deferrals had to tell them apart with a flag.

    The contract, in full in `gobj.h`: self-send only, posted from the thread
    of the event loop, `kw` owned, the event checked against the gclass at
    post time so the error names the caller, a ceiling of 10000 pending
    (it is not a work queue), and delivery as **a snapshot per cycle** — the
    messages queued when a cycle begins are the ones delivered in it, so a
    chain of posted events advances one step per turn of the loop and never
    starves the io_uring completions. `gobj_destroy()` drops what its gobj
    left posted, and `gobj_end()` says how many were never delivered.

    `yev_loop_run()` delivers them at the top of each cycle — before the
    completions, so an event posted in `mt_play()`, with the loop not yet
    running, does not wait for a completion that may never come — and does
    not block on the ring while any are pending. A gclass never calls the
    delivery itself. The ESP32 port carries its own copy of gobj and does not
    have this yet.

    Covered by `tests/c/gobj_post_message`, which checks each clause and, for
    the snapshot, that a chain posting itself for 200 ms still hears a 1 ms
    periodic timer: draining the queue until empty instead gives 2.2 million
    links and not one completion seen.

### Fixed

- **`webstats`: a file whose reader could not be built left the report and
    skipped the next one.** The path recorded nothing in `sources`, so the
    file vanished from a report that then read as a report of everything —
    the one thing `sources` exists to prevent. It also dropped the file from
    the pending list itself, and `ac_next_file()` drops the head too, so the
    **next** file went with it, unread and unmentioned.

    The second half only became reachable with the change below: before it,
    the correlator guard abandoned the run before either removal. Found by
    forcing the path, which had never executed — the fix is that the file is
    recorded as unread and the list is left to its single owner.

### Changed

- **`webstats` uses posted events for its two continuations.** The
    continuation between files is `EV_NEXT_FILE` and the one between chunks of
    a file is `EV_READ_CHUNK`, each named for what it does instead of arriving
    as `EV_TIMEOUT`. Both `C_TIMER0` children are gone; `C_WEBSTATS` keeps its
    `C_TIMER`, which measures a real time — the daily schedule.

    This also removes a way for a run to stall for ever. The reader-creation
    failure path armed the deferred continuation while the `reader_done` flag
    that told the two `EV_TIMEOUT`s apart was still false, and the guard that
    read that flag abandoned the run: `ST_READING` with the schedule already
    disarmed, so no further report until the yuno was restarted, and every
    `report-day` answering *"A run is already going"*. With the event named,
    the flag and its guard have nothing left to do.


## 7.9.13

A release to carry one agent fix to the nodes. `yuno_agent` changed, so this is
a version bump and not a packaging revision.

It is a **version** bump for a reason worth stating: the agent is not a managed
yuno, so no `install-binary` reaches it. On a node with no SDK sources the
package is the only road to its binary — the same argument that cut 7.9.12.

`yuneta_agent22` is untouched and needs no update: the escape hatch exposes tty
and consoles only, no config commands. That is what lets a node take the new
agent with its second agent still running.

### Fixed

- **`update-config` never refreshed the `description` column**, so a config row
  kept the description of its **first** version for the rest of its life while
  its content changed underneath. `cmd_create_config` wrote the column from the
  content's `__description__`; `cmd_update_config` wrote only `zcontent` and
  `date` and left the column alone.

  That column is not decoration. `list-configs` is where an operator reads what
  a config row is for, and the batch convention exists to feed it: every edit
  rewrites `__description__` as that version's changelog entry. The convention
  was resting on a field the update command did not maintain, and `sync-configs`
  drives its whole `UPDATE` path through that command.

  Found on `e.com`: the `webstats` row still announced *"1: initial load"* long
  after its content had stopped reading the dead openresty tree. The content
  was right, the label was two edits stale, and nothing anywhere said so.

  **How far it had spread: one row.** The five nodes were swept afterwards,
  comparing every config row's column against the `__description__` its own
  content carries — 51 rows over 546 stored records — and the `e.com` one was
  the only mismatch. The reason is the convention itself: an edit normally
  **bumps `__version__`**, which takes the `create-config` path and writes the
  column correctly. `update-config` overwrites the row in use, which is the
  deliberate exception for changing one yuno without bouncing a node, and it is
  rare. The defect was real and silent; its blast radius was not wide.

  An update now writes the description with the content, unconditionally, as
  `create-config` does — a content with no `__description__` leaves the column
  empty rather than keeping a text that describes content that no longer
  exists. Exercised on a live agent: changed, restored, and emptied.


## 7.9.12

A release with one reason to exist: **to put the `webstats` binary in the
packages**, so the other four nodes can run it. The yuno was written and proved
on one node, and a yuno that lives only in a build tree reaches nobody — the
`.deb` and the `.rpm` ship `outputs/yunos/` whole, and that is the road to a
node that carries no SDK sources.

Nothing under `kernel/`, `modules/` or `utils/` moved, so no running yuno needs
a rebuild for this. `@yuneta/gobj-js` stays at 7.9.11 on npm: this release
carries no JavaScript.

### Added

- **`webstats`, a new yuno: the daily report of the node's web server logs.**
  It reads the nginx (or openresty) access and error logs of a day, counts what
  they hold, keeps the numbers in TimeRanger2 and mails the result through
  `emailsender`. One yuno per node.

  It exists because a sealed node has no SSH, and reading a log by logging in
  is a task that stops working the day the node is sealed. The five nodes had
  never had their logs read until somebody went looking.

  The decision that keeps it small: **the day of a line comes from the
  timestamp the line carries, never from the file it sits in.** No read offset,
  no hook into `logrotate`, so the report of any day still on disk can be
  rebuilt at will and a yuno that was down loses nothing.

  What the report leads with is **visitors, and how many are new** — an address
  that asked for a piece of the page and got it, whose user agent carries no
  crawler mark. Counting requests answers a different question: on the first
  real day the top user agent was one `curl` making 3015 requests from a single
  address, and 1346 addresses claimed to be a browser while 71 ever fetched a
  script. Fingerprints are stored, never addresses.

  Then: totals and status classes, per hour, per vhost, the top paths, 404s,
  clients, agents and referrers, every 5xx whole, probes counted (banning stays
  `fail2ban`'s job), a latency histogram with percentiles, and the error log
  grouped by signature — the half where the real findings were.

  Documented at [`/webstats`](https://doc.yuneta.io/webstats) and in
  `yunos/c/webstats/README.md`, which carries the design and the traps.

### Changed

- **`log_format vhost` gained `$request_time` and `"$upstream_response_time"`**,
  appended last, under the same rule as `$host`: everything before them stays
  byte for byte `combined`, so `awk` column positions, goaccess and the stock
  fail2ban filters keep working. The count of quotes now says which generation
  a line belongs to — 6, 8 or 10 — and the rotated files hold all three for 30
  days, so any reader of these logs has to accept all three. Deployed on the
  five nodes; the configs live in their own operations repos.

- **`CLAUDE.md`: the `command-yuno` name collision is not only `id`.**
  `cmd_command_yuno` passes its whole kw as the filter that selects the yuno,
  so every field of the yuno record is a reserved parameter name. A command
  with a `date` parameter answers *"Yuno not found"* and names the yuno, never
  the parameter.

- **`CLAUDE.md`: a bumped config version does not reach the yuno on its own.**
  `create-config` appends a row and the primary does not move — not even a
  restart of the yuno picks it up. Only a node-wide `deactivate-snap` promotes
  it. The note now says when to bump and when to overwrite the row in use with
  `update-config` instead of bouncing a node to change one yuno's config.


## 7.9.11-3

A packaging revision, like the one before it: nothing under `kernel/`,
`modules/`, `utils/`, `yunos/` or `tests/` changed, so `YUNETA_VERSION` stays at
7.9.11 and only the `RELEASE` counter moves. The packages are rebuilt as
`yuneta-agent-7.9.11-3` and attached to the existing 7.9.11 tag.

It exists for one reason: the fail2ban filter shipped in 7.9.11-2 could not ban
anybody on a single-page app, and a package is the only way that fix reaches a
node that installs from scratch. It was found by doing exactly that — imaging
two nodes, installing 7.9.11-2 on them and probing them end to end.

### Fixed

- **The fail2ban probe filter never fired on a SPA vhost.** It matched only
  responses 404, 403 and 444, on the reasoning that a path some app really
  serves would stop matching once it answered 200. That holds for a static site
  and collapses on a single-page app: with `try_files $uri $uri/ /index.html`
  every unknown path answers **200** with `index.html`, so `/wp-login.php` came
  back 200 and the filter matched nothing. Both yunovatios consoles were
  running the jail blind — enabled, healthy, watching the right file, and
  incapable of ever banning anybody.

  Found by installing 7.9.11-2 on a freshly imaged node and probing it end to
  end, which is the only way it could have been found: on the nodes it was
  developed against, every unknown path 404s.

  The status is gone from the three patterns. It costs nothing, because the
  paths cannot be legitimate on a Yuneta node — no PHP, no WordPress, nothing
  serving `.env` or `.git`. On one day of real traffic the restriction was also
  hiding 606 further lines on a mixed node: probes answered 301 by the
  http→https redirect and probes answered 200 by a SPA. Of all of them, none
  came from another node of the fleet and none from a real crawler.

  The path is now matched up to the query string, so a `.php` appearing only in
  a parameter is not a match.

## 7.9.11-2

A packaging revision, not a new version of Yuneta: nothing under `kernel/`,
`modules/`, `utils/`, `yunos/` or `tests/` changed, so `YUNETA_VERSION` stays
at 7.9.11 and only the `RELEASE` counter moves. The packages are rebuilt as
`yuneta-agent-7.9.11-2` and attached to the existing 7.9.11 tag.

What it ships is the answer to a question nobody had asked of these nodes:
what is in their logs. nginx has no rotation of its own, so `access.log` and
`error.log` had been growing since the day each node was installed — on all
five, none had ever been rotated. Reading them for the first time turned up
that 99.9% of one node's `error.log` was scanner noise, that 43% of all
requests were probes for `/.env` and `/wp-login.php`, and that nothing was
watching any of it.

### Added

- **The packages rotate the web server logs** — `/etc/logrotate.d/yuneta`, a
  conffile in both the `.deb` and the `.rpm`, with `logrotate` added to
  `Depends` / `Requires`.

  nginx has no rotation of its own: it only knows how to reopen its files when
  it gets `USR1`. Nothing was sending that signal, so `access.log` and
  `error.log` had been growing since the day each node was installed — on all
  five, none had ever been rotated, and the busiest was writing 3.5 MB a day.
  Disk was never the problem. A log nobody can open is.

  Both trees are listed (`/yuneta/bin/nginx/logs/`,
  `/yuneta/bin/openresty/nginx/logs/`): a node runs one or the other, and
  `missingok` covers the absent one. Daily, 30 kept, compressed with
  `delaycompress` so the file the master still writes to is not cut. The
  `postrotate` sends `USR1` only to a pid that is alive, so a stale pid file
  cannot signal a process the kernel handed to somebody else. The logs of the
  certbot deploy hook (`/var/log/yuneta/*.log`) rotate monthly in the same
  file.

  The **yunos are not touched**: each one writes numbered files under
  `/yuneta/realms/<realm>/<yuno>/logs/` and rotates them itself.

- **The packages ship a fail2ban filter and jails for the web server** —
  `/etc/fail2ban/filter.d/yuneta-nginx-probe.conf` and
  `/etc/fail2ban/jail.d/yuneta-nginx.conf`, both conffiles.

  The stock `nginx-botsearch` filter looks for webmail, phpMyAdmin and
  WordPress. Measured against 15 days of a real node's `access.log` it matched
  1089 lines out of 224 645, while that log held 95 829 refused requests: the
  node was scanned all day with nothing watching. The new filter matches 52 383
  of those lines, from 972 addresses.

  It does not ban on 404s — search engines collect those honestly, and a rate
  rule bans Googlebot first. It bans on what was asked for: any `.php` path (no
  node runs PHP), the dot-directories that hold source control or credentials,
  and `/wp-*`. Several hundred matching lines carry the user agent of Googlebot,
  GPTBot or ClaudeBot, and every one is an impostor: the addresses reverse to
  `googleusercontent.com` and to Cloudflare, and the real Googlebot does not ask
  for `/.env.backup`.

  **Both jails ship disabled**, which is not timidity. If none of a jail's
  `logpath` globs resolves to a file, fail2ban does not skip the jail — it
  refuses to configure and the whole server exits 255, taking every other jail
  down with it, `sshd` included. A node carrying this package whose web server
  has not run yet is exactly that case. `packages/deb/README.md` carries the
  two-line command to enable them, and the two ways a jail lies about its own
  health: watching nothing while reporting healthy (a node whose `[DEFAULT]`
  sets `backend = systemd`, so the jails now pin `backend = auto`), and
  recording bans that never reach the firewall (a `banaction` naming a command
  that is not installed).

- **The documentation site owns its `robots.txt`** — `docs/doc.yuneta.io/robots.txt`,
  installed by `deploy.sh` over the one mystmd builds.

  myst has no absolute address for the site (`site.options.base_url` is `/`),
  so the `Sitemap:` line it wrote named the address of its own development
  server, `http://localhost:3000/sitemap.xml`. Every crawler had to discard
  that line, which left the sitemap myst does build — 12 KB of it — reachable
  by nobody.

  It also refuses six backlink and rank crawlers (Semrush, Ahrefs, MJ12,
  dotbot, DataForSeo, SERanking), which read the whole site to sell the numbers
  back and brought about 14 000 requests in 15 days. Search engines and the
  assistants are deliberately not on that list: those are how somebody finds
  Yuneta.

- **A 404 page for the documentation site** — `docs/doc.yuneta.io/errors/404.html`,
  installed at the root of the build by `deploy.sh` the same way as the landing
  page, because the `--delete` rsync erases anything dropped straight into the
  docroot. Five hostnames share that docroot and every one of their server
  blocks declared `error_page 404 /404.html` for a file that did not exist, so
  nginx logged a failed open on every 404 and the reader got the built-in page
  of nginx.

### Fixed

- **`install.sh` fetched the OLDEST package of a release, not the newest.** It
  picked the asset with `head -n1`, and the GitHub API lists assets in upload
  order — so on a release carrying more than one packaging revision it chose
  the first one uploaded. Caught the morning after this revision shipped: the
  installer downloaded `yuneta-agent-7.9.11-1` while `-2` sat next to it, and
  reported a clean install of a package with none of the logrotate or fail2ban
  configuration the revision exists to deliver.

  Now `sort -V | tail -n1`, which compares the numbers as numbers: revision 10
  sorts after 2, where a plain `sort` would not.

  `install.sh` is served from `main`, not from any package, so this needs no
  rebuild and no new revision — it takes effect on the next `curl | sh`.

## 7.9.11

The version skips 7.9.10 on purpose: `@yuneta/gobj-js` shipped 7.9.10 and
7.9.11 on its own line while the SDK sat at 7.9.9, and the SDK catches up to
the package rather than the other way round.

### Added

- **`shutdown` command on `C_YUNO`** — the control plane can now ask a yuno to
  stop itself, orderly. It **answers first and dies after**: the response
  travels over the very event loop the shutdown stops, so a handler that called
  `set_yuno_must_die()` on the spot would take the socket down with the answer
  still in it. The handler arms a timer and `ac_timeout_periodic()` does the
  dying — the same shape `timeout_restart` has always used, so the wait is at
  most one `timeout_periodic` (1 s by default). Measured end to end: asked at
  `t`, dead at `t+999 ms`.

  It exits with code **0**, so the ydaemon watcher does not relaunch the yuno.
  It does not replace `kill-yuno`, which stays the deploy path: that one is
  asked of the **agent**, which knows the yuno and deregisters it; this one is
  asked of the **yuno**, which is what you have when the yuno answers and the
  agent does not, or when the yuno runs under no agent.

  Pinned by `tests/c/command_shutdown/`, which asserts the order that matters:
  the command answers `result=0`, the yuno is still running when the answer
  comes back, and it dies on its own afterwards (a watchdog turns "it never
  died" into a failed check instead of a ctest timeout).

- **`@yuneta/gobj-js` 7.9.11 — `C_TIMER`: the two calls are the whole contract**
  (submodule bump). `set_timeout()` arms and `clear_timeout()` disarms, as in C
  (`c_timer.h`); whether the gobj is running stops being the caller's problem.
  The JS port had neither half: every view had to pair its `set_timeout()` with
  a `gobj_start()` and remember a `gobj_stop()` on the way out, or get
  *"Destroying a RUNNING gobj"* when its route was left. The running state now
  follows the timeout in `mt_writing` on the `msec` attribute — not in the
  helpers, because those three PUBLIC functions are an escape from the gclass
  interface and must be sugar and nothing else: writing `msec` by hand leaves
  the timer exactly as `set_timeout()` would.

  A second fix from the same reading: a **periodic cleared from inside its own
  action** now really stops. The re-arm ran after the action, undoing the clear
  and re-arming with the `msec` the clear had just written — a negative delay,
  which `setTimeout()` serves immediately, so the timer became a busy loop.

  **BREAKING for callers that stop the timer themselves** (`clear_timeout()`
  followed by `gobj_stop()` now logs *"GObj NOT RUNNING"*). Every in-tree
  consumer was migrated in the same release; the `yunos/js` submodule carries
  its half. 7.9.11 finishes the job inside the runtime itself, where
  `c_ievent_cli.mt_stop()` still stopped its timer by hand and logged that very
  complaint on every disconnect.


- **`@yuneta/gobj-js` 7.9.9 — `gobj_set_gclass_no_trace()`** (submodule bump).
  The C kernel has it; the JS port did not, even though the field was there
  and already consulted. Without it the idiom every C `main()` uses to keep
  timers out of a `machine` trace could not be written in JS at all, so the
  SPAs' machine trace drowned in the yuno's one-second periodic tick. Both JS
  yunos now carry that block. Also realigns the package with
  `YUNETA_VERSION`, which had drifted at 7.9.6.

- **`@yuneta/gobj-ui` 5.9.0 — shared clipboard helpers** (submodule bump).
  `yui_clipboard.js` gives every table one line to hand its rows over:
  `yui_copy_table_json()` copies what the user is LOOKING AT — the selected
  rows when there is a selection, otherwise every row the current filters leave
  on screen. Four views had each grown their own copy code and it had drifted;
  two wrote unindented JSON and two said nothing when the write failed.

- **gui_agent's tables copy as JSON** (`yunos/js` submodule bump). Until now
  the only way to pass one of those lists to anyone was a screenshot. Nodes and
  Statistics get a *Copy JSON* button; the console's copy button — created
  disabled and only re-enabled for text answers, so it was dead for `top`,
  `list-yunos` and most commands — now copies table answers too. Each history
  row also runs its command in one gesture instead of two.

- **`@yuneta/gobj-ui` 5.10.0** — `yui_button_mark_done()` /
  `yui_button_unmark()`, so a copy button can say "Copied" for a moment. The
  timing stays in the consumer's FSM (a `C_TIMER` + `EV_TIMEOUT`), not in a
  `setTimeout` inside the library.

- **`@yuneta/gobj-ui` 5.11.0 — the PWA install offer** (submodule bump). Chrome
  advertises its install banner on a heuristic nobody can read, and goes quiet
  on an origin for months after a dismissal or an uninstall — the app then
  looks uninstallable when it is only unadvertised. `yui_install.js` refuses
  the banner, keeps the event and asks once per browser with the family
  dialog. The event arrives before the bundle is parsed, so each SPA catches
  it in `public/install-prompt.js` loaded by `<script src>` — never inline,
  which their `script-src 'self'` drops in silence. Ported from yunomúsica.

- **The JS yunos install as PWAs and their tables copy as JSON** (`yunos/js`
  submodule bump), and every action in them crosses the FSM: the *Refresh*
  buttons and the console's copy flash were a direct call and a bare
  `setTimeout`, so a click and its consequences never reached the `machine`
  trace.

- **doc.yuneta.io reads offline.** `pwa/sw.js` keeps the theme bundles
  (cache-first — their names are content-hashed) and every page the reader
  opened (network-first, so a redeployed page never reads stale), and falls
  back to a `offline.html` for a page that was never read. It does not
  precache: the build is 28 MB, which is not a cost to put on a mobile
  connection for pages nobody asked for. `deploy.sh` stamps the deploy version
  into `sw.js` — the caches are named after it and `activate` drops every
  other, so a deploy cannot leave a mixture of an old page and new bundles.
  A page arrives under two urls (`/<slug>`, and `/<slug>?_data=<route>` when
  the theme routes on the client); this host answers the same bytes to both,
  so the query is dropped from the cache key and one entry serves both.
  `pwa/offline-test.mjs` asserts the lot against the deployed site — and does
  it by relaunching a profile behind a dead proxy, because
  `context.setOffline()` is a no-op in Playwright's Firefox and passes every
  assertion without cutting anything.

- **doc.yuneta.io installs as a PWA.** A manifest and its icons ship in
  `docs/doc.yuneta.io/pwa/`, and `deploy.sh` installs them and injects the
  `<link rel="manifest">` into the `<head>` of every built page — the
  book-theme has no hook for the head. There is no service worker, so this
  buys a window and an icon, not offline reading. The manifest is served at
  `/manifest.webmanifest` by one `location` in the `doc.yuneta.io` vhost
  alone, aliased to the `/pwa/` copy: `yuneta.io`, `yuneta.com`, `yuneta.es`
  and `yunetas.com` share that docroot, but `/` on them is the landing page,
  so a manifest sitting at the root would offer five installs of a different
  app under one name.

### Changed

- **`c_yuno.h` sheds 4 of its 9 public functions, and the two that stay say why.**
  `c_yuno` is one of the 4 gclasses in `root-linux` whose header exports PUBLIC
  C functions. Four of them — `add_allowed_ip()`, `remove_allowed_ip()`,
  `add_denied_ip()`, `remove_denied_ip()` — had **no caller anywhere**: the
  add-/remove- commands that use them live in `c_yuno.c` itself, so they are
  `PRIVATE` now. Nothing outside changes; the operator interface (the
  `allowed_ips`/`denied_ips` `SDF_PERSIST` attributes plus the six
  list/add/remove commands) was already complete.

  The rest of the header is kept **on purpose, with the reason written next to
  it**: `is_ip_allowed()`/`is_ip_denied()` are asked once per accepted
  connection (`c_tcp_s`) and once per login (`c_authz`), where the canonical
  local method would cost a `kw` per call; and `yuno_event_loop()` /
  `set_yuno_must_die()` are process-level, not gclass interface — every caller
  of the latter is ending its OWN process (a signal handler, a CLI told to
  quit, a test yuno that finished: 63 call sites across 49 test files), so an
  event would be a second door to the same thing. A justified escape that is
  written down is not the same as an oversight.

  `c_authz`'s two — `authz_checker()` / `authentication_parser()` — are not an
  escape at all: they implement kernel typedefs (`authorization_checker_fn`,
  `authentication_parser_fn`), are installed as process defaults by
  `entry_point.c` and swapped through `yuneta_setup()`. The call goes
  **downwards**, from the kernel into the gclass, which no gclass-interface
  mechanism can express — `authz_checker()` does not even take a C_AUTHZ
  instance, it looks the service up itself.

- **`yuno_event_detroy()` → `yuno_event_destroy()`.** The typo had been in the
  header since the function was written. One in-tree caller
  (`entry_point.c`), no out-of-tree ones.

- **`C_TIMER` / `C_TIMER0`: the timeout helpers are sugar now, and the
  behaviour lives behind the interface.** `set_timeout()`, `set_timeout_periodic()`
  and `clear_timeout()` are three of the very few PUBLIC C functions a gclass
  header exports — 4 of the 33 gclasses in `root-linux` do it, and CLAUDE.md
  says to treat those as defects. The arming therefore moved out of them and
  into **`mt_writing()` on the `msec` attribute**, which is the real interface:
  `gobj_write_integer_attr(timer, "msec", 1000)` now leaves the timer exactly
  as `set_timeout(timer, 1000)` does, where before it armed the countdown and
  left the gobj stopped. Same for `C_TIMER0` with its io_uring event.

  **No caller changes and no behaviour change** for anyone using the helpers —
  they write the same two attributes they always did, and every trace line is
  unchanged. What changes is that the gclass no longer has two doors with
  different behaviour. `c_timer0`'s helpers had to swap their two writes
  (`periodic` before `msec`), since the `msec` write is the one that arms.

  This is the C half of the same normalization shipped in `@yuneta/gobj-js`
  7.9.10/7.9.11, where the split was doing real damage: JS had never started or
  stopped the gobj at all, so every view paired its `set_timeout()` with a
  `gobj_start()` and got *"Destroying a RUNNING gobj"* when it forgot the
  matching stop. The two ports are now the same contract, function for
  function. `GOBJ.md` §7 (the worked example is `c_timer.c` itself) and the
  timer API page were rewritten to match.

  Not touched: the **ESP32** `c_timer` (`root-esp32`), which arms with
  `gobj_play()`/`gobj_pause()` and cannot be built or tested here.

## 7.9.9

### Fixed

- **A foreground process honours SIGTERM; a daemon still ignores it.**
  `c_yuno.c`'s signalfd handler marked SIGTERM `// ignored` for every process
  built on the framework, because `capture_signals()` runs unconditionally in
  `mt_start`. For a daemon that is deliberate — its watcher parent is deaf to
  everything, `--stop` kills with SIGQUIT then SIGKILL, and a stray SIGTERM
  from `init` at shutdown must not take a node's yunos down. For a CLI it was
  inherited by accident, and it breaks the Unix contract: `timeout` never
  escalates to SIGKILL on its own, so `timeout N ycommand …` **hung forever**
  and left an immortal process behind — two were found alive after two and a
  half days, with the password still visible in `ps`.

  SIGTERM now takes the same orderly path as SIGQUIT/SIGINT **unless the
  process runs as a daemon**, reported by the new `yuneta_is_daemon()`
  (`entry_point.h`), which is true exactly when `--start` was given. The agent
  launches every managed yuno with `--start` (`c_agent.c`), so no daemon
  changes behaviour. Verified on both branches: `ycommand` now dies on
  SIGTERM and `timeout 8` returns in 8 s, while a `--start`ed yuno ignores
  SIGTERM in both watcher and child and still stops cleanly with `--stop`.

  The standalone timeranger tools (`tr2list`, `tr2keys`, `tr2search`,
  `tr2migrate`, `treedb_list`, `msg2db_list`, `stats_list`, `fs_watcher`) do
  not use `yuneta_entry_point`, so they carried their own copy of the same
  v6-era boilerplate; each now routes SIGTERM to its orderly quit handler
  instead of ignoring it.

### Added

- **`@yuneta/gobj-ui` 5.6.0 → 5.8.2** (submodule bump; its own `CHANGELOG.md`
  carries the detail).

  - **The bottom toolbar of `C_YUI_FORM` is configurable**, and a toolbar with
    a single group is centred. A button the caller drops no longer breaks the
    form.
  - **`yui_shell_confirm_danger()`** — a destructive confirmation whose button
    is RED. `yui_shell_confirm_yesno()` puts its yes in `is-link`, the right
    colour for *"do you want to continue"* and the wrong one for *"this deletes
    an account"*: the two read the same at a glance, and the destructive one is
    the one that must not be clicked by reflex. The safe answer is the last
    button, so Escape, the backdrop and the X all resolve to it.
  - **The treedb table's search stretches on a phone**, where its row used to
    keep its natural width and leave the most used control the narrowest thing
    on screen. Its placeholder was the literal `'search...'` — a placeholder is
    not a text node, so `refresh_language()` could never reach it and it stayed
    English in every language; it carries `data-i18n-placeholder` now.
  - **Edit is a mode toggle** in that table, not one more action.
  - **A toolbar dropdown no longer opens off-screen.** The panel was anchored
    to one edge of its trigger and only that edge was guarded against the
    viewport, so a right-aligned panel near the left of the bar hung off the
    screen — which is what *every* `navbar-end` trigger does under
    `dir="rtl"`. It is clamped on both edges now; LTR positions are unchanged.

### Changed

- **The JS yunos consume the libraries from npm, not by `file:`** (`yunos/js`
  submodule bump). `gui_agent` and `gui_treedb` pointed `@yuneta/gobj-js` and
  `@yuneta/gobj-ui` at the `kernel/js/*` submodule checkouts, which tied a
  build to the superproject and to whatever those working trees held. They now
  resolve `^7.9.6` / `^5.8.2` from the registry, like wattyzer — **so there
  are no `file:` consumers left**, and a local edit under `kernel/js/**`
  reaches an app only after `npm publish` plus a range bump. The `file:` deps
  were symlinks and forced `resolve.preserveSymlinks`, which loads duplicate
  module instances; that flag and the `src/` aliases are gone, and `dedupe`
  now also lists `@yuneta/gobj-js`.

- **Both JS yunos install as a WebAPK.** Each ships a complete manifest
  (`display: standalone`, `start_url`, `scope`, 192/512 PNG icons and a
  maskable 512 variant) rendered from its existing SVG mark. `gui_treedb` had
  a `site.webmanifest` that was never installable: no `display` (so it
  defaulted to `browser`) and an SVG as its only icon, which Chrome does not
  accept. No service worker is involved — Chrome no longer requires one for
  installability. The manifests declare no `orientation`: `orientation: "any"`
  overrides the device's rotation lock, so the app rotates even when the user
  locked it. Both are named `manifest.webmanifest`, the name every SPA in the
  family uses, so the nginx `location` that declares their MIME type is the
  same line everywhere — `.webmanifest` is absent from nginx's stock
  `mime.types`, and without that block a manifest goes out as
  `application/octet-stream`.

### Documentation

- **doc.yuneta.io links back to the landing page.** The landing is raw HTML
  installed at `/landing` and is the front door served at `yuneta.io`, but it
  sits outside the myst toc, so nothing in the built site pointed at it: a
  reader who entered through `doc.yuneta.io` had no way to reach it. It is a
  `Home` entry in `site.nav` now. The book theme hides nav items below 1024px
  and its hamburger only opens the toc, so two rules in `_static/custom.css`
  keep the link reachable on a phone.

- **A new standalone page at `/high-semantics`** — *"High-level semantics,
  low-level language"*: Yuneta in one page, with the `machine` trace in the
  format it really prints, the same state table in C and in the browser, and a
  bill next to every decision. It argues rather than instructs, so it opens a
  third band on the landing (**"The idea"**, `essay-card`), with its own
  `ESSAYS` list and `check_band` guard in `deploy.sh`.

- **The standalone pages moved above the argument on the landing.** They were
  the last three bands before the site map: ten screens down on a 390×844
  phone, on a page 13.6 screens long. A card nobody scrolls to is a card that
  does not exist. `/navigation` was also missing from the site map.

## 7.9.8

### Fixed

- **A deferred answer never reached a browser: `input_service` did not name a
  service.** `C_IEVENT_SRV` recorded it as `gobj_name(gobj_parent(gobj))`. That
  parent IS a service in the agent's topology, and it is the CHANNEL when a SPA
  connects straight to a yuno through an iogate
  (`C_IOGATE^__top_side__` → `C_CHANNEL^tcps-N` → `C_IEVENT_SRV^tcps-N`).
  Anything that read the field back to route a deferred answer then looked the
  requester up under a name that is not a service, found nothing, and dropped
  the answer.

  That is why `register-idp-user` answered `command-yuno` and never answered a
  SPA — the browser sat waiting for ever, with no error anywhere. It is now the
  nearest **service** ancestor, which is the same value as before wherever the
  parent already was one.

- **`kc_answer()` dropped an answer in silence** when it could not resolve the
  requester. Silence there is indistinguishable from a request that never
  arrived, and that ambiguity is what hid the bug above. It logs a warning with
  `op`, `req_service` and `req_channel` now — a client that disconnects
  mid-flight is normal, hence warning and not error.

- **`list-idp-users` and `get-idp-user` asked Keycloak for the user-profile
  schema of every row.** Measured against a real realm: 1.5 kB per account of
  which 230 bytes are the account, and the same `userProfileMetadata` block
  repeated for each one — a page of 50 came to ~77 kB instead of ~12 kB.
  `briefRepresentation=true` does not suppress it; `userProfileMetadata=false`
  does, and both commands send it now.

## 7.9.7

### Added

- **Five commands to manage the accounts of the IdP**, so an operator does not
  have to open the Keycloak web console: `list-idp-users` (search + paging),
  `get-idp-user`, `update-idp-user` (name, `enabled`, `emailVerified`,
  `requiredActions`), `delete-idp-user` and `send-idp-user-actions` (the
  invitation email again, with the actions you choose). All on the `idp`
  service, all asynchronous, all through the same bounded queue and the same
  shared connection as the registration.

  They share one gated permission, **`manage-idp-users`**, kept apart from
  `register-idp-user` on purpose: an operator who registers people does not
  have to be able to delete them.

  **`delete-idp-user` deletes in the IdP only.** The `treedb_authzs` record
  stays. The two planes are deleted one by one so nobody loses an
  authorization record to a cascade they did not ask for; `delete-user` of the
  `authz` service is the other half.

  **The page is bounded** (default 50, ceiling 500). The realm is shared with
  every other product of the organization, so one call must not be able to pull
  all of it.

- **`default_role` in `C_AUTHZ`** (persistent, writable). When it reacts to
  `EV_IDP_USER_CREATED` and the event carries no role, it links this one. Empty
  by default, so the user enters and can do nothing, which is visible and safe.
  No role is hardcoded, because the roles come from the `initial_load` of each
  realm and none is guaranteed to exist — and for the same reason the attr is
  checked before use: a `default_role` that does not exist creates the user
  with no role and logs an error, instead of failing in silence for ever.

- **A `messages` trace level in `C_IDP_KEYCLOAK`**: the operation, the method,
  the resource, and the status of the answer. Deliberately not the request and
  not the body — the request carries the `Authorization: Bearer <admin token>`
  header, which opens `manage-users` over the whole realm until it expires, and
  the body of a read is the account data of the realm. A trace turned on to
  debug a request must not leave either in the log.

### Changed

- **The IdP request queue is generic.** `KC_PENDING` carried the fields of the
  registration in the struct (`email`, `first_name`, `last_name`, `role`);
  adding five operations that way would have been five copies of the same
  shape. It is now `IDP_PENDING` with `{op, params}`, the job list is chosen
  from `op`, and the five one-round-trip operations share one job pair instead
  of five near-identical ones. A new operation is a case in two functions and
  nothing else.

- **BREAKING: IdP account provisioning leaves `C_AUTHZ` for `C_IDP_KEYCLOAK`.**
  `C_AUTHZ` answers one question — does this user hold this permission, against
  `treedb_authzs`. Creating accounts in an external identity provider is a
  different responsibility, with different credentials (a confidential admin
  client with `manage-users`), a different transport (`C_PROT_HTTP_CL` over
  `C_TASK` with its own queue) and different failure modes (the IdP down, a
  409, an expired token). They shared a file by accident of how it was written.

  `set-kc-config`, `view-kc-config` and `register-idp-user` move unchanged to
  the new gclass, together with the `kc_*` attrs, the pending queue and the
  three-job Keycloak pipeline — about 600 lines out of `c_authz.c`. The
  commands and the service name stay **neutral** (`idp`, `register-idp-user`),
  so a second provider enters as a sibling gclass serving the same vocabulary,
  selected in configuration: the `ytls` pattern with OpenSSL / mbedTLS, and not
  an abstraction invented against a single implementation.

  **What callers must change.** Send `register-idp-user`, `set-kc-config` and
  `view-kc-config` to the service `idp`, not `authz`, and declare the service
  in the yuno config:

  ```
  {'name': 'idp', 'gclass': 'C_IDP_KEYCLOAK', 'priority': 0,
   'default_service': false, 'autostart': true, 'autoplay': false, 'kw': {}}
  ```

  **What operators must do.** Persistent attrs live in
  `<GCLASS>-<service>-persistent-attrs.json`, so what `set-kc-config` wrote for
  `C_AUTHZ-authz` is not found by `C_IDP_KEYCLOAK-idp`, and the first
  `register-idp-user` answers `kc_unavailable`. **Move the six `kc_*` keys to
  the new file with the yuno stopped** — move, not copy: `C_AUTHZ` no longer
  declares those attrs, so a key left behind logs *"GClass Attribute NOT
  FOUND"* at every start. The recipe is in `YUNO_AUTH.md` §7.3; running
  `set-kc-config` again also works, at the cost of the client secret on a
  command line. The permissions moved with their commands and are now
  permissions of the `idp` service.

- **New event `EV_IDP_USER_CREATED`, the seam between the two planes.** The
  provisioner publishes it after a created account and each plane records its
  own user: `C_AUTHZ` writes the `treedb_authzs` node it used to write from
  inside the Keycloak pipeline. The dependency points one way — the provisioner
  knows there is an authz plane to notify, and the authz plane knows of no
  provider — so the event is declared in `c_authz.h` and no authz code includes
  a provider header. A subscriber action returns 0 when it recorded the user; a
  negative return reaches the caller as the existing `authz_write_failed`
  warning. Tagged `EVF_NO_WARN_SUBS`: a yuno may provision accounts with no
  authz plane at all.

- **New local method `has_role` on `C_AUTHZ`.** The roles belong to the authz
  plane, and the provisioner has to refuse an unknown one *before* it creates
  anything in the IdP — which is what `register-idp-user` did when both lived
  in the same gclass. A local method and not a public C function, because a
  gclass exposes itself through attributes, commands, events, local methods and
  statistics, and nothing else.

## 7.9.6

### Added

- **`--ssl-server-name` in `ycommand`, `ybatch` and `ystats`, and as a
  parameter of `connect` in `ycli`.** The name to check the server certificate
  against, used for SNI too. It travels to `ytls` as `ssl_server_name`, which
  both TLS backends already implemented and `c_tcp` already honored ("a
  config-supplied ssl_server_name wins"); no tool exposed it.

  This is what a pinned certificate needs. The agent serves one long-life
  certificate of its own on every node (`CN=yuneta_agent.yuneta.io`), so its
  name never matches the host dialed, and every tool rejected the handshake
  for a hostname mismatch and then retried in silence. Nothing is weakened:
  the chain is still validated, against the PEM given in
  `--ssl-trusted-certificate`.

- **`ybatch` and `ystats` take the whole TLS family**
  (`--ssl-use-system-ca`, `--ssl-trusted-certificate`, `--ssl-server-name`,
  `--ssl-allow-insecure-client`). Their crypto was a literal
  `{"ssl_use_system_ca": true}` in the code, so a private CA was out of reach
  and a `wss://` agent answered *"unknown CA"*. `ycli` takes the same four as
  parameters of its `connect` command, next to the url they belong to.

### Changed

- **Submodules.** `kernel/js/gobj-ui` moves to the maplibre-gl floor raise
  (peer `^6.0.0` → `^6.1.0`, the version this SDK ships), and
  `utils/python/tui_yunetas` to the yunetas CLI **0.19.1**, which stores the
  TLS values of a node and carries the node identity on every `ycommand` call
  it makes.

### Fixed

- **`C_AUTHZ` hammered the IdP with a connection nobody asked for.** The
  outbound client to Keycloak is created with manual start, `process_next_kc()`
  starts it, and nothing stopped it: `C_TCP` retries for ever, so the yuno
  reconnected once a minute for its whole life. One run of a deployed yuno had
  759 of those round trips, every one publishing to nobody.

  `ac_end_task()` now stops the client when no request is left, and
  `process_next_kc()` starts it again for the next one. Beyond the noise, this
  gives **each registration a fresh connection**, so a late answer can no
  longer land on the following task. The stop on a timed-out round trip stays
  where it was — before the next task starts — because there the connection is
  poisoned even when more requests are queued.

  Measured against a stub of the IdP: three idle minutes, zero connections,
  where the same window used to open three.

- **`set-kc-config` destroyed a running gobj.** It dropped the cached client
  without stopping it first, which the framework reports as *"Destroying a
  RUNNING gobj"*. It stops it now.

- **`gobj_local_method()` matched a prefix, not the name.** The dispatcher
  compared with `strncasecmp()` over the length of the TABLE entry, so a
  gclass whose `LMETHOD` table held `do_it` and `do_it_result` answered BOTH
  lookups with `do_it`: the second method was unreachable, and the job that
  asked for it silently ran the first one again. It now compares the whole
  name.

  This is what kept `register-idp-user` from ever working. `C_AUTHZ` names its
  jobs `kc_create_user` / `kc_create_user_result` and `kc_send_email` /
  `kc_send_email_result`, so the create-user request went out twice and the
  handler that reads the answer of Keycloak — the status, and the `Location`
  header that carries the id of the new user — never ran. The task then failed
  on an empty user id and answered the catch-all `kc_unavailable`, which reads
  as an unreachable IdP. Verified end to end against a stub of the Keycloak
  admin API: token, then 201 with `Location`, then `execute-actions-email`,
  and the caller reads *"User registered"*.

  BREAKING for a gclass that leaned on the prefix behavior: a lookup that used
  to reach a shorter entry now answers *"internal method NOT EXIST"*. A sweep
  of every `LMETHOD` table in this repo found exactly two prefix pairs, both in
  `C_AUTHZ`, and both were the bug.

- **A scalar answer was invisible in `ycommand` and `ybatch`.** Their display
  handled `data` only as an array or an object, so a command that answers with
  a string, a number or a boolean printed nothing at all — legal JSON, read by
  nobody. `node-uuid` is how it showed: the uuid appeared in `ycli`, which has
  its own display, and vanished through `ycommand` and therefore through the
  controlcenter scripts, which run `ycommand`. The answer had left the agent
  well formed.

- **`node-uuid` answers like every other command.** It put the uuid in `data`
  as a bare string. It now carries it in `comment`, prefixed with the yuno
  identity as the other `cmd_*` handlers do, and in `data` as
  `{"uuid": "..."}`. Clients no longer need to special-case a scalar to show
  it, which is the half of the fault that lives in the agent.

- **`ystats` never sent its token.** Its gobj tree defined `__jwt__` and no
  field used it, so every remote connection went out anonymous and the agent
  refused it with *"Without JWT/passw only localhost is allowed"*. The `jwt`
  field is now in the `C_IEVENT_CLI` kw, as in `ycommand` and `ybatch`.

- **`ystats` swallowed a failed open.** Its FSM declared neither
  `EV_ON_OPEN_ERROR` nor `EV_ON_ID_NAK`, so a refused connection was lost in
  *"Event NOT DEFINED in state"* and the tool retried for ever without a
  word. Both now reach `ac_on_close`, which is what `ycommand` does.

- **`ystats` ignored `-o`, `-O` and `-S`.** The connection was built with
  literals (`yuneta_agent`, `__default_service__`), so the three options
  existed and changed nothing.

- **`ycommand` applied the pinned name to the IdP too.** `build_client_crypto()`
  serves two different peers, and `ssl_server_name` names ONE certificate:
  given to the token endpoint as well, the login died with a hostname mismatch
  before the agent was ever dialed. The builder now takes `pin_server_name`,
  true only for the agent.

- **`C_AUTHZ`: `register-idp-user` could never work.** The outbound client to
  Keycloak (`C_PROT_HTTP_CL`) is a CHILD gclass, so it subscribed its parent to
  every event it publishes, and `ensure_kc_client()` never dropped that
  subscription. Three faults at once, all from the one cause:

  - `EV_ON_MESSAGE` is not in the FSM of `C_AUTHZ`, so **every answer of
    Keycloak was lost** in *"Event NOT DEFINED in state"*. The task ended
    without an answer and the caller got the catch-all `kc_unavailable`,
    *"Keycloak register-idp-user failed"*, which reads as an unreachable IdP.
  - `EV_ON_OPEN` is not in the FSM either, so the connection logged a second
    error.
  - `EV_ON_CLOSE` **is** in the FSM, for a USER channel that closes. The close
    of this outbound socket therefore entered `ac_on_close` and ran the logout
    of a user that does not exist (*"__session_id__ not found"*, *"User not
    found"*, and *"kw must be list or dict"* from the treedb).

  `ensure_kc_client()` now unsubscribes the parent right after it creates the
  client, which is what `C_AUTH_BFF` already does with its IdP client. The
  audience is the `C_TASK`: it subscribes to `gobj_results` by itself.

  The path was never exercised before, because it needs a confidential IdP
  client that no deployment had. Verified against a real Keycloak with a
  `kc_admin_client_id` that does not exist: the answer is now
  `kc_token_refused` with *"Keycloak admin authentication failed (status
  401)"*, and the log carries neither the undefined events nor the phantom
  logout.

- **`C_AUTH_BFF` now logs what the IdP answered.** An IdP failure that did not
  match a specific mapping was reported to the operator as
  `auth_unexpected_error` and nothing else, because `send_error_response` logs
  the browser-facing code, not the cause. A deleted `client_id` was therefore
  indistinguishable from an IdP outage, and a real deployment lost its login to
  exactly that (Keycloak answers 401 + `invalid_client` for a client that no
  longer exists).

  `send_token_to_browser` now emits one line with `action`, `idp_status`,
  `idp_error`, `idp_description`, `client_id` and the browser code it mapped
  to. It fires only for the three generic mappings (`auth_unexpected_error`,
  `auth_config_error`, `auth_service_unavailable`); the specific ones
  (`invalid_credentials`, `session_expired`, `account_disabled`,
  `auth_rate_limited`) already name the cause and stay on one line, so a wrong
  password does not double its log volume. Nothing that reaches the browser
  changes.

- **`C_AUTH_BFF` maps `invalid_client` on 401 too.** The mapping accepted that
  IdP error only with status 400, and Keycloak answers **401** for a
  `client_id` that does not exist or whose secret is wrong. The commonest
  misconfiguration of all — a client deleted or renamed in the IdP — therefore
  reported `auth_unexpected_error` (502), which reads as an outage of the IdP.
  It now answers `auth_config_error` (500), the code whose catalogue entry in
  `c_auth_bff.h` already said *"BFF misconfigured (bad client_id, ...)"*.

  BREAKING for a client that switches on the code: this case moves from
  `auth_unexpected_error`/502 to `auth_config_error`/500. Both are already in
  the published catalogue, and no SPA in this repo branches on either.

## 7.9.5

### Security

- **llhttp 9.4.3** (vendored, `kernel/c/gobj-c/src/llhttp*`). Upstream fix:
  *do not allow an empty transfer-encoding*. The parser now rejects a request
  whose `Transfer-Encoding` header is present but blank, instead of accepting
  it, which is the class of ambiguity that request smuggling is built on.
  `c_prot_http_sr` parses requests from the network, so this reaches every
  yuno that serves HTTP.

  The four vendored files were pristine 9.4.2 with no local modification, so
  9.4.3 is a verbatim drop-in. Only the generated state machine and the
  version constant change: `api.c` and `http.c` are byte-identical between
  the two releases. Verified with a clean SDK rebuild and the full suite,
  119 of 119 passing, `test_c_llhttp_parser` included.

### Changed

- **maplibre-gl 6.1.0.** No breaking change, no worker or bundling change, so
  the v6 worker handling stays as it is: the bundle still emits
  `maplibre-gl-worker.js` with a `.js` extension, which is what keeps the MIME
  type servable. Worth having are the renderer fix for raster tile sources
  with errored tiles, the terrain resource leak when switching configurations,
  and the tile/image race with an undefined `AbortController`.

  `gui_treedb` pins `^6.1.0`, because an application takes the floor it was
  tested against. `gobj-ui` keeps its peer range at `^6.0.0`, because a
  library must not force the update on estadodelaire, hidraulia or wattyzer
  for fixes it does not itself depend on.

### Added

- **`@yuneta/gobj-js` **7.9.5**: the `machine` trace is back, aligned with
  `gobj.c`.** The JS port
  had the trace lines written but disconnected — `tracea` came from a yuno attr
  and the calls in `gobj_change_state`, start/stop and create/delete were
  commented out — so the runtime that the browser SPAs are built on could not
  answer *"what happened?"* the way a node does. The C kernel's level model is
  now in place, with the **same names and the same bits**:
  `gobj_set_global_trace("machine", true)`, `gobj_set_gclass_trace(...)`,
  `gobj_set_gobj_trace(...)`, plus the no-trace veto by SOURCE, the union of
  global|gclass|gobj, and `timer`/`timer_periodic` firing for their own event
  only. Read it with `set_log_callback()`. See gobj-js's own CHANGELOG.

  Fixed on the way: `log_error` / `log_warning` reached for `window.console`
  directly, so **any error logged outside a browser threw `ReferenceError`** —
  the failure path replacing the failure it was reporting.

  Published to npm as a patch ahead of the SDK; consumers on the registry
  (wattyzer, estadodelaire, hidraulia) get it by bumping their range.

- **The Spanish artifact of `/navigation` is in the repo**, beside the page it
  translates (`docs/doc.yuneta.io/navigation/artifact/`), with the script that
  assembles it: content, the demos' CSS from the English page, `demos.js` with
  its strings translated, and gobj-js's IIFE build inlined — an artifact serves
  no sibling files. `deploy.sh` excludes `artifact/` from the install, since it
  is source for a page published elsewhere, not part of this site.

### Fixed

- **`gui_treedb` talks to the shell through `yui_shell_of()`, not through its
  parent.** `ac_child_selected` (mirror the selected topic into the url) and
  `ac_remove_conn` (the confirm dialog) took the parent to be the shell, which
  only holds while a view hangs off a route the shell itself declares — under a
  `C_YUI_NODE` tree the parent is the NODE, with no `use_hash`, no `item_index`
  and no `EV_ROUTE_REQUESTED`. Latent here (gui_treedb mounts on declared
  routes); it is what actually broke yunovatios, whose treedb views moved under
  a node tree. The `EV_ROUTE_CHANGED` subscription keeps using the parent on
  purpose — a subscription goes to whoever PUBLISHES — and its variable is now
  named `host` to keep the two apart.

### Tools

- **`scripts/check_ste.py` — a linter for the documentation's English.** The
  docs are written to the structural rules of ASD-STE100: short sentences, one
  word one meaning, simple tenses, active voice, condition before command. Most
  of those rules need judgement, but a useful subset does not, and that subset
  is what this reports: contractions, semicolons, the banned modals
  (`should`/`would`/`may`/`might`/`could`), British spellings, Latin
  abbreviations, perfect tenses, phrasal verbs and the usual filler.

  It reads markdown table cells, which is where reference material lives, and
  it skips the Untouchables — code fences, inline code, link targets, URLs and
  double-quoted text, that last one because a quoted log line or command
  description is quoted material under rule 8.6 and must stay exact. Quote
  state carries across a line break, so a quotation that wraps is not scanned
  as prose. `--summary` gives one line per file, `--rule <id>` filters to one
  category, and `--strict` adds the judgement calls that are off by default: a
  possessive is legal when it is correct, and "just" is filler in "just run it"
  but temporal in "the yuno you just built". Exit 1 when anything is reported,
  so it fits a pre-commit hook.

  A clean run is not compliance. It is a spell-checker for the rules that need
  no judgement, and it says so.

### Documentation

- **The declarative shell has a public demo: [demo.yuneta.io](https://demo.yuneta.io).**
  The `gobj-ui` test-app was only reachable at `niyamaka.com`, a domain whose
  name says nothing about Yuneta, and no page linked to it. It now has its own
  host on the documentation box, with its own certificate, and
  `doc.yuneta.io` links to it under *See it run*. It shows every `C_YUI_NAV`
  layout and the per-zone responsive model, with no backend and no login.

  The script that deploys it was untracked, listed in `.git/info/exclude`, so
  nobody could reproduce the deployment from a clone. `test-app/deploy.sh`
  replaces it: the host is an argument, `demo.yuneta.io` is the default,
  `niyamaka.com` stays for mobile testing, and it curl-verifies the result
  instead of reporting success on the rsync alone.

- **`/guide-folders`: the `tools` entry linked to the CHANGELOG.** The list of
  top folders links each name to its section below, and `[tools](#tools)`
  resolved to `/changelog#tools` instead — a reader who clicked `tools` left
  the guide entirely. `#tools` had no owner on the page as far as myst was
  concerned, so it matched an implicit heading elsewhere in the site and took
  the first one it found.

  The file already carried the answer: its `yunos` entry is `(folders-yunos)=`,
  renamed at some point for the same reason. `tools` is now `(folders-tools)=`
  and resolves on the page. myst reported this as *"Linking 'tools' to an
  implicit heading reference"*, which reads like a style note and is why it
  survived so long.

- **The whole documentation set is rewritten in Simplified Technical English**
  — 194 files: `docs/doc.yuneta.io/**` and the eleven onboarding chapters under
  `yunos/c/yuno_agent/`. The content did not change. Every fact of the previous
  text survives, and the sentences that carried it are shorter. `check_ste.py`
  reports zero on all of it.

  Two defects turned up that were not style. `DEBUGGING.md` §1 announced "Two
  destinations" above a diagram of four, with a sentence below it that already
  said "all four destinations". And `deploying-yunos.md` and `NODE_SEALING.md`
  both buried a node-wide SIGKILL under the command block that causes it, so a
  reader who followed a recipe top-down had already killed the node before
  reading the warning. Both now carry a CAUTION above the commands, which is
  the rule for a safety instruction: the risk level first, then the command,
  then the result.

  What changed in the prose. British spellings became American. Contractions
  are expanded. `should` is gone where it stated a requirement, because a
  reader treats it as optional. Each concept keeps one term, so `enable` and
  `disable` replaced a rotation of `turn on`, `activate`, `arm` and `switch`.
  Semicolons became sentences, phrasal verbs became plain verbs, and
  `e.g.`, `i.e.` and `etc.` are written out. The alt text of every diagram was
  re-punctuated too: a screen reader reads it as continuous prose, and every
  one of them was a run-on joined by semicolons.

  Untouched throughout: commands, code blocks, identifiers, quoted log lines
  and quoted source strings. The command description
  *"WARNING: Don't use in production!"* keeps its contraction, because it is
  the literal string in `c_agent.c`.

- **The landing's live trace INDENTS, like the kernel's.** Every line sat at
  column zero, so the one thing a machine trace exists to show — that this
  event was fired from inside that action — was invisible. `tab()` in `gobj.c`
  writes 2 spaces per level of `__inside__`; the panel now does the same, and
  the sequence carries the two publications `C_TCP` really makes
  (`EV_CONNECTED` / `EV_DISCONNECTED` to its iogate) so there is a nested level
  to see. The state chips keep tracking `C_TCP`: a nested line belongs to
  another gobj and leaves them alone.

- **`@yuneta/gobj-js` **7.9.6**: `tab()` indents 2 spaces per level, like
  `gobj.c`** — it was `2n - 1`, one short at every level, and its floor was
  zero where the C version's is one, so a JS trace read beside a node's did not
  line up. Published to npm.

- **`/login-flow`: the transport controls moved INSIDE the graph box**, under
  the canvas. They drive what the canvas shows, and sitting below the stage
  meant reaching past a screenful of text to pause the thing you were watching.
  The row of twelve numbered step buttons is gone with them: it was longer than
  the rest of the bar, and the arrows already walk the sequence. In its place,
  `5|12` between the arrows — the number belongs to what the arrows move, and
  `← 5|12 →` is one glance — with a fixed width and tabular figures so a number
  changing twelve times a cycle does not shove the `→` button. Speed is a
  setting rather than transport, so it wraps to its own row: on a phone the bar
  reads as two lines, on a desktop as one.

- **`/navigation`: the three mechanisms are live, and they run on gobj-js.**
  Each demo is a real GCLASS on the real runtime (the 7.9.4 ES build ships next
  to the page): a card click sends `EV_OPEN_CARD`, the tree sends `EV_GO` and
  `EV_SET_NAV_MODE`, the pager sends `EV_PUSH_PAGE` / `EV_POP_PAGE`, and a
  panel under each demo shows which event landed in which state. A page whose
  argument is *a click IS an action* had no business making it out of DOM
  callbacks. The `nav_mode` demo is the one that earns its keep: the same tree
  at the same depth, redrawn as stacked strips, a single `← parent`, or one
  breadcrumb — the table's three rows, live.

  The panel under each demo is **the kernel's own `machine` trace**, not a log
  the page writes: `gobj_set_global_trace("machine", true)` +
  `set_log_callback()`, the same two calls a yuno makes on a node. That is what
  the gobj-js fix above was for — the trace did not exist when the demos were
  written.

- **New walkthrough: `/navigation` — "Getting back".** Navigation in a gobj-ui
  app is one decision — *where the reader's position lives* — and the page is
  built on that axis: the url (`C_YUI_NAV`, cards + subpath), the node tree
  (`C_YUI_NODE`, one declared route and free depth), and the pager's in-memory
  stack (`C_YUI_PAGER`). It exists because `stack` / `back` / `path` read like
  three ways to navigate when they are the three values of `nav_mode`, and all
  three live **inside** the url: they choose how the way back is drawn, not
  where the position lives. Taking `"stack"` for the pager's stack inverts the
  one distinction that matters.

- **`/login-flow`: the player was stuck on step 1 for anyone with "reduce
  motion" on.** `restart()` returned before scheduling anything when
  `prefers-reduced-motion: reduce` matched, so `playing = true` had no effect
  and Play did nothing — the walkthrough only moved with the arrows. Honouring
  the preference means not animating a packet along a wire nobody asked to
  watch; it does not mean refusing to turn the page the reader pressed Play to
  turn, so the step now advances on a timer with no travel.

- **`/login-flow`: the graph fits a phone.** Below 700 px it is drawn turned —
  the same topology with its axes swapped, its five columns spread as five
  rows — instead of keeping the 700 px floor and letting the container scroll
  sideways, which on a phone meant watching a third of the graph while the
  packet animated off-screen.


## 7.9.4

Ships with `@yuneta/gobj-js` **7.9.4** and `@yuneta/gobj-ui` **5.4.0**.

A kernel fix that had been paid for three times at the call site, and the
three ways of showing depth in a node tree become one runtime knob.

### Fixed

- **`gobj_destroy()` now actually stops the gobj it complains about**
  (`kernel/c/gobj-c/src/gobj.c`, and identically in `kernel/js/gobj-js`).
  Destroying a live gobj is the caller's bug and the kernel has always said so
  out loud — *"Destroying a RUNNING gobj"*, *"Destroying a PLAYING gobj"* —
  and then tried to repair it with `gobj_stop()` / `gobj_pause()`. That repair
  could never run: `obflag_destroying` was raised **first**, and both entry
  points refuse a destroying gobj. That refusal is correct and stays — nobody
  OUTSIDE may stop something already being dismantled — but it meant the
  rescue died on its own guard, logging a second, misleading *"hgobj
  destroying"*, and `mt_stop` / `mt_pause` never ran: the gobj was taken apart
  still holding its timers, subscriptions and children.

  The pause/stop now happen **before** the flag goes up, so `mt_stop` sees
  exactly what an orderly stop sees. That is not cosmetic ordering: a gclass
  that stops its children in `mt_stop` goes through `gobj_stop_children()`,
  which carries the same guard — with the flag already up, the whole subtree
  would have stayed running. Both complaints now also carry
  `LOG_OPT_TRACE_STACK`, since the useful information is *who* destroyed a
  live gobj.

  The fix at the call site is unchanged: `gobj_stop_tree()` before
  `gobj_destroy()`. What changed is that the framework no longer pretends to
  repair it when you forget. The trap had been diagnosed and fixed at the
  caller at least three times in the JS GUIs alone. New unit tests in gobj-js
  (`tests/destroy_stops.test.js`) pin the order and fail against the old one.

- **`scripts/check_doc_line_refs.py --repin` reaches the site's "Current
  version" stamp.** The regex required a path after the tag, so a link to the
  repo ROOT (`/tree/7.9.2`) never matched — and that link is the front page's
  version stamp, the most visible version number the site has. It was left
  behind on every release and corrected by hand, which is the exact failure
  mode the script exists to remove. The path is optional now, and a link whose
  TEXT is just the tag gets the text swapped too.

### Changed

- **`@yuneta/gobj-ui` 5.3.2 → 5.4.0: `nav_mode`, the three shapes of a node
  tree as one knob.** A `C_YUI_NODE` tree can be asked for stacked strips
  (`"stack"`, the default and what it declares), a single `← parent`
  (`"back"`) or the trail as one breadcrumb (`"path"`) —
  `yui_node_set_nav_mode(root, mode)` at runtime, or `"nav_mode"` in the
  root's declaration. All three shapes were expressible before, but only by
  rewriting the declarations: `"back"` is not a root-level edit (projections
  do not inherit, so every branch had to be rewritten) and going back was
  lossy, since restoring "stacked" meant imposing a canonical shape on
  branches that had declared their own. A mode is now a **filter applied when
  the renders are asked for**, so `"stack"` is an exact restore, per branch.
  The knob is per tree, so an app can run one section as a breadcrumb and
  another as a backbar.

  With it, the rule that was missing from the docs: **chrome belongs to the
  node that declares it, so every branch declares its own.** The library was
  already right — a node's backbar goes back to *its* route — but the README's
  own example showed the shape that breaks it (the pair on the root, a child
  with only an `index`), and an app that copies it ends up with a single ←
  that reads "← root" at every depth. 5.3.3 also fixed a `link` node's viewer
  being sized by its own content instead of by the body.


## 7.9.3

Ships with `@yuneta/gobj-js` **7.8.7** and `@yuneta/gobj-ui` **5.3.2**.

Two `auth_bff` fixes finish the session-restore path opened in 7.9.1, and the
UI library turns navigation into a tree of gobjs.

- **`auth_bff`: `/auth/refresh` answers with the identity too.** It returned
  only `{success, expires_in, refresh_expires_in}`, which made session restore
  impossible to finish: the tokens are httpOnly, so after a reload JavaScript
  has no other way to learn who the user is — the SPA came back authenticated
  but anonymous, avatar on `?`, until the next full login. `username` and
  `email` are already decoded from the access token for every action, so this
  only ADDS fields.

- **`auth_bff`: one `error_code`, one HTTP status.** The discovery-drain path
  added in 7.9.1 answered `503` with `auth_service_unavailable`, but
  `c_auth_bff.h` pins that code to `502` and `503` is already `server_busy`.
  Clients branch on the status as well as the code, so one code answering with
  two statuses is a defect however transient both happen to be.

- **`@yuneta/gobj-ui` 5.2.1 → 5.3.2: navigation becomes a tree of gobjs.**
  `C_YUI_NODE` lets a section declare its own subtree, so depth stops being a
  flat route table: a node projects its children as an `index` or as `chrome`
  (tabs, cards), and the new `projection.path` adds a third mode — a breadcrumb
  drawn from the tree ROOT, for branches where one strip per level becomes a
  wall. `yui_node_set_chrome_depth()` makes that cap reachable at runtime,
  which the config could already declare but the API could not change. The
  shell root is itself a node now (`config.shell.tree`).

  Around it: the **site map** became a navigation PANEL instead of a transient
  overlay — it joins the window manager when the app has one, draws each
  subtree once (a route reachable from three surfaces repeated its whole
  branch three times), and hides reference rows behind a toggle without ever
  emptying a menu. `remember_section_position` (opt-in) returns a menu click
  to where the reader was inside that section, and `C_YUI_JSON` grew depth
  guides. Indentation is **four spaces** wherever structure is shown as
  indentation, rendered trees indenting in `ch` so the guides stay lined up
  with the text at any zoom.

  The same line shipped broken twice, for a reason worth recording:
  `keep_on_navigate` was verified only in an app **with** a window manager,
  where a dock-managed window registers no overlay at all — the one
  configuration in which the flag is not used. 5.3.1 fixed a different bug on
  it (`gobj_find_service()` answers `undefined`, not null, and handing that to
  a `DTP_POINTER` attr fails the whole kw), 5.3.2 the flag itself, and
  `_qa_routing` now drives the shell contract directly instead of the app.

  Also in 5.2.1: clicking a node in the schema graph threw
  `ReferenceError: gobj_send_event is not defined` — the module published
  `EV_NODE_CLICK` with it and never imported it, breaking the schema landing in
  every consumer.

- **`yunos/js`: the console dump indents with four spaces**, the last
  two-space `JSON.stringify` left in either SPA. Both also carry the site map's
  two new i18n keys: the library translates through the APP's i18next, so a key
  missing there renders as the key itself, in lower-case English, and never
  changes language.

- **Docs: the 7.9.2 packaging incident is written up at `/package-transition`**,
  with `verify-package-transition.sh` — a harness that reproduces the delete
  with real dpkg in a temp root and then runs the SHIPPED hooks against a fake
  one. The login exchange is a running graph at `/login-flow`.


## 7.9.2

**Install this instead of 7.9.1.** On a node coming from **7.9.0 or older**,
7.9.1 *deletes* the web server configuration it was meant to protect. If a
node already runs 7.9.1 it is past that transition and is not affected.

- **The upgrade no longer DELETES the configuration it stopped shipping.**
  7.9.1 removed `nginx.conf` from the payload so an upgrade could not
  overwrite it. It cannot — but dpkg and rpm **delete** files that an upgrade
  no longer provides, so the very transition that fixed the overwrite removed
  the operator's config instead. It took down the GUIs on two nodes, and the
  two managers failed differently:

  - **deb** — dpkg dropped the file, then `postinst` seeded the stock default,
    so nginx served a configuration with no vhosts.
  - **rpm** — `%post` runs *before* the old package's files are erased, so the
    seeding was skipped and the erase left **no** `nginx.conf` at all; nginx
    would not even start.

  The configuration is now saved before the manager can touch it and restored
  afterwards: a new `preinst` (deb) / `%pre` (rpm) copies it to
  `nginx.conf.pkgsave`, and the restore prefers that copy over the stock
  default. On rpm the restore moved from `%post` to **`%posttrans`** — the
  only hook that runs *after* the erase; anything `%post` writes is wiped
  moments later.

  Both webservers are covered, `nginx` and `openresty`, each keeping its own
  file. Verified with real dpkg (the delete on upgrade is reproducible), then
  by running the **shipped** `preinst`/`postinst` and `%pre`/`%posttrans`
  against a bind-mounted `/yuneta`, for the upgrade case and the fresh-install
  case.

- **`preinst` shipped non-executable.** The blanket
  `find DEBIAN -type f -exec chmod 0644` re-flattened it after its own
  `chmod 0755`, and `dpkg-deb` refuses a maintainer script that is not
  executable — the package would not build at all.


## 7.9.1

A recovery release: after a machine reboot, a node came back with its login
wedged and stayed that way. Three defects were behind it, at three layers.

- **`c_prot_http_cl` sent `EV_DROP` as a string literal, so the event never
  matched.** Events are compared by POINTER identity (`_find_event_action`),
  never by text, so the interned symbol and a `"EV_DROP"` literal are two
  different things. `ac_timeout_inactivity` used the literal, which produced
  the misleading *"Event NOT DEFINED in state / C_TCP / ST_CONNECTED /
  EV_DROP"* — naming an event the table does declare — and left the outbound
  to an unreachable peer never dropped. Present since `80e6f7ad3` and in
  7.8.7 too: it only bites once the inactivity timeout actually fires, which
  is precisely what an unreachable IdP causes. It was the only such literal in
  the tree.

- **`c_auth_bff` now bounds the wait for an IdP it cannot reach, and retries.**
  `C_TASK`'s `exec_timeout` does not cover this: it is armed inside
  `execute_action`, and `execute_action` only runs once the channel is
  connected (`c_task.c`, `mt_start`). While the IdP is unreachable the task
  waits for a connection that never arrives — unarmed and unbounded — so no
  `EV_END_TASK` is ever published, `discovery_done` stays FALSE, and every
  browser request queues behind it forever with nothing logged. A reboot
  produces exactly that: the yuno starts before the network is usable.

  The BFF now owns the deadline, since it is the one with browsers waiting:

  - a `C_TIMER` watchdog over the **connect gap only**, with its own budget
    (`idp_connect_timeout_ms`, default 30 s). `idp_timeout_ms` keeps timing the
    round-trip once connected — conflating the two failed an IdP that is merely
    slow to appear. `ac_on_open` disarms the watchdog the moment the outbound
    connects, so the two never time the same thing twice.
  - the stuck task is failed through its **own** path (`EV_TIMEOUT` →
    `stop_task(-2)`), so the existing `EV_END_TASK` handling applies unchanged:
    504 for a request, a drained queue for discovery.
  - a failed discovery is no longer terminal: `process_next` re-arms it on the
    next request, so recovery rides on a retry rather than on a background
    timer — there is nothing to poll for, the endpoints are only needed at
    login.
  - queued requests are answered **503 `auth_service_unavailable`** instead of
    being left on a socket that will never produce bytes.
  - on fire, a connect that burned its whole budget is aborted, so the next
    attempt starts fresh instead of waiting out the kernel's SYN ladder
    (`tcp_syn_retries=6` → ~127 s); and when a request needs an outbound that
    sits disconnected, it is nudged with `EV_CONNECT` — `c_tcp`'s own
    on-demand entry point — instead of waiting out a backoff already grown to
    its 30 s cap.

  Verified against a blackholed IdP: login answered 503 in 22 s instead of
  hanging, back to 200 in 0.6 s once the IdP returned **without restarting the
  yuno**, 0.3 s in steady state.

- **The `.deb`/`.rpm` no longer overwrite the node's web server
  configuration.** The packagers stage `/yuneta/bin/nginx` wholesale, and
  `conffiles` / `%config(noreplace)` only cover `/etc`, so every upgrade
  replaced the operator's `nginx.conf` — with the stock one built in CI — and
  shipped the build machine's `conf.d/` on top. `%files` lists `/yuneta` as a
  directory, so the file cannot even be tagged `%config` (rpmbuild: *"file
  listed twice"*). Both packagers now **strip** `nginx.conf` and `conf.d/`
  from the payload — a file the package does not contain cannot be replaced —
  and `postinst`/`%post` seed `nginx.conf` from the pristine
  `nginx.conf.default` only when there is none. Verified on a built `.deb`.

- **CI: the packaging actions moved onto the Node 24 runtime.** Every run of
  `release-packages.yml` carried the annotation *"Node.js 20 is deprecated …
  being forced to run on Node.js 24: actions/checkout@v4,
  softprops/action-gh-release@v2"*. GitHub was compensating; when it stops, both
  steps fail, and since this is the repo's **only** workflow that means a
  release with no `.deb` and no `.rpm` — found while cutting one. Bumped to the
  latest majors that declare `using: node24` (`checkout` v4→v7,
  `action-gh-release` v2→v3); neither breaking change touches this workflow.
  Verified with a `workflow_dispatch` against a throwaway tag — **not** against
  `7.9.0`, whose packages must keep matching the tagged tree — both jobs green
  and zero Node 20 annotations.


## 7.9.0

Ships with `@yuneta/gobj-js` **7.8.7** and `@yuneta/gobj-ui` **5.2.0**.

The headline is a **BREAKING** change in the agent: `delete-yuno` no longer
deletes a whole yuno by omission. See the entry below for why the old default
was the wrong way round.

- **BREAKING (agent): `delete-yuno` no longer deletes the whole yuno by
  omission — it requires `whole=1`.** Without `yuno_release=` the command
  targeted the in-memory primary, i.e. the yuno *and every release behind it*.
  That put the destructive reading behind the SHORTER command line, and the
  safe one behind the longer: an operator reaching for "drop this release" and
  forgetting the parameter got "drop the yuno". It has already cost a realm its
  `auth_bff`, with `delete-yuno` cascading onto the config row.

  The three forms are now explicit:

  ```
  delete-yuno id=<id>                      # refused, with both options named
  delete-yuno id=<id> yuno_release=<rel>   # one release
  delete-yuno id=<id> whole=1              # the yuno and all of its releases
  ```

  `force=1` does **not** stand in for `whole=1` — it bypasses the snap-tag
  guard, a different question, and its help line now says so. Nothing in the
  tree called the bare form (not the `yunetas` CLI, the sync tools or the agent
  SPA); the callers were operators following `REALMS.md` and
  `YUNO_LIFECYCLE.md`, both updated.

- **`create-yuno` / `delete-yuno` / `find-new-yunos` help lines name what they
  actually do.** All three act on a yuno *or* on one of its releases, with a
  parameter as the discriminator, and the old one-liners ("Create yuno",
  "Delete yuno") hid it — the flow to adopt a new binary/config version is
  `find-new-yunos create=1` + `deactivate-snap` (bundled as `yunetas
  upgrade-yunos`), and reading `create-yuno`/`delete-yuno` as a symmetric pair
  is what leads to inventing a destructive substitute for it.

  Also dropped three alias arrays that pointed each of those commands at its
  own name: they add nothing to `command_get_cmd_desc` (which matches `name`
  first whenever the command has a `json_fn`) and surfaced in the help JSON as
  an alias identical to the name, advertising a spelling that does not exist.

- **`delete-user` (C_AUTHZ) can now delete a role-holding user, and reports
  its result truthfully.** Two problems fixed together: (1) the command always
  returned the comment `"User deleted"` while passing `gobj_delete_node`'s code
  through verbatim, so a failed delete surfaced as `ERROR -1: User deleted`;
  (2) it could not delete any user linked to a role, even though roles are not
  a deletion boundary — a local password user (e.g. an MQTT/IoT gate identity)
  can hold roles too, and the seed admins are protected by **immutability**,
  not by having roles. `delete-user` now: checks immutability up front and
  refuses cleanly (force-proof, no session reject); refuses a role-holding
  user unless `force=1` is passed (with `force` it unlinks the roles first,
  via `gobj_delete_node`'s force option); and returns an accurate result in
  every path. `pm_delete_user` adds the `force` boolean. A new regression test
  `tests/c/command_delete_user/` drives a real C_AUTHZ over a temp store and
  covers all four cases.
  This also fixes a latent **use-after-free** in `cmd_delete_user`: it passed
  the user *node* to `EV_REJECT_USER` (whose handler reads `username` and frees
  the kw) and then reused that freed node in `gobj_delete_node` — a double
  consume that also made session-rejection a silent no-op (the node keys on
  `id`, not `username`). It never crashed in production (freed-but-unreused
  memory) but the new test caught it under `CONFIG_DEBUG_TRACK_MEMORY`.
  `EV_REJECT_USER` now gets its own `{username}` kw, so it actually kicks
  sessions and `gobj_delete_node` owns the single node ref.

- **`dns_warn_due()` (`static_resolv.c`) silences `-Wformat-truncation`.** The
  helper took a `const char *ns` GCC could not prove NUL-terminated within its
  46-byte source array, so `snprintf("%s")` worst-cased a read spanning the
  whole `char[3][46]` into the 46-byte slot. `parse_resolv_conf()` already
  bounds every entry, so no real truncation was possible; the copy is now
  precision-capped (`%.*s`) so the bound is provable. No behaviour change.

- **Scaffolding: new projects and yunos get a `CHANGELOG.md`, not a
  `CHANGES.txt`.** The `yuno-skeleton` templates (`c_project`, `yuno_citizen`,
  `yuno_standalone`) emitted a bare `CHANGES.txt` that nothing in the toolchain
  read and no project kept up. Every repo in the ecosystem keeps a
  Keep-a-Changelog `CHANGELOG.md` instead, so the skeletons now start one.

- **`is_service_authorized()` no longer trusts every `authorized_services`
  entry to be a string.** The list is filled by `ac_identity_card` from
  `json_object_foreach` keys, so entries are always strings today — but a
  non-string entry would have made `json_string_value()` return NULL and
  `strcasecmp(NULL, …)` crash the yuno. A non-string entry now logs a
  `MSGSET_INTERNAL` error (broken invariant) and is skipped.

## 7.8.7

A release the toolchain asked for. Ubuntu 26.04 brought gcc-15 and clang-21,
whose `<string.h>` hands back a `const char *` where the code expected a
writable one, and the fourteen warnings that surfaced pointed at one place
that really was editing memory it did not own. Following that thread through
the inter-event layer turned up four identity checks that had drifted apart
from each other and from how the framework matches names everywhere else.

- **The ievent identity checks agree with each other, and with the framework.**
  `dst_role` and `dst_yuno` were checked in four places that all disagreed.
  `C_IEVENT_CLI` compared only the part before a `^`, and it obtained that part
  by writing a NUL over the `^` and restoring it after `strcmp()` — over a
  buffer owned by jansson, since `kw_get_str()` returns `json_string_value()`.
  `C_IEVENT_SRV` did no `^` handling at all, and compared case-insensitively
  while greeting (`identity_card`) but case-sensitively for every message after
  it, so a peer could pass the handshake and be rejected by its first event.

  Yuno, role and service names are a case-insensitive namespace — `gobj.c`
  registers services through `strntolower()` and matches names with
  `strcasecmp()`. All four checks now do the same, the `^` handling is gone
  (nothing in the tree ever put a `^` in `dst_yuno`; it came from the pre-v7
  wire format), and no one edits a jansson string any more.

- **The cross-service authorization gate stops rejecting on letter case.**
  `is_service_authorized()` matched the names in `authorized_services` — which
  come from the roles written in the treedb — against the live service with
  `strcmp()`, while `gobj_find_service()` resolves them lowercased. A role
  granting `TreeDB` therefore resolved the service and then failed the gate.
  It now uses `strcasecmp()`, like the rest of the naming. This only turns
  false denials into the access the role already granted; it cannot widen a
  grant, since the name still has to be in the list.

- **The string helpers stop discarding `const`.** glibc's `<string.h>` now
  declares `strchr`/`strrchr`/`strstr` as the C23 const-generic macros, so on a
  `const char *` they return `const char *` — and thirteen call sites that
  stored that in a `char *` started warning under gcc-15 / clang-21. All of
  them only did pointer arithmetic or printing, so nothing changed at runtime;
  they are now declared `const char *`. The one site that does write through
  the pointer (`gobj_search_path()`, splitting `gclass^name`) keeps an explicit
  cast, since the string it edits is `split2()`'s own heap copy.

- **`install.sh` refreshes the apt index before installing the `.deb`.** It
  never did, and a freshly imaged Debian node carries whatever index its image
  was built with. Debian keeps only the current version of each package in the
  pool, so apt asked for superseded files and got 404s — `rsync
  3.4.1+ds1-5+deb13u1` while the mirror already served `+deb13u4`,
  `libpython3.13 3.13.5-2` against `3.13.5-2+deb13u3` — the dependencies went
  unmet and `yuneta-agent` was left unpacked but unconfigured. **Re-running the
  installer did not recover**: nothing in it refreshed the index, so the node
  stayed stuck until someone ran `apt update` by hand. Two nodes installed
  minutes apart from the same release differed only in how old their image was.
  (This one reaches every node immediately: `install.sh` is fetched from `main`,
  not from a release asset.)

## 7.8.6-4

Another packaging revision — `YUNETA_VERSION` stays at 7.8.6, only `RELEASE`
moves. Everything here is about what an operator is told when something goes
wrong: a clean install on a *fast* Debian 13 dedicated server failed to install
certbot and reported three symptoms and no cause, and a Rocky node reported its
firewall work in words that sent the reader to the command that says
"FirewallD is not running".

- **The Debian certbot helper stops hiding why it failed, and retries.** It ran
  `snap wait system seed.loaded 2>/dev/null || true` and `snap install core ||
  true`: the one safeguard against snapd's start-up race, silenced, plus the
  first two real errors swallowed — so a failed run showed only the third line
  and no cause. snapd restarts itself right after being installed, and a store
  request in flight when that happens dies with `context canceled`, which reads
  like a network fault and is not one. The seed wait is now reported (and
  bounded at 180s, since `snap wait` has no timeout of its own and a broken
  snapd would hang the installer), the installs retry three times, and a
  genuine failure prints what each error class means plus the commands that
  tell them apart — `uname -r`, `snap debug sandbox-features`, `snap changes`,
  `journalctl -u snapd`.

  Both helpers now also document how to add a DNS-01 provider plugin, which
  neither installs. On RHEL it is one `dnf install
  python3-certbot-dns-<provider>`. On Debian the plugins are separate snaps and
  the first command is the one nobody guesses — `snap set certbot
  trust-plugin-with-root=ok` — without which the install dies complaining about
  trusting the plugin author, inside a box snap prints in a way that is easy to
  miss entirely.

  It also warns instead of staying quiet when `/usr/bin/certbot` is a real file
  rather than the snap symlink. **Debian's own `certbot` package is not a drop-in
  replacement**: it has no `dns-ovh` plugin (the archive ships cloudflare,
  desec, google, infomaniak, rfc2136, route53 and standalone only), so a node
  whose certificates carry `authenticator = dns-ovh` cannot renew them with it —
  and installing it takes over `/usr/bin/certbot` and adds a second renewal
  timer beside snap's, both aimed at the same `/etc/letsencrypt`.

- **The `.rpm`'s firewall message names the branch it took, and the command
  that checks it.** It said `opened …` whether the ports had been applied to a
  running firewalld or written to the permanent config of one that has not
  started yet. An operator reading that reaches for `firewall-cmd
  --list-ports`, which talks to the *daemon*: on a fresh node — firewalld
  enabled but not started, which is the normal case right after installing —
  that answers *"FirewallD is not running"* and reads as a failure when nothing
  failed. Each branch now says what it did and points at the matching
  `firewall-cmd` / `firewall-offline-cmd` verify command.

## 7.8.6-3

A packaging revision, not a new version of Yuneta: no source under `kernel/`,
`modules/`, `utils/` or `yunos/` changed, so `YUNETA_VERSION` stays at 7.8.6
and only the `RELEASE` counter moves. The packages are rebuilt as
`yuneta-agent-7.8.6-3` and attached to the existing 7.8.6 tag.

Revision 2 was withdrawn without being announced: its `.deb` still came off
the `ubuntu-22.04` runner, so the archives it ships were stamped glibc 2.35
while Debian 13 runs 2.41. `libc_guard.cmake` compares the two as an exact
string, so that package installs and runs but leaves the node unable to build
anything against the SDK it just dropped there. The number is burned rather
than reused: two different payloads must never share one version string.

- **The AMD64 `.deb` is built on Debian 13.** It came off an `ubuntu-22.04`
  runner while the `.rpm` had already moved into a `rockylinux:9` container —
  the asymmetry that made the whole glibc-provenance rule necessary in the
  first place. Both jobs are now containers with the same step order. Dropping
  `libpcre3-dev` was part of it: PCRE1 no longer exists in Debian 13, and
  nothing links it — the build vendors PCRE2 and openresty wants PCRE2 too, so
  it only survived because it still existed on Ubuntu. `installation.md` told
  Debian users to install it, which fails on trixie.

What prompted it: clean installs on Debian 13 and Rocky 9 both finished on a
green tick while leaving something broken behind them — a node that would stop
answering at its first reboot, certificates that would not renew, and an apt
that could not tell whether the payload fit on the disk.

The two `install.sh` fixes below are listed for the record but were live the
moment they were pushed: that script is fetched from `main`, not from a
release asset.

- **The `.rpm` opens the ports the node serves.** `fail2ban` — a weak
  dependency — pulls `fail2ban-firewalld`, which pulls `firewalld`, which its
  own scriptlet **enables**. firewalld is not started during the transaction,
  so a fresh Rocky node worked right after installing and would have started
  refusing connections at the first reboot — the very thing the installer
  recommends doing. Nothing in the package touched the firewall. The `%post`
  now opens 1993/tcp (the agent's external control plane) and 80/443 (the
  bundled web server), via `firewall-offline-cmd` when firewalld is enabled but
  not yet running. A node whose ports were moved from the defaults still needs
  them opened by hand; the failure path says so instead of passing silently.

- **The `.rpm` certbot helper starts the renewal timer.** certbot's scriptlet
  enables `certbot-renew.timer` without starting it, and says so — meaning
  renewals did not begin until the node rebooted, while the Debian side gets a
  live timer from the snap in the same run. The helper printed the command to
  fix it instead of running it.

- **The `.deb` declares `Installed-Size`.** `dpkg-deb --build` does not compute
  the field — only `dpkg-gencontrol` does, and the packager writes
  `DEBIAN/control` by hand — so apt counted the package as taking zero bytes:
  a 294 MB `.deb` announced "After this operation, 49.0 MB of additional disk
  space will be used" (the sum of its dependencies alone). apt's disk-space
  check therefore passed on a box with no room for the payload, and the install
  filled the disk instead of refusing up front.

- **`install.sh` verifies the agent is running before declaring success.** The
  postinst starts the service through `invoke-rc.d` and ignores the result; on
  a systemd box the init script's output goes to the journal, so nothing about
  the start ever reached the terminal. A `curl | sh` run ended on a green tick
  whether or not anything came up. It now waits for the process, names what is
  running, and exits non-zero with where to look when the agent is not. The
  init script's own `status` exit code is unusable for this: it reports the web
  server — absent on a fresh node — not the agent.

- **The Debian developer toolchain installs `wget`.** `install.sh` promised it
  in its own header and the `.rpm` list already carried it; only the `.deb`
  list did not.

- **`install.sh` no longer makes apt drop its download sandbox.** apt fetches
  as `_apt` even from a local file and could not traverse the 0700 root-owned
  `mktemp` directory, so it fell back to an unsandboxed download and said so.
  Cosmetic, but it was noise introduced by the 7.8.6 move to `apt-get install`.

## 7.8.6

A release about the first minutes of a node's life. Installing on a clean
Ubuntu printed a wall of dependency errors and rescued itself; the two distro
families named the same helper differently; and nothing in the docs said that
the SDK a package drops on a node can only compile there when the glibc
matches — which, on Ubuntu 26.04, it does not.

- **`install.sh` installs the `.deb` with `apt-get`, not `dpkg -i`.** `dpkg`
  does not resolve dependencies, so every clean install printed a wall of
  dependency errors and left the package unconfigured until the `apt-get -f`
  fallback rescued it — `gdb`, new in 7.8.4, made this fire on every node.
  `apt-get install ./pkg.deb` resolves deps from the file directly; the `dpkg`
  path stays only as a fallback for an apt too old to take a file argument.

- **The certbot helper is `install-certbot.sh` on both families.** Debian
  shipped it as `install-certbot-snap.sh`, so operators and `install.sh` had to
  know which distro they were on to name the same helper. The old name remains
  as a symlink.

- **Docs: `installation.md` gains the glibc-provenance rule and a
  verify-a-fresh-install checklist.** The page promised that the sparse SDK
  lets projects "compile against the published runtime" without saying that
  this only holds when the node's glibc matches the one the package was built
  against — a mismatch links silently and corrupts the heap at run time. Since
  the AMD64 `.deb` is built on ubuntu-22.04 (glibc 2.35), Ubuntu 26.04 nodes
  are runtime-only.

- **`yunetas` CLI 0.18.0: `--help` is now an extended help.** It printed the
  same one-line-per-command summary that bare `yunetas` already shows, so the
  flag told you nothing you had not just seen. It now documents every command
  and option, grouped by job. Bare `yunetas` keeps the compact listing and
  `yunetas <command> --help` keeps click's rendering.

- **Docs: the CLI page documents the node registry and secret overlays.** Both
  shipped in 0.15/0.16 and the command map still listed neither.

## 7.8.5

A release about where things live. The deploy tooling was split across two
release channels — the CLI on PyPI, the Python tools inside the packages — and
the halves drifted until a `pipx upgrade` alone could break a deploy. They are
one package now. The `.rpm` likewise stops being built on Ubuntu, and configs
stop carrying credentials into git.

- **The Python deploy tools move into the CLI.** `sync_binaries`,
  `sync_configs` and `set_start_priorities` ship inside the `yunetas` package
  as `yunetas.agent_tools.*` (CLI 0.17.0) instead of being read from
  `tools/agent/` at run time. One tool released through two channels meant the
  halves drifted: a node found running CLI 0.14.0 against scripts from 7.8.4
  would have broken on a `pipx install --upgrade yunetas` alone, handing
  `--secrets-dir` to a script that had never heard of it. The files under
  `tools/agent/` are now deprecated forwarding shims — operator runbooks
  reference those paths — and go away in a release or two. The CLI still shells
  out to `ycommand`, so this removes the version skew, not that dependency.
  CLI 0.17.1 also fixes the dependency declaration (`typer[all]` names an extra
  typer no longer provides; `rich` is now declared, since the CLI imports it).

- **Secret overlays for config deploys** (`sync_configs.py --secrets-dir`, and
  `yunetas` CLI 0.16.0 wiring it to `~/.yuneta/secrets/<node>/`). A committed
  config declares a credential with the value `"__SECRET__"`; the value lives
  only on the deploy machine and is deep-merged in just before the push. The
  *shape* of the config stays versioned in git — which is what makes it
  reconstructable — and only the value is withheld. **Fails closed**: a
  surviving `"__SECRET__"` refuses the push rather than shipping an empty
  password, which for SMTP means an auth failure or an unauthenticated send.

  Prompted by an SMTP password committed in cleartext in a project repo. It
  does not fix that one — that is still in git history and needs the credential
  rotated.

  The merged copies hold real credentials, so they are written 0600 into a 0700
  temp dir removed in a `finally`, on SIGINT/SIGTERM/SIGHUP, and swept at the
  start of the next run. That last one is not belt-and-braces: a SIGKILLed run
  (timeout, OOM) skips every handler, and testing this left exactly such a
  plaintext copy behind.

- **`yunetas` CLI 0.15.0: a node registry**, so deploying to another machine is
  `--node <name>` instead of a url plus four OAuth2 flags re-derived from a
  config file each time. `register-node` / `list-nodes` / `unregister-node`
  back it with `~/.yuneta/nodes.json` (0600), and `--node/-N` works on `sync`,
  `sync-binaries`, `sync-configs` and `upgrade-yunos`. It stores where a node
  is and which identity you present, **never a credential** — those come from
  the environment at call time. A node registered with `--ssh` instead of
  `--url` is reached by forwarding a free local port to its loopback-bound
  agent, torn down after the command. See the deploy guide.

- **The `.rpm` is now built on Rocky 9, not on the Ubuntu runner.** Both
  packages came out of a single `ubuntu-22.04` job, on the stated assumption
  that fully static binaries run on RHEL too, so a separate EL9 build was
  unnecessary. That is true of the **binaries** and false of the **archives**:
  each package also ships `outputs/lib` + `outputs_ext/lib` for nodes that
  compile their own yunos, and those are tied to their building glibc. So the
  `.rpm` shipped glibc-2.35 archives onto EL9 nodes running glibc 2.34 — the
  same defect that destroyed an Ubuntu 26.04 node, surviving on Rocky only
  because the two versions are adjacent. Verified on a live Rocky 9 node: its
  `libyunetas-gobj.a` reported `GCC: (Ubuntu 11.4.0)`.

  The workflow is now two jobs, each building its package on its own
  distribution, and each asserts its payload's glibc stamp before packaging —
  the EL9 job additionally requires the stamp to equal the container's own
  glibc, so this cannot silently regress.

## 7.8.4

A release about a lie the build was telling. A node with a compiler could
compile its own yunos against the archives shipped in the package, the link
would succeed, and the binary would corrupt its heap the moment it ran. It
cost a day of chasing a refcount bug that did not exist — the stack traces
pointed at `kw_decref` and jansson, and both were innocent bystanders. The
build now refuses that link instead of producing the binary, and the CLI stops
reporting success when the refusal happens.

- **The build refuses to link prebuilt archives against a different glibc.**
  The packages ship prebuilt static archives (`outputs/lib`, `outputs_ext/lib`)
  built by CI on `ubuntu-22.04`, i.e. glibc 2.35. A node that has a compiler and
  project sources can compile its own yunos against them, and with
  `CONFIG_FULLY_STATIC` the link then mixes those archives with the **node's**
  static glibc. That is not portable: the archives reach into glibc internals
  such as `_dl_x86_cpu_features`, which back the ifunc resolvers selecting the
  CPU-tuned `memcpy`/`strlen`. A dynamic link fails loudly with an undefined
  reference; a **static** link resolves silently against a layout that changed
  between releases, the resolver picks a wrong routine, and the heap is
  corrupted at run time.

  The failure is brutal to diagnose because it looks like someone else's bug:
  SIGABRT/SIGSEGV inside `unlink_chunk` / `_int_free_merge_chunk` /
  `_int_malloc` a couple of seconds after start, no Yuneta error logged first,
  and a stack trace blaming whatever innocent code happened to free next.
  Found on an Ubuntu 26.04 node (glibc 2.43) where four yunos crash-looped
  ~130 times from their very first launch and had never once run.

  `tools/cmake/libc_guard.cmake` now records the building glibc next to the
  archives (`outputs/lib/yuneta_libc.stamp`, written by the `gobj-c` build and
  shipped in the package), and every other build compares it against the glibc
  of the machine doing the linking, failing at **configure** time with the
  three ways out: build off-node and ship binaries, build the whole SDK from
  source locally, or install a package built for that distribution. Archives
  older than the stamp warn instead of failing. Both packagers now refuse to
  build a package whose payload has no stamp.

  Note this guards glibc only. The compiler is deliberately *not* checked:
  rebuilding the same sources on the affected node with clang instead of GCC
  still crashed, so the compiler is not the variable — only the libc is.

- **`gdb` is now a hard dependency of the agent package** (`Depends:` in the
  `.deb`, `Requires:` in the `.rpm`). A Yuneta node is expected to produce core
  dumps in `/var/crash` (7.8.2/7.8.3 went to some length to make sure it does),
  and a core is useless on a node with no debugger: analysing one meant copying
  a multi-hundred-MB file off the node, or installing `gdb` by hand at exactly
  the moment something is already broken. It is a dependency rather than a
  recommendation because `--no-install-recommends` is common on server installs,
  which is precisely where the crash will happen.

- **`yunetas` CLI 0.14.0**: `init` no longer prints `init done` and exits `0`
  after a cmake it just watched fail. It now reports `init FAILED`, lists the
  directories, and exits `1`. The glibc guard above fired correctly on a node
  and the CLI declared success anyway, which is exactly the combination that
  makes a real error invisible. `build`'s failure path also stops using the
  bare `exit()` (absent under `python -S`) and exits `1` instead of `255`.

## 7.8.3

A packaging release, and a lesson about who else wants your kernel knobs.
7.8.2 fixed `/var/crash` losing a tug-of-war with kdump; this one fixes
`core_pattern` losing the same kind of fight to Ubuntu's `apport`. Validated
end to end on an Ubuntu 26.04 node: a deliberate `SIGSEGV` now leaves a real
core in `/var/crash`, which it did not before.


- **`apport` is disabled on `.deb` nodes, and `core_pattern` taken back**. On Ubuntu,
  `apport.service` starts *after* `systemd-sysctl.service` and overwrites
  `/proc/sys/kernel/core_pattern` with a pipe to itself, so the value from
  `/etc/sysctl.d/99-yuneta-core.conf` survived the install (the post-install
  runs `sysctl --system`) and was lost at the next boot. Since apport discards
  cores from binaries that did not come from a distro package, yuno cores then
  stopped existing with nothing logged — found on a node reading
  `|/usr/share/apport/apport …` while the Rocky node next to it still had
  `/var/crash/core.%e`. The package now ships
  `yuneta-core-pattern.service` (`After=apport.service`, ordering-only so it is
  harmless where apport does not exist) which re-applies the file, and the
  post-install warns via `logger` if `core_pattern` still is not ours — a
  generic check, so it also catches `systemd-coredump` or RHEL's `abrt`.
  The post-install now also **turns apport off** (`enabled=0` +
  `systemctl disable --now`): apport is Ubuntu's crash *telemetry* client,
  shipping deduplicated reports to `errors.ubuntu.com` for Canonical's benefit,
  and it deliberately discards crashes of binaries outside its packaging
  allowlist — `/yuneta` is not on it. On a dedicated appliance node that trade
  is all cost. **Order matters and is load-bearing**: `apport --stop` does not
  restore our value, it writes the bare word `core`, which drops dumps into the
  crashing process's CWD — so the sysctl is re-applied *after* apport is
  stopped, never before.
  Same shape as the `/var/crash` fix in 7.8.2: a one-shot setting losing to a
  later actor, so re-assert instead of fighting. **The RHEL/Rocky equivalent
  (`abrt-addon-ccpp`) is not covered yet** — it is not installed on our nodes,
  but it would do exactly the same thing.

## 7.8.2

A release about a failure that could not be seen. A node came up with a
black-holed nameserver first in `/etc/resolv.conf`; every name resolution paid
~6 s, resolution runs synchronously inside the event loop, and a yuno building
25 channels spent ~2 min 40 s in start up — long past the agent's handshake
timeout, so the agent reported it as not running while the process sat there
alive and listening. Nothing in any log said any of that. The fixes below are
in the order they were needed: a way to trace start up at all, then the fix,
then the two warnings that would have made the whole hunt a one-line grep.

- **Core dumps survive a `kexec-tools` update** (`.deb` and `.rpm`). `/var/crash`
  is **co-owned**: on RHEL/Rocky kdump's `kexec-tools` also declares it, as
  `root:root 0755`. The packages' one-shot `chmod 0775` / `chown root:yuneta` in
  the post-install therefore held only until the next transaction touching that
  package, which reverted group and mode — and then cores silently stopped being
  written, with `rpm -V yuneta-agent` reporting `.....UG.. /var/crash` for anyone
  who thought to look. A `/usr/lib/tmpfiles.d/yuneta-crash.conf` drop-in now
  re-asserts it on every boot (and immediately, via `systemd-tmpfiles --create`).
  `kernel.core_pattern` is unchanged.
- **`getaddrinfo()` reports when it blocks the event loop** (`yev_loop.c`).
  Resolution is synchronous and the loop calls it while arming a connect, a
  source bind or a listen, so a slow resolver does not delay one socket — it
  stops every gobj, timer and pending completion in the process. All three call
  sites are timed, and over 1 s they emit a `gobj_log_warning` (msgset `OS`)
  carrying the host and the elapsed `msec`, attributed to the gobj that paid
  it. This is what makes "the yuno is slow" legible as "resolving X stopped us
  for 7032 ms".
- **The static resolver leaves a trail in syslog** (`static_resolv.c`). It sits
  below the gobj log and cannot reach it, so it now writes to `syslog(3)`
  directly — a stack buffer and no allocation, unlike `print_error()`, which
  `malloc`s its message and is therefore the wrong tool for reporting that
  `malloc` failed. It reports an unresponsive `nameserver` when there is
  another to fall back to (rate limited to one per nameserver per 5 min, since
  this runs on every connect), any resolution over 1 s, an empty
  `/etc/resolv.conf`, and allocation failures that used to return a silent
  `EAI_MEMORY`.
- **The static resolver caches its DNS answers** (`static_resolv.c`, so
  `CONFIG_FULLY_STATIC` builds). Every connect used to re-query, so a yuno
  building N channels to the same host paid the full round-trip N times —
  invisible while DNS answers in a millisecond, fatal when it does not.
  A node whose `/etc/resolv.conf` listed a black-holed nameserver *first* cost
  the A and AAAA timeouts (~6 s) on **every** connect, and `yuneta_getaddrinfo()`
  runs synchronously inside the event loop, so it blocked the whole process:
  an `auth_bff` with 25 channels spent ~2 min 40 s in start up, never sent its
  `agent_client` WebSocket handshake in time, and the agent gave up on it
  (`1 raised, 0 reached`) while the process sat there alive and listening.
  With the cache the same yuno starts in ~7 s and the agent reports `yuno up`.
  Entries are held for the answer's own TTL (now parsed instead of skipped),
  clamped to 5..300 s; the table is fixed-size and allocation-free; only the
  DNS step is cached, not numeric literals or `/etc/hosts`; failures are not
  cached, so a recovered IdP is picked up at once.
  This mitigates the blast radius, it does not make a broken `resolv.conf`
  free: the first lookup still pays it.
- **`--global-trace=LEVEL` enables global traces from the command line.**
  Repeatable and comma-separated (`--global-trace=machine,create_delete`), with
  `--global-trace=list` printing the available levels. Levels are applied right
  after every gclass is registered, so they are already live when the first
  service starts — which is the whole point: until now a yuno that failed
  *before* it could reach the agent could not be traced at all, because
  `set-global-trace` travels over the very control channel that is missing. The
  fallback was `kill -10 <pid>` (SIGUSR1 cycling the global mask), which cannot
  catch anything that happens during start up. Unknown levels are rejected with
  a pointer to `list` rather than being ignored.
  `--verbose-log` is **not** a trace switch — it only overrides the stdout log
  handler's field bitmask — and its help text now says so.
- **`memlock` added to the packaged resource limits** (`.deb` and `.rpm`).
  io_uring rings are pinned memory charged against `RLIMIT_MEMLOCK`, and the
  budget is **per user**, shared by every yuno running as `yuneta`. A yuno with
  `io_uring_entries=32768` pins ~3.1 MB, so the usual 8 MB default admitted only
  two of them: from the third on, `yev_loop_create()` failed with `ENOMEM`, the
  yuno aborted at startup and the ydaemon watcher relaunched it indefinitely.
  Freshly installed nodes with gigabytes of free RAM could not bring up their
  full yuno set. The limits drop-in, the init script and `profile.d` now all
  raise it.
- **`yev_loop_create()` names the culprit on `ENOMEM`**: the warning and the
  critical now carry the ring's estimated footprint (`ring_bytes`) and the
  effective `memlock` limit, and the critical adds a hint pointing at
  `ulimit -l` and `io_uring_entries`. The warning no longer calls the condition
  *"transient pressure"* — a fixed ceiling is not transient, and the wording
  sent readers looking for a memory leak.

## 7.8.1

A bugfix release. Most of it came out of watching a node come back from a cold
machine reboot with its IdP (Keycloak) still booting — the state where half of
these paths run for the first time. Two crashes and several noisy-but-harmless
log storms were fixed, plus one refcount contract that had been quietly wrong.

- **Two crashes on the login path**: `controlcenter` dereferenced a
  not-yet-open treedb for any login arriving before it played, and `prot_tcp4h`
  re-sent `EV_RX_DATA` to itself after a synchronous publish had already dropped
  the connection. `authz`'s login publish became a veto point so a service can
  refuse a user it cannot yet register (see YUNO_AUTH.md §4.9).
- **A gbuffer double-free**: a kw carrying a serialized gbuffer must be
  refcounted with `kw_incref`/`kw_decref`, never the `json_*` pair — `c_task`'s
  lmethod forward and `c_iogate`'s broadcast both got it wrong. The rule is now
  in CLAUDE.md and the tree was swept.
- **The agent launched a slow-to-open yuno twice at boot** (and `run-yuno` could
  too), which a controlcenter lost to its treedb lock. It now marks a launch
  until the yuno registers, expiring the mark on a monotonic timer.
- **Log-noise fixes** for the IdP-down window: the bad-json cascade on ievent
  ports collapsed to one capped warning, and `auth_bff` stopped logging two
  stack traces per request when a 5xx body isn't the RFC-6749 JSON envelope.

No BREAKING changes. Ships with the same `@yuneta/gobj-js` **7.8.0** and
`@yuneta/gobj-ui` **4.0.0** as 7.8.0 — this release is C-only.

    - **fix(auth_bff): a 5xx from the IdP with a non-JSON body logged two errors
      per request.** `send_token_to_browser()` read `error` / `error_description`
      off the response body to map the RFC-6749 error envelope, but that envelope
      only exists when the IdP itself answered. When the IdP is **down**, the 5xx
      body is a reverse proxy's HTML 502 or empty, so `kw_get_dict(kw, "body")`
      returns NULL and the two `kw_get_str()` calls path into NULL —
      `kw_find_path()` logs *"kw must be list or dict"* with a stack trace, twice
      per failed request (seen as 10 of those against 5 `👤BFF server error`).
      The envelope is now read only when the body really is a dict; otherwise
      `idp_err`/`idp_desc` stay `""` and the generic `auth_unexpected_error` (502)
      mapping applies, which is the correct outcome for an IdP outage. The 5xx
      `👤BFF server error` itself is legitimate and unchanged — the operator does
      want to know the IdP is down.

    - **fix(iogate): broadcasting to two or more channels double-freed the
      gbuffer.** `send_all()` handed each child `json_incref(kw)` while every
      child `KW_DECREF()`s it, and `kw_decref()` drops the serialized binary
      fields on every call. So the gbuffer took one decref per child plus
      `send_all()`'s own, against the single `kw_incref()` of the caller: the
      arithmetic only worked out with exactly one open channel, and from the
      second on it was a double free (*"BAD gbuf_decref()"*). Now `kw_incref()`.
      `send_one_rotate()` was never affected — it hands its own reference to a
      single channel. Same defect as the `c_task` one below; the rule ("a kw is
      refcounted with `kw_incref`/`kw_decref`, and events always carry a kw") is
      now in CLAUDE.md's API footguns. The remaining `json_incref(kw)` event
      sends in `root-linux` and `utils` (timer, ievent_cli, yuno, authz, ota,
      the three CLIs) are corrected too: none of their kws carries a gbuffer
      today, so they were latent, not live.

    - **chore(gobj-c): `load_persistent_json()`'s cannot-open critical now
      carries a stack trace.** It names the file but not who was opening it,
      which is exactly what you need when two processes race for the same
      exclusive lock.

    - **fix(agent): a yuno slow to open was launched twice at boot.** The boot
      runs in two sweeps — `run_util_yunos()` for the `yuno_tag=util` yunos,
      then `run_enabled_yunos()` for everything else, `timerStBoot` later — and
      both skip a yuno only when its `yuno_running` is TRUE. That flag is set in
      `ac_on_open()`, i.e. when the yuno registers back, so a yuno that takes
      longer than `timerStBoot` to open is still marked not-running when the
      second sweep arrives and gets launched a second time. `run_yuno()` now
      marks the yuno as launching and both sweeps honor the mark, which
      `ac_on_open()` clears. The `run-yuno` **command** honors it too: it
      guarded on the same `yuno_running`, so an operator launching a yuno that
      was still coming up got a second instance the same way. The mark carries
      its launch time and expires after `timeout_expiration` (30s, the window
      the command counters already give a yuno to connect back): a yuno that
      dies before opening never clears its mark and the agent doesn't watch
      pids, so without the expiry a failed launch would block `run-yuno` for
      that yuno until the agent restarted. This only ever bit at machine boot: on a warm
      `yshutdown` + `restart-yuneta` every yuno registers well inside the
      window. Seen on a controlcenter (a `util` yuno that loads a treedb, unlike
      logcenter/emailsender): the second instance died on timeranger2's
      exclusive `__timeranger2__.json` lock, logging a CRITICAL. That lock is
      what kept two masters off the same store — a `util` yuno without a tranger
      would simply have run twice, with nothing to log.

    - **fix(task): forwarding a kw with a gbuffer to an lmethod double-freed the
      gbuffer ("BAD gbuf_decref()").** `C_TASK`'s `ac_on_message()` handed the kw
      to the job's lmethod with `json_incref(kw)`. A kw carrying a serialized
      binary field has its own symmetric pair — `kw_incref()`/`kw_decref()` —
      because `kw_decref()` drops the binary on *every* call, not only on the
      last one. `json_incref()` bumps the JSON refcount but not the gbuffer's,
      so the two `KW_DECREF()`s that follow (the lmethod's and the action's own)
      decref the gbuffer twice against a single incref: it is freed early, and
      the publisher's final `KW_DECREF()` then reads a freed header. Now uses
      `kw_incref()`, like `c_iogate`/`c_qiogate` do when forwarding.
      Only reachable when the kw actually carries a gbuffer, which for the HTTP
      task path means a non-`application/json` response body (`ghttp_parser`
      parses JSON into `kw["body"]` and keeps the gbuffer instead) — in
      practice, an error page from a reverse proxy. Seen in production as 75
      "BAD gbuf_decref()" matching 75 OIDC discovery failures 1:1, while
      Keycloak was still booting and nginx answered 502 + text/html.

    - **fix(prot_tcp4h): parsing a buffer whose frame dropped the connection
      raised "Event NOT DEFINED in state".** `ac_process_payload_data()` ends by
      re-sending `EV_RX_DATA` to *itself* to parse whatever is left in the
      buffer. Just above, `frame_completed()` publishes `EV_ON_MESSAGE`
      synchronously — and a subscriber may drop the whole chain from inside that
      publish (an authz NAK, or a peer sending bad json). The FSM is then in
      `ST_DISCONNECTED`, where `EV_RX_DATA` is not defined, so the self-send
      logged an ERROR plus a full stack trace for every such connection.
      `frame_completed()` already knew about this cascade — it guards its own
      `start_wait_frame_header()` with the same state check — but it returns 0
      regardless, so the caller had no way to tell it was already dead. The
      leftover self-send is now skipped once we are `ST_DISCONNECTED`: nobody
      upstream wants the rest of the buffer. The sibling self-send in
      `ac_process_frame_header()` needs no guard — it follows a plain
      `gobj_change_state()` with no publish in between, so the connection cannot
      have died under it.

    - **fix(ievent): one garbage packet no longer costs four ERROR entries and
      two stack traces.** A peer sending non-JSON to an ievent port (a port
      scanner is enough) walked a cascade of logs that all described the same
      event: `gbuf2json()` logged "json_load_callback() FAILED" with a full
      stack trace, `iev_create_from_gbuffer()` logged "gbuf2json() FAILED",
      `ac_on_message()` logged "iev_create_from_gbuffer() FAILED", and none of
      them named the peer. All three were `gobj_log_error` — errors are for our
      own broken invariants, and a stranger sending junk is not one.
      `C_IEVENT_SRV`/`C_IEVENT_CLI` now ask for the parse silently
      (`verbose = 0`, which `iev_create_from_gbuffer()` newly honors for its own
      log too) and emit a single `gobj_log_warning` under `MSGSET_PROTOCOL`,
      carrying `peername`/`sockname` and a dump of the offending bytes capped at
      `MAX_LOG_DUMP_SIZE` (256), mirroring `c_prot_tcp4h`. The dump reads from
      the gbuffer head, so it survives the parser having consumed the data and
      leaves the read pointer alone. Behaviour is unchanged: the connection is
      still dropped. Callers passing a non-zero `verbose` keep the old logs.

    - **feat(authz): a subscriber of `EV_AUTHZ_USER_LOGIN` can now refuse a
      login.** `mt_authenticate()` published the event fire-and-forget and threw
      away the result, so a subscriber unable to accept the user had no way to
      say so — the login succeeded regardless, and the user was left
      authenticated but unregistered downstream. It now checks
      `gobj_publish_event()`'s return and answers `result: -1` ("Some subscriber
      refusing user") when it comes back negative. Contract note for out-of-tree
      gclasses: an action handling this event that returns a negative value now
      denies the login; every in-tree subscriber (`c_controlcenter`, `c_agent`,
      `c_mqtt_broker`) returns 0 and is unaffected. Two caveats worth knowing:
      the checked value is the *sum* of the subscriber returns, and a subscriber
      holding `__own_event__` short-circuits before the accumulation, so its
      refusal is not seen. The auth failure paths also log `peername`/`sockname`
      now.

    - **fix(controlcenter): a login arriving before the service played crashed
      against a NULL treedb.** `C_CONTROLCENTER` subscribes to the authz service
      in `mt_start` but only opens `treedb_controlcenter` in `mt_play`, so every
      login landing in that window called `gobj_get_node()` / `gobj_create_node()`
      with a NULL gobj and logged "hgobj NULL or DESTROYED" with a stack trace —
      loud at startup, when many agents reconnect at once. `ac_user_login` and
      `ac_user_new` now check the treedb is open and refuse the login until the
      service reaches `mt_play` (fail-closed, riding the authz contract above),
      so a user never gets in without its controlcenter record.

## 7.8.0

A feature release built around the **C_TRANGER / C_NODE read surface** — the
command set a GUI needs to browse a timeranger2/treedb without pulling whole
topics into the browser: server-side key filtering/sorting/paging
(`list-keys`), cursor pagination (`open-iterator` / `get-page`), realtime feeds
(`open-rt` / `close-rt`), the restored record-read commands, and
`print-tranger`'s bounded, drillable raw-JSON dump. Alongside it, a
timeranger2 audit pass that fixed several ways a failed append could be
reported as success.

No BREAKING changes: the one signature correction is a header/implementation
mismatch (prototype names only, no ABI change), and the one event-payload
change is additive.

Ships with `@yuneta/gobj-js` **7.8.0** and `@yuneta/gobj-ui` **4.0.0** (a
MAJOR — five BREAKING contract changes; see its CHANGELOG before upgrading a
v2 SPA).

    - **feat(treedb): C_NODE gains a `print-tranger` command.** Mirrors
      C_TRANGER's: it dumps the tranger the treedb lives on as bounded,
      `kw_collapse()`-truncated JSON, and accepts `path=` to lazily drill into
      one subtree (arrays indexed by numeric position, so the path round-trips
      through `kw_find_path`). This is what the gui_treedb "Raw JSON" viewer
      calls to inspect a treedb's raw tranger; a primitive path returns an
      explicit error rather than a silent null.

    - **feat(gobj-c): `kw_collapse()` now accepts a top-level array.** Drilling
      `print-tranger path=<array>` used to fail because `kw_collapse()` required
      a dict at the requested path; it now collapses a top-level array too (new
      `collapse_array`, mirroring the array-value branch of `collapse()` —
      object elements recursed, arrays/primitives copied shallow, element paths =
      numeric index). Dict output is byte-for-byte unchanged, and only
      `print-tranger` calls it. Covered by `tests/c/kw/test_kw1` (top-level-array
      collapse shape + `kw_find_path` round-trip + primitive-rejected).

    - **fix(timeranger2): the realtime feeds ignored `only_md`, always reading
      and delivering the record body.** A feed opened with `only_md` wants a
      record's metadata but not its content — the historical iterator honors it,
      but both realtime paths (the master append fan-out to rt_mem lists, and
      `publish_new_rt_disk_records` on a follower) always called
      `read_record_content()` and handed the callback the full body. On rt_disk
      that was an extra disk read per live record, and within a single `only_md`
      list historical rows arrived md-only while live rows carried full content.
      They now honor `only_md`: the only_md feeds get NULL `jn_record`, and the
      rt_disk read is skipped when no audience of the key needs the body. No
      consumer relied on the old behavior (c_tranger already synthesizes an
      md-only record for a NULL `jn_record`; tr_queue and the mqtt broker use
      `only_md` only for one-shot historical dumps). Covered by
      `tests/c/timeranger2/test_rt_disk_multi_feed` (an rt_disk and an rt_mem
      `only_md` feed asserted to receive metadata, never a body).

    - **fix(treedb): `treedb_create_node()` could return a dangling pointer.**
      When the primary already existed (`save_id` false) and every listed pkey2
      had no `indexy` (a schema/index inconsistency), the node landed in no
      index, yet the final `json_decref` dropped its only ref and the freed
      pointer was returned. It now frees the node and returns NULL when it
      reached no index.

    - **fix(timeranger2): two fs_watcher edge cases.** (1) the inotify read
      buffer leaked when `yev_create_read_event()` failed (which only happens
      before it takes ownership of the gbuffer). (2) after a concurrent
      watch-descriptor removal `get_path()` returns NULL, and every event branch
      except IN_DELETE built a `"(null)/..."` path or handed the callback a NULL
      directory; the stale event is now skipped once, up front.

    - **fix(tr2migrate): a failed record append was counted as migrated.** The
      migration callback ignored `tranger2_append_record()`'s return and bumped
      its counters before appending, so a failed append still counted toward the
      migrated totals. It now counts only after a successful append.

    - **fix(mqtt): `tr2q_append()` enqueued a garbage entry when the append
      failed** (the broker session queue). Like `trq_append2` it ignored
      `tranger2_append_record()`'s return, building a `q2_msg_t` from an
      uninitialized `md_record` (bogus `rowid`) that later readers would trust;
      and because it had already `KW_EXTRACT`'d the gbuffer out of `kw`, the
      failure path also leaked that gbuffer. It now frees the extracted gbuffer,
      decrefs `kw`, and returns NULL on a failed append.

    - **fix(timeranger2): `tranger2_append_record()` reported success after a
      file-open failure.** Both write stages are gated by `if(fd >= 0)` with no
      else, so when `get_topic_wr_fd()` failed for the content or the md2 file
      the function fell through and returned 0 (success) — persisting an index
      row with a bogus offset/size (content open failed), or content bytes with
      no index row and `g_rowid` 0 fed to the realtime feeds (md2 open failed).
      It now returns -1 at either failure, like the sibling lseek/write errors.

    - **fix(timeranger2): use-after-free in `fs_watcher` `remove_watch()` under
      `TRACE_FS`.** `path` (the `IN_DELETE_SELF` caller passes `get_path()`'s
      borrowed string, aliasing an entry of `jn_tracked_paths`) was read by the
      trace and by the `inotify_rm_watch` error log AFTER `json_object_del()`
      freed the backing string. It is now snapshotted before the delete.

    - **fix(tr_queue): `trq_append2()` enqueued a garbage entry when the append
      failed.** It ignored `tranger2_append_record()`'s return, so on failure it
      built a `q_msg_t` from an uninitialized `md_record` (bogus `rowid`/`__t__`)
      that later readers would trust. It now decrefs `kw` and returns NULL on a
      failed append, matching `tr_msg`/`tr_msg2db`.

    - **fix(tr_msg2db): `msg2db_close_db()` could leak the shared descriptor
      with concurrent msg2dbs.** It released `topic_cols_desc` with an
      unconditional `JSON_DECREF` (which nulls the global), so closing one of two
      open msg2dbs nulled the global while the other still held a ref — leaked on
      the second close. It now uses the same refcount-guarded decref as
      `treedb_close_db`.

    - **fix(treedb): a failed treedb/msg2db open leaked the shared column
      descriptor.** `treedb_open_db()` / `msg2db_open_db()` incref-or-create the
      module-global `topic_cols_desc` before validating the schema, but the two
      early error returns ("No topics found", "TreeDB ALREADY opened") skipped
      the matching decref — and a failed open is never paired with a
      `close_db()`, so the descriptor stayed alive to `gobj_end()` (a leak under
      `CONFIG_DEBUG_TRACK_MEMORY`, failing ctest). Both paths now undo the
      incref/create with the same refcount-guarded decref `close_db` uses (plain
      `json_decref` while another open still holds it — the double-open case —
      else `JSON_DECREF` to free and null the global). Covered by
      `tests/c/tr_treedb_hook_hygiene` (a no-topics open + the end-of-test memory
      check).

    - **fix(timeranger2): a corrupt md2 file was detected, logged, then read
      anyway.** `load_first_and_last_record_md()` logged "md2 file corrupted" on
      a negative or misaligned `lseek(SEEK_END)` but did not return, falling into
      `if(offset >= sizeof(md2_record_t))` — where the signed `offset` is promoted
      to unsigned, so a negative (lseek-error) offset passed the guard and a
      truncated (misaligned) file was read one record short, both feeding a bogus
      "last record" into the key cache. It now closes the fd and returns -1 at the
      corruption point, so the caller aborts instead of caching garbage.

    - **fix(treedb): force-deleting a node with an array hook skipped every
      other child and then aborted.** The down-link teardown in
      `treedb_delete_node(force=1)` iterated the parent's hook array while
      `_unlink_nodes()` removed each child from that SAME array in place, so the
      index-based loop stepped over the shifted tail: with three children it
      unlinked the 1st and 3rd, left the 2nd, and the re-check then found a
      leftover down link and refused the delete — leaving a half-unlinked graph
      persisted on disk (the cleared children were already saved) while
      reporting failure. The teardown now snapshots the child refs before
      unlinking. Covered by `tests/c/tr_treedb_hook_hygiene` (force-delete of a
      config with three linked yunos).

    - **fix(treedb): `parse_schema()` validated every schema against an EMPTY
      descriptor.** It built a local column descriptor but passed the
      module-global `topic_cols_desc` to `parse_schema_cols()` — and that global
      is NULL until the first `treedb_open_db()`. `parse_schema()` is the
      validate-before-open helper the gclasses call at `mt_start`, i.e. before
      any treedb is open, so `json_array_foreach(NULL, ...)` ran zero checks and
      ANY malformed schema passed unvalidated. It now validates against the
      descriptor it builds.

    - **fix(timeranger2): an md2 open failure poisoned the key's cache with a
      ~1.8e19 row count.** `load_first_and_last_record_md()` returns -1 on a
      failed `open()`, but its type was `uint64_t`, so the caller's
      `if(file_rows < 0)` never fired: -1 became `UINT64_MAX` and the cache cell
      was built with that as its row count. A follower reloading a key whose
      `.md2` was unlinked mid-append (`tranger2_delete_key` racing an append)
      then ran `publish_new_rt_disk_records` with `to_rowid = UINT64_MAX`, a
      near-unbounded read loop. The function is now `json_int_t`, the guard
      fires, and both callers bail (or skip the file) on NULL.

    - **fix(timeranger2): a deleted key left its watermark behind in every disk
      feed.** `published` holds one mark per key, and nothing dropped it when
      the key died: a keyless feed on a topic that cycles its keys (an hourly
      bucket) kept a mark for every key that ever existed, for as long as it
      lived. Worse, a key RE-CREATED in the same file inherited the corpse's
      mark — and a mark above the reborn key's rowids is a ceiling, not a
      watermark, so its records were served to nobody. The mark now dies with
      the key, on both delete paths (the master's `tranger2_delete_key()` and
      the follower's `FS_SUBDIR_DELETED` branch, which share
      `fire_key_deleted_locally()`). Covered by
      `tests/c/timeranger2/test_rt_disk_multi_feed` (a key deleted and
      re-created in the file its mark counted in).

    - **fix(c_tranger): `list-keys` answered an UNSORTED page as if it were
      sorted.** When the sort could not allocate its row array it gave up and
      returned, and the command went on to page the untouched list and answer
      OK: `from`/`limit` then cut it at positions that mean nothing, and the
      client — which had asked for `order` — had no way to tell. The sort now
      says whether it sorted, and the command is refused with the reason.

    - **perf(c_tranger): `list-keys` sorts with `qsort`, not an insertion
      sort.** The ordered variant inserted each key by linear scan — O(n²)
      with a jansson refcount round-trip per swap, inside the event loop;
      `order=records` over a topic with enough keys to need `list-keys` at
      all could stall the yuno for seconds. Ties on the record count now
      break by key, so equal counts page deterministically (`qsort` is not
      stable).

    - **fix(timeranger2): the first record of every new md2 file reached NO
      realtime disk feed** — a Live card left open across midnight silently
      dropped one record a day, per key. A feed's `published` watermark counts
      rows IN A FILE (`read_md()` seeks `(rowid-1)` records into `<file_id>.md2`,
      and the batch bounds come from that file's cache cell), but the topic
      ROTATES its file (`filename_mask`, by default one a day). The mark was
      stored per key alone, so at the rotation yesterday's mark — say row 226651
      — met a file whose rowids restart at 1 and became a ceiling instead of a
      watermark: `from_disk_rowid` sat above `to_rowid` and the batch was served
      to nobody. It self-healed on the next append (the mark is overwritten with
      the new file's row count), which is exactly why it read as "a record went
      missing" and not as a broken feed. The mark now carries the file it counts
      in: a mark of another file is no mark, and the feed is re-seeded at the
      new file's start.

      With it, the rowid a disk feed is GIVEN is now the GLOBAL rowid of the key
      (the file's base plus the position in it) — what the callback's contract
      promises (*"global rowid of key"*) and what the master's rt_mem path
      already delivered (`update_new_record_from_mem()` returns `g_rowid`). The
      follower's disk path was handing out the file-relative one, so `g_rowid`
      in the published `__md_tranger__` was wrong after the first rotation and a
      consumer that dedupes by it (the Live cards' key counter) took the new
      file's records for records it already had. Covered by
      `tests/c/timeranger2/test_rt_disk_multi_feed` (a rotation phase: the first
      append of the new file reaches each feed exactly once, with the key's
      global rowid).

    - **fix(timeranger2): a realtime DISK feed re-broadcast every other feed's
      wake-up, so N feeds on a key meant N copies of every record for each of
      them.** With a per-key Live card and a whole-topic Live card open on the
      same key (gui_treedb), every row appeared DUPLICATED in both. The master
      hard-links each new md2 into the directory of EVERY feed that wants the key
      (`master_to_update_client_load_record_callback`), so a follower is woken
      once per feed — but `publish_new_rt_disk_records()` then fanned the new
      records out to EVERY feed of that key, turning N wake-ups into N x N
      publishes. The wake-up now serves the feed whose `/disks/<rt_id>/`
      directory fired, and nobody else: the `rt_id` was already parsed off the
      path and thrown away. Each feed carries its own `published` watermark per
      key, because the shared key cache advances with the FIRST wake-up and a
      feed served by a later one would otherwise find "nothing new" and lose the
      record; the in-process rt_mem lists of a follower keep being fed from that
      shared cache, so they still see each record exactly once.

      The watermark of every feed of the key is SEEDED at the batch start on
      the batch's first wake-up, while that start is still known: a feed with
      no watermark yet (one that just opened) cannot recover it from the
      already-advanced cache, and unseeded it permanently lost the first
      records after opening — they reached the sibling Live card and never
      that one (on a brand-new key, whose batch starts at rowid 1, BOTH feeds
      lost the first record; presence in `published` is what says "seeded",
      since the legitimate seed there is 0). Covered by `tests/c/c_tranger`
      (a keyed feed and a keyless feed over the same key, one publish each,
      rt_mem on the master) and by `tests/c/timeranger2/test_rt_disk_multi_feed`
      (the rt_disk fan-out itself: master + in-process follower, three feeds,
      first-append-after-open and brand-new-key cases); verified against the
      live backend by counting the WebSocket frames of a browser with both
      cards open.

    - **fix(timeranger2): a brand-new key made every rt_disk follower log an
      `unlink() FAILED`.** On staging, `agregador_wz` logged one *"unlink()
      FAILED, errno 2 (No such file or directory)"* per new key — an hourly
      bucket meant an hourly ERROR in the log, and monitors alerting on nothing.

      A follower is notified of new records by a hard link the master drops in
      `<topic>/disks/<rt_id>/<key>/<file>.md2`; the follower unlinks it (consuming
      the notification) and reads what is new. A brand-new key reaches it
      **twice**, by construction: `fs_watcher`, on the `IN_CREATE` of the key
      directory, adds the watch on it **first** and calls back **after**
      (`fs_watcher.c`), so in the window between the two the master hard-links the
      `.md2` inside — and the file then gets its OWN `IN_CREATE`
      (`FS_FILE_CREATED_TYPE`) *and* is found by the directory scan
      (`scan_disks_key_for_new_file`). Both paths lead to
      `update_key_by_hard_link()`; whichever runs second finds the file already
      gone. It is exactly the hazard the inotify(7) note quoted in that handler
      warns about — the scan exists to cover it, but nothing deduplicated what
      the watch would also report.

      `ENOENT` is therefore not an error: it is the notification having been
      consumed already. It is traced (`fs`) instead of logged as an error; any
      other errno still is one. The read still runs — it is idempotent (it
      publishes only the rows the cache does not have yet), so consuming twice
      costs a re-read and nothing else, while skipping it would lose the records
      if the other path had unlinked and then failed.

    - **feat(c_tranger): `list-keys` filters, sorts and pages the keys IN THE
      SERVER.** It answered every key of the topic, always: a client that wanted
      the keys of one device was handed a hundred thousand of them and filtered
      what it had been given — and a browser that only shows 15 at a time was
      transferring, holding and sorting the whole index on its main thread.
      New parameters: `rkey` (a PCRE2 regex the key must match), `order`
      (`key`|`records`) + `desc`, and `from`/`limit`.

      Backwards compatible by shape: with no `limit` the answer is the plain
      list it has always been, so every existing client keeps working. Asking
      for a PAGE gets the same envelope `get-page` uses —
      `{total_rows, pages, data}` — so a client pages KEYS exactly as it pages
      records, and `total_rows` counts the MATCHING set, not the page. Sorting
      has to happen here for the same reason: a client holding 15 of 100.000
      keys cannot order what it was not given.

      PCRE2 and not POSIX `regcomp()`: every yuno already links `libpcre2-8`
      (`tools/cmake/project.cmake`) and gobj-c already speaks it
      (`json_replace_vars.c`), and this is the one place in the read path that
      runs the same pattern against up to a hundred thousand subjects — the case
      its JIT exists for. The pattern is compiled and JIT-compiled once, outside
      the loop.

      Cost is paid where it belongs: the record COUNT of a key is a cache lookup,
      so it is taken for the whole matching set (that is what lets `order=records`
      sort by it and `total_rows` be exact), while the key's TIME SPAN copies the
      cache totals into a fresh dict and is therefore built only for the keys
      that actually travel. A bad `rkey` and an unknown `order` are refused with
      an error, never answered as "nothing matched".

    - **feat(timeranger2): `rkey` governs a keyless list — the disk load AND the
      realtime feed.** A list opened with no `key` is the whole topic; `rkey`
      narrows it to the keys matching a regex. It was documented and half true:
      the one-shot read (`open-list return_data=1`) filtered the keys, but a LIVE
      list **refused** `rkey` outright (*"rkey is only supported with
      return_data=1"*) — and rightly so, because `tranger2_open_list()` visited
      every key on load and the realtime feeds (`rt_mem` / `rt_disk`) knew
      nothing about it. A list filtered on load and unfiltered on append is worse
      than no filter at all: the caller cannot tell it is being lied to.

      `rkey` is now honoured in both halves, so a live list can finally be opened
      on a SUBSET of a topic's keys. The three dispatch sites that decided who
      gets an appended record (mem feed, disk feed, and the non-master path) went
      through one `list_wants_key()`, so the two halves cannot drift apart again.
      The compiled pattern lives IN the list (a pointer in its json, like its
      `load_record_callback`) and dies with it: it is consulted once per appended
      record, so compiling it per record would cost more than the load it saves.
      PCRE2 + JIT, and `c_tranger`'s one-shot path was moved off POSIX
      `regcomp()` onto it as well — one flavour of regex per parameter, or the
      same `rkey` would mean two different things depending on which half of
      `open-list` ran it.

      A malformed `rkey` REFUSES the list (and the feed), rather than degrading
      to "every key": that would push the caller exactly the records it asked not
      to receive. The refusal is logged as a **warning**, not an error with a
      stack trace: an `rkey` arrives from a remote peer (a SPA opening a live
      list), so a bad pattern is peer input, not a broken internal invariant —
      the decoder-severity policy. `gobj_log_set_last_message()` carries the
      regex's compile error (pattern + offset) into the refusal the caller gets
      back, so the SPA can show WHY the pattern was rejected.

      (An earlier note in this section claimed `open-list`'s `rkey` was *silently
      ignored*. That was wrong, and worth correcting: it was honoured in the
      one-shot read and explicitly refused in the live one. The dead code is
      timeranger2's commented-out `find_keys_in_disk`.)

    - **fix(c_tranger): use-after-free closing a handle whose topic was closed
      (SIGSEGV on shutdown).** A topic OWNS the iterators / rt_mem / rt_disk
      handles opened on it: `tranger2_close_topic()` closes them all
      (`tranger2_close_all_lists`) and frees the topic. C_TRANGER registered
      what a remote client opened as a RAW POINTER, so once anything closed a
      topic — the app closing a treedb at shutdown, a `delete-topic` at runtime
      — its registry pointed at freed memory, and the next close dereferenced
      it. In production this crashed a yuno with SIGSEGV on EVERY shutdown that
      had a Rows card open: close-treedb frees the topics, then C_TRANGER's
      `mt_destroy` walked its registry calling `tranger2_close_iterator()` on
      iterators that no longer existed (jansson reading a dead hashtable). The
      daemon then relaunched the yuno, so its binary was never idle and
      `update-binary` failed with `copyfile() FAILED` — an un-updatable yuno was
      the visible symptom. The registry now stores the topic name beside the
      pointer and every use goes through a guard (new
      `tranger2_topic_is_open()`): topic gone ⇒ the handle was freed with it ⇒
      drop the entry, never touch it. Regression test: open an iterator + a feed,
      close the topic under them, then command and tear down — it SIGSEGVs
      without the guard.

    - **fix(timeranger2): the cache totals of a key stored a timestamp with the
      metadata flags baked into it.** In an on-disk `md2_record_t` the 16 high
      bits of `__t__` carry the `user_flag` and those of `__tm__` the
      `system_flag`. The disk-load path masks them off; the APPEND path did not,
      and fed the raw record to `update_cache_cell()` — so on the master, a key's
      cached `fr_t`/`to_t`/`fr_tm`/`to_tm` were poisoned by the flag bits (a `tm`
      of 946684800 was cached as 17593132729216). This was not cosmetic:
      `get_segments()` chooses which `.md2` files to read by comparing against
      those very ranges, so **file selection by time was wrong for any record
      carrying a non-zero user_flag or system_flag** (treedb tagged records,
      queue pending flags). `update_cache_cell()` now reads the times through the
      `get_time_t()` / `get_time_tm()` accessors. `test_topic_pkey_integer`'s foto
      had the defect baked in — it held BOTH forms of the same value (polluted in
      the in-memory cache, clean after a reload) and has been repointed.

    - **feat(timeranger2): a filtered paging iterator now honors its
      `match_cond` per RECORD (row index).** `get_segments()` can only reason
      about whole files, and `tranger2_iterator_get_page()` ignored the
      iterator's conditions entirely (it built its own `match_cond` with just
      `from_rowid`/`to_rowid` and reported the FULL key row count), so the
      time / rowid / user_flag conditions of `open-iterator` were only applied at
      file granularity: a 4-record key filtered down to 2 still reported 4 and
      its page returned all 4. An iterator that filters now builds its row
      **index** when it opens (`tranger2_match_metadata` over each record's
      32-byte metadata, no content read): `tranger2_iterator_size()`, `pages` and
      the pages themselves count only matching records, and `get_page`'s
      `from_rowid` is a position among THOSE rows. An unfiltered iterator builds
      no index — its open stays O(1) on the key size and its positions remain the
      global rowids. A dead row (`tranger2_delete_instance`) never enters the
      index, so a filtered iterator's count no longer over-reports it.

    - **feat(c_tranger): `list-keys` reports each key's time span, and `topics
      expanded=1` its topic descs.** `list-keys` now returns `fr_t`/`to_t` and
      `fr_tm`/`to_tm` alongside `records`, so a client can bound a time picker to
      what the key actually holds without reading a record;
      `topics expanded=1` returns a desc per topic (`topic_name`, `system_flag`,
      `pkey`, `tkey`) — `system_flag` is the only thing that says whether the
      topic's `t`/`tm` are seconds or milliseconds. Both are additive: the
      default `topics` answer (an array of names) is unchanged. New public
      `tranger2_topic_key_range()`.

    - **fix(timeranger2): `get-page` reported "0 pages" for an out-of-range page.**
      `pages` is a property of the KEY and the requested page size, not of the
      particular answer: a page past the end has no data, but the key still has
      its pages. Returning 0 told the client "there is nothing at all" and
      collapsed its pager to a single page — a remote-paginated table then
      refused to move ("Next page would be greater than maximum page of 1") and
      its Last button landed on an empty table. Both empty branches
      (out-of-range `from_rowid`, no first segment) now report the real page
      count.

    - **fix(c_tranger): realtime feeds leaked, and every leaked feed duplicated
      records for EVERY subscriber.** `publish_rt_callback()` runs once per OPEN
      FEED, and each run published `EV_TRANGER_RECORD_ADDED` to the whole
      channel. Feeds were only ever released by an explicit `close-rt` (or at
      `mt_stop`), so a remote client that died without one — browser reload,
      closed tab, dropped websocket — left its feed alive forever. Every
      surviving feed then re-published each append, so N leaked feeds meant N
      copies of the same record delivered to all subscribers, not only to the
      session that leaked them (observed in gui_treedb: the same `rowid`
      repeated ~20 times in a Live card).
      Two fixes: (1) the payload now carries the `rt_id` of the feed that
      produced it, so a record can be routed to the subscriber that OPENED that
      feed instead of broadcast (a foreign feed can no longer duplicate anyone's
      rows); (2) `mt_subscription_deleted` closes the feeds **and iterators**
      opened by a subscriber once its last subscription is gone — the owner is
      stamped into the rt/iterator as `src_gobj` at `open-rt` / `open-iterator`.
      Consumers of the event get one new field (`rt_id`) and are otherwise
      unaffected; a consumer that does not filter on it keeps working.
      An iterator opened by a session that never subscribes (Rows-only browsing)
      is still only reclaimed at `mt_stop` — see `TODO.md`.

    - **feat(c_tranger): `open-iterator` accepts metadata match conditions.**
      Beyond `key` + `backward`, the command now forwards the record-metadata
      conditions honored by `tranger2_match_metadata` into the iterator's
      match_cond, so they pre-filter the page index and the reported
      `total_rows` / pagination reflect the filtered set: `from_t`/`to_t`
      (t, epoch seconds), `from_tm`/`to_tm` (tm, epoch ms),
      `from_rowid`/`to_rowid` (1-based; negative = from end) and the user_flag
      conditions (`user_flag`, `not_user_flag`, `user_flag_mask_set`,
      `user_flag_mask_notset`). Each is optional — `0`/empty means unset. Only
      keys actually supplied are added to match_cond. Record-FIELD filters
      (e.g. `voltage > 200`) are deliberately NOT plumbed here: they are not
      indexable at the metadata level, so they stay client-side in the SPA.
      `open-rt` is unchanged — the realtime feed filters only by `key` (its
      stored match_cond is not consulted on append), so no non-functional
      params were added there. Enables gui_treedb's Rows-card request options
      (yunos-js).

    - **fix(timeranger2): fail loud on inotify `IN_Q_OVERFLOW`.** Under a burst
      the kernel drops inotify events and emits a single `IN_Q_OVERFLOW`
      (`wd == -1`); until now `fs_watcher` ignored it, so a dropped
      `FS_FILE_CREATED`/`FS_SUBDIR_*` left an rt-disk follower silently out of
      sync. `fs_watcher` now logs it `critical` with `LOG_OPT_ABORT`: the yuno
      aborts and ydaemon relaunches it, and the clean reload re-establishes
      every feed correctly — the proven recovery path, chosen over a
      hard-to-test in-place resync. The deb/rpm packagers also raise the
      inotify sysctl provisioning in `99-yuneta-core.conf`:
      `fs.inotify.max_user_instances` 1024 → 4096,
      `fs.inotify.max_user_watches` → 524288, and
      `fs.inotify.max_queued_events` → 65536 (a defensive cushion above the
      16384 kernel default), so the abort/relaunch stays rare.

    - **feat(c_yuno): `info-inotify` command** — reports the system inotify
      limits (`/proc/sys/fs/inotify/*`) and this yuno's own usage (instances +
      watches, via `/proc/self/fd` + `fdinfo`), alongside `info-cpus` /
      `info-ifs` / `info-os`. The `/proc/self/fd` probing lives in
      `helpers.c` as `get_inotify_self_usage()`.

    - **feat(c_tranger): realtime feed commands `open-rt` / `close-rt`, and
      `EV_TRANGER_RECORD_ADDED` is now `EVF_PUBLIC_EVENT`.** `open-rt {rt_id,
      topic_name, key}` opens a realtime-only feed on a topic key — NO history
      load, no data retention — that streams each NEW append to subscribers as
      `EV_TRANGER_RECORD_ADDED {topic_name, key, rowid, record}` (the record
      carries a `__md_tranger__` with the same field names `get-page` emits, so
      live and paged rows render identically). rt-by-memory on the master
      (fires on `tranger2_append_record`), rt-by-disk on a reader (inotify).
      `close-rt {rt_id}` closes it (via `tranger2_close_list`, which dispatches
      by list_type). Feeds live in a per-gclass registry closed at `mt_destroy`
      if a client never sends `close-rt`. Making the event `EVF_PUBLIC_EVENT`
      is what lets a remote subscriber (a browser SPA) subscribe over the
      ievent gate (`c_ievent_srv` requires the flag). The regression test
      (`tests/c/c_tranger/`) subscribes a probe and asserts a fresh append
      publishes exactly once, a cross-key append does not reach the feed, no
      publish after `close-rt`, and a left-open feed is leak-free at destroy.
      Enables gui_treedb's Live records card (yunos-js).

    - **fix(timeranger2): three defects found while documenting the public API.**
      (1) `tranger2_topic_key_size()` with an empty `key` passed `gobj` where a
      `json_t *tranger` is expected, so the whole-topic fallback silently
      returned 0; now it returns the real total. (2) `tranger2_close_all_lists()`
      was declared `(…, rt_id, creator)` in the header but the implementation and
      both callers use `(…, creator, rt_id)`; the header (and the `kernel/c`
      README signature) are corrected to match — a caller trusting the old header
      filtered by the wrong field (prototype names only, no ABI change).
      (3) `tranger2_get_iterator_by_id()` / `_get_rt_mem_by_id()` /
      `_get_rt_disk_by_id()` tested `empty_string(creator) && empty_string(creator)`
      (the second operand was meant to be the stored `creator_`); an empty
      query-creator now matches only creatorless entries (both-empty) instead of
      any creator. The three functions also normalize a NULL query-creator to
      `""` (like `tranger2_open_rt_mem()` does), so a NULL `creator` can no
      longer reach `strcmp(NULL, …)` when the entry has a creator.
      Also: the whole `timeranger2.h` public API was re-documented
      (ownership, return semantics, NULL/error paths, master-only, disk-vs-memory)
      and the doc.yuneta.io timeranger2 API page synced.

    - **feat(c_tranger): restore the record-read commands `open-list`,
      `get-list-data` and `close-list`** — v7-port stubs ("Pending to review")
      rewired to the current timeranger2 iterator/list API, keeping the v6
      contract: `open-list` accepts the full match_cond parameter set
      (`key`, `rkey`, `from/to_rowid` incl. negative-from-end, `from/to_t(m)`,
      `fields`, `only_md`, `backward`, user-flag masks). With `return_data=1`
      it is a ONE-SHOT snapshot read — loads the matching records per key with
      short-lived iterators and auto-closes (a remote client, e.g. a SPA, may
      never send `close-list`); without it the list stays open (registry in
      the gclass, closed at destroy), collecting history + realtime appends in
      its `data` (readable via `get-list-data`) and publishing appends as
      `EV_TRANGER_RECORD_ADDED`. `only_md` records are synthesized md-only
      dicts (the current loader hands a NULL content record). Bool/int params
      are read with `KW_WILD_NUMBER` (command-yuno/-agent forward strings).
      `add-record` stays stubbed (write path). Consumed by gui_treedb's new
      tranger records browser (yunos-js repo).

    - **feat(c_tranger): cursor-pagination command surface — `list-keys`,
      `open-iterator`, `get-page`, `close-iterator`.** Exposes the timeranger2
      per-key iterator primitives (`tranger2_open_iterator` +
      `tranger2_iterator_get_page`) as commands so a remote client can page
      through a key's records with a real cursor instead of re-reading a
      growing snapshot. `open-iterator` builds the key's row index only (no
      upfront record load, no realtime feed — `load_record_callback` NULL) and
      returns `{iterator_id, total_rows}`; `get-page from_rowid=<1-based>
      limit=<n> [backward=1]` returns `{total_rows, pages, data}`, reading
      records lazily; `close-iterator` closes and deregisters. Open iterators
      live in a per-gclass registry (mirror of the open-lists one) and are
      closed at `mt_destroy` if a client never sends `close-iterator` (a leaked
      iterator would retain file handles). `list-keys` returns a topic's keys
      with their record counts (`[{key, records}]`) — the input for a
      two-level keys→records browser. All check `read`/`list` authz; ints/bools
      read with `KW_WILD_NUMBER`; the parameter is named `iterator_id` (not
      `id`) to dodge the command-yuno `id=` collision. New regression test
      `tests/c/c_tranger/` drives the four commands through `gobj_command`
      (forward paging, out-of-range, dup-open, close-then-404, missing key /
      topic, and a deliberately-left-open iterator proving the destroy-time
      cleanup is leak-free). To be consumed by gui_treedb's two-level tranger
      records browser (yunos-js repo, Phase 2).

## 7.7.2
_C / SDK patch release — agent TTY console lifecycle: `open-console` re-attach
(the gui_agent Terminal shell now survives a browser refresh instead of leaking
a PTY per reload), `max_consoles` off-by-one, and clean shutdown with consoles
open. Pairs with the gui_agent stable per-tab console name + screen restore
(tracked in the `yunos-js` repo CHANGELOG); `@yuneta/gobj-js` is `7.7.2` on npm._

    - **fix(agent/agent22): `open-console` re-attach — a browser refresh of the
      gui_agent Terminal no longer accumulates PTYs until `max_consoles`.**
      Re-opening an EXISTING console from the same channel (the
      controlcenter↔agent route, which stays up across browser refreshes, so
      the agent never sees the client disconnect) answered `-1 "Console
      already open"` and each refresh had to fork a new console. Now it is a
      re-attach: the stored route `__md_iev__` is refreshed so the tty stream
      routes to the NEW requester (not the dead one), and `EV_TTY_OPEN` is
      replayed to the requester with the same kw shape C_PTY publishes at
      start (name/process/uuid/cwd/rows/cols) so the client leaves
      "Connecting…" — the shell session survives the refresh. Also fixed the
      `max_consoles` off-by-one (`>` → `>=`: the limit admitted max+1
      consoles). Pairs with the gui_agent stable per-tab console name
      (yunos-js); both `c_agent.c` and `c_agent22.c`.
    - **fix(agent/agent22): close live consoles on shutdown.** With a console
      open, an orderly exit (Ctrl-C) reached `gobj_end` with the C_PTY still
      started — "Destroying a RUNNING gobj" + "hgobj destroying" + a running
      `YEV_READ_TYPE` event destroyed hot. `mt_stop` now deletes every entry
      of `list_consoles` (the volatil C_PTY services stop before the tree is
      destroyed). Note the key-aliasing trap: `delete_console` KW_EXTRACTs the
      console from `list_consoles`, freeing the dict key mid-call, so the
      loop passes a copy of the name.

## 7.7.1
_C / SDK patch release — control-plane / auth-BFF hardening and correctness
fixes, plus an mbedTLS backend bump. JavaScript framework changes are tracked in
their own repositories (`@yuneta/gobj-js`, `@yuneta/gobj-ui` CHANGELOGs); the
`gui_agent` / `gui_treedb` yunos record their own UI changes under
`yunos/js/*/README.md`._

    - **chore(ext-libs): bump mbedTLS 4.1.0 → 4.2.0 (v1.21).** The mbedTLS
      TLS backend (runtime-selectable, statically linked into every yuno built
      with `CONFIG_HAVE_MBEDTLS`) moves to the `v4.2.0` upstream tag;
      `repos2clone.sh` re-pins `TAG_MBEDTLS` and `configure-libs.sh` bumps the
      ext-libs `VERSION` 1.20 → 1.21. Rebuild ext-libs (`extrae.sh` +
      `configure-libs.sh`) and relink the affected yunos.
    - **harden(c_auth_bff):** cookie/token paths sized and fail-closed. One
      `BFF_TOKEN_MAX` (8 KB) now covers the stored refresh_token, the cookie
      extraction buffers and the Set-Cookie build (a Keycloak access_token with
      many roles exceeds the old 4 KB extraction buffer — `/auth/token` would
      forward it silently clipped and remote backends rejected the signature with
      no evidence). `make_set_cookie` refuses to emit a truncated cookie (logs +
      returns NULL; the login/refresh answer becomes `500 token_too_large`),
      `extract_cookie` treats a value that doesn't fit as missing (clean 401
      instead of a corrupted token), and `/auth/token` checks its `json_pack`.
    - **fix(c_authz):** `create-user` / `update-user` no longer report success
      when the treedb write failed — the `EV_ADD_USER` result is propagated, so a
      failed autolink (bad `roles^ROLE^users` string) or tranger write error
      answers `Can't create/update user`. Also: "User not exist" → "User does not
      exist", and `update-user` shares `pm_create_user` (the table was a verbatim
      copy).
    - **fix(yuno_agent):** a `write-tty` naming a console that no longer exists
      now answers the requester with a synthetic `EV_TTY_CLOSE` (same path as
      `ac_tty_close`), so a client whose original close was lost in a link flap
      can close its Terminal tab instead of typing into the void. `multiple_dir`
      logs on snprintf failure/truncation (a truncated tags path silently placed
      the yuno under a wrong repos dir). `ac_stats_yuno_answer` gains the same
      self-name short-circuit as its command twin (no more "Event NOT DEFINED"
      noise when the agent itself is the stats requester).
    - **fix(prot):** restore the `gobj_has_bottom_attr()` guard on the
      peername/sockname log reads in `c_prot_tcp4h` / `c_ievent_srv` — the bare
      `gobj_read_str_attr()` logged "Attribute NOT FOUND" + stack trace in the
      race windows where the bottom chain is unset (dropped by 390b8c679, which
      overlooked that internal log).
    - **fix(glogger):** the `TRACE_GBUFFERS` pretty-print also accepts
      `[`-rooted JSON arrays (they fell through to the hex dump).
    - **chore(trace samples):** drop the `"monitor"` / `"event_monitor"` lines
      from the commented trace blocks in `ycommand` and the yuno skeletons — those
      global levels don't exist (uncommenting yielded "global trace level NOT
      FOUND"); the stale `gobj.h` name list is now synced with
      `s_global_trace_level`.

    - **feat(c_auth_bff):** new opt-in `POST /auth/token` endpoint returns the
      access_token to JavaScript so a single SPA can forward it in a `C_IEVENT_CLI`
      identity_card to Yuneta backends on **other** hosts (multi-backend browsing,
      e.g. `gui_treedb`). A deliberate, scoped SEC-06 relaxation, off by default and
      double-guarded: `expose_access_token` attr (default `false`; when off the
      endpoint is an invisible `404`) **and** fail-closed Origin pinning (the token
      is emitted only when the request `Origin` exactly matches `allowed_origin`,
      else `403 origin_not_allowed`; enabling the flag without pinning an origin
      yields nothing). Every existing BFF (wattyzer, estadodelaire, hidraulia) keeps
      full SEC-06 — they never enable the flag. The server side already accepts an
      identity-card JWT with priority over the cookie (`c_ievent_srv.c`) and
      validates it against the issuer JWKS (`c_authz.c`); each remote backend must
      have that JWKS provisioned. Docs: `YUNO_AUTH.md` §2.2.
    - **fix(c_ievent_srv):** `ac_mt_command`'s "Service not found" error path
      answered with `EV_MT_STATS_ANSWER` (copy-paste from the stats handler)
      instead of `EV_MT_COMMAND_ANSWER`, so a remote command to an unknown service
      got its error back under the wrong answer event.
    - **fix(c_tcp):** a running client dropped via `EV_DROP` could stall in
      `ST_STOPPED` (the reconnect `EV_TIMEOUT` was then ignored) and never
      reconnect; `try_to_stop_yevents()` now finalizes to `ST_STOPPED` only when the
      gobj is actually stopping. Generic to every C_TCP client dropped while alive.
    - **fix(c_pty):** `EV_TTY_CLOSE`'s `json_pack` had a stray extra `}` → NULL kw
      (no `name`/`uuid`/`slave_name`), so consumers keying off the console name
      (e.g. gui_agent's Terminal) never saw a usable close.
    - **fix(controlcenter):** `write-tty` now matches a node by UUID **or** hostname
      (like `command-agent`) and, on a no-match, logs instead of `EV_DROP`-ping the
      requester's shared control socket.
    - **fix(controlcenter):** the `write-tty` no-match log is now a **warning**
      (`MSGSET_PROTOCOL`), matching the agent side's demotion of the same benign
      client race — an agent briefly disconnecting while a Terminal tab is focused
      no longer emits one ERROR per keystroke into logcenter.
    - **fix(glogger):** `gobj_trace_json`'s `TRACE_GBUFFERS` pretty-print path
      parsed the gbuffer with `string2json(…, verbose=TRUE)`: a gbuffer starting
      with `{` that is not one complete JSON value (partially-consumed buffer,
      back-to-back messages, binary starting `0x7B`) injected a spurious
      `gobj_log_error` + stack trace from a pure trace path. Now non-verbose — the
      existing hex-dump fallback covers the parse failure.
    - **fix(yuno_agent):** `write-tty` no longer drops the whole control link on a
      benign per-write error ("console not found" → warning); it logs and drops just
      that message.

## 7.7.0
_C / SDK release — **capability marker**. No C/SDK source change since 7.6.8;
this minor advertises the agent version boundary from which a **controlcenter**
can drive the command/stats control plane of a node's managed yunos. JavaScript
framework packages (`@yuneta/gobj-js`, `@yuneta/gobj-ui`) are unchanged this
cycle; the `gui_agent` yuno records its own UI changes in
`yunos/js/gui_agent/README.md`._

    - **Controlcenter-driven `command-yuno` / `stats-yuno` of managed yunos —
      minimum agent version is now `7.7.0`.** The agent-side plumbing that
      returns a controlcenter-cascaded answer to the original requester
      (SPA → controlcenter → agent → yuno, back again) shipped in 7.6.8: the
      agent hands the answer to its outbound `controlcenter` `C_IEVENT_CLI`,
      which serializes the inner inter-event via its `EV_SEND_IEV` action
      exactly as `C_CHANNEL` does server-side
      (`kernel/c/root-linux/src/c_ievent_cli.c`, `yunos/c/yuno_agent/src/c_agent.c`).
      7.7.0 **promotes this to an advertised compatibility boundary**: the agent
      reports `YUNETA_VERSION` as its `__version__`
      (`yunos/c/yuno_agent/src/main.c`: `APP_VERSION = YUNETA_VERSION`), so a
      controlcenter can gate on `agent >= 7.7.0` before routing commands/stats
      down to a node. An agent below 7.7.0 must not be assumed to answer these
      cascaded control-plane calls.
    - **chore(gui_agent): controlcenter web console rollup.** The console that
      consumes this capability matured into the `yunos-js` submodule: a live
      **Stats** panel per selected node, console command **history**,
      command **shortkeys** (ycli parity) managed from Preferences, a
      **copy-response** button, and TreeDB removed from the console. Tracked in
      `yunos/js/gui_agent/README.md`.
    - **chore(repo): `yunos/js` extracted into the `yunos-js` submodule**
      (`github.com/artgins/yunos-js`, tracks `main`). It sits at its original
      path, so the yunos' local `@yuneta/gobj-js` / `@yuneta/gobj-ui` `file:`
      deps resolve unchanged. Edit in `yunos/js`, commit on `main` in the
      standalone repo, then bump the submodule pointer here — the same flow as
      `gobj-js` / `gobj-ui`.
    - **chore(cli): yunetas CLI `0.12.0`** — pre-build ext-libs version guard,
      published to PyPI.

## 7.6.8
_C / SDK release. JavaScript framework changes are tracked in their own
repositories (`@yuneta/gobj-js`, `@yuneta/gobj-ui` CHANGELOGs); the `gui_agent`
yuno records its own UI changes in `yunos/js/gui_agent/README.md`._

    - **fix(agent): controlcenter-cascaded `command-yuno` / `stats-yuno`
      answers now return to the requester.** A command routed down from a
      controlcenter (SPA → controlcenter → agent → target yuno) reached the
      yuno and produced its answer, but the agent dropped it on the way back
      (`ac_command_yuno_answer` / `ac_stats_yuno_answer` logged
      `child not found`), so the caller only saw the synchronous dispatch ack.
      The reverse hop resolved the requester **only** among `__input_side__`
      children; a command that arrived over the agent's outbound
      `controlcenter` `C_IEVENT_CLI` link has its requester on the client side,
      not there. The answer is now handed to that `C_IEVENT_CLI`, which gained
      an `EV_SEND_IEV` action (`kernel/c/root-linux/src/c_ievent_cli.c`) that
      unwraps and serializes the inner inter-event exactly as `C_CHANNEL` does
      on the server side — one uniform path for both server- and
      client-initiated links, covering command and stats answers alike
      (`yunos/c/yuno_agent/src/c_agent.c`).
    - **feat(authz): add `update-user`, split from `create-user`.**
      `create-user` already rejected an existing user, so there was no way to
      modify one: `create-user` now cleanly creates (reject if exists) and the
      new `update-user` modifies (reject if missing); both funnel through
      `EV_ADD_USER`. Two latent defects fixed while reviewing the new path
      (`kernel/c/root-linux/src/c_authz.c`): a password-less update no longer
      wipes stored credentials or silently re-enables a disabled account
      (`disabled` is written only when supplied or the user is new), and the
      existence-check node is no longer leaked. The `ROLE` format
      (`roles^ROLE^users`) is now spelled out in the command help.
    - **chore(ext-libs): bump jansson 2.15.0 → 2.15.1 (v1.20).** Patch release,
      no API/ABI change. Caps recursion depth in `json_dump` / `json_equal` /
      `json_deep_copy` (anti-DoS hardening on functions every `kw`
      serialize/compare/copy runs through), rejects a negative string length in
      the `json_pack` `s#` / `+#` formats, and adds the offending key/index to
      `json_unpack` type-mismatch errors. `libjansson.a` links statically into
      every yuno, so all must be rebuilt + relinked.
    - **chore(ext-libs): bump liburing 2.14 → 2.15 (v1.19)**. Pin-only — no
      API/ABI change and no removed/renamed symbols, so no yuneta consumer,
      header or CMakeLists change rides along. Two of the 2.15 bug fixes land
      on the exact APIs the event loop uses (`kernel/c/yev_loop/src/yev_loop.c`):
      `io_uring_peek_cqe()` drops out-of-line round trips and a redundant
      acquire ordering (loop drain path), and a stale-CQE-pointer fix on
      wait-with-timeout errors (`io_uring_wait_cqe_timeout` in the wait path).
      The new 2.15 helpers (`register_bpf_filter` / `register_query` /
      `register_zcrx_ctrl`) are additive and unused here. `liburing.a` is
      linked statically into every yuno, so yunos that link `yev_loop` must be
      rebuilt + relinked (`extrae.sh` + `configure-libs.sh`) to pick it up.
    - **chore(trace): richer unknown-event drop diagnostics + JSON-gbuffer
      pretty-print.** On the empty-`iev_event` drop path both `C_IEVENT_SRV`
      and `C_IEVENT_CLI` now dump the offending kw via `gobj_trace_json` — the
      informative error is already logged upstream by
      `gclass_find_public_event`, so the CLI's duplicate `gobj_log_error` was
      removed. `gobj_trace_json` also pretty-prints a JSON gbuffer under
      `TRACE_GBUFFERS` instead of a hex dump (and decrefs the parsed value, so
      the trace path no longer leaks). (`kernel/c/gobj-c/src/glogger.c`,
      `kernel/c/root-linux/src/c_ievent_{cli,srv}.c`)

## 7.6.7
    - **fix(security): close three buffer/parse defects found in a source
      audit.** (1) `release-packages.yml` interpolated `github.event.release.
      tag_name` / the `workflow_dispatch` input straight into a `run:` shell
      body — an actor with write access could inject commands via a crafted tag;
      the values now flow through `env:` and are referenced as quoted shell
      variables. (2) `c_auth_bff`'s `make_set_cookie()` reused `snprintf`'s
      return value without clamping, so an oversized token value (truncation)
      drove the later `buf+n` / `sizeof(buf)-n` offsets past the stack buffer —
      an out-of-bounds write in the auth path; `n` is now clamped to
      `[0, sizeof(buf)-1]`. (3) `c_agent`'s `multiple_dir()` advanced
      `p += ln; bflen -= ln;` on the `snprintf` return without a truncation
      check, so a domain component that overflowed the buffer sent `p` past the
      end and `bflen` (int) negative — widening to a huge `size_t` in the next
      `snprintf`; it now breaks on truncation.
    - **harden: route every `strtok` through `strtok_r`; add split-helper
      tests.** `get_cpus()` (`c_yuno`), `split2()` (`helpers`) and
      `json_unflatten_dict()` (`kwid`) relied on `strtok`'s hidden static state
      — not a live bug (single-threaded, self-contained parses) but a
      reentrancy footgun, notably for the public `split2()` (16 call sites). All
      now use `strtok_r` with a local saveptr; no behaviour or signature change.
      Added `tests/c/helpers/` (split2 + a reentrancy regression asserting
      split2 no longer clobbers a caller's in-progress `strtok` parse) and a
      `json_unflatten_dict` case in `tests/c/kw`. The vendored `linenoise.c/.h`
      reference snapshot was refreshed (it is non-compiled — the console uses
      `c_editline`) and `modules/c/console/README.md` now documents that.
    - **fix(js): bump `@yuneta/gobj-js` submodule to 7.6.7 — restore the
      `EV_ON_CLOSE`-on-deliberate-stop contract in `c_ievent_cli`.** 7.6.6
      nulled the WebSocket `.onclose` handler in `mt_stop()`, so a deliberate
      stop never delivered the async `EV_ON_CLOSE` to the FSM and never
      published it to subscribers — diverging from the C kernel (where
      `mt_stop` stops the bottom transport and `ac_on_close` publishes
      `EV_ON_CLOSE` when a session was open). Consumers that drive their logout
      UI teardown from that event (estadodelaire) stopped hiding the app. 7.6.7
      keeps `.onclose` wired and instead guards the handler with
      `gobj_is_destroying()`, so only the stop+destroy-in-the-same-turn case
      (gui_agent) bails out silently while a stop that keeps the gobj alive
      still gets its `EV_ON_CLOSE`. wattyzer/gui_agent (explicit teardown) are
      unaffected. gobj-js-only patch: `YUNETA_VERSION` stays 7.6.6.
    - **refactor(decoders): protocol decode errors caused by a malformed packet
      from the peer are now warnings, not errors.** Across the protocol gclasses
      (`c_prot_tcp4h`, `c_websocket`, `ghttp_parser`, `c_prot_mqtt`,
      `c_prot_mqtt2`) a malformed/unexpected frame from the peer logs as
      `gobj_log_warning` with its assigned category (`MSGSET_PROTOCOL`, or
      `MSGSET_MQTT` for mqtt) plus a length-capped dump of the offending frame
      (`MAX_LOG_DUMP_SIZE`, 256). MQTT uses a single capped dump at the
      `frame_completed()` dispatch chokepoint, and the peer-malformed warnings
      no longer carry `LOG_OPT_TRACE_STACK`. This also covers `mqtt_read_string`'s
      "malformed utf8" path (both gclasses), previously mis-tagged
      `MSGSET_INTERNAL` with a stack trace: a peer sending an invalid-UTF8 string
      field is peer-malformed, now a `MSGSET_MQTT` warning with a capped dump of
      the offending bytes. Reserved for our own faults
      (`gobj_log_error`): broken internal invariants (e.g. `c_ievent_srv`'s
      "gbuffer NULL", where the gbuffer must arrive in the `kw`), allocation
      failures, outgoing-encode paths, and unsupported/unknown protocol fields
      ("NOT IMPLEMENTED"/"NOT FOUND") that mark our own TODO/map-gaps. The point:
      error logs should mean "our fault", so routine peer misbehaviour stops
      polluting error counts. Test `c_mqtt/malformed` updated to expect the
      rejection as a warning.
    - **feat(agent): report each binary's on-disk file time in
      `*list-binaries` / `*list-binaries-instances`.** Both commands now add
      `time` (epoch seconds) and `time_str` (local timestamp) next to `size`,
      computed live by `stat()`ing the stored `binary` path (no treedb schema
      change; covers binaries installed before the field existed; never mutates
      the in-memory node — the listed records are fresh `node_collapsed_view`
      dicts). The motivation is `sync_binaries`: `size` alone calls a rebuild
      that kept the byte count identical (a one-char log edit, or a relink
      against a changed static lib) "up-to-date", so it was never offered for
      `update-binary`. Added `add_binary_file_time()` in `c_agent.c`.
    - **fix(sync-binaries): detect a same-version rebuild by file time, not just
      size.** `classify()` now flags a `REBUILD` when the local file is newer
      than the agent's installed slot even when `Δsize` is 0: it prefers the
      numeric `time` (file mtime) the agent reports next to `size`, and falls
      back to the embedded build `date` (`__DATE__ " " __TIME__`, in
      `--print-role` / `*list-binaries`) for an older agent. The candidate table
      gains a `note` column spelling out a date-triggered rebuild ("newer
      build") so a 0-`Δsize` `REBUILD` doesn't read as a no-op. `tools/README.md`
      updated. The same newer-than-slot check now also applies in the snap-pinned
      (`INSTALLED`) branch.
    - **refactor(ytls): clarify the rejected-handshake log line (both backends).**
      The default-on `gobj_log_warning` in `do_handshake` dropped the misleading
      parenthetical hint from its `msg` — OpenSSL's
      `(check ssl_min_version for legacy peers)` and mbedTLS's
      `(mbedTLS floors at TLS1.2; use OpenSSL backend for legacy peers)` both now
      read just `TLS handshake rejected`. The hint implied every rejection was a
      protocol-floor issue, but most are internet background noise
      (HTTP-on-TLS-port, port-scan garbage, open-proxy `CONNECT`); only
      `unsupported protocol` / `version too low` are actual legacy peers. The
      OpenSSL `tls_version` field was renamed to `negotiated_version` (since
      `SSL_get_version()` returns the **server** object's version — equal to the
      peer's offer only when the ClientHello was parsed far enough, otherwise the
      server default, so a plaintext-HTTP probe is logged as `TLSv1.3`), and the
      same `negotiated_version` field was **added** to the mbedTLS line via
      `mbedtls_ssl_get_version()` (which honestly returns `"unknown"` pre-
      negotiation) so both backends log a symmetric field set. Log-text/field-name
      only; no behaviour change.
    - **fix(ytls): raise the `ssl_verify_depth` default from 1 to 2.** OpenSSL
      counts the trust anchor in the chain depth, so the minimal verification
      path against any public CA is leaf(0) → intermediate(1) → root(2). With
      the old default of 1, a verifying TLS **client** (`ssl_verify_mode`
      `required`/`optional` with a CA) rejected every normal modern chain as
      `X509_V_ERR_CERT_CHAIN_TOO_LONG` at depth 2 — observed on `auth_bff`
      connecting to a Let's Encrypt-fronted Keycloak (`certificate verify
      failed` after "certificate chain too long"). 2 is the de-facto floor;
      cross-signed / extra-intermediate chains still need an explicit higher
      `ssl_verify_depth`. Only the computed default changed in `openssl.c`;
      `ytls.h` and `guide_tls.md` updated. The mbed-TLS backend has no depth
      knob and is unaffected.
    - **refactor(ytls): drop the handshake "forensic transcript".** Both TLS
      backends captured every inbound handshake byte into a 16 KB per-socket
      buffer and dumped it (hex) on handshake failure. The dump was useless — in
      TLS 1.3 everything after ServerHello is ciphertext, and the cleartext
      records are better read with the `C_TCP` `traffic` trace or a pcap — while
      every connection paid for the allocation and a per-chunk memcpy. Removed
      `HANDSHAKE_TRANSCRIPT_MAX`, the `handshake_transcript` field,
      `capture_handshake_bytes()` and all decref sites from `openssl.c` /
      `mbedtls.c`. The default-on `gobj_log_warning` recording the rejection
      reason (error, peername, sockname, SNI, negotiated_version) is kept. The two
      `test_handshake_dump_{openssl,mbedtls}` tests were repurposed as
      `test_handshake_reject_*` (a bogus HTTP-on-TLS-port handshake is rejected
      cleanly: `error=-1`, no crash) — both backends pass.
    - **fix(agent-sync): classify `sync-binaries`/`sync-configs` against every
      installed slot, not just the active primary.** `*list-binaries` /
      `*list-configs` report only the primary; with a snap active the primary
      can be an OLD version while the freshly built one is already installed as a
      non-primary slot, so every role was mis-classed `BUMP` and then fired a
      doomed `install-binary` / `create-config` (the agent rejects with "Node
      already exists"). The full set is now read from `*list-binaries-instances`
      / `*list-configs-instances` and used as the authoritative "is this version
      already installed?" check: a version installed but not primary is the new
      `INSTALLED` status (skipped, with a hint to promote via
      `yunetas upgrade-yunos`); `BUMP` now means "not installed and newer than
      the primary". `tools/README.md` tables updated.
    - **fix(agent-sync): skip the kill/restart cycle when the rebuilt version
      isn't the one running.** The REBUILD path (`update-binary`) stopped and
      restarted the role if ANY instance was live, but `update-binary` overwrites
      only the slot whose version equals the uploaded binary's, and
      text-file-busy bites only when a LIVE process is mapped to that exact file.
      `deploy_update_with_restart` now reads `role_version` per instance from
      `*list-yunos` and only kills/restarts when an instance running the version
      being written is live (unknown `role_version` → treated as on-target,
      killed on the safe side).

## 7.6.6
    - **build(js): extract `@yuneta/gobj-js` to its own repository as a git
      submodule (symmetric with gobj-ui).** gobj-js was the last in-tree JS
      framework package; it now lives at `github.com/artgins/gobj-js` (public)
      and is embedded as a git submodule at `kernel/js/gobj-js`, the same model
      as gobj-ui and `utils/python/tui_yunetas`. The new repo is a clean
      snapshot (history not preserved), single line on `main`, tag `7.6.5`
      tracking `YUNETA_VERSION`. **Clone with `--recurse-submodules`** (or
      `git submodule update --init`). The submodule sits at the original path,
      so local `file:` consumers (`wattyzer`, in-repo `yunos/js/gui_treedb`)
      resolve unchanged and npm consumers (`estadodelaire`, `hidraulia`) are
      unaffected. New publish flow: bump `package.json` in lockstep with
      `YUNETA_VERSION` and `npm publish` **in the standalone repo**, then bump
      the submodule pointer here. `.gitmodules` uses the HTTPS url (so
      `--recurse-submodules` works for every cloner) for all three submodules;
      the gobj-ui submodule's stale internal name (`lib-yui`) was also aligned
      to `gobj-ui` and its url switched SSH → HTTPS in the same pass.
    - **chore(ext-libs): bump nginx 1.30.2 → 1.31.2 (v1.17)**. Fixes three
      CVEs: CVE-2026-42530 (use-after-free in `ngx_http_v3_module`),
      CVE-2026-42055 (buffer overflow in the HTTP/2 paths of
      `ngx_http_proxy_module` / `ngx_http_grpc_module`) and CVE-2026-48142
      (buffer overread in `ngx_http_charset_module`). Pin-only — nginx is a
      separate dynamically-linked binary (see `configure-libs.sh` v1.10), so
      no yuneta consumer / header / CMake change rides along. NOTE: `1.31.x`
      is the nginx *mainline* branch (odd minor), not the `1.30.x` stable
      line we were on — chosen because the fixes landed there. openresty
      (`1.29.2.5`) is a separate binary and is **not** covered by this bump;
      track upstream openresty for a release that picks up these patches.
      Each deployed project must rebuild its own nginx copy.
    - **chore(ext-libs): bump openresty 1.29.2.5 → 1.31.1.1 (v1.18)**. Advances
      the openresty-bundled nginx core from `1.29.2` to `1.31.1` (released
      2026-05-29). Pin-only — openresty is a separate dynamically-linked binary
      (so its bundled OpenSSL 3.5.6 is irrelevant to our build). ⚠️ **CVE
      status:** nginx 1.31.1 does NOT cover the three CVEs fixed in nginx
      `1.31.2` (CVE-2026-42530 / 42055 / 48142, 2026-06-17); openresty 1.31.1.1
      was tagged *before* nginx 1.31.2, so the openresty binary — the one that
      actually fronts the SPAs — remains exposed until upstream ships a release
      based on ≥ nginx 1.31.2. The standalone nginx binary IS patched (v1.17).
      Each deployed project must rebuild its own openresty copy.
    - **observability(prot): attribute protocol parse errors to the source IP
      (`peername`).** Server-side protocol gclasses logged malformed-input
      errors without the remote peer's address — `peername` is set on the bottom
      `C_TCP` (`SDF_VOLATIL`) and the upper layers never copied it into their own
      logs, so a bad-frame / bad-header event was not attributable to a device
      or attacker without cross-referencing the `C_TCP` `Connected` line by
      timestamp. The canonical read pattern (already in `c_websocket.c` /
      `c_prot_mqtt2.c`) is now applied in the cold error branch of each
      remote-data parse-error log: `c_prot_tcp4h.c` (head-too-long,
      protocol-error disconnect, protocol timeouts) and `ghttp_parser.c` (the
      invalid-UTF-8 header-value store error; the main "non-HTTP data received"
      violation already carried it). `c_prot_http_sr.c` / `c_channel.c` only
      emit registration / internal "no bottom" logs (not remote-attributable)
      and are left untouched; outbound clients (`c_prot_http_cl.c`) and the
      `c_prot_mqtt2.c` gap-fill are deferred. No FSM/schema/API change — logs
      gain a `peername` field only. See TODO.md "source-IP attribution".
    - **security(glogger): escape invalid UTF-8 in log fields (logcenter parse
      DoS).** `_ul_str_escape()` copied every byte `0x7f-0xff` verbatim (via the
      `json_exceptions[]` table) without validating UTF-8. A corrupted device
      payload logged verbatim (e.g. an FS00802_4G sensor leaking modem AT
      commands + raw bytes into its MQTT JSON) leaked lone invalid bytes
      (`0x8a`, `0xc2`, ...) into the log record; the record was then no longer
      valid UTF-8, so the logcenter's `gbuf2json()` rejected and dropped it
      ("unable to decode byte 0x8a"), losing the log. Fixed at the root so no
      field from any emitter can produce an unparseable record: `json_exceptions[]`
      and the non-thread-safe static `exmap` are gone; a strict
      `utf8_valid_seq_len()` validator (rejects overlong encodings, surrogates,
      `> U+10FFFF`, never reads past the NUL terminator) now drives the escaper,
      which copies valid UTF-8 sequences verbatim (logs stay readable) and
      escapes invalid/control bytes as `\u00XX`. Worst-case output size is
      unchanged (<=6 bytes/char), so the caller's buffer sizing is untouched.
      Regression test `tests/c/glogger_utf8` registers a capture log handler and
      re-parses the emitted record with `anystring2json` (what the logcenter
      does), proving an invalid-UTF-8 payload now yields a valid JSON/UTF-8
      record while legitimate UTF-8 (`café`, `€`) is preserved verbatim.
    - **security(mqtt): reject zero-length payload frames that crashed the
      broker (NULL-gbuf remote DoS).** A control packet whose MQTT "remaining
      length" was 0 left `frame_completed()` with a NULL payload gbuffer, which
      it then handed to a handler that dereferenced it
      (`gbuffer_leftbytes(NULL)`) → SIGSEGV. A single malformed packet from a
      remote client crashed the whole broker process (observed in
      `handle__subscribe`). Fixed in both protocol gclasses (`C_PROT_MQTT` and
      `C_PROT_MQTT2`) with two layers: at header validation, `frame_length == 0`
      is now rejected for every command carrying a mandatory payload, before a
      NULL gbuf can reach a handler (MQTT5 still permits a zero-length
      DISCONNECT/AUTH, whose handlers already tolerate it; PINGREQ/PINGRESP keep
      their must-be-zero check); and the read primitives
      (`mqtt_read_uint16/uint32/bytes/byte/varint`) now treat a NULL gbuf as a
      malformed packet before touching `gbuffer_leftbytes`. Regression test
      `tests/c/c_mqtt/test_mqtt_malformed` injects a malformed in-session
      SUBSCRIBE (`0x82 0x00`) at the transport: it crashes with the exact
      production backtrace without the fix and passes with it.
    - **build(js): the JS UI library was extracted to its own repository and
      renamed `@yuneta/lib-yui` → `@yuneta/gobj-ui`.** It now lives at
      `github.com/artgins/gobj-ui.js` and is embedded as a git submodule at
      `kernel/js/gobj-ui` (clone with `--recurse-submodules`), the same model as
      `utils/python/tui_yunetas`. The repo carries two maintained lines, each
      consumed a different way:
        - **`main`/v2** (npm dist-tag `latest`, tag `2.0.0`+, `src/` layout) —
          active development: the declarative shell
          (`C_YUI_SHELL/NAV/PAGER/WIZARD`) on top of the legacy stack. **The
          yunetas submodule now tracks this line**, and `wattyzer` consumes that
          checkout locally via a `file:` dependency (importing
          `@yuneta/gobj-ui/src/*` by package specifier).
        - **`v1`** (npm dist-tag `legacy`, tag `1.0.1`, `src/` layout) — the
          frozen legacy GClass GUI stack. Consumed from the **npm registry** as
          `@yuneta/gobj-ui@^1.0.1` by `estadodelaire`, `hidraulia` and the
          in-repo `yunos/js/gui_treedb` (NOT a local `file:` — the local
          submodule is v2 now).
      The old `lib-yui` name collided with Yahoo's YUI on npm; only the package
      identity changed — internal naming (`C_YUI_*`, `c_yui_*`, `yui_*`, `yi-*`)
      is unchanged. Both lines use the `src/` layout (v2 was restructured to
      match v1). Published to npm as `@yuneta/gobj-ui` (`latest`=2.0.0,
      `legacy`=1.0.1); the abandoned `@yuneta/lib-yui` was unpublished.
    - **build(js): `@yuneta/gobj-js` is versioned to `YUNETA_VERSION` and
      published to npm.** `kernel/js/gobj-js/package.json` now tracks the SDK
      version (currently `7.6.5`); bump it in lockstep and `npm publish`.
      `estadodelaire`/`hidraulia` consume it from the registry
      (`@yuneta/gobj-js@^7.6.5`); `wattyzer` and `yunos/js/gui_treedb` keep a
      local `file:` dependency on `kernel/js/gobj-js`.

## 7.6.5
    - **security(libjwt): re-review against upstream v3.4.0 and backport the
      reachable hardenings.** v3.4.0 is a large feature release (full JWE, the
      `crit` header, `jti` callbacks, PEM→JWK public API), almost none of which
      touches Yuneta's compiled subset. After filtering to the JWS verify/parse
      path, three items were backported into the vendored tree:
        - **`18133e4` (L17): reject duplicate JSON members on the token parse**
          (`jwt-verify.c`). The inbound header/payload is now parsed with
          `JSON_REJECT_DUPLICATES` (RFC 8725 §2.4), so a peer that selects a
          different occurrence of a duplicated claim/header cannot be made to
          disagree with us.
        - **`d180cc7`: enforce strict base64url on decode** (`jwt.c`). The
          decoder accepted the standard-base64 `+`/`/` and silently truncated
          on an embedded `=`; it now rejects anything outside `[A-Za-z0-9_-]`.
          Reachable on every token segment and JWK member decode.
        - **`fe8840a`: enforce the RFC 7515 §4.1.11 `crit` (Critical) header**
          (`jwt-verify.c` + both checker entry points). The parser previously
          ignored `crit`; since this copy understands no extension headers, any
          token carrying a well-formed `crit` is now rejected (checker side
          only — the builder side is not ported, C_AUTHZ does not sign).
      The batch's only CVE-class bug (`5fada81`, mbedTLS RSA short-signature
      OOB read) is not present here: the vendored mbedTLS backend is a v4.0/PSA
      rewrite using the length-aware `mbedtls_pk_verify_ext`, immune by
      construction. Regression coverage added to `test_jwt_alg_confusion`
      (`crit` rejection on both entry points; positive controls still verify).
      Full classification in `kernel/c/libjwt/README.md`.

## 7.6.4
    - **fix(tr_msg2db): stop `msg2db_open_db` logging spurious schema errors
      when reopening with `jn_schema=NULL`.** The persistent reopen path (no
      schema dict passed — the schema is loaded from
      `<db>.msg2db_schema.json`) read the name and `schema_version` straight off
      the NULL `jn_schema`, so every open via `msg2db_list` (and any non-master
      reopen) emitted three red errors — `kw must be list or dict` /
      `path NOT FOUND` for `id`, and the same for `schema_version` — before the
      function then correctly loaded the schema from file. Now mirrors
      `treedb_open_db`: the name comes from the passed `msg2db_name_` when
      `jn_schema` is NULL (and is read with a non-`KW_REQUIRED` flag otherwise),
      and `schema_version` is guarded with `jn_schema? kw_get_int(...) : 0`. The
      resolved name and version are unchanged for the one non-NULL-schema caller
      (`c_mqtt_broker`, whose passed name already equals `jn_schema["id"]`);
      only the noise is gone.
    - **refactor(msg2db_list): modernize the CLI to the `treedb_list` style.**
      The tool still carried its V6-era flags — most visibly `--path` / `-a`
      as the only way to point it at a store. It now takes the store as a
      **positional `PATH` argument** (the `-a` flag is gone) and shares the
      `treedb_list` ergonomics:
        - `resolve_msg2db_path()` deduces the tranger root, `--database` and
          `--topic` from `PATH` (a tranger root, a `<db>.msg2db_schema.json`,
          or a topic directory), auto-discovering the single schema when
          `--database` is omitted and listing the candidates when it is
          ambiguous or missing.
        - new presentation flags `--mode form|table` / `-m` and
          `--fields` / `-f` (field selection implies table mode), rendering
          columns from the topic's `cols` `fillspace` just like `treedb_list`.
        - new `--dry-run` / `-n` prints the resolved path / database / topic
          plus the ids and filter JSON, then exits without listing.
        - `PATH` is normalized (trailing slashes stripped, `./` prefixed for
          bare relative names); the per-topic header is highlighted and the
          recursive walk keeps its per-database record count.
      `--follow` is intentionally NOT ported: `tr_msg2db` exposes no
      change-callback equivalent to `treedb_set_callback`. The mega-header
      `<yunetas.h>` include was replaced by the specific kernel headers.
    - **chore(ytls): de-duplicate and de-noise the rejected-handshake logs.** A
      single rejected connection (e.g. a non-TLS/HTTP client hitting the TLS
      port) emitted overlapping lines across the ytls and transport layers.
      Now:
        - "TLS handshake rejected" is INFO (was WARNING) in both backends — a
          sub-floor/legacy/non-TLS peer is routine, not actionable; it was
          inflating "Global Warnings".
        - that default-on line is self-contained: it now carries
          `peername`/`sockname`, handed to ytls by the transport through a new
          optional `ytls_set_peer_name()` (per-`sskt`, both backends), so the
          offending peer is identifiable even with `connections` trace off.
          (ytls does NOT reinterpret `user_data` as a gobj — unit tests pass a
          non-gobj `user_data`; callers that skip the setter just log `""`.)
        - the transport's `ytls_on_handshake_done_callback` no longer logs the
          FAILS case (it duplicated the ytls line); it keeps "TLS Handshake OK".
        - `set_trace` no longer logs the per-connection `trace:0` disable (pure
          noise on every accept); it logs only when enabling. Both backends.
    - **feat(libjwt): trace the claims JSON on a failed-claims verification.**
      `__verify_config_post` now calls `gobj_trace_json` with `jwt->claims` when
      `__verify_claims` reports one or more failed claims, so the offending
      token's `iss`/`aud`/`exp`/`nbf`/… values are visible at the point of
      rejection. Emits unconditionally on the failure path (LOG_DEBUG). libjwt
      now back-references `gobj_trace_json`: real yunos already pull `glogger.o`,
      but the standalone libjwt unit test pulls nothing from it, so its link line
      repeats `libyunetas-gobj.a` after `JWT_LIBS` to resolve the reference.
    - **fix(utils): TLS client utilities could not connect over `wss://` /
      `https://`.** The verify-by-default change made `build_ssl_ctx` refuse any
      TLS client whose `crypto` config lacks server-certificate validation, but
      the CLI utilities were never ported: they passed an empty `crypto` to
      their sockets, so every remote TLS connection (e.g. `ycommand` against the
      controlcenter, including its OIDC `task-authenticate` to the issuer) failed
      with *"TLS client refused: no server-certificate validation"*. All
      utilities now pass `crypto: {ssl_use_system_ca: true}` to their C_TCP (and
      to `C_TASK_AUTHENTICATE` where present): `ycommand`, `ystats`, `ybatch`,
      `ytests`, `ycli`, `mqtt_tui`, `emu_device`. `ycommand` additionally gains
      `--ssl-use-system-ca` (default on), `--ssl-trusted-certificate` (private
      CA) and `--ssl-allow-insecure-client` (MITM bypass) for non-public-CA
      endpoints. Plain `ws://` is unaffected (C_TCP ignores `crypto` without TLS).
    - **feat(ytls): log `ssl_server_name` in TLS diagnostics; drop dead
      fields.** Every post-init `gobj_log_*` in the OpenSSL and mbedTLS backends
      now carries `ssl_server_name`, so handshake/verify/read/write errors show
      which SNI/server name the context was for. Also removed the never-used
      `rx_bf[16*1024]` field from `sskt_t` in both backends (~16 KB per live TLS
      connection) and the unused `error` field from the mbedTLS `sskt_t`.
    - **fix(tui_yunetas 0.10.1): quieter `upgrade-yunos` output.** The two
      `find-new-yunos` steps no longer dump ycommand's raw stdout — the preview
      prints once (formatted) and `create=1` shows a one-line `Created N new
      yuno row(s).` summary. The post-`sync-binaries` install-binary reminder
      now leads with `yunetas upgrade-yunos` (raw ycommand sequence kept as the
      manual equivalent).
    - **feat(tui_yunetas 0.10.0): agent-aware deploy.** `sync-configs` without
      `--host` now matches each registered project's `yunos/batches/<host>/`
      directories against the realm_ids the local agent manages
      (`*list-realms`) and syncs every match — a node running several realms
      deploys all the relevant ones in one pass, since a batches dir is named
      after its realm_id (the deploy FQDN). `--host` still targets one dir; an
      unreachable agent falls back to the legacy single-hostname guess; new
      `--url`/`-u`. New `upgrade-yunos` command bundles the version-bump
      promotion flow: optional rollback snapshot (idempotent by name,
      `pre-upgrade-<YYYYMMDD>`, `--no-snap`) -> `find-new-yunos` preview +
      confirm (`--yes`) -> `find-new-yunos create=1` -> `deactivate-snap`
      (restart_nodes: SIGKILL + treedb reload, newest release wins).
      `--dry-run` prints the agent commands without running them.
    - **fix(tools): a resumed deploy is now idempotent instead of failing.**
      When a prior run installed the binaries / configs and registered the new
      yuno rows but never promoted them (`deactivate-snap` not reached),
      re-running the deploy hit the agent's "... already exists" answers.
      `sync_binaries.py` / `sync_configs.py` now report such an
      `install-binary` / `create-config` as `ALREADY PRESENT` (idempotent) and
      count it as ok, not a red `FAILED`. The matching `upgrade-yunos`
      fall-through (don't abort when `find-new-yunos create=1` only hits
      already-existing rows) ships in the tui_yunetas CLI 0.11.1. A genuine
      (non-idempotent) error still fails closed.

## 7.6.3
    - **feat(treedb): immutable (non-deletable) topics and records.** A record
      can be marked immutable (md2 system_flag bit `sf_immutable_record`,
      surfaced as `__md_treedb__`immutable`) and a topic non-deletable
      (`system_topic` in `topic_var.json`) — the protection is METADATA, not a
      data column, so it needs no user-schema change and no `topic_version`
      bump. `treedb_delete_node` / `treedb_delete_instance` /
      `treedb_delete_topic` refuse it and `force` does NOT override; the record
      bit is inherited across updates and survives reload. New
      `treedb_set_node_immutable()` and a `system_topic` param on
      `treedb_create_topic`; the `__system__` treedb structural topics and per-
      treedb `__snaps__`/`__graphs__` are marked system. `c_authz` `mt_start`
      runs a master-only idempotent ensure-loop that stamps the Authz seed
      (`root` role / `yuneta` user) immutable on every start — deployed stores
      protected on next restart, no schema change, no wipe. Out of scope on
      purpose: `delete-treedb` / whole-store wipe. Test
      `tests/c/tr_treedb_immutable`; design in
      `kernel/c/timeranger2/DESIGN-immutable-topics-records.md`; docs in
      `YUNO_TREEDB.md` §3.10 + `YUNO_AUTH.md` §4.2.
    - **fix(ytls): portable system-CA trust (`ssl_use_system_ca`) for static
      binaries, both backends.** A fully-static binary doesn't inherit the host
      OPENSSLDIR / `SSL_CERT_FILE`, so OpenSSL's `set_default_verify_paths()`
      loaded an EMPTY store and a valid public cert failed to verify. New
      `ytls_get_system_ca_bundle()` probes the well-known CA bundle FILES across
      distros (Debian/Ubuntu, RHEL/Rocky/Alma/Fedora, SUSE, Alpine — the
      hashed-dir CApath is not portable); OpenSSL loads it via
      `load_verify_locations`, and mbedTLS (no system store of its own) now
      parses it too instead of refusing the client. `C_PROT_HTTP_CL` gains a
      `crypto` attr (default verify-by-default) forwarded to its bottom C_TCP,
      and emailsender's `c_smtp_session` the same — so HTTPS polls (e.g. ESIOS)
      and SMTPS verify out of the box. This unbroke auth_bff's IdP TLS and
      stopped a ~1.2 MB/s "TLS handshake FAILS" log flood (-> ~0.6 KB/s).
    - **feat(c_tcp): opt-in exponential reconnect backoff.** New
      `timeout_between_connections_max`: when > `timeout_between_connections`,
      the reconnect delay backs off from base up to the cap, resetting to base
      once a connection is established (for a TLS client, only on a successful
      handshake). A peer that keeps failing — e.g. an IdP whose cert won't
      verify — no longer hammers at the base cadence. `auth_bff` uses it (100 ms
      first retry, 30 s cap).
    - **fix(logcenter): `search` / `tail` no longer crash on a truncated log,
      and read fast.** `extrae_json` brace-counting `abort()`ed the whole daemon
      when a truncated UDP log entry left a `{` with no `}` (it grew past the
      max block) — taking logcenter down on a read-only command. Records are now
      split on the `<PRIORITY>: ` line prefix `rotatory_write()` already writes
      (robust to truncation and multi-line JSON; a `\0` on-disk terminator was
      rejected as it would make the log binary for grep/less/vim). Reads use
      64KB blocks + `memchr` instead of `fgetc()` per byte, and `tail` seeks to
      the last window: on a 518MB log, tail 72s -> 2s, search 70s+/crash -> 2-7s.
    - **refactor(tui_yunetas 0.9.1): project registry moved to
      `~/.yuneta/projects.json`.** The external-project registry was written
      inside the source tree (`$YUNETAS_BASE/.projects.json`, gitignored). But
      which projects to build alongside the SDK is runtime/usage state, not a
      property of any checkout — it does not belong in the tree at all. It now
      lives in the user's home (`~/.yuneta/projects.json`), independent of
      `YUNETAS_BASE`. A one-time soft migration moves an existing legacy file
      on the next CLI run; no manual step. The `.gitignore` / `.hgignore`
      entries for the old path are kept as a safety net while pipx CLIs on
      other nodes still write the legacy location.
    - **fix(treedb): refused `treedb_delete_instance()` no longer drops a
      borrowed node ref.** The snapshot-tag guard's refusal path decref'd the
      node even though callers (`mt_delete_node`, tests) pass the index's
      borrowed pointer — a refused per-instance delete of a snap-tagged node
      left the index slot one ref short (latent use-after-free / double-free).
      The refusal path now leaves the node untouched; the ref is consumed only
      on success, where `delete_secondary_node()` extracts it from the index,
      same convention as `treedb_delete_node()`.
    - **fix(performance): `perf_yev_ping_pong2` no longer reports a first-run
      `tranger2_startup` error.** The startup phase expected NO logs, but on a
      node where `~/tests_yuneta/` had never been created (e.g. a fresh VM)
      `tranger2_startup` emits the one-time INFO "Creating
      `__timeranger2__.json`" — flagged as unexpected by the strict
      expected-results FIFO. Simply adding the log to the expected list would
      break the opposite case (database already created by a previous test or
      run). Fix follows the established pattern
      (`test_tr_treedb_update_instance.c`): wipe the database with `rmrdir`
      before `tranger2_startup` so the creation INFO is always emitted, and
      expect it. Verified with back-to-back runs (fresh and leftover store).
    - **chore(packages): drop `stress_*` lab binaries from the .deb/.rpm
      payload.** The CI builds the whole tree and the packagers copied
      `outputs/` wholesale, so the stress load-generators (`stress_auth_bff`,
      `stress_listen`, …) shipped on every production node. They are now
      stripped at staging time. `perf_*` benchmarks stay on purpose: fully
      static, they are handy to measure a target machine right after install
      (validated on the 7.6.2 Ubuntu VM).
    - **fix(packages): pipx CLIs install for the operator, not for root.**
      `install-yuneta-dev-deps.sh` (deb/rpm) runs as root, so `pipx install
      kconfiglib yunetas` landed in `/root/.local/bin` — invisible to the
      `yuneta` operator account (verified on a clean Ubuntu VM: `yunetas:
      command not found` after a full install). The script now installs the
      pipx apps for the `yuneta` user when it exists (falling back to
      `$SUDO_USER`, then root), via `runuser -l` so pipx resolves the right
      `$HOME`. The staged `profile.d/yuneta.sh` already has
      `/home/yuneta/.local/bin` on PATH, so the CLIs work on next login with
      no `pipx ensurepath` step.

## 7.6.2
    - **feat(tui_yunetas 0.9.0): external projects integrated into the
      `yunetas` CLI.** New `register-project` / `unregister-project` /
      `list-projects` commands keep a machine-local registry in
      `$YUNETAS_BASE/.projects.json` (gitignored); `init` / `build` / `clean`
      now also process each registered project's `yunos/` after the SDK
      (select with positional project names, or `--sdk-only` to skip them).
      New `sync-binaries` / `sync-configs` subcommands wrap
      `tools/agent/sync_*.py`, forwarding arguments; `sync-configs` walks the
      registered projects' `yunos/batches/<host>/` directories (`--host`
      selector with hostname auto-match), closing the discovery gap that
      forced a manual `cd` into each batches dir.
    - **fix(env): `yunetas-env.sh` exported a stale artefacts layout.**
      `YUNETAS_OUTPUTS` / `YUNETAS_YUNOS` pointed at the PARENT directory of
      the repo (`$(dirname $YUNETAS_BASE)/outputs`), a layout nothing else
      uses: `project.cmake`, the CLI and the `.deb`/`.rpm` payload all agree
      on `$YUNETAS_BASE/outputs[_ext]`. Both variables now follow that rule,
      a new `YUNETAS_OUTPUTS_EXT` is exported, and `deactivate_yunetas`
      unsets all of them. The legacy `$HOME/yunetaprojects` branch was
      dropped from the profile script staged by the `.deb`/`.rpm` packagers,
      and the docs (`CLAUDE.md`, `installation.md`) were aligned.
    - **feat(packages): one outputs/ path on every node.** The `.deb`/`.rpm`
      used to stage the SDK payload directly under `/yuneta/development/`
      (`outputs/`, `outputs_ext/`, `tools/`, `.config`), so runtime-only nodes
      had a different `YUNETAS_BASE` (and outputs path) than source checkouts.
      Both packagers now stage the same payload as a sparse SDK under
      `/yuneta/development/yunetas/` — the SAME base path as a full source
      tree — so `outputs/` is `/yuneta/development/yunetas/outputs` everywhere
      and the two-branch layout conditional in the staged `profile.d/yuneta.sh`
      collapses to a single unconditional block. The `yunetas` CLI handles
      these runtime-only trees (no `YUNETA_VERSION`): `init <project>` /
      `build <project>` work against the shipped headers, and a plain `init`
      refuses to wipe the shipped `outputs/`. Legacy `/yuneta/development`
      fallbacks remain in the resolution chains for nodes installed with older
      packages; on upgrade dpkg/rpm relocate the payload automatically, but
      already-configured project `build/` dirs cache the old paths — re-run
      `cmake` (or `yunetas init <project>`) after upgrading.
    - **fix(ycli,ycommand): assemble local-config paths via `build_path`.**
      The `save_local_json/string/base64` helpers built
      `$HOME/.yuneta/configs/<name>` with `snprintf` into a NAME_MAX buffer
      while the sanitized name was also NAME_MAX, so GCC emitted
      `-Wformat-truncation`. `build_path()` (the hard-rule path helper) does
      the assembly and logs LOG_CRIT on real overflow instead of silently
      truncating.
    - Note: the 7.6.1 tag already shipped two undocumented renames — the
      legacy `cli` gobj/service is now `ycli`, and its global config key
      `Cli.shortkeys` is now `ycli.shortkeys`.

## 7.6.1
    - **fix(authz,root-linux): a root superuser reaches any service of the
      node.** The 7.6.0 per-message `dst_service` gate (`is_service_authorized`
      in `c_ievent_srv.c`) authorized only the channel's `authorized_services`
      — the *keys* of `services_roles`. But the local trusted `yuneta` user
      authenticated through the `yuneta_by_local_ip` shortcut in `c_authz.c`,
      which hardcoded an EMPTY role set (`{"agent":[]}`, the old
      `// TODO not need role?`), so its real `root` role (`service="*"`,
      `realm_id="*"`) never reached the channel. Result: the local control
      plane (`ycli` warming its command cache with `list-gobj-commands` to
      `dst_service="__yuno__"`, `ycommand`, …) was REJECTED at `__yuno__` and
      any sibling service — root could not reach the yuno root. Now the local
      `yuneta` goes through the SAME `get_user_roles()` filter as any user (no
      hardcode); `get_user_roles()` flags the channel `superuser` when the user
      holds an effective `service="*"` role (computed from the wildcard, not the
      literal role name), propagated in the authenticate response and stored as
      the `is_superuser` channel attr. `is_service_authorized()` returns TRUE for
      a superuser: any realm/service/permission by definition, so it is not a
      cross-service escalation. Scoped roles (`developer`, `sysop`, …) stay
      limited to their granted services — the 7.6.0 cross-service protection is
      intact for them. What a command may DO is still governed by the
      default-off per-command authz, orthogonal to this routing gate.
    - **fix(root-linux): the ievent server never leaves a channel zombie when it
      refuses a message.** `ac_on_message` rejected an unrouted `dst_service`
      (unauthorized or not found) with a bare `return -1`, which both skipped
      any answer AND left the socket read un-rearmed (`c_tcp` only re-arms on a
      `0` return from the `EV_RX_DATA` publish chain): the channel stayed
      connected but deaf and the peer waited forever. New `reject_unrouted_iev()`
      never returns `-1` silently: `command` / `stats` (which have a natural
      answer channel) get a negative `EV_MT_*_ANSWER` with the reason and the
      read re-arms (`return 0`); `subscribe` / `unsubscribe` / `inject` (no
      answer) `drop()` the channel for a clean disconnect. Applied to both the
      unauthorized-service and the service-not-found paths.
    - **build(cmake): link the kernel and external static libraries by full
      path so consumers auto-relink.** `tools/cmake/project.cmake` listed the
      `.a` files as bare names resolved via `link_directories()` `-L`, which
      CMake treats as plain `-l` flags with NO file dependency: after editing a
      kernel source and rebuilding its `.a`, dependent yunos were NOT relinked
      (`make` reported `Built target` with the stale binary; the workaround was
      to delete the binary first). Each archive is now given by full path
      (`${LIB_DEST_DIR}/...` for yuneta's own, `${EXT_LIB_DIR}/...` for
      `outputs_ext/lib` third-party), so CMake tracks it as a link dependency
      and `make` / `yunetas build` relinks automatically when a lib changes.
      System libs (`pthread`, `dl`) stay bare. Verified: a full clean rebuild is
      green and touching `libyunetas-core-linux.a` relinks the agent with a
      plain `make`.

## 7.6.0
    - **security(root-linux): authorize per-message dst_service against the
      authenticated service set on the ievent server.** `ac_on_message`
      (subscribe / unsubscribe / inject) and `ac_mt_stats` resolved the
      `dst_service` / `service` from the attacker-controlled routing stack and
      dispatched against any registered service. A peer authenticated for
      service A could subscribe to events of, inject into, or read/reset the
      stats of another service B by naming it. Both paths now check the resolved
      service against the set this channel is authorized to reach, captured at
      identity-card time from the `services_roles` returned by
      `gobj_authenticate()` (the keys: the primary `dst_service` plus any
      `required_services` the user holds real treedb roles in — see `append_role`
      in `c_authz.c`; the no-treedb path yields just the primary service). This
      implements the long-standing `available_services` TODO: a single
      authentication legitimately grants several services (the multi-service GUI
      frontends authenticate against `db_history_wz` and reach
      `treedb_wattyzer` / `treedb_authzs` / …), while a service outside the
      granted set is refused. The authorized set derives from real roles, not
      the client-supplied `required_services`, so it cannot be spoofed.
      Validated end-to-end: the wattyzer SPA against a patched `db_history_wz`
      logs in, opens its ievent channel, and loads multi-service data with zero
      gate denials. (`ac_mt_command` cross-service reach stays gated by the
      default-off per-command authz — threat-model T7, an accepted posture.)
    - **security(root-linux): resolve the `C_PTY` `process` attr against a
      trusted-dir allowlist, never `$PATH`.** The remote-settable `process`
      value (set by the authz-gated `open-console` command) reached `execvp()`,
      which consults the inherited `$PATH` — a planted PATH entry could hijack a
      bare name. New `resolve_process_path()` accepts an absolute path only if
      executable, resolves a bare name against a fixed list of system dirs
      (`/bin`, `/usr/bin`, `/sbin`, `/usr/sbin`, `/usr/local/bin`), rejects
      relative-with-slash, and fails closed (empty argv[0] → no exec).
      `execvp` → `execv`.
    - **security(ycommand/ycli): sanitize peer-supplied config record names
      before they become local filenames.** A malicious peer's command-answer
      record `name`/`id` (`view-config` / `read-json` / `read-file` /
      `edit-config`) flowed unsanitized into the `"<editor> <path>"` string that
      `pty_sync_spawn()` hands to `/bin/sh -c` — RCE on the operator host (e.g.
      `"x; rm -rf ~ #"`). New `sanitize_config_name()` folds everything outside
      `[A-Za-z0-9._-]` to `_`, forbids a leading dot, and collapses path
      separators to a single inert basename, applied in all `save_local_*`
      builders of both tools.
    - **harden(ytls/mbedtls): opted-in insecure client is never silent —
      observability parity with openssl.** An accepted
      `ssl_allow_insecure_client=true` client now logs the same *"TLS client
      WITHOUT server-certificate validation (MITM surface)"* warning as the
      openssl backend, and runs the handshake under `VERIFY_OPTIONAL` instead of
      `NONE` so mbedTLS still computes the verify result and the tolerated
      failure is surfaced at handshake end (openssl records it natively even
      under `VERIFY_NONE`; mbedTLS skips verification entirely under `NONE`).
      The accept decision is unchanged (`OPTIONAL` never aborts; `CA_CHAIN_REQUIRED`
      fires only under `REQUIRED`, and the missing-hostname hard error is
      `REQUIRED`-only too). The *"did NOT verify"* warning guard now keys off
      the effective authmode instead of `has_ca_cert` (under `NONE`,
      `verify_result` holds `BADCERT_SKIP_VERIFY` and must not false-fire).
      Fixes the 11 TLS ctest failures under an mbedTLS-only `.config` — the
      test expectations had encoded openssl-only emissions. Verified 112/112
      with each backend.
    - **security(gobj-c): reject `gbuffer_create()` `data_size == SIZE_MAX`.**
      `GBMEM_MALLOC(data_size+1)` wrapped to `malloc(0)` — a non-NULL ~0-byte
      buffer that slips past the `__max_block__` guard while `gbuf->data_size`
      stays `SIZE_MAX`, defeating every later bounds check (`gbuffer_freebytes`
      / append `memmove`). Now rejected at creation. Regression test
      `test_gbuffer_guards.c::test_wrap_guard`.
    - **security(gobj-c): fix OOB heap over-read in `kwid.c` `collapse()`.** The
      in-tree `gbmem_strndup(str, size)` is a raw `memmove(s, str, size)` (not a
      real `strndup`), so building the path with
      `gbmem_strndup(path, strlen(path)+strlen(key)+2)` read past `path` by
      `strlen(key)+1` bytes and left the buffer unterminated before the
      `strcat`s. Replaced with `GBMEM_MALLOC` + `strcpy` at the exact size.
    - **security(libjwt): pin the exact JWT algorithm, not just the key family.**
      On the common pinned path (`config->alg == config->key->alg`, both set) the
      alg-vs-alg chain in `__verify_config_post` never compared the token alg, and
      the `kty` backstop is only family-granular (`jwt_alg_required_kty` maps every
      RS/PS alg to RSA) — so e.g. an **RS512 token verified against an RS256-pinned
      key**. Added an exact-alg check against whichever alg is pinned. Purely
      additive: no legitimate token is newly rejected. Complements the
      GHSA-q843-6q5f-w55g cross-family fix already in 7.x.
    - **security(yev_loop): fix use-after-free when a callback destroys its own
      event.** A callback calling `yev_destroy_event()` on its own event freed it
      synchronously, then `callback_cqe`'s re-arm block and dispatch tail
      dereferenced freed memory (last in-flight CQE). New `in_dispatch` flag
      defers the free to the dispatch tail; the re-arm blocks now also test
      `!destroy_requested` so a dying event is never re-armed.
    - **security(root-linux): guard NULL header value in `ghttp_parser.c`
      `on_header_value()`.** A previous chunk's `json_string()` failing on invalid
      UTF-8 leaves no value under `cur_key`; the next continuation chunk then hit
      `strlen(NULL)` (attacker bytes + TCP segmentation). Guarded before `strlen`,
      restart the accumulator from the current chunk, and log the store failure
      instead of silently truncating.
    - **security(root-linux): fix re-entrant-free UAF and TLS-error teardown in
      `c_tcp.c` `set_secure_connected()`.** The post-handshake `ytls_flush()` can
      re-enter and destroy the connection (an `EV_RX_DATA` subscriber) or report a
      TLS error. The return was ignored, so `start_pending_writes()` then ran on a
      freed gobj. Now: `-2222` (re-entrant free) bails without touching gobj/priv;
      `-1111` (TLS error, gobj alive) calls `try_to_stop_yevents()` and returns —
      matching the decrypt-path discipline. Requires the ytls change below.
    - **security(ytls): propagate `flush_clear_data()` errors out of openssl
      `flush()`.** It swallowed the negative return (including the `-2222`
      re-entrant-free sentinel); now returns it so `c_tcp` can act on it.
    - **security(emailsender): reject CR/LF/control chars in `attachment` and
      `inline_file_id`.** Both reach MIME headers raw via `append_attachment_part()`
      (`Content-Type name=` / `Content-Disposition filename=` / `Content-ID`), and
      `EV_SEND_EMAIL` is public — so they were an SMTP/MIME header-injection vector.
      Added to the single-line control-char rejection set alongside the envelope
      and display fields.
    - **security(gobj-c): pre-auth NULL deref in `gbuffer_deserialize()`.** A
      malformed base64 `data` field makes `gbuffer_base64_to_binary()` return
      NULL, fed straight into the unguarded `gbuffer_setmark()` inline — a daemon
      crash reachable pre-auth via the ievent server (`ac_on_message` →
      `kw_deserialize`). Added the NULL check after decode plus a central guard
      on the `gbuffer_setmark`/`getmark` inlines. Same change: `kwid_find_record_in_list()`
      returned `0` (a valid index) on not-found instead of `-1` (silent
      wrong-record match in the list comparator), and the flatten/unflatten
      helpers in `kwid.c` moved off raw libc `malloc`/`strdup`/`free` onto the
      mandated `gbmem_*`. Regression tests in `test_kw1.c`.
    - **security(libjwt): make the JWT verify contract fail closed.**
      `jwt_checker_verify2()` handed back the parsed claims regardless of outcome
      — the verdict lived only in `jwt_checker_error()`, so a caller trusting the
      non-NULL return would accept a forged / expired / alg-confused / unsigned
      token. It now returns NULL on any verification failure (the return value
      carries the verdict) and `jwt_verify_complete()` aborts early on
      `__verify_config_post` failure. Also fixed a wrong-free on the
      `jwk_process_one()` OOM path (freed the borrowed `jwk`, not the owned
      `item`). Regression: `test_jwt_alg_confusion.c::test_verify2_fail_closed`.
    - **security(timeranger2): validate pkey/id against path traversal.** A
      string primary key or treedb/msg2db node id becomes a `keys/<key>/`
      directory component, so an attacker-influenced value containing `/` or
      beginning with `.` could escape the topic's `keys/` dir on append
      (mkrdir/newfile), `tranger2_delete_key` (rmrdir), the disk mirror, and
      `tranger2_delete_instance`. Rejected at every sink in `tranger2_append_record`
      / `tranger2_delete_key` / `tranger2_delete_instance` / `treedb_create_node`
      / `msg2db_append_message`. Regression:
      `tests/c/timeranger2/test_pkey_path_traversal.c`.
    - **security(yev_loop): connect() the static DNS resolver socket.** The
      `CONFIG_FULLY_STATIC` resolver read UDP replies from any source, so
      authenticity rested only on the 16-bit transaction id — an off-path
      attacker could forge an A/AAAA answer and redirect a yuno's outbound
      connection. `dns_query()` now `connect()`s the UDP socket to the chosen
      nameserver (both IPv4/IPv6 branches) so the kernel drops datagrams from any
      other source. Regression:
      `tests/c/yev_loop/static_resolv/test_static_resolv_spoof.c`.
    - **security(ytls): verify-by-default for TLS clients (BREAKING).** A TLS
      *client* that would run `VERIFY_NONE` (no CA / effective authmode NONE) is
      now refused at ctx/state build time in both backends instead of merely
      logging a warning — closing a live MITM hole (the auth_bff → Keycloak
      outbound client ran unverified). Opt back in per gate with the new
      `ssl_allow_insecure_client=true` (default false) for self-signed / PSK /
      IoT bring-up. The mbedTLS gate keys off the effective authmode for openssl
      parity. The `C_AUTH_BFF` `crypto` and `c_authz` `kc_crypto` defaults
      flipped to a verifying posture (`ssl_use_system_ca` + `ssl_verify_mode=required`).
      **Rollout:** any TLS-client deployment relying on silent `VERIFY_NONE` must
      add a CA (or `ssl_allow_insecure_client=true`) before it will connect.
    - **fix(root-linux): check `ytls_init()` NULL in `c_tcp.c` connect path.**
      Follow-up to verify-by-default: the refusal is a soft failure (`ytls_init`
      returns NULL, yuno stays up), but `ac_connect` never checked it and the
      established connection then called `ytls_new_secure_filter(NULL, ...)` —
      SEGFAULT (caught by `perf_c_tcps` test4/test5). Now the connect aborts
      cleanly via `try_to_stop_yevents()`. Also added the missed
      `ssl_allow_insecure_client=true` to the `perf_c_tcps` test4/test5 client
      crypto blocks (the `tests/c/c_tcps*` sweep skipped `performance/`).
    - **harden(yuno_agent/yuno_agent22): pin the controlcenter client to the
      canonical agent certificate.** The outbound controlcenter client
      (`tcps://<arch>.<owner>.<output_url>`, active when `node_owner != "none"`)
      ran with no CA and would be refused under verify-by-default. It now pins
      `/yuneta/agent/certs/yuneta_agent.crt` — the self-signed canonical cert
      the controlcenter actually serves (fingerprint-verified), already present
      on every node (the agent's own wss server uses the same file) — with
      `ssl_server_name=yuneta_agent.yuneta.io` (the cert has no SAN, hostname
      check falls back to CN). Supporting change in `c_tcp.c`: a config-supplied
      `ssl_server_name` now wins over the url-derived host, enabling pinning
      where the pinned cert's name differs from the dialed host. **Caveats:**
      the pin authenticates "a yuneta install" (the key ships on every node),
      not the controlcenter specifically — still a real upgrade over
      no-verification; and on a node missing the cert file the first
      controlcenter dial exits the agent (`ssl_trusted_certificate` load is
      fatal on first init) — verify the file exists before deploying with a
      non-none owner.

## 7.5.12
    - **fix(packages): default agent `node_owner` to `"none"` — no controlcenter
      on a fresh node.** The bundled `yuneta_agent.json.sample` /
      `yuneta_agent22.json.sample` shipped `"node_owner": "owner"`, a placeholder.
      The agent starts the controlcenter client whenever `node_owner != "none"`
      (`c_agent.c` `mt_start`), so a freshly installed standalone node kept
      dialing `tcps://<arch>.owner.yunetacontrol.com:1994` and logging
      `getaddrinfo() FAILED` every ~17 s. The default is now `"none"`, the
      design's built-in off-switch, so a fresh box is quiet. Operators **with** a
      controlcenter still opt in with `YUNETA_OWNER=mycompany` at install time
      (the postinst sed now swaps `"none"` → their owner). Note: blanking the
      config to `{}` does **not** silence it — the framework default for
      `node_owner` is `""`, which is also `!= "none"`; `"none"` must be explicit.
      Affects fresh installs only (the `.json` files are conffiles, never
      overwritten on upgrade).
    - **refactor(packages): minimal, sanitized agent config templates.** The two
      `*.json.sample` templates are trimmed to the smallest valid standalone
      baseline (the operator parameterizes per project afterwards): replaced the
      deprecated `authz.authz_yuno_role` key with `authz.authz_service` (the
      former is `SDF_DEPRECATED` in `c_authz.c`), and dropped the confusing
      `__realm_id__` override (`"/yuneta_agent.trdb"`) — the agent's treedb store
      dir is now inherited from the compiled-in `main.c` default
      (`/yuneta/store/agent/yuneta_agent.trdb`), exactly as a real parameterized
      node does. No `authz.jwks` / `authz.initial_load` in the template: the
      `root` role + `yuneta` user come from `main.c` via the config merge, so a
      fresh agent still bootstraps local `ycommand` on port 1991.

## 7.5.11
    - **security(ext-libs): bump vendored OpenSSL 3.6.2 → 3.6.3.** Security patch
      release (`configure-libs.sh` v1.16, `TAG_OPENSSL=openssl-3.6.3`). Fixes one
      **High** CVE — CVE-2026-45447, heap use-after-free in `PKCS7_verify()` —
      plus a batch of CMS / QUIC / ASN.1 / AES CVEs (CVE-2026-34180..34183,
      35188, 42764..42770, 45445/45446, 7383, 9076). No API change (3.6 series).
      OpenSSL is linked **statically into every yuno**, so every yuno must be
      rebuilt + relinked to pick it up; the release CI builds ext-libs fresh, so
      the published `.deb`/`.rpm` get it automatically. Stayed on 3.6 (not 4.0),
      same LTS rationale as before.
    - **feat(install): no prompt — `install.sh` runs straight through.** The
      installer no longer asks `Install the developer toolchain? [Y/n]` mid-run;
      it installs everything in one pass without stops. Use `--runtime-only` to
      skip the toolchain on a pure deployment box. (Served from `main`, so it
      ships on push.)

## 7.5.10
    - **fix(deb): drop obsolete `libpcre3-dev` from the dev-deps helper.** PCRE1
      (`libpcre3-dev`) was removed from current Ubuntu (26.04) — it is "referred
      to but has no installation candidate", so the helper printed
      `[!] Failed: libpcre3-dev`. Yuneta does not need it (it builds its own
      static PCRE2, and the bundled nginx is static), so it is removed;
      `libpcre2-dev` stays. The `apt-cache show` guard didn't catch it (the
      transitional record still resolves), so the helper now just installs and
      reports the real failures.
    - **fix(deb): honest dev-deps end summary.** The Debian helper ended with a
      vague `Dev environment setup attempt complete` and only printed failures
      inline; it now collects them and reports `all N packages installed` or the
      exact list that did NOT install, matching the `.rpm` helper.

## 7.5.9
    - **fix(deb): never auto-reboot in `postinst`.** The Debian `postinst` forced
      a reboot at the end of install (auto-yes when non-interactive), which under
      `curl | sh` rebooted the box mid-flight — killing the SSH session before
      `install.sh` could install the developer toolchain. The kernel tuning is
      already applied live (`sysctl --system`), so a reboot is not required: it
      now only leaves the `reboot-required` hint and recommends a reboot, never
      forcing one. Matches the `.rpm` `%post` policy.
    - **feat(install): `install.sh` installs certbot on both distros.** After the
      package, the installer now runs the bundled certbot helper (snap on Debian,
      EPEL `dnf` on RHEL) so TLS for the bundled web server is set up in the same
      run, regardless of `--runtime-only` (it is a runtime/ops tool).

## 7.5.8
    - **fix(rpm): dev-deps helper used a dnf5-only flag that RHEL 9 rejects.**
      The 7.5.7 helper ran `dnf install --skip-unavailable`, but
      `--skip-unavailable` only exists in dnf5 (Fedora); RHEL 9 / Rocky 9 ship
      dnf4, which errors `unrecognized arguments: --skip-unavailable` and aborts
      the whole transaction — so the toolchain (git, clang, gcc, wget, …)
      installed nothing again. Now uses `--setopt=strict=0`, the dnf4-native way
      to skip unavailable packages (and valid on dnf5 too).
    - **fix(rpm): create the `nogroup` group so the bundled nginx starts.** nginx
      falls back to its compiled-default group `nogroup`, which exists on Debian
      but not on RHEL; without it nginx aborted at startup
      (`getgrnam("nogroup") failed`). `%post` now creates it (RHEL-only, before
      the service starts).
    - **fix(rpm): honest web-server start in the init script.** `start_web()` ran
      `nginx || true; log_end_msg 0`, printing `OK` even when nginx failed to
      start. It now captures the real exit code and reports it, like the agent
      start. (Matches the no-silent-failure rule.)
    - **feat(install): `install.sh` is now a single cross-distro installer that
      sets up everything in one run.** It detects the distro (`apt` vs `dnf`),
      and on RHEL/Rocky/Alma enables **EPEL + CRB** first; pulls the matching
      package (`.deb` / `.rpm`) from the latest Release and installs it; then
      installs the **full developer toolchain** (git, mercurial, clang, gcc,
      cmake, ninja, wget, pipx, …) by delegating to the bundled, resilient
      `/yuneta/bin/install-yuneta-dev-deps.sh` — so a fresh box is build-ready
      from one command, with no second script to remember. Asks first when a
      terminal is attached (reads `/dev/tty`, so it works under `curl | sh`);
      installs by default when non-interactive. `--runtime-only` skips the
      toolchain for pure deployment boxes. Served from `main`, so it reaches
      users on push (it installs the latest published Release packages). Was
      Debian-only and runtime-only before.

## 7.5.7
    - **fix(rpm): dev-deps helper no longer installs nothing when one package is
      unavailable.** `install-yuneta-dev-deps.sh` ran `dnf -y install "${PKGS[@]}"`
      as a single atomic transaction, so one unfindable package (typically a
      CRB-only `-devel`/`-static` when CRB was never enabled) aborted the WHOLE
      set — leaving `clang`, `wget`, `gcc`, `cmake` … none installed — and the
      `|| echo continuing` + final `[✓] complete` hid it. It now uses
      `dnf --skip-unavailable` (installs every available package, skips only the
      missing) and reports per-package with `rpm -q` which ones did NOT install,
      pointing at EPEL/CRB when a `-devel`/`-static` is absent, instead of a
      green lie. Matches the `.deb` helper's per-package resilience. (`wget` and
      `clang` are dev-deps installed by this helper, not by the base `.rpm`.)

## 7.5.6
    - **fix(rpm): honest agent start in `%post`; don't source the `set -u`-unsafe
      RHEL init functions.** Two RHEL-only packaging bugs in the 7.5.5 `.rpm`
      (the `.deb` was never affected). (1) The generated `/etc/init.d/yuneta_agent`
      runs under `set -u`; on RHEL `/lib/lsb/init-functions` is absent so it fell
      to sourcing `/etc/init.d/functions`, which references unset vars
      (`SYSTEMCTL_SKIP_REDIRECT`…) and aborted the whole init script with
      "unbound variable" **before the agent was ever launched** — the binary was
      fine, the service "failed". It now defines the only two functions it uses
      (`log_daemon_msg`/`log_end_msg`) itself and only sources Debian's
      `set -u`-clean LSB file when present. (2) `%post` started the agent with
      `service start || true`, hiding a failed start behind RPM's always-"Complete"
      transaction. It now re-reads the effective `kernel.io_uring_disabled` after
      `sysctl --system`, only starts when io_uring is usable, captures the real
      result, and prints a loud "AGENT IS NOT RUNNING" warning with diagnosis
      hints (`systemctl status` / `journalctl` / `getenforce` for SELinux) instead
      of a green install over a dead agent.

## 7.5.5
    - **refactor(packaging): split Debian packaging into `packages/deb/`.**
      With the new `packages/rpm/`, the Debian scripts moved from the root of
      `packages/` into a sibling `packages/deb/` (`AMD64`/`ARM32`/`ARMhf`/
      `RISCV64` wrappers + `make-yuneta-agent-deb.sh` + its `README.md`), so the
      two packagers are now symmetric: `packages/deb/` and `packages/rpm/`. The
      shared agent config samples stay at `packages/templates/` (referenced by
      both via an absolute `$YUNETAS_BASE/packages/templates/` path);
      `packages/README.md` is now a short index. The deb arch wrappers read
      `../../YUNETA_VERSION` + `../../RELEASE` (one level deeper); the release CI
      and the `.gitignore` `authorized_keys`/`webserver` rules follow the move.
      No change to the produced `.deb` or its contents.
    - **feat(build): RHEL/Rocky/Alma build support (was Debian-only).**
      Yuneta now builds and runs on the RHEL family; verified end-to-end on
      Rocky Linux 9.7 (full static build + 110/110 ctest). New
      `install-dependencies.sh` auto-detects the distro from `/etc/os-release`
      and installs the right packages with `apt` (Debian) or `dnf` (RHEL,
      enabling EPEL + CRB). RHEL-specific build fixes that ride along:
      `configure-libs.sh` v1.15 forces `-DCMAKE_INSTALL_LIBDIR=lib` on the
      CMake libs (mbedtls/pcre2/jansson/argp), which on RHEL default to
      `lib64` and so were missed by the kernel's `outputs_ext/lib` link path
      (no-op on Debian); `set_compiler.sh` gained a `dnf reinstall` branch;
      and the postgres module includes `<libpq-fe.h>` (not
      `<postgresql/libpq-fe.h>`) with the dir resolved via `pg_config` in
      CMake, since the libpq header sits in `/usr/include` on RHEL vs
      `/usr/include/postgresql` on Debian. RHEL also needs `glibc-static`/
      `libstdc++-static`/`libxcrypt-static` (CRB) for the default static link.
    - **feat(runtime): document the io_uring requirement on RHEL.** Yuneta's
      `yev_loop` is io_uring-based, and RHEL 9 / Rocky 9 / Alma 9 ship
      `kernel.io_uring_disabled=2` (fully disabled), so every yuno aborts at
      startup until it is re-enabled (`kernel.io_uring_disabled=0`). Called
      out in `installation.md`; `install-dependencies.sh` warns when it
      detects the disabled state. No code change — a deployment prerequisite.
    - **feat(packaging): RPM packaging for the Yuneta Agent (`packages/rpm/`).**
      Counterpart of the Debian `packages/`: stages the same `/yuneta` payload
      and builds an `.rpm` with `rpmbuild` (`make-yuneta-agent-rpm.sh` +
      `x86_64`/`aarch64`/`riscv64` wrappers). RHEL-specific envelope: `.spec`
      instead of `control`, `%post`/`%preun`/`%postun` instead of
      maintainer scripts, `useradd`/`wheel`/`chkconfig`/langpacks/EPEL-certbot,
      and the shipped `kernel.io_uring_disabled=0`. Built + inspected on Rocky
      9.7 (`rpm -qlp`/`--scripts`/`rpmlint`); not installed.
    - **ci(release): `release-deb.yml` -> `release-packages.yml`, now also
      publishes the x86_64 `.rpm`.** The release job builds the `.rpm` next to
      the AMD64 `.deb` and uploads both as release assets. It runs on the same
      Ubuntu runner: the default build is fully static, so the binaries also
      run on RHEL/Rocky — no EL9 container needed.
    - **fix(tests): make `ytls/test_cert_info` portable to OpenSSL >= 3.5.**
      The expired-cert helper used `openssl x509 -req -days -1`, which
      OpenSSL >= 3.5 rejects ("end date before start date"). It now falls back
      to `-not_before/-not_after` with fixed past dates (OpenSSL >= 3.2) when
      the negative-span form fails, keeping older OpenSSL (e.g. Debian 12's
      3.0.x) working. Not RHEL-specific — any host with a modern OpenSSL.
    - **fix(emu_device): don't truncate the final frame on standalone exit.**
      `finish_replay()` called `exit(0)` right after queueing the last frames,
      before io_uring completed the write — the final frame could be lost. In
      standalone CLI mode it now waits for the C_TCP `EV_TX_READY` drain signal
      (tx queue empty + in-flight write completed) and exits from `ac_tx_ready`;
      empty replays still exit immediately. Verified end-to-end: a 3-frame replay
      delivered all bytes including the last frame, clean exit. Agent-managed
      mode is unchanged (it never exited).
    - **chore(auth_bff): remove the deprecated `idp_url` + `realm` pair.** The
      legacy Keycloak path-scheme fallback (build
      `<idp_url>/realms/<realm>/protocol/openid-connect/{token,logout}`) was
      `SDF_DEPRECATED` since the 2026-04-30 OIDC migration and a release has
      shipped with the warning. Both `SDATA` attrs and the resolution branch in
      `c_auth_bff` `mt_create` are gone; configure `issuer` (discovery) or the
      explicit `token_endpoint` + `end_session_endpoint` instead. No yunetas or
      private deployment still set the legacy pair (all on `issuer`). Docs
      updated (`YUNO_AUTH.md` §2.5, `guide_oauth2_pkce_bff.md`). The now-subjectless
      `tests/c/c_auth_bff/test17_legacy_idp_url` is removed; the suite is 18/18.
    - **docs(auth): ROPC in `c_task_authenticate` deferred by design.** The CLI
      grant stays `grant_type=password` (works on Keycloak, the only deployed
      IdP). Documented the constraint and the real migration path (device-flow
      for interactive + client-credentials for headless CI, not loopback PKCE —
      the six CLI callers have no browser) in the `c_task_authenticate.c` header,
      `YUNO_AUTH.md` §3.4, and `TODO.md`. No behavior change.

## v7.5.4 -- 08/Jun/2026
    - **feat(mqtt/security): subscribe-side ACL enforcement.** Completes the
      publish/subscribe ACL started in 7.5.3. The per-topic SUBACK reason is
      built in the broker's `ac_mqtt_subscribe`, so the check lives there
      (alongside the existing `deny_subscribes` gate), calling
      `mqtt_acl_check(…, "read")` per requested filter — a denied filter is not
      added and gets a `MQTT_RC_NOT_AUTHORIZED` (v5) / `0x80` (v3.x) SUBACK
      reason, logged. Unchanged when `enable_acl` is off or a group has no
      `subscribe_acl` patterns.
    - **fix(security/authz): per-command authz gate redesign.** A local agent
      pilot showed the 7.5.3 gate (`enable_command_authz`) was undeployable — it
      denied a yuno's own internal startup commands (e.g. `open-treedb`) and the
      yuno exited. Two bugs fixed in gobj-c: (A) a specific-authz lookup on a
      concrete gobj now falls back to the **global** authz table
      (`authzs_list`), so `__execute_command__` resolves on any gobj (was
      "authz not found" → deny-all, root included); (B) the gate fires **only
      for external commands** (those whose kw carries the authenticated
      `__username__` injected by `c_ievent_srv`), so internal `gobj_command()`
      calls are never gated (`command_parser`). Re-piloted: the agent now boots
      clean with the gate on and `ycommand` (root) works. `enable_command_authz`
      remains **default-off**.
    - **feat(security/authz): seed root role model** in the C_AUTHZ yunos that
      lacked one (`controlcenter`, `mqtt_broker`, `emailsender`) via
      `Authz.initial_load` (role `root` + user `yuneta`, mirroring the agent),
      a prerequisite for enabling the command-authz gate there.

## v7.5.3 -- 08/Jun/2026
    - **feat(security): per-command authorization re-armed (gated opt-in).** The
      `SDF_AUTHZ_X` check at the command-dispatch boundary in `command_parser.c`
      (commented out for years) now runs again, but only when the yuno sets the
      new `enable_command_authz` attr (`c_yuno`, `SDF_RD`, default `"0"`), so the
      default posture is unchanged and non-breaking. Self-issued commands
      (`src == gobj`) bypass the check; a denial returns `-403` and is logged
      (`MSGSET_AUTH`). Turning the gate on requires a running `C_AUTHZ` role
      model (the global checker is fail-closed). Test:
      `tests/c/command_authz/`.
    - **feat(mqtt): publish-side ACL (model A, group-based, default off).** New
      `enable_acl` attr on `C_MQTT_BROKER` plus `publish_acl`/`subscribe_acl`
      array columns on the `client_groups` topic (schema_version 25→26,
      topic_version 3→4; additive). `C_PROT_MQTT2` queries the broker over a
      direct `EV_MQTT_ACL_CHECK` event on PUBLISH; allow when ACL off or no
      patterns authored, deny unknown clients, deny logged. Subscribe-side
      wiring is staged (schema + helper ready) but not yet enforced. Test:
      `tests/c/c_mqtt/acl`. Also fixed a latent fkey bug: the helper now passes
      `{fkey_only_id:1}` so `client_groups` resolves as plain ids (otherwise the
      ACL silently allowed all).
    - **fix(emu_device): frame emission path.** `window`/`interval` were coerced
      to 0 (CLI values passed as `json_string` into `DTP_INTEGER` attrs;
      `cmd_write_*` used `kw_get_str`+`atoi`), so the replay sent nothing. Now
      `json_integer(atoi(...))` on the CLI side and `kw_get_int(KW_WILD_NUMBER)`
      in the commands; also freed the replay resources on `mt_play` error paths
      and log a skipped record with no `frame64`.
    - **refactor(emu_device): moved from `yunos/c/` to `utils/c/`.** It is a
      standalone CLI utility yuno (a device-gate emulator run by hand for
      testing), not a deployable service — now installed to `/yuneta/bin` like
      the other `utils/c` tools.
    - **docs:** `YUNO_AUTH.md` rewritten to describe the gated authz (was
      documented as "commented out", including the auth-flow diagram);
      `mqtt_broker.md` gains an Authorization (ACL) section.

## v7.5.2 -- 06/Jun/2026
    - **security: hardening batch (memory-safety + injection across the stack).**
      - **gobj-c:** NULL-guard in `gbuffer_deserialize`; bounded recursion in
        `kw_find_path` and the kwid comparators (hostile-JSON stack exhaustion).
      - **root-linux:** NUL-terminate accumulated URL / header field / header
        value in `ghttp_parser` (over-read fed to `json_string`); guard
        `/auth/logout` against an uninitialized refresh-token read.
      - **ytls:** re-entrant use-after-free in `encrypt_data` WANT path + a
        double `gbuffer_get` (stream corruption).
      - **yev_loop:** bound DNS response parsing + unpredictable transaction id;
        defer event free until in-flight io_uring CQEs drain (UAF).
      - **timeranger2:** validate on-disk md2 `__offset__`/`__size__` before
        read AND before the `delete_instance` payload wipe (cross-record
        overwrite); guard `read_md` return; bound the inotify parse loop.
      - **libjwt:** backport GHSA-q843-6q5f-w55g algorithm-confusion JWT forgery
        fix + the full cfd8902 hardening; add an in-tree regression test
        (`tests/c/libjwt/`, RSA/EC/EdDSA/`alg:none`).
      - **modbus:** reject MBAP length < 3 (heap overflow). **dba_postgres:**
        escape SQL identifiers/literals in insert + create-table (SQLi).
      - **emailsender:** reject CR/LF in header/envelope fields (SMTP/MIME
        injection). **mqtt:** reject property-length underflow in C_PROT_MQTT2.

## v7.5.1 -- 03/Jun/2026
    - **fix(treedb): multi-version parent reverse-hook hygiene.** Two
      in-memory hook quirks around versioned (pkey2) parents are fixed at the
      treedb layer:
        - **Unlink targeted only the primary parent version.** A child's fkey
          ref carries just `parent_topic^parent_id^hook` (no version), so
          `treedb_clean_node` unlinked from the PRIMARY instance, leaving a
          stale entry on the non-primary version the child was actually hooked
          on. It now locates the parent-version instance that really holds the
          child (`find_parent_version_holding_child` over the pkey2 index) and
          unlinks that one; the primary-instance behaviour is the fallback for
          hook+fkey combos the read-only probe can't match.
        - **Duplicate hook entries.** Repeated create/link of the same child id
          left it more than once in the parent hook (`yunos:["5000","5000"]`),
          inflating "Using in N". `_link_nodes` and the loader
          `link_child_to_parent` now dedup by child id before appending
          (idempotent link), and the child-side fkey array is deduped too.
      A skipped duplicate is **warned** (not silently swallowed): a re-link, or
      a duplicate fkey self-healed on load, is surfaced via `gobj_log_warning`.
      Closes the TODO follow-up; both quirks also self-heal on reload.

## v7.5.0 -- 03/Jun/2026
    - **fix(agent): version-aware, stale-safe `delete-config`/`delete-binary`
      usage guard.** The "Using in N yunos" guard read the raw `yunos` hook
      count, which is config-id-level (shared across versions) and can carry
      stale/duplicate refs — so an UNUSED config/binary version could not be
      pruned while another version was in use, and lingering refs blocked
      deletes. New `count_yunos_using()` validates every hooked yuno id via
      `gobj_get_node` (a deleted yuno → NULL → skipped) and, when a version is
      given, counts only yunos pinned to THAT version (config↔`name_version`,
      binary↔`role_version`). So an unused/superseded version prunes cleanly,
      only the in-use version blocks, and `force=1` overrides. Combined with the
      durable per-instance delete below, `delete-config version=`/`delete-binary
      version=` now remove a single version durably. (Underlying treedb
      multi-version reverse-hook hygiene remains a minor follow-up — see TODO.md.)
    - **feat(treedb): durable per-instance (pkey2) node delete.**
      `treedb_delete_instance` now tombstones EVERY md2 row belonging to a
      `(key, pkey2_value)` via `tranger2_delete_instance()` (enumerated with a
      transient disk list) and drops the in-memory pkey2 slot — previously it
      only dropped the in-memory slot, so the instance resurrected on the next
      reopen (a treedb instance spans several rows: create + each link/update
      re-appends one with the same id/pkey2; tombstoning just the latest let an
      earlier row reload). The primary index is untouched (callers route only
      non-primary instances; the loader re-elects the highest surviving rowid).
      Whole-key delete (`treedb_delete_node`/`tranger2_delete_key`) is unchanged.
      Exposed through the agent: `delete-yuno`/`delete-config`/`delete-binary`
      now accept the pkey2 (`yuno_release`/`version`) to prune a single
      non-primary release/version, listing via `gobj_list_instances` and
      reading the running-guard from the primary (instance records carry a
      stale `yuno_running`). New regression test covers a multi-row instance +
      close/reopen. (Remaining follow-up: stale reverse-hooks on linked parents
      — see TODO.md.)
    - **fix(agent): `find-new-yunos` inherits node placement across a version
      bump.** A version-bump deploy (`install-binary` + `find-new-yunos
      create=1` + `deactivate-snap`) created the fresh `yunos` row at schema
      defaults, dropping the operator-set `start_priority`/`sched_priority`/
      `cpu_core` and forcing a re-run of `tools/agent/set_start_priorities.py`.
      `cmd_find_new_yunos` now copies those three fields from the prior primary
      row into the emitted `create-yuno` command (and `pm_create_yuno` accepts
      them), so launch tiers and CPU placement survive the bump. Same-version
      REBUILD hot-patches were already unaffected (they keep the existing row);
      genuinely-new yunos still get defaults plus the `util`-tag seed.

## v7.4.8 -- 03/Jun/2026
    - **feat(agent): per-yuno `start_priority` launch tiers.** The agent's
      `yunos` topic gains `start_priority` (band 0..9, default 5). `run-yuno`
      launches ascending (utilities first), `kill-yuno`/`pause-yuno` descending
      (utilities last, so logcenter captures everyone's shutdown), stable within
      a tier. The node-wide relaunch (`run_enabled_yunos`, used by
      `restart_nodes`/`deactivate-snap` and at startup) honours the same order;
      the force-SIGKILL pass stays unordered (no graceful drain to sequence).
      `create-yuno` seeds `start_priority=1` for `util`-tagged yunos (the set
      `run_util_yunos` already starts first) — no app role names in the agent.
      Schema `topic_version` 19→20 + `schema_version` 22→23; the bump only
      refreshes the col schema files, record data is untouched (no store wipe).
      Assign app tiers per node with `tools/agent/set_start_priorities.py`.
    - **feat(agent): node CPU placement (`sched_priority`, `cpu_core`) from the
      agent treedb.** Both are new `yunos` columns the agent injects into the
      launched yuno's config as its `sched_priority`/`cpu_core` attrs, so OS
      scheduling/affinity is a node-local decision instead of being baked into
      the config that travels across nodes. Defaults only: the user config file
      is merged after the agent's and still wins. `cpu_core=0` (default) = no
      boost, unchanged behaviour.
    - **refactor(c_yuno): scheduling attr `priority` renamed to `sched_priority`.**
      The `sched_setscheduler` attr (default 20, applied only when `cpu_core>0`)
      collided with the per-service start order (0..9) and the agent's
      `start_priority`; renamed so the name states what it does. `SDF_PERSIST`
      fallback is a no-op in practice (consulted only at `cpu_core>0`, which no
      shipped yuno sets); no migration shim. The per-service `priority` is
      unchanged. See `TODO.md`.
    - **refactor(agent): `set-ordered-kill` renamed to `set-graceful-kill`.** The
      command never ordered anything — it only sets `signal2kill=SIGQUIT` (the
      yuno catches it and shuts itself down cleanly). Renamed to the honest axis
      (graceful SIGQUIT vs quick SIGKILL, `set-quick-kill`), which also frees
      "ordered" for the real `start_priority` ordering above. No alias kept.
    - **feat(tools): `set_start_priorities.py` — assign `start_priority` by role.**
      One-shot operator tool mapping each managed yuno's role to a launch tier
      (defaults: utilities=1, `gate_*`=4, `db_*`=7; unmatched left as-is) and
      writing the differences via `update-node` (record base64'd into
      `content64`; the inline `record={...}` form is not coerced by the CLI).
      `--rule PATTERN=PRIO` adds/overrides (matched before the built-ins),
      `--dry-run`/`--all`/`--show-all`. Same OAuth2-once + `-j` plumbing as the
      other agent scripts, so it can drive a remote wss:// agent.
    - **feat(tools): `sync_binaries.py`/`sync_configs.py` order restarts by
      `start_priority`.** Both bounced yunos alphabetically; now they read the
      per-yuno `start_priority` the agent exposes via `*list-yunos` and restart
      ascending, so infrastructure comes back before its dependents. Both
      degrade to the previous order when the agent has no `start_priority` yet.
      The quit/decline message also reads `Cancelled - no changes made.` instead
      of `Aborted.` (which looked like a crash).
    - **feat(c_yuno): `print-role` command — runtime equivalent of `--print-role`.**
      Every yuno (the agent included) now answers a `print-role` command that
      returns its basic identity: `role`, `name`, `alias`, **`version`** (the
      yuno's own APP_VERSION) and **`yuneta_version`** (the framework version),
      plus description/tags/required_services/public_services/service_descriptor.
      Until now that info was only printable offline via the binary's
      `--print-role` flag; there was no way to read a *running* yuno's version.
      Lives in C_YUNO's command table, so it is inherited by all yunos. Address
      the yuno gobj with the `-S __yuno__` flag: `ycommand -S __yuno__ -c
      'print-role'` (inline `service=...` is a command parameter, not routing).
    - **feat(tools): `sync_binaries.py` automates the same-version REBUILD
      hot-patch.** A `REBUILD` (`update-binary`) overwrites the slot the running
      yuno executes from, so it failed with `text-file-busy` and the script only
      printed a "kill-yuno first" reminder. Now, once both confirmation gates are
      cleared, it runs the documented per-role cycle itself, scoped by
      `yuno_role` (never node-wide): `kill-yuno` (only if running; orderly
      SIGQUIT, so the gbmem audit runs) → poll `*list-yunos` until the process
      exits → `update-binary` → `run-yuno play=0` (if it was running) →
      `play-yuno` (if it was playing). Prior run/play state is read from
      `*list-yunos` and restored per role, and a role with several instances
      across realms is handled in one shot. New `--no-restart` flag keeps the old
      print-only behaviour. The version-bump path (`find-new-yunos` +
      `deactivate-snap`, a node-wide bounce) stays a reminder.
    - **feat(tools): `sync_configs.py` gains an opt-in `--restart`.** Installing
      a config does NOT need a kill — unlike `update-binary` (which hits
      `text-file-busy` while the yuno runs), a config push always succeeds on a
      running yuno; it just does not take effect until that yuno next (re)starts.
      So by default the script still only pushes and prints the affected yuno ids
      (from the agent record's `yunos` field) as a `kill-yuno` + `run-yuno`
      reminder — restarting is a separate, optional step. Pass `--restart` to
      also bounce the using yunos right away, scoped by yuno `id` (never
      node-wide): `kill-yuno` (only if running; orderly SIGQUIT) → poll
      `*list-yunos` until it exits → `run-yuno play=0` → `play-yuno` (if it was
      playing), preserving prior run/play state. A stopped yuno is left stopped;
      NEW configs (no agent record) print a reminder.

## v7.4.7 -- 02/Jun/2026
    - **feat(c_authz): `create-user` password is now optional.** KC/IdP-
      authenticated users have no local password (`credentials` null) — auth is
      by JWT. The command no longer rejects an empty password; it only hashes
      credentials when one is given, otherwise creates the user password-less,
      the same way `register-idp-user` and the `initial_load` users do.
    - **fix(c_authz): stop resetting a user's "Created Time" on update.** In
      `ac_create_user` the `new_user` flag was inverted (`user?TRUE:FALSE` is
      TRUE when the node already exists), so updating an existing user wrote
      `time=now` into the record, overwriting its creation timestamp. New users
      were unaffected (treedb auto-stamps the `time`-flagged column on create).
      Corrected to `user?FALSE:TRUE`.
    - **fix(yuno_agent): silence spurious "Event NOT DEFINED in state" on every
      login.** C_AGENT subscribes to all of its `authz` service's output events
      but had no FSM entry for `EV_AUTHZ_USER_LOGIN`/`LOGOUT`/`NEW` (consumed by
      controlcenter from its own local authz), so each login/logout logged an
      error. Added accept-and-ignore handlers.
    - **fix(ycommand): accept `EV_ON_OPEN_ERROR` in `ST_DISCONNECTED`.** A failed
      connect / identity-card NAK publishes `EV_ON_OPEN_ERROR` after the close;
      the FSM lacked it and logged "Event NOT DEFINED in state". Now handled like
      `EV_ON_ID_NAK` (`ac_on_close`).
    - **feat(tools): sync_binaries.py / sync_configs.py — OAuth2 passthrough for
      remote agents.** Both scripts now log in ONCE (Keycloak password grant via
      stdlib, or a `--jwt` passed verbatim) and thread the token through `-j` to
      every `ycommand` call, so they can drive a remote `wss://` agent without
      SSH. New flags: `-I/--issuer` (OIDC discovery), `-T/--token-endpoint`,
      `-Z/--client-id`, `--client-secret`, `-x/--user-id`, `-X/--user-passw`,
      `-j/--jwt`. The no-arg local path is unchanged (no auth). `$$()` already
      resolves client-side, so the LOCAL build is what gets uploaded.
    - **fix(emailsender): handle `EV_ON_OPEN` in `ST_WAIT_RESPONSE`
      (reconnect-on-demand).** When the SMTP link had idle-closed, the head
      message is dispatched anyway and `c_smtp_session` reconnects to deliver
      it; on reaching `ST_IDLE` it publishes `EV_ON_OPEN` *before* beginning the
      stashed message — but `c_emailsender` is already in `ST_WAIT_RESPONSE` (it
      changes state before `EV_SEND_MESSAGE`), so every reconnect-to-deliver
      cycle logged a spurious *"Event NOT DEFINED in state"* even though the
      mail was delivered. `ST_WAIT_RESPONSE` now accepts `EV_ON_OPEN` via
      `ac_on_open_waiting`, which only marks the link ready and does NOT
      re-dequeue (a message is already in flight). Also corrected the misleading
      `ac_disconnected` comment in `c_smtp_session`.
    - **feat(c_authz): IdP (Keycloak) user provisioning.** New commands
      `register-idp-user` (create the user in Keycloak via the admin REST API +
      the local `treedb_authzs` user with the chosen role, then email a
      set-password invite), plus `set-kc-config` / `view-kc-config` to configure
      the admin connection. The connection params are neutral persistent attrs
      (`kc_*`, `SDF_PERSIST`) set at runtime — no endpoints/secrets in code or
      committed config; the secret is masked by `view-kc-config`. Lives in
      C_AUTHZ so every auth-enabled yuno inherits it. The outbound work is an
      async multi-job `C_TASK` over a lazily-created `C_PROT_HTTP_CL`.
    - **feat(c_prot_http_cl): accept any JSON value as the request body.**
      `data` is now read with `kw_get_dict_value` instead of `kw_get_dict`, so a
      JSON array body (e.g. Keycloak `execute-actions-email`) is sent verbatim
      via `json_dumps`; the x-www-form-urlencoded path is unchanged (it only
      iterates objects).
    - **fix(packages): cert-sync no longer reloads TLS on every tick.**
      `copy-certs.sh` re-copied the certs each run (mtime bump → spurious
      `reload-certs` broadcast every 15 min): GNU `install -C` never skips a
      symlink source (letsencrypt `live/*.pem`) and re-copies on root/yuneta
      owner mismatch. Now resolves the symlink with `readlink -f` and sets the
      owner via `install -o/-g`, dropping the trailing `chown`.

## v7.4.6 -- 01/Jun/2026
    - **feat(tools): `tools/agent/sync_binaries.py` — reconcile built yunos
      against the agent and push updates.** Drives from the agent's installed
      binaries (`ycommand -c '*list-binaries'`), looks each one up in
      `outputs/yunos` (`--print-role`), and classifies it
      BUMP/DOWNGRADE/REBUILD/UP-TO-DATE/NO-BUILD. After confirmation it runs
      `install-binary` / `update-binary id=<role> content64=$$(<role>)` for the
      chosen roles; it does not automate the node-wide lifecycle steps
      (`kill-yuno`, `find-new-yunos` + `deactivate-snap`) but prints them as
      reminders. Lives under `tools/` (shipped in the install `.deb`, usable on
      a bare node), and is documented at doc.yuneta.io under the new **Tools**
      section.

    - **feat(tools): `tools/agent/sync_configs.py` — reconcile a directory's
      configs against the agent and push updates.** Config-side sibling of
      `sync_binaries.py`. Because configs are not centralized like binaries
      (they live under each yuno's `batches/<host>/`), it drives from the
      current directory: each `*.json` config's id is its filename (minus
      `.json`) and its version is the `__version__` field inside the file
      (`_*.json` batch helpers and files without `__version__` are skipped). It
      looks each up via `ycommand -c '*list-configs'` and classifies it
      NEW/BUMP/UPDATE/UP-TO-DATE/DOWNGRADE/agent-only. After confirmation it runs
      `create-config` / `update-config id='<id>' content64=$$(<path>)`; a
      DOWNGRADE (local older than the agent) is reported but never pushed. It
      prints the affected yuno ids as a `kill-yuno` + `run-yuno` reminder rather
      than automating the restart. Documented at doc.yuneta.io under **Tools**.

    - **feat(yuno_agent): `install-config` alias for `create-config`.** Added by
      analogy with `install-binary`, so config installs read symmetrically with
      binary installs (`c_agent.c`). Corrected the config-command docs that this
      exposed as stale: `YUNO_LIFECYCLE.md` claimed there was no `install-config`
      and that `update-config` "creates and updates" (it only overwrites an
      existing `(id, version)`); the onboarding recipes in `YUNO_LIFECYCLE.md` /
      `SCAFFOLDING.md` / `YUNO_AUTH.md` used `update-config … version=<v>
      zcontent=$$()` — the real form is `create-config … content64=$$()` (version
      read from the file's `__version__`).

    - **chore(packages): yuno binaries + `tools/agent` on PATH per layout.** In
      `make-yuneta-agent-deb.sh` the hardcoded `outputs/yunos` PATH entries moved
      into the profile snippet's layout-detection branch (full source tree vs
      deployed `.deb` node), and `tools/agent` was added there too, so
      `sync_binaries.py` / `sync_configs.py` are runnable by name on a node.

    - **chore(tools): retire `tools/docs-migration/`.** The myst migration is
      done and the Quarto pilot was abandoned, so the two one-off helpers
      (`myst_to_quarto.py`, `strip_toctrees.py`) were removed.
      `verify_api_coverage.py` is a repo-dev verifier (not a node tool), so it
      moved to `scripts/`; its five stale "extra" reports were resolved by
      correct header→landing mapping (no docs removed). `scripts/` is repo-only;
      `tools/` ships in the `.deb`.

## v7.4.5 -- 30/May/2026
    - **fix(yuno_agent): `delete-yuno`'s snap-tag guard read the wrong metadata
      key — it was dead.** `cmd_delete_yuno` read `__md_treedb__`__tag__`, but
      the metadata key is `tag` (set in `tr_treedb.c`; the kernel guard
      `treedb_delete_node` reads `__md_treedb__`tag`). So the agent-level guard
      always saw 0 and never fired — only the kernel `treedb_delete_node`
      backstopped the actual delete (with a cryptic message), and the bogus
      `KW_REQUIRED` on a missing key risked log noise. Fixed to read
      `__md_treedb__`tag` with flag 0 (default 0 = untagged), matching the
      kernel guard and the `delete-binary` guard, and clarified the message to
      *"tagged by snap N (rollback)"*. Found while adding the `delete-binary`
      guard. Verified the key is populated: `snap-content name=<tag>
      topic_name=yunos` lists the tagged yuno records.

    - **fix(yuno_agent): `delete-binary` refuses to purge a binary a snap
      references (clear reason).** A snap pins the binaries it captured:
      `shoot-snap` stamps its id on each topic's current-primary record
      (md2 `user_flag`, surfaced as `__md_treedb__.tag`), `binaries` included,
      and `activate-snap` rolls back to exactly those records — so the binary
      file must survive or `run-yuno` fails with *"primary binary not found"*.
      The kernel `treedb_delete_node` already refuses a tagged node unless
      `force`, and `cmd_delete_binary` breaks before the `rmrdir` when the node
      delete fails — so the file was never actually lost. But unlike the
      sibling `delete-yuno`, `delete-binary` gave no reason (just a cryptic
      kernel log + a generic failure). Added the explicit agent-level guard
      (mirroring `delete-yuno`, reading `__md_treedb__.tag`): a snap-tagged
      binary is refused with *"referenced by snap N (rollback)"* and `force=1`
      overrides — which breaks that snap's rollback, as documented. Verified
      the mechanism live: `snap-content name=<tag> topic_name=binaries` lists
      the exact binary records the snap pinned.

    - **fix(yuno_agent): `list-binaries` shows the binary in use, not every
      instance.** `cmd_list_binaries` had been switched (df0e50e70) from
      `gobj_list_nodes` to `gobj_list_instances`, which made it return one row
      per `(role, version)` — identical to `list-binaries-instances`, and after
      a same-version `update-binary` (an append) even two rows for the same
      `(role, version)`. The reason given at the time ("the new instance is
      invisible in the primary index until `deactivate-snap`") was the pkey2
      staleness bug since fixed in `dbf532ec9`. Reverted `list-binaries` to
      `gobj_list_nodes("binaries", …)`: ONE node per role — the primary, i.e.
      the binary actually in use. `list-binaries-instances` keeps the full
      `(role, version)` enumeration. Verified live: `list-binaries` returns 15
      rows (one per role, the in-use version) while `list-binaries-instances`
      returns 30 (every record). The primary correctly tracks the in-use
      version — an `update-binary` updates it in place; an `install-binary` of
      a new version only changes it once `deactivate-snap` promotes+reloads it
      (correct: the new binary is not in use until then).

    - **feat(ycommand): `history` / `!history` work non-interactively, and
      fix the local-command hang.** Two problems with the history command
      outside `-i`: (1) the line editor (`C_EDITLINE`, `priv->gobj_editline`)
      is only created in interactive mode, and both `list_history()` (the bare
      `history` intercept) and `cmd_local_history()` (the `!history`
      local-table entry) read only that live editor — so `ycommand history` /
      `ycommand -c history` printed nothing even though the history is
      persisted to `~/.yuneta/history2.txt`. Both now fall back to that file
      when there is no live editor. (2) A trailing **local** command in
      non-interactive mode hung: the shutdown timeout is scheduled from
      `ac_command_answer`, which only fires for **remote** commands (a local
      `history` produces no `EV_MT_COMMAND_ANSWER`), so the queue drained and
      ycommand waited forever for an answer that never came. Added
      `schedule_exit_if_done()` at the tail of `run_next_pending()` — when the
      queue is empty, the session is non-interactive, no async command is in
      flight, and we are not in long-lived stdin-pipe mode, it schedules the
      same shutdown timeout. Interactive sessions and pipe mode (which waits
      for EOF) are unaffected.

    - **feat(snap-content): friendlier snap inspection.** The agent's
      `snap-content` (served by `C_NODE` in `c_node.c`) required the numeric
      `snap_id` AND an exact `topic_name`, so you could not ask "where does
      this snap point?" without already knowing the topic names. Two additive,
      backward-compatible changes: (1) the snap is now selectable by
      `snap_id`, `id` (alias), or `name` (resolved against `__snaps__`); the
      legacy `snap_id=` keeps working. (2) `topic_name` is now optional — when
      omitted, the command returns the **overview** of every topic the snap
      tags and how many records each (a cheap count-only walk via a new
      `snap_count_cb`, not a full load), e.g. `snap-content name=pre-744` →
      `realms:3, yunos:16, binaries:15, configurations:16, public_services:2`.
      Pass `topic_name=<topic>` to drill into one topic's foto as before. The
      `id`/`name` params were added to the param schema in both `c_node.c` and
      the agent's `c_agent.c`.

## v7.4.4 -- 30/May/2026
    - **fix(c_websocket): stop synthesizing `EV_ON_OPEN_ERROR` at the
      transport layer.** `EV_ON_OPEN_ERROR` is a high-level event owned by
      the session layer (`c_ievent_cli`, which emits it with the remote-yuno
      identity). The commit that introduced it ("EV_ON_OPEN_ERROR — close
      before open") also added an emission in `c_websocket` `ac_disconnected`
      for the "transport closed before the WS upgrade completed" case. That
      emission is mislayered and has no consumer: no FSM declares
      `EV_ON_OPEN_ERROR` as an input action, so `c_websocket` publishing it to
      its parent (`C_CHANNEL`, sitting in `ST_CLOSED` because it never opened)
      was rejected by `gobj_send_event` with "Event NOT DEFINED in state". On
      slower nodes the `run-yuno` reconnect window widens and the race fired
      once per affected yuno (1:1 with the close-before-upgrade warning).
      `ac_disconnected` now publishes only `EV_ON_CLOSE` when a real session
      existed, otherwise returns silently (pre-918be48b9 behavior), and
      `EV_ON_OPEN_ERROR` was dropped from `c_websocket` `event_types`. The
      high-level emission in `c_ievent_cli` is unchanged.

    - **fix(c_websocket): raise default `timeout_handshake` 5s → 30s.**
      During a mass yuno launch (`kill-yuno` + `run-yuno`) every yuno's
      `agent_client` (`C_IEVENT_CLI` → … → `C_WEBSOCKET`) reconnects to the
      single-threaded agent at once; the agent's event loop is stalled doing
      launch work (loading binaries, fork/exec, treedb) and could not complete
      each WS upgrade handshake within the old 5s window for the yunos at the
      back of the queue → "Timeout waiting websocket handshake" in synchronized
      bursts (one per launched yuno). The timeout firing was counterproductive:
      it `ws_close` + `EV_DROP`s and reconnects after
      `timeout_between_connections`, adding load to the very herd that caused
      it. The new 30s default sits comfortably above the observed agent
      loop-stall during mass launch; the attr is per-instance configurable
      (`SDF_PERSIST`) so a public-facing WS server that wants faster dead-peer
      detection can still tighten its own. The remaining root-cause work
      (jitter on `timeout_between_connections` in `c_tcp` to break the
      synchronized reconnect herd) is not addressed here.

    - **feat(yuno_agent): single command response for `run-yuno`, plus a
      `play` knob.** Scripts driving the agent need exactly ONE answer per
      command to stay in sync; `kill-yuno`/`pause-yuno`/`play-yuno` already
      do, but `run-yuno` emitted ~2N answers over N yunos. Two independent
      causes were fixed: (1) `cmd_run_yuno` created one `C_COUNTER` with
      `max_count=1` INSIDE the per-yuno loop (one answer each); it now
      aggregates the `EV_ON_OPEN` filters into a single counter with
      `max_count=total` AFTER the loop, mirroring kill/pause/play — one
      `"N yunos found to run"` answer. (2) The implicit auto-play: on connect
      `ac_on_open` reconciles `must_play` by calling `play-yuno`, one async
      answer per yuno. A new `run-yuno play=0` parameter (default `1`,
      backward-compatible) launches the process(es) WITHOUT auto-play, so a
      script does `run-yuno play=0` (1 answer) then `play-yuno` (1 answer,
      aggregated over already-running yunos). The suppression is per-launch and
      kept **in-memory in the agent**: `run-yuno play=0` records each
      `launch_id` in `priv->no_play_launches`, and `ac_on_open` consumes it by
      matching the connecting yuno's `identity_card`launch_id`, deleting the
      marker on first connect. It is NOT a treedb column and does NOT mutate the
      persistent `must_play`; a watcher crash relaunch reuses the same
      `launch_id` but the marker is already gone, so autonomous `must_play`
      recovery is untouched.

    - **fix(tr_treedb): refresh the pkey2 secondary index on a runtime
      `treedb_save_node()`.** The secondary `pkey2` index kept objects
      SEPARATE from the primary `id` index, populated only while loading
      from disk (`load_pkey2_callback` gated on `sf_loading_from_disk`).
      At runtime `treedb_update_node()` mutated the primary node in place
      and `treedb_save_node()` only appended a tranger row — neither
      touched the secondary index, so `treedb_get_instance()` /
      `treedb_list_instances()` returned the OLD content after an update.
      Surfaced as the agent's `list-binaries` showing the previous binary
      right after a successful `update-binary` (the agent returns the
      in-memory pkey2 index; the new record was already on disk). Now
      `treedb_save_node()` re-points every pkey2 slot of the node at the
      node object itself. No-op for topics without pkey2s. New regression
      test `tests/c/tr_treedb_update_instance` (create → reload from disk →
      update → assert via get_instance/list_instances); it fails against
      the pre-fix code.

    - **fix(emailsender): correctness + resilience of the SMTP send path.**
      (1) Duplicate `MAIL FROM` → "503 MAIL already given": on AUTH-OK the
      session published EV_ON_OPEN (whose subscriber already begins the
      queued send, being idle) and then began it again; snapshot the
      pending flag before publishing. (2) Permanent 5xx rejections now
      dead-letter immediately (reply code forwarded via EV_ON_CLOSE /
      EV_ON_MESSAGE; `code>=500` = permanent, 4xx/timeout/drop = transient
      retry). (3) Binary (non-UTF-8) bodies persisted base64 under
      `body_base64` instead of being silently dropped by `json_stringn`.
      (4) Reconnection is owned by `c_smtp_session`, not the sender: after a
      `timeout_inactivity` idle-close, `c_emailsender` just dispatches the head
      message (it no longer carries any reconnect timer/backoff), and
      `c_smtp_session` — which must redo the handshake — reconnects its bottom
      `C_TCP` on `EV_SEND_MESSAGE` in `ST_DISCONNECTED` and sends on reaching
      `ST_IDLE`. A handshake failure (AUTH 535, EHLO/banner 5xx) is transient
      for the in-flight message (the server never saw it); only a 5xx in the
      message's own MAIL/RCPT/DATA transaction dead-letters it. (5) Fixed a
      shutdown SIGSEGV at its root: `tira_dela_cola()` now returns early when
      the yuno is not playing (`gobj_pause()` clears the playing flag before
      `mt_pause()`/`close_queues()`), so a deferred EV_ON_CLOSE delivered
      during shutdown no longer touches the closed queue — no defensive NULL
      check needed.

    - **fix(c_tcp): retry with backoff after a failed reconnect in the
      inactivity model.** `set_disconnected()` always cleared the timer in the
      `timeout_inactivity` model — correct for a deliberate idle-close, but it
      also stalled a failed on-demand reconnect (no retry, and no
      EV_DISCONNECTED for a never-connected socket). Now only an idle-close
      (`ac_timeout_inactivity` sets `idle_closed`) skips the retry; a connect
      failure / dropped link schedules the `timeout_between_connections`
      backoff and retries via `EV_TIMEOUT -> ac_connect`, like the classic
      model. This is the layer that owns reconnection/backoff (the emailsender
      rework relies on it).

    - **fix(c_tcp): keep the pending tx queue across a FAILED reconnect in the
      inactivity model.** `set_disconnected()` flushed `dl_tx` on every
      disconnect, so bytes queued while disconnected (to be sent on the
      on-demand reconnect) were lost if the connect failed before succeeding —
      the message vanished silently and was never delivered. Now the queue is
      kept when the connection was NEVER established (`inform_disconnection`
      still FALSE) in the `timeout_inactivity` model on a running gobj, and
      `start_pending_writes()` flushes it once a retry connects. An established
      connection still flushes (its byte stream is broken); `mt_stop()` still
      flushes unconditionally (no leak on stop). New regression test
      `tests/c/c_tcp_inactivity` test4 (queue while the server is down → fail
      retries → server up → echo confirms delivery); it fails against the
      pre-fix code (no echo, FIFO timeout).

    - **refactor(c_tcp): `timeout_inactivity` / `timeout_between_connections`
      / `rx_buffer_size` are deployment config (`SDF_RD`), not runtime
      knobs** — dropped `SDF_WR` (and the misleading `SDF_PERSIST` on the
      two timeouts); widened `priv->timeout_inactivity` to `json_int_t`.

    - **fix(yev_loop): retry the static resolver's UDP `recv()` on `EINTR`,
      and log `gai_strerror(ret)` not `strerror(errno)`.** A signal
      interrupting the blocking DNS `recv()` made `yuneta_getaddrinfo()`
      fail spuriously (the logged "Interrupted system call" was a stale
      residual errno; getaddrinfo-family return an `EAI_*` code). Both
      `getaddrinfo() FAILED` sites now log the real `gai_*` cause.

    - **fix(ytls): send SNI in OpenSSL client handshakes.** The
      OpenSSL backend never set the TLS `server_name` extension —
      the code was a `// TODO SSL_set_tlsext_host_name` stub — so
      client ClientHellos went out without SNI. Virtual-hosted TLS
      endpoints behind a CDN/WAF (e.g. an Imperva Incapsula front
      end) reject SNI-less handshakes with HTTP 403. The mbedTLS
      backend already set SNI and `c_tcp` already supplies
      `ssl_server_name`; only the OpenSSL path dropped it. Now
      stores `ssl_server_name` in `init()` and calls
      `SSL_set_tlsext_host_name()` per-connection in
      `new_secure_filter()` for client sockets. Server side is
      unaffected (no servername callback registered, so incoming
      SNI is ignored). Verified end-to-end: 403 → 200 against an
      Imperva-fronted HTTPS API.

    - **fix(timeranger2): fire key-delete callbacks for `rt_by_disk`
      followers.** `fire_key_deleted_locally()` skipped every entry
      with an `fs_event_client` (i.e. every `rt_by_disk` follower),
      assuming the `FS_SUBDIR_DELETED` inotify branch fired their
      `key_deleted_callback`. But that branch's only firing
      mechanism *was* `fire_key_deleted_locally()`, which skipped
      them — so a `rt_by_disk` follower's key-delete callback never
      fired on a `tranger2_delete_key()`. The follower's in-memory
      state only reconciled on restart (LOADING reload from
      `keys/`); live deletes were silently dropped for every
      fs-watcher follower framework-wide. Split the fan-out by
      transport via a new `fs_followers` flag: the master in-process
      path fires only non-watcher subscribers (rt_mem
      lists/iterators); the `FS_SUBDIR_DELETED` inotify branch fires
      only the `rt_disk` followers (the inotify event IS their
      signal). Each subscriber now fires exactly once; also removes
      a latent same-process double-fire of non-watcher subscribers.
      Verified: a master `tranger2_delete_key` now drops the key
      from a separate-process follower's in-memory cache live, no
      restart.

    - **fix(yuno_agent): promote highest `yuno_release` to primary on
      `restart_nodes`.** The treedb primary for a yuno-id is the
      highest-ROWID record, not the highest `yuno_release`:
      lifecycle writes (kill/run/snap) append records for whatever
      release is *active*, so after `install-binary` +
      `find-new-yunos` an older release could stay primary and
      `deactivate-snap` relaunched it instead of the new version
      (the long-standing "force volatil" TODO; a `shoot-snap`
      between `find-new-yunos` and `deactivate-snap` reliably
      triggered it). New `promote_highest_release_yunos()` runs in
      `restart_nodes()` BEFORE the treedb reload: for each id whose
      highest non-disabled `yuno_release` is newer than the current
      primary, it re-appends that release so it becomes the highest
      rowid; the reload then makes it primary and
      `run_enabled_yunos()` launches it. An append does NOT move the
      in-memory primary index — only the reload rebuilds it — so the
      promote must precede the `gobj_stop/start`. Version order via
      the existing `get_n_v()`. (`volatil` itself was already
      honored — `mt_update_node` routes volatil updates to
      `set_volatil_values`, in-memory only — so the culprit was the
      non-volatil lifecycle/snap writes, not the run-update.)
      Verified: a multi-yuno realm upgraded to a new release with a
      single `deactivate-snap`.

    - **fix(yev_loop): retry transient ENOMEM in
      `io_uring_queue_init_params`**. A synchronised restart of
      many yunos (e.g. an agent `deactivate-snap` on a node with
      10+ yunos) used to drop 1-3 SIGABRT cores per yuno in
      `/var/crash`, even though every yuno eventually came up
      after the `ydaemon` watcher relaunched it. Root cause was
      `yev_loop_create` aborting via `LOG_OPT_ABORT` on a
      transient `-ENOMEM` from io_uring init: rings consume
      pinned kernel memory (RLIMIT_MEMLOCK / vm.max_user_locks)
      and a simultaneous restart of N yunos saturates that
      budget for a few ms while the previous rings' pages are
      released. Forensic evidence: 12 cores at 13:23 today with
      identical bt bottoming at `yev_loop.c:184`, all `err=-12`
      with `entries=32768`. Fix wraps the init call in a
      5-iteration exponential-backoff retry (100/200/400/800/1600
      ms ≈ 3 s) for `ENOMEM`/`EAGAIN` only; non-transient errors
      (EINVAL, ENOSYS, EPERM…) fall through to the original
      abort path unchanged. Each retry logs a warning so an
      operator can see the pressure event without it being
      silent. Local stress test (3 consecutive `deactivate-snap`
      cycles = 48 yuno restarts) generated 0 cores.

    - **fix(ycommand): keep stdin-pipe queue draining when a
      command returns -1**. The long-lived stdin-pipe mode added
      in 7.4.3 inherited the ybatch convention from `-c` / `-i` /
      file-fed batches: a `-1` result with no leading `-` on the
      command drops the rest of the queue. That convention is
      hostile to stdin-pipe deploys — the operator has already
      piped every line in, and one common non-fatal `-1` (e.g.
      `install-binary` returning "Binary already exists" for a
      slot that's already filled) silently swallows the rest.
      Surfaced on the 7.4.3 wattyzer deploy: binaries got
      registered, then `find-new-yunos` + `deactivate-snap`
      vanished, leaving yunos on the old release until the
      trailing commands were re-run by hand. Fix: in
      `stdin_pipe_mode` every command behaves as `ignore-fail`
      (no queue clear on error). Explicit `-` prefix path stays.
      Repro matrix (4 install-binary in pipe, slot pre-filled):
      before fix 3/4 responses, after fix 4/4. The earlier
      "WS frame interleaving" hypothesis logged in TODO.md was
      ruled out (kept as a post-mortem trail).

    - **fix(emailsender): retry queued emails instead of
      dead-lettering on the first failure, and persist the body**.
      Any send failure (SMTP server down, wrong URL, rejected
      AUTH, or simply the SMTP child still connecting when the
      dequeue timer fired) used to move the email straight to the
      `emails_failed` dead-letter queue and unload it — `max_retries`
      was declared but never used and nothing drains the failed
      queue, so one transient hiccup shelved the message forever.
      Now the message stays at the head of `emails_queue` and is
      only dispatched while the SMTP session is connected and
      authenticated (a momentary outage just waits and retries on
      reconnect); transient failures are retried up to `max_retries`
      total attempts before being dead-lettered. The body is now
      persisted as a string in the queue — it was carried as a
      transient gbuffer pointer that the dequeued kw's auto-decref
      freed after the first attempt, so retries (and any yuno
      restart) lost the body. Also split RCPT recipients on `;` as
      well as `,` (an Outlook-style list or a stray trailing `;`,
      e.g. logcenter's summary `to`, was rejected by the server as
      `501 Invalid TO`). Deployed and validated live on
      `emailsender^artgins`.

    - **feat(emu_device): implement the frame-emission path on
      timeranger2**. The device-gate emulator was a scaffold: its
      replay was written against the removed timeranger v1 API and
      its `__output_side__` had no TCP connex (the v6 Connex/Tcp0
      globals were dead). Ported to v7 — the output side is built
      in code (`C_IOGATE > C_CHANNEL > C_PROT_RAW > C_TCP` to `url`,
      like `sgateway`); `mt_play` loads matching `frame64` records
      via `tranger2_open_list`, and on connect it sends the
      `leading` frame then `window` frames every `interval` ms.
      Compile-verified only; end-to-end runtime validation (needs a
      `frame64` topic + a TCP sink) is tracked in `TODO.md`.

## v7.4.3 -- 27/May/2026
    - **feat(emailsender)!: drop libcurl, native SMTP over ytls**.
      `emailsender` was the only yuno that linked libcurl, which
      dragged OpenSSL/libssh2/c-ares/libidn2/libpsl/libnghttp2/3/
      zlib/brotli into its runtime graph. The dev-host glibc kept
      bumping while production stayed on older versions (e.g. 2.36
      on `app.wattyzer.com`), so emailsender was the one yuno where
      every upgrade needed a build environment matched to the
      target — and it was deliberately skipped from the 7.4.1
      deploy bundle for that reason. Three new building blocks
      land in this release: (1) `C_SMTP_SESSION` (yunos/c/
      emailsender/src/c_smtp_session.{c,h}) — a CHILD-pattern
      protocol gclass that owns a `C_TCP` bottom and walks the
      RFC 5321 submission FSM (banner → EHLO → AUTH PLAIN →
      MAIL FROM → RCPT TO → DATA → \r\n.\r\n → QUIT) with
      multi-recipient support, RFC 5321 §4.5.2 dot-stuffing and
      a best-effort QUIT on `mt_stop`. Uses `istream_read_until_
      delimiter("\r\n", 2, EV_RX_LINE)` so raw bytes from C_TCP
      (`EV_RX_DATA`) and parsed lines (`EV_RX_LINE`) flow through
      distinct actions and never feed back into the istream.
      (2) `mime_encoder.{c,h}` — pure helpers, no gclass; builds a
      complete RFC 5322 message with optional single attachment
      (`multipart/mixed`) or inline-image attachment with
      Content-ID (`multipart/related`), base64-wrapped at 76
      chars per RFC 2045 §6.8, and RFC 2047 base64 encoded-words
      for non-ASCII Subject / From display-name. (3) Cutover in
      `c_emailsender.c`: `priv->curl` → `priv->smtp` as a
      pure_child created with `{url, username, password,
      helo_name=gethostname()}`; the synchronous `gobj_send_event
      (priv->curl, EV_CURL_COMMAND, …)` + immediate
      `process_curl_response` flow becomes async — `ac_smtp_
      command` MIME-encodes, sends `EV_SEND_MESSAGE` to the smtp
      child and stays in `ST_WAIT_RESPONSE`; the response arrives
      later via the new `ac_on_message` handler. Kernel-side
      precursor: `_yev_protocol_fill_hints()` in `kernel/c/
      yev_loop/src/yev_loop.c` learns the `smtps` schema (port
      465, marked `secure=TRUE`) alongside the existing
      `mqtts`/`wss` — strictly additive. Together this removes
      `libcurl4-openssl-dev` from `docs/doc.yuneta.io/
      installation.md`, drops `find_package(CURL REQUIRED)` and
      `${CURL_LIBRARIES}` from the yuno's CMakeLists, deletes
      `c_curl.{c,h}` outright, and brings the emailsender binary
      down to `ldd` reporting only `libgcc_s` + `libc` (vs the
      previous ~12 shared libs). `emailsender^artgins` is already
      pointed at `smtps://ssl0.ovh.net:465` in all three realms +
      the staging batch, so the cutover is a no-config-change
      deploy. **Breaking change for callers building EV_SEND_EMAIL
      kw manually**: the libcurl-era attrs `strict_tls` and
      `auto_inline_images` are ignored (TLS is now decided by the
      URL schema; auto-inline-image HTML rewriting was a libcurl
      `curl_mime_*` feature not reimplemented). The `cmd_send_
      email` command schema is unchanged; existing callers (the
      whole estadodelaire batch + every realm config) keep
      working without edits.

    - **feat(ycommand): long-lived stdin-pipe session keeps OAuth2
      auth open across many commands**. Until now `ycommand` had
      three input shapes: `-c CMD` (one command, exit), `-i`
      (interactive editline over a raw TTY), and an undocumented
      synchronous pipe path inside `ac_on_open` that did
      `while(fgets(line, stdin))` — fine for pre-buffered batches
      but it blocked the yev_loop between lines, so any
      programmatic driver that wanted to send a command, read its
      response, then send another would hang. `-i` was also
      unusable for non-TTY drivers (e.g. claudia-console running
      `ycommand` via Bash) because `tty_keyboard_init`
      unconditionally calls `enableRawMode`, which fails with
      "NOT a TTY" on a piped fd. Net effect: every remote command
      paid the full OAuth2 ROPC round-trip (~200-400 ms against
      Keycloak) even when the caller had ten queued up. New
      behavior, no new flag: when stdin is not a TTY and neither
      `-c` nor `-i` was supplied, ycommand sets up an io_uring read
      event on `dup(STDIN_FILENO)` (yev_loop refuses fd<=0, so we
      dup) and drives lines through the existing
      `split_commands_into_queue` + `run_next_pending` machinery.
      The process stays alive between commands, EOF triggers an
      orderly shutdown via the existing `set_timeout(timer,
      wait*1000)` → `ac_timeout` → `exit()` path, and a
      `priv->cmd_in_flight` gate serialises async dispatches so a
      stdin line arriving mid-flight enqueues instead of racing.
      Auth happens once; the rest of the session is free. Tested
      against local `ws://127.0.0.1:1991` (4-line batches and
      delayed sequences with 3 s gaps between lines) and against
      `wss://app.wattyzer.com:1993` + OAuth2 (three commands with
      2 s gaps, single ROPC). Backwards-compatible: the pre-existing
      `-c` and `-i` paths are untouched, the pipe-mode trigger is a
      strict superset of the previous synchronous fgets behaviour,
      and `echo cmd | ycommand` keeps producing the same output as
      before (just via an event-driven reader). `c_ycommand.c`
      grew +220/-25; no changes elsewhere.

    - **docs(philosophy): add "The Typed-Graph Model" chapter**.
      New page under `philosophy/` slotted between
      [Design Principles](doc.yuneta.io/philosophy/design_principles.md)
      and [Domain Model](doc.yuneta.io/philosophy/domain_model.md),
      articulating the conceptual claim the framework rests on: data
      and behavior are two views of the same typed graph
      (`topic`↔`gclass`, `node`↔`gobj`, `hook`/`fkey`↔
      subscription/`bottom_gobj`, with `sdata_desc_t` describing
      schemas on both planes). Sections cover: the unit is the
      typed binding, not just the node; the two-plane primitive
      table; what kinds of organisation the model can express
      (hierarchies, matrix, workflows, communication topologies,
      versioned-over-time); what does not fit cleanly (schemaless
      iteration, OLAP, eventually-consistent distributed state,
      truly opaque payloads); the implicit axiom; the payoffs at
      scale; and the empirical justification from 15 + years of
      v2/v6 production. Cross-links added in `philosophy/`
      neighbours and reverse-links from `yunos/c/yuno_agent/`'s
      `ENTRY_POINT.md` (See also), `GOBJ.md` (Conceptual frame
      callout: behavior plane) and `YUNO_TREEDB.md` (Conceptual
      frame callout: information plane) so a reader landing in the
      technical chapters can step up one level on demand. Build is
      warning-free; the new chapter appears in the TOC under
      Philosophy.

    - **fix(install-binary): surface the real cause in error
      response**. `cmd_install_binary` built its failure comment as
      `json_sprintf("Cannot create binary: %s",
      gobj_log_last_message())`, which produced *"Cannot create
      binary: "* (empty cause + trailing space) whenever the
      underlying `treedb_create_node` returned NULL because the
      `(id, pkey2=version)` combination already existed — that path
      logs *"Node already exists"* via `gobj_log_warning`, and
      `gobj_log_warning` does not populate `last_message` (only
      `LOG_ERR` and above do). After the per-command reset in
      `command_parser` (7.4.1, `b1abd7f69`), the buffer is `""` by
      then. Two-layer fix, same shape as the snap commands in 7.4.1:
      `treedb_create_node` now calls `gobj_log_set_last_message()`
      alongside the warning so the cause (*"Node already exists in
      '<topic>': id='<id>'"*) reaches every caller that pipes
      `gobj_log_last_message()` into the response (≈13 callers in
      `c_node.c` benefit alongside `cmd_install_binary`);
      `cmd_install_binary` reads `last_msg` once and falls back to
      `"(see log)"` if it's empty, so the response is always
      informative regardless of whether layer-1 was reached. Drive-by:
      removed the stale `// TODO check tranger2_write_user_flag`
      marker above `treedb_shoot_snap` — the function was completed
      in 7.4.0/7.4.1 (`4c89e4b2c` + `46f8f0434`) and the audit
      confirmed no remaining wiring gap.

## v7.4.1 -- 27/May/2026
    - **fix(command_parser): stop misleading stale strerror in
      command responses**. Many `cmd_*` in `c_node.c` build their
      failure comment as `json_string(gobj_log_last_message())`,
      but `gobj_log_set_last_message()` is only called by
      `gobj_log_*()` with priority `<= LOG_ERR`. If the failure
      path logs at `LOG_INFO` (or doesn't log at all),
      `last_message` keeps whatever it had from the previous
      `LOG_ERR` — frequently `strerror(errno)` of an earlier TCP
      disconnect ("Connection reset by peer"). The response was
      being delivered correctly with that strerror as the comment;
      ycommand rendered it verbatim and it looked indistinguishable
      from a real network error, sending operators down a wild
      diagnostic chase. Two-part fix at the kernel command
      boundary in `kernel/c/gobj-c/src/command_parser.c`: (1)
      `command_parser()` resets `last_message` to `""` at entry,
      so every command dispatch starts with a clean slate;
      (2) `build_command_response()` substitutes `"(see log)"`
      when the response is a failure (`result != 0`) and the
      comment is an empty string, so callers that use the bare
      `json_string(gobj_log_last_message())` idiom produce a
      useful placeholder instead of `ERROR -1: `. Success
      responses keep their empty comment (cmd_topics etc. return
      data without a comment by design). Documented the new
      semantics in
      `docs/doc.yuneta.io/api/logging/log.md`
      (`gobj_log_last_message` + `gobj_log_set_last_message`).
    - **fix(agent): `list-binaries` enumerates every
      `(role, version)` instance**. `cmd_list_binaries` called
      `gobj_list_nodes("binaries", ...)`, which only returns the
      in-memory primary per id (role). After an `install-binary`
      that added a second version under the same role, the new
      instance was invisible until a `deactivate-snap` rebuilt
      the primary index — and even then only the most-recent
      version survived. The doc claimed *"returns all rows"* but
      the call had never matched that promise. Switched to
      `gobj_list_instances("binaries", "", ...)`: the topic has
      `pkey2=version`, so the instances iterator returns one row
      per `(role, version)` and multi-version installs are
      visible from the moment `install-binary` appends the
      record. Validated with two coexisting `emailsender`
      versions (7.4.1 + 7.4.2): `list-binaries` now returns four
      rows instead of three. `list-binaries-instances` stays as
      the explicit "instances" alias. `YUNO_LIFECYCLE.md` table
      updated to match.
    - **fix(snap): implement `snap-content` + recover error
      responses to ycommand**. (a) `cmd_snap_content` in
      `c_node.c` was a literal `"TODO"` stub. Now walks the
      requested topic with `tranger2_open_list` filtered by
      `user_flag=snap_id` and a `load_record_callback` that
      drains matching records into a `json_array` carried via
      the rt's `extra` (merged into the rt object by
      `json_object_update_missing_new`, so the callback reads
      `list->snap_data`, not `list->extra->snap_data`).
      Validates `topic_name` and `snap_id` (1..65534, matching
      the `uint16_t` md2 `user_flag` range), returns the schema
      + the array and a `(count)` comment. (b) Error responses
      from `shoot-snap` / `activate-snap` / `deactivate-snap`
      were arriving at ycommand as `"Connection reset by peer"`
      instead of the real cause. Root cause:
      `json_string(gobj_log_last_message())` produced an empty
      JSON string whenever the kernel function logged with
      `gobj_log_info` (which doesn't populate `last_message`),
      and the buffer still held the strerror of a prior
      disconnect. Two-layer fix: `treedb_shoot_snap` /
      `treedb_activate_snap` explicitly call
      `gobj_log_set_last_message()` in their "already exists"
      and "not found" paths; `cmd_shoot_snap` /
      `cmd_activate_snap` / `cmd_deactivate_snap` build the
      error comment with
      `json_sprintf("Cannot ... '%s': %s", name, empty_string(last)?"(see log)":last)`
      so the comment is never empty even if some future caller
      forgets the layer-1 update. Note: there are ~13 other
      `cmd_*` in `c_node.c` with the same
      `json_string(gobj_log_last_message())` pattern — same trap
      — covered by the systemic `command_parser` reset documented
      in the entry above.
    - **fix(snap): preserve previous snap's tag + harden
      `run_yuno` launcher**. (a) `treedb_shoot_snap` stamped
      `user_flag` IN PLACE on every primary record. A second
      `shoot-snap` over a record that hadn't changed since the
      first snap therefore overwrote the older snap's tag,
      making that record unreachable from
      `activate-snap(older)`. Fix: if the primary record already
      carries a tag from a different snap, append a CLONE via
      `tranger2_append_record(user_flag=new)` so the older
      record keeps its tag. Untagged primaries (and same-snap
      re-stamps) still take the in-place path — no extra
      storage. The in-memory `__md_treedb__` is intentionally
      left untouched on the clone branch so subsequent
      `treedb_save_node()` appends keep using the original base
      `rowid`. Validated end-to-end: shoot A → 13 records carry
      `uflag=1`; shoot B (no intermediate change) → still 13
      records carry `uflag=1` + 13 clones carry `uflag=2`;
      `activate-snap A` and `activate-snap B` both restore all
      yunos. (b) `build_yuno_running_script` in `c_agent.c` took
      an uninitialised `char bfbinary[]` from the caller's stack
      and could early-return `0` (silently) on a missing realm
      or binary. All three callers ignored the return value,
      then ran or serialised whatever garbage was on the stack —
      hence the corrupted `/yuneta/bin/<role>^<name>.sh`
      launchers (~21-27 bytes of stack noise) that
      `activate-snap` produced when the binary record didn't come
      back. Fix: zero-init `bfbinary` at function entry, log the
      two early-return sites (no silent errors), and have every
      caller check the return value and respond with an error
      instead of piping uninitialised memory into
      `run_process2()` or a JSON reply. Treedb `treedb_shoot_snap`
      doc page updated to describe the new clone-vs-stamp
      behaviour.
    - **fix(timeranger2): silence two `-W` warnings without
      losing errors**. (a) `treedb_shoot_snap` in
      `kernel/c/timeranger2/src/tr_treedb.c` now returns the
      accumulated `ret` so `tranger2_write_user_flag` failures
      across topics surface to callers instead of being silently
      dropped. (b) `mirror_key_delete_to_disks` in
      `timeranger2.c` uses `build_path()` to assemble the
      `disks/<rt_id>/<key>` path; `build_path` already syslogs
      `LOG_CRIT` on overflow, so no silent skip on truncation
      (closes a `-Wformat-truncation` warning without
      introducing a silent early-out).
    - **docs(api/treedb): document the real snap semantics**.
      The `treedb_shoot_snap` / `treedb_activate_snap` API pages
      described the surface only — name, parameters, return — and
      missed the parts that determine whether snaps actually work
      for the caller: `treedb_shoot_snap` tags the live `.md2`
      record's `user_flag` in place via
      `tranger2_write_user_flag` (so rowid order is preserved
      and re-shoots overwrite prior tags on the same record);
      `treedb_activate_snap("__clear__")` is the deactivate
      path; the active/inactive toggle only flips a flag, and
      the new primary visibility materialises on the **next**
      `treedb_open_db()` — not on the call itself. Updated
      `docs/doc.yuneta.io/api/timeranger2/treedb.md` §§
      `treedb_shoot_snap` + `treedb_activate_snap` with the
      reload semantics + the in-place tag mechanic + the
      16-bit snap-id ceiling. Companion to the `treedb_shoot_snap`
      completion shipped in the same release.
    - **feat(tr_treedb): complete `treedb_shoot_snap` so
      `activate-snap <name>` rolls back primaries correctly**.
      The TODO at the heart of `treedb_shoot_snap` was a dead
      branch: it walked the primary index of every topic but
      the actual `tranger2_write_user_flag` call was commented
      out, so snaps only ever created an entry in `__snaps__`
      and never tagged any record. The agent's `activate-snap
      <name>` path queried `snap_tag` correctly on reload (see
      `treedb_open_db` line 1299 — the user_flag filter is
      enforced), but with no record carrying the tag the load
      returned an empty primary index, and the rollback silently
      did nothing — the test suite never caught it because there
      was none. Wired the tag write in-place via
      `tranger2_write_user_flag(tranger, topic_name, key, t,
      i_rowid, user_flag)` so the existing record gets stamped
      without inflating the `.md2` (using `treedb_save_node`
      would create a new instance at the highest rowid and
      then steal "latest" after `deactivate-snap`, masking the
      newer records the user actually wants live). Also tightened
      the snap-id range check from 32-bit (`0xFFFFFFFF`) to 16-bit
      (`0xFFFF`) since `user_flag` is `uint16_t` end-to-end. New
      regression: `tests/c/tr_treedb_snap` walks 9 phases
      modelling the agent's upgrade lifecycle (seed v1 → add v2
      → shoot snap_v1 → deactivate → reload picks v2 → add v3 →
      deactivate → reload picks v3 → shoot snap_v3 → activate
      snap_v1 → reload rolls back to v1 → activate snap_v3 →
      reload to v3 → deactivate → stays at v3) on two topics
      (`binaries` + `yunos`) keyed exactly like the agent's
      `binaries` / `configurations` / `yunos`. `tr_treedb` and
      `tr_treedb_delete_instance` rerun green against the patched
      library — no regressions.
    - **docs(yuno_agent): document the version-bump upgrade flow**.
      `kill-yuno` + `run-yuno` does not pick up a new release —
      `cmd_run_yuno` walks the `yunos` topic primary index, which
      keeps pointing at the older `pkey2` (`yuno_release`) after
      `find-new-yunos create=1` appends the new row. The fix is
      always `install-binary` → `find-new-yunos create=1` →
      `deactivate-snap`. `deactivate-snap` with no args (and no
      active snap) is the only supported way to trigger
      `restart_nodes()` (`c_agent.c:8816`), which SIGKILLs every
      running yuno, `gobj_stop/start`s the treedb resource so the
      primary index is rebuilt from disk with the newest `pkey2`
      first, then runs every must-play yuno. Equivalent to
      `yshutdown` + `restart-yuneta` at the agent-process level,
      but without restarting the daemon. Added as `YUNO_LIFECYCLE.md`
      §6.5 + §6.6 (rollback via `shoot-snap` / `activate-snap`) and
      refreshed `CLAUDE.md` §3 with the same-version vs version-bump
      split. Verified live with `gate_pvpc 1.3.1.0 → 1.3.1.1`
      on the local agent.

## v7.4.0 -- 26/May/2026
    - **chore(lib-yui)!: declarative shell stack removed —
      `@yuneta/lib-yui` jumps to 8.0.0**. The new declarative
      shell (`C_YUI_SHELL`, `C_YUI_NAV`, `C_YUI_PAGER`,
      `C_YUI_WIZARD`, `shell_modals` and every `shell_*_helpers`
      module + the full Playwright e2e suite and the `test-app/`
      vite project) was already being maintained from the
      wattyzer-vendored copy at `wattyzer/gui/src/lib-yui`. The
      kernel copy was just dead weight in the bundle consumed
      by estadodelaire (legacy apps that only use
      `C_YUI_MAIN` + `WINDOW` + `TABS` + routing) and the two
      copies had drifted enough to make every fold-back a
      conflict. Verified by grep that neither legacy consumer
      imports any shell symbol before deleting. `dist/lib-yui.es.js`
      is now 3.4 MB / gzip 706 KB (~25% lighter). Migration: if
      you need the declarative shell, the canonical copy is
      `wattyzer/gui/src/lib-yui/` (private repo) as of
      2026-05-15. The yuno-skeleton `js_gui` scaffold that
      referenced `register_c_yui_shell` was dropped here too;
      the replacement scaffold lives in
      `wattyzer/templates/js_gui/`.
    - **chore(ext-libs): three security bumps (v1.12 → v1.13 →
      v1.14)**. v1.12: nginx 1.28.3 → 1.30.1 (CVE-2026-42945),
      openresty → 1.29.2.4, openssl 3.6.1 → 3.6.2. v1.13: nginx
      → 1.30.2 (CVE-2026-9256, buffer overflow in
      `ngx_http_rewrite_module`). v1.14: openresty → 1.29.2.5
      (backports the CVE-2026-9256 patch into the
      openresty-bundled nginx + a `proxy_protocol v2`
      over-read fix). All three are pin-only — nginx and
      openresty are separate dynamically-linked binaries (see
      `configure-libs.sh` v1.10), so no yuneta consumer /
      header / CMake change rides along. OpenSSL deliberately
      held on the 3.6 LTS series; the 4.0 jump (non-LTS,
      EOL 2027-05, drops engines / legacy init) is tracked
      separately.
    - **refactor(tranger2): rename `tranger2_delete_record` →
      `tranger2_delete_key`**. Locks the vocabulary
      timeranger2 was using loosely: *record* = a primary key
      (whole `keys/<key>/` directory, deleted via
      `tranger2_delete_key`); *instance* = one row of that
      key's `.md2` index, addressed by
      `(key, __t__, rowid)`. The legacy name is kept as a
      source-level alias
      (`#define tranger2_delete_record tranger2_delete_key`),
      so external callers keep compiling unchanged. In-tree
      caller (`treedb_delete_node`) updated; README, the
      timeranger2 API page (with the old MyST anchor preserved
      so external links to `(tranger2_delete_record)=` keep
      resolving) and the appendix index follow the rename. A
      subsequent commit dropped "soft" from the delete-instance
      vocabulary: granularity, not reversibility — both
      deletes are irrecoverable.
    - **feat(tranger2): `tranger2_delete_instance()` — per-row
      tombstone**. Mutates one row of the `.md2` index in place via
      `sf_deleted_instance = 0x0400` (reinstated in `system_flag2_t`
      on the inherited side of the mask, so `rt_by_disk` followers
      see the same tombstone as the master). Optional `zero_payload`
      overwrites the matching `__size__` bytes at `__offset__` in the
      data `.json` for sensitive-data wipes. Three read sites honour
      the bit and skip dead rows: `tranger2_open_iterator` history
      loop, `tranger2_iterator_get_page`, and
      `publish_new_rt_disk_records`. Treedb is downstream and
      inherits the skip with no `tr_treedb` change. Master-only.
      Second delete of the same row is a silent no-op. `rowid`s do
      NOT renumber; `iterator_size` / `total_rows` keep counting
      slots, not live rows. `tranger2_read_record_content` and
      `tranger2_read_user_flag` still serve dead rows when the caller
      addresses them directly (audit / wipe-verification tooling).
      Coverage: `tests/c/timeranger2/test_delete_instance.c` (5
      sub-cases).
    - **feat(tranger2): `tranger2_delete_key()` propagates to
      subscribers**. Pre-2026-05-26 the function `rmrdir`'d
      `keys/<key>/` and cleared the in-memory rollup cache, but
      never notified subscribers — `rt_mem` listeners kept stale
      references and `rt_by_disk` followers in other processes kept
      their cached view alive. Now: (1) `topic/disks/<rt_id>/<key>/`
      subdirectories are removed BEFORE the live `keys/<key>/`, so
      followers catch the deletion on the standard inotify channel
      (`FS_SUBDIR_DELETED_TYPE`, the v7 TODO branch that had been
      logging "NOT processed" since inception is now wired); (2)
      in-process subscribers receive a registered
      `tranger2_key_deleted_callback_t` via the new
      `tranger2_set_rt_key_deleted_callback()` setter. Additive
      typedef and setter — no breaking signature change to
      `open_rt_mem` / `open_rt_disk` / `open_iterator`. Coverage:
      `tests/c/timeranger2/test_delete_key_propagation.c` (5
      sub-cases). Wattyzer's "tombstone-then-delete" workaround in
      `db_history_wz` becomes redundant after one production cycle
      of coexistence — cleanup planned in the wattyzer repo, not
      here.
    - **fix(tr_treedb): repair `treedb_delete_instance`
      (pkey2-index cleanup only)**. The function was a dead
      copy-paste of `treedb_delete_node` with the real work
      fenced behind `if(0) { ... tranger2_delete_instance(...) }`
      and an `else` returning -1 with *"Cannot delete node"*. The
      dead branch was also wrong — it would have wiped the
      underlying `.md2` row while the primary index and the
      other `pkey2_*` indexes still referenced it (state
      corruption). The only in-tree caller
      (`c_node.c::ac_delete_node`) was getting -1 on every call
      without acting on the return, so the breakage was silent.
      Rewritten to do what the function name promises: drop the
      in-memory entry for THIS pkey2 via `delete_secondary_node`,
      fire `EV_TREEDB_NODE_DELETED`, preserve the JSON_INCREF /
      DECREF pattern. The whole-node wipe stays the job of
      `treedb_delete_node` → `tranger2_delete_key`. Contract
      spelled out in the `.c` and `.h` docstrings.
    - **fix(ytls/openssl): ship the full certificate chain**.
      `build_ssl_ctx()` was loading the server certificate via
      `SSL_CTX_use_certificate_file()`, which only parses the first
      cert of a PEM bundle.  With a Let's Encrypt fullchain.pem on
      disk, that meant the listener served only the leaf — browsers
      hid the issue via AIA-fetch / cached intermediates, but
      strict-TLS clients (e.g. Node's native `fetch`, used by the
      Playwright QA driver against the public URL) failed chain
      verification.  Switched to
      `SSL_CTX_use_certificate_chain_file()` (chain-aware, PEM-only
      — no `SSL_FILETYPE_PEM` arg).  The mbedTLS backend
      (`mbedtls_x509_crt_parse_file`) was always chain-aware so it
      was not affected.  Every yuno that exposes a TLS server with
      the OpenSSL backend needs a relink + redeploy to pick up the
      fix; for binaries shared by several live yunos (e.g.
      `auth_bff` 1802+1804) the atomic `mv old old.bak; cp new old`
      pattern avoids the `ETXTBSY` that breaks `update-binary`.
    - **chore(ytls, c_authz): drop OpenSSL legacy init/cleanup
      calls** (OpenSSL 4.0 prep). Removed four
      deprecated-since-1.1.0 calls that were no-ops on the 3.x
      series and disappear in 4.0: `SSL_library_init()` +
      `OpenSSL_add_all_algorithms()` (with the redundant
      `__initialized__` guard) in `ytls/openssl.c` init,
      `EVP_cleanup()` in cleanup, and
      `OpenSSL_add_all_digests()` (with its
      `CONFIG_HAVE_OPENSSL` wrapper) in `c_authz.c`. OpenSSL
      ≥ 1.1.0 auto-initialises on first use and cleans up via
      `atexit`. The `OPENSSL_API_COMPAT 30100` define already
      gates the rest of the 1.1.x compat surface; the yuneta
      source is now 4.0-clean (the jump itself stays deferred).
    - **fix(c_prot_tcp4h, c_prot_mqtt): guard state reset
      against in-publish disconnect cascade**. Under io_uring,
      publishing `EV_ON_MESSAGE` is synchronous and can trigger
      a full disconnect cascade upstream (authz NAK in
      `C_IEVENT_CLI` → `EV_DROP` → `C_TCP` ac_drop →
      `try_to_stop_yevents` → `set_disconnected` publishes
      `EV_DISCONNECTED` → the protocol gclass moves to
      `ST_DISCONNECTED`). The caller of `frame_completed`
      then unconditionally reset the FSM back to
      `ST_WAIT_FRAME_HEADER` / `ST_CONNECTED`, leaving the
      protocol "connected" without an underlying TCP. Symptom:
      *"Event NOT DEFINED in state: EV_CONNECTED in
      C_PROT_TCP4H@ST_WAIT_FRAME_HEADER"* alternating with
      broken-pipe / local-dropping cycles. Guard
      (`state != ST_DISCONNECTED`) added — same form already
      present in `c_websocket.c` and `c_prot_mqtt2.c`. The
      twin guard initially added to `c_prot_modbus_m` was
      reverted in a follow-up: the modbus-master flow does
      not expose the same cascade (see `GOBJ.md §8.13`).
      Verified live on `app.wattyzer.com` across the
      controlcenter dial-out loop.
    - **fix(gobj): `gobj_read_attrs` honours `mt_reading` via a new
      `item2json` helper**.  The bulk reader (behind `view-attrs`,
      introspection and `db_save_persistent_attrs`) was the only
      attribute path bypassing `mt_reading`, so `SDF_RSTATS` counters
      kept in `priv->X` read as zero through `view-attrs` even though
      `stats-yuno` (typed readers) saw the live value.  The helper
      dispatches by `DTP_*` and falls back to the stored value when
      `mt_reading` is absent or returns `!v.found`.  `gobj_read_attr`
      (single, borrowed-ref) is intentionally left alone: its
      "Return is NOT yours!" contract is incompatible with allocating
      a fresh `json_t` from a typed override.
    - **fix(gobj-js): mirror C — `gobj_read_attrs` honours
      `mt_reading`**.  Same shape as the C kernel patch, simplified
      (dynamic types, no `DTP_*` switch): `undefined` falls back to
      the stored value.  Defensive — no JS gclass implements
      `mt_reading` today, so this only closes the symmetry.
    - **fix(c_tcp): set `v.found = 1` for `cur_tx_queue` in
      `mt_reading`**.  Pre-existing one-liner; the branch updated
      `v.v.i` but forgot the discriminant, so the value was always
      shadowed by the stored zero.  Surfaces now that
      `gobj_read_attrs` consults `mt_reading`.
    - **fix(lib-yui): normalize `navigator.language` before
      `Intl.DateTimeFormat`**. Playwright Firefox without locale
      config (and some embedded webviews) report
      `navigator.language` as the literal string `"undefined"`;
      passing that to `Intl.DateTimeFormat` throws `RangeError`
      and breaks SPA bootstrap. Guard with a string +
      `"undefined"` check, fall back to `undefined` so Intl
      resolves to the system locale. Fold-back of wattyzer
      `1bf08aa`.
    - **feat(yuno_agent): `stats-yuno` defaults `service` to the
      matched `yuno_role`**. `cmd_stats_yuno` previously passed
      an empty `service` when the operator didn't spell it out,
      so the remote fell back to `priv->gobj_service` (the top
      `C_YUNO` instance) and returned only the few attrs declared
      on `c_yuno` — typically all zero, missing the real
      `SDF_RSTATS` counters that live on the citizen service
      (e.g. `C_AUTOMATIONS_WZ`'s `alarms_seen` /
      `tracks_seen` / `fires_seen` / `runs_done`). Convention is
      `gobj_create_default_service(yuno_role, GCLASS, ...)` so
      service name == yuno role; the operator now gets the real
      counters with the natural `stats-yuno yuno_role=X`
      invocation. Pass `service=__yuno__` to explicitly query the
      top `C_YUNO` attrs (previous default).
    - **fix(tr_treedb): include the required-field name in error
      logs**. The three "Field required" `gobj_log_error` sites in
      `check_desc_field` / `normalize_node_field_value` /
      `convert_node2tranger` logged the same opaque message; now
      the field name is interpolated into `msg` so the offending
      column is visible at a glance without expanding the
      structured payload.
    - **chore(yuno_agent): increase agent log file size**. Bumps
      the agent's own log rotation threshold in `yuno_agent` and
      `yuno_agent22` (two-line `main.c` change). Avoids
      tighter-than-needed rollovers under the trace volume the
      onboarding doc work surfaced.
    - **note**: validated by relinking every yuno (15 binaries) and
      running the full `ctest` suite (93/93 passed, 436 s).  A
      project-wide `yunetas build` after a kernel-side change does
      pick up the relink correctly — the `rm <yuno_bin> &&
      make install` workaround is only needed when rebuilding a
      single yuno's `build/` directory in isolation.

## v7.3.4 -- 16/May/2026
    - **chore(release): corrective republish of `@yuneta/lib-yui`**.
      `@yuneta/lib-yui@7.3.3` was published to npm from a branch that
      had the 7.3.3 release prep (gobj-js dependency pin, CHANGELOG,
      version) but **not** the G6 v5 canvas panning fix (PR #115):
      the published 7.3.3 tarball is missing
      `src/g6_drag_canvas_touch.js` and the `autoResize:false` /
      `ensure_drag_canvas_patch` changes, so installing lib-yui from
      npm still had the broken touch/desktop panning. npm versions
      are immutable, so 7.3.4 republishes `@yuneta/lib-yui` from
      `main` with the complete set (#115 + #116). No source changes
      versus what `main` already contained at 7.3.3 — this is a
      packaging correction only.
    - **chore(release): `@yuneta/gobj-js` 7.3.3 → 7.3.4 (lockstep)**.
      `@yuneta/gobj-js@7.3.3` on npm was already correct (it carries
      the `createElement2` nullish-`data-i18n` guard). It is bumped to
      7.3.4 with no functional change purely to keep the two JS
      packages in lockstep and avoid version-skew confusion; the
      `@yuneta/lib-yui` peer range moves to `^7.3.4`.
    - **note**: deprecate the bad artifact —
      `npm deprecate "@yuneta/lib-yui@7.3.3" "incomplete: missing the
      G6 panning fix; use >=7.3.4"`.

## v7.3.3 -- 16/May/2026
    - **fix(lib-yui): G6 v5 canvas panning (touch broken, desktop
      desynced)** (PR #115).  G6 v5.1.0 `drag-canvas` derived the pan
      delta from `event.movement`, which `@antv/g` fills from native
      `PointerEvent.movementX/Y`: left at 0 for touch pointers on most
      mobile browsers (canvas barely panned) and skewed by OS pointer
      acceleration / `devicePixelRatio` on desktop (graph lagged the
      cursor).  New `g6_drag_canvas_touch.js` subclasses `DragCanvas`,
      reuses its clamp/cursor logic, and pans by the `event.viewport`
      delta — reliable on mouse and touch, correct for any canvas
      scale/zoom.  Registered once over the built-in `'drag-canvas'`
      id so every graph (gobj-tree, json-graph, treedb editor) is
      fixed without per-consumer changes.  `c_yui_gobj_tree_js.js`
      and `c_yui_json_graph.js` also stop fighting G6 `autoResize`
      (window-only in v5) and size the canvas to its content box via
      a self-contained `ResizeObserver`.

    - **fix(gobj-js): `createElement2` no longer poisons `data-i18n`
      with `undefined`**.  A nullish `i18n` attribute (e.g. a field
      whose `header` is undefined) rendered the literal
      `data-i18n="undefined"` and suppressed translation, leaving
      form labels blank.  The attribute is now skipped when the value
      is `null`/`undefined`; an explicit empty string is still
      honoured.

    - **fix(lib-yui): pin `@yuneta/gobj-js` dependency**.  The
      peer dependency was `"*"`, and a stale lockfile had frozen it
      to the ancient `@yuneta/gobj-js@0.3.0` from npm — so lib-yui
      (and downstream apps) silently built against 0.3.0 and updates
      had no effect.  Peer range is now `^7.3.3` and a
      `file:../gobj-js` devDependency makes local builds use the
      in-tree source.  Downstream apps (wattyzer, estadodelaire)
      must likewise repin and reinstall so they stop
      resolving 0.3.0.

    - **feat(lib-yui): shell-mountable developer panel**.
      `build_dev_panel` plus a new `C_YUI_GOBJ_TREE_JS` hierarchical
      gobj-tree viewer; the dev panel is now a real window box (was a
      floating transparent overlay), with silent optional
      `__yui_main__` lookup and theme-aware styling.

    - **feat(lib-yui): TreeDB graph node redesign**.  Unified node
      design system: doc-style HTML node cards (theme-aware), soft
      topic palette, rectangular leaves, size tiers, ports back to
      the topic colour, no circles, click-detail popover (no hover
      tooltips), redesigned edges/ports, dark-theme contrast fix.

    - **feat(lib-yui): graph toolbar / context-menu**.  All
      toolbar/context-menu icons unified on one FA7 sprite;
      theme-aware G6 context menu (readable in dark); G6 popup
      transitions disabled (immediate, no glide); "reset zoom" home
      icon restored; bolder/longer "create node" plus.

    - **feat(lib-yui): shell / nav**.  View-owned dynamic 3rd-level
      runtime subroute; unknown route falls back to the default
      route; secondary-nav zone collapses from config; single-row
      toolbar on touch; `toolbar type:"connection"` + `context_action`
      + modal `on_close`; TreeDB topics persist the selected topic
      across reloads with self-contained tab navigation; TreeDB
      table row-count footer and email/tel/url subtypes; edit/delete
      modal mount fallback.

    - **fix(lib-yui): self-containment / responsiveness**.
      `C_G6_NODES_TREE` self-contained `ResizeObserver` and toolbar
      reconfig guarded until the graph is rendered;
      `C_YUI_TREEDB_GRAPH` emits `EV_OPERATION_MODE_CHANGED`; g6
      views detect theme from `<html data-theme>`; `C_YUI_UPLOT`
      responsive width.

    - **feat(treedb): system schema v5 → v6**.  Restore the
      `cols.topics` fkey, refresh `cols` topic, show all system
      topics; keep the original ArtGins start year as a range
      (2024-2026).

    - **feat(ycommand): `--editor`/`-e` and stdout dump**.  Dump a
      file to stdout when stdout is not a TTY (was: always vim);
      `ac_read_file` signals exit on success, not just on error;
      `pty_sync_spawn` drains the master pty after child exit
      (was: truncated `cat`).

    - **feat(c_mqiogate): `broadcast` method** to fan out events to
      every child (tidy `lastdigits` to mirror it).

    - **chore(packages / ci)**.  Ship sanitized agent JSON templates
      from the repo (drop the `/yuneta/agent/` dependency); move
      `RELEASE` to the repo root; `release-deb` generates a default
      `.config` via `alldefconfig` and drops the pgdg apt source.

    - **chore(ext-libs): TODO bump nginx 1.28.3 → 1.30.1**
      (CVE-2026-42945) for the next ext-libs refresh.

    - **misc(C)**: remove `sf_deleted_record` flag; fix stale
      `md2_record_t` "Size: 96 bytes" comment; log the key with bad
      metadata; revert the `C_TCP_S channel_filter` two-TLS-listener
      change.

    - **feat(tr2list): `--dry-run` and `--follow` modes**.
      `--dry-run` / `-n` prints the resolved search parameters and
      the `match_cond` JSON (times already resolved by `approxidate`),
      plus a human-readable rendering of any `from-t`/`to-t`/`from-tm`/
      `to-tm` set — respects `--print-local-time` and flags millisecond
      input.  `--follow` / `-F` opens an `rt_disk` list and runs
      `yev_loop_run` until SIGINT (tail-f style; single topic, so it
      errors out when combined with `--recursive`).  `--help` now
      documents the full `approxidate` grammar accepted by TIME
      options (units, specials, absolute forms) via argp's `\v`
      separator.
    - **feat(treedb_list): `--dry-run` and `--follow` modes**.
      `--dry-run` / `-n` runs `resolve_treedb_path` and prints the
      deduced path / database / topic alongside the filter and
      options JSON (also flags resolution failure and falls back to
      the raw user input).  `--follow` / `-F` keeps listening for
      node CREATED / UPDATED / DELETED / LINKED / UNLINKED events
      after the initial listing — uses the existing rt_disk path
      that treedb already opens internally when `master=false`, plus
      a `treedb_set_callback` that honours `--topic` and `--ids`.
      Errors out on `--follow --recursive` and on `--follow` with
      `--print-tranger` / `--print-treedb`.
    - **fix(helpers/approxidate): accept short unit suffixes**.
      `1s`, `1m`, `1h`, `1d`, `1w`, `1M`, `1y` (plus `1sec`, `1mi`,
      `1min`, `1mo`, `1hr`, `1wk`, `1yr`) now resolve to the
      expected relative duration instead of silently falling through
      to the numeric date parser as day-of-month / year.  Lowercase
      `m` keeps minute, uppercase `M` is month (case-sensitive to
      disambiguate, mirroring `sleep` / `find -mmin` conventions).
      `mon` is intentionally left as Monday so weekday parsing
      keeps its existing behaviour.  Benefits every yuneta tool
      that consumes `approxidate` (tr2list, treedb_list, ybatch,
      tr2search, tr2keys, tr2migrate, ...).
    - **refactor(tr2list): simpler `--dry-run` time block**.
      Each time line now ends with `(<show_date_relative>)` —
      `2 hours ago`, `3 days ago`, etc. — instead of echoing the
      raw input and warning about parser footguns.  The warning
      block in `--help` is dropped and replaced by a `short`
      group listing the new 1-3 char unit forms accepted by
      `approxidate`.

## v7.3.2 -- 09/May/2026
    - **feat(release): publish runtime `.deb` on GitHub Releases +
      one-liner `install.sh`**.  First CI workflow in the repo
      (`.github/workflows/release-deb.yml`) builds the AMD64 `.deb`
      on `release.published` (or `workflow_dispatch` against an
      existing tag) via `packages/AMD64.sh` and uploads it as a
      release asset.  Pairs with a new `install.sh` at the repo
      root: a POSIX one-shot installer that detects host arch
      (`amd64` / `armhf` / `riscv64`), queries the GitHub Releases
      API for the latest (or pinned) tag, downloads the matching
      `yuneta-agent-*-<arch>.deb`, and installs it via
      `dpkg + apt-get -f`:

          curl -fsSL https://raw.githubusercontent.com/artgins/yunetas/main/install.sh | sudo sh

      Pin a version with `sudo sh -s -- 7.3.2`.  ARMhf / ARM32 /
      RISCV64 wait for cross-compile or matching runners.  Past
      releases (7.2.0 .. 7.3.1) have no `.deb` assets — the
      workflow operates forward.

    - **refactor(packages): extract `RELEASE` to a shared
      `packages/RELEASE` file** (reset to `1`).  The four arch
      wrappers had `RELEASE` hardcoded with divergent counters
      (3× "9", 1× "6"); now all four read from one file the same
      way they read `YUNETA_VERSION`.  Yunetas isn't widely
      distributed yet, so the renumbering is harmless.

    - **docs(installation): rewrite as 7-step "guía burros" path**.
      `installation.md` restructured: prerequisites + 7 numbered
      steps from "create the `yuneta` user" through "build and
      test", with verbose detail (apt explanations, miniconda
      bootstrap, full `menuconfig` options) tucked into dropdowns.
      Adds a top-of-page **Quick install** section with the
      `install.sh` one-liner and clarifies that the PyPI `yunetas`
      package (0.x) is the management CLI, **not** the framework
      runtime (7.x).  Step 5 documents the env vars `yunetas-env.sh`
      exports — `YUNETAS_BASE`, `YUNETAS_OUTPUTS`, `YUNETAS_YUNOS`
      — plus the `PATH` prepends and the layout contract
      (`outputs/` and project repos as siblings of the `yunetas`
      repo).  Adds an explicit "re-source per shell" warning, a
      silent footgun in cron / SSH / CI sessions where
      `ybatch` / `ycommand` vanish from `PATH`.

    - **fix(gobj-js): `DTP_STRING` attr coerces null / undefined to
      `""`**.  `json2item` used `JSON.stringify()` as the catch-all
      coercion for non-string values; for `null` that produced the
      literal 4-char string `"null"`, which leaked into IEvent
      payloads.  Specifically: `c_ievent_cli`'s `IDENTITY_CARD`
      sent `"jwt": "null"` to the backend, defeating the
      `empty_string()` check in `c_ievent_srv` that drives the BFF
      httpOnly-cookie auth path; `verify_token` then tried to
      validate the literal `"null"` as a JWT and failed with
      "No OAuth2 Issuer found".  Treat `null` and `undefined` as
      `""` in `DTP_STRING` to bring JS in line with the C runtime
      (where `DTP_STRING` cannot hold a `NULL` pointer).

    - **refactor(C kernel): log hygiene for monitor stats**.  Several
      warning counters in the global-warnings dashboard were noisier
      than they needed to be:
        * `c_auth_bff`: 4xx HTTP responses logged as warning instead
          of info (5xx still error).  Sudden 4xx bursts now show in
          dashboards that filter on warning severity.
        * `c_prot_mqtt`, `c_prot_mqtt2`: malformed CONNECT frames
          (client-side protocol issues) downgraded from error to
          warning; rejection messages tightened so v1 and v2 paths
          bucket into the same precise counter.  Dump the offending
          gbuf when `handle__connect` returns < 0 so the bad CONNECT
          can be inspected in the trace.
        * `c_authz`, `c_ievent_srv`: dropped the duplicate
          "Authentication rejected" warning in `c_ievent_srv` (each
          `result < 0` path in `c_authz::mt_authenticate` already
          logs its own); audited `mt_authenticate` so every
          `result < 0` contributes to the per-msg stats counter;
          fixed a `peername`-empty branch whose `msg` field was
          leaking into the unrelated `dst_service`-not-found stats
          bucket.
        * `ydaemon`: translated the lone Spanish `msg` field
          ("Soy el Matador" → "I am the killer") so monitors and
          search tools group cleanly under the English-only
          convention.

    - **feat(lib-yui): toolbar brand / avatar / dropdown item types
      with per-item `show_on`**.  Three new toolbar item kinds
      validated by `shell_toolbar_helpers.js` and rendered by
      `c_yui_shell.js`: `type:"brand"` (logo image + wordmark, with
      optional action — passive `<div>` if action is omitted),
      `type:"avatar"` (circular initials rendered from a
      host-registered provider via the new
      `yui_shell_set_avatar_provider` /
      `yui_shell_refresh_avatars` helpers), and
      `action.type:"dropdown"` (panel mounted on the popup layer
      with `divider` entries, focus-trap, escape-stack push/pop and
      capture-phase click-outside dismissal — closes on `scroll`
      and `resize` to match native `<select>` UX).  `show_on` now
      applies per item, not just per area.  CSS for all three
      shipped, SHELL.md §3.4 cheatsheet rewritten and §10
      "Implemented" updated.  23 new unit tests for the validators,
      27 chromium e2e specs still pass.

    - **build(linux-ext-libs): nginx / openresty link against system
      libs; ncurses switched to widec for UTF-8**.  The vendored
      OpenSSL / PCRE2 stay only for yuneta's own static binaries
      (`ytls`, `yev_loop`); nginx and openresty now embed
      `libssl` / `libcrypto` / `libpcre` / `libz` from the host,
      same as the distro-packaged nginx — closes a latent
      Makefile-clobbering bug in `re-install-libs.sh`.  Ncurses
      re-enabled `--enable-widec` (v1.11) so `ycli` and `mqtt_tui`
      render UTF-8 emoji / accents instead of `M-x` escape
      sequences; consumers migrated to `<ncursesw/...>` and call
      `setlocale(LC_ALL, "")` before `initscr()`.  Also:
      `MAKEFLAGS=-j$(nproc)` for parallel builds, mbedtls Debug →
      Release, and explicit Release+static+PIC flags across mbedtls
      / jansson / pcre2 / libbacktrace / argp-standalone.

    - **fix(c_auth_bff): shrink `legacy_base` buffer to silence
      `-Wformat-truncation`**.  PATH_MAX-sized `legacy_base` plus
      `/token` or `/logout` suffix into a PATH_MAX destination
      tripped GCC's truncation analysis.  A legacy Keycloak base
      URL is realistically well under 1 KB.

    - **fix(lib-yui): TomSelect re-initialisation guards**.  The
      "Tom Select already initialized on this element" exception
      could be thrown when `build_topic_modal` ran twice — the
      query for `.select2-multiple` was matching inputs in earlier
      modals still attached to the popup-layer.  Scope the query
      to the freshly built `$element`; also add a defensive skip
      when the element already has a `tomselect` instance.

    - **feat(gobj-c, gobj-js): EV_ON_OPEN_ERROR — close before open**.
      When a connection-oriented gobj closes before ever opening (TCP
      connect failed, TLS cert refused, non-101 handshake response,
      handshake timeout, firewall) it now publishes a separate
      `EV_ON_OPEN_ERROR` instead of `EV_ON_CLOSE`, preserving the
      EV_ON_OPEN→EV_ON_CLOSE FSM contract for subscribers that only
      handle close in their connected state.  Declared as a kernel
      event in `g_ev_kernel.{h,c}` and wired in:
        * `kernel/c/root-linux/src/c_ievent_cli.c` (IEvent client)
        * `kernel/c/root-linux/src/c_websocket.c` (low-level WS)
        * `kernel/js/gobj-js/src/c_ievent_cli.js` (browser client)
      Mirrors the browser WebSocket split (.onopen/.onclose/.onerror).
      Flagged with `EVF_NO_WARN_SUBS` so backend FSMs that ignore it
      don't trip the no-subscribers warning; interactive frontends
      opt in.  Retry policy unchanged: the connection-responsible
      gobj keeps reconnecting forever while running — only the parent
      (by stopping the gobj) decides to give up.  Each emission also
      writes a `log_warning` (`MSGSET_CONNECT_DISCONNECT` in C)
      including the remote yuno identity / url / peername — gives
      logcenter and other monitors a precise per-attempt alert that
      a silent retry loop is in progress.

    - **fix(lib-yui): bare-route redirect skips decorative items**.
      `navigate_to()` was using `submenu.items[0].route` as the
      fallback for a level-1 container — undefined when item 0 is a
      `type:"header"` / `type:"divider"`, which caused the bare route
      to fall through to "no target".  Use the first item with a
      `route` instead; `submenu.default` still wins.  SHELL.md §3
      updated.

    - **feat(lib-yui): item tooltips**.  Nav and toolbar items accept a
      `tooltip` field (fallback: `aria_label`); rendered as the HTML
      `title` attribute on the generated `<a>`/`<button>`.

    - **feat(yuno-skeleton): `js_gui` template**.  New skeleton type for
      JS GUI yunos — Vite + lib-yui declarative shell with locales/
      (en+es), public/ web assets, 5 placeholder primary areas, and a
      burger drawer hosting Account + Help.  Registered in
      `__skeletons__.json` (type: Yuno; vars: version, description,
      author, author_email, license_name).

    - **feat(gobj-js, lib-yui): translatable tooltips**.  Nav and toolbar
      items rendered by lib-yui now also emit `data-i18n-title="<key>"`
      next to their `title` attribute, and `refresh_language()` in
      gobj-js gained a second pass that walks `[data-i18n-title]` and
      re-translates the `title`.  Hover tooltips swap language alongside
      the visible labels.

    - **feat(gobj-js, lib-yui): translatable aria-labels**.  Nav and
      toolbar renderers now also emit `data-i18n-aria-label="<key>"`
      next to their `aria-label` attribute (toolbar root, action items,
      brand, avatar, dropdown panel, dropdown rows, and nav items), and
      `refresh_language()` walks `[data-i18n-aria-label]` to rewrite
      `aria-label`.  Screen-reader names now follow the active locale.

    - **fix(lib-yui): toolbar dropdown anchor drift on scroll/resize**.
      The `position:fixed` panel coordinates are frozen at open time
      from `getBoundingClientRect()`; any layout shift previously left
      the panel detached from its trigger.  Match native `<select>` UX
      and dismiss the dropdown on scroll (capture, passive — catches
      every ancestor scroller) and on window resize.

    - **refactor(gui_treedb): apply locale convention**.  Trimmed
      en.js/es.js to the 19 keys actually called from src/ (auth_bff
      protocol IDs + the half-dozen `t(...)` calls in
      `c_yuneta_gui.js`); deleted ~140 aspirational entries that had
      no caller.  Renamed `remote-service` → `remote service`,
      `connection-backend-refused` → `connection to backend refused`
      (rule: spaces, not kebab); fixed top-level `nombre:` → `name:`
      to match the rest of the codebase.  Added
      `keySeparator: false` + `nsSeparator: false` in
      `setup_locale()` so a future dotted key (e.g. device-namespace)
      doesn't fall silently to nested-lookup.  Same
      `scripts/validate-locales.mjs` + `prebuild` wiring as
      wattyzer.  Auth_bff snake_case codes kept as-is (wire
      contract, see `c_auth_bff.c`).

    - **feat(yuno-skeleton): locale convention + validator**.  The
      `js_gui` template now ships `scripts/validate-locales.mjs`
      (asserts every i18n key is ASCII + lower-case + present in every
      locale) wired as `npm run validate-locales` and `prebuild`.
      en.js/es.js header banners spell out the convention so new
      yunos inherit it from day one.

## v7.3.1 -- 30/Apr/2026
    - **breaking(auth): standard OIDC migration of `c_auth_bff` and
      `c_task_authenticate`**.  Both gclasses now resolve IdP endpoints
      in the same priority order:

        1. Explicit `token_endpoint` + `end_session_endpoint` attrs
           (full URLs, skips discovery — one fewer round-trip).
        2. `issuer` attr — task chain prepends a GET of
           `<issuer>/.well-known/openid-configuration` and caches the
           resolved endpoints in priv before the auth flow runs.
        3. Refuse to start.

      Any conformant OIDC IdP works (Keycloak, Auth0, Cognito, Azure AD,
      Authentik, ...).  Hardcoded Keycloak path scheme removed.

      - **`c_task_authenticate` and its 6 callers** (`c_cli`, `c_mqtt_tui`,
        `c_ycommand`, `c_ystats`, `c_ytests`, `c_ybatch`) had their
        legacy `auth_url`+`auth_system` attrs **removed outright** and
        the `azp` attr **renamed to `client_id`** to match the form
        parameter actually sent on `/token` and `/logout`.
      - **CLI flag set** in `ycommand` / `ystats` / `ytests` / `ybatch` /
        `mqtt_tui` is now `-I/--issuer`, `-T/--token-endpoint`,
        `-E/--end-session-endpoint`, `-Z/--client-id`.  Old `-K/--auth_system`,
        `-k/--auth_url` and `-Z/--azp` (renamed) are gone.
      - **`c_auth_bff` keeps `idp_url`+`realm`** as a deprecated path
        (warning fired at `mt_create`); removal scheduled once one
        release has shipped with the warning in place.  See
        [`TODO.md`](TODO.md) for the remaining smoke tests against
        non-Keycloak IdPs and the open ROPC-vs-PKCE question.

    - **feat(gobj, gobj-js): `SDF_DEPRECATED` attribute flag**.  New
      sdata flag (`0x00000100`) to mark a gclass attribute as deprecated.
      Both the C runtime and the JS runtime emit a warning when a
      deprecated attribute is set during gobj creation, naming the
      gclass and the attr.  First adopter: `c_authz::authz_yuno_role`
      (use `authz_service` instead).

    - **test(c_task_authenticate)**: new self-contained suite under
      `tests/c/c_task_authenticate/` (`test1_discovery`,
      `test2_explicit_endpoints`, `test4_discovery_failure`).  Mock IdP
      gclass with `override_*_body` knobs for failure injection;
      shared `test_main.c` boilerplate; the driver subscribes to
      `EV_ON_TOKEN`, asserts the result code, and dies.

    - **test(c_auth_bff)**: new `test17_legacy_idp_url` covers the
      `idp_url`+`realm` deprecation path that tests 1–16 missed.
      Captures the deprecation warning at `LOG_OPT_UP_WARNING` and
      drives the full login flow against the same mock-Keycloak.

    - **feat(lib-yui): declarative app shell `C_YUI_SHELL` + `C_YUI_NAV`**.
      A JSON-driven replacement for `C_YUI_MAIN` + `C_YUI_ROUTING`, shipped
      alongside the legacy stack (no migration planned — see
      [`SHELL.md` §10](kernel/js/lib-yui/SHELL.md)).  New GUIs can adopt
      the new shell; existing GUIs keep using the old one unchanged.
      - **Layered grid**: 6 z-stacked layers (`base`, `overlay`, `popup`,
        `modal`, `notification`, `loading`) and 7 zones (`top`, `top-sub`,
        `left`, `center`, `right`, `bottom-sub`, `bottom`) inside `base`,
        all driven by a single declarative JSON config.
      - **Six menu layouts**: `vertical`, `icon-bar`, `tabs`, `drawer`,
        `submenu`, `accordion`.  Same menu may render differently per
        zone via `render[zone]`.  Auto-expand of the active branch on
        accordion when the route changes.
      - **`show_on` parser**: zone visibility per Bulma breakpoint with
        the operators `>=`, `<=`, `<`, `>`, enumeration and `|`.  Pure
        module (`shell_show_on.js`), 13 `node --test` unit tests.
      - **Three lifecycle modes per item** (`eager` / `keep_alive` /
        `lazy_destroy`) decide when the routed view is created and
        destroyed.
      - **Single router**: `C_YUI_NAV` publishes `EV_NAV_CLICKED`; the
        shell publishes `EV_ROUTE_REQUESTED` (intent, audit witness)
        and `EV_ROUTE_CHANGED` (fact).  Hash-based 2-level routing,
        no dependency on `C_YUI_ROUTING`.
      - **Drawer overlay** on the `overlay` layer with focus-trap
        (Tab/Shift+Tab cycling, focus restoration on close), backdrop
        click closes via `EV_DRAWER_CLOSE_REQUESTED` (canonical close
        path with focus-trap release + escape-stack pop).
      - **Escape priority chain**: `priv.escape_stack` is a LIFO of
        `{layer, handler}`; the global `keydown` listener calls only
        the top entry.  Modal-over-drawer closes the modal first.
        Public API `yui_shell_push_escape` / `yui_shell_pop_escape`
        for app-level overlays.
      - **Modal / notification API** on top of the shell layers
        (`yui_shell_show_info` / `show_warning` / `show_error` /
        `show_modal` for non-blocking; `yui_shell_confirm_ok` /
        `confirm_yesno` / `confirm_yesnocancel` for blocking dialogs
        that resolve a Promise).  Each modal/dialog auto-pushes onto
        the Escape stack and installs a focus-trap.  Bulma `.modal-card`
        / `.notification` markup verbatim.  Generic focus-trap moved
        to `shell_focus_trap.js` with 10 unit tests.
      - **Canonical i18n via `data-i18n` + `refresh_language`**: every
        translatable text node carries `data-i18n="<canonical key>"`;
        apps switch language by calling
        `refresh_language(shell.$container, t)` from `@yuneta/gobj-js`,
        the same flow `c_yui_main.js` uses in `change_language()`.
        Modals/dialogs accept `opts.t` so they render in the active
        language at open time AND retranslate live afterwards.
      - **Generalised secondary-nav loop**: `instantiate_menus()` walks
        every menu mounted via a `"menu.<id>"` host whose items declare
        a `submenu` (not just `menu.primary`).  Synthesised menu_id is
        `secondary.<owning_menu_id>.<item.id>`, scoped so two
        primary-style menus can share item ids without colliding.
      - **`gcflag_no_check_output_events`** on the shell so the toolbar
        can publish arbitrary user-defined events
        (`action.type:"event"`) without each app having to extend the
        shell's `event_types` table.
      - **Hard contracts**: every view gclass MUST expose `$container`
        in `mt_create`; every navigation through an empty/unknown route
        logs `log_error` and surfaces a placeholder banner; every
        try/catch logs via `log_warning` (no silent swallow).
      - **`validate_config()`**: system-boundary guard run at the top
        of `mt_start`.  Rejects malformed configs with a visible
        "invalid config" banner instead of producing a half-built
        shell.  Checks: object/array shapes, zone-id membership in the
        7 valid zones, `host` syntax (`toolbar` | `menu.<id>` |
        `stage.<id>`), stage zones declared in `shell.zones`, and
        cross-menu route-target uniqueness (warn when two menus claim
        the same target).
      - **Playwright e2e harness**: 22 spec files × 3 browsers
        (chromium + firefox + webkit) = 69 tests covering boot /
        navigation / drawer / modals / multimenu / validator /
        lifecycle / breakpoint / live-i18n.  CI workflow
        `.github/workflows/lib-yui.yml` runs unit + e2e on PRs and
        pushes touching `kernel/js/lib-yui/**` or
        `kernel/js/gobj-js/**`.  `kernel/js/lib-yui/install-e2e-deps.sh`
        helper installs the apt packages WebKit links against
        (`libgstreamer-plugins-bad1.0-0`, `libavif16`).
      - **Test-app**: standalone harness in `kernel/js/lib-yui/test-app/`
        with three presets (`default`, `?preset=accordion`,
        `?preset=multimenu`) plus a deliberately-broken `?preset=invalid`
        used by the validator regression test.  `C_TEST_LANG`
        controller demonstrates the canonical pattern for reacting to
        custom toolbar events (language toggle, hello toast, ask
        dialog).
      - **Docs**: [`SHELL.md`](kernel/js/lib-yui/SHELL.md) (design,
        configuration JSON, GClasses + events, modal/notification API,
        Escape chain, internationalisation),
        [`TODO.md`](kernel/js/lib-yui/TODO.md) (status of every task on
        the new shell), updated `lib-yui/README.md` with the
        "Which app shell to use?" decision tree.
      - **CLAUDE.md**: new "GClass section layout" addendum (JS skeleton
        banners + canonical CHILD/SERVICE subscription model + Always
        braces rule + EVF_NO_WARN_SUBS) so future agents stay on the
        rails the user established for this work.

## v7.3.0 -- 18/Apr/2026
    - **feat(ytls, c_yuno, c_agent): TLS certificate hot-reload with
      three-layer defence-in-depth**. Lets a Yuneta host keep thousands of
      persistent TLS connections alive across a Let's Encrypt renewal,
      with no deploy-hook single point of failure.
      - **ytls**: new [`ytls_reload_certificates()`](docs/doc.yuneta.io/api/ytls/ytls.md)
        that rebuilds the backend context (OpenSSL `SSL_CTX` or mbed-TLS
        `mbedtls_state_t` bundle), validates it, and atomically swaps it
        in. Live sessions hold their own refcount on the previous context,
        so already-established connections keep working until they close.
        Invalid material rolls back cleanly — traffic is never interrupted
        by a bad reload. `ytls_get_cert_info()` returns
        `{subject, issuer, not_before, not_after, serial, days_remaining}`
        for the live context, not just the file on disk.
      - **c_agent**: new cert auto-sync timer (attr
        `cert_sync_interval_sec`, default 900 s) that re-reads
        `/yuneta/store/certs/` via `sudo -n copy-certs.sh`; when any
        `size+mtime` changes, broadcasts `reload-certs` to every running
        yuno. Exposes `cert-sync-now` / `cert-sync-status` commands and
        self-heals if the certbot deploy hook fails silently.
      - **c_yuno**: periodic expiry monitor (attr `timeout_cert_check`,
        default 3600 s) that walks every `C_TCP_S` / `C_UDP_S` listener
        and logs `gobj_log_warning()` at `cert_warn_days` (default 7) and
        `gobj_log_critical()` at `cert_critical_days` (default 2).
        Alert-only — the sync layer owns the reload responsibility.
      - **c_tcp_s / c_udp_s**: per-listener `reload-certs` and `view-cert`
        commands, routable via `ycommand -c 'command-yuno command=reload-certs
        service=__yuno__'` or `gobj=<name>` for a single listener.
      - **packages**: `/etc/letsencrypt/renewal-hooks/deploy/reload-certs`
        hook copies certs, reloads the web server and broadcasts the
        yuno-level reload. Each step runs with `set +e`; output is logged
        to `/var/log/yuneta/deploy-hook.log` and the hook writes its last
        run timestamp to `/var/lib/yuneta/last-deploy-hook-run` so
        `cert-sync-status` can spot a hook that never runs.
      - **tests**: `tests/c/ytls/test_cert_reload`,
        `test_cert_info`, `test_cert_reload_mem` (1000 reloads, zero leak)
        and `tests/c/yev_loop/yev_events_tls/test_yevent_reload_live`,
        `test_yevent_reload_stress` (50 reloads with a live session).
      - **docs**: new guide [`guide/guide_cert_management.md`](docs/doc.yuneta.io/guide/guide_cert_management.md)
        covers the end-to-end story, layered design and file / permission
        layout; `guide/guide_ytls.md` gains a hot-reload section.

    - **feat(gobj): `gobj_set_manual_start()` + `gobj_flag_manual_start`**.
      A gobj can now opt out of the automatic `start-tree` walk so its
      parent keeps ownership of lifecycle but decides *when* to bring it
      up. Used in `c_auth_bff` to keep `gobj_idprovider` dormant until the
      BFF has validated its configuration.

    - **feat(ycommand)**: major interactive / scripting overhaul.
      - TAB completion of command names, parameter names and boolean values,
        from a remote `list-gobj-commands` cache fetched at connect time
        (routed through `service=__yuno__`) and from a local command table
        for `!cmd` built-ins.
      - Inline parameter hints in gray (`<name=type>` required,
        `[name=type]` optional, already-typed params dropped).
      - Connect-time informative prompt (`<role>^<name>> `) and schema-driven
        table rendering in both interactive and non-interactive modes (use
        the `*cmd` prefix to force raw-JSON form).
      - `Ctrl+R` / `Ctrl+S` incremental history search, `Ctrl+L` clear screen,
        bash-style `!!` / `!N` history expansion, erasedups history.
      - c_cli-style local commands via the `!` prefix: `!help` (alias `!h` /
        `!?`), `!history`, `!clear-history`, `!exit` / `!quit`,
        `!source <file>` (alias `!.`). Full keybinding + syntax reference
        available as `!help` and in `utils/c/ycommand/README.md`.
      - Command chaining with `cmd1 ; cmd2 ; cmd3` (quote/brace-aware split),
        `-cmd` ignore-fail (ybatch convention), stdin piping
        (`cat batch.ycmd | ycommand -u ws://...`). A single shared
        command queue drains one command at a time, waiting for the previous
        response before sending the next.
      - `did-you-mean` suggestions on `command not available` errors,
        Levenshtein-matched against the cache.
      - Positional command form (`ycommand kill-yuno id=foo`, equivalent to
        `-c`). The `-c` flag still wins when both are present.
    - **feat(c_editline)**: new public helpers shared by every editline
      client — `editline_set_completion_callback` /
      `editline_set_hints_callback` / `editline_add_completion` /
      `editline_history_count` / `editline_history_get`. New events
      `EV_EDITLINE_REVERSE_SEARCH` / `EV_EDITLINE_FORWARD_SEARCH` for
      incremental history search; candidate list + description is rendered
      on TAB when multiple options exist.
    - **fix(c_editline)**: after the user selects a TAB candidate, the
      keystroke that committed the selection (Enter, Backspace, printable)
      is now re-dispatched so the action takes effect in the same press
      instead of requiring a second press.
    - **fix(ycommand)**: `on_read_cb` no longer drops trailing bytes of a
      batched read that matched a keytable entry, so rapid TAB+value typing
      no longer needs a second press.
    - **feat(ycli)**: TAB completion brought in line with ycommand, adapted
      to the multi-window ncurses UI.
      - `!cmd<TAB>` completes local `c_cli` commands; `cmd<TAB>` (no `!`)
        completes remote commands of the yuno attached to the focused
        display window. Cache is per-connection, fetched silently on
        `EV_ON_OPEN` via `list-gobj-commands` and dropped on
        `EV_ON_CLOSE`.
      - Multi-candidate list is rendered in a temporary ncurses popup
        above the editline (no more blocking `read(STDIN_FILENO)` inside
        the yev_loop callback); cycling is driven through the normal FSM
        (TAB / Up / Down navigate, Enter commits to the edit line only,
        Esc / Ctrl+G / Backspace cancel, printable keys commit + insert).
      - Scrollable popup with a status row (`N/M  ↑ K above  ↓ L below`)
        rendered in dim attributes so A_REVERSE on the selected row can
        never bleed into it.
      - Inline hints (`<req=type>` / `[opt=type]`) in gray (A_BOLD on
        COLOR_BLACK = bright-black / gray in most terminals).
    - **feat(c_editline)**: new `EV_EDITLINE_CANCEL` event for escape-style
      cancellation of reverse-i-search and TAB-popup sub-modes; `refreshSearchLine`
      now draws through ncurses (`wmove/waddnstr/wrefresh`) on `use_ncurses`
      clients instead of bypassing the pane via `printf`.
    - **feat(ycli / ycommand)**: `Ctrl+K` switched to readline semantics —
      delete from cursor to end of line (`EV_EDITLINE_DEL_EOL`).
      `Ctrl+U` / `Ctrl+Y` remain "delete whole line"; `Ctrl+L` is the
      clear-screen shortcut (previously shared with `Ctrl+K`).
    - **docs**: added `utils/c/ycommand/README.md`, `TODO.md` and updated
      `docs/doc.yuneta.io/{utilities,yunos,modules}.md` to cover the new
      features.

    - **API change(ghttp_parser)**: `ghttp_parser_reset()` is **removed** from
      the public API.  It was a foot-gun: calling it from inside an llhttp
      callback (as `on_message_complete` used to do) corrupted llhttp's state
      machine and silently swallowed pipelined messages.  Callers that need a
      pristine parser for a new connection now use the destroy+create cycle
      (see `c_prot_http_sr::ac_connected`, `c_prot_http_cl::ac_connected`,
      `c_websocket::ac_connected`).  The llhttp settings vtable is now
      initialised once, lazily, via `llhttp_settings_init()` in
      `ensure_settings_initialized()`.
    - **feat(ghttp_parser)**: new `ghttp_parser_finish()` that signals
      end-of-stream (`llhttp_finish()`) to the parser.  Fixes a latent bug
      where HTTP/1.0 responses (or HTTP/1.1 `Connection: close` responses
      without `Content-Length` / `Transfer-Encoding: chunked`) never fired
      `on_message_complete` because the peer's socket close was the only
      message terminator.  Wired up in `c_prot_http_cl::ac_disconnected`
      (the critical case for response parsers), `c_prot_http_sr::ac_disconnected`,
      and `c_websocket::ac_disconnected`.
    - **fix(ghttp_parser)**: on `HPE_PAUSED_UPGRADE`, `ghttp_parser_received()`
      now returns the actual number of bytes llhttp consumed (computed via
      `llhttp_get_error_pos()`) instead of lying that it consumed the whole
      buffer.  This lets the caller re-route any tail bytes that belong to
      the new protocol (e.g. a WebSocket frame piggy-backed on the same TCP
      segment as the upgrade request) to the next handler.
    - **CRITICAL fix(ghttp_parser)**: HTTP/1.1 pipelining was silently broken —
      `on_message_complete()` called `ghttp_parser_reset()`, which in turn called
      `llhttp_init()` from inside the llhttp callback, corrupting the parser's
      internal state machine so every subsequent message in the same buffer was
      swallowed without a log.  Affects every yuno serving or consuming HTTP
      over keep-alive when more than one message is in flight on a single
      connection (c_prot_http_sr, c_prot_http_cl, c_websocket).  Fix: reset the
      per-message app fields inline in `on_message_complete` without touching
      llhttp; leave `ghttp_parser_reset()` for the other (non-callback) call
      sites.  Surfaced by the new test suite `tests/c/c_auth_bff/test8_queue_full`.
    - **refactor(c_auth_bff): IdP-agnostic naming, single-job task, queue +
      routing hardening**. The BFF used to be visibly wired to Keycloak
      (`kc_*` attrs, stats, logs). Code, attrs and stats now use the
      generic `idp_*` prefix; any OIDC provider fits. The outbound IdP
      gobj chain is now named `<bff-name>-idp` for trace clarity.
      - **Pending queue** migrated from a fixed-size `PENDING_AUTH *` ring
        to a `dl_list`, drained one job at a time. Configurable per
        instance via `pending_queue_size` (default 16, clamped to
        `[1, 1024]`). Overflow bumps `q_full_drops` and the browser sees
        a mapped `error_code`; peak depth is exposed as `q_max_seen`.
      - **Flush-on-disconnect**: when a browser closes mid-round-trip the
        BFF flushes its pending queue for that channel; late IdP replies
        for disconnected clients are dropped (`responses_dropped` counter)
        instead of being forwarded. Each task also carries a per-browser
        generation so a cross-user token leak cannot occur.
      - **Single-job task, teardown-safe close**: the C_TASK instance
        holds a single job at a time; `mt_stop` drains the inbound
        `C_PROT_HTTP_SR + C_TCP` chain and the outbound `gobj_http` so a
        SIGTERM with live browser connections no longer logs
        "Destroying a RUNNING gobj".
      - **Outbound watchdog**: per-instance attr `idp_timeout_ms`
        (default 30000, 0 disables) armed via a `C_TIMER0` child right
        after the outbound HTTP client is created and cleared in
        `ac_end_task`. On fire, responds 504 to the browser and drains
        the task; closes the "IdP silence → channel wedged forever"
        deadlock. New `idp_timeouts` stat counter.
      - **IdP health signal fix**: count any 2xx IdP reply as `idp_ok`;
        previously only 200 counted, so every successful `/logout`
        (Keycloak returns spec-compliant 204 No Content) poisoned the
        ratio as an `idp_error`.
      - **Logout routing fix**: route the logout reply to the bottom
        browser channel, not to the dangling `_browser_src` from an
        earlier round-trip.
      - **`mt_stats` filter** mirrors the default `stats_parser.c`
        two-stage matcher (full name OR underscore-prefix) and is
        case-insensitive, so `gobj_stats(bff, "idp_", ...)` returns the
        idp_* set as expected. `redact_for_trace()` key matching is also
        case-insensitive so HTTP headers like "Cookie"/"cookie"/"COOKIE"
        are all masked.
      - **Stats moved to PRIVATE_DATA + `mt_stats`** for zero hot-path
        cost; the gclass now also exposes a stats/queue-state command
        through the normal command interface.
      - **Stable `error_code`** in every BFF response (snake_case, e.g.
        `invalid_refresh_token`, `idp_unreachable`, `queue_full`) — the
        GUI uses this as its i18n translation key. Action-aware error
        mapping wired through `gui_treedb`.
      - **Log hygiene**: 4xx IdP replies are logged as `INFO`, not
        `ERROR` (a wrong password is not a server error), with
        `MSGSET_PROTOCOL`. New `messages` / `traffic` trace levels; 👤
        BFF log prefix and ⏩/⏪ direction arrows across BFF traces.
      - **Own orchestrator GClass** at the top of the `auth_bff` yuno
        (replaces the citizen-yuno shortcut) and `gobj_idprovider` is
        tagged `gobj_flag_manual_start` so it stays dormant until the
        BFF validates its configuration.
      - `gobj_http` single-instance invariant is now asserted in debug
        builds to catch re-entrancy regressions.

    - **perf(auth_bff)**: new `perf_auth_bff` ping-pong-style live
      throughput benchmark (`performance/c/perf_auth_bff/`). Default
      10 s run, ~180 000 ops on the reference box; registered as ctest.

    - **test(c_auth_bff)**: 16-binary suite self-contained under
      `tests/c/c_auth_bff/` with a scriptable mock Keycloak
      (`c_mock_keycloak`): signed HS256 JWTs, configurable latency /
      status / body override. Covers login, callback, refresh, logout,
      validation errors, IdP 401, slow IdP, queue pipelining + overflow,
      browser cancel mid-round-trip, cancel-then-retry, cross-user stale
      replies, expired refresh, 405 / missing body / unknown endpoint.
      Gates the watchdog, `browser_alive`, flush-on-disconnect and
      ghttp_parser fixes.

    - **test(c_llhttp_parser)**: sanity suite for the vendored llhttp
      library and the `ghttp_parser` wrapper (`tests/c/c_llhttp_parser/`).

    - **stress(auth_bff)**: new concurrent stress runner
      (`stress/c/auth_bff/`) that exercises the pending queue, the
      watchdog and the flush-on-disconnect path.

    - **fix(c_prot_http_sr)**: omit response body on 1xx / 204 / 304
      replies (RFC 7230). The parser path was emitting a body for these
      status codes, confusing downstream clients and tripping some
      proxies.

    - **fix(c_task)**: `volatil` gobjs now self-destroy at end-of-work —
      making the long-standing `// auto-destroy` comment actually true.
      The outbound HTTP client used by the BFF is created `volatil` so
      teardown is explicit and framework-free (PR #95). Also silences
      the `-Wcomment` warning in the auto-destroy comment and dedups
      `TRACE_MESSAGES` / `TRACE_MESSAGES2` output.

    - **fix(lib-yui)**: restore `publi_page` iframe rendering for
      logged-out users — a regression in the login split hid the public
      landing page behind the auth screen.

    - **fix(ytls/openssl)**: guard `flush_clear_data` against a
      re-entrant `sskt` free under specific TLS teardown paths.

    - **build(libjwt)**: yuno skeleton `CMakeLists.txt` templates now
      link `${JWT_LIBS}` out of the box (PR #92).

    - **refactor(gobj)**: drop TLS knowledge from `gobj-c`, inject it
      from the ytls layer via a new `gobj_add_global_variable()`
      extension point. Removes the `CONFIG_HAVE_OPENSSL/MBEDTLS` `#if`
      blocks from `gobj_global_variables()` and keeps the core
      backend-agnostic — `root-linux`'s `yunetas_register_c_core()`
      publishes `__tls_library__` and `__tls_libraries__` at startup.

## v7.2.1 -- 07/Apr/2026
    - TLS: change Kconfig from radio (choice) to checkboxes — both OpenSSL and mbedTLS can be
      enabled simultaneously for runtime backend selection per connection
    - TLS: add `__tls_libraries__` global variable (reports all compiled backends)
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
