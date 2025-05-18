
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>

#define MAX_BUFFER 1024
#define COMMAND_FILE "command.txt"

pid_t monitor_pid = -1;
int monitor_pipe_fd = -1;
int monitor_running = 0;

void handle_sigchld(int sig) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (pid == monitor_pid) {
            printf("[INFO] Monitor process (PID: %d) terminated.\n", pid);
            monitor_pid = -1;
            monitor_running = 0;
            if (monitor_pipe_fd != -1) {
                close(monitor_pipe_fd);
                monitor_pipe_fd = -1;
            }
        }
    }
}

void send_command(const char *command_line, int signal) {
    if (monitor_pid == -1) {
        printf("[ERROR] Monitor not running.\n");
        return;
    }

    int fd = open(COMMAND_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { perror("open command.txt"); return; }
    write(fd, command_line, strlen(command_line));
    close(fd);

    kill(monitor_pid, signal);
}

void start_monitor() {
    if (monitor_pid != -1) {
        printf("[ERROR] Monitor is already running.\n");
        return;
    }

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return;
    }

    monitor_pid = fork();
    if (monitor_pid == 0) {
        close(pipefd[0]);
        char fd_str[16];
        snprintf(fd_str, sizeof(fd_str), "%d", pipefd[1]);
        execl("./monitor", "treasure_monitor", fd_str, NULL);
        perror("execl failed");
        exit(EXIT_FAILURE);
    } else if (monitor_pid > 0) {
        close(pipefd[1]);
        monitor_pipe_fd = pipefd[0];
        monitor_running = 1;

        
        int flags = fcntl(monitor_pipe_fd, F_GETFL, 0);
        fcntl(monitor_pipe_fd, F_SETFL, flags | O_NONBLOCK);

        printf("[INFO] Monitor started with PID %d.\n", monitor_pid);
    } else {
        perror("fork");
    }
}

void stop_monitor() {
    if (monitor_pid == -1) {
        printf("[ERROR] Monitor not running.\n");
        return;
    }
    send_command("stop", SIGINT);
}

void read_monitor_output() {
    if (monitor_pipe_fd == -1) return;

    char buffer[MAX_BUFFER];
    ssize_t n;
    while ((n = read(monitor_pipe_fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[n] = '\0';
        printf("%s", buffer);
    }
}

void calculate_scores() {
    DIR *dir = opendir(".");
    if (!dir) {
        perror("opendir");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_DIR || strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char treasure_file[512];
        snprintf(treasure_file, sizeof(treasure_file), "%s/treasures.dat", entry->d_name);
        if (access(treasure_file, F_OK) == -1) continue;

        printf("\n[Hunt: %s]\n", entry->d_name);

        int pipefd[2];
        if (pipe(pipefd) == -1) {
            perror("pipe");
            continue;
        }

        pid_t pid = fork();
        if (pid == 0) {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);
            execl("./score_calc", "score_calc", entry->d_name, NULL);
            perror("execl failed");
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            close(pipefd[1]);

            char buffer[MAX_BUFFER];
            ssize_t n;
            while ((n = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
                buffer[n] = '\0';
                printf("%s", buffer);
            }
            close(pipefd[0]);

            int status;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                fprintf(stderr, "[ERROR] score_calc exited with status %d for hunt %s\n", WEXITSTATUS(status), entry->d_name);
            }
        } else {
            perror("fork");
            close(pipefd[0]);
            close(pipefd[1]);
        }
    }

    closedir(dir);
}

int main() {
    struct sigaction sa;
    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);

    char input[256];
    while (1) {
       
        while (1) {
            read_monitor_output();
            usleep(10000); 
            if (monitor_pipe_fd == -1) break;
            
            char buffer[MAX_BUFFER];
            ssize_t n = read(monitor_pipe_fd, buffer, sizeof(buffer) - 1);
            if (n <= 0) break; 
            buffer[n] = '\0';
            printf("%s", buffer);
        }

        printf("treasure_hub> ");
        fflush(stdout);
        if (!fgets(input, sizeof(input), stdin)) break;
        input[strcspn(input, "\n")] = 0;

        if (strcmp(input, "start_monitor") == 0) {
            start_monitor();
        } else if (strcmp(input, "stop_monitor") == 0) {
            stop_monitor();
        } else if (strcmp(input, "list_hunts") == 0) {
            send_command("list_hunts", SIGUSR1);
        } else if (strncmp(input, "list_treasures", 14) == 0) {
            send_command(input, SIGUSR2);
        } else if (strncmp(input, "view_treasure", 13) == 0) {
            send_command(input, SIGTERM);
        } else if (strcmp(input, "calculate_score") == 0) {
            calculate_scores();
        } else if (strcmp(input, "exit") == 0) {
            if (monitor_pid != -1) {
                printf("[ERROR] Monitor still running. Use stop_monitor first.\n");
            } else {
                break;
            }
        } else {
            printf("Unknown command.\n");
        }
    }

    if (monitor_pipe_fd != -1) close(monitor_pipe_fd);

    return 0;
}
