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
// p, q, r: Nodes.
// u, v: Node grid coordinates (top-left is 0,0).
// x, y: Canvas pixel coordinates (top-left is 0,0).
//
// A node is a plain Javascript object with the following fields:
//
// - key (string or false or true):
//     Booleans represent negative and positive infinity, respectively.
//
// - up (node or undefined)
// - down (node or undefined)
// - left (node or undefined)
// - right (node or undefined)
//
// - id (graphics object id)
// - u, v, x, y (number): As above.

function SkipList(am, w, h) {
    this.init(am, w, h);
}

SkipList.prototype = new Algorithm();
SkipList.prototype.constructor = SkipList;
SkipList.superclass = Algorithm.prototype;

SkipList.KEY_LENGTH = 4;
SkipList.MARGIN_X = 40;
SkipList.MARGIN_Y = 60;
SkipList.STATUS_LABEL_X = 100;
SkipList.STATUS_LABEL_Y = 20;
SkipList.SPACING_X = 70;
SkipList.SPACING_Y = 70;

SkipList.NORMAL_FG_COLOR = "#000";
SkipList.ACTIVE_FG_COLOR = "#00f";

// Returns a random boolean, reading from the coin flip text box if possible.
SkipList.prototype.randomBool = function() {
    var ch;

    while (this.txtRandom.value !== "") {
        ch = this.txtRandom.value[0];
        this.txtRandom.value = this.txtRandom.value.substr(1);
        if (ch === '1' || ch === '0') {
            break;
        }
    }

    if (ch === '0') {
        return false;
    } else if (ch === '1') {
        return true;
    }
    var n = Math.random();
    return n < 0.5;
}

// Compares two keys numerically, including false (-infinity) and true
// (+infinity).
SkipList.keyCmp = function(k1, k2) {
    if (k1 === k2) {
        return 0;
    } else if (k1 === false) {
        return -1;
    } else if (k1 === true) {
        return 1;
    } else if (k2 === false) {
        return 1;
    } else if (k2 === true) {
        return -1;
    } else if (k1 < k2) {
        return -1;
    } else if (k1 > k2) {
        return 1;
    } else {
        throw new Error(
            "SkipList.keyCmp: Can't compare " + k1 + " (" + typeof(k1) +
            ") and " + k2 + " (" + typeof(k2) + ").");
    }
}

// Returns the visual string for a key, including false (-infinity) and true
// (+infinity).
SkipList.keyToString = function(key) {
    if (key === false) {
        return "-inf";
    } else if (key === true) {
        return "+inf";
    } else {
        return key + "";
    }
}

// Returns the far-right node on a row.
SkipList.endOfRow = function(p) {
    while (p.right !== undefined) {
        p = p.right;
    }
    return p;
}

// Updates the x,y properties of a node from its u,v properties, and
// moves its UI position to match.
SkipList.prototype.uvToXy = function(p) {
    var xOld = p.x;
    var yOld = p.y;
    p.x = SkipList.MARGIN_X + SkipList.SPACING_X * p.u;
    p.y = SkipList.MARGIN_Y + SkipList.SPACING_Y * p.v;
    if (p.x !== xOld || p.y !== yOld) {
        if (p.id === undefined) {
            p.id = this.newId();
            this.cmd("CreateCircle", p.id, SkipList.keyToString(p.key), p.x, p.y);
        } else {
            this.cmd("Move", p.id, p.x, p.y);
        }
    }
}

SkipList.prototype.init = function(am, w, h) {
    SkipList.superclass.init.call(this, am, w, h);
    this.addControls();
    this.reset();
}

SkipList.prototype.addControls = function() {
    this.controls = [];

    this.btnLookup = addControlToAlgorithmBar("Button", "Lookup");
    this.btnLookup.onclick = this.lookupWrapper.bind(this);
    this.controls.push(this.btnLookup);

    this.btnInsert = addControlToAlgorithmBar("Button", "Insert");
    this.btnInsert.onclick = this.insertWrapper.bind(this);
    this.controls.push(this.btnInsert);

    this.btnRemove = addControlToAlgorithmBar("Button", "Remove");
    this.btnRemove.onclick = this.removeWrapper.bind(this);
    this.controls.push(this.btnRemove);

    this.lblKey = addLabelToAlgorithmBar("Key:");
    this.txtKey = addControlToAlgorithmBar("Text", "");
    this.txtKey.onkeydown = this.returnSubmit(
        this.txtKey,
        this.insertWrapper.bind(this),
        SkipList.KEY_LENGTH,
        true /* digits only */);
    this.controls.push(this.txtKey);

    this.lblRandom = addLabelToAlgorithmBar("Coin flips (e.g. 110100):");
    this.txtRandom = addControlToAlgorithmBar("Text", "");
    this.controls.push(this.txtRandom);
}

