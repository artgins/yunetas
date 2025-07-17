#/bin/bash
#
#   ./test.sh       # silence (point ok, red X error)
#   ./test.sh -l1   # with verbose level 1
#   ./test.sh -l2   # with verbose level 2
#

ytests test_treedb.json $1
