// Copyright 2015 Yves Lucet, University of British Columbia Okanagan. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
// conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
// of conditions and the following disclaimer in the documentation and/or other materials
// provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
// FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
// ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// The views and conclusions contained in the software and documentation are those of the
// authors and should not be interpreted as representing official policies, either expressed
// or implied, of the University of British Columbia Okanagan.

// Naming conventions:
//
// u, v: Node grid coordinates (top-left is 0,0).
// x, y: Canvas pixel coordinates (top-left is 0,0).
//
// A rect is a plain Javascript object with the following fields:
//
// - id (graphics object id)
// - u, v, x, y (number): As above.

function KMP(am, w, h) {
    this.init(am, w, h);
}

KMP.prototype = new Algorithm();
KMP.prototype.constructor = KMP;
KMP.superclass = Algorithm.prototype;

KMP.MARGIN_X = 40;
KMP.MARGIN_Y = 100;
KMP.COUNTER_BUILD_LABEL_X = 20;
KMP.COUNTER_BUILD_LABEL_Y = 20;
KMP.COUNTER_SEARCH_LABEL_X = 20;
KMP.COUNTER_SEARCH_LABEL_Y = 40;
KMP.STATUS_LABEL_X = 20;
KMP.STATUS_LABEL_Y = 60;
KMP.BOX_WIDTH = 30;
KMP.BOX_HEIGHT = 30;
KMP.ROW_SPACING = 20;
KMP.FAILURE_FN_LABEL_X = 30;
KMP.LEFT_LABEL_MARGIN = KMP.BOX_WIDTH;

KMP.NORMAL_FG_COLOR = "#000";
KMP.ACTIVE_FG_COLOR = "#f00";

KMP.prototype.init = function(am, w, h) {
    KMP.superclass.init.call(this, am, w, h);
    this.addControls();
    this.reset();
}

KMP.prototype.addControls = function() {
    this.controls = [];

    this.btnReset = addControlToAlgorithmBar("Button", "Reset");
    this.btnReset.onclick = this.reset.bind(this);
    this.controls.push(this.btnReset);

    this.btnBuildFailureFn = addControlToAlgorithmBar("Button", "Build failure function");
    this.btnBuildFailureFn.onclick = this.buildFailureFnWrapper.bind(this);
    this.controls.push(this.btnBuildFailureFn);

    this.btnLookup = addControlToAlgorithmBar("Button", "Search");
    this.btnLookup.onclick = this.searchWrapper.bind(this);
    this.controls.push(this.btnLookup);

    this.lblText = addLabelToAlgorithmBar("Text:");
    this.txtText = addControlToAlgorithmBar("Text", "aababcabcdabcdeabcdef");
    this.controls.push(this.txtText);

    this.lblPattern = addLabelToAlgorithmBar("Pattern:");
    this.txtPattern = addControlToAlgorithmBar("Text", "abcdef");
    this.controls.push(this.txtPattern);
}

KMP.prototype.disableUI = function(event) {
    this.setEnabled(false);
}

KMP.prototype.enableUI = function(event) {
    this.setEnabled(true);
}

KMP.prototype.setEnabled = function(b) {
    for (var i = 0; i < this.controls.length; ++i) {
        this.controls[i].disabled = !b;
    }
}

