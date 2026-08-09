/***********************************************************************
 *          c_log_reader.c
 *          Log_reader GClass.
 *
 *          Read a text file and publish its lines
 *
 *          The gclass knows nothing about nginx. It turns a file into
 *          events: batches of complete lines, then EOF, or an error.
 *
 *          The file is read in bounded chunks and each chunk is a new
 *          action, so a log of any size cannot hold the event loop. The
 *          continuation is EV_READ_CHUNK posted to itself: there is
 *          nothing to wait for, so it is not a timer -- it is the way to
 *          give the loop a turn between chunks. Delivery is a snapshot
 *          per cycle, so the chain advances one chunk per turn of the
 *          loop and the completions of everything else keep arriving.
 *
 *          io_uring is not used here on purpose. yev_create_read_event()
 *          submits its reads with offset 0, which is what a socket wants
 *          and is wrong for a regular file: every chunk would read the
 *          head of the file again. When yev learns a read offset this
 *          gclass can change, and the events it publishes do not.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "c_log_reader.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int read_chunk(hgobj gobj);
PRIVATE int publish_error(hgobj gobj, const char *error);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type--------name----------------flag----------------default-----description---------- */
SDATA (DTP_POINTER, "subscriber",       0,                  0,          "Subscriber of output-events"),
SDATA (DTP_STRING,  "path",             SDF_RD,             "",         "File to read"),
SDATA (DTP_INTEGER, "chunk_size",       SDF_RD,             "262144",   "Bytes read per turn"),
SDATA (DTP_INTEGER, "max_line_size",    SDF_RD,             "65536",    "Longest line accepted"),
SDATA (DTP_INTEGER, "batch_lines",      SDF_RD,             "2000",     "Lines per published batch"),
SDATA (DTP_POINTER, "user_data",        0,                  0,          "user data"),
SDATA (DTP_POINTER, "user_data2",       0,                  0,          "more user data"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_READ = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"read",            "Trace file open, chunks and eof"},
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    istream_h istream;
    char *bf;
    int fd;
    const char *path;
    int32_t chunk_size;
    int32_t max_line_size;
    int32_t batch_lines;
    uint64_t lines;
    uint64_t bytes;
    uint64_t too_long;
    BOOL eof_published;
} PRIVATE_DATA;




                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->fd = -1;

    /*
     *  CHILD subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(path,                  gobj_read_str_attr)
    SET_PRIV(chunk_size,            gobj_read_integer_attr)
    SET_PRIV(max_line_size,         gobj_read_integer_attr)
    SET_PRIV(batch_lines,           gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(chunk_size,          gobj_read_integer_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
}

/***************************************************************************
 *      Framework Method start
 *
 *  The file is opened here, but nothing is published from here: the parent
 *  is inside gobj_start() and must not be re-entered. Every exit posts
 *  EV_READ_CHUNK, so whatever this found is reported on the next cycle,
 *  out of the parent's stack.
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->bf = gbmem_malloc(priv->chunk_size);
    if(!priv->bf) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY,
            "msg",          "%s", "No memory for the read buffer",
            "chunk_size",   "%d", priv->chunk_size,
            NULL
        );
        gobj_post_message(gobj, EV_READ_CHUNK, 0);
        return 0;
    }

    ISTREAM_CREATE(priv->istream, gobj, 4*1024, priv->max_line_size);
    if(!priv->istream) {
        // Error already logged
        gobj_post_message(gobj, EV_READ_CHUNK, 0);
        return 0;
    }
    istream_read_until_delimiter(priv->istream, "\n", 1, 0);

    priv->fd = open(priv->path, O_RDONLY|O_CLOEXEC);
    if(priv->fd < 0) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot open log file",
            "path",         "%s", priv->path,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        gobj_post_message(gobj, EV_READ_CHUNK, 0);
        return 0;
    }

    if(gobj_trace_level(gobj) & TRACE_READ) {
        gobj_trace_msg(gobj, "log_reader: open '%s'", priv->path);
    }

    gobj_post_message(gobj, EV_READ_CHUNK, 0);

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->fd >= 0) {
        close(priv->fd);
        priv->fd = -1;
    }

    ISTREAM_DESTROY(priv->istream)

    GBMEM_FREE(priv->bf)

    return 0;
}




                    /***************************
                     *      Commands
                     ***************************/




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  Publish the failure and stop the work. The parent decides what a file
 *  it cannot read means for the report.
 ***************************************************************************/
PRIVATE int publish_error(hgobj gobj, const char *error)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw = json_pack("{s:s, s:s}",
        "path", priv->path,
        "error", error
    );

    return gobj_publish_event(gobj, EV_LOG_ERROR, kw);
}

/***************************************************************************
 *  Read one chunk, publish the complete lines it closes.
 *
 *  Returns 0 when there is more to read, 1 at the end of the file, -1 on
 *  a failure already published.
 ***************************************************************************/
