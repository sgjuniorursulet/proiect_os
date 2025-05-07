#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#define COMMAND_FILE "command.txt"

volatile sig_atomic_t received_signal = 0;

void handle_signal(int sig) {
    received_signal = sig;
}

void process_command() {
    char buffer[256];
    int fd = open(COMMAND_FILE, O_RDONLY);
    if (fd < 0) {
        perror("open command.txt");
        return;
    }
    ssize_t n = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);
    if (n <= 0) return;
    buffer[n] = '\0';

    if (strncmp(buffer, "list_hunts", 10) == 0) {
        printf("[Monitor] Listing hunts...\n");
        system("ls -d */ | cut -f1 -d'/'");
    } else if (strncmp(buffer, "list_treasures", 14) == 0) {
        char *hunt_id = strchr(buffer, ' ');
        if (hunt_id) {
            hunt_id++;
            printf("[Monitor] Listing treasures in hunt: %s\n", hunt_id);
            char cmd[300];
            snprintf(cmd, sizeof(cmd), "./manager --list %s", hunt_id);
            system(cmd);
        }
    } else if (strncmp(buffer, "view_treasure", 13) == 0) {
        char *args = strchr(buffer, ' ');
        if (args) {
            args++;
            char *tid = strchr(args, ' ');
            if (tid) {
                *tid = '\0';
                tid++;
                printf("[Monitor] Viewing treasure %s in hunt %s\n", tid, args);
                char cmd[300];
                snprintf(cmd, sizeof(cmd), "./manager --view %s %s", args, tid);
                system(cmd);
            }
        }
    } else if (strncmp(buffer, "stop", 4) == 0) {
        printf("[Monitor] Stopping...\n");
        usleep(2000000); // Delay to simulate cleanup
        exit(0);
    } else {
        printf("[Monitor] Unknown command: %s\n", buffer);
    }
}

int main() {
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    printf("[Monitor] Ready. PID: %d\n", getpid());
    while (1) {
        pause();
        if (received_signal) {
            process_command();
            received_signal = 0;
        }
    }
    return 0;
}