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

/**
 * @file file_ops.c
 * @brief Simple, safe helpers for reading/writing single‑value files.
 *
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>   /* access() */


/* -------------------------------------------------------------------------
 *  Internal helper – forward declared so the public functions can use it.
 * ------------------------------------------------------------------------- */
FILE *file_fopen(const char *fname, const char *perms);

/* -------------------------------------------------------------------------
 *  Public API
 * ------------------------------------------------------------------------- */

/**
 * @brief Read a single integer from a file.
 *
 * The file must contain **only** one integer (leading / trailing white‑space
 * is allowed, but any extra characters after the integer will cause an error).
 *
 * @param[in]  file_path Absolute path of the file.
 * @param[out] var       Pointer to the integer that will hold the read value.
 *                       Memory is **not** allocated by this function.
 *
 * @return 0 on success, < 0 on failure:
 *         -1 : bad pointer,
 *         -2 : cannot open file,
 *         -3 : no integer found or extra data present.
 */
int file_read_int(const char *file_path, int *var)
{
    if (var == NULL) {
        fprintf(stderr, "ERROR: NULL pointer passed to file_read_int\n");
        return -1;
    }

    FILE *fp = file_fopen(file_path, "r");
    if (fp == NULL) {
        perror(file_path);
        return -2;
    }

    /* Read the integer and immediately check for trailing garbage. */
    if (fscanf(fp, "%d", var) != 1) {
        fprintf(stderr, "ERROR: could not read integer from %s\n", file_path);
        fclose(fp);
        return -3;
    }

    /* Make sure there is nothing else in the file (except white‑space). */
    int c;
    while ((c = fgetc(fp)) != EOF) {
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            fprintf(stderr,
                    "ERROR: unexpected characters after integer in %s\n",
                    file_path);
            fclose(fp);
            return -3;
        }
    }

    fclose(fp);
    return 0;
}

/**
 * @brief Write a single integer to a file.
 *
 * The file is created if it does not exist, and truncated if it does.
 *
 * @param[in] file_path Absolute path of the file.
 * @param[in] var       Integer to write.
 *
 * @return 0 on success, -1 on failure.
 */
