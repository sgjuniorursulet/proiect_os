#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>


#define USERNAME_SIZE 32
#define CLUE_SIZE 128
#define TREASURE_FILE "treasures.dat"
#define LOG_FILE "logged_hunt"
#define SYMLINK_PREFIX "logged_hunt-"

typedef struct {
    char id[16];
    char username[USERNAME_SIZE];
    float latitude;
    float longitude;
    char clue[CLUE_SIZE];
    int value;
    char _padding[60];
} Treasure;

void log_operation(const char *hunt_id, const char *operation) {
    char log_path[256];
    snprintf(log_path, sizeof(log_path), "%s/%s", hunt_id, LOG_FILE);
    FILE *log = fopen(log_path, "a");
    if (log) {
        time_t now = time(NULL);
        fprintf(log, "%s: %s\n", ctime(&now), operation);
        fclose(log);
    }
}

void create_symlink(const char *hunt_id) {
    char target[256], linkname[256];
    snprintf(target, sizeof(target), "%s/%s", hunt_id, LOG_FILE);
    snprintf(linkname, sizeof(linkname), "%s%s", SYMLINK_PREFIX, hunt_id);
    symlink(target, linkname);
}

void add_treasure(const char *hunt_id) {
    if (mkdir(hunt_id, 0755) == -1 && errno != EEXIST) {
        perror("mkdir");
        exit(1);
    }

    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s", hunt_id, TREASURE_FILE);
    int fd = open(filepath, O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (fd < 0) { perror("open"); exit(1); }

    Treasure t;
    printf("Enter Treasure ID: "); scanf("%15s", t.id);
    printf("Enter Username: "); scanf("%31s", t.username);
    printf("Enter Latitude: "); scanf("%f", &t.latitude);
    printf("Enter Longitude: "); scanf("%f", &t.longitude);
    printf("Enter Clue: "); getchar(); fgets(t.clue, CLUE_SIZE, stdin);
    t.clue[strcspn(t.clue, "\n")] = 0;
    printf("Enter Value: "); scanf("%d", &t.value);

    if (write(fd, &t, sizeof(Treasure)) != sizeof(Treasure)) {
        perror("write");
    }
    close(fd);

    log_operation(hunt_id, "Added treasure");
    create_symlink(hunt_id);
}

void list_treasures(const char *hunt_id) {
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s", hunt_id, TREASURE_FILE);

    struct stat st;
    if (stat(filepath, &st) == 0) {
        printf("Hunt: %s\nSize: %ld bytes\nLast modified: %s\n",
            hunt_id, st.st_size, ctime(&st.st_mtime));
    }

    int fd = open(filepath, O_RDONLY);
    if (fd < 0) { perror("open"); return; }

    Treasure t;
    while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
        printf("ID: %s | User: %s | Lat: %.2f | Lon: %.2f | Value: %d\n",
               t.id, t.username, t.latitude, t.longitude, t.value);
    }
    close(fd);
    log_operation(hunt_id, "Listed treasures");
}

void view_treasure(const char *hunt_id, const char *tid) {
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s", hunt_id, TREASURE_FILE);

    int fd = open(filepath, O_RDONLY);
    if (fd < 0) { perror("open"); return; }

    Treasure t;
    while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
        if (strcmp(t.id, tid) == 0) {
            printf("ID: %s\nUser: %s\nLat: %.2f\nLon: %.2f\nClue: %s\nValue: %d\n",
                   t.id, t.username, t.latitude, t.longitude, t.clue, t.value);
            close(fd);
            log_operation(hunt_id, "Viewed a treasure");
            return;
        }
    }
    printf("Treasure not found.\n");
    close(fd);
}

void remove_treasure(const char *hunt_id, const char *tid) {
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s", hunt_id, TREASURE_FILE);

    int fd = open(filepath, O_RDWR);
    if (fd < 0) { perror("open"); return; }

    Treasure t;
    off_t pos = 0;
    int found = 0;

    while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
        if (strcmp(t.id, tid) == 0) {
            found = 1;
            break;
        }
        pos += sizeof(Treasure);
    }

    if (!found) {
        printf("Treasure not found.\n");
        close(fd);
        return;
    }

    off_t file_size = lseek(fd, 0, SEEK_END);
    off_t last_record_pos = file_size - sizeof(Treasure);

    if (pos != last_record_pos) {
        Treasure last_t;
        lseek(fd, last_record_pos, SEEK_SET);
        if (read(fd, &last_t, sizeof(Treasure)) != sizeof(Treasure)) {
            perror("read last");
            close(fd);
            return;
        }

        lseek(fd, pos, SEEK_SET);
        if (write(fd, &last_t, sizeof(Treasure)) != sizeof(Treasure)) {
            perror("write swapped");
            close(fd);
            return;
        }
    }

    if (ftruncate(fd, file_size - sizeof(Treasure)) == -1) {
        perror("ftruncate");
    }

    close(fd);
    log_operation(hunt_id, "Removed a treasure");
}

void remove_hunt(const char *hunt_id) {
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s", hunt_id, TREASURE_FILE);
    unlink(filepath);

    snprintf(filepath, sizeof(filepath), "%s/%s", hunt_id, LOG_FILE);
    unlink(filepath);

    rmdir(hunt_id);

    char linkname[256];
    snprintf(linkname, sizeof(linkname), "%s%s", SYMLINK_PREFIX, hunt_id);
    unlink(linkname);
    printf("Hunt %s removed.\n", hunt_id);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s --<operation> <hunt_id> [<treasure_id>]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "--add") == 0) {
        add_treasure(argv[2]);
    } else if (strcmp(argv[1], "--list") == 0) {
        list_treasures(argv[2]);
    } else if (strcmp(argv[1], "--view") == 0 && argc == 4) {
        view_treasure(argv[2], argv[3]);
    } else if (strcmp(argv[1], "--remove_treasure") == 0 && argc == 4) {
        remove_treasure(argv[2], argv[3]);
    } else if (strcmp(argv[1], "--remove_hunt") == 0) {
        remove_hunt(argv[2]);
    } else {
        fprintf(stderr, "Invalid command or missing arguments.\n");
        return 1;
    }

    return 0;
}