PRIVATE int read_chunk(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    ssize_t nread;
    do {
        nread = read(priv->fd, priv->bf, priv->chunk_size);
    } while(nread < 0 && errno == EINTR);

    if(nread < 0) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot read log file",
            "path",         "%s", priv->path,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        publish_error(gobj, strerror(errno));
        return -1;
    }

    priv->bytes += (uint64_t)nread;

    /*
     *  A line that crosses the chunk boundary is the classic bug of every
     *  chunked reader: it eats one line per chunk, a small and stable
     *  percentage, so the numbers stay plausible and are wrong. The istream
     *  holds the incomplete tail between chunks, so it cannot happen here.
     */
    json_t *jn_lines = json_array();

    char *bf = priv->bf;
    size_t len = (size_t)nread;

    while(len > 0) {
        size_t consumed = istream_consume(priv->istream, bf, len);
        bf += consumed;
        len -= consumed;

        if(!istream_is_completed(priv->istream)) {
            if(consumed == 0) {
                /*
                 *  The istream is full and found no newline: the line is
                 *  longer than max_line_size. A local log with a line that
                 *  long is corrupted input, not a broken invariant here, so
                 *  it is a warning. Drop what is held and resume at the next
                 *  newline of this chunk.
                 */
                gobj_log_warning(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER,
                    "msg",          "%s", "Line longer than max_line_size, skipped",
                    "path",         "%s", priv->path,
                    "max_line_size","%d", priv->max_line_size,
                    NULL
                );
                priv->too_long++;
                istream_clear(priv->istream);
                istream_read_until_delimiter(priv->istream, "\n", 1, 0);

                char *nl = memchr(bf, '\n', len);
                if(!nl) {
                    break;      // the rest of this chunk belongs to the bad line
                }
                len -= (size_t)(nl - bf) + 1;
                bf = nl + 1;
                continue;
            }
            break;              // the tail waits for the next chunk
        }

        size_t line_len = 0;
        char *line = istream_extract_matched_data(priv->istream, &line_len);
        if(line) {
            if(line_len > 0 && line[line_len-1] == '\n') {
                line_len--;     // the matched data carries the delimiter
            }
            if(line_len > 0 && line[line_len-1] == '\r') {
                line_len--;
            }
            json_array_append_new(jn_lines, json_stringn(line, line_len));
            priv->lines++;
        }
        istream_reset_wr(priv->istream);
        istream_read_until_delimiter(priv->istream, "\n", 1, 0);

        if(json_array_size(jn_lines) >= (size_t)priv->batch_lines) {
            gobj_publish_event(gobj, EV_LOG_LINES, json_pack("{s:s, s:o}",
                "path", priv->path,
                "lines", jn_lines
            ));
            jn_lines = json_array();
        }
    }

    if(nread == 0) {
        /*
         *  A last line with no newline is a line. nginx does not write one,
         *  but a truncated file does, and dropping it silently is how a
         *  reader loses the newest record of the day.
         */
        size_t pending = istream_length(priv->istream);
        if(pending > 0) {
            char *p = istream_cur_rd_pointer(priv->istream);
            if(p) {
                json_array_append_new(jn_lines, json_stringn(p, pending));
                priv->lines++;
            }
        }
    }

    if(json_array_size(jn_lines) > 0) {
        gobj_publish_event(gobj, EV_LOG_LINES, json_pack("{s:s, s:o}",
            "path", priv->path,
            "lines", jn_lines
        ));
    } else {
        JSON_DECREF(jn_lines)
    }

    return (nread == 0)? 1:0;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  Read the next chunk, or report what mt_start could not do.
 ***************************************************************************/
PRIVATE int ac_read_chunk(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->fd < 0) {
        publish_error(gobj, "cannot open file");
        KW_DECREF(kw)
        return 0;
    }

    int ret = read_chunk(gobj);
    if(ret < 0) {
        // Error already published
        KW_DECREF(kw)
        return 0;
    }

    if(ret == 0) {
        gobj_post_message(gobj, EV_READ_CHUNK, 0);
        KW_DECREF(kw)
        return 0;
    }

    if(!priv->eof_published) {
        priv->eof_published = TRUE;

        if(gobj_trace_level(gobj) & TRACE_READ) {
            gobj_trace_msg(gobj, "log_reader: eof '%s', %llu lines",
                priv->path, (unsigned long long)priv->lines
            );
        }

        gobj_publish_event(gobj, EV_LOG_EOF, json_pack("{s:s, s:I, s:I, s:I}",
            "path", priv->path,
            "lines", (json_int_t)priv->lines,
            "bytes", (json_int_t)priv->bytes,
            "too_long", (json_int_t)priv->too_long
        ));
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create  = mt_create,
    .mt_destroy = mt_destroy,
    .mt_start   = mt_start,
    .mt_stop    = mt_stop,
    .mt_writing = mt_writing,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_LOG_READER);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_LOG_LINES);
GOBJ_DEFINE_EVENT(EV_LOG_EOF);
GOBJ_DEFINE_EVENT(EV_LOG_ERROR);
GOBJ_DEFINE_EVENT(EV_READ_CHUNK);

/***************************************************************************
 *          Create the GClass
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL,
            "msg",      "%s", "GClass ALREADY created",
            "gclass",   "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*------------------------*
     *      States
     *------------------------*/
    ev_action_t st_idle[] = {
        {EV_READ_CHUNK,             ac_read_chunk,           0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_READ_CHUNK,             0},
        {EV_LOG_LINES,              EVF_OUTPUT_EVENT},
        {EV_LOG_EOF,                EVF_OUTPUT_EVENT},
        {EV_LOG_ERROR,              EVF_OUTPUT_EVENT},
        {NULL, 0}
    };

    /*----------------------------------------*
     *          Register GClass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0, // local methods
        attrs_table,
        sizeof(PRIVATE_DATA),
        0, // authz_table,
        0, // command_table,
        s_user_trace_level,
        0 // gcflags
    );
    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC int register_c_log_reader(void)
{
    return create_gclass(C_LOG_READER);
}
