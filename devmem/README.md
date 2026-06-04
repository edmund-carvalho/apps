
---

# Devmem Access Utility

A lightweight, user-friendly C library designed to simplify hardware register access from user-space via `/dev/mem`. It provides both simple, one-shot functions and a high-performance, session-based API for handling continuous register operations across multiple applications.

## Features

* **64-bit Architecture Ready**: Supports physical addresses beyond the 4 GB boundary (`uint64_t`).
* **Zero Memory Leaks**: Safely manages `mmap` virtual space and file descriptors.
* **Dual-Mode API**:
* **Stateless API**: Quick, one-off reads/writes (best for isolated configurations).
* **Persistent API**: Single initialization mapping for an entire block of peripherals (drastically minimizes system call overhead in tight loops).



---

## Architecture Overview

Using the standard one-shot `devmem_read` inside a fast loop introduces immense overhead because the system must execute `open`, `mmap`, `munmap`, and `close` system calls every time.

The Persistent API maps the entire peripheral page block *once* during application initialization, allowing subsequent register access to occur via direct pointer manipulation in user space.

---

## Integration Guide

### 1. File Structure

Add the following files into your existing C project source tree:

* `devmem.h` — External interface and data structures.
* `devmem.c` — Implementation logic.

### 2. Header Inclusion

Include the utility in your source files:

```c
#include "devmem.h"

```

---

## Usage Examples

### Option A: Persistent Block Mapping (Recommended for High Performance)

Use this approach if your application needs to read/write multiple registers inside a specific peripheral engine or engine block repeatedly.

```c
#include <stdio.h>
#include "devmem.h"

#define PERIPHERAL_BASE_ADDR  0x40001000 // Example physical base address
#define REG_STATUS_OFFSET     0x00       // Offset for Status Register
#define REG_CONTROL_OFFSET    0x04       // Offset for Control Register

int main(void) {
    devmem_handle_t h_periph;
    uint32_t reg_val = 0;

    // 1. Initialize the mapping region (Map a 4KB block)
    if (devmem_init(&h_periph, PERIPHERAL_BASE_ADDR, 4096) != 0) {
        fprintf(stderr, "Failed to initialize hardware mapping.\n");
        return -1;
    }

    // 2. Read from Status Register
    if (devmem_reg_read(&h_periph, REG_STATUS_OFFSET, &reg_val) == 0) {
        printf("Current Status: 0x%08X\n", reg_val);
    }

    // 3. Write to Control Register
    uint32_t enable_cmd = 0x00000001;
    devmem_reg_write(&h_periph, REG_CONTROL_OFFSET, enable_cmd);

    // 4. Clean up before application exit
    devmem_close(&h_periph);
    return 0;
}

```

### Option B: Stateless One-Shot Operations

Use this approach for simple, non-time-critical tasks, such as checking a single hardware strap configuration during bootup.

```c
#include <stdio.h>
#include "devmem.h"

#define TARGET_REG_ADDR  0x40001004

int main(void) {
    uint32_t data = 0;

    // Read a single address
    if (devmem_read(TARGET_REG_ADDR, &data) == 0) {
        printf("Register 0x%lx value: 0x%08X\n", TARGET_REG_ADDR, data);
    }

    // Write a single address
    uint32_t new_value = 0xDEADBEEF;
    if (devmem_write(TARGET_REG_ADDR, new_value) == 0) {
        printf("Successfully wrote 0x%08X to hardware.\n", new_value);
    }

    return 0;
}

```

---

## Compilation & Linking

When compiling your application, ensure `devmem.c` is included in your compilation unit or build script.

**Using GCC Directly:**

```bash
gcc -O2 main.c devmem.c -o my_app

```

---

## ⚠️ Requirements & Troubleshooting

### 1. Root Permissions

Accessing `/dev/mem` requires administrative privileges. You must execute your binary with `sudo` or configure specialized permissions:

```bash
sudo ./my_app

```

### 2. Kernel Restriction (`CONFIG_STRICT_DEVMEM`)

Many modern Linux distributions (especially standard x86 and modern desktop Ubuntu releases) enforce a security option called `CONFIG_STRICT_DEVMEM` within the kernel. If enabled, the kernel restricts `/dev/mem` maps strictly to memory ranges not claimed by system drivers.

* **Symptom:** `mmap failed: Operation not permitted` or a Segmentation Fault on access.
* **Fix:** You must ensure your target physical range isn't explicitly claimed by a kernel driver, disable `CONFIG_STRICT_DEVMEM` in your custom kernel build config, or switch to a dedicated UIO (Userspace I/O) driver.