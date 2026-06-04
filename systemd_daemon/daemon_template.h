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
#ifndef DAEMON_TEMPLATE_H
#define DAEMON_TEMPLATE_H

#include <signal.h>

#define APP_NAME    "myapp"
#define APP_VERSION "1.0.0"

// PID file path (used only in daemon mode)
#define PID_FILE    "/var/run/" APP_NAME ".pid"

// Set to 1 if you have libsystemd and want Type=notify
#define USE_SYSTEMD_NOTIFY 0

// ---------- You implement these two ----------
int  app_main(void *ctx);          // Called once after init
void app_shutdown(void *ctx);      // Called on graceful exit

// Application context (customise this struct)
typedef struct {
    int dummy;                     // replace with your data
} app_context_t;

// Global exit flag – set by signal handler
extern volatile sig_atomic_t g_running;

#endif