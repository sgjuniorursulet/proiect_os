#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>

#define COMMAND_FILE "command.txt"
pid_t monitor_pid = -1;
int monitor_running = 0;

void handle_sigchld(int sig) {
    int status;
    waitpid(monitor_pid, &status, 0);
    monitor_pid = -1;
    monitor_running = 0;
}

void send_command(const char *command_line, int signal) {
    if (!monitor_running) {
        printf("Error: Monitor not running.\n");
        return;
    }
    int fd = open(COMMAND_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { perror("open command.txt"); return; }
    write(fd, command_line, strlen(command_line));
    close(fd);
    kill(monitor_pid, signal);
}

int main() {
    struct sigaction sa;
    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);

    char input[256];
    while (1) {
        printf("treasure_hub> ");
        fflush(stdout);
        if (!fgets(input, sizeof(input), stdin)) break;
        input[strcspn(input, "\n")] = 0;

        if (strcmp(input, "start_monitor") == 0) {
            if (monitor_running) {
                printf("Monitor is already running.\n");
                continue;
            }
            monitor_pid = fork();
            if (monitor_pid == 0) {
                execl("./monitor", "monitor", NULL);
                perror("execl");
                exit(1);
            } else if (monitor_pid > 0) {
                monitor_running = 1;
                printf("Monitor started (PID: %d)\n", monitor_pid);
            } else {
                perror("fork");
            }
        } else if (strncmp(input, "list_hunts", 10) == 0) {
            send_command("list_hunts", SIGUSR1);
        } else if (strncmp(input, "list_treasures", 14) == 0) {
            send_command(input, SIGUSR2);
        } else if (strncmp(input, "view_treasure", 13) == 0) {
            send_command(input, SIGTERM);
        } else if (strcmp(input, "stop_monitor") == 0) {
            send_command("stop", SIGINT);
        } else if (strcmp(input, "exit") == 0) {
            if (monitor_running) {
                printf("Error: Monitor still running. Use stop_monitor first.\n");
            } else {
                break;
            }
        } else {
            printf("Unknown command.\n");
        }
    }
    return 0;
}