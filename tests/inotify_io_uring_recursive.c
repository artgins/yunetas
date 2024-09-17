#include <liburing.h>
#include <sys/inotify.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

#define BUF_LEN 4096

// Function to handle inotify events
void handle_inotify_event(struct inotify_event *event) {
    if (event->mask & IN_CREATE) {
        printf("Created: %s\n", event->name);
        if (event->mask & IN_ISDIR) {
            printf("Directory created: %s\n", event->name);
        }
    }
    if (event->mask & IN_MODIFY)
        printf("Modified: %s\n", event->name);
    if (event->mask & IN_DELETE) {
        printf("Deleted: %s\n", event->name);
        if (event->mask & IN_ISDIR) {
            printf("Directory deleted: %s\n", event->name);
        }
    }
}

// Recursively add inotify watches to all subdirectories
void add_watch_recursive(int inotify_fd, const char *base_path) {
    DIR *dir;
    struct dirent *entry;

    if ((dir = opendir(base_path)) == NULL) {
        perror("opendir");
        return;
    }

    // Add watch for the base directory
    int wd = inotify_add_watch(inotify_fd, base_path, IN_CREATE | IN_MODIFY | IN_DELETE);
    if (wd == -1) {
        perror("inotify_add_watch");
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
            add_watch_recursive(inotify_fd, path);
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

    // Initialize inotify
    int inotify_fd = inotify_init1(IN_NONBLOCK);
    if (inotify_fd == -1) {
        perror("inotify_init1");
        exit(EXIT_FAILURE);
    }

    // Add watch recursively on the root directory and its subdirectories
    add_watch_recursive(inotify_fd, watched_dir);

    // Initialize io_uring
    struct io_uring ring;
    if (io_uring_queue_init(32, &ring, 0) < 0) {
        perror("io_uring_queue_init");
        exit(EXIT_FAILURE);
    }

    char buffer[BUF_LEN];
    struct iovec iov = {
        .iov_base = buffer,
        .iov_len = sizeof(buffer),
    };

    while (1) {
        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        if (!sqe) {
            fprintf(stderr, "Could not get SQE\n");
            break;
        }

        // Prepare read operation for inotify events
        io_uring_prep_readv(sqe, inotify_fd, &iov, 1, 0);

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

                // Handle the event (create, modify, delete)
                handle_inotify_event(event);

                // If a directory is created, add a watch for the new subdirectory
                if (event->mask & IN_CREATE && event->mask & IN_ISDIR) {
                    char new_dir_path[PATH_MAX];
                    snprintf(new_dir_path, PATH_MAX, "%s/%s", watched_dir, event->name);
                    add_watch_recursive(inotify_fd, new_dir_path);
                }

                ptr += sizeof(struct inotify_event) + event->len;
            }
        } else if (cqe->res < 0) {
            fprintf(stderr, "Error reading from inotify fd: %s\n", strerror(-cqe->res));
        }

        io_uring_cqe_seen(&ring, cqe);
    }

    // Cleanup
    close(inotify_fd);
    io_uring_queue_exit(&ring);

    return 0;
}