KMP.prototype.reset = function() {
    var i, lastKeys, lastStr;

    this.text = undefined;
    this.pattern = this.txtPattern.value;
    this.nextId = 0;

    this.animationManager.resetAll();
    this.clearHistory();

    this.commands = [];

    // Create label for counting comparisons in the build and search steps.
    this.counterBuildId = this.newId();
    this.counterSearchId = this.newId();
    this.cmd(
        "CreateLabel", this.counterBuildId, "",
        KMP.COUNTER_BUILD_LABEL_X, KMP.COUNTER_BUILD_LABEL_Y, 0);
    this.cmd(
        "CreateLabel", this.counterSearchId, "",
        KMP.COUNTER_SEARCH_LABEL_X, KMP.COUNTER_SEARCH_LABEL_Y, 0);
    this.counterBuild = 0;
    this.counterSearch = 0;

    // Create a status label for displaying what the algorithm is doing.
    this.statusId = this.newId();
    this.cmd(
        "CreateLabel", this.statusId, "", KMP.STATUS_LABEL_X, KMP.STATUS_LABEL_Y, 0);

    // Create rectangles for displaying the pattern.
    this.textIndex = undefined;
    this.patternIndex = undefined;
    this.patternShift = 0;
    this.textRects = [this.newCharBox("---", 0, 0)];
    this.patternRects = [];
    for (i = 0; i < this.pattern.length; ++i) {
        this.patternRects.push(this.newCharBox(this.pattern[i], i, 1));
    }

    // Create rectangles for displaying the failure function.
    this.failureFnPattern = this.pattern;
    this.failureFn = [];
    this.failureFnIndexRects = [];
    this.failureFnCharRects = [];
    this.failureFnValueRects = [];
    for (i = 0; i < this.pattern.length; ++i) {
        this.failureFn.push(undefined);
        this.failureFnIndexRects.push(this.newCharBox(i + "", i, 2));
        this.failureFnCharRects.push(this.newCharBox(this.pattern[i], i, 3));
        this.failureFnValueRects.push(this.newCharBox("-", i, 4));
    }
    if (this.pattern.length > 0) {
        this.cmd(
            "CreateLabel", this.newId(), "Failure function:",
            KMP.FAILURE_FN_LABEL_X,
            this.failureFnIndexRects[0].y - KMP.BOX_HEIGHT * 1.2,
            0);
        this.cmd(
            "CreateLabel", this.newId(), "j",
            this.failureFnIndexRects[0].x - KMP.BOX_WIDTH,
            this.failureFnIndexRects[0].y);
        this.cmd(
            "CreateLabel", this.newId(), "P[j]",
            this.failureFnCharRects[0].x - KMP.BOX_WIDTH,
            this.failureFnCharRects[0].y);
        this.cmd(
            "CreateLabel", this.newId(), "f(j)",
            this.failureFnValueRects[0].x - KMP.BOX_WIDTH,
            this.failureFnValueRects[0].y);
    }

    this.animationManager.StartNewAnimation(this.commands);
}

// Allocates a new graphics ID.
KMP.prototype.newId = function() {
    return this.nextId++;
}

// Increments the build-step comparison counter and updates the label.
KMP.prototype.incrementCounterBuild = function() {
    ++this.counterBuild;
    this.cmd("SetText", this.counterBuildId, "Build comparisons: " + this.counterBuild);
}

// Increments the search-step comparison counter and updates the label.
KMP.prototype.incrementCounterSearch = function() {
    ++this.counterSearch;
    this.cmd("SetText", this.counterSearchId, "Search comparisons: " + this.counterSearch);
}

// Sets the text displayed in the status label.
KMP.prototype.setStatus = function(msg) {
    this.cmd("SetText", this.statusId, msg);
}

// Creates a new rectangle that holds a string.
KMP.prototype.newCharBox = function(value, u, v) {
    var x = KMP.MARGIN_X + u * KMP.BOX_WIDTH;
    var y = KMP.MARGIN_Y + v * (KMP.BOX_HEIGHT + KMP.ROW_SPACING);
    var rect = {value: value, u: u, v: v};
    this.uvToXy(rect);
    return rect;
}

// Updates the x,y properties of a rectangle from its u,v properties, and
// moves its UI position to match.
KMP.prototype.uvToXy = function(rect) {
    rect.x =
        KMP.MARGIN_X +
        KMP.BOX_WIDTH * (rect.u + (rect.v === 1 ? this.patternShift : 0)) +
        (rect.v >= 2 ? KMP.LEFT_LABEL_MARGIN : 0);

    // The first two rows are close to each other with a little padding between.
    // Rows after that are set apart from the first two rows, and are touching.
    rect.y =
        KMP.MARGIN_Y +
        (rect.v < 2 ?
         rect.v * (KMP.BOX_HEIGHT + KMP.ROW_SPACING) :
         2 * (KMP.BOX_HEIGHT + KMP.ROW_SPACING) + rect.v * KMP.BOX_HEIGHT);

    if (rect.id !== undefined) {
        this.cmd("Move", rect.id, rect.x, rect.y);
    } else {
        rect.id = this.newId();
        this.cmd("CreateRectangle", rect.id, rect.value, KMP.BOX_WIDTH, KMP.BOX_HEIGHT, rect.x, rect.y);
    }
}

