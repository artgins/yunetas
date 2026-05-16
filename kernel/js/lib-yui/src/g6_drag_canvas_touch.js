/***********************************************************************
 *          g6_drag_canvas_touch.js
 *
 *          Drop-in replacement for G6 v5 built-in 'drag-canvas'.
 *
 *          Why this exists
 *          ----------------
 *          G6 v5.1.0 drag-canvas computes the pan delta as:
 *
 *              const x = event.movement?.x ?? event.dx;
 *
 *          but @antv/g always attaches a `movement` Point to every
 *          federated pointer event (g-lite: `this.movement = new
 *          Point()`), and fills it from the native
 *          `PointerEvent.movementX/Y` (g-lite: `event.movement.x =
 *          nativeEvent.movementX`).  So `event.movement?.x` is never
 *          nullish and the `?? event.dx` fallback never runs.
 *
 *          Native `movementX/Y`:
 *            - on touch pointers most mobile browsers leave it at 0
 *              (it is a mouse / pointer-lock feature), so the canvas
 *              barely pans on mobile;
 *            - on desktop it is skewed by OS pointer acceleration and
 *              devicePixelRatio, so the graph lags the cursor.
 *
 *          The fix tracks `event.viewport` (the federated pointer
 *          position already mapped to canvas/viewport space by
 *          @antv/g, the exact units `translateBy` expects) and pans
 *          by its frame-to-frame delta.  This is:
 *            - reliable on mouse AND touch (derived from clientX/Y,
 *              never from native movementX);
 *            - correct for ANY canvas scale (it is post-`getScale()`,
 *              so a configured-vs-displayed size mismatch can no
 *              longer make panning over- or under-shoot the cursor);
 *            - the same source G6's own `zoom-canvas` uses for its
 *              origin (`event.viewport`).
 *          The subclass reuses all of the built-in clamp/cursor logic
 *          and only swaps the delta source.
 *
 *          Registered globally over the built-in 'drag-canvas' id so
 *          every graph (tree, json, treedb, editor) is fixed without
 *          touching each consumer's behaviors list.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import {
    DragCanvas,
    CommonEvent,
    ExtensionCategory,
    register,
} from '@antv/g6';

/************************************************************
 *  Custom drag-canvas: same behavior, reliable delta source
 ************************************************************/
class DragCanvasTouch extends DragCanvas {
    constructor(context, options) {
        super(context, options);

        /*
         *  Last pointer position in viewport space; null between
         *  drags so each gesture re-seeds on its first DRAG frame.
         */
        this._lastVP = null;

        /*
         *  The base constructor already bound its own (buggy)
         *  onDrag.  Replace it with ours; keep base onDragStart /
         *  onDragEnd (cursor + isDragging) untouched.
         */
        this._onDragTouch = (event) => {
            if(!this.isDragging) {
                this._lastVP = null;
                return;
            }
            let vp = event.viewport;
            if(!vp || typeof vp.x !== "number" || typeof vp.y !== "number") {
                return;
            }
            if(this._lastVP === null) {
                this._lastVP = {x: vp.x, y: vp.y};
                return;
            }
            let x = vp.x - this._lastVP.x;
            let y = vp.y - this._lastVP.y;
            this._lastVP.x = vp.x;
            this._lastVP.y = vp.y;
            if(x !== 0 || y !== 0) {
                this.translate([x, y], false);
            }
        };

        this._onDragEndReset = () => {
            this._lastVP = null;
        };

        let graph = this.context.graph;
        graph.off(CommonEvent.DRAG, this.onDrag);
        graph.on(CommonEvent.DRAG, this._onDragTouch);
        graph.on(CommonEvent.DRAG_END, this._onDragEndReset);
    }

    update(options) {
        /*
         *  super.update() runs unbind+bind, which reinstalls the
         *  base onDrag.  Swap it back to ours afterwards.
         */
        super.update(options);

        let graph = this.context.graph;
        graph.off(CommonEvent.DRAG, this.onDrag);
        graph.off(CommonEvent.DRAG, this._onDragTouch);
        graph.off(CommonEvent.DRAG_END, this._onDragEndReset);
        graph.on(CommonEvent.DRAG, this._onDragTouch);
        graph.on(CommonEvent.DRAG_END, this._onDragEndReset);
    }

    destroy() {
        let graph = this.context.graph;
        graph.off(CommonEvent.DRAG, this._onDragTouch);
        graph.off(CommonEvent.DRAG_END, this._onDragEndReset);
        super.destroy();
    }
}

/************************************************************
 *  Register once, replacing the built-in 'drag-canvas'
 ************************************************************/
let __drag_canvas_patched__ = false;

function ensure_drag_canvas_patch()
{
    if(__drag_canvas_patched__) {
        return;
    }
    register(ExtensionCategory.BEHAVIOR, 'drag-canvas', DragCanvasTouch);
    __drag_canvas_patched__ = true;
}

export { ensure_drag_canvas_patch };
