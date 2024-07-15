#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h> 
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>
#include <limits.h>
#include <pwd.h>
#include <grp.h>
#include <libgen.h>
#include <pthread.h>

#define PORT 9090
#define BUFFER_SIZE 1024

char buffer[BUFFER_SIZE];

void error(char* text) {
    perror(text);
    exit(EXIT_FAILURE);
}

int command_exists(const char* command) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "which %s > /dev/null 2>&1", command);
    return system(cmd) == 0;
}

void pipeline_simulator(char* command1, char* command2) {
    int pfd[2];

    if (pipe(pfd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) { 
        close(pfd[0]); 

        dup2(pfd[1], STDOUT_FILENO); // Redirect stdout to pipe
        close(pfd[1]); 

        execlp(command1, command1, NULL); // Execute first command
        perror("exec command1");
        exit(EXIT_FAILURE);
    } else { 
        close(pfd[1]);

        dup2(pfd[0], STDIN_FILENO); // Redirect stdin to pipe
        close(pfd[0]); 

        execlp(command2, command2, NULL); // Execute second command
        perror("exec command2");
        exit(EXIT_FAILURE);
    }
}

void my_ls_a(const char* path) {
    DIR* dir = opendir(path);
    if (dir == NULL) {
        perror("opendir");
        return;
    }

    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL) {
        printf("%s\n", ent->d_name);
    }

    closedir(dir);
}

void print_permissions(mode_t val) {
    printf((S_ISDIR(val)) ? "d" : "-");
    printf((val & S_IRUSR) ? "r" : "-");
    printf((val & S_IWUSR) ? "w" : "-");
    printf((val & S_IXUSR) ? "x" : "-");
    printf((val & S_IRGRP) ? "r" : "-");
    printf((val & S_IWGRP) ? "w" : "-");
    printf((val & S_IXGRP) ? "x" : "-");
    printf((val & S_IROTH) ? "r" : "-");
    printf((val & S_IWOTH) ? "w" : "-");
    printf((val & S_IXOTH) ? "x " : " ");
}

void my_ls_l(const char* path) {
    DIR* dir = opendir(path);
    if (dir == NULL) {
        perror("opendir");
        return;
    }

    struct dirent* ent;
    struct stat filestat;
    struct passwd* pw;
    struct group* gr;
    char timebuf[80];

    while ((ent = readdir(dir)) != NULL) {
        char filename[PATH_MAX];
        snprintf(filename, PATH_MAX, "%s/%s", path, ent->d_name);

        if (stat(filename, &filestat) < 0) {
            perror("stat");
            continue;
        }

        print_permissions(filestat.st_mode);
        printf("%hu ", filestat.st_nlink);
        

        pw = getpwuid(filestat.st_uid);
        gr = getgrgid(filestat.st_gid);
        printf("%s %s ", pw->pw_name, gr->gr_name);

        printf("%lld ", (long long)filestat.st_size);

        strftime(timebuf, sizeof(timebuf), "%b %d %H:%M", localtime(&filestat.st_mtime));
        printf("%s ", timebuf);

        printf("%s\n", ent->d_name);
    }

    closedir(dir);
}

void* thread(void* args) {
    int new_fd = *(int*)args;

    while (1) {
        int r_bytes = recv(new_fd, buffer, BUFFER_SIZE, 0);
        if (r_bytes < 0) {
            perror("recv");
            break;
        }

        buffer[r_bytes] = '\0';
        char* token = strtok(buffer, " \n");
        char* argument = strtok(NULL, " \n");

        if (token == NULL) {
            continue;
        }

        if (!command_exists(token)) {
            printf("Command '%s' does not exist\n", token);
            continue;
        }

        if (strcmp(token, "ls") == 0) {
            if (argument && strcmp(argument, "-a") == 0) {
                my_ls_a(".");
            } else if (argument && strcmp(argument, "-l") == 0) {
                my_ls_l(".");
            } else {
                system(buffer);
            }
        } else if (argument != NULL) {
            pipeline_simulator(token, argument);
        } else {
            system(buffer);
        }
    }

    close(new_fd);
    return NULL;
}

int main() {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_size = sizeof(client_addr);
    int sock_fd, new_fd, reuse = 1;
    pthread_t tid;

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        error("socket");
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        error("setsockopt");
    }

    if (bind(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        error("bind");
    }

    if (listen(sock_fd, 5) < 0) {
        error("listen");
    }

    while (1) {
        new_fd = accept(sock_fd, (struct sockaddr*)&client_addr, &client_size);
        if (new_fd < 0) {
            perror("accept");
            continue;
        }

        if (pthread_create(&tid, NULL, thread, &new_fd) != 0) {
            perror("pthread_create");
            close(new_fd);
        }
    }

    close(sock_fd);
    return 0;
}
