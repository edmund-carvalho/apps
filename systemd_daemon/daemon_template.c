/**
 * Copyright (c) 2026 Edmund C
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/stat.h>
#include <errno.h>

#include "daemon_template.h"

#if USE_SYSTEMD_NOTIFY
#include <systemd/sd-daemon.h>
#endif

// ---- exit flag (set by SIGTERM/SIGINT) ----
volatile sig_atomic_t g_running = 1;   // if undefined, change to volatile int

// ---- signal handler ----
static void signal_handler(int sig, siginfo_t *info, void *ucontext) {
    if (sig == SIGTERM || sig == SIGINT) {
        g_running = 0;                     // graceful stop
    } else {
        // Fatal signal – log and die immediately
        const char *msg = "Fatal signal, exiting\n";
        write(STDERR_FILENO, msg, strlen(msg));
        _exit(EXIT_FAILURE);
    }
}

static int setup_signals(void) {
    struct sigaction sa = {0};
    sa.sa_flags   = SA_SIGINFO;
    sa.sa_sigaction = signal_handler;

    int sigs[] = {SIGTERM, SIGINT, SIGFPE, SIGSEGV, SIGILL, SIGBUS, SIGHUP};
    for (size_t i = 0; i < sizeof(sigs)/sizeof(sigs[0]); i++) {
        if (sigaction(sigs[i], &sa, NULL) == -1) {
            fprintf(stderr, "sigaction(%d): %s\n", sigs[i], strerror(errno));
            return -1;
        }
    }
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);
    return 0;
}

// ---- PID file helpers ----
static int write_pid(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) { perror("fopen pid"); return -1; }
    fprintf(f, "%ld\n", (long)getpid());
    fclose(f);
    return 0;
}

static int read_pid(const char *path, pid_t *pid) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    if (fscanf(f, "%d", pid) != 1) { fclose(f); return -1; }
    fclose(f);
    return 0;
}

static void remove_pid(const char *path) {
    unlink(path);
}

// ---- classic double‑fork daemonization (only for standalone) ----
static int daemonize(void) {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) _exit(EXIT_SUCCESS);

    if (setsid() < 0) return -1;
    signal(SIGHUP, SIG_IGN);

    pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) _exit(EXIT_SUCCESS);

    umask(0);
    chdir("/");

    int fd = open("/dev/null", O_RDWR);
    if (fd < 0) return -1;
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    if (fd > 2) close(fd);

    return 0;
}

// ---- foreground run (used by --run and systemd) ----
static int run_foreground(void) {
    printf("Starting %s in foreground (pid=%ld)\n", APP_NAME, (long)getpid());
    if (setup_signals() != 0) return EXIT_FAILURE;

    app_context_t ctx = {0};
    if (app_main(&ctx) != 0) {
        fprintf(stderr, "app_main failed\n");
        return EXIT_FAILURE;
    }

#if USE_SYSTEMD_NOTIFY
    sd_notify(0, "READY=1");
#endif

    while (g_running) {
        pause();   // or your event loop
    }

    printf("Shutting down...\n");
    app_shutdown(&ctx);
#if USE_SYSTEMD_NOTIFY
    sd_notify(0, "STOPPING=1");
#endif
    return EXIT_SUCCESS;
}

// ---- daemon start (background) ----
static int start_daemon(void) {
    // Check if already running
    pid_t existing;
    if (read_pid(PID_FILE, &existing) == 0) {
        if (kill(existing, 0) == 0) {
            fprintf(stderr, "Daemon already running (pid %d)\n", existing);
            return 0;
        }
        remove_pid(PID_FILE);  // stale pid
    }

    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) {
        // Parent: wait a moment, then verify child started
        sleep(1);
        pid_t child;
        if (read_pid(PID_FILE, &child) == 0 && kill(child, 0) == 0) {
            printf("Daemon started (pid=%d)\n", child);
            return 0;
        } else {
            fprintf(stderr, "Daemon failed to start\n");
            return -1;
        }
    }

    // Child
    if (daemonize() != 0) _exit(EXIT_FAILURE);
    write_pid(PID_FILE);

    if (setup_signals() != 0) _exit(EXIT_FAILURE);

    app_context_t ctx = {0};
    if (app_main(&ctx) != 0) _exit(EXIT_FAILURE);

    while (g_running) pause();

    app_shutdown(&ctx);
    remove_pid(PID_FILE);
    _exit(EXIT_SUCCESS);
}

static int stop_daemon(void) {
    pid_t pid;
    if (read_pid(PID_FILE, &pid) != 0) {
        fprintf(stderr, "No PID file – daemon not running?\n");
        return 0;
    }
    if (kill(pid, 0) != 0) {
        printf("Process %d not running, removing PID file\n", pid);
        remove_pid(PID_FILE);
        return 0;
    }
    printf("Stopping daemon (pid %d)\n", pid);
    kill(pid, SIGTERM);

    // Wait for shutdown
    for (int i = 0; i < 50; i++) {
        usleep(100000);
        if (kill(pid, 0) != 0) break;
    }
    if (kill(pid, 0) == 0) {
        fprintf(stderr, "Timeout – sending SIGKILL\n");
        kill(pid, SIGKILL);
        sleep(1);
    }
    remove_pid(PID_FILE);
    printf("Daemon stopped\n");
    return 0;
}

static int restart_daemon(void) {
    stop_daemon();
    return start_daemon();
}

// ---- CLI parser (original -- options) ----
typedef enum {
    CMD_NONE = 0,
    CMD_RUN,       // --run
    CMD_START,     // --start
    CMD_STOP,      // --stop
    CMD_RESTART,   // --restart
    CMD_HELP       // --help
} command_t;

static command_t parse_args(int argc, char *argv[]) {
    static struct option long_options[] = {
        {"run",     no_argument, 0, 'r'},
        {"start",   no_argument, 0, 's'},
        {"stop",    no_argument, 0, 't'},
        {"restart", no_argument, 0, 'e'},
        {"help",    no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    command_t cmd = CMD_NONE;
    int c;
    while ((c = getopt_long(argc, argv, "", long_options, NULL)) != -1) {
        switch (c) {
            case 'r': cmd = CMD_RUN;    break;
            case 's': cmd = CMD_START;  break;
            case 't': cmd = CMD_STOP;   break;
            case 'e': cmd = CMD_RESTART; break;
            case 'h': cmd = CMD_HELP;   break;
            default:
                fprintf(stderr, "Unknown option\n");
                exit(EXIT_FAILURE);
        }
    }

    // refuse extra positional arguments
    if (optind < argc) {
        fprintf(stderr, "Unexpected arguments: ");
        while (optind < argc) fprintf(stderr, "%s ", argv[optind++]);
        fprintf(stderr, "\n");
        exit(EXIT_FAILURE);
    }

    return cmd;
}

static void print_help(const char *prog) {
    printf("Usage: %s OPTION\n", prog);
    printf("  --run         Run in foreground (no daemon)\n");
    printf("  --start       Start as daemon\n");
    printf("  --stop        Stop running daemon\n");
    printf("  --restart     Restart daemon\n");
    printf("  --help        This help\n");
    printf("Version: %s\n", APP_VERSION);
}

// ---- main ----
int main(int argc, char *argv[]) {
    command_t cmd = parse_args(argc, argv);

    if (cmd == CMD_NONE || cmd == CMD_HELP) {
        print_help(argv[0]);
        return cmd == CMD_HELP ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    switch (cmd) {
        case CMD_RUN:     return run_foreground();
        case CMD_START:   return start_daemon();
        case CMD_STOP:    return stop_daemon();
        case CMD_RESTART: return restart_daemon();
        default:          return EXIT_FAILURE;
    }
}