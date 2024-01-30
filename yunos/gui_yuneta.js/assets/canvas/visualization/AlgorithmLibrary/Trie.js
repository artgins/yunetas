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

function Trie(am, w, h) {
    this.init(am, w, h);
}

Trie.prototype = new Algorithm();
Trie.prototype.constructor = Trie;
Trie.superclass = Algorithm.prototype;

Trie.MARGIN_X = 40;
Trie.MARGIN_Y = 40;
Trie.STATUS_LABEL_X = 20;
Trie.STATUS_LABEL_Y = 20;
Trie.NODE_WIDTH = 25;
Trie.NODE_HEIGHT = 25;
Trie.NODE_SPACING_X = 30;
Trie.NODE_SPACING_Y = 35;

Trie.NORMAL_FG_COLOR = "#000";
Trie.EOW_FG_COLOR = "#00f";

// Sadly wrong outside of the Unicode BMP.
Trie.ordCompare = function(char1, char2) {
    return char1.charCodeAt(0) - char2.charCodeAt(0);
}

Trie.prototype.init = function(am, w, h) {
    Trie.superclass.init.call(this, am, w, h);
    this.addControls();
    this.reset();
}

Trie.prototype.addControls = function() {
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
    this.controls.push(this.txtKey);
}

Trie.prototype.disableUI = function(event) {
    this.setEnabled(false);
}

Trie.prototype.enableUI = function(event) {
    this.setEnabled(true);
}

Trie.prototype.setEnabled = function(b) {
    for (var i = 0; i < this.controls.length; ++i) {
        this.controls[i].disabled = !b;
    }
}

Trie.prototype.reset = function() {
    this.nextId = 0;

    this.animationManager.resetAll();
    this.clearHistory();

    this.commands = [];

    this.statusId = this.newId();
    this.cmd(
        "CreateLabel", this.statusId, "Ready.", Trie.STATUS_LABEL_X, Trie.STATUS_LABEL_Y, 0);

    this.root = this.newNode("");
    this.redrawTree();

    this.animationManager.StartNewAnimation(this.commands);
}

// Allocates a new graphics ID.
Trie.prototype.newId = function() {
    return this.nextId++;
}

Trie.prototype.newNode = function(value, parent) {
    return {
        value: value,
        eow: false,
        parent: parent,
        childValuesSorted: [],
        children: {},

        // Visualization properties.
        id: undefined,
        width: undefined,
        x: undefined,
        y: undefined
    };
}

Trie.prototype.addNodeChild = function(node, child) {
    node.childValuesSorted.push(child.value);
    node.childValuesSorted.sort(Trie.ordCompare);
    node.children[child.value] = child;
}

Trie.prototype.removeNode = function(node) {
    var parent = node.parent;
    if (parent === undefined) {
        throw new Error("removeNode: Can't remove the root node.");
    }
    if (node.childValuesSorted.length !== 0) {
        throw new Error("removeNode: Can't remove node with children.");
    }

    this.cmd("Disconnect", parent.id, node.id);
    this.cmd("Delete", node.id);

    delete parent.children[node.value];
    parent.childValuesSorted =
        parent.childValuesSorted.filter(function(ch) { return ch !== node.value; });

    return parent;
}

Trie.prototype.setNodeEow = function(node, eow) {
    node.eow = eow;
    this.cmd("SetForegroundColor", node.id, eow ? Trie.EOW_FG_COLOR : Trie.NORMAL_FG_COLOR);
}

// Sets the text displayed in the status label.
Trie.prototype.setStatus = function(msg) {
    this.cmd("SetText", this.statusId, msg);
}

// Performs all the steps to update the visualization based on the current trie
// state.
Trie.prototype.redrawTree = function() {
    this.calculateWidth(this.root);
    this.repositionTree();
    this.realizePositions(this.root);
}

// Calculates the widths of the given node and all descendents, with a
// post-order traversal.
Trie.prototype.calculateWidth = function(node) {
    var i, ch;
    var width = 0;
    for (i = 0; i < node.childValuesSorted.length; ++i) {
        ch = node.childValuesSorted[i];
        this.calculateWidth(node.children[ch]);
        width += node.children[ch].width;
    }
    width += Math.max(0, node.childValuesSorted.length - 1) * Trie.NODE_SPACING_X;
    width = Math.max(Trie.NODE_WIDTH, width);

    node.width = width;
    return width;
}

// Calculates positions for all of the nodes in the tree, using their
// already-calculated widths.
Trie.prototype.repositionTree = function() {
    this.root.x = canvas.width / 2;
    this.root.y = Trie.MARGIN_Y + Trie.NODE_HEIGHT / 2;
    this.repositionChildren(this.root);
}

// Calculates positions for all of the children underneath the node, based on
// the node's position, using their already-calculated widths.  Centers the
// children underneath the node.
Trie.prototype.repositionChildren = function(node) {
    var x = node.x - node.width / 2;
    var y = node.y + Trie.NODE_SPACING_Y + Trie.NODE_HEIGHT;
    var i, child;
    for (i = 0; i < node.childValuesSorted.length; ++i) {
        child = node.children[node.childValuesSorted[i]];
        child.x = x + child.width / 2;
        child.y = y;
        this.repositionChildren(child);
        x += child.width + Trie.NODE_SPACING_X;
    }
}

