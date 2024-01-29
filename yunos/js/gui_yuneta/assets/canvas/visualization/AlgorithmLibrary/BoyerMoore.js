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

function BoyerMoore(am, w, h) {
    this.init(am, w, h);
}

BoyerMoore.prototype = new Algorithm();
BoyerMoore.prototype.constructor = BoyerMoore;
BoyerMoore.superclass = Algorithm.prototype;

BoyerMoore.MARGIN_X = 40;
BoyerMoore.MARGIN_Y = 100;
BoyerMoore.LAST_LABEL_X = 20;
BoyerMoore.LAST_LABEL_Y = 20;
BoyerMoore.COUNTER_LABEL_X = 20;
BoyerMoore.COUNTER_LABEL_Y = 40;
BoyerMoore.STATUS_LABEL_X = 20;
BoyerMoore.STATUS_LABEL_Y = 60;
BoyerMoore.BOX_WIDTH = 30;
BoyerMoore.BOX_HEIGHT = 30;
BoyerMoore.ROW_SPACING = 20;

BoyerMoore.NORMAL_FG_COLOR = "#000";
BoyerMoore.ACTIVE_FG_COLOR = "#f00";

BoyerMoore.prototype.init = function(am, w, h) {
    BoyerMoore.superclass.init.call(this, am, w, h);
    this.addControls();
    this.reset();
}

