# Generic systemd Daemon Runner - README

A reusable C template for turning any application into a well-behaved daemon.  
Supports standalone (SysV‑style) operation as well as modern **systemd** integration with optional readiness notification.

## Features

- **Clean CLI** - `--run`, `--start`, `--stop`, `--restart`, `--help`  
- **Foreground mode** (`--run`) - perfect for `systemd`’s `Type=simple` or `Type=notify`  
- **Classic daemon mode** (`--start`) - double‑fork, PID file, works everywhere  
- **Graceful shutdown** - `SIGTERM` / `SIGINT` trigger your `app_shutdown()`  
- **Fatal signal trapping** - `SIGSEGV`, `SIGFPE`, etc. logged before exit  
- **Optional `sd_notify`** - tell systemd exactly when your service is ready  

## Requirements

- A C compiler (GCC or Clang)  
- `make` (optional - you can compile directly)  
- `libsystemd` (only if `USE_SYSTEMD_NOTIFY=1` and you want `Type=notify`)  

## Quick Start

1. **Clone / copy the files**  
   - `daemon_template.h` - macros and your application interface  
   - `daemon_template.c` - the reusable daemon engine  
   - `myapp.c` - your business logic (example provided)

2. **Write your application logic**  
   Implement the two required functions in your own `.c` file:

   ```c
   #include "daemon_template.h"

   int app_main(void *ctx) {
       // Initialise everything here (threads, network, hardware, …)
       // Return 0 on success, non-zero to abort.
       return 0;
   }

   void app_shutdown(void *ctx) {
       // Cleanup all resources - join threads, close files, …
   }
   ```

   You can customise the `app_context_t` struct in the header to hold your own data.

3. **Compile**

   **Without systemd notify:**
   ```bash
   gcc -O2 -Wall -o myapp daemon_template.c myapp.c -lpthread
   ```

   **With systemd notify** (requires `libsystemd`):
   ```bash
   gcc -O2 -Wall -DUSE_SYSTEMD_NOTIFY=1 -o myapp daemon_template.c myapp.c -lsystemd -lpthread
   ```

   Override the application name or PID file path at build time:
   ```bash
   gcc -O2 -Wall -DAPP_NAME='"mycoolapp"' -DPID_FILE='"/tmp/mycoolapp.pid"' -o mycoolapp daemon_template.c myapp.c -lpthread
   ```

4. **Run**

   ```bash
   # Test in foreground (stop with Ctrl+C)
   ./myapp --run

   # Classic daemon background
   sudo ./myapp --start
   ./myapp --stop
   ./myapp --restart

   # Show help
   ./myapp --help
   ```

## Integrating with systemd

The daemon works with both `Type=simple` (no notify) and `Type=notify` (with `sd_notify`).

### Option A: `Type=simple` (no `sd_notify`)

1. Compile **without** `USE_SYSTEMD_NOTIFY`.
2. Create a service unit file (`/etc/systemd/system/myapp.service`):

   ```ini
   [Unit]
   Description=My Application
   After=network.target

   [Service]
   Type=simple
   ExecStart=/usr/local/bin/myapp --run
   Restart=on-failure
   RestartSec=5
   User=nobody
   Group=nogroup

   [Install]
   WantedBy=multi-user.target
   ```

3. Enable and start:
   ```bash
   sudo systemctl daemon-reload
   sudo systemctl enable --now myapp
   ```

### Option B: `Type=notify` (with `sd_notify`)

This gives systemd exact startup/shutdown visibility and allows watchdog monitoring.

1. Compile **with** `USE_SYSTEMD_NOTIFY=1`.
2. Use the same unit file, but change the type and add `WatchdogSec` if desired:

   ```ini
   [Service]
   Type=notify
   ExecStart=/usr/local/bin/myapp --run
   WatchdogSec=30          # optional
   ```

3. (Optional) Enable the watchdog inside your `app_main()`:
   ```c
   // After initialisation, periodically call in a loop or thread:
   sd_notify(0, "WATCHDOG=1");
   ```

4. Install as before. Systemd will wait for `READY=1` before starting dependent units.

## How It Works

- **`--run`** - Calls `app_main()`, then enters a main loop waiting for `SIGTERM`/`SIGINT`.  
  After the loop, `app_shutdown()` runs. Perfect for systemd or debugging.

- **`--start`** - Forks a child, daemonises it, writes a PID file, and runs the same logic.

- **`--stop`** - Reads the PID file, sends `SIGTERM`, waits for the process to die, and removes the PID.

- **`--restart`** - Combines `--stop` + `--start`.

- **`--help`** - Prints usage.

### Signal Handling

- `SIGTERM` / `SIGINT` - sets a global flag `g_running = 0`, the main loop exits gracefully.
- `SIGFPE`, `SIGSEGV`, `SIGILL`, `SIGBUS` - logs a message and exits immediately (no core dump in production).
- `SIGHUP` - ignored (can be overridden for reload).

## Customising the Template

All configurable macros are in `daemon_template.h`:

| Macro | Default | Meaning |
|-------|---------|---------|
| `APP_NAME` | `"myapp"` | Name used in logs and PID path |
| `APP_VERSION` | `"1.0.0"` | Shown in help |
| `PID_FILE` | `"/var/run/APP_NAME.pid"` | Only used in daemon mode (`--start`) |
| `USE_SYSTEMD_NOTIFY` | `0` | Set to `1` to enable `sd_notify` |

You can change them directly in the header or via `-D` flags when compiling.

## Troubleshooting

- **“Daemon failed to start”** - Check permissions for the PID file path; run with `sudo` if needed.  
- **PID file left behind** - The daemon removes it on clean shutdown. If it crashes unexpectedly, `--stop` detects the stale PID and removes it.  
- **`sig_atomic_t` undefined** - Very old libc. Change the line `volatile sig_atomic_t g_running;` to `volatile int g_running;` in `daemon_template.c`.  
- **`sd_notify` not found** - Install `libsystemd-dev` (or `systemd-devel`) and link with `-lsystemd`.  

## Example Application

`myapp.c` contains a minimal skeleton. Replace it with your own logic - the daemon code is completely independent.

---