// Applies the positions of the given node and all of its descendents to the
// visualization.
Trie.prototype.realizePositions = function(node) {
    var i;
    if (node.id === undefined) {
        node.id = this.newId();
        this.cmd("CreateCircle", node.id, node.value, node.x, node.y);
        this.setNodeEow(node, node.eow);
        if (node.parent !== undefined) {
            this.cmd("Connect", node.parent.id, node.id);
        }
    } else {
        this.cmd("Move", node.id, node.x, node.y);
    }
    for (i = 0; i < node.childValuesSorted.length; ++i) {
        this.realizePositions(node.children[node.childValuesSorted[i]]);
    }
}

Trie.prototype.lookupWrapper = function(event) {
    var key = this.txtKey.value;
    this.txtKey.value = "";
    this.implementAction(this.lookup.bind(this), key);
}

Trie.prototype.insertWrapper = function(event) {
    var key = this.txtKey.value;
    this.txtKey.value = "";
    this.implementAction(this.insert.bind(this), key);
}

Trie.prototype.removeWrapper = function(event) {
    var key = this.txtKey.value;
    this.txtKey.value = "";
    this.implementAction(this.remove.bind(this), key);
}

Trie.prototype.lookup = function(key) {
    var node = this.root;
    var fullKeyPresent = true;
    var i, ch, child;

    this.commands = [];

    this.cmd("SetHighlight", node.id, 1);

    for (i = 0; i < key.length; ++i) {
        ch = key[i];
        child = node.children[ch];

        this.setStatus("Looking for child '" + ch + "'.");
        this.cmd("Step");
        if (child === undefined) {
            fullKeyPresent = false;
            break;
        }

        this.setStatus("");
        this.cmd("SetHighlight", node.id, 0);
        node = child;
        this.cmd("SetHighlight", node.id, 1);
        this.cmd("Step");
    }

    if (!fullKeyPresent) {
        this.setStatus("Lookup failed, '" + ch + "' not found.");
    } else if (node.eow) {
        this.setStatus("Lookup successful.");
    } else {
        this.setStatus("Lookup failed, end-of-word marker not present.");
    }

    this.cmd("Step");
    this.setStatus("");
    this.cmd("SetHighlight", node.id, 0);

    return this.commands;
}

Trie.prototype.insert = function(key) {
    var node = this.root;
    var i, ch, child;

    this.commands = [];

    for (i = 0; i < key.length; ++i) {
        ch = key[i];
        child = node.children[ch];

        this.setStatus("Looking for child '" + ch + "'.");
        this.cmd("SetHighlight", node.id, 1);
        this.cmd("Step");
        this.cmd("SetHighlight", node.id, 0);

        if (child === undefined) {
            this.setStatus("'" + ch + "' not found, creating child.");
            child = this.newNode(ch, node);
            this.addNodeChild(node, child);
            this.redrawTree();
            this.cmd("SetHighlight", child.id, 1);
            this.cmd("Step");
            this.cmd("SetHighlight", child.id, 0);
        }

        node = child;
    }

    this.setStatus("Setting end-of-word marker.");
    this.cmd("SetHighlight", node.id, 1);
    this.setNodeEow(node, true);
    this.cmd("Step");
    this.setStatus("");
    this.cmd("SetHighlight", node.id, 0);

    return this.commands;
}

Trie.prototype.remove = function(key) {
    var node = this.root;
    var found = true;
    var i, child;

    this.commands = [];

    this.setStatus("Looking up \"" + key + "\".");
    this.cmd("Step");
    this.cmd("SetHighlight", node.id, 1);

    for (i = 0; i < key.length; ++i) {
        ch = key[i];
        child = node.children[ch];

        this.setStatus("Looking for child '" + ch + "'.");
        this.cmd("Step");

        if (child === undefined) {
            found = false;
            break;
        }

        this.cmd("SetHighlight", node.id, 0);
        node = child;
        this.cmd("SetHighlight", node.id, 1);
    }

    if (!found) {
        this.setStatus("'" + ch + "' not found, key not present in trie.");
        this.cmd("Step");
    } else if (!node.eow) {
        this.setStatus("End-of-word marker missing, key not present in trie.");
        this.cmd("Step");
    } else {
        this.setStatus("Removing end-of-word marker from node.");
        this.setNodeEow(node, false);
        this.cmd("Step");

        this.cmd("SetHighlight", node.id, 0);
        this.setStatus("Removing unnecessary nodes from trie.");
        this.cmd("Step");
        this.setStatus("");
        this.cmd("SetHighlight", node.id, 1);
        this.cmd("Step");

        if (node.childValuesSorted.length > 0) {
            this.setStatus("Non-leaf node, can't remove it.");
            this.cmd("Step");
        } else {
            while (!node.eow && node.childValuesSorted.length === 0 && node.parent !== undefined) {
                this.setStatus("Leaf node for incomplete word, removing it.");
                this.cmd("SetHighlight", node.id, 0);
                node = this.removeNode(node);
                this.redrawTree();
                this.cmd("SetHighlight", node.id, 1);
                this.cmd("Step");
            }

            if (node.parent === undefined) {
                this.setStatus("At root, can't remove further.");
            } else if (node.eow) {
                this.setStatus("Node is a complete word, can't remove.");
            } else if (node.childValuesSorted.length !== 0) {
                this.setStatus("Node has children, can't remove.");
            } else {
                // From the termination cases of the while loop above, this
                // case shouldn't happen.  It's here for safety.
                this.setStatus("Can't remove.");
            }
            this.cmd("Step");
        }
    }

    this.setStatus("");
    this.cmd("SetHighlight", node.id, 0);
    return this.commands;
}

var currentAlg;

function init() {
    var animManag = initCanvas();
    currentAlg = new Trie(animManag, canvas.width, canvas.height);
}
