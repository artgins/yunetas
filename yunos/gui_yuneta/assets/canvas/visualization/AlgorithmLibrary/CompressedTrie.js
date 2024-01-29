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

function CompressedTrie(am, w, h) {
    this.init(am, w, h);
}

CompressedTrie.prototype = new Algorithm();
CompressedTrie.prototype.constructor = CompressedTrie;
CompressedTrie.superclass = Algorithm.prototype;

CompressedTrie.MARGIN_X = 40;
CompressedTrie.MARGIN_Y = 40;
CompressedTrie.STATUS_LABEL_X = 20;
CompressedTrie.STATUS_LABEL_Y = 20;
CompressedTrie.NODE_WIDTH = 25;
CompressedTrie.NODE_HEIGHT = 25;
CompressedTrie.NODE_SPACING_X = 30;
CompressedTrie.NODE_SPACING_Y = 35;

CompressedTrie.NORMAL_FG_COLOR = "#000";
CompressedTrie.EOW_FG_COLOR = "#00f";

// Results from matchNodeValue().
CompressedTrie.RESULT_MATCH = "RESULT_MATCH";
CompressedTrie.RESULT_MISMATCH = "RESULT_MISMATCH";
CompressedTrie.RESULT_KEY_IS_PREFIX = "RESULT_KEY_IS_PREFIX";
CompressedTrie.RESULT_NODE_VALUE_IS_PREFIX = "RESULT_NODE_VALUE_IS_PREFIX";

// Sadly wrong outside of the Unicode BMP.
CompressedTrie.ordCompare = function(str1, str2) {
    var i, cmp;
    var end = Math.min(str1.length, str2.length);
    for (i = 0; i < end; ++i) {
        cmp = str1.charCodeAt(i) - str2.charCodeAt(i);
        if (cmp !== 0) {
            return cmp;
        }
    }
    return str1.length - str2.length;
}

// Returns an object with 'result' set to RESULT_MATCH, RESULT_MISMATCH (with
// 'index' = the position of the first mismatched character),
// RESULT_KEY_IS_PREFIX, or RESULT_NODE_VALUE_IS_PREFIX.
CompressedTrie.matchNodeValue = function(nodeValue, key) {
    var i, cmp, lenDiff;
    var end = Math.min(nodeValue.length, key.length);
    for (i = 0; i < end; ++i) {
        cmp = nodeValue.charCodeAt(i) - key.charCodeAt(i);
        if (cmp !== 0) {
            return {result: CompressedTrie.RESULT_MISMATCH, index: i};
        }
    }

    lenDiff = nodeValue.length - key.length;
    if (lenDiff > 0) {
        return {result: CompressedTrie.RESULT_KEY_IS_PREFIX};
    } else if (lenDiff < 0) {
        return {result: CompressedTrie.RESULT_NODE_VALUE_IS_PREFIX};
    } else {
        return {result: CompressedTrie.RESULT_MATCH};
    }
}

CompressedTrie.prototype.init = function(am, w, h) {
    CompressedTrie.superclass.init.call(this, am, w, h);
    this.addControls();
    this.reset();
}

