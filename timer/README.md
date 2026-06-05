# Generic POSIX Timer App for Linux Applications

A lightweight, thread-safe, and highly efficient POSIX-timer-based wrapper for Linux applications. This library abstracts the complexities of POSIX timers, provides an asynchronous-signal-safe implementation, and exposes a clean, user-friendly API with millisecond precision and custom context injection (`user_data`).

---

## Features

- **High Precision:** Built on Linux POSIX timers (`timer_create`, `timer_settime`) targeting `CLOCK_REALTIME`.
- **$O(1)$ Performance Dispatcher:** Directly executes user callbacks using pointer context injection instead of traversing a linked list inside the signal handler.
- **Thread-Safe:** Managed internally via `pthread` mutexes to protect modifications to active timers safely.
- **Context Injection:** Allows passing arbitrary custom object pointers (`void *user_data`) directly into your callbacks.
- **Prevented Common Edge Cases:** Explicitly resolves standard POSIX implementation pitfalls such as head-deletion memory leaks, open-mutex deadlocks on allocation errors, and time-unit conversion bugs.

---

## Integration Guide

### 1. File Structure
Add `timer.h` and `timer.c` to your project repository:

### 2. Compilation

Ensure your build system is linked with both the **Realtime Extensions library (`-lrt`)** and the **POSIX Threads library (`-lpthread`)**.

#### Example gcc Command:

```bash
gcc -Iinclude src/timer.c src/main.c -o my_app -lrt -lpthread

```

#### Example Makefile Entry:

```makefile
CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -O2
LIBS = -lrt -lpthread

TARGET = my_app
SRCS = src/timer.c src/main.c
OBJS = $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

```

---

## API Reference

### `int timer_queue_init(void);`

Initializes the global timer module and configures the system signal actions (`sigaction`) for the designated realtime signal routing.

* **Returns:** `0` on success, `-1` on failure.

### `int timer_start(timer_t *timerid, long long delay_ms, long long interval_ms, timer_cb_t callback, void *user_data);`

Creates, tracks, and fires an active timer instance.

* `timerid`: Pointer to an uninitialized `timer_t` handle. This will be populated on success.
* `delay_ms`: Initial expiration time in milliseconds.
* `interval_ms`: Cyclic period execution rate in milliseconds (Pass `0` for a single-shot/one-off timer).
* `callback`: Function pointer matching `void (*timer_cb_t)(void *user_data)`.
* `user_data`: Pass-through context pointer accessible inside the executing callback.
* **Returns:** `0` on success, `-1` on failure.

### `int timer_reload(timer_t *timerid, long long delay_ms, long long interval_ms);`

Dynamically rearms or adjustments an already active timer instance without breaking its list reference or changing its tracking lifecycle.

* **Returns:** `0` on success, `-1` on failure.

### `int timer_stop(timer_t *timerid);`

Disarms the timer execution structure, deletes it from OS tracking resources, removes it from the safe list chain, clears memory allocations, and zeroes out the user's `timerid`.

* **Returns:** `0` on success, `-1` on failure.

---

## Usage Example

Below is a complete implementation demonstrating single-shot executions, continuous intervals, and state manipulation using `user_data`.

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "timer.h"

// Define an application state to inject into the timer context
typedef struct {
    int counter;
    const char *timer_name;
} app_context_t;

// Standard user-friendly callback matching signature timer_cb_t
void my_timer_callback(void *user_data) {
    app_context_t *context = (app_context_t *)user_data;
    if (!context) return;

    context->counter++;
    printf("[%s Callback] Fired! Total count: %d\n", 
           context->timer_name, context->counter);
}

int main(void) {
    printf("Initializing Timer System...\n");
    if (timer_queue_init() < 0) {
        fprintf(stderr, "Failed to initialize timer framework.\n");
        return EXIT_FAILURE;
    }

    // Prepare context structures
    app_context_t periodic_ctx = { .counter = 0, .timer_name = "PeriodicTimer" };
    app_context_t singleshot_ctx = { .counter = 0, .timer_name = "SingleShotTimer" };

    timer_t periodic_id;
    timer_t singleshot_id;

    // 1. Start a Periodic Timer: Fires after 1 second, repeats every 500ms
    printf("Starting Periodic Timer...\n");
    if (timer_start(&periodic_id, 1000, 500, my_timer_callback, &periodic_ctx) < 0) {
        fprintf(stderr, "Failed to start periodic timer.\n");
        return EXIT_FAILURE;
    }

    // 2. Start a Single-Shot Timer: Fires once after 2.5 seconds (0ms interval)
    printf("Starting Single-Shot Timer...\n");
    if (timer_start(&singleshot_id, 2500, 0, my_timer_callback, &singleshot_ctx) < 0) {
        fprintf(stderr, "Failed to start single-shot timer.\n");
        return EXIT_FAILURE;
    }

    // Let the main thread sleep to observe execution output
    printf("Main thread sleeping for 4 seconds while timers fire...\n\n");
    sleep(4);

    // 3. Clean up and halt timers
    printf("\nStopping all timers explicitly...\n");
    timer_stop(&periodic_id);
    timer_stop(&singleshot_id);

    printf("Application finished execution cleanly.\n");
    return EXIT_SUCCESS;
}

```

---

## Best Practices and Caveats

1. **Keep Callbacks Lean:** Your callbacks execute directly in an asynchronous signal handler context. Avoid heavy system processing, complex memory modifications, or calls to non-async-signal-safe functions (like raw standard I/O blocks or unmanaged locking mechanisms) inside intensive industrial applications.
2. **Handle Initialization Gracefully:** Always call `timer_queue_init()` exactly once near your program startup before making any downstream calls to `timer_start()`.
3. **Double-Stop Mitigation:** Calling `timer_stop()` safely handles cleanup and resets the referenced `timer_t` pointer block to `0`. Subsequent invocations against that zeroed variable will yield a safe `-1` reject bounds exit code instead of an unexpected double-free segmentation error.

```