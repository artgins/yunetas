const randomNumber = function() { return Math.floor(Math.random() * 100) }

const compare = function(a, b) {
  if (typeof a === 'number' && typeof b === 'number') {
    if (a === b) { return 0 }
    if (a < b) { return -1 }
    if (a > b) { return 1 }
  } else if (typeof a === 'string' && typeof b === 'string') {
    return a.localeCompare(b)
  } else if (a.compare !== undefined && typeof a === typeof b) {
    return a.compare(b)
  }
}

class Tree {
  get width() {
    if (this.children === undefined || this.children.length == 0) {
      return nodeWidth
    } else {
      var sum = 0;
      for (var i = 0; i < this.children.length; i++) {
        sum += this.children[i].width
      }
      return sum + (horizontalSpace * (this.children.length - 1))
    }
  }

}

class BinaryTree extends Tree {
  constructor(data, parent, left, right) {
    super()
    this.data = data
    this.left = left
    this.right = right
    this.parent = parent
  }

  get meta() {
    if (this.data !== undefined) {
      var h = (this.data/100)*300
      return {
        color: `hsl(${h}, 100%, 50%)`,
        textColor: 'white'
      }
    }
  }

  get root() {
    if (this.parent === undefined) {
      return this
    } else {
      return this.parent.root
    }
  }

  get children() {
    return [this.left, this.right].filter(v => v !== undefined)
  }

  find(value) {
    switch (compare(value, this.data)) {
      case -1:
        return this.left.find(value)
      case 0:
        return this
      case 1:
        return this.right.find(value)
    }
  }

  insert(value) {
    if (this.data === undefined) {
      this.data = value
      return this
    }
    switch (compare(value, this.data)) {
      case -1:
        if (this.left === undefined) {
          this.left = new BinaryTree(value, this)
          return this.root
        } else {
          return this.left.insert(value)
        }
        /* jshint -W086 */
      case 0:
        return this.root
      case 1:
        if (this.right === undefined) {
          this.right = new BinaryTree(value, this)
          return this.root
        } else {
          return this.right.insert(value)
        }
    }
  }

}

const red = 'red'
const black = 'black'

class RedBlackTree extends BinaryTree {
  constructor(data, color, parent, left, right) {
    super(data, left, right)
    this.color = color || black
    this.parent = parent
  }

  get meta() {
    return {
      color: this.color,
      textColor: 'white'
    }
  }

  get children() {
    return [this.left, this.right].filter(v => v !== undefined)
  }

  get root() {
    if (this.parent) {
      return this.parent.root
    } else {
      return this
    }
  }

  get grandparent() {
    if (this.parent) {
      return this.parent.parent
    } else {
      return undefined
    }
  }

  get sibling() {
    if (this.parent.right === this) {
      return this.parent.left
    } else {
      return this.parent.right
    }
  }

  get uncle() {
    return this.parent.sibling
  }

  setLeft(node) {
    if (node) {
      node.parent = this
      this.left = node
    } else {
      this.left = undefined
    }
  }

  setRight(node) {
    if (node) {
      node.parent = this
      this.right = node
    } else {
      this.right = undefined
    }
  }

  replaceSelfInParent(node) {
    if (!this.parent) {
      node.parent = undefined
    } else if (this.parent.right && this.parent.right === this) {
      this.parent.setRight(node)
    } else {
      this.parent.setLeft(node)
    }
  }

  rotateLeft() {
    var newNode = this.right
    if (newNode === undefined) {
      throw new Error('Cannot set leaf as internal node')
    }
    this.setRight(newNode.left)
    this.replaceSelfInParent(newNode)
    newNode.setLeft(this)
  }

  rotateRight() {
    var newNode = this.left
    if (newNode === undefined) {
      throw new Error('Cannot set leaf as internal node')
    }
    this.setLeft(newNode.right)
    this.replaceSelfInParent(newNode)
    newNode.setRight(this)
  }

  repair() {
    if (this.parent === undefined) {
      this.color = black
      return this.root
    } else if (this.parent.color === black) {
      // all good
      return this.root
    } else if (this.uncle && this.uncle.color === red) {
      this.parent.color = black
      this.uncle.color = black
      this.grandparent.color = red
      return this.grandparent.repair()
    } else {
      var n = this
      var p = n.parent
      var g = n.grandparent
      if (g) {
        if (g.left && n === g.left.right) {
          p.rotateLeft()
          n = n.left
        } else if (g.right && n === g.right.left) {
          p.rotateRight()
          n = n.right
        }
        p = n.parent
        g = n.grandparent
        if (n == p.left) {
          g.rotateRight()
        } else {
          g.rotateLeft()
        }
        p.color = black
        g.color = red
      }
      return this.root
    }
  }