CompressedTrie.prototype.addControls = function() {
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

CompressedTrie.prototype.disableUI = function(event) {
    this.setEnabled(false);
}

CompressedTrie.prototype.enableUI = function(event) {
    this.setEnabled(true);
}

CompressedTrie.prototype.setEnabled = function(b) {
    for (var i = 0; i < this.controls.length; ++i) {
        this.controls[i].disabled = !b;
    }
}

CompressedTrie.prototype.reset = function() {
    this.nextId = 0;

    this.animationManager.resetAll();
    this.clearHistory();

    this.commands = [];

    this.statusId = this.newId();
    this.cmd(
        "CreateLabel", this.statusId, "Ready.",
        CompressedTrie.STATUS_LABEL_X, CompressedTrie.STATUS_LABEL_Y, 0);

    this.root = this.newNode("");
    this.redrawTree();

    this.animationManager.StartNewAnimation(this.commands);
}

// Allocates a new graphics ID.
CompressedTrie.prototype.newId = function() {
    return this.nextId++;
}

CompressedTrie.prototype.newNode = function(value, parent) {
    return {
        value: value,
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

CompressedTrie.prototype.setNodeValue = function(node, value) {
    node.value = value;
    this.cmd("SetText", node.id, node.value);
}

CompressedTrie.prototype.addNodeChild = function(node, child) {
    var ch = child.value[0];
    node.childValuesSorted.push(ch);
    node.childValuesSorted.sort(CompressedTrie.ordCompare);
    node.children[ch] = child;
}

// Splits the "tail" of a node whose value has >=2 characters into a new child
// node, and moves the node's existing children under the new child.  The node's
// value keeps characters before the given index, and the new child holds the
// remaining characters.
CompressedTrie.prototype.splitNode = function(node, index) {
    var i, ch, child, grandchild;

    if (!(0 < index && index < node.value.length)) {
        throw new Error(
            "splitNode: Invalid index " + index + ", should be in (0, " +
                node.value.length + "), node.value == \"" + node.value + "\".");
    }

    // Create a new child that will go under 'node'.
    child = this.newNode(node.value.substr(index), node);

    // Move children from 'node' to 'child'.  We don't use addNodeChild to avoid
    // sorting the child values list over and over.
    for (i = 0; i < node.childValuesSorted.length; ++i) {
        ch = node.childValuesSorted[i]
        grandchild = node.children[ch];
        grandchild.parent = child;
        child.children[ch] = grandchild;
        child.childValuesSorted.push(ch);
    }
    child.childValuesSorted.sort(CompressedTrie.ordCompare);

    // Reset 'node' to only have 'child'.
    node.children = {};
    node.childValuesSorted = [];
    this.addNodeChild(node, child);

    // Truncate the value in 'node'.
    this.setNodeValue(node, node.value.substr(0, index));

    // Create a UI object for 'child'...
    this.redrawTree();
    // ...then fix up edges.
    for (i = 0; i < child.childValuesSorted.length; ++i) {
        grandchild = child.children[child.childValuesSorted[i]];
        this.cmd("Disconnect", node.id, grandchild.id);
        this.cmd("Connect", child.id, grandchild.id);
    }
}

CompressedTrie.prototype.removeNode = function(node) {
    var parent = node.parent;
    var ch;

    if (parent === undefined) {
        throw new Error("removeNode: Can't remove the root node.");
    }

    if (node.childValuesSorted.length !== 0) {
        throw new Error("removeNode: Can't remove node with children.");
    }

    this.cmd("Disconnect", parent.id, node.id);
    this.cmd("Delete", node.id);

    ch = node.value[0];
    delete parent.children[ch];
    parent.childValuesSorted =
        parent.childValuesSorted.filter(function(c) { return c != ch; });

    return parent;
}

// Sets the text displayed in the status label.
CompressedTrie.prototype.setStatus = function(msg) {
    this.cmd("SetText", this.statusId, msg);
}

// Performs all the steps to update the visualization based on the current trie
// state.
CompressedTrie.prototype.redrawTree = function() {
    this.calculateWidth(this.root);
    this.repositionTree();
    this.realizePositions(this.root);
}

// Calculates the widths of the given node and all descendents, with a
// post-order traversal.
CompressedTrie.prototype.calculateWidth = function(node) {
    var i, ch;
    var width = 0;
    for (i = 0; i < node.childValuesSorted.length; ++i) {
        ch = node.childValuesSorted[i];
        this.calculateWidth(node.children[ch]);
        width += node.children[ch].width;
    }
    width += Math.max(0, node.childValuesSorted.length - 1) * CompressedTrie.NODE_SPACING_X;
    width = Math.max(CompressedTrie.NODE_WIDTH, width);

    node.width = width;
    return width;
}

// Calculates positions for all of the nodes in the tree, using their
// already-calculated widths.
CompressedTrie.prototype.repositionTree = function() {
    this.root.x = canvas.width / 2;
    this.root.y = CompressedTrie.MARGIN_Y + CompressedTrie.NODE_HEIGHT / 2;
    this.repositionChildren(this.root);
}

// Calculates positions for all of the children underneath the node, based on
// the node's position, using their already-calculated widths.  Centers the
// children underneath the node.
CompressedTrie.prototype.repositionChildren = function(node) {
    var x = node.x - node.width / 2;
    var y = node.y + CompressedTrie.NODE_SPACING_Y + CompressedTrie.NODE_HEIGHT;
    var i, child;
    for (i = 0; i < node.childValuesSorted.length; ++i) {
        child = node.children[node.childValuesSorted[i]];
        child.x = x + child.width / 2;
        child.y = y;
        this.repositionChildren(child);
        x += child.width + CompressedTrie.NODE_SPACING_X;
    }
}

// Applies the positions of the given node and all of its descendents to the
// visualization.
CompressedTrie.prototype.realizePositions = function(node) {
    var i;
    if (node.id === undefined) {
        node.id = this.newId();
        this.cmd("CreateCircle", node.id, node.value, node.x, node.y);
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

CompressedTrie.prototype.lookupWrapper = function(event) {
    var key = this.txtKey.value;
    this.txtKey.value = "";
    if (key !== "") {
        this.implementAction(this.lookup.bind(this), key);
    }
}

CompressedTrie.prototype.insertWrapper = function(event) {
    var key = this.txtKey.value;
    this.txtKey.value = "";
    if (key !== "") {
        this.implementAction(this.insert.bind(this), key);
    }
}

CompressedTrie.prototype.removeWrapper = function(event) {
    var key = this.txtKey.value;
    this.txtKey.value = "";
    if (key !== "") {
        this.implementAction(this.remove.bind(this), key);
    }
}

// Animates navigation down the trie searching for the given key.  The current
// node is left highlighted!  The caller must disable the highlight.  Returns an
// object containing:
//
// - isPrefix, boolean: whether the key is a prefix of a word in the trie.
//
// - isKeyInTrie, boolean: whether the complete key is a word in the trie.
//
// - node: the final node visited.
//
// - nodeValueIndex: 1 + the index of the final character considered in
//   node.value.
//
// - keyIndex: 1 + the index of the final character considered in the key.
CompressedTrie.prototype.navigate = function(key) {
    var node = this.root;
    var keyIndex = 0;
    var ch, child, subkey, match, result;

    this.cmd("SetHighlight", node.id, 1);
    this.cmd("Step");

    while (keyIndex < key.length) {
        // Try to find a child that matches key[keyIndex].
        child = node.children[key[keyIndex]];
        if (child === undefined) {
            this.setStatus("Couldn't find child for key[" + keyIndex + "] == '" + key[keyIndex] + "'.");
            this.cmd("Step");
            result = {isPrefix: false, nodeValueIndex: node.value.length};
            break;
        }

        this.setStatus("Moved to child for key[" + keyIndex + "] == '" + key[keyIndex] + "'.");
        this.cmd("SetHighlight", node.id, 0);
        node = child;
        this.cmd("SetHighlight", node.id, 1);
        this.cmd("Step");

        subkey = key.substr(keyIndex);
        this.setStatus(
            "Matching key[" + keyIndex + "..] == \"" + subkey + "\" against node \"" +
                node.value + "\".");
        this.cmd("Step");

        match = CompressedTrie.matchNodeValue(node.value, subkey);
        if (match.result === CompressedTrie.RESULT_MATCH) {
            this.setStatus("Complete match.");
            this.cmd("Step");
            keyIndex += subkey.length;  // keyIndex now is key.length.
            result = {isPrefix: true, nodeValueIndex: node.value.length};
            break;
        } else if (match.result === CompressedTrie.RESULT_MISMATCH) {
            this.setStatus("Mismatch at index " + match.index + ".");
            this.cmd("Step");
            keyIndex += match.index;
            result = {isPrefix: false, nodeValueIndex: match.index};
            break;
        } else if (match.result === CompressedTrie.RESULT_KEY_IS_PREFIX) {
            this.setStatus(
                "key[" + keyIndex + "..] == \"" + subkey +
                    "\" is a prefix of the current node.");
            this.cmd("Step");
            keyIndex += subkey.length;  // keyIndex is now key.length.
            result = {isPrefix: true, nodeValueIndex: subkey.length};
            break;
        } else if (match.result === CompressedTrie.RESULT_NODE_VALUE_IS_PREFIX) {
            this.setStatus(
                "key[" + keyIndex + "..] == \"" + subkey +
                    "\" extends past the current node.");
            this.cmd("Step");
            keyIndex += node.value.length;
            // Continue to the next iteration of the loop.
        } else {
            throw new Error(
                "CompressedTrie.prototype.navigate: Unknown result " + match.result + ".");
        }
    }

    result.node = node;
    result.keyIndex = keyIndex;
    result.isKeyInTrie =
        result.nodeValueIndex === node.value.length &&
        node.childValuesSorted.length === 0;

    this.setStatus("");
    return result;
}

CompressedTrie.prototype.lookup = function(key) {
    var navigation, node;

    this.commands = [];
    navigation = this.navigate(key);
    node = navigation.node;

    if (!navigation.isPrefix) {
        this.setStatus("Mismatch, key not in trie.");
    } else if (navigation.isKeyInTrie) {
        this.setStatus("At end of leaf node, key found in trie.");
    } else {
        this.setStatus("Not at end of leaf node, key not in trie.");
    }

    this.cmd("Step");
    this.setStatus("");
    this.cmd("SetHighlight", node.id, 0);

    return this.commands;
}

CompressedTrie.prototype.insert = function(key) {
    var navigation, node, child;

    this.commands = [];
    navigation = this.navigate(key);
    node = navigation.node;

    if (navigation.isPrefix) {
        this.setStatus(
            navigation.isKeyInTrie ?
                "At end of leaf node, key already in tree." :
                "Key is a prefix of an existing word, can't insert.");
        this.cmd("Step");
    } else if (navigation.isKeyInTrie && node.parent !== undefined) {
        this.setStatus("Key is a suffix of an existing key, can't insert.");
        this.cmd("Step");
    } else {
        if (navigation.nodeValueIndex < node.value.length) {
            // There is a mismatch between key[navigation.keyIndex] and
            // node.value[navigation.nodeValueIndex].  Split node.
            this.setStatus("Splitting node into matched and unmatched parts.");
            this.cmd("Step");
            this.splitNode(node, navigation.nodeValueIndex);
            this.cmd("Step");
        }

        // We matched up to the end of the value in the current node, so
        // insert a new child node with the remainder of the key.
        this.setStatus("Adding a new child.");
        this.cmd("Step");

        child = this.newNode(key.substr(navigation.keyIndex), node);
        this.addNodeChild(node, child);
        this.redrawTree();
        this.cmd("SetHighlight", node.id, 0);
        node = child;
        this.cmd("SetHighlight", node.id, 1);
        this.cmd("Step");
    }

    this.setStatus("");
    this.cmd("SetHighlight", node.id, 0);

    return this.commands;
}

CompressedTrie.prototype.remove = function(key) {
    var navigation, node, child;

    this.commands = [];
    navigation = this.navigate(key);
    node = navigation.node;

    if (!navigation.isKeyInTrie) {
        this.setStatus("Key is not in trie, can't remove.");
        this.cmd("Step");
    } else {
        this.setStatus("Removing leaf node from trie.");
        this.cmd("Step");
        this.cmd("SetHighlight", node.id, 0);
        node = this.removeNode(node);
        this.redrawTree();
        this.cmd("SetHighlight", node.id, 1);
        this.cmd("Step");

        if (node.parent === undefined) {
            this.setStatus("Parent is root, no merging to do.");
            this.cmd("Step");
        } else if (node.childValuesSorted.length > 1) {
            this.setStatus("Parent has multiple children, no merging to do.");
            this.cmd("Step");
        } else {
            this.setStatus("Parent has a single child, merging them together.");
            this.cmd("Step");

            child = node.children[node.childValuesSorted[0]];
            this.setNodeValue(node, node.value + child.value);
            this.removeNode(child);
            this.redrawTree();
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
    currentAlg = new CompressedTrie(animManag, canvas.width, canvas.height);
}