int file_write_int(const char *file_path, int var)
{
    FILE *fp = file_fopen(file_path, "w");
    if (fp == NULL) {
        perror(file_path);
        return -1;
    }

    if (fprintf(fp, "%d", var) < 0) {
        perror("fprintf failed");
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

/**
 * @brief Read a single float from a file.
 *
 * The file must contain **only** one floating‑point value.  Trailing
 * garbage (other than white‑space) will be treated as an error.
 *
 * @param[in]  file_path Absolute path of the file.
 * @param[out] var       Pointer to the float that will receive the value.
 *                       Memory is **not** allocated here.
 *
 * @return 0 on success, < 0 on failure:
 *         -1 : bad pointer,
 *         -2 : cannot open file,
 *         -3 : no valid float found or extra data present.
 */
int file_read_float(const char *file_path, float *var)
{
    if (var == NULL) {
        fprintf(stderr, "ERROR: NULL pointer passed to file_read_float\n");
        return -1;
    }

    FILE *fp = file_fopen(file_path, "r");
    if (fp == NULL) {
        perror(file_path);
        return -2;
    }

    if (fscanf(fp, "%f", var) != 1) {
        fprintf(stderr, "ERROR: could not read float from %s\n", file_path);
        fclose(fp);
        return -3;
    }

    /* Reject any non‑white‑space trailing characters. */
    int c;
    while ((c = fgetc(fp)) != EOF) {
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            fprintf(stderr,
                    "ERROR: unexpected characters after float in %s\n",
                    file_path);
            fclose(fp);
            return -3;
        }
    }

    fclose(fp);
    return 0;
}

/**
 * @brief Write a single float to a file.
 *
 * The value is printed using the `%f` format specifier (six decimal places).
 * The file is created if it does not exist, and truncated if it does.
 *
 * @param[in] file_path Absolute path of the file.
 * @param[in] var       Float value to write.
 *
 * @return 0 on success, -1 on failure.
 */
int file_write_float(const char *file_path, float var)
{
    FILE *fp = file_fopen(file_path, "w");
    if (fp == NULL) {
        perror(file_path);
        return -1;
    }

    if (fprintf(fp, "%f", var) < 0) {
        perror("fprintf failed");
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

/**
 * @brief Check if a C‑string ends with a given suffix.
 *
 * @param[in] str    The string to examine (must be null‑terminated).
 * @param[in] suffix The suffix to look for (must be null‑terminated).
 *
 * @return `true` if @p str ends with @p suffix, `false` otherwise.
 */
bool file_str_endswith(const char *str, const char *suffix)
{
    size_t len_str = strlen(str);
    size_t len_suf = strlen(suffix);

    if (len_str < len_suf)
        return false;

    return (memcmp(str + len_str - len_suf, suffix, len_suf) == 0);
}

/**
 * @brief Check whether a regular file (or any node) exists.
 *
 * @param[in] fname Absolute or relative path to check.
 *
 * @return `true` if the path exists, `false` otherwise.
 */
bool file_exists(const char *fname)
{
    return (access(fname, F_OK) == 0);
}

/* -------------------------------------------------------------------------
 *  Internal helper – a thin wrapper around fopen() that logs failures.
 * ------------------------------------------------------------------------- */

/**
 * @brief Open a file with custom error messages.
 *
 * If the mode starts with `'r'` (i.e. the file is being opened for reading),
 * this function first checks that the file exists.  If it does not, a message
 * is printed and `NULL` is returned **without** calling `fopen()`.
 *
 * In all other cases (`"w"`, `"a"`, …) the file is opened/created normally.
 * On failure a message is printed and `NULL` is returned.
 *
 * @param[in] fname File path (absolute or relative).
 * @param[in] perms Mode string exactly as accepted by `fopen()` (e.g. `"r"`, `"w"`, `"rb"`).
 *
 * @return File pointer on success, `NULL` on failure.
 */
FILE *file_fopen(const char *fname, const char *perms)
{
    /* Modes that require the file to exist */
    if (perms[0] == 'r') {
        if (access(fname, F_OK) != 0) {
            fprintf(stderr, "File does not exist: %s\n", fname);
            return NULL;
        }
    }

    FILE *fp = fopen(fname, perms);
    if (fp == NULL) {
        fprintf(stderr, "Failed to open %s with mode \"%s\": ",
                fname, perms);
        perror("");   /* perror appends the system error message */
    }
    return fp;
}


/**
 * @brief  Count the number of lines in an already opened text file.
 *
 * The file position is **reset to the beginning** before counting starts,
 * and **left at the beginning** when the function returns.
 *
 * The algorithm counts every newline (`\n`) as one line.  If the file
 * is not empty and the last character is **not** a newline, the final
 * line is also counted (so a file containing `"hello"` has 1 line, and
 * `"hello\nworld\n"` has 2 lines).  An empty file returns 0.
 *
 * @param[in] fp  Pointer to a file opened in text mode (e.g., `"r"`).
 *
 * @return  Line count (≥ 0) on success; **-1** if the file pointer is
 *          `NULL` or a read error occurs.
 */
int file_get_line_count(FILE *fp)
{
    if (fp == NULL) {
        fprintf(stderr, "ERROR: NULL file pointer in file_get_line_count\n");
        return -1;
    }

    rewind(fp);   /* Reset to start; also clears EOF indicator */

    int ch;
    unsigned int lines = 0;
    int last_char = EOF;   /* Remember previous character to handle empty files */

    while ((ch = fgetc(fp)) != EOF) {
        if (ch == '\n') {
            lines++;
        }
        last_char = ch;
    }

    /* If the file is non‑empty and does not end with a newline,
     * the last line hasn't been counted yet. */
    if (last_char != EOF && last_char != '\n') {
        lines++;
    }

    /* Restore the file position to the beginning */
    rewind(fp);

    return (int)lines;   /* unsigned → signed, safe because line count fits */
}