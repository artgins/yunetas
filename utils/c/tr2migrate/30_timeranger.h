/****************************************************************************
 *      TIMERANGER.H
 *
 *      Time Ranger, a serie-time key-value database over flat files
 *
 *      Copyright (c) 2017-2024 Niyamaka.
 *      All Rights Reserved.
 *
 *  Básicamente un resource (topic) está implementado como:
 *      - Diccionario de id's (key's) con lista de imágenes en tiempo.
 *      - !persistente! en disco.
 *
 *  Conteniendo un objeto basado en
 *      - tiempo __t__, rowid __rowid__
 *      - clave (entero o string, de tamaño máximo RECORD_RECORD_KEY_VALUE_MAX bytes)
 *      - valor (json, tamaño sin límite, limitado al sistema operativo)
 *
 *  Organización en disco:
 *
 *  (store directory) -> (database):
 *
 *      __timeranger__.json     TimerRanger metadata.
 *                              Creado al crear la database, no modificable jamás.
 *                              Fichero usado como semaforo para los procesos en su uso.
 *                              Por ahora, un único proceso master abre con permiso exclusivo para escribir.
 *                              Los no master, abren la base de datos solo en modo lectura.
 *                              Define los permisos de ficheros y directorios,
 *                              el tipo de organizacion de directorios y ficheros
 *                              y el nombre de la base de datos.
 *
 *                              Only read, persistent
 *                              "path"
 *                              "database"
 *                              "directory"
 *                              "filename_mask"
 *                              "rpermission"
 *                              "xpermission"
 *
 *                              Only read, volatil, defining in run-time
 *                              "on_critical_error",
 *                              "master",
 *                              "topics",
 *
 *
 *          /{topic}            Directorio conteniendo los ficheros del recurso.
 *
 *          topic_desc.json     Fixed topic metadata (Only read)
 *
 *                              "topic_name"    Topic name
 *                              "pkey"          Record's field with Key of message.
 *                              "tkey"          Record's field with Time of message.
 *                              "directory"
 *                              'system_flag' data, HACK inherited by records
 *
 *
 *          topic_var.json      Variable topic metadata (Writable)
 *
 *                              'user_flag' data
 *
 *          topic_idx.md        Register of record's metadata
 *
 *          /{topic}/data       Directorio conteniendo los registros del topic.
 *
 *          {format-file}.json  Ficheros de datos
 *                              Jamás debe ser modificado externamente.
 *                              Los datos son inmutables, una vez son escritos, jamás se pueden modificar.
 *                              Para variar los datos, añade un nuevo registro con los cambios deseados.
 *
 *
 ****************************************************************************/

#pragma once

#include <gobj.h>
#include <kwid.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Structures
 ***************************************************************/

typedef enum { // WARNING table with name's strings in 30_timeranger.c
    sf0_string_key           = 0x00000001,
    sf0_rowid_key            = 0x00000002,
    sf0_int_key              = 0x00000004,
    sf0_zip_record           = 0x00000010,
    sf0_cipher_record        = 0x00000020,
    sf0_t_ms                 = 0x00000100,   // record time in miliseconds
    sf0_tm_ms                = 0x00000200,   // message time in miliseconds
    sf0_no_record_disk       = 0x00001000,
    sf0_no_md_disk           = 0x00002000,
    sf0_no_disk              = 0x00003000,   // sf_no_record_disk + sf_no_md_disk
    sf0_loading_from_disk    = 0x01000000,
    sf0_mark1                = 0x02000000,
    sf0_deleted_record       = 0x80000000,
} system_flag_t;

#define RECORD_KEY_VALUE_MAX   (48)
#define tranger_max_key_size() (RECORD_KEY_VALUE_MAX-1)

#pragma pack(1)

typedef struct { // Size: 96 bytes
    uint64_t __rowid__;
    uint64_t __t__;

    uint64_t __offset__;
    uint64_t __size__;

    uint64_t __tm__;
    uint32_t __system_flag__;
    uint32_t __user_flag__;
    union {
        uint64_t i;
        char s[RECORD_KEY_VALUE_MAX];  // Maximum size of string keys is RECORD_KEY_VALUE_MAX-1
    } key;
} md_record_t;

#pragma pack()


/***************************************************************
 *              Prototypes
 ***************************************************************/

/**rst**
   Startup TimeRanger database
**rst**/
static const json_desc_t tranger_json_desc[] = {
// Name                 Type    Default     Fillspace
{"path",                "str",  "",         ""}, // If database exists then only needs (path,[database]) params
{"database",            "str",  "",         ""}, // If null, path must contains the 'database'
{"filename_mask",       "str",  "%Y-%m-%d", ""}, // Organization of tables (file name format, see strftime())
{"xpermission" ,        "int",  "02770",    ""}, // Use in creation, default 02770;
{"rpermission",         "int",  "0660",     ""}, // Use in creation, default 0660;
{"on_critical_error",   "int",  "2",        ""},  // Volatil, default LOG_OPT_EXIT_ZERO (Zero to avoid restart)
{"master",              "bool", "false",    ""}, // Volatil, the master is the only that can write.
{0}
};
PUBLIC json_t *tranger_startup(
    json_t *jn_tranger // owned
);

