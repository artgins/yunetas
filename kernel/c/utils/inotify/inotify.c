#include <liburing.h>
#include <sys/inotify.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <jansson.h>

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
        printf("ERROR Watch directory EXISTS: %s\n", path);
        return -1;
    }

    int wd = inotify_add_watch(inotify_fd, path, IN_ALL_EVENTS | IN_DONT_FOLLOW | IN_EXCL_UNLINK);
    if (wd == -1) {
        printf("ERROR inotify_add_watch %s\n", strerror(errno));
    } else {
        printf("Watching directory: %s\n", path);
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
                printf("ERROR inotify_rm_watch %s\n", strerror(errno));
            }
            json_object_del(jn_tracked_paths, path);
            return 0;
        }

    }
    return -1;
}

const char *get_path(int wd)
{
    const char *path; json_t *jn_wd; void *n;
    json_object_foreach_safe(jn_tracked_paths, n, path, jn_wd) {
        int wd_ = (int)json_integer_value(jn_wd);
        if(wd_ == wd) {
            return path;
        }

    }
    return NULL;
}


// Function to handle inotify events
void handle_inotify_event(struct inotify_event *event)
{
    const char *path = get_path(event->wd);
    char full_path[PATH_MAX];
    snprintf(full_path, PATH_MAX, "%s/%s", path, event->name);
    printf("==> %s\n",full_path);
    for(int i=0; i< sizeof(bits_table)/sizeof(bits_table[0]); i++) {
        bits_table_t entry = bits_table[i];
        if(entry.bit & event->mask) {
            printf("    - %s (%s)\n", entry.name, entry.description);
        }
    }

    if (event->mask & (IN_CREATE|IN_ISDIR)) {
        printf("Directory created: %s\n", event->name);
        add_watch(full_path);
    }
    if (event->mask & (IN_DELETE|IN_ISDIR)) {
        printf("Directory deleted: %s\n", event->name);
        remove_watch(event->wd);
    }
}

// Recursively add inotify watches to all subdirectories
void add_watch_recursive(const char *base_path) {
    DIR *dir;
    struct dirent *entry;

    if ((dir = opendir(base_path)) == NULL) {
        perror("opendir");
        return;
    }

    // Add watch for the base directory
    int wd = add_watch(base_path);
    if (wd == -1) {
        perror("ERROR inotify_add_watch");
        return;
    } else {
        printf("Watching directory: %s\n", base_path);
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // Construct full path
        char path[PATH_MAX];
        snprintf(path, PATH_MAX, "%s/%s", base_path, entry->d_name);

        // Check if entry is a directory and add watch recursively
        struct stat statbuf;
        if (stat(path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
            add_watch_recursive(path);
        }

    }
    closedir(dir);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <directory>\n", argv[0]);
        return 1;
    }

    char *watched_dir = argv[1];
    if(access(watched_dir, 0)!=0) {
        printf("path not found %s\n", watched_dir);
        exit(EXIT_FAILURE);
    }
    // Initialize inotify
    inotify_fd = inotify_init1(IN_NONBLOCK|IN_CLOEXEC);
    if (inotify_fd == -1) {
        perror("inotify_init1");
        exit(EXIT_FAILURE);
    }

    // Add watch recursively on the root directory and its subdirectories
    add_watch_recursive(watched_dir);

    // Initialize io_uring
    struct io_uring ring;
    if (io_uring_queue_init(32, &ring, 0) < 0) {
        perror("io_uring_queue_init");
        exit(EXIT_FAILURE);
    }

    char buffer[sizeof(struct inotify_event) + NAME_MAX + 1];

    while (1) {
        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        if (!sqe) {
            fprintf(stderr, "Could not get SQE\n");
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
            while (ptr < buffer + len) {
                struct inotify_event *event = (struct inotify_event *) ptr;

                // Handle the file modification event
                handle_inotify_event(event);

                ptr += sizeof(struct inotify_event) + event->len;
            }
            printf("<==\n");
        } else if (cqe->res < 0) {
            fprintf(stderr, "Error reading from inotify fd: %s\n", strerror(-cqe->res));
        }

        io_uring_cqe_seen(&ring, cqe);
    }

    close(inotify_fd);
    io_uring_queue_exit(&ring);

    return 0;
}
