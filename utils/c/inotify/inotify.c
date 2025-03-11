/****************************************************************************
 *              inotify.c
 *
 *              Utility to test inotify
 *
 *              Copyright (c) 2024, ArtGins.
 *              All Rights Reserved.
 ****************************************************************************/
#include <liburing.h>
#include <sys/inotify.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <jansson.h>
#include <helpers.h>
#include <ansi_escape_codes.h>

typedef struct {
    uint32_t bit;
    char *name;
    char *description;
} bits_table_t;

bits_table_t bits_table[] = {
{IN_ACCESS,         "IN_ACCESS",        "File was accessed"},
{IN_MODIFY,         "IN_MODIFY",        "File was modified"},
{IN_ATTRIB,         "IN_ATTRIB",        "Metadata changed"},
{IN_CLOSE_WRITE,    "IN_CLOSE_WRITE",   "Writtable file was closed"},
{IN_CLOSE_NOWRITE,  "IN_CLOSE_NOWRITE", "Unwrittable file closed"},
{IN_OPEN,           "IN_OPEN",          "File was opened"},
{IN_MOVED_FROM,     "IN_MOVED_FROM",    "File was moved from X"},
{IN_MOVED_TO,       "IN_MOVED_TO",      "File was moved to Y"},
{IN_CREATE,         "IN_CREATE",        "Subfile was created"},
{IN_DELETE,         "IN_DELETE",        "Subfile was deleted"},
{IN_DELETE_SELF,    "IN_DELETE_SELF",   "Self was deleted"},
{IN_MOVE_SELF,      "IN_MOVE_SELF",     "Self was moved"},

//" Events sent by the kernel"
{IN_UNMOUNT,        "IN_UNMOUNT",       "Backing fs was unmounted"},
{IN_Q_OVERFLOW,     "IN_Q_OVERFLOW",    "Event queued overflowed"},
{IN_IGNORED,        "IN_IGNORED",       "File was ignored"},

// " Special flags"
{IN_ONLYDIR,        "IN_ONLYDIR",       "Only watch the path if it is a directory"},
{IN_DONT_FOLLOW,    "IN_DONT_FOLLOW",   "Do not follow a sym link"},
{IN_EXCL_UNLINK,    "IN_EXCL_UNLINK",   "Exclude events on unlinked objects"},
{IN_MASK_CREATE,    "IN_MASK_CREATE",   "Only create watches"},
{IN_MASK_ADD,       "IN_MASK_ADD",      "Add to the mask of an already existing watch"},
{IN_ISDIR,          "IN_ISDIR",         "Event occurred against dir"},
{IN_ONESHOT,        "IN_ONESHOT",       "Only send event once"},
{0,0,0}
};

typedef struct {
    int wd;		/* Watch descriptor.  */
    char path[PATH_MAX];
} tracked_paths_t;


json_t *jn_tracked_paths = 0;

int inotify_fd = -1;

int add_watch(const char *path)
{
    json_t *watch = json_object_get(jn_tracked_paths, path);
    if(watch) {
        printf("%sERROR%s Watch directory EXISTS '%s'\n", On_Red BWhite, Color_Off, path);
        return -1;
    }

    BOOL all = 0;
    uint32_t mask = all? IN_ALL_EVENTS:
        IN_DELETE_SELF|IN_MOVE_SELF|IN_CREATE|IN_DELETE;
    mask |= IN_DONT_FOLLOW | IN_EXCL_UNLINK;

    int wd = inotify_add_watch(inotify_fd, path, mask);
    if (wd == -1) {
        printf("%sERROR%s inotify_add_watch '%s' %s\n", On_Red BWhite, Color_Off, path, strerror(errno));
    } else {
        printf("  Watching directory: %s\n", path);
    }
    json_object_set_new(jn_tracked_paths, path, json_integer(wd));
    return wd;
}