/**rst**
   Shutdown TimeRanger database
**rst**/
PUBLIC int tranger_shutdown(json_t *tranger);

/**rst**
   Convert string (..|..|...) to system_flag_t integer
**rst**/
PUBLIC system_flag_t tranger_str2system_flag(const char *system_flag);

/**rst**
   Create topic if not exist. Alias create table.
   HACK IDEMPOTENT function
**rst**/
PUBLIC json_t *tranger_create_topic( // WARNING returned json IS NOT YOURS
    json_t *tranger,    // If topic exists then only needs (tranger, topic_name) parameters
    const char *topic_name,
    const char *pkey,
    const char *tkey,
    system_flag_t system_flag,
    json_t *jn_cols,    // owned
    json_t *jn_var      // owned
);

/**rst**
   Open topic
   HACK IDEMPOTENT function, always return the same json_t topic
**rst**/
PUBLIC json_t *tranger_open_topic( // WARNING returned json IS NOT YOURS
    json_t *tranger,
    const char *topic_name,
    BOOL verbose
);

/**rst**
   Get list of opened topics
**rst**/
PUBLIC json_t *tranger_opened_topics( // Return must be decref
    json_t *tranger
);

/**rst**
   Get topic by his topic_name.
   Topic is opened if it's not opened.
   HACK topic can exists in disk, but it's not opened until tranger_open_topic()
**rst**/
PUBLIC json_t *tranger_topic( // WARNING returned json IS NOT YOURS
    json_t *tranger,
    const char *topic_name
);

/**rst**
   Get topic size (number of records)
**rst**/
PUBLIC json_int_t tranger_topic_size(
    json_t *topic
);

/**rst**
   Return topic name of topic.
**rst**/
PUBLIC const char *tranger_topic_name(
    json_t *topic
);

/**rst**
   Close record topic.
**rst**/
PUBLIC int tranger_close_topic(
    json_t *tranger,
    const char *topic_name
);

/**rst**
   Close topic opened files
**rst**/
PUBLIC int tranger_close_topic_opened_files(
    json_t *tranger,
    const char *topic_name
);

/**rst**
   Delete topic. Alias delete table.
**rst**/
PUBLIC int tranger_delete_topic(
    json_t *tranger,
    const char *topic_name
);

/**rst**
   Backup topic and re-create it.
   If ``backup_path`` is empty then it will be used the topic path
   If ``backup_name`` is empty then it will be used ``topic_name``.bak
   If overwrite_backup is true and backup exists then it will be overwrited
        but before tranger_backup_deleting_callback() will be called
            and if it returns TRUE then the existing backup will be not removed.
   Return the new topic
**rst**/

typedef BOOL (*tranger_backup_deleting_callback_t)( // Return TRUE if you control the backup
    json_t *tranger,
    const char *topic_name,
    const char *path
);
PUBLIC json_t *tranger_backup_topic(
    json_t *tranger,
    const char *topic_name,
    const char *backup_path,
    const char *backup_name,
    BOOL overwrite_backup,
    tranger_backup_deleting_callback_t tranger_backup_deleting_callback
);

/**rst**
   Write topic var
**rst**/
PUBLIC int tranger_write_topic_var(
    json_t *tranger,
    const char *topic_name,
    json_t *jn_topic_var  // owned
);

/**rst**
   Write topic cols
**rst**/
PUBLIC int tranger_write_topic_cols(
    json_t *tranger,
    const char *topic_name,
    json_t *jn_cols  // owned
);

PUBLIC json_t *tranger_topic_desc( // Return MUST be decref
    json_t *tranger,
    const char *topic_name
);
PUBLIC json_t *tranger_list_topic_desc( // Return a list!,  MUST be decref
    json_t *tranger,
    const char *topic_name
);
PUBLIC json_t *tranger_dict_topic_desc( // Return a dict!,  MUST be decref
    json_t *tranger,
    const char *topic_name
);
PUBLIC json_t *tranger_filter_topic_fields(
    json_t *tranger,
    const char *topic_name,
    json_t *kw  // owned
);

/**rst**
    Return json object with record metadata
**rst**/
PUBLIC json_t *tranger_md2json(md_record_t *md_record);

/**rst**
    Append a new item to record.
    Return the new record's metadata.
**rst**/
PUBLIC int tranger_append_record(
    json_t *tranger,
    const char *topic_name,
    uint64_t __t__,         // if 0 then the time will be set by TimeRanger with now time
    uint32_t user_flag,
    md_record_t *md_record, // required
    json_t *jn_record       // owned
);

/**rst**
    Delete record.
**rst**/
PUBLIC int tranger_delete_record(
    json_t *tranger,
    const char *topic_name,
    uint64_t rowid
);