SkipList.prototype.disableUI = function(event) {
    this.setEnabled(false);
}

SkipList.prototype.enableUI = function(event) {
    this.setEnabled(true);
}

SkipList.prototype.setEnabled = function(b) {
    for (var i = 0; i < this.controls.length; ++i) {
        this.controls[i].disabled = !b;
    }
}

SkipList.prototype.reset = function() {
    this.nextId = 0;
    this.size = 0;
    this.root = {key: false, u: 0, v: 0, right: {key: true, u: 1, v: 0}};
    this.root.right.left = this.root;

    this.commands = [];
    this.statusId = this.newId();
    this.cmd(
        "CreateLabel", this.statusId, "Ready.",
        SkipList.STATUS_LABEL_X, SkipList.STATUS_LABEL_Y, 0);
    this.uvToXy(this.root);
    this.uvToXy(this.root.right);
    this.connect(this.root, this.root.right);
    this.animationManager.StartNewAnimation(this.commands);
}

// Allocates a new graphics ID.
SkipList.prototype.newId = function() {
    return this.nextId++;
}

// Creates a new with the given key, at the given position.
SkipList.prototype.newNode = function(key, u, v) {
    var p = {
        key: key,
        u: u,
        v: v
    };
    this.uvToXy(p);
    return p;
}

// Increments the v (row) of all nodes.
SkipList.prototype.shiftAllNodesDown = function() {
    var p, q;
    for (p = this.root; p !== undefined; p = p.down) {
        for (q = p; q !== undefined; q = q.right) {
            ++q.v;
            this.uvToXy(q);
        }
    }
}

// Connects two nodes with an edge.
SkipList.prototype.connect = function(p, q, active) {
    this.disconnect(p, q);
    this.cmd(
        "Connect", p.id, q.id,
        active ? SkipList.ACTIVE_FG_COLOR : SkipList.NORMAL_FG_COLOR,
        0 /* curviness */, 0 /* directed */);
}

// Removes an edge between two nodes, if there is one.
SkipList.prototype.disconnect = function(p, q) {
    this.cmd("Disconnect", p.id, q.id);
};

// Displays a status message.
SkipList.prototype.setMessage = function(str) {
    this.cmd("SetText", this.statusId, str);
}

// Displays a status message for a brief pause then clears it.
SkipList.prototype.message = function(str) {
    this.setMessage(str);
    this.cmd("Step");
    this.setMessage("");
}

// Makes the given ID flash temporarily.
SkipList.prototype.pulseId = function(id) {
    this.pulseIds([id]);
}

// Makes the given IDs flash temporarily.
SkipList.prototype.pulseIds = function(ids) {
    var i;
    for (i = 0; i < ids.length; ++i) {
        this.cmd("SetHighlight", ids[i], 1);
    }
    this.cmd("Step");
    for (i = 0; i < ids.length; ++i) {
        this.cmd("SetHighlight", ids[i], 0);
    }
}

SkipList.prototype.lookupWrapper = function(event) {
    var key = this.normalizeNumber(this.txtKey.value, SkipList.KEY_LENGTH);

    if (key !== "") {
        this.txtKey.value = "";
        this.implementAction(this.lookup.bind(this), key);
    }
}

SkipList.prototype.insertWrapper = function(event) {
    var key = this.normalizeNumber(this.txtKey.value, SkipList.KEY_LENGTH);

    if (key !== "") {
        this.txtKey.value = "";
        this.implementAction(this.insert.bind(this), key);
    }
}

SkipList.prototype.removeWrapper = function(event) {
    var key = this.normalizeNumber(this.txtKey.value, SkipList.KEY_LENGTH);

    if (key !== "") {
        this.txtKey.value = "";
        this.implementAction(this.remove.bind(this), key);
    }
}

// Performs the skip list search algorithm, setting this.currentNode to the
// final node visited.
SkipList.prototype.search = function(key) {
    var p, moveRight;

    this.commands = [];
    this.message("Searching for " + key + ".");

    p = this.root;
    while (true) {
        // Scan right.
        while (true) {
            // Highlight the current node and the one to the right that we are comparing.
            this.cmd("SetForegroundColor", p.id, SkipList.ACTIVE_FG_COLOR);
            this.cmd("SetHighlight", p.id, 1);
            this.cmd("Step");

            moveRight = SkipList.keyCmp(p.right.key, key) <= 0;
            this.message(
                moveRight ?
                "Key to right <= target; moving right." :
                "Key to right > target; moving down.");
            this.cmd("Step");
            this.cmd("SetHighlight", p.id, 0);
            if (!moveRight) {
                break;
            }
            this.connect(p, p.right, true);
            p = p.right;
        }

        // If we can move down, do so.  Otherwise, we're done.
        if (p.down === undefined) {
            break;
        }
        this.connect(p, p.down, true);
        p = p.down;
    }

    this.message("Can't move down; search finished.");
    this.currentNode = p;
    return this.commands;
}