  insert(value) {
    if (this.data === undefined) {
      this.data = value
      return this.root
    }
    switch (compare(value, this.data)) {
      case -1:
        if (this.left !== undefined) {
          return this.left.insert(value)
        } else {
          this.left = new RedBlackTree(value, red, this)
          return this.left.repair()
        }
        /* jshint -W086 */
      case 0:
        return this.root
      case 1:
        if (this.right !== undefined) {
          return this.right.insert(value)
        } else {
          this.right = new RedBlackTree(value, red, this)
          return this.right.repair()
        }
    }
  }

}

const drawLine = function(ctx, center, tree) {
  ctx.save()
  ctx.lineTo(center[0]-nodeWidth/2, center[1])
  ctx.stroke()
  ctx.restore()
}

const drawNode = function(ctx, center, tree) {
  ctx.save()
  if (tree.meta) {
    ctx.fillStyle = tree.meta.color || '#000000'
  }
  ctx.beginPath()
  ctx.arc(center[0]-nodeWidth/2, center[1], nodeWidth, 0, 2 * Math.PI)
  ctx.fill()
  ctx.fillStyle = 'black'
  ctx.stroke()
  ctx.restore()
}

const drawLabel = function(ctx, center, tree) {
  ctx.save()
  ctx.font = "20px sans-serif"
  if (tree.meta) {
    ctx.fillStyle = tree.meta.textColor
  }
  ctx.textAlign = "center"
  ctx.fillText(tree.data, center[0]-nodeWidth/2, center[1]+nodeWidth/2)
  ctx.restore()
}

const drawTree = function(ctx, tree, center, fn) {
  if (tree.data === undefined) return
  ctx.save()
  fn(ctx, center, tree)
  ctx.moveTo(center[0]-nodeWidth/2, center[1])
  const baseWidth = tree.width
  var origin = center[0]-(baseWidth/2)
  if (tree.children !== undefined) {
    tree.children.forEach((child) => {
      ctx.save()
      const childWidth = child.width
      const childCenter = [origin+(childWidth/2), center[1]+verticalSpace]
      ctx.moveTo(center[0]-nodeWidth/2, center[1])
      drawTree(ctx, child, childCenter, fn)
      origin = childCenter[0] + childWidth/2 + horizontalSpace
      ctx.restore()
    })
  }
  ctx.restore()
}

const scale = 1.0
const nodeWidth = 15 * scale
const horizontalSpace = 50 * scale
const verticalSpace = 50 * scale
const nodeCount = 30

const treeSelect = document.getElementById('tree-select')
const canvas = document.getElementById('canvas')
const valueList = document.getElementById('value-list')
const insertInput = document.getElementById('insert-input')
const insertSubmit = document.getElementById('insert-submit')
const insertRandom = document.getElementById('insert-random')
const nukeSubmit = document.getElementById('nuke-submit')

const ctx = canvas.getContext('2d')

const treeTypes = {
  'bst': BinaryTree,
  'rbt': RedBlackTree
}

var inserted
var tree

const setType = function(event) {
  const type = treeSelect.value
  init(treeTypes[type])
}

const init = function(TreeClass) {
  inserted = []
  tree = new TreeClass()
  redraw()
}

const draw = function() {
  const origin = [ctx.canvas.width/2, 20]
  drawTree(ctx, tree, origin, drawLine)
  drawTree(ctx, tree, origin, drawNode)
  drawTree(ctx, tree, origin, drawLabel)
}

const resize = function() {
  const height = innerHeight * 0.75
  const ratio = canvas.width/canvas.height
  const width = height * ratio
  canvas.style.width = width + 'px'
  canvas.style.height = height + 'px'
}

const redraw = function() {
  canvas.width = innerWidth * scale
  canvas.height = innerHeight * scale
  valueList.innerHTML = "[" + inserted.join(', ') + "]"
  draw()
}

const insert = function(event) {
  const value = insertInput.value
  const intValue = parseInt(value)
  if (value !== undefined && !isNaN(intValue)) {
    inserted.push(intValue)
    tree = tree.insert(value)
    redraw()
  }
}

const insertRnd = function(event) {
  const newInt = randomNumber()
  inserted.push(newInt)
  tree = tree.insert(newInt)
  redraw()
}

const nuke = function(event) {
  init(treeTypes[treeSelect.value])
}

init(BinaryTree)

treeSelect.onchange = setType
insertSubmit.onclick = insert
insertRandom.onclick = insertRnd
nukeSubmit.onclick = nuke
window.onresize = resize
window.onload = resize