BoyerMoore.prototype.addControls = function() {
    this.controls = [];

    this.btnReset = addControlToAlgorithmBar("Button", "Reset");
    this.btnReset.onclick = this.reset.bind(this);
    this.controls.push(this.btnReset);

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

BoyerMoore.prototype.disableUI = function(event) {
    this.setEnabled(false);
}

BoyerMoore.prototype.enableUI = function(event) {
    this.setEnabled(true);
}

BoyerMoore.prototype.setEnabled = function(b) {
    for (var i = 0; i < this.controls.length; ++i) {
        this.controls[i].disabled = !b;
    }
}

BoyerMoore.prototype.reset = function() {
    var i, lastKeys, lastStr;

    this.text = this.txtText.value;
    this.pattern = this.txtPattern.value;
    this.nextId = 0;

    this.animationManager.resetAll();
    this.clearHistory();

    this.commands = [];

    // Create the "last" function and a label showing its definition.
    this.last = {};
    for (i = 0; i < this.text.length; ++i) {
        this.last[this.text[i]] = -1;
    }
    for (i = this.pattern.length - 1; i >= 0; --i) {
        if (!this.last.hasOwnProperty(this.pattern[i]) ||
            this.last[this.pattern[i]] === -1) {
            this.last[this.pattern[i]] = i;
        }
    }
    lastKeys = Object.keys(this.last).sort();
    lastStr = "";
    for (i = 0; i < lastKeys.length; ++i) {
        lastStr =
            (lastStr === "" ? "Last: {" : lastStr + ", ") +
            (lastKeys[i] + ": " + this.last[lastKeys[i]]);
    }
    if (lastStr !== "") {
        lastStr += "}";
    }
    this.lastId = this.newId();
    this.cmd(
        "CreateLabel", this.lastId, lastStr, BoyerMoore.LAST_LABEL_X, BoyerMoore.LAST_LABEL_Y, 0);

    // Create a label for counting comparisons.
    this.counterId = this.newId();
    this.cmd(
        "CreateLabel", this.counterId, "",
        BoyerMoore.COUNTER_LABEL_X, BoyerMoore.COUNTER_LABEL_Y, 0);
    this.counter = 0;

    // Create a status label for displaying what the algorithm is doing.
    this.statusId = this.newId();
    this.cmd(
        "CreateLabel", this.statusId, "", BoyerMoore.STATUS_LABEL_X, BoyerMoore.STATUS_LABEL_Y, 0);

    // Create rectangles for displaying the text and pattern.
    this.textRects = [];
    for (i = 0; i < this.text.length; ++i) {
        this.textRects.push(this.newCharBox(this.text[i], i, 0));
    }
    this.patternRects = [];
    for (i = 0; i < this.pattern.length; ++i) {
        this.patternRects.push(this.newCharBox(this.pattern[i], i, 1));
    }

    this.textIndex = undefined;
    this.patternIndex = undefined;
    this.patternShift = 0;
    if (this.pattern.length > 0) {
        this.setTextIndex(this.pattern.length - 1, true);
        this.setPatternIndex(this.pattern.length - 1);
    }

    this.animationManager.StartNewAnimation(this.commands);
}

// Allocates a new graphics ID.
BoyerMoore.prototype.newId = function() {
    return this.nextId++;
}

// Increments the comparison counter and updates the label.
BoyerMoore.prototype.incrementCounter = function() {
    ++this.counter;
    this.cmd("SetText", this.counterId, "Comparisons: " + this.counter);
}

// Sets the text displayed in the status label.
BoyerMoore.prototype.setStatus = function(msg) {
    this.cmd("SetText", this.statusId, msg);
}

// Creates a new rectangle that holds a string.
BoyerMoore.prototype.newCharBox = function(value, u, v) {
    var id = this.newId();
    var x = BoyerMoore.MARGIN_X + u * BoyerMoore.BOX_WIDTH;
    var y = BoyerMoore.MARGIN_Y + v * (BoyerMoore.BOX_HEIGHT + BoyerMoore.ROW_SPACING);
    this.cmd("CreateRectangle", id, value, BoyerMoore.BOX_WIDTH, BoyerMoore.BOX_HEIGHT, x, y);
    return {id: id, u: u, v: v, x: x, y: y};
}

// Updates the x,y properties of a rectangle from its u,v properties, and
// moves its UI position to match.
BoyerMoore.prototype.uvToXy = function(rect) {
    rect.x = BoyerMoore.MARGIN_X +
        BoyerMoore.BOX_WIDTH * (rect.u + (rect.v === 1 ? this.patternShift : 0));
    rect.y = BoyerMoore.MARGIN_Y + rect.v * (BoyerMoore.BOX_HEIGHT + BoyerMoore.ROW_SPACING);
    this.cmd("Move", rect.id, rect.x, rect.y);
}

// Updates the cursor position in the text,
BoyerMoore.prototype.setTextIndex = function(index, inhibitShift) {
    if (this.textIndex !== undefined && this.textIndex < this.text.length) {
        this.setActive(this.textRects[this.textIndex].id, false);
    }
    this.textIndex = index;
    if (index < this.text.length) {
        this.setActive(this.textRects[this.textIndex].id, true);
    }
    if (!inhibitShift) {
        this.updateShift();
    }
}

// Updates the cursor position in the pattern.
BoyerMoore.prototype.setPatternIndex = function(index) {
    if (this.patternIndex !== undefined) {
        this.setActive(this.patternRects[this.patternIndex].id, false);
    }
    this.patternIndex = index;
    this.setActive(this.patternRects[this.patternIndex].id, true);
    this.updateShift();
}

// Sets the color of a graphics object to be active or not.
BoyerMoore.prototype.setActive = function(id, b) {
    this.cmd(
        "SetForegroundColor",
        id,
        b ? BoyerMoore.ACTIVE_FG_COLOR : BoyerMoore.NORMAL_FG_COLOR);
}

// Updates the pattern's rectangles' positions so that the active text and
// pattern characters are aligned.
BoyerMoore.prototype.updateShift = function() {
    var i;
    if (this.textIndex !== this.patternShift + this.patternIndex) {
        this.patternShift = this.textIndex - this.patternIndex;
        for (i = 0; i < this.pattern.length; ++i) {
            this.uvToXy(this.patternRects[i]);
        }
    }
}

BoyerMoore.prototype.searchWrapper = function() {
    this.reset();
    this.implementAction(this.search.bind(this));
}

// Implements the Boyer-Moore search algorithm.
BoyerMoore.prototype.search = function() {
    var found = false;
    var match;
    var shift;
    this.commands = [];

    if (this.text === "" || this.pattern === "") {
        return;
    }

    this.counter = -1;
    this.incrementCounter();
    this.cmd("Step");

    while (this.textIndex < this.text.length) {
        match = this.text[this.textIndex] === this.pattern[this.patternIndex];
        this.incrementCounter();

        this.cmd("SetHighlight", this.textRects[this.textIndex].id, 1);
        this.cmd("SetHighlight", this.patternRects[this.patternIndex].id, 1);
        if (match) {
            this.setStatus("Match.  Moving backward.");
        } else {
            shift = Math.max(1, this.patternIndex - this.last[this.text[this.textIndex]]);
            this.setStatus(
                "Mismatch.  last[" + this.text[this.textIndex] + "] = " +
                this.last[this.text[this.textIndex]] + ".  Shifting by " + shift + ".");
        }
        this.cmd("Step");
        this.cmd("Step");
        this.cmd("SetHighlight", this.textRects[this.textIndex].id, 0);
        this.cmd("SetHighlight", this.patternRects[this.patternIndex].id, 0);
        this.setStatus("");

        if (match) {
            if (this.patternIndex === 0) {
                found = true;
                break;
            } else {
                this.setTextIndex(this.textIndex - 1, true);
                this.setPatternIndex(this.patternIndex - 1);
            }
        } else {
            this.setTextIndex(
                this.textIndex + (this.pattern.length - this.patternIndex - 1) + shift,
                true);
            this.setPatternIndex(this.pattern.length - 1);
            this.cmd("Step");
        }
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
    currentAlg = new BoyerMoore(animManag, canvas.width, canvas.height);
}
