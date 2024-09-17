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

// Track file sizes before modification
typedef struct {
    char path[PATH_MAX];
    off_t size;
} FileInfo;

// Map to hold file sizes (this is just a simple array, you can optimize it with a more efficient data structure)
FileInfo tracked_files[100];
int tracked_files_count = 0;

// Find the file info by path
FileInfo* find_tracked_file(const char *path) {
    for (int i = 0; i < tracked_files_count; i++) {
        if (strcmp(tracked_files[i].path, path) == 0) {
            return &tracked_files[i];
        }
    }
    return NULL;
}

// Add a file to the tracked list (for file size tracking)
void add_tracked_file(const char *path) {
    FileInfo *file_info = find_tracked_file(path);
    if (!file_info) {
        strncpy(tracked_files[tracked_files_count].path, path, PATH_MAX);
        tracked_files[tracked_files_count].size = 0;
        tracked_files_count++;
    }
}

// Update file size after modification
void update_file_size(const char *path) {
    FileInfo *file_info = find_tracked_file(path);
    if (file_info) {
        struct stat st;
        if (stat(path, &st) == 0) {
            file_info->size = st.st_size;
        }
    }
}

// Read appended data from the file
void read_appended_data(const char *path, off_t old_size) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        perror("open");
        return;
    }

    // Seek to the old file size
    lseek(fd, old_size, SEEK_SET);

    // Calculate the new size
    struct stat st;
    if (stat(path, &st) == 0) {
        off_t new_size = st.st_size;
        off_t added_size = new_size - old_size;

        // Read the newly appended data
        char *buffer = malloc(added_size + 1);
        ssize_t bytes_read = read(fd, buffer, added_size);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0'; // Null terminate the buffer
            printf("Appended data to %s: %s\n", path, buffer);
        }
        free(buffer);
    }

    close(fd);
}

// Function to handle inotify events
void handle_inotify_event(struct inotify_event *event, const char *watched_dir) {
    char file_path[PATH_MAX];
    snprintf(file_path, PATH_MAX, "%s/%s", watched_dir, event->name);

    if (event->mask & IN_MODIFY) {
        printf("Modify: %s\n", event->name);
        // Find the old size of the file
        FileInfo *file_info = find_tracked_file(file_path);
        if (file_info) {
            off_t old_size = file_info->size;

            // Update file size after modification
            update_file_size(file_path);

            // Get the new size and calculate the difference
            struct stat st;
            if (stat(file_path, &st) == 0 && st.st_size > old_size) {
                read_appended_data(file_path, old_size); // Read the newly added part of the file
            }
        } else {
            // If the file is not being tracked, add it to the list and start tracking it
            add_tracked_file(file_path);
            update_file_size(file_path);
        }
    }

    if (event->mask & IN_CREATE) {
        printf("Created: %s\n", event->name);
        if (event->mask & IN_ISDIR) {
            printf("Directory created: %s\n", event->name);
        }
    }

    if (event->mask & IN_DELETE) {
        printf("Deleted: %s\n", event->name);
        if (event->mask & IN_ISDIR) {
            printf("Directory deleted: %s\n", event->name);
        }
    }

    if (event->mask & IN_MOVED_FROM) {
        printf("Moved from: %s\n", event->name);
        if (event->mask & IN_ISDIR) {
            printf("Directory moved from: %s\n", event->name);
        }
    }

    if (event->mask & IN_MOVED_TO) {
        printf("Moved to: %s\n", event->name);
        if (event->mask & IN_ISDIR) {
            printf("Directory moved to: %s\n", event->name);
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
    int wd = inotify_add_watch(inotify_fd, base_path, IN_CREATE | IN_MODIFY | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);
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

        // Add the file to the tracking list for size tracking
        add_tracked_file(path);
        update_file_size(path);
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

                // Handle the file modification event
                handle_inotify_event(event, watched_dir);

                ptr += sizeof(struct inotify_event) + event->len;
            }
        } else if (cqe->res < 0) {
            fprintf(stderr, "Error reading from inotify fd: %s\n", strerror(-cqe->res));
        }

        io_uring_cqe_seen(&ring, cqe);
    }

    close(inotify_fd);
    io_uring_queue_exit(&ring);

    return 0;
}