// Sets this.text, the pattern to be searched through, and recreates rects as
// necessary, using this.cmd (does *not* start an animation).
KMP.prototype.setText = function(text, doAnim) {
    var i;

    if (doAnim === undefined) {
        doAnim = true;
    }

    // We have to clear the old rects, if there are any; it doesn't make sense
    // to leave them around while we replace this.text.
    for (i = 0; i < this.textRects.length; ++i) {
        this.cmd("Delete", this.textRects[i].id);
    }

    this.text = text;
    this.textRects = [];
    if (doAnim) {
        for (i = 0; i < this.text.length; ++i) {
            this.textRects.push(this.newCharBox(this.text[i], i, 0));
        }
    }
}

// Updates the cursor position in the text,
KMP.prototype.setTextIndex = function(index, doAnim, doShift) {
    if (doAnim === undefined) {
        doAnim = true;
    }
    if (doShift === undefined) {
        doShift = true;
    }

    if (doAnim) {
        if (this.textIndex !== undefined && this.textIndex < this.text.length) {
            this.setActive(this.textRects[this.textIndex].id, false);
        }
    }
    this.textIndex = index;
    if (doAnim) {
        if (index < this.text.length) {
            this.setActive(this.textRects[this.textIndex].id, true);
        }
        if (doShift) {
            this.updateShift();
        }
    }
}

// Updates the cursor position in the pattern.
KMP.prototype.setPatternIndex = function(index, doAnim) {
    if (doAnim === undefined) {
        doAnim = true;
    }

    if (doAnim) {
        if (this.patternIndex !== undefined) {
            this.setActive(this.patternRects[this.patternIndex].id, false);
        }
    }
    this.patternIndex = index < this.pattern.length ? index : undefined;
    if (doAnim) {
        if (this.patternIndex !== undefined) {
            this.setActive(this.patternRects[this.patternIndex].id, true);
        }
        this.updateShift();
    }
}

// Sets the color of a graphics object to be active or not.
KMP.prototype.setActive = function(id, b) {
    this.cmd(
        "SetForegroundColor",
        id,
        b ? KMP.ACTIVE_FG_COLOR : KMP.NORMAL_FG_COLOR);
}

// Updates the pattern's rectangles' positions so that the active text and
// pattern characters are aligned.
KMP.prototype.updateShift = function() {
    var i;
    if (this.textIndex !== this.patternShift + this.patternIndex) {
        this.patternShift = this.textIndex - this.patternIndex;
        for (i = 0; i < this.pattern.length; ++i) {
            this.uvToXy(this.patternRects[i]);
        }
    }
}

// Changes the value of the failure function at a specific index.
KMP.prototype.setFailureFnValue = function(index, value, doAnim) {
    if (doAnim === undefined) {
        doAnim = true;
    }

    var id = this.failureFnValueRects[index].id;
    this.failureFn[index] = value;
    this.cmd("SetText", id, value);
    if (doAnim) {
        this.cmd("SetHighlight", id, 1);
        this.cmd("Step");
        this.cmd("SetHighlight", id, 0);
    }
}

KMP.prototype.buildFailureFnWrapper = function() {
    this.reset();
    if (this.pattern === "") {
        return;
    }
    this.commands = [];
    this.implementAction(this.buildFailureFn.bind(this));
}