int remove_watch(int wd)
{
    const char *path; json_t *jn_wd; void *n;
    json_object_foreach_safe(jn_tracked_paths, n, path, jn_wd) {
        int wd_ = (int)json_integer_value(jn_wd);
        if(wd_ == wd) {
            if(inotify_rm_watch(inotify_fd, wd)<0) {
                if(errno != EINVAL) { // Han borrado el directorio
                    printf("%sERROR%s inotify_rm_watch %d '%s' %s\n", On_Red BWhite, Color_Off, wd, path,
                           strerror(errno));
                }
            }
            printf("  Unwatching directory: %s\n", path);
            json_object_del(jn_tracked_paths, path);
            return 0;
        }

    }
    return -1;
}

const char *get_path(int wd, BOOL verbose)
{
    const char *path; json_t *jn_wd; void *n;
    json_object_foreach_safe(jn_tracked_paths, n, path, jn_wd) {
        int wd_ = (int)json_integer_value(jn_wd);
        if(wd_ == wd) {
            return path;
        }

    }
    if(verbose) {
        printf("%sERROR%s wd not found '%d'\n", On_Red BWhite, Color_Off, wd);
    }
    return NULL;
}

int get_wd(const char *path, BOOL verbose)
{
    const char *path_; json_t *jn_wd;
    json_object_foreach(jn_tracked_paths, path_, jn_wd) {
        if(strcmp(path, path_)==0) {
            int wd = (int)json_integer_value(jn_wd);
            return wd;
        }

    }
    if(verbose) {
        printf("%sERROR%s path not found '%s'\n", On_Red BWhite, Color_Off, path);
    }
    return -1;
}


// Function to handle inotify events
void handle_inotify_event(struct inotify_event *event)
{
    const char *path;
    char full_path[PATH_MAX];

    printf("  ev: %d '%s',", event->wd, event->len? event->name:"");
    for(int i=0; i< sizeof(bits_table)/sizeof(bits_table[0]); i++) {
        bits_table_t entry = bits_table[i];
        if(entry.bit & event->mask) {
            printf(" - %s (%s)", entry.name, entry.description);
        }
    }
    printf("\n");

    /*
     *  being:
     *      "tr_topic_pkey_integer/topic_pkey_integer/keys": 7,
     *      "tr_topic_pkey_integer/topic_pkey_integer/keys/0000000000000000002": 8,
     *
     *  TRICK when a watched directory .../keys/0000000000000000002 is deleted, the events are:
     *
     *      - IN_DELETE_SELF 8  ""  with the wd you can know what directory is. !!!HACK use this!!!
     *      - IN_IGNORED     8  ""  the wd of deleted directory has been removed of watchers
     *
     *      - IN_DELETE      7  "0000000000000000002"
     *                          comes with the wd of the directory parent (keys),
     *                          informing that his child has been deleted (0000000000000000002).
     *                          but in a tree, in the final first subdirectories deleting
     *                          this event is not arriving.
     *
     *      HACK don't use IN_MODIFY in intense writing, cause IN_Q_OVERFLOW and event lost.
     */


    if(event->mask & (IN_DELETE_SELF)) {
        // The directory is removed or moved
        if((path=get_path(event->wd, true)) != NULL) {
            printf("  %s-> Directory deleted:%s %d %s\n", On_Green BWhite, Color_Off, event->wd, path);

            remove_watch(event->wd);
            print_json2("PATHS", jn_tracked_paths);
        }
        return;
    }

    if(event->mask & (IN_IGNORED)) {
        // The Watch was removed
        if((path=get_path(event->wd, false)) != NULL) {
            printf("%sERROR%s wd yet found %d %s '%s'\n", On_Red BWhite, Color_Off,
                event->wd,
                event->len? event->name:"",
                path
            );
        }
        return;
    }

    path = get_path(event->wd, true);

    if(event->mask & (IN_ISDIR)) {
        /*
         *  Directory
         */
        if (event->mask & (IN_CREATE)) {
            snprintf(full_path, PATH_MAX, "%s/%s", path, event->len? event->name:"");
            printf("  %s-> Directory created:%s %s\n", On_Green BWhite, Color_Off, full_path);
            add_watch(full_path);
            print_json2("PATHS", jn_tracked_paths);
        }
        if (event->mask & (IN_DELETE)) {
            snprintf(full_path, PATH_MAX, "%s/%s", path, event->len? event->name:"");
            printf("  %s-> Directory deleted:%s %s\n", On_Green BWhite, Color_Off, full_path);
        }
    } else {
        /*
         *  File
         */
        path = get_path(event->wd, true);
        snprintf(full_path, PATH_MAX, "%s/%s", path, event->len? event->name:"");
        if (event->mask & (IN_CREATE)) {
            printf("  %s-> File created:%s %s\n", On_Green BWhite, Color_Off, full_path);
        }
        if (event->mask & (IN_DELETE)) {
            printf("  %s-> File deleted:%s %s\n", On_Green BWhite, Color_Off, full_path);
        }
        if (event->mask & (IN_MODIFY)) {
            printf("  %s-> File modified:%s %s\n", On_Green BWhite, Color_Off, full_path);
        }
    }

}

