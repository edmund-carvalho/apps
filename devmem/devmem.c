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
#include "devmem.h"
#include "logger.h"

/* =========================================================================
   Persistent Session-Based API Implementation (Highly Recommended for Apps)
   ========================================================================= */

int32_t devmem_init(devmem_handle_t *handle, uint64_t phys_addr, size_t size)
{
    if (!handle || size == 0) return -EINVAL;

    long page_size = getpagesize();
    // Align physical address to page boundary
    uint64_t page_phys = phys_addr & ~(page_size - 1);
    size_t offset_in_page = phys_addr & (page_size - 1);
    
    // Total size to map must include the page offset
    handle->mapped_size = size + offset_in_page;
    handle->base_phys_addr = phys_addr;

    handle->fd = open("/dev/mem", (O_RDWR | O_SYNC));
    if (handle->fd < 0) {
        APP_PERROR("Failed to open /dev/mem");
        return -1;
    }

    handle->map_base = mmap(NULL,
                            handle->mapped_size,
                            (PROT_READ | PROT_WRITE),
                            MAP_SHARED,
                            handle->fd,
                            (off_t)page_phys);

    if (handle->map_base == MAP_FAILED) {
        APP_PERROR("mmap session initialization failed");
        close(handle->fd);
        handle->fd = -1;
        return -1;
    }

    return 0;
}

int32_t devmem_reg_read(devmem_handle_t *handle, uint32_t offset, uint32_t *data)
{
    if (!handle || !handle->map_base || !data) return -EINVAL;

    long page_size = getpagesize();
    size_t offset_in_page = handle->base_phys_addr & (page_size - 1);
    
    // Target address inside mapped region
    volatile uint32_t *virt_addr = (volatile uint32_t *)((char *)handle->map_base + offset_in_page + offset);
    *data = *virt_addr;
    return 0;
}

int32_t devmem_reg_write(devmem_handle_t *handle, uint32_t offset, uint32_t writeval)
{
    if (!handle || !handle->map_base) return -EINVAL;

    long page_size = getpagesize();
    size_t offset_in_page = handle->base_phys_addr & (page_size - 1);
    
    volatile uint32_t *virt_addr = (volatile uint32_t *)((char *)handle->map_base + offset_in_page + offset);
    *virt_addr = writeval;
    return 0;
}

void devmem_close(devmem_handle_t *handle)
{
    if (!handle) return;

    if (handle->map_base && handle->map_base != MAP_FAILED) {
        munmap(handle->map_base, handle->mapped_size);
        handle->map_base = NULL;
    }
    if (handle->fd >= 0) {
        close(handle->fd);
        handle->fd = -1;
    }
}


/* =========================================================================
   Stateless One-Shot API Implementation (Fixed leaks and extended to 64-bit)
   ========================================================================= */

int32_t devmem_write(uint64_t target, uint32_t writeval)
{
    void *map_base, *virt_addr;
    long page_size;
    size_t mapped_size, offset_in_page;
    int fd;
    const unsigned width = 32;

    fd = open("/dev/mem", (O_RDWR | O_SYNC));
    if (fd < 0) {
        APP_PERROR("Failed to open /dev/mem");
        return -1;
    }

    page_size = getpagesize();
    mapped_size = page_size;
    offset_in_page = (size_t)(target & (page_size - 1));

    if (offset_in_page + (width / 8) > (size_t)page_size) {
        mapped_size *= 2;
    }

    map_base = mmap(NULL,
                    mapped_size,
                    (PROT_READ | PROT_WRITE),
                    MAP_SHARED,
                    fd,
                    (off_t)(target & ~(page_size - 1)));

    if (map_base == MAP_FAILED) {
        APP_PERROR("mmap failed");
        close(fd);
        return -1;
    }

    virt_addr = (char *)map_base + offset_in_page;
    *(volatile uint32_t *)virt_addr = writeval;

    // Clean up properly to prevent leaks
    munmap(map_base, mapped_size);
    close(fd);

    return 0;
}

int32_t devmem_read(uint64_t target, uint32_t *data)
{
    void *map_base, *virt_addr;
    long page_size;
    size_t mapped_size, offset_in_page;
    int fd;
    const unsigned width = 32;

    if (!data) return -EINVAL;

    fd = open("/dev/mem", (O_RDWR | O_SYNC));
    if (fd < 0) {
        APP_PERROR("Failed to open /dev/mem");
        return -1;
    }

    page_size = getpagesize();
    mapped_size = page_size;
    offset_in_page = (size_t)(target & (page_size - 1));

    if (offset_in_page + (width / 8) > (size_t)page_size) {
        mapped_size *= 2;
    }

    map_base = mmap(NULL,
                    mapped_size,
                    (PROT_READ | PROT_WRITE),
                    MAP_SHARED,
                    fd,
                    (off_t)(target & ~(page_size - 1)));

    if (map_base == MAP_FAILED) {
        APP_PERROR("mmap failed");
        close(fd);
        return -1;
    }

    virt_addr = (char *)map_base + offset_in_page;
    *data = *(volatile uint32_t *)virt_addr;

    // Clean up properly to prevent leaks
    munmap(map_base, mapped_size);
    close(fd);

    return 0;
}