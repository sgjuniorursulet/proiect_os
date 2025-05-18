
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#define COMMAND_FILE "command.txt"

volatile sig_atomic_t received_signal = 0;
static int output_pipe = -1;

void handle_signal(int sig) {
    received_signal = sig;
}

void write_to_pipe(const char *message) {
    if (output_pipe != -1) {
        write(output_pipe, message, strlen(message));
    }
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

    char output[1024] = {0};

    if (strncmp(buffer, "list_hunts", 10) == 0) {
        strcat(output, "[Monitor] Listing hunts...\n");
        FILE *fp = popen("ls -d */ | cut -f1 -d'/'", "r");
        if (fp) {
            char line[256];
            while (fgets(line, sizeof(line), fp)) {
                strcat(output, line);
            }
            pclose(fp);
        }
    } else if (strncmp(buffer, "list_treasures", 14) == 0) {
        char *hunt_id = strchr(buffer, ' ');
        if (hunt_id) {
            hunt_id++;
            snprintf(output, sizeof(output), "[Monitor] Listing treasures in hunt: %s\n", hunt_id);
            char cmd[300];
            snprintf(cmd, sizeof(cmd), "./manager --list %s", hunt_id);
            FILE *fp = popen(cmd, "r");
            if (fp) {
                char line[256];
                while (fgets(line, sizeof(line), fp)) {
                    strncat(output, line, sizeof(output) - strlen(output) - 1);
                }
                pclose(fp);
            }
        }
    } else if (strncmp(buffer, "view_treasure", 13) == 0) {
        char *args = strchr(buffer, ' ');
        if (args) {
            args++;
            char *tid = strchr(args, ' ');
            if (tid) {
                *tid = '\0';
                tid++;
                snprintf(output, sizeof(output), "[Monitor] Viewing treasure %s in hunt %s\n", tid, args);
                char cmd[300];
                snprintf(cmd, sizeof(cmd), "./manager --view %s %s", args, tid);
                FILE *fp = popen(cmd, "r");
                if (fp) {
                    char line[256];
                    while (fgets(line, sizeof(line), fp)) {
                        strncat(output, line, sizeof(output) - strlen(output) - 1);
                    }
                    pclose(fp);
                }
            }
        }
    } else if (strncmp(buffer, "stop", 4) == 0) {
        strcat(output, "[Monitor] Stopping...\n");
        usleep(2000000); 
        write_to_pipe(output);
        exit(0);
    } else {
        snprintf(output, sizeof(output), "[Monitor] Unknown command: %s\n", buffer);
    }

    write_to_pipe(output);
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        output_pipe = atoi(argv[1]);
    }

    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    write_to_pipe("[Monitor] Ready.\n");

    while (1) {
        pause();
        if (received_signal) {
            process_command();
            received_signal = 0;
        }
    }

    return 0;
}