// Recursively add inotify watches to all subdirectories
PRIVATE BOOL search_by_paths_cb(
    hgobj gobj,
    void *user_data,
    wd_found_type type,     // type found
    char *fullpath,         // directory+filename found
    const char *directory,  // directory of found filename
    char *name,             // dname[255]
    int level,              // level of tree where file found
    wd_option opt           // option parameter
)
{
    add_watch(fullpath);
    return true; // to continue
}

void add_watch_recursive(const char *path)
{
    add_watch(path);
    walk_dir_tree(
        0,
        path,
        0,
        WD_RECURSIVE|WD_MATCH_DIRECTORY,
        search_by_paths_cb,
        0
    );
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <directory>\n", argv[0]);
        return 1;
    }

    char path[PATH_MAX];
    build_path(path, sizeof(path), argv[1], NULL);
    if(!is_directory(path)) {
        printf("%sERROR%s path not found '%s'\n", On_Red BWhite, Color_Off, path);
        exit(EXIT_FAILURE);
    }

    // Initialize inotify
    inotify_fd = inotify_init1(IN_NONBLOCK|IN_CLOEXEC);
    if (inotify_fd == -1) {
        printf("%sERROR%s inotify_init1() '%s'\n", On_Red BWhite, Color_Off, strerror(errno));
        exit(EXIT_FAILURE);
    }

    jn_tracked_paths = json_object();

    // Add watch recursively on the root directory and its subdirectories
    add_watch_recursive(path);

    print_json2("PATHS", jn_tracked_paths);

    // Initialize io_uring
    struct io_uring ring;
    if (io_uring_queue_init(32, &ring, 0) < 0) {
        printf("%sERROR%s io_uring_queue_init() '%s'\n", On_Red BWhite, Color_Off, strerror(errno));
        exit(EXIT_FAILURE);
    }

    char buffer[sizeof(struct inotify_event) + NAME_MAX + 1];

    while (1) {
        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        if (!sqe) {
            printf("%sERROR%s io_uring_get_sqe() '%s'\n", On_Red BWhite, Color_Off, strerror(errno));
            break;
        }

        // Prepare read operation for inotify events
        io_uring_prep_read(sqe, inotify_fd, buffer, sizeof(buffer), 0);

        // Submit the SQE
        io_uring_submit(&ring);

        // Wait for completion
        struct io_uring_cqe *cqe;
        io_uring_wait_cqe(&ring, &cqe);

        if (cqe->res > 0) {
            int len = cqe->res;
            char *ptr = buffer;
            printf("==>\n");
            while (ptr < buffer + len) {
                struct inotify_event *event = (struct inotify_event *) ptr;

                // Handle the file modification event
                handle_inotify_event(event);

                ptr += sizeof(struct inotify_event) + event->len;
            }
            printf("<==\n\n");
        } else if (cqe->res < 0) {
            printf("%sERROR%s io_uring cqe '%s'\n", On_Red BWhite, Color_Off, strerror(-cqe->res));
            exit(EXIT_FAILURE);
        }

        io_uring_cqe_seen(&ring, cqe);
    }

    close(inotify_fd);
    io_uring_queue_exit(&ring);

    return 0;
}