// Clears the active colors on nodes visited during the search algorithm.
SkipList.prototype.clearActiveColors = function() {
    var p, q;
    for (p = this.root; p !== undefined; p = p.down) {
        for (q = p; q !== undefined; q = q.right) {
            this.cmd("SetForegroundColor", q.id, SkipList.NORMAL_FG_COLOR);
            if (q.right !== undefined) {
                this.connect(q, q.right);
            }
            if (q.down !== undefined) {
                this.connect(q, q.down);
            }
        }
    }
}

// Searches for an element in the skip list, displaying the result to the user.
SkipList.prototype.lookup = function(key) {
    this.search(key);

    this.cmd("SetHighlight", this.currentNode.id, 1);
    this.message(
        SkipList.keyCmp(this.currentNode.key, key) === 0 ?
        "Key found." :
        "Key not found.");
    this.cmd("SetHighlight", this.currentNode.id, 0);
    this.clearActiveColors();

    return this.commands;
}

// Inserts a key into the skip list.
SkipList.prototype.insert = function(key) {
    var p, q, r, u, newRoot;
    var keepGoing;

    this.search(key);

    p = this.currentNode;
    if (SkipList.keyCmp(key, p.key) === 0) {
        this.cmd("SetHighlight", p.id, 1);
        this.message("Key is already present.");
        this.cmd("SetHighlight", p.id, 0);
    } else {
        this.message("Inserting the key.");
        this.disconnect(p, p.right);
        ++this.size;
        u = p.u + 1;

        // Shift all entries right of p to the right one space.
        for (q = p.right; q !== undefined; q = q.right) {
            for (r = q; r !== undefined; r = r.up) {
                ++r.u;
                this.uvToXy(r);
            }
        }
        this.cmd("Step");

        // Walk up the tree, inserting nodes.
        // p is the node to the left of the node we're inserting.
        r = undefined;  // r is the node below the one we're inserting.
        while (true) {
            // Insert the node at the current level.
            // q is the node we're inserting.
            this.disconnect(p, p.right);
            q = this.newNode(key, u, p.v);
            (q.right = p.right).left = q;
            (q.left = p).right = q;
            if (r !== undefined) {
                (q.down = r).up = q;
                this.connect(q, q.down);
            }
            this.connect(q.left, q);
            this.connect(q, q.right);
            this.pulseId(q.id);

            // Flip a coin to see if we should insert above the current level.
            keepGoing = this.randomBool();
            this.message("Coin flip to insert above: " + keepGoing);
            if (!keepGoing) {
                break;
            }

            // Move p left until we can move it up.
            while (true) {
                this.pulseId(p.id);
                if (p.up !== undefined) {
                    break;
                } else if (p.left !== undefined) {
                    p = p.left;
                } else {
                    // We've reached the top left corner of the skip list.
                    // Insert a new layer above to keep going.
                    this.shiftAllNodesDown();
                    this.cmd("Step");
                    newRoot = this.newNode(false, 0, 0);
                    newRoot.right = this.newNode(true, this.size - 1 + 2 /* infs */, 0);
                    newRoot.right.left = newRoot;
                    (newRoot.down = p).up = newRoot;
                    (newRoot.right.down = SkipList.endOfRow(p)).up = newRoot.right;
                    this.connect(newRoot, newRoot.right);
                    this.connect(newRoot, newRoot.down);
                    this.connect(newRoot.right, newRoot.right.down);
                    this.root = newRoot;
                    this.cmd("Step");
                    break;
                }
            }
            r = q;
            p = p.up;
            this.pulseId(p.id);
        }
    }

    this.clearActiveColors();
    return this.commands;
}

// Removes a key from the skip list.
SkipList.prototype.remove = function(key) {
    var p, q;

    this.search(key);

    p = this.currentNode;
    if (SkipList.keyCmp(key, p.key) !== 0) {
        this.message("Key not found, can't remove.");
    } else {
        this.setMessage("Removing.");

        q = p.right;

        while (p !== undefined) {
            this.pulseId(p.id);
            p.left.right = p.right;
            p.right.left = p.left;
            this.cmd("Delete", p.id);
            this.connect(p.left, p.right);
            p = p.up;
        }

        for (; q !== undefined; q = q.right) {
            for (p = q; p !== undefined; p = p.up) {
                --p.u;
                this.uvToXy(p);
            }
        }

        this.setMessage("");
    }

    this.clearActiveColors();
    return this.commands;
}

var currentAlg;

function init() {
    var animManag = initCanvas();
    currentAlg = new SkipList(animManag, canvas.width, canvas.height);
}
