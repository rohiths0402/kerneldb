#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <poll.h>
#include <unistd.h>
#include <errno.h>
#include "reph.h"
#include "command.h"
#include "dispatcher.h"
#define POLL_TIMEOUT_MS 200
#define REPL_PROMPT "kerneldb> "
#define REPL_VERSION "KernelDB v0.1 — Layer 1: Control Plane"

extern char current_db[64];

static volatile sig_atomic_t g_interrupted = 0; 
static volatile sig_atomic_t g_quit = 0;

static void handle_sigint(int sig) {
    (void)sig;
    g_interrupted = 1;
}

static void setup_signals(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;   
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa, NULL) < 0) {
        perror("sigaction");
        exit(1);
    }
    sa.sa_handler = handle_sigint;
    sigaction(SIGTERM, &sa, NULL);
}

static int read_input(char *buf, size_t bufsize) {
    struct pollfd fds[1];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    int ready = poll(fds, 1, POLL_TIMEOUT_MS);

    if (ready < 0) {
        if (errno == EINTR) return 0;
        perror("poll");
        return -1;
    }

    if (ready == 0) {
        return 0;
    }
    if (fgets(buf, (int)bufsize, stdin) == NULL) {
        return -1;
    }
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n')
        buf[len - 1] = '\0';

    return 1;
}
 
void repl_run(void) {
    setup_signals();

    Command cmd;

    printf("\n  kerneldb v0.1 — Embedded DB Engine\n");

    char query[1024] = {0};
    char line[256]; 
    int show_prompt = 1;

    while (!g_quit) {

        if (show_prompt) {
            if(strcmp(current_db , "default") == 0) {
                printf("KernelDB> ");
            } else {
                printf("KernelDB[%s]> ", current_db);
            }
            fflush(stdout);
            show_prompt = 0;
        }

        int status = read_input(line, sizeof(line));

        if (status == 0) continue;

        if (g_interrupted) {
            g_interrupted = 0;
            printf("\n  (Interrupted. Type .exit to quit.)\n\n");
            query[0] = '\0';
            show_prompt = 1;
            continue;
        }

        if (status < 0) {
            printf("\nBye.\n");
            break;
        }
        if (strlen(line) == 0) {
            show_prompt = 1;
            continue;
        }
        if (line[0] == '.' ||
            strcasecmp(line, "show db") == 0 ||
            strcasecmp(line, "showdb") == 0) {

            strncpy(cmd.raw, line, sizeof(cmd.raw) - 1);
            cmd.raw[sizeof(cmd.raw) - 1] = '\0';

            cmd.type = CMD_UNKNOWN;
            cmd.args = NULL;

            ExecResult result = dispatch(&cmd);

            if (result == EXEC_EXIT) {
                printf("Bye.\n");
                g_quit = 1;
            }

            show_prompt = 1;
            continue;
        }
        if (strlen(query) == 0 && strchr(line, ';') == NULL) {

            if (strncasecmp(line, "SELECT", 6) != 0 &&
                strncasecmp(line, "INSERT", 6) != 0 &&
                strncasecmp(line, "UPDATE", 6) != 0 &&
                strncasecmp(line, "DELETE", 6) != 0 &&
                strncasecmp(line, "CREATE", 6) != 0 &&
                strncasecmp(line, "DROP", 4) != 0 &&
                strncasecmp(line, "USE", 3) != 0) {

                printf("  Unrecognized command: [%s]\n\n", line);
                show_prompt = 1;
                continue;
            }
        }
        strncat(query, line, sizeof(query) - strlen(query) - 1);
        if (strchr(line, ';')) {

            char *semi = strchr(query, ';');
            if (semi) *semi = '\0';

            strncpy(cmd.raw, query, sizeof(cmd.raw) - 1);
            cmd.raw[sizeof(cmd.raw) - 1] = '\0';

            cmd.type = CMD_UNKNOWN;
            cmd.args = NULL;

            ExecResult result = dispatch(&cmd);

            if (result == EXEC_EXIT) {
                printf("Bye.\n");
                g_quit = 1;
            }

            query[0] = '\0';
            show_prompt = 1;
        }
    }
}