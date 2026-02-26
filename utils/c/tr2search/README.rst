C Project
=========

Name: tr2search

Description
===========

Search messages in TimeRanger2 database with content-based filtering.

Extends ``tr2list`` with the ability to search *within* record content:
decode fields (e.g. base64-encoded payloads), match text patterns,
and display results as JSON or hexdump.

Options
-------

Database:
  ``-r, --recursive``        List recursively through subdirectories.

Presentation:
  ``-l, --verbose LEVEL``    0=total, 1=metadata, 2=metadata+path, 3=metadata+record.
  ``-m, --mode MODE``        Display mode: ``form`` or ``table``.
  ``-f, --fields FIELDS``    Print only specified fields.
  ``-d, --show_md2``         Show ``__md_tranger__`` metadata field.

Search conditions (time):
  ``--from-t TIME``          Start time.
  ``--to-t TIME``            End time.
  ``--from-tm TIME``         From message time.
  ``--to-tm TIME``           To message time.

Search conditions (row):
  ``--from-rowid ROWID``     Start row ID (use -N for "last N").
  ``--to-rowid ROWID``       End row ID.

Search conditions (flags):
  ``--user-flag-set MASK``          User flag mask (set).
  ``--user-flag-not-set MASK``      User flag mask (not set).
  ``--system-flag-set MASK``        System flag mask (set).
  ``--system-flag-not-set MASK``    System flag mask (not set).

Search conditions (keys):
  ``--key KEY``              Filter by key.
  ``--not-key KEY``          Exclude key.

Search conditions (content):
  ``--search-content-key KEY``        JSON field containing data to search.
  ``--search-content-filter FILTER``  Filter to apply: ``clear`` or ``base64``.
  ``--search-content-text TEXT``      Text to search in filtered content.
  ``--diplay-format FORMAT``          Display format: ``json`` or ``hexdump``.

Examples
--------

Search base64-encoded frames, show last 10 records::

    tr2search /yuneta/store/queues/frames/myrole^myname/frames \
      --search-content-key=frame64 \
      --search-content-filter=base64 \
      --from-rowid=-10 -l3

Search by key with content text match::

    tr2search /yuneta/store/queues/frames/gps^device/frames \
      --key=867060032083772 \
      --from-rowid=3793 --to-rowid=3799 \
      --search-content-key=frame64 -l3

License
-------

Licensed under the  `The MIT License <http://www.opensource.org/licenses/mit-license>`_.
See LICENSE.txt in the source distribution for details.