/**rst**
    Write record mark1
**rst**/
PUBLIC int tranger_write_mark1(
    json_t *tranger,
    const char *topic_name,
    uint64_t rowid,
    BOOL set
);

/**rst**
    Write record user flag
**rst**/
PUBLIC int tranger_write_user_flag(
    json_t *tranger,
    const char *topic_name,
    uint64_t rowid,
    uint32_t user_flag
);

/**rst**
    Write record user flag using mask
**rst**/
PUBLIC int tranger_set_user_flag(
    json_t *tranger,
    const char *topic_name,
    uint64_t rowid,
    uint32_t mask,
    BOOL set
);

/**rst**
    Read record user flag (for writing mode)
**rst**/
PUBLIC uint32_t tranger_read_user_flag(
    json_t *tranger,
    const char *topic_name,
    uint64_t rowid
);




        /*********************************************
         *          Read mode functions
         *********************************************/




/**rst**
    Walk in topic through records, and callback with the matched record found.
    The return is a list handler that must be close with tranger_close_list()
    When the list is open, before return, all records will be sent to callback.
    While the list is not closed, TimeRanger will be send new matched records to callback.

    match_cond:

        backward
        only_md     (don't load jn_record on loading disk)

        from_rowid
        to_rowid
        from_t
        to_t
        user_flag
        not_user_flag
        user_flag_mask_set
        user_flag_mask_notset
        key
        notkey
        from_tm
        to_tm
        rkey    regular expression of key
        filter  dict with fields to match

**rst**/

/*
 *  HACK Return of callback:
 *      0 do nothing (callback will create their own list, or not),
 *      1 add record to returned list.data,
 *      -1 break the load
 */
typedef int (*tranger_load_record_callback_t)(
    json_t *tranger,
    json_t *topic,
    json_t *list,
    md_record_t *md_record,
    /*
     *  can be null if sf_loading_from_disk (use tranger_read_record_content() to load content)
     */
    json_t *jn_record // must be owned
);

/**rst**
    Open list, load records in memory
**rst**/
static const json_desc_t list_json_desc[] = {
// Name                     Type        Default     Fillspace
{"topic_name",              "str",      "",         ""},
{"match_cond",              "dict",     "{}",       ""},
{"load_record_callback",    "int",      "",         ""},
{0}
};
PUBLIC json_t *tranger_open_list(
    json_t *tranger,
    json_t *jn_list // owned
);

/**rst**
    Get list by his id
**rst**/
PUBLIC json_t *tranger_get_list(
    json_t *tranger,
    const char *id
);

/**rst**
    Close list
**rst**/
PUBLIC int tranger_close_list(
    json_t *tranger,
    json_t *list
);

/**rst**
    Get md record by rowid
**rst**/
PUBLIC int tranger_get_record(
    json_t *tranger,
    json_t *topic,
    uint64_t rowid,
    md_record_t *md_record,
    BOOL verbose
);

/**rst**
    Read record content. Return must be decref!
**rst**/
PUBLIC json_t *tranger_read_record_content(
    json_t *tranger,
    json_t *topic,
    md_record_t *md_record
);

/**rst**
    Match md record
**rst**/
PUBLIC BOOL tranger_match_record(
    json_t *tranger,
    json_t *topic,
    json_t *match_cond,  // not owned
    const md_record_t *md_record,
    BOOL *end
);

/**rst**
    Get the first md record matching conditions
**rst**/
PUBLIC int tranger_find_record(
    json_t *tranger,
    json_t *topic,
    json_t *match_cond,  // owned
    md_record_t *md_record
);

/**rst**
    Walk over md records (disk!)
**rst**/
PUBLIC int tranger_first_record(
    json_t *tranger,
    json_t *topic,
    md_record_t *md_record
);
PUBLIC int tranger_last_record(
    json_t *tranger,
    json_t *topic,
    md_record_t *md_record
);
PUBLIC int tranger_next_record(
    json_t *tranger,
    json_t *topic,
    md_record_t *md_record
);
PUBLIC int tranger_prev_record(
    json_t *tranger,
    json_t *topic,
    md_record_t *md_record
);

/*
 *  print_md1_record: info of record metadata
 *  print_md2_record: info of message record metadata
 *  print_md3_record: info of file record metadata
 */
PUBLIC void print_md0_record(
    json_t *tranger,
    json_t *topic,
    const md_record_t *md_record,
    char *bf,
    int bfsize
);
PUBLIC void print_md1_record(
    json_t *tranger,
    json_t *topic,
    const md_record_t *md_record,
    char *bf,
    int bfsize
);
PUBLIC void print_md2_record(
    json_t *tranger,
    json_t *topic,
    const md_record_t *md_record,
    char *bf,
    int bfsize
);

PUBLIC void print_record_filename(
    json_t *tranger,
    json_t *topic,
    const md_record_t *md_record,
    char *bf,
    int bfsize
);

PUBLIC void tranger_set_trace_level(
    json_t *tranger,
    int trace_level
);

#ifdef __cplusplus
}
#endif
