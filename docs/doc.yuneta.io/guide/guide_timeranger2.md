(timeranger2)=
# **Timeranger2**

**Timeranger2** is a time-series data storage and retrieval system designed for efficient event tracking and data management. It is a core component of **Yuneta**, operating based on **TimeRanger Philosophy**, where data is stored as **key-value pairs** in **flat files** with timestamp indexing.

## Features

- **Time-Series Storage:** Stores data in a time-ordered format for efficient querying.
- **Flat File System:** Utilizes simple yet optimized storage mechanisms for performance.
- **Key-Value Data Model:** Stores records with parent-child relationships, forming a structured hierarchy.
- **Efficient Querying:** Enables retrieval of time-based data segments quickly.
- **Hierarchical Organization:** Supports **one-to-one, one-to-many, and many-to-one** relationships.
- **Event-Based Processing:** Works in sync with Yuneta's event-driven architecture.

## Architecture

- **Files and Memory:** Operates primarily on flat files while maintaining essential structures in memory.
- **Indexing:** Uses timestamp-based indexing for efficient access to time-sequenced data.
- **Hierarchical Data Handling:** Facilitates relationships between stored records.

## Use Cases

- **Event Logging:** Track and analyze system or application events over time.
- **Monitoring & Metrics Collection:** Store and retrieve time-based metrics for analytics.
- **Data Persistence for Applications:** Act as a persistent backend for event-driven applications.

## Relationship with Yuneta

- **Integrated with Yuneta’s Object Model**: Works alongside `gobj` architecture for seamless event handling.
- **Persistence & Retrieval**: Complements **Treedb** by focusing on long-term storage and efficient retrieval.

Timeranger2 is optimized for scenarios where structured time-based data is essential, making it a key component of Yuneta’s ecosystem.

(tr_queue)=
```c
typedef void *tr_queue;
```

(q_msg)=
```c
typedef void *q_msg;
```

(topic_json_desc)=
```c
static const json_desc_t topic_json_desc[] = {
// Name                 Type    Default     Fillspace
{"on_critical_error",   "int",  "2",        ""}, // Volatil, default LOG_OPT_EXIT_ZERO (Zero to avoid restart)
{"filename_mask",       "str",  "%Y-%m-%d", ""}, // Organization of tables (file name format, see strftime())
{"xpermission" ,        "int",  "02770",    ""}, // Use in creation, default 02770;
{"rpermission",         "int",  "0660",     ""}, // Use in creation, default 0660;
```

(tranger2_json_desc)=
```c
static const json_desc_t tranger2_json_desc[] = {
// Name                 Type    Default     Fillspace
{"path",                "str",  "",         ""}, // If database exists then only needs (path,[database]) params
{"database",            "str",  "",         ""}, // If null, path must contains the 'database'

// Default for topics
{"filename_mask",       "str",  "%Y-%m-%d", ""}, // Organization of tables (file name format, see strftime())
{"xpermission" ,        "int",  "02770",    ""}, // Use in creation, default 02770;
{"rpermission",         "int",  "0660",     ""}, // Use in creation, default 0660;

// Volatil fields
{"on_critical_error",   "int",  "2",        ""}, // Volatil, default LOG_OPT_EXIT_ZERO (Zero to avoid restart)
{"master",              "bool", "false",    ""}, // Volatil, the master is the only that can write.
{"gobj",                "int",  "",         ""}, // Volatil, gobj of tranger
{"trace_level",         "int",  "0",        ""}, // Volatil, trace level

{0}
};
```

(tranger2_load_record_callback_t)=
```c
typedef int (*tranger2_load_record_callback_t)(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,       // iterator or rt_mem/rt_disk, don't own
    json_int_t rowid,   // in a rt_mem will be the relative rowid, in rt_disk the absolute rowid
    md2_record_ex_t *md_record,
    json_t *jn_record  // must be owned
);
```


## Summary of `fs_watcher.h`

This header file defines the **fs_watcher** module, which provides filesystem event watching functionality.

### **Typedefs**
- Contains multiple `typedef` declarations:
    - **Enumerations:**
      ```c
        typedef enum  {
            FS_SUBDIR_CREATED_TYPE  = 1,    // use directory / filename
            FS_SUBDIR_DELETED_TYPE,         // use directory / filename
            FS_FILE_CREATED_TYPE,           // use directory / filename
            FS_FILE_DELETED_TYPE,           // use directory / filename
            FS_FILE_MODIFIED_TYPE,          // use directory / filename, see WARNING

            // There are more fs events available with io_uring, but this code only manages these events.
        } fs_type_t;

        typedef enum  {
            FS_FLAG_RECURSIVE_PATHS     = 0x0001,     // add path and all his subdirectories
            FS_FLAG_MODIFIED_FILES      = 0x0002,     // Add FS_FILE_MODIFIED_TYPE, WARNING about using it.
        } fs_flag_t;
      ```

(fs_event_t)=
    - **Structures:**
      ```c

        typedef struct fs_event_s fs_event_t;

        typedef int (*fs_callback_t)(
            fs_event_t *fs_event
        );

        struct fs_event_s {
            yev_loop_h yev_loop;
            yev_event_h yev_event;
            const char *path;
            fs_flag_t fs_flag;
            fs_type_t fs_type;          // Output
            volatile char *directory;   // Output
            volatile char *filename;    // Output
            hgobj gobj;
            void *user_data;
            void *user_data2;
            fs_callback_t callback;
            int fd;
            json_t *jn_tracked_paths;
        } ;

      ```
