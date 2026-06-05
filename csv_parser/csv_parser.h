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
 * @file    csv_parser.h
 * @brief   Generic CSV / text file parser with configurable format string.
 *
 * The parser object is **opaque** - its implementation is hidden.  Use
 * csv_parser_create() to obtain a handle, then csv_parser_parse() to
 * process a file, and csv_parser_destroy() to free all resources.
 */

#ifndef CSV_PARSER_H_
#define CSV_PARSER_H_

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 *  Opaque parser handle
 * ------------------------------------------------------------------------- */

/** @brief Opaque parser object. */
typedef struct csv_parser csv_parser_t;

/* -------------------------------------------------------------------------
 *  Callback type
 * ------------------------------------------------------------------------- */

/**
 * @brief  Prototype of a line‑parsing callback.
 *
 * @param[in]  line      Null‑terminated text line (may include trailing newline).
 * @param[out] element   Pointer to the memory where the parsed data should be written.
 * @param[in]  format    The format string stored in the parser object.
 *
 * @return Number of successfully parsed fields (like `sscanf`), or a negative
 *         value to signal an error.  The parser stops immediately when a
 *         negative value is returned.
 */
typedef int (*csv_parser_line_cb)(const char *line,
                                  void       *element,
                                  const char *format);

/* -------------------------------------------------------------------------
 *  Public API
 * ------------------------------------------------------------------------- */

/**
 * @brief  Create a new parser object and allocate internal resources.
 *
 * @param[in] filename   Path to the input file (a copy is **not** made;
 *                       the caller must keep the string alive).
 * @param[in] format     Format string passed to the callback (same lifetime note).
 * @param[in] cb         Line parsing callback.
 * @param[in] max_line   Maximum line length to support (≥ 2).  The internal
 *                       buffer will have exactly this many bytes.
 * @param[in] has_header `true` to discard the first line as a header.
 *
 * @return Pointer to a new csv_parser_t, or `NULL` if memory allocation fails.
 */
csv_parser_t *csv_parser_create(const char        *filename,
                                const char        *format,
                                csv_parser_line_cb cb,
                                size_t             max_line,
                                bool               has_header);

/**
 * @brief  Free all resources associated with the parser.
 *
 * After calling this, the parser pointer must no longer be used.
 *
 * @param[in] parser  Parser handle returned by csv_parser_create().
 */
void csv_parser_destroy(csv_parser_t *parser);

/**
 * @brief  Parse the file, storing elements in the supplied array.
 *
 * The file is opened, optionally a header line is skipped, and then every
 * subsequent line is read, passed to the callback, and the result stored
 * sequentially in `array`.
 *
 * @param[in] parser    Parser handle returned by csv_parser_create().
 * @param[out] array    Pointer to the start of a pre‑allocated array.
 * @param[in] elem_size Size in bytes of one array element.
 * @param[in] max_elems Maximum number of elements that `array` can hold.
 *
 * @return Number of elements successfully parsed (≥ 0), or a negative value
 *         on error:
 *         - -1 : could not open file
 *         - -2 : parser handle is NULL or array is NULL
 *         - -3 : a line was too long (exceeded max_line_size)
 *         - other negative values are forwarded from the callback.
 */
int csv_parser_parse(csv_parser_t *parser,
                     void         *array,
                     size_t        elem_size,
                     int           max_elems);

#ifdef __cplusplus
}
#endif

#endif /* CSV_PARSER_H_ */