// Builds the KMP failure function.
KMP.prototype.buildFailureFn = function(doAnim) {
    if (doAnim === undefined) {
        doAnim = true;
    }

    this.counterBuild = -1;
    this.incrementCounterBuild();
    this.setText(this.pattern, doAnim);  // this.text === this.pattern for this whole function!
    this.setFailureFnValue(0, 0, doAnim);
    if (doAnim) {
        this.cmd("Step");
    }
    this.setTextIndex(1, doAnim, false);
    this.setPatternIndex(0, doAnim);
    if (doAnim) {
        this.cmd("Step");
    }

    while (this.textIndex < this.text.length) {
        var match = this.text[this.textIndex] == this.pattern[this.patternIndex];
        this.incrementCounterBuild();

        if (doAnim) {
            if (match) {
                this.setStatus("Match.  Setting failure function and advancing.");
                this.cmd("Step");
            } else if (this.patternIndex > 0) {
                this.setStatus("Mismatch.  Consulting failure function.");
                this.cmd("Step");
                this.cmd("SetHighlight", this.patternRects[this.patternIndex - 1].id, 1);
                this.cmd("SetHighlight", this.failureFnValueRects[this.patternIndex - 1].id, 1);
                this.cmd("Step");
                this.setStatus("");
                this.cmd("SetHighlight", this.patternRects[this.patternIndex - 1].id, 0);
                this.cmd("SetHighlight", this.failureFnValueRects[this.patternIndex - 1].id, 0);
            } else {
                this.setStatus("Mismatch.  Setting failure function to 0 and shifting one position.");
                this.cmd("Step");
            }
        }

        if (match) {
            this.setFailureFnValue(this.textIndex, this.patternIndex + 1, doAnim);
            this.setTextIndex(this.textIndex + 1, doAnim, false);
            this.setPatternIndex(this.patternIndex + 1, doAnim);
        } else if (this.patternIndex > 0) {
            this.setPatternIndex(this.failureFn[this.patternIndex - 1], doAnim);
        } else {
            this.setFailureFnValue(this.textIndex, 0, doAnim);
            this.setTextIndex(this.textIndex + 1, doAnim);
        }

        if (doAnim) {
            this.setStatus("");
            this.cmd("Step");
        }
    }

    if (doAnim) {
        this.setStatus("Failure function built.");
    }

    return this.commands;
}

KMP.prototype.searchWrapper = function() {
    this.reset();
    if (this.pattern === "") {
        return;
    }
    this.commands = [];
    this.implementAction(this.search.bind(this));
}

// Implements the KMP search algorithm.
KMP.prototype.search = function() {
    var found = false;

    this.buildFailureFn(false);

    this.counterSearch = -1;
    this.incrementCounterSearch();
    this.setText(this.txtText.value);
    this.setTextIndex(0, true, false);
    this.setPatternIndex(0);
    this.cmd("Step");

    while (this.textIndex < this.text.length) {
        var match = this.text[this.textIndex] === this.pattern[this.patternIndex];
        this.incrementCounterSearch();

        if (match) {
            this.setStatus("Match.  Advancing.");
            this.cmd("Step");
        } else if (this.patternIndex > 0) {
            this.setStatus("Mismatch.  Consulting failure function.");
            this.cmd("Step");
            this.cmd("SetHighlight", this.patternRects[this.patternIndex - 1].id, 1);
            this.cmd("SetHighlight", this.failureFnValueRects[this.patternIndex - 1].id, 1);
            this.cmd("Step");
            this.setStatus("");
            this.cmd("SetHighlight", this.patternRects[this.patternIndex - 1].id, 0);
            this.cmd("SetHighlight", this.failureFnValueRects[this.patternIndex - 1].id, 0);
        } else {
            this.setStatus("Mismatch.  Shifting pattern one position.");
            this.cmd("Step");
        }

        if (match) {
            if (this.patternIndex === this.pattern.length - 1) {
                found = true;
                break;
            }
            this.setTextIndex(this.textIndex + 1, true, false);
            this.setPatternIndex(this.patternIndex + 1);
        } else if (this.patternIndex > 0) {
            this.setPatternIndex(this.failureFn[this.patternIndex - 1]);
        } else {
            this.setTextIndex(this.textIndex + 1);
        }

        this.setStatus("");
        this.cmd("Step");
    }

    if (found) {
        this.setStatus("Pattern found in text.");
    } else {
        this.setStatus("Pattern not found in text.");
    }

    return this.commands;
}

var currentAlg;

function init() {
    var animManag = initCanvas();
    currentAlg = new KMP(animManag, canvas.width, canvas.height);
}
