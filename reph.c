#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <poll.h>
#include <unistd.h>
#include <errno.h>

#include "repl.h"
#include "command.h"
#include "dispatcher.h"

#define _POSIX_C_SOURCE 200809L

static volatile sig_atomic_t g_interrupted = 0; 
static volatile sig_atomic_t g_quit        = 0;

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

#define POLL_TIMEOUT_MS 200

static int read_input(char *buf, size_t bufsize) {
    struct pollfd fds[1];
    fds[0].fd     = STDIN_FILENO;
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

static void print_prompt(void) {
    printf("mydb> ");
    fflush(stdout);
}

void repl_run(void) {
    setup_signals();
    Command cmd;
    printf("\n  mydb v0.1 — Layer 1: Control Plane\n");
    printf("  Type .help for commands, .exit to quit.\n\n");
    while (!g_quit) {
        print_prompt();
        int status;
        do {
            status = read_input(cmd.raw, sizeof(cmd.raw));

            if (g_interrupted) {
                g_interrupted = 0;
                printf("\n  (Interrupted. Type .exit to quit.)\n\n");
                print_prompt();
            }

        } while (status == 0);

        if (status < 0) {
            printf("\nBye.\n");
            break;
        }

        cmd.type = CMD_UNKNOWN;
        cmd.args = NULL;
        ExecResult result = dispatch(&cmd);
        if (result == EXEC_EXIT) {
            printf("Bye.\n");
            g_quit = 1;
        }
    }
}