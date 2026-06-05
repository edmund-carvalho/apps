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


typedef struct {
    char name[20];
    char surname[20];
    float weight;
    float height;
    int age;
} person_t;

int parse_person(const char *line, void *element, const char *format) {
    person_t *p = (person_t *)element;
    int n = sscanf(line, format,
                   p->name, p->surname,
                   &p->weight, &p->height,
                   &p->age);
    /* Expect exactly 5 fields; return negative on error. */
    return (n == 5) ? 5 : -1;
}

int main(void)
{
    /* 1. Create the parser - the library allocates it */
    csv_parser_t *parser = csv_parser_create(
        "people.csv",
        "%19[^,],%19[^,],%f,%f,%d",
        parse_person,
        1024,   // max line size
        true    // has header
    );

    if (parser == NULL) {
        fprintf(stderr, "Failed to create parser\n");
        return 1;
    }

    person_t people[5];
    /* 2. Parse the file */
    int n = csv_parser_parse(parser, people, sizeof(person_t), 100);

    if (n >= 0) {
        printf("Parsed %d records\n", n);
        for (int i = 0; i < n; i++) {
            printf("[%d] name::%s_%s weight::%f height::%f age::%d\n", i, people[i].name, people[i].surname, people[i].weight, people[i].height, people[i].age);
        }
    } else {
        fprintf(stderr, "Parse error code: %d\n", n);
    }

    /* 3. Clean up */
    csv_parser_destroy(parser);
    return 0;
}