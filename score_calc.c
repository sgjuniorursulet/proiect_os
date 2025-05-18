
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define USERNAME_SIZE 32
#define MAX_USERS 100

typedef struct {
    char username[USERNAME_SIZE];
    int total_value;
} UserScore;

typedef struct {
    char id[16];
    char username[USERNAME_SIZE];
    float latitude;
    float longitude;
    char clue[128];
    int value;
} Treasure;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <hunt_id>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char filename[256];
    snprintf(filename, sizeof(filename), "%s/treasures.dat", argv[1]);

    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "[DEBUG] Failed to open file: %s. Error: %s\n", filename, strerror(errno));
        return EXIT_FAILURE;
    }

    UserScore users[MAX_USERS];
    int user_count = 0;
    memset(users, 0, sizeof(users));

    Treasure t;
    while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
        int found = 0;
        for (int i = 0; i < user_count; i++) {
            if (strcmp(users[i].username, t.username) == 0) {
                users[i].total_value += t.value;
                found = 1;
                break;
            }
        }
        if (!found && user_count < MAX_USERS) {
            strncpy(users[user_count].username, t.username, USERNAME_SIZE);
            users[user_count].total_value = t.value;
            user_count++;
        }
    }

    if (user_count == 0) {
        fprintf(stderr, "[DEBUG] No users found in hunt %s.\n", argv[1]);
    }

    close(fd);

    for (int i = 0; i < user_count; i++) {
        printf("%s: %d\n", users[i].username, users[i].total_value);
    }

    fflush(stdout);

    return EXIT_SUCCESS;
}