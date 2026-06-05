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
#include "csv_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief  The actual definition - hidden from users.
 */
struct csv_parser {
    const char *filename;
    const char *format;
    csv_parser_line_cb callback;
    size_t       max_line_size;
    bool         has_header;

    /* private state */
    char *line_buffer;
    FILE *fp;
    int   current_line;
};

/* ------------------------------------------------------------------------- */
csv_parser_t *csv_parser_create(const char        *filename,
                                const char        *format,
                                csv_parser_line_cb cb,
                                size_t             max_line,
                                bool               has_header)
{
    if (!filename || !format || !cb)
        return NULL;
    if (max_line < 2)
        max_line = 2;

    csv_parser_t *parser = (csv_parser_t *)malloc(sizeof(csv_parser_t));
    if (!parser) return NULL;

    parser->line_buffer = (char *)malloc(max_line);
    if (!parser->line_buffer) {
        free(parser);
        return NULL;
    }

    parser->filename      = filename;
    parser->format        = format;
    parser->callback      = cb;
    parser->max_line_size = max_line;
    parser->has_header    = has_header;
    parser->fp            = NULL;
    parser->current_line  = 0;

    return parser;
}

/* ------------------------------------------------------------------------- */
void csv_parser_destroy(csv_parser_t *parser)
{
    if (!parser) return;
    free(parser->line_buffer);
    parser->line_buffer = NULL;
    free(parser);
}

/* ------------------------------------------------------------------------- */
int csv_parser_parse(csv_parser_t *parser,
                     void         *array,
                     size_t        elem_size,
                     int           max_elems)
{
    if (!parser || !array) {
        fprintf(stderr, "csv_parser_parse: invalid argument (NULL pointer)\n");
        return -2;
    }

    parser->fp = fopen(parser->filename, "r");
    if (!parser->fp) {
        perror(parser->filename);
        return -1;
    }

    /* skip header if requested */
    if (parser->has_header) {
        int c;
        while ((c = fgetc(parser->fp)) != EOF && c != '\n')
            ;
    }

    int count = 0;
    parser->current_line = 0;

    while (count < max_elems) {
        if (fgets(parser->line_buffer, (int)parser->max_line_size, parser->fp) == NULL)
            break;

        parser->current_line++;

        size_t len = strlen(parser->line_buffer);
        if (len == parser->max_line_size - 1 && parser->line_buffer[len - 1] != '\n') {
            fprintf(stderr, "%s:%d: line too long (max %zu chars)\n",
                    parser->filename, parser->current_line, parser->max_line_size - 1);
            fclose(parser->fp);
            parser->fp = NULL;
            return -3;
        }

        void *dest = (unsigned char *)array + (size_t)count * elem_size;
        int ret = parser->callback(parser->line_buffer, dest, parser->format);
        if (ret < 0) {
            fprintf(stderr, "%s:%d: callback returned error %d\n",
                    parser->filename, parser->current_line, ret);
            fclose(parser->fp);
            parser->fp = NULL;
            return ret;
        }
        count++;
    }

    fclose(parser->fp);
    parser->fp = NULL;
    return count;
}