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
#include <time.h>
#include <string.h>
#include <unistd.h>   // for getpid if needed

#define BYTES_PER_LINE  32

/* Print a human-readable timestamp (YYYY-MM-DD HH:MM:SS.nnnnnnnnn) */
static void print_timestamp(FILE *fp)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    struct tm tm_info;
    localtime_r(&ts.tv_sec, &tm_info);   // thread-safe

    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &tm_info);
    fprintf(fp, "%s.%09ld", time_buf, ts.tv_nsec);
}

/* Core hexdump. Writes to 'fp'. Returns 0 on success, -1 on write error. */
int hexDump(FILE *fp, const void *src_ptr, int byte_count)
{
    if (!fp || !src_ptr || byte_count < 0)
        return -1;

    const unsigned char *ptr = (const unsigned char *)src_ptr;

    /* Optional header */
    fprintf(fp, "#######################################################\n");
    fprintf(fp, "# HEX_DUMP %d BYTES  TIMESTAMP: ", byte_count);
    print_timestamp(fp);
    fprintf(fp, " #\n");
    fprintf(fp, "#######################################################\n");

    if (byte_count == 0)
        return 0;

    int i = 0;
    while (i < byte_count) {
        /* Start of a new line: print offset */
        if ((i % BYTES_PER_LINE) == 0) {
            if (i != 0)          // end previous line (with ASCII)
                fprintf(fp, "\n");
            fprintf(fp, "%04x  ", i);  // 4-digit offset + 2 spaces
        }

        /* Print hex byte */
        fprintf(fp, "%02x ", ptr[i]);

        /* Print ASCII block at end of line */
        if ((i % BYTES_PER_LINE) == BYTES_PER_LINE - 1 || i == byte_count - 1) {
            /* Pad remaining hex space on the line */
            int bytes_left_on_line = BYTES_PER_LINE - (i % BYTES_PER_LINE) - 1;
            for (int j = 0; j < bytes_left_on_line; j++)
                fprintf(fp, "   ");   // 3 spaces per missing byte

            /* Separator */
            fprintf(fp, " |");

            /* Print ASCII for the bytes that were actually on this line */
            int line_start = i - (i % BYTES_PER_LINE);
            for (int j = line_start; j <= i; j++) {
                unsigned char c = ptr[j];
                fputc((c >= 32 && c <= 126) ? c : '.', fp);
            }

            /* Pad ASCII space if line is short */
            for (int j = 0; j < bytes_left_on_line; j++)
                fputc(' ', fp);

            fprintf(fp, "|");
        }
        i++;
    }
    fprintf(fp, "\n");
    return 0;
}

/* Convenience wrapper that opens the default file and calls hexDump */
void hexDump_to_default(const void *src_ptr, int byte_count)
{
    FILE *fp = fopen("hexDump.txt", "a");;
    if (!fp) {
        perror(" : Error opening hex dump log file");
        return;
    }
    hexDump(fp, src_ptr, byte_count);
    fclose(fp);
}