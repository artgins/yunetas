C Project
=========

Name: treedb_list

Description
===========

List records of Tree Database over TimeRanger2.

A TreeDB organizes records (nodes) in a hierarchical tree of objects
linked by child-parent relations, with typed schemas discovered
automatically from ``.treedb_schema.json`` files.

Options
-------

Database:
  ``-b, --database DATABASE``  TreeDB name (auto-discovered if only one exists).
  ``-c, --topic TOPIC``        Topic name (lists all topics if omitted).
  ``-i, --ids ID``             Specific ID or comma-separated list of IDs.
  ``-r, --recursive``          List recursively across all TreeDB databases.

Presentation:
  ``-m, --mode MODE``          Display mode: ``form`` (JSON) or ``table`` (columnar).
  ``-f, --fields FIELDS``      Print only specified fields.

Debug:
  ``--print-tranger``          Print tranger JSON configuration.
  ``--print-treedb``           Print treedb JSON structure.

Examples
--------

List all records in default treedb::

    treedb_list /yuneta/store/agent/agent

List a specific topic in table mode::

    treedb_list -c binaries -m table /yuneta/store/agent/agent

List specific IDs with selected fields::

    treedb_list -c yunos -i "yuno1,yuno2" -f id,name,status /path/to/tranger

Recursive listing of all treedbs under a path::

    treedb_list -r /yuneta/store

License
-------

Licensed under the  `The MIT License <http://www.opensource.org/licenses/mit-license>`_.
See LICENSE.txt in the source distribution for details.
