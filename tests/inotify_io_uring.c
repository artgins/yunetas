#include <liburing.h>
#include <sys/inotify.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define BUF_LEN 4096

void handle_inotify_event(struct inotify_event *event) {
    if (event->mask & IN_CREATE)
        printf("File created: %s\n", event->name);
    if (event->mask & IN_MODIFY)
        printf("File modified: %s\n", event->name);
    if (event->mask & IN_DELETE)
        printf("File deleted: %s\n", event->name);
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

    // Add watch on directory (including IN_DELETE for detecting deletions)
    int wd = inotify_add_watch(inotify_fd, watched_dir, IN_CREATE | IN_MODIFY | IN_DELETE);
    if (wd == -1) {
        perror("inotify_add_watch");
        exit(EXIT_FAILURE);
    }

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
                handle_inotify_event(event);
                ptr += sizeof(struct inotify_event) + event->len;
            }
        } else if (cqe->res < 0) {
            fprintf(stderr, "Error reading from inotify fd: %s\n", strerror(-cqe->res));
        }

        io_uring_cqe_seen(&ring, cqe);
    }

    // Cleanup
    inotify_rm_watch(inotify_fd, wd);
    close(inotify_fd);
    io_uring_queue_exit(&ring);

    return 0;
}
