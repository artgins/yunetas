/*--
zim.Window = function(width, height, backgroundColor, borderColor, borderWidth, padding, corner, swipe, scrollBarActive, scrollBarDrag, scrollBarColor, scrollBarAlpha, scrollBarFade, scrollBarH, scrollBarV, slide, slideDamp, slideSnap, interactive, shadowColor, shadowBlur, paddingHorizontal, paddingVertical, scrollWheel, damp, titleBar, titleBarColor, titleBarBackgroundColor, titleBarHeight, draggable, boundary, onTop, close, closeColor, cancelCurrentDrag, fullSize, fullSizeColor, resizeHandle, collapse, collapseColor, collapsed, optimize, style, group, inherit)

Window
zim class - extends a zim.Container which extends a createjs.Container

DESCRIPTION
Adds a window for content that can be swiped and scrolled.
NOTE: if zim namespace zns = true then this overwrites a JS Window - so the JS Window is stored as document.window

NOTE: set the enable property to false if animating the position of the whole Window
then set the enable property to true on the animate call function.  See update() method for more.

NOTE: to add ZIM Swipe() to objects in window set the overrideNoSwipe parameter of Swipe to true

NOTE: as of ZIM 5.5.0 the zim namespace is no longer required (unless zns is set to true before running zim)

EXAMPLE
const w = new Window({scrollBarDrag:true, padding:20}).center();
const t = new Tile(new Circle(20, red), 4, 20, 20, 20);
w.add(t);
// the above would only drag on the circles (or the scrollbars)
// adding a Rectangle to help dragging
w.add(new Rectangle(w.width-20,t.height,dark), 0);
// or could have added it to the bottom of the Tile
// new Rectangle(w.width-20,t.height,dark).addTo(t).bot();
END EXAMPLE

PARAMETERS
** supports DUO - parameters or single object with properties below
** supports OCT - parameter defaults can be set with STYLE control (like CSS)
width - (default 300) the width of the window
height - (default 200) the height of window - including the titleBar if there is a titleBar
backgroundColor - (default #333) background color (use clear - or "rbga(0,0,0,0)" for no background)
borderColor - (default #999) border color
borderWidth - (default 1) the thickness of the border
padding - (default 0) places the content in from edges of border (see paddingHorizontal and paddingVertical)
	padding is ignored if content x and y not 0 - and really only works on top left - so more like an indent
corner - (default 0) is the rounded corner of the window (does not accept corner array - scrollBars are too complicated)
swipe - (default auto/true) the direction for swiping set to none / false for no swiping
	also can set swipe to just vertical or horizontal
scrollBarActive - (default true) shows scrollBar (set to false to not)
scrollBarDrag - (default false) set to true to be able to drag the scrollBar
scrollBarColor - (default borderColor) the color of the scrollBar
scrollBarAlpha - (default .3) the transparency of the scrollBar
scrollBarFade - (default true) fades scrollBar unless being used
scrollBarH - (default true) if scrolling in horizontal is needed then show scrollBar
scrollBarV - (default true) if scrolling in vertical is needed then show scrollBar
slide - (default true) Boolean to throw the content when drag/swipe released
slideDamp - (default .6) amount the slide damps when let go 1 for instant, .01 for long slide, etc.
slideSnap - (default "vertical") "auto" / true, "none" / false, "horizontal"
	slides past bounds and then snaps back to bounds when released
	vertical snaps when dragging up and down but not if dragging horizontal
interactive - (default true) allows interaction with content in window
	set to false and whole window will be swipeable but not interactive inside
shadowColor - (default rgba(0,0,0,.3)) the color of the shadow
shadowBlur - (default 20) set shadowBlur to -1 for no drop shadow
paddingHorizontal - (default padding) places content in from left and right (ignored if content x not 0)
paddingVertical - (default padding) places content in from top and bottom (ignored if content y not 0)
scrollWheel - (default true) scroll vertically with scrollWheel
damp - (default null) set to .1 for instance to damp the scrolling
titleBar - (default null - no titleBar) a String or ZIM Label title for the window that will be presented on a titleBar across the top
titleBarColor - (default "black") the text color of the titleBar if a titleBar is requested
titleBarBackgroundColor - (default "rgba(0,0,0,.2)") the background color of the titleBar if a titleBar is requested
titleBarHeight - (default fit label) the height of the titleBar if a titleBar is requested
draggable - (default true if titleBar) set to false to not allow dragging titleBar to drag window
boundary - (default null) set to ZIM Boundary() object - or CreateJS.rectangle()
onTop - (default true) set to false to not bring Window to top of container when dragging
close - (default false) - a close X for the top right corner that closes the window when pressed
closeColor - (default #555) - the color of the close X if close is requested
cancelCurrentDrag - (default false) - set to true to cancel window dragging when document window loses focus
	this functionality seems to work except if ZIM is being used with Animate - so we have left it turned off by default
fullSize - (default false) - set to true to add a fullsize icon to the titleBar
 	to let user increase the size of the window to the frame - will turn into a reduce size icon
fullSizeColor - (default #555) - the color of the fullSize icon
resizeHandle - (default false) - set to true to rollover bottom right corner to resize window with resizeHandle
collapse - (default false) - set to true to add a collapse button to the titleBar that reduces the window so only the bar shows and adds a button to expand
	also can double click bar to collapse window
collapseColor - (default #555) - the color of the collapse icon
collapsed - (default false) set to true to start the window collapsed
optimize - (default true) set to false to not turn DisplayObjects that are not on the stage visible false
	as the Window is scrolled, any objects within the content and any objects within one level of those objects
	are set to visible false if their bounds are not hitting the stage bounds - thanks Ami Hanya for the suggestion
	also see optimize property
style - (default true) set to false to ignore styles set with the STYLE - will receive original parameter defaults
group - (default null) set to String (or comma delimited String) so STYLE can set default styles to the group(s) (like a CSS class)
inherit - (default null) used internally but can receive an {} of styles directly

METHODS
add(obj, index, center, replace) - supports DUO - parameters or single object with properties that match parameters
 	adds obj to content container of window (at padding) must have bounds set
	it is best to position and size obj first before adding
	otherwise if adjusting to outside current content size then call update()
	index is the level or layer in the content with 0 being at the bottom
	center will center the content in the visible window
	replace defaults to false and if set to true, removes all content then adds the obj.
	returns window for chaining
remove(obj) - removes object from content container of window and updates - returns window for chaining
removeAll() - removes all objects from content container of window and updates - returns window for chaining
resize(width, height) - resizes the Window without scaling the content (also calls update() for scroll update)
	width and height are optional - returns window for chaining
update() - resets window scrolling if perhaps the content gets bigger or smaller
	update() does not quite update the dragBoundary due to a timeout in waiting for scrolls to be set
	so if animating the position of a window, set the enable property to false before animating
	then set the enable property to true on the animate call function
cancelCurrentDrag() - stop current drag on window - but add dragging back again for next drag
fullSize(state) - state defaults to true to set window to fullsize or set to false to go back to normal
	must start with the fullSize parameter set to true
	also see fullSized property
collapse(state) - state defaults to true to collapse or set to false to expand collapsed window
	must start with the collapse parameter set to true
	also see collapsed property
clone(recursive) - makes a copy with properties such as x, y, etc. also copied
	recursive (default true) clones the window content as well (set to false to not clone content)
dispose() - removes from parent, removes event listeners - must still set outside references to null for garbage collection

ALSO: ZIM 4TH adds all the methods listed under Container (see above), such as:
drag(), hitTestRect(), animate(), sca(), reg(), mov(), center(), centerReg(),
addTo(), removeFrom(), loop(), outline(), place(), pos(), alp(), rot(), setMask(), etc.
ALSO: see the CreateJS Easel Docs for Container methods, such as:
on(), off(), getBounds(), setBounds(), cache(), uncache(), updateCache(), dispatchEvent(),
addChild(), removeChild(), addChildAt(), getChildAt(), contains(), removeAllChildren(), etc.

PROPERTIES
type - holds the class name as a String
backing - CreateJS Shape used for backing of Window
backgroundColor - the color of the backing
content - ZIM Container used to hold added content
optimize - see optimize parameter - set to true (default) or false to optimize scrolling of Window
enabled - get or set whether the Window is enabled
scrollEnabled - get or set whether the Window can be scrolled
scrollBar - data object that holds the following properties (with defaults):
	you can set after object is made - call window.update() to see change
	scrollBar.horizontal = zim Shape // the horizontal scrollBar rectangle shape
	scrollBar.vertical = zim Shape // the vertical scrollBar rectangle shape
	scrollBar.color = borderColor; // the color of the scrollBar
	scrollBar.size = 6; // the width if vertical or the height if horizontal
	scrollBar.minSize = 12; // for the height if vertical or the width if horizontal
	scrollBar.spacing = 3 + size + borderWidth / 2;
	scrollBar.margin = 0; // adds extra space only at end by scrollBars
	scrollBar.corner = scrollBar.size / 2;
	scrollBar.showTime = .5; // s to fade in
	scrollBar.fadeTime = 3; // s to fade out
scrollX - gets and sets the content x position in the window (this will be negative)
scrollY - gets and sets the content y position in the window (this will be negative)
scrollXMax - gets the max we can scroll in x based on content width - window width (plus padding and margin)
scrollYMax - gets the max we can scroll in y based on content height - window height (plus padding and margin)
titleBar - access to the ZIM Container for the titleBar if there is a titleBar also has a backing property
titleBarLabel - access to the ZIM Label of the titleBar if there is a titleBar
closeIcon - access to the ZIM Shape if there is a close X
fullSizeIcon -  access to the ZIM Container if there is a fullSize icon
fullSized - get or set whether the window is full sized - must start with fullSize parameter set to true
	also see fullSize() method
collapseIcon - access to the ZIM Shape if there is a collapse triangle
collapsed - get or set whether the window is collapsed - must start with collapse parameter set to true
	also see collapse() method
resizeHandle - access the ZIM Rectangle that makes up the resizeHandle when resizeHandle parameter is set to true
	resizeHandle.removeFrom() would stop resize from being available and resizeHandle.addTo(window) would activate it again

ALSO: see ZIM Container for properties such as:
width, height, widthOnly, heightOnly, draggable, level, depth, group
blendMode, hue, saturation, brightness, contrast, etc.

ALSO: see the CreateJS Easel Docs for Container properties, such as:
x, y, rotation, scaleX, scaleY, regX, regY, skewX, skewY,
alpha, cursor, shadow, name, mouseChildren, mouseEnabled, parent, numChildren, etc.

EVENTS
dispatches a "select" event when clicked on in a traditional manner (fast click with little movement)
dispatches a "hoverover" event when rolled on without moving for 300 ms
dispatches a "hoverout" event when not hovering due to movement or mouseout on the window
dispatches a "scrolling" event when the window scrolls
dispatches a "close" event when the window is closed with the x on the titleBar if there is a titleBar
dispatches a "slidestart" event if slide is true and window starts sliding (on pressup)
dispatches a "slidestop" event if slide is true and window stops sliding
dispatches a "resize" event if resizing
dispatches a "collapse" event if collapsing
dispatches a "expand" event if expanding after being collapsed
dispatches a "fullsize" event if made fullscreen
dispatches a "originalsize" event if reduced from fullscreen

ALSO: see the CreateJS Easel Docs for Container events such as:
added, click, dblclick, mousedown, mouseout, mouseover, pressdown (ZIM), pressmove, pressup, removed, rollout, rollover
--*///+58.1
zim.Window = function(width, height, backgroundColor, borderColor, borderWidth, padding, corner, swipe, scrollBarActive, scrollBarDrag, scrollBarColor, scrollBarAlpha, scrollBarFade, scrollBarH, scrollBarV, slide, slideDamp, slideSnap, interactive, shadowColor, shadowBlur, paddingHorizontal, paddingVertical, scrollWheel, damp, titleBar, titleBarColor, titleBarBackgroundColor, titleBarHeight, draggable, boundary, onTop, close, closeColor, cancelCurrentDrag, fullSize, fullSizeColor, resizeHandle, collapse, collapseColor, collapsed, optimize, style, group, inherit) {
    var sig = "width, height, backgroundColor, borderColor, borderWidth, padding, corner, swipe, scrollBarActive, scrollBarDrag, scrollBarColor, scrollBarAlpha, scrollBarFade, scrollBarH, scrollBarV, slide, slideDamp, slideSnap, interactive, shadowColor, shadowBlur, paddingHorizontal, paddingVertical, scrollWheel, damp, titleBar, titleBarColor, titleBarBackgroundColor, titleBarHeight, draggable, boundary, onTop, close, closeColor, cancelCurrentDrag, fullSize, fullSizeColor, resizeHandle, collapse, collapseColor, collapsed, optimize, style, group, inherit";
    var duo; if (duo = zob(zim.Window, arguments, sig, this)) return duo;
    z_d("58.1");
    this.zimContainer_constructor(null,null,null,null,false);
    this.type = "Window";
    this.group = group;
    var DS = style===false?{}:zim.getStyle(this.type, this.group, inherit);

    if (zot(width)) width=DS.width!=null?DS.width:300;
    if (zot(height)) height=DS.height!=null?DS.height:200;
    if (zot(backgroundColor)) backgroundColor=DS.backgroundColor!=null?DS.backgroundColor:"#333";
    var originalBorderColor = borderColor;
    var originalBorderWidth = borderWidth;
    if (zot(borderColor)) borderColor=DS.borderColor!=null?DS.borderColor:"#999";
    if (zot(borderWidth)) borderWidth=DS.borderWidth!=null?DS.borderWidth:1; // 0
    if (zot(padding)) padding=DS.padding!=null?DS.padding:0;
    if (zot(corner)) corner=DS.corner!=null?DS.corner:0;
    if (zot(swipe)) swipe=DS.swipe!=null?DS.swipe:true; // true / auto, vertical, horizontal, false / none
    if (zot(scrollBarActive)) scrollBarActive=DS.scrollBarActive!=null?DS.scrollBarActive:true;
    if (zot(scrollBarDrag)) scrollBarDrag=DS.scrollBarDrag!=null?DS.scrollBarDrag:false;
    if (zot(scrollBarColor)) scrollBarColor=DS.scrollBarColor!=null?DS.scrollBarColor:borderColor;
    if (zot(scrollBarAlpha)) scrollBarAlpha=DS.scrollBarAlpha!=null?DS.scrollBarAlpha:.3;
    if (zot(scrollBarFade)) scrollBarFade=DS.scrollBarFade!=null?DS.scrollBarFade:true;
    if (zot(scrollBarH)) scrollBarH = DS.scrollBarH!=null?DS.scrollBarH:true;
    if (zot(scrollBarV)) scrollBarV = DS.scrollBarV!=null?DS.scrollBarV:true;
    if (scrollBarDrag) scrollBarFade = DS.scrollBarFade!=null?DS.scrollBarFade:false;
    if (zot(slide)) slide=DS.slide!=null?DS.slide:true;
    if (zot(slideDamp)) slideDamp=DS.slideDamp!=null?DS.slideDamp:.6;
    if (zot(slideSnap)) slideSnap=DS.slideSnap!=null?DS.slideSnap:"vertical"; // true / auto, vertical, horizontal, false / none
    if (zot(interactive)) interactive=DS.interactive!=null?DS.interactive:true;
    if (zot(shadowColor)) shadowColor=DS.shadowColor!=null?DS.shadowColor:"rgba(0,0,0,.3)";
    if (zot(shadowBlur)) shadowBlur=DS.shadowBlur!=null?DS.shadowBlur:20;
    if (zot(paddingVertical)) paddingVertical=DS.paddingVertical!=null?DS.paddingVertical:padding;
    if (zot(paddingHorizontal)) paddingHorizontal=DS.paddingHorizontal!=null?DS.paddingHorizontal:padding;
    if (zot(scrollWheel)) scrollWheel = DS.scrollWheel!=null?DS.scrollWheel:true;
    if (zot(titleBar)) titleBar = DS.titleBar!=null?DS.titleBar:null;
    if (zot(titleBarColor)) titleBarColor = DS.titleBarColor!=null?DS.titleBarColor:null;
    if (zot(titleBarBackgroundColor)) titleBarBackgroundColor = DS.titleBarBackgroundColor!=null?DS.titleBarBackgroundColor:null;
    if (zot(titleBarHeight)) titleBarHeight = DS.titleBarHeight!=null?DS.titleBarHeight:null;
    if (zot(draggable)) draggable = DS.draggable!=null?DS.draggable:null;
    if (zot(boundary)) boundary = DS.boundary!=null?DS.boundary:null;
    if (zot(onTop)) onTop = DS.onTop!=null?DS.onTop:null;
    if (zot(close)) close = DS.close!=null?DS.close:null;
    if (zot(closeColor)) closeColor = DS.closeColor!=null?DS.closeColor:null;
    if (zot(cancelCurrentDrag)) cancelCurrentDrag = DS.cancelCurrentDrag!=null?DS.cancelCurrentDrag:false;
    if (zot(fullSize)) fullSize = DS.fullSize!=null?DS.fullSize:null;
    if (zot(fullSizeColor)) fullSizeColor = DS.fullSizeColor!=null?DS.fullSizeColor:null;
    if (zot(resizeHandle)) resizeHandle = DS.resizeHandle!=null?DS.resizeHandle:null;
    if (zot(collapse)) collapse = DS.collapse!=null?DS.collapse:false;
    if (zot(collapseColor)) collapseColor = DS.collapseColor!=null?DS.collapseColor:null;
    if (zot(collapsed)) collapsed = DS.collapsed!=null?DS.collapsed:false;
    if (zot(optimize)) optimize = DS.optimize!=null?DS.optimize:true;

    if (titleBar === false) titleBar = null;
    if (!zot(titleBar)) {
        if (zot(titleBarHeight)) titleBarHeight = 30;
        height = height - titleBarHeight;
    }

    var that = this;
    that.optimize = optimize;
    this.scrollX = this.scrollY = this.scrollXMax = this.scrollYMax = 0;

    var backing = this.backing = new zim.Shape({style:false});
    this.addChild(backing);

    var mask = new createjs.Shape();
    mask.type = "WindowBacking";
    var mg = mask.graphics;
    // make the mask in the update function
    // when we know if there are vertical and horizontal scrollBars
    this.addChild(mask);

    var content = this.content = new zim.Container({style:false});
    this.addChild(content);
    content.mask = mask;
    var stage;

    if (!interactive) {
        // hitArea makes the whole window draggable
        // but then you can't interact with the content inside the window
        var hitArea = new createjs.Shape();
    }
    if (borderWidth > 0) {
        var border = new createjs.Shape();
        this.addChild(border);
    }

    var titleBarCorner = titleBar?0:corner;

    // we call this function at start and when resize() is called to resize the window without scaling content
    function sizeWindow() {
        that.setBounds(0,0,width,height);
        backing.graphics.c().f(backgroundColor).rc(0,0,width,height,titleBarCorner,titleBarCorner,corner,corner);
        if (shadowColor != -1 && shadowBlur > 0) backing.shadow = new createjs.Shadow(shadowColor, 4, 4, shadowBlur);

        if (borderWidth > 0) {
            if (corner) {
                border.graphics.c().s(borderColor).ss(borderWidth, "square", "miter").rc(0,0,width,height,titleBarCorner,titleBarCorner,corner,corner);
            } else {
                border.graphics.c().s(borderColor).ss(borderWidth, "square", "miter").dr(0,0,width,height);
            }
        }

        if (!zot(titleBarRect)) {
            titleBarRect.widthOnly = that.width+borderWidth;
            that.setBounds(0,-titleBarHeight,that.width,height+titleBarHeight);
        }

        if (closeIcon) {
            if (titleBar) closeIcon.pos({x:width-Math.max(corner/2, 10)-closeIcon.width/2, y:titleBarHeight/2, reg:true});
            else close.pos((Math.max(corner/2, 10))/2, closeIcon.height/2, true, false, that);
        }

        if (fullSizeIcon) {
            fullSizeIcon.pos({x:width-Math.max(corner/2, 10)-fullSizeIcon.width/2, y:titleBarHeight/2, reg:true});
            if (closeIcon) fullSizeIcon.mov(-closeIcon.width-10);
        }

        if (collapseIcon) {
            collapseIcon.pos({x:width-Math.max(corner/2, 10)-collapseIcon.width/2, y:titleBarHeight/2, reg:true});
            if (closeIcon) collapseIcon.mov(-closeIcon.width-10);
            if (fullSizeIcon) collapseIcon.mov(-fullSizeIcon.width-10);
        }

    }
    sizeWindow();

    // this exposes an scrollBar data object so creators can adjust scrollBar properties
    // note that these properties are set dynamically in the update function
    var scrollBar = this.scrollBar = {}; // data object to expose scrollBar properties
    scrollBar.color = scrollBarColor;
    scrollBar.size = 6;
    scrollBar.minSize = scrollBar.size*2; // if vertical scroll, this is vertical minSize where size is horizontal size
    scrollBar.spacing = 3.5 + borderWidth / 2;
    scrollBar.margin = 0;
    scrollBar.corner = scrollBar.size / 2;
    scrollBar.showTime = .5;
    scrollBar.fadeTime = 3;

    if (scrollBarActive) {
        var hscrollBar = scrollBar.horizontal = new zim.Shape({style:false});
        var hg = hscrollBar.graphics;
        hscrollBar.alpha = scrollBarAlpha;
        this.addChild(hscrollBar);
        if (scrollBarDrag) hscrollBar.drag({localBounds: true});

        var vscrollBar = scrollBar.vertical = new zim.Shape({style:false});
        var vg = vscrollBar.graphics;
        vscrollBar.alpha = scrollBarAlpha;
        this.addChild(vscrollBar);
        if (scrollBarDrag) vscrollBar.drag({localBounds: true});
    }

    var hProportion;
    var vProportion;
    var hCheck;
    var vCheck;
    var gap;
    var contentWidth;
    var contentHeight;

    var hEvent;
    var vEvent;

    this.update = function() {
        if (scrollBarActive) {
            // clear the scrollBars and remake anytime this function is called
            // as these may change as people add and remove content to the Window
            hg.clear(); // horizontal scrollBar
            vg.clear(); // vertical scrollBar
        }

        // assume no gap at left and top
        // gap is applied in x if there is a scroll in y
        // gap is applied in y if there is a scroll in x
        gap = (scrollBarActive) ? scrollBar.size+scrollBar.spacing*2 : 0;
        contentWidth = content.getBounds()?content.getBounds().width:0;
        contentHeight = content.getBounds()?content.getBounds().height:0;

        // note, the contentWidth and contentHeight include ONE padding
        hCheck = (scrollBarH && contentWidth > width-paddingHorizontal && (scrollBarActive || swipe === true || swipe == "auto" || swipe == "horizontal"));
        vCheck = (scrollBarV && contentHeight > height-paddingVertical && (scrollBarActive || swipe === true || swipe == "auto" || swipe == "vertical"));

        that.scrollXMax = contentWidth+paddingHorizontal*2-width+(vCheck?gap+scrollBar.margin:0);
        that.scrollYMax = contentHeight+paddingVertical*2-height+(hCheck?gap+scrollBar.margin:0);

        // set mask dynamically as scrollBars may come and go affecting the mask size slightly
        mg.clear();
        var xx = borderWidth/2;
        var yy = borderWidth/2;
        var ww = width-((vCheck && scrollBarActive)?scrollBar.size+scrollBar.spacing*2:0)-(vCheck?0:borderWidth);
        var hh = height-((hCheck && scrollBarActive)?scrollBar.size+scrollBar.spacing*2:0)-(hCheck?0:borderWidth);
        mg.f("rgba(0,0,0,0)").rc(xx,yy,ww,hh,titleBarCorner,titleBarCorner,vCheck&&scrollBarActive?0:corner,hCheck&&scrollBarActive?0:corner);

        mask.setBounds(that.getBounds().x,that.getBounds().y,that.getBounds().width, that.getBounds().height);
        zim.expand(mask, 0);
        if (!interactive) {
            hitArea.graphics.c().f("red").dr(xx,yy,ww,hh);
            content.hitArea = hitArea;
        }

        var edgeAdjust = Math.max(corner, Math.min(scrollBar.corner, scrollBar.spacing));
        var edgeLeft = edgeAdjust + borderWidth/2;
        var edgeRight = edgeAdjust + (vCheck?gap:0) + borderWidth/2;
        var edgeTop = edgeAdjust + borderWidth/2;
        var edgeBottom = edgeAdjust + (hCheck?gap:0) + borderWidth/2;

        var scrollBarLength, rect;
        if (hCheck && scrollBarActive) {
            scrollBarLength = Math.max(scrollBar.minSize, (width-edgeLeft-edgeRight) * (width-edgeLeft-edgeRight) / (contentWidth + paddingHorizontal + scrollBar.margin));
            hg.f(scrollBar.color).rr(0,0,scrollBarLength,scrollBar.size,scrollBar.corner);
            hscrollBar.x = edgeLeft;
            hscrollBar.y = height-scrollBar.size-scrollBar.spacing;
            // for swiping window:
            hProportion = new zim.Proportion(-that.scrollXMax, 0, edgeLeft, width-scrollBarLength-edgeRight, -1);
            if (scrollBarDrag) {
                hscrollBar.setBounds(0,0,scrollBarLength,scrollBar.size);
                // drag rect for scrollBar
                rect = new createjs.Rectangle(
                    edgeLeft, hscrollBar.y, width-scrollBarLength-edgeLeft-edgeRight, 0
                );
                hscrollBar.dragBoundary(rect);
                hscrollBar.proportion = new zim.Proportion(
                    rect.x, rect.x+rect.width, 0, -that.scrollXMax
                );
                hscrollBar.off("pressmove", hEvent);
                hEvent = hscrollBar.on("pressmove", function() {
                    that.dispatchEvent("scrolling");
                    if (hitArea) {
                        // move hitarea to display box
                        hitArea.x = -content.x;
                        hitArea.y = -content.y;
                    }
                    content.x = hscrollBar.proportion.convert(hscrollBar.x);
                    testContent();
                });
            }
        }

        if (vCheck && scrollBarActive) {
            scrollBarLength = Math.max(scrollBar.minSize, (height-edgeTop-edgeBottom) * (height-edgeTop-edgeBottom) / (contentHeight + paddingVertical + scrollBar.margin));
            vg.f(scrollBar.color).rr(0,0,scrollBar.size,scrollBarLength,scrollBar.corner);
            vscrollBar.x = width-scrollBar.size-scrollBar.spacing;
            vscrollBar.y = edgeTop;
            // for swiping window:
            vProportion = new zim.Proportion(-that.scrollYMax, 0, edgeTop, height-scrollBarLength-edgeBottom, -1);
            if (scrollBarDrag) {
                vscrollBar.setBounds(0,0,scrollBar.size,scrollBarLength);
                // drag rect for scrollBar
                rect = new createjs.Rectangle(
                    vscrollBar.x, edgeTop, 0, height-scrollBarLength-edgeTop-edgeBottom
                );
                vscrollBar.dragBoundary(rect);
                vscrollBar.proportion = new zim.Proportion(
                    rect.y, rect.y+rect.height, 0, -that.scrollYMax
                );
                vscrollBar.off("pressmove", vEvent);
                vEvent = vscrollBar.on("pressmove", function() {
                    that.dispatchEvent("scrolling");
                    if (hitArea) {
                        // move hitarea to display box
                        hitArea.x = -content.x;
                        hitArea.y = -content.y;
                    }
                    desiredY = content.y = vscrollBar.proportion.convert(vscrollBar.y);
                    testContent();
                });
            }
        }
        movescrollBars();

        clearTimeout(that.d2Timeout);
        that.d2Timeout = setTimeout(function(){
            if (hscrollBar && hscrollBar.proportion) content.x = hscrollBar.proportion.convert(hscrollBar.x);
            if (vscrollBar && vscrollBar.proportion) content.y = vscrollBar.proportion.convert(vscrollBar.y);
        }, 50);
        clearTimeout(that.dTimeout);
        that.dTimeout = setTimeout(function(){setdragBoundary();}, 300);
        setdragBoundary();
        testContent();
    };

    this.resize = function(w, h) {
        if (zot(w)) w = width;
        if (zot(h)) h = height;
        var min = 20;
        if (titleBar) min = titleBarLabel.x+titleBarLabel.width+10;
        if (w < min) w = min;
        if (h < 20) h = 20;
        width = w;
        height = h;
        sizeWindow();
        for (var i=0; i<content.numChildren; i++) {
            var cont = content.getChildAt(i);
            if (cont.type == "Wrapper") resizeWrapper(cont);
        }
        that.update();
        desiredY = content.y;
        if (damp) dampY.immediate(desiredY);
        that.dispatchEvent("resize");
        return that;
    };

    if (!zot(titleBar)) {
        if (zot(draggable)) draggable = true;
        if (typeof titleBar == "string") titleBar = new zim.Label({
            text:titleBar, color:titleBarColor, size:DS.size!=null?DS.size:20,
            backing:"ignore", shadowColor:"ignore", shadowBlur:"ignore", padding:"ignore", backgroundColor:"ignore",
            group:this.group
        });
        var titleBarLabel = that.titleBarLabel = titleBar;
        if (zot(titleBarBackgroundColor)) titleBarBackgroundColor = "rgba(0,0,0,.2)";
        that.titleBar = titleBar = new zim.Container(width, titleBarHeight, null, null, false).centerReg(that).mov(0,-height/2-titleBarHeight/2);
        var titleBarRect = that.titleBar.backing = new zim.Rectangle(width+borderWidth, titleBarHeight, titleBarBackgroundColor, null, null, [corner*.95, corner*.95, 0, 0], true, null, false).center(titleBar);
        titleBarLabel.center(titleBar).pos({x:Math.max(corner/2, Math.max(10, padding)), reg:true});
        that.regX = 0; that.regY = -titleBarHeight;
        that.setBounds(0,-titleBarHeight,width,height+titleBarHeight);

        if (draggable) {
            titleBar.cursor = "pointer";
            titleBar.on("mousedown", function() {
                that.drag({rect:boundary, currentTarget:true, onTop:onTop});
            });
            titleBar.on("pressup", function() {
                that.noDrag(false);
            });
        } else {
            titleBar.on("mousedown", function () {});
        }

    }

    if (close) {
        if (zot(closeColor)) closeColor = "#555";
        var closeIcon = that.closeIcon = new zim.Shape(-40,-40,80,80,null,false);
        closeIcon.graphics.f(closeColor).p("AmJEVIEUkTIkXkWIB4h5IEWEYIETkTIB4B3IkTESIEQERIh4B4IkRkRIkSEVg"); // width about 90 reg in middle
        if (titleBar) closeIcon.centerReg(that).scaleTo(titleBar, null, 50).pos({x:width-Math.max(corner/2, 10)-closeIcon.width/2, y:titleBarHeight/2, reg:true}).expand(40);
        else {
            closeIcon.sca(.3).pos((Math.max(corner/2, 10))/2, closeIcon.height/2, true, false, that).expand(40);
        }
        closeIcon.cursor = "pointer";
        closeIcon.expand();
        closeIcon.on((!zns?W.ACTIONEVENT=="mousedown":zim.ACTIONEVENT=="mousedown")?"mousedown":"click", function(){
            var ss = that.stage;
            that.removeFrom();
            that.dispatchEvent("close");
            if (ss) ss.update();
        });
    }

    var collapseCheck,fullCheck,lastWidth,lastHeight,lastX,lastY;
    if (!zot(titleBar)) {
        if (fullSize) {
            that.fullSize = function(state) {
                if (zot(state)) state = true;
                if (state==fullCheck || !that.parent) return that;
                if (!state) {
                    that.resize(lastWidth, lastHeight-titleBar.height);
                    that.x = lastX;
                    that.y = lastY;
                    fullCheck = false;
                    reduceSize.alpha = 0;
                    fullSizeIcon.getChildAt(0).alpha = 1;
                    that.dispatchEvent("originalsize");
                } else {
                    lastWidth = that.width;
                    lastHeight = that.height;
                    lastX = that.x;
                    lastY = that.y;
                    that.resize(that.parent.width, that.parent.height);
                    that.x = 0;
                    that.y = 0;
                    fullCheck = true;
                    reduceSize.alpha = 1;
                    fullSizeIcon.getChildAt(0).alpha = .01;
                    that.dispatchEvent("fullsize");
                }
                if (that.stage) that.stage.update();
            }
            if (zot(fullSizeColor)) fullSizeColor = "#555";
            var fullSizeIcon = that.fullSizeIcon = new zim.Rectangle(80,80,zim.faint,fullSizeColor,16);
            var reduceSize = new zim.Shape()
                .s(fullSizeColor).ss(16)
                .mt(-19.6,-20.6)
                .lt(-19.6,-40).lt(40,-40).lt(40,20.7).lt(19.6,20.7).lt(19.6,40).lt(-40,40).lt(-40,-20.6).lt(-19.6,-20.6).lt(19.6,-20.6).lt(19.6,20.7)
                .addTo(fullSizeIcon)
                .mov(40,40)
                .alp(0);
            fullSizeIcon.centerReg(that)
                .scaleTo(titleBar, null, 42)
                .pos({x:width-Math.max(corner/2, 10)-fullSizeIcon.width/2, y:titleBarHeight/2, reg:true})
                .expand(40);
            if (closeIcon) fullSizeIcon.mov(-closeIcon.width-10);
            fullSizeIcon.cursor = "pointer";
            fullSizeIcon.expand();
            fullCheck = false;
            lastWidth = width;
            lastHeight = height;
            lastX = that.x;
            lastY = that.y;
            fullSizeIcon.on((!zns?W.ACTIONEVENT=="mousedown":zim.ACTIONEVENT=="mousedown")?"mousedown":"click", function(){
                if (fullCheck) {
                    that.fullSize(false);
                } else {
                    that.fullSize(true);
                }
            });
        }
        if (collapse) {
            collapseCheck = collapsed;
            that.collapse = function(state) {
                if (zot(state)) state = true;
                if (state==collapseCheck) return that;
                if (!state) {
                    collapseIcon.rot(0);
                    that.backing.visible = true;
                    that.content.visible = true;
                    mask.visible =  true;
                    if (hscrollBar) hscrollBar.visible = true;
                    if (hscrollBar) vscrollBar.visible = true;
                    if (border) border.visible = true;
                    collapseCheck = false;
                    titleBarRect.sha(null);
                    that.dispatchEvent("expand");
                } else {
                    collapseIcon.rot(180);
                    that.backing.visible = false;
                    that.content.visible = false;
                    mask.visible = false;
                    if (hscrollBar) hscrollBar.visible = false;
                    if (hscrollBar) vscrollBar.visible = false;
                    if (border) border.visible = false;
                    collapseCheck = true;
                    titleBarRect.sha(shadowColor, 5,5, shadowBlur);
                    that.dispatchEvent("collapse");
                }
                if (that.stage) that.stage.update();
            }
            if (zot(collapseColor)) collapseColor = "#555";
            var collapseIcon = that.collapseIcon = new zim.Triangle(90,90,90,zim.faint,collapseColor,16);
            collapseIcon.centerReg(that)
                .scaleTo(titleBar, null, 42)
                .pos({x:width-Math.max(corner/2, 10)-collapseIcon.width/2, y:titleBarHeight/2, reg:true})
                .expand(40);
            if (closeIcon) collapseIcon.mov(-closeIcon.width-10);
            if (fullSizeIcon) collapseIcon.mov(-fullSizeIcon.width-10);
            collapseIcon.cursor = "pointer";
            collapseIcon.expand();
            collapseCheck = false;
            lastWidth = width;
            lastHeight = height;
            lastX = that.x;
            lastY = that.y;
            collapseIcon.on((!zns?W.ACTIONEVENT=="mousedown":zim.ACTIONEVENT=="mousedown")?"mousedown":"click", function(){
                if (collapseCheck) {
                    that.collapse(false);
                } else {
                    that.collapse(true);
                }
            });
        }
    }

    if (resizeHandle) {
        var handle = that.resizeHandle = new zim.Rectangle(25, 25, zim.grey, zim.white)
            .alp(.01)
            .centerReg()
            .rot(45)
            .hov(.5)
            .loc(0, 0, that)
            .mov(that.width, that.height - (that.titleBar ? that.titleBar.height : 0))
            .drag();
        handle.on("pressmove", function () {
            that.resize(handle.x, handle.y);
        });
        handle.on("pressup", placeHandle);
        if (that.titleBar) that.titleBar.on("pressup", placeHandle);
    }

    function placeHandle(e) {
        e.currentTarget
            .loc(0, 0, that)
            .top()
            .alp(.01)
            .mov(that.width, that.height - (that.titleBar ? that.titleBar.height : 0));
    }

    function resizeWrapper(cont) {
        if (cont.align == "right") {
            cont.x=paddingHorizontal;
            cont.resize(that.width-(vCheck?scrollBar.size+scrollBar.spacing*2:paddingHorizontal)-paddingHorizontal*2);
        } else if (cont.align == "center" || cont.align == "middle") {
            cont.resize(that.width-(vCheck?scrollBar.size+scrollBar.spacing*2:0)-paddingHorizontal*2);
        } else {
            cont.resize(that.width-(vCheck?scrollBar.size+scrollBar.spacing*2:0)-paddingHorizontal*2);
        }
        cont.alpha = cont.wrapperLastAlpha;
    }

    // METHODS to add and remove content from Window
    this.add = function(obj, index, center, replace) {
        var sig = "obj, index, center, replace";
        var duo; if (duo = zob(that.add, arguments, sig, that)) return duo;
        var c = obj;
        if (!c.getBounds()) {zogy("Window.add() - please add content with bounds set"); return;}
        makeDamp(c);
        if (zot(index)) index = content.numChildren;
        if (replace) {
            index = 0;
            that.removeAll();
        }
        if (center) {
            c.center(that).addTo(content, index).mov(0,titleBarHeight?titleBarHeight/2:0);
        } else {
            c.addTo(content, index);
        }
        if (c.type == "Wrapper") {
            vscrollBar.alpha = 0;
            scrollBarH = false;
            c.wrapperLastAlpha = c.alpha;
            c.alpha = 0;
            this.added(function(){
                resizeWrapper(c);
                that.resize();
                hscrollBar.alpha = 1;
                vscrollBar.alpha = 1;
            });
        }
        if (c.x == 0) c.x = paddingHorizontal;
        if (c.y == 0) c.y = paddingVertical;
        that.update();
        return that;
    };

    this.remove = function(c) {
        content.removeChild(c);
        that.update();
        return that;
    };

    this.removeAll = function() {
        content.removeAllChildren();
        that.update();
        return that;
    };

    function setdragBoundary(on) {
        if (zot(stage)) stage = that.stage || W.zdf.stage;
        if (zot(on)) on = true;
        if (on) zim.dragBoundary(content, new createjs.Rectangle(0, 0, hCheck?-that.scrollXMax:0, vCheck?-that.scrollYMax:0));
        else zim.dragBoundary(content, new createjs.Rectangle(-1000, -1000, stage.width+2000, stage.height+2000));
    }

    var swipeCheck = false;
    if (swipe) {
        content.on("mousedown", function() {
            if (!swipeCheck) zim.Ticker.add(swipeMovescrollBars, content.stage);
            swipeCheck = true;
            if (hCheck && scrollBarActive) if (scrollBarFade) zim.animate(hscrollBar, {alpha:scrollBarAlpha}, scrollBar.showTime);
            if (vCheck && scrollBarActive) if (scrollBarFade) zim.animate(vscrollBar, {alpha:scrollBarAlpha}, scrollBar.showTime);
        });
    }

    function swipeMovescrollBars() {
        // this is being called by the swipe which has its own damping
        // so we need to set the desiredY and then move the scrollBars
        // as the movescrollBars needs to run independently - so both types of damp can controll it
        desiredY = content.y;
        if (damp) dampY.immediate(desiredY);
        if (scrollBarActive) movescrollBars();
    }

    function testContent() {
        if (!that.optimize) return;
        stage = that.stage;
        content.loop(function(item) {
            if (!item.hitTestBounds || !item.stage) return; // don't turn off items if not on stage yet
            if (item.hitTestBounds(stage)) {
                item.visible = true;
                if (item.loop) item.loop(function(item2) {
                    if (!item2.hitTestBounds) return;
                    if (item2.hitTestBounds(stage)) item2.visible = true;
                    else item2.visible = false;
                });
            } else item.visible = false;
        });
    }

    function movescrollBars() {
        testContent();
        that.dispatchEvent("scrolling");
        if (hitArea) {
            // move hitarea to display box
            hitArea.x = -content.x;
            hitArea.y = -content.y;
        }
        if (hCheck && scrollBarActive) hscrollBar.x = hProportion.convert(content.x);
        if (vCheck && scrollBarActive) vscrollBar.y = vProportion.convert(content.y);
    }

    // may add content before adding Window to stage...
    this.on("added", function() {
        setDrag(50);
    }, null, true); // once
    function setDrag(delay) {
        if (zot(delay)) delay = 100;
        makeDamp(that);
        if (!swipe) return;
        setTimeout(function(){
            if (content) {
                zim.drag({
                    obj:content,
                    currentTarget:true,
                    localBounds:true,
                    slide:slide,
                    slideDamp:slideDamp,
                    slideSnap:(scrollBarH && (swipe===true||swipe=="auto"||swipe=="horizontal")) || (scrollBarV && (swipe===true||swipe=="auto"||swipe=="vertical"))?slideSnap:false
                });
                content.removeAllEventListeners("slidestart");
                content.on("slidestart", function () {
                    that.dispatchEvent("slidestart");
                });
                content.removeAllEventListeners("slidestop");
                content.on("slidestop", function (e) {
                    if (slide) stageUp(e);
                    that.dispatchEvent("slidestop");
                });
                if (content.getBounds() && content.getBounds().width > 0) {
                    setdragBoundary();
                }
            }
        }, delay);
    }
    this.cancelCurrentDrag = function() {
        if (that.content) that.content.noDrag(false);
        setTimeout(function(){
            if (content) {
                zim.drag({
                    obj:content,
                    currentTarget:true,
                    localBounds:true,
                    slide:slide, slideDamp:slideDamp,
                    slideSnap:(scrollBarH && (swipe===true||swipe=="auto"||swipe=="horizontal")) || (scrollBarV && (swipe===true||swipe=="auto"||swipe=="vertical"))?slideSnap:false
                });
                if (content.getBounds() && content.getBounds().width > 0) {
                    setdragBoundary();
                }
            }
        }, 300);
    };

    var stageEvent;
    this.added(function (_stage) {
        stage = _stage;
        stageEvent = stage.on("stagemousemove", function (e) {
            that.windowMouseX = e.stageX/zim.scaX;
            that.windowMouseY = e.stageY/zim.scaY;
        });
    });

    if (slide) {
        content.on("slidestop", stageUp);
    } else {
        content.on("mousedown", function() {
            content.stage.on("stagemouseup", stageUp, null, true);
        });
    }
    if (cancelCurrentDrag) {
        that.blurEvent = function () {
            that.cancelCurrentDrag();
            stageUp();
        };
        document.W.addEventListener("blur", that.blurEvent);
    }

    function stageUp() {
        zim.Ticker.remove(swipeMovescrollBars);
        swipeCheck = false;
        if (hCheck) if (scrollBarFade && scrollBarActive) zim.animate(hscrollBar, {alpha:0}, scrollBar.fadeTime);
        if (vCheck) if (scrollBarFade && scrollBarActive) zim.animate(vscrollBar, {alpha:0}, scrollBar.fadeTime);
    }

    var hoverOutCalled = false;
    var startTime;
    var lastMouseX = 0;
    var lastMouseY = 0;
    var lastReportX = 0;
    var lastReportY = 0;
    var pauseTime = 300;
    var thresh = 2;

    function moveOn() {
        startTime=Date.now();
        zim.Ticker.add(timeMouse, content.stage);
    }
    function moveOff() {
        if (!hoverOutCalled) {
            that.dispatchEvent("hoverout");
            hoverOutCalled = true;
        }
        zim.Ticker.remove(timeMouse);
    }
    function timeMouse() {
        if (!content.stage) {
            if (!hoverOutCalled) {
                that.dispatchEvent("hoverout");
                hoverOutCalled = true;
            }
            zim.Ticker.remove(timeMouse);
            return;
        }
        if (Math.abs(lastMouseX-that.windowMouseX) > thresh || Math.abs(lastMouseY-that.windowMouseY) > thresh) {
            if (!hoverOutCalled) {
                that.dispatchEvent("hoverout");
                hoverOutCalled = true;
            }
            startTime=Date.now();
            lastMouseX=that.windowMouseX;
            lastMouseY=that.windowMouseY;
        } else {
            if (Date.now()-startTime > pauseTime) {
                if (Math.abs(lastReportX-that.windowMouseX) > thresh || Math.abs(lastReportY-that.windowMouseY) > thresh) {
                    that.contentMouse = content.globalToLocal(that.windowMouseX, that.windowMouseY);
                    that.dispatchEvent("hoverover");
                    lastReportX=that.windowMouseX;
                    lastReportY=that.windowMouseY;
                    hoverOutCalled = false;
                }
                startTime=Date.now();
            }
        }
    }

    if (interactive) {
        // dispatches SELECT (click) and HOVEROVER (500 ms) and gives mouseX and mouseY on content
        // CLICKS (in the traditional sense rather than a mouseup replacement)
        var downLoc;
        var downTime;

        content.on("mousedown", function(e){stage=e.target.stage; downLoc=e.stageX/zim.scaX; downTime=Date.now();});
        content.on("click", function(e){
            if (Date.now()-downTime<600 && Math.abs(e.stageX/zim.scaX-downLoc)<5) {
                that.contentMouse = content.globalToLocal(e.stageX/zim.scaX, e.stageY/zim.scaY);
                that.dispatchEvent("select");
            }
        });
        // HOVER (must stay within thresh pixels for pauseTime ms)
        content.on("mouseover", moveOn);
        content.on("mouseout", moveOff);
    }

    var desiredY = that.scrollY;
    that.scrollWindow = function scrollWindow(e) {
        if (vCheck && that.stage && that.hitTestPoint(that.windowMouseX, that.windowMouseY) && that.contains(that.stage.getObjectUnderPoint(that.windowMouseX*zim.scaX, that.windowMouseY*zim.scaY))) {
            if (zot(e)) e = event;
            var delta = e.detail ? e.detail*(-19) : e.wheelDelta;
            if (zot(delta)) delta = e.deltaY*(-19);
            desiredY += delta;
            desiredY = Math.max(-that.scrollYMax, Math.min(0, desiredY));
            if (!damp) {
                that.scrollY = desiredY;
                content.stage.update();
            }
        }
    }
    if (scrollWheel) {
        W.addEventListener("mousewheel", that.scrollWindow);
        W.addEventListener("wheel", that.scrollWindow);
        W.addEventListener("DOMMouseScroll", that.scrollWindow);
    }
    var dampCheck = false;
    var dampY;
    function makeDamp(obj) {
        if (damp && !dampCheck && obj.stage) {
            dampCheck = true;
            dampY = new zim.Damp(that.scrollY, damp);
            that.dampTicker = zim.Ticker.add(function() {
                if (swipeCheck) return;
                if (!zot(desiredY)) that.scrollY = dampY.convert(desiredY);
            }, obj.stage);
        }
    }

    this._enabled = true;
    Object.defineProperty(that, 'enabled', {
        get: function() {
            return that._enabled;
        },
        set: function(value) {
            if (!value) {
                clearTimeout(that.dTimeout);
                zim.noDrag(content);
            } else {
                setDrag();
            }
            zenable(that, value);
        }
    });

    this._scrollEnabled = true;
    Object.defineProperty(that, 'scrollEnabled', {
        get: function() {
            return that._scrollEnabled;
        },
        set: function(value) {
            if (!value) {
                clearTimeout(that.dTimeout);
                zim.noDrag(content);
                if (scrollBarDrag) {
                    if (hEvent) hscrollBar.mouseEnabled = false; // hscrollBar.off("pressmove", vEvent);
                    if (vEvent) vscrollBar.mouseEnabled = false; // vscrollBar.off("pressmove", vEvent);

                }
                W.removeEventListener("mousewheel", that.scrollWindow);
                W.removeEventListener("wheel", that.scrollWindow);
                W.removeEventListener("DOMMouseScroll", that.scrollWindow);
            } else {
                setDrag();
                if (scrollBarDrag) {
                    if (hEvent)  hscrollBar.mouseEnabled = true; //hEvent = hscrollBar.on("pressmove", vEvent);
                    if (vEvent)  vscrollBar.mouseEnabled = true; //vEvent = vscrollBar.on("pressmove", vEvent);
                }
                W.addEventListener("mousewheel", that.scrollWindow);
                W.addEventListener("wheel", that.scrollWindow);
                W.addEventListener("DOMMouseScroll", that.scrollWindow);
            }
            that._scrollEnabled = value;
        }
    });

    Object.defineProperty(that, 'scrollX', {
        get: function() {
            return content.x;
        },
        set: function(value) {
            content.x = value;
            clearTimeout(that.d2Timeout);
            if (content.zimDragImmediate) content.zimDragImmediate(content.x, content.y);
            movescrollBars();
        }
    });

    Object.defineProperty(that, 'scrollY', {
        get: function() {
            return content.y;
        },
        set: function(value) {
            content.y = value;
            clearTimeout(that.d2Timeout);
            if (content.zimDragImmediate) content.zimDragImmediate(content.x, content.y);
            movescrollBars();
        }
    });

    Object.defineProperty(that, 'backgroundColor', {
        get: function() {
            return backgroundColor;
        },
        set: function(value) {
            backgroundColor = value;
            sizeWindow();
        }
    });

    Object.defineProperty(that, 'fullSized', {
        get: function() {
            return fullCheck;
        },
        set: function(value) {
            if (that.fullSize) that.fullSize(value);
        }
    });

    Object.defineProperty(that, 'collapsed', {
        get: function() {
            return collapseCheck;
        },
        set: function(value) {
            if (that.collapse) that.collapse(value);
        }
    });

    if (collapsed) {
        collapseCheck = false;
        that.collapsed = true;
    }
    if (that.titleBar) {
        that.collapseEvent = that.titleBar.on("dblclick", function () {
            that.collapsed = !that.collapsed;
        });
    }

    if (style!==false) zim.styleTransforms(this, DS);
    this.clone = function(recursive) {
        if (zot(recursive)) recursive = true;
        var w = that.cloneProps(new zim.Window(width, height, backgroundColor, originalBorderColor, originalBorderWidth, padding, corner, swipe, scrollBarActive, scrollBarDrag, scrollBar.color, scrollBarAlpha, scrollBarFade, scrollBarH, scrollBarV, slide, slideDamp, slideSnap, interactive, shadowColor, shadowBlur, paddingHorizontal, paddingVertical, titleBar, titleBarColor, titleBarBackgroundColor, titleBarHeight, draggable, boundary, onTop, close, closeColor, cancelCurrentDrag, fullSize, fullSizeColor, resizeHandle, collapse, collapseColor, collapsed, optimize, style, group, inherit));
        if (recursive) {
            that.content.cloneChildren(w.content);
            w.update();
        }
        return w;
    };

    this.doDispose = function(a,b,disposing) {
        if (scrollWheel) {
            W.removeEventListener("mousewheel", that.scrollWindow);
            W.removeEventListener("wheel", that.scrollWindow);
            W.removeEventListener("DOMMouseScroll", that.scrollWindow);
        }
        if (that.titleBar) that.titleBar.removeAllEventListeners();
        if (stageEvent && stage) stage.off("stagemousemove", stageEvent);
        if (that.resizeHandle) that.resizeHandle.removeAllEventListeners();
        if (that.blurEvent) W.removeEventListener("blur", that.blurEvent);
        if (typeof timeMouse != "undefined") zim.Ticker.remove(timeMouse);
        if (!zot(swipeMovescrollBars)) zim.Ticker.remove(swipeMovescrollBars);
        if (!disposing) this.zimContainer_dispose(true);
        content = that.content = null;
        return true;
    };
};
