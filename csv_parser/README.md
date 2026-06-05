# csv_parser - Generic CSV / Text File Parser for C

A lightweight, single‑header + single‑source CSV/text parser library for C.
It allows you to parse structured text files using a **configurable format
string** and a user‑provided callback, without tying you to a fixed record
layout.

## Features

- **Variable format string** - the same parser works with any column layout.
- **Callback‑based** - you define how a line is converted into a struct.
- **Safe** - detects truncated lines, invalid input, and callback errors.
- **Pre‑allocated destination** - you supply the array; no hidden `malloc`.
- **Optional header skip** - ignore the first line (e.g., column names).
- **Portable C99** - only depends on standard libraries.

## Integration

1. Copy `csv_parser.h` and `csv_parser.c` into your project.
2. Include the header in your source files:
   ```c
   #include "csv_parser.h"
   ```
3. Compile `csv_parser.c` together with the rest of your code, e.g.:
   ```bash
   gcc -std=c99 -Wall your_app.c csv_parser.c -o your_app
   ```

## Usage Example

### Data file (`people.csv`)

```text
name,surname,weight,height,age
Eddy,Murphy,77.5,179.99,33
Bill,Burr,77.7,179.88,34
```

### Define your record structure

```c
typedef struct {
    char name[20];
    char surname[20];
    float weight;
    float height;
    int age;
} person_t;
```

### Write the line‑parsing callback

```c
int parse_person(const char *line, void *element, const char *format) {
    person_t *p = (person_t *)element;
    int n = sscanf(line, format,
                   p->name, p->surname,
                   &p->weight, &p->height,
                   &p->age);
    /* Expect exactly 5 fields; return negative on error. */
    return (n == 5) ? 5 : -1;
}
```

### Parse the file

```c
int main(void) {
    csv_parser_t *parser = csv_parser_create(
        "people.csv",
        "%19[^,],%19[^,],%f,%f,%d",  /* format string */
        parse_person,
        1024,   /* max line size */
        true    /* has header */
    );

    if (parser == NULL) {
        fprintf(stderr, "Failed to create parser\n");
        return 1;
    }

    person_t people[100];
    int n = csv_parser_parse(parser, people, sizeof(person_t), 100);

    if (n >= 0) {
        printf("Parsed %d records\n", n);
        for (int i = 0; i < n; i++) {
            printf("[%d] %s %s: %.1f kg, %.1f cm, %d years\n",
                   i, people[i].name, people[i].surname,
                   people[i].weight, people[i].height,
                   people[i].age);
        }
    } else {
        fprintf(stderr, "Parsing failed with error code %d\n", n);
    }

    csv_parser_destroy(parser);
    return 0;
}
```

### Format string hints

- Use `%[^,]` to read a string until a comma (e.g., `%19[^,]` limits to 19 chars).
- Separate fields with a literal comma `,`.
- `%f` and `%d` automatically skip whitespace.

## API Reference

### Types

- **`csv_parser_t`** - Fully opaque parser handle. The internal structure is hidden; obtain one via `csv_parser_create()` and release it with `csv_parser_destroy()`.

- **`csv_parser_line_cb`** -  
  ```c
  int callback(const char *line, void *element, const char *format);
  ```
  Must return the number of parsed fields (like `sscanf`) on success, or a
  negative value on error.

### Functions

- **`csv_parser_create()`** - Allocate and configure a new parser; returns a handle pointer, or `NULL` on failure.
- **`csv_parser_parse()`** - Run the parser. Returns the number of records stored, or a negative error code.
- **`csv_parser_destroy()`** - Free all resources. The handle must not be used after this call.

### Error codes

| Return | Meaning                                    |
|--------|--------------------------------------------|
| ≥ 0    | Number of records successfully parsed      |
| -1     | Cannot open the file                       |
| -2     | Parser handle or array is NULL             |
| -3     | Line too long for the buffer               |
| < -3   | Callback returned that negative value      |

### Error Handling in the Callback

Always verify that `sscanf` returned the expected number of fields.
If your format expects 5 items, check `n == 5` and return `5` on success,
or a negative value on failure. The parser will stop immediately and
propagate that negative code.

```c
if (n == 5) return 5;
else return -1;  /* or a more specific code, e.g. -(5-n) */
```

## Notes

- The library is not thread‑safe.
- The `filename` and `format` strings are not copied - they must remain valid for the lifetime of the parser object.
- The destination array must be pre‑allocated; the parser never allocates memory for output records.