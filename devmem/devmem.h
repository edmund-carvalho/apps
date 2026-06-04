#ifndef DEVMEM_H
#define DEVMEM_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>

// Handle structure to maintain persistent mappings for performance
typedef struct {
    int fd;
    void *map_base;
    size_t mapped_size;
    uint64_t base_phys_addr;
} devmem_handle_t;

/* --- High-Performance Persistent API --- */

// Initialize a persistent mapping for a specific region (e.g., a peripheral block)
int32_t devmem_init(devmem_handle_t *handle, uint64_t phys_addr, size_t size);

// Read/Write relative to the initialized base address
int32_t devmem_reg_read(devmem_handle_t *handle, uint32_t offset, uint32_t *data);
int32_t devmem_reg_write(devmem_handle_t *handle, uint32_t offset, uint32_t writeval);

// Clean up and release the persistent mapping
void devmem_close(devmem_handle_t *handle);


/* --- Classic Stateless API (Fixed & Updated) --- */
int32_t devmem_write(uint64_t target, uint32_t writeval);
int32_t devmem_read(uint64_t target, uint32_t *data);

#endif // DEVMEM_H