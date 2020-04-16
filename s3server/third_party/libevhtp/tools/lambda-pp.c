/*
* Copyright (C) 2014
*   Wolfgang Bumiller
*   Dale Weiler
*
* Permission is hereby granted, free of charge, to any person obtaining a copy of
* this software and associated documentation files (the "Software"), to deal in
* the Software without restriction, including without limitation the rights to
* use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
* of the Software, and to permit persons to whom the Software is furnished to do
* so, subject to the following conditions:
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
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>

#define isalpha(a) ((((unsigned)(a)|32)-'a') < 26)
#define isdigit(a) (((unsigned)(a)-'0') < 10)
#define isalnum(a) (isalpha(a) || isdigit(a))
#define isspace(a) (((a) >= '\t' && (a) <= '\r') || (a) == ' ')

static const char *DefaultKeyword = "lambda";

typedef struct {
    size_t begin;
    size_t length;
} lambda_range_t;

typedef struct {
    size_t pos;
    size_t line;
} lambda_position_t;

typedef struct {
    const char *file;
    char       *data;
    size_t      length;
    size_t      line;
    const char *keyword;
    size_t      keylength;
    bool        short_enabled;
} lambda_source_t;

typedef struct {
    size_t         start;
    lambda_range_t decl;
    lambda_range_t body;
    size_t         name_offset;
    size_t         decl_line;
    size_t         body_line;
    size_t         end_line;
    bool           is_short;
} lambda_t;

typedef struct {
    union {
        char              *chars;
        lambda_t          *funcs;
        lambda_position_t *positions;
    };
    size_t size;
    size_t elements;
    size_t length;
} lambda_vector_t;

typedef struct {
  lambda_vector_t lambdas;
  lambda_vector_t positions;
} parse_data_t;

typedef enum {
    PARSE_NORMAL, PARSE_TYPE, PARSE_LAMBDA, PARSE_LAMBDA_EXPRESSION
} parse_type_t;

static size_t parse(lambda_source_t *source, parse_data_t *data, size_t j, parse_type_t parsetype, size_t *nameofs);

/* Vector */
static inline bool lambda_vector_init(lambda_vector_t *vec, size_t size) {
    vec->length   = 32;
    vec->size     = size;
    vec->elements = 0;
    return (vec->chars = (char *)malloc(vec->length * vec->size));
}

static inline void lambda_vector_destroy(lambda_vector_t *vec) {
    free(vec->chars);
}

static inline bool lambda_vector_resize(lambda_vector_t *vec) {
    if (vec->elements != vec->length)
        return true;
    vec->length <<= 1;
    char *temp = realloc(vec->chars, vec->length * vec->size);
    if (!temp)
        return false;
    vec->chars = temp;
    return true;
}

static inline bool lambda_vector_push_char(lambda_vector_t *vec, char ch) {
    if (!lambda_vector_resize(vec))
        return false;
    vec->chars[vec->elements++] = ch;
    return true;
}

static inline bool lambda_vector_create_lambda(lambda_vector_t *vec, size_t *idx) {
    if (!lambda_vector_resize(vec))
        return false;
    *idx = vec->elements++;
    memset(&vec->funcs[*idx], 0, sizeof(lambda_t));
    return true;
}

static inline bool lambda_vector_push_position(lambda_vector_t *vec, size_t pos, size_t line) {
    if (!lambda_vector_resize(vec))
        return false;
    vec->positions[vec->elements].pos  = pos;
    vec->positions[vec->elements].line = line;
    vec->elements++;
    return true;
}

static inline void lambda_source_init(lambda_source_t *source) {
    memset(source, 0, sizeof(*source));
    source->keyword       = DefaultKeyword;
    source->short_enabled = true;
}

/* Source */
static void parse_error(lambda_source_t *source, const char *message, ...) {
    char buffer[2048];
    va_list va;
    va_start(va, message);
    vsnprintf(buffer, sizeof(buffer), message, va);
    va_end(va);
    fprintf(stderr, "%s:%zu error: %s\n", source->file, source->line, buffer);
    fflush(stderr);
}

static bool parse_open(lambda_source_t *source, FILE *handle) {
    if (!handle)
        return false;

    source->line = 1;

    if (fseek(handle, 0, SEEK_END) != -1) {
        source->length = ftell(handle);
        fseek(handle, 0, SEEK_SET);

        if (!(source->data = (char *)malloc(source->length)))
            goto parse_open_oom;
        if (fread(source->data, source->length, 1, handle) != 1)
            goto parse_open_failed;
    }
    else {
        static const size_t bs = 4096;
        source->length = 0;
        if (!(source->data = (char*)malloc(bs)))
            goto parse_open_oom;
        while (true) {
            size_t r = fread(source->data, 1, bs, handle);
            source->length += r;
            if (feof(handle))
                break;
            if (ferror(handle) && errno != EINTR)
                goto parse_open_failed;
            char *temp = (char*)realloc(source->data, source->length + bs);
            if (!temp)
                goto parse_open_oom;
            source->data = temp;
        }
    }

    fclose(handle);
    return true;

parse_open_oom:
    parse_error(source, "out of memory");
parse_open_failed:
    free(source->data);
    fclose(handle);
    return false;
}

static inline void parse_close(lambda_source_t *source) {
    free(source->data);
}

/* Parser */
static inline size_t parse_skip_string(lambda_source_t *source, size_t i, char check) {
    while (i != source->length) {
        if (source->data[i] == check)
            return i + 1;
        else if (source->data[i] == '\\')
            if (++i == source->length)
                break;
        ++i;
    }
    return i;
}

static inline size_t parse_skip_white(lambda_source_t *source, size_t i) {
    while (i != source->length && isspace(source->data[i])) {
        if (source->data[i] == '\n')
            source->line++;
        ++i;
    }
    return i;
}

static size_t parse_word(lambda_source_t *source, parse_data_t *data, size_t j, size_t i) {
    if (j != i) {
        if (strncmp(source->data + j, source->keyword, source->keylength) == 0)
            return parse(source, data, i, PARSE_LAMBDA, false);
    }
    if (source->data[i] == '\n')
        source->line++;
    else if (!strncmp(source->data + i, "//", 2)) {
        /* Single line comments */
        i = strchr(source->data + i, '\n') - source->data;
    } else if (!strncmp(source->data + i, "/*", 2)) {
        /* Multi line comments */
        i = strstr(source->data + i, "*/") - source->data;
    }
    return i;
}

#define ERROR ((size_t)-1)

static size_t parse(lambda_source_t *source, parse_data_t *data, size_t i, parse_type_t parsetype, size_t *nameofs) {
    lambda_vector_t parens;
    size_t          lambda = 0;
    bool            mark = (!parsetype && !nameofs);
    size_t          protopos = i;
    bool            protomove = true;
    bool            preprocessor = false;
    bool            expectbody = false;
    bool            movename = false;
    /* 'mark' actually means this is the outer most call and we should
     * remember where to put prototypes now!
     * when protomove is true we move the protopos along whitespace so that
     * the lambdas don't get stuck to the tail of hte previous functions.
     * Also we need to put lambdas after #include lines so if we encounter
     * a preprocessor directive we create another position marker starting
     * at the nest new line
     */

    lambda_vector_init(&parens, sizeof(char));


    if (parsetype == PARSE_LAMBDA) {
        if (!lambda_vector_create_lambda(&data->lambdas, &lambda))
            goto parse_oom;
        lambda_t *l = &data->lambdas.funcs[lambda];
        l->start = i - 6;
        i = parse_skip_white(source, i);
        l->decl.begin = i;
        l->decl_line = source->line;
        size_t ofs = 0;
        if ((i = parse(source, data, i, PARSE_TYPE, &ofs)) == ERROR)
            goto parse_error;
        l->name_offset = ofs - l->decl.begin;
        l->decl.length = i - l->decl.begin;
        l->body.begin = i;
        l->body_line  = source->line;
        i = parse_skip_white(source, i);
        if (source->short_enabled) {
            if (source->data[i] == '=' && source->data[i+1] == '>') {
                l->body.begin = i += 2;
                l->is_short = true;
                parsetype = PARSE_LAMBDA_EXPRESSION;
            }
        }
    }

    size_t j = i;
    while (i < source->length) {
        if (mark && !parens.elements) {
            if (protomove) {
                if (isspace(source->data[i])) {
                    if (source->data[i] == '\n')
                        source->line++;
                    protopos = j = ++i;
                    continue;
                }
                protomove = false;
                if (!lambda_vector_push_position(&data->positions, protopos, source->line))
                    goto parse_oom;
            }

            if (source->data[i] == ';') {
                if (!nameofs && (i = parse_word(source, data, j, i)) == ERROR)
                    goto parse_error;
                j = ++i;
                protomove = true;
                protopos  = i;
                continue;
            }

            if (source->data[i] == '#') {
                if (!nameofs && (i = parse_word(source, data, j, i)) == ERROR)
                    goto parse_error;
                j = ++i;
                protomove = false;
                protopos  = i;
                preprocessor = true;
                continue;
            }
            if (preprocessor && source->data[i] == '\n') {
                if (!nameofs && (i = parse_word(source, data, j, i)) == ERROR)
                    goto parse_error;
                j = ++i;
                protomove = true;
                protopos  = i;
                preprocessor = false;
                continue;
            }
        }

        if (movename) {
            if (source->data[i] != '*' && source->data[i] != '(' && !isspace(source->data[i]))
                movename = false;
            else if (source->data[i] != '(')
                *nameofs = i+1;
        }

        if (source->data[i] == '"') {
            if (!nameofs && (i = parse_word(source, data, j, i)) == ERROR)
                goto parse_error;
            j = i = parse_skip_string(source, i+1, source->data[i]);
        } else if (source->data[i] == '\'') {
            if (!nameofs && (i = parse_word(source, data, j, i)) == ERROR)
                goto parse_error;
            j = i = parse_skip_string(source, i+1, source->data[i]);
        } else if (strchr("([{", source->data[i])) {
            if (nameofs && !parens.elements) {
                if (expectbody && source->data[i] == '{') {
                    lambda_vector_destroy(&parens);
                    return i;
                }
                if (!expectbody && source->data[i] == '(') {
                    expectbody = true;
                    movename = true;
                    *nameofs = i;
                }
            }
            if (!nameofs && (i = parse_word(source, data, j, i)) == ERROR)
                goto parse_error;
            if (!lambda_vector_push_char(&parens, strchr("([{)]}", source->data[i])[3]))
                goto parse_oom;
            j = ++i;
        } else if (strchr(")]}", source->data[i])) {
            if (!parens.elements) {
                parse_error(source, "too many closing parenthesis");
                goto parse_error;
            }
            char back = parens.chars[parens.elements >= 1 ? parens.elements - 1 : 0];
            if (source->data[i] != back) {
                parse_error(source, "mismatching `%c' and `%c'", back, source->data[i]);
                goto parse_error;
            }
            if (parens.elements != 0)
                parens.elements--;
            if (source->data[i] == '}' && !parens.elements) {
                if (parsetype == PARSE_LAMBDA)
                    goto finish_lambda;
                else if (nameofs) {
                    if (!expectbody)
                        movename = true;
                }
            }
            bool domark = (mark && !parens.elements && source->data[i] == '}');
            if (!nameofs && (i = parse_word(source, data, j, i)) == ERROR)
                goto parse_error;
            j = ++i;
            if (domark) {
                protopos = i;
                protomove = true;
            }
        } else if (source->data[i] != '_' && !isalnum(source->data[i])) {
            if (!nameofs && (i = parse_word(source, data, j, i)) == ERROR)
                goto parse_error;
            if (!parens.elements) {
                if (parsetype == PARSE_LAMBDA_EXPRESSION && source->data[i] == ';')
                    goto finish_lambda;
                if (source->short_enabled) {
                    if (parsetype == PARSE_TYPE && expectbody && source->data[i] == '=' && source->data[i+1] == '>') {
                        lambda_vector_destroy(&parens);
                        return i;
                    }
                }
            }
            j = ++i;
        } else
            ++i;
    }

    lambda_vector_destroy(&parens);
    return i;

parse_oom:
    parse_error(source, "out of memory");
parse_error:
    lambda_vector_destroy(&parens);
    return ERROR;
finish_lambda:
    {
        lambda_t *l = &data->lambdas.funcs[lambda];
        l->body.length = i - l->body.begin;
        l->end_line = source->line;
        lambda_vector_destroy(&parens);
        return i;
    }
}

/* Generator */
static inline void generate_marker(FILE *out, const char *file, size_t line, bool newline) {
    fprintf(out, "%s#line %zu \"%s\"\n", newline ? "\n" : "", line, file);
}

static inline void generate_begin(FILE *out, lambda_source_t *source, lambda_vector_t *lambdas, size_t idx) {
    generate_marker(out, source->file, lambdas->funcs[idx].decl_line, true);
    fprintf(out, "static ");
    size_t ofs = lambdas->funcs[idx].name_offset;
    fwrite(source->data + lambdas->funcs[idx].decl.begin, ofs, 1, out);
    fprintf(out, " lambda_%zu", idx);
    fwrite(source->data + lambdas->funcs[idx].decl.begin+ofs, lambdas->funcs[idx].decl.length-ofs, 1, out);
}

static size_t next_prototype_position(parse_data_t *data, size_t lam, size_t proto) {
    if (lam == data->lambdas.elements)
        return data->positions.elements;
    for (; proto != data->positions.elements; ++proto) {
        if (data->positions.positions[proto].pos > data->lambdas.funcs[lam].start)
            return proto-1;
    }
    return data->positions.elements-1;
}

static void generate_code(FILE *out, lambda_source_t *source, size_t pos, size_t len, parse_data_t *data, size_t lam, bool source_only);
static void generate_functions(FILE *out, lambda_source_t *source, parse_data_t *data, size_t lam, size_t proto) {
    size_t end = (proto+1) == data->positions.elements ? (size_t)-1 : data->positions.positions[proto+1].pos;
    size_t first = lam;
    for (; lam != data->lambdas.elements; ++lam) {
        if (data->lambdas.funcs[lam].start > end)
            break;
    }
    while (lam-- != first) {
        lambda_t *lambda = &data->lambdas.funcs[lam];
        generate_begin(out, source, &data->lambdas, lam);
        if (lambda->is_short)
            fprintf(out, "{");
        generate_code(out, source, lambda->body.begin, lambda->body.length + 1, data, lam + 1, true);
        if (lambda->is_short)
            fprintf(out, "}");
    }
    fprintf(out, "\n");
}

/* when generating the actual code we also take prototype-positioning into account */
static void generate_code(FILE *out, lambda_source_t *source, size_t pos, size_t len, parse_data_t *data, size_t lam, bool source_only) {
    /* we know that positions always has at least 1 element, the 0, so the first search is there */
    size_t proto = source_only ? data->positions.elements : next_prototype_position(data, lam, 1);
    while (len) {
        if (proto != data->positions.elements) {
            lambda_position_t *lambdapos = &data->positions.positions[proto];
            size_t point = lambdapos->pos;
            if (pos <= point && pos+len >= point) {
                /* we insert prototypes here! */
                size_t length = point - pos;
                fwrite(source->data + pos, length, 1, out);
                generate_functions(out, source, data, lam, proto);
                generate_marker(out, source->file, lambdapos->line, true);
                len -= length;
                pos += length;
            }
        }

        if (lam == data->lambdas.elements || data->lambdas.funcs[lam].start > pos + len) {
            fwrite(source->data + pos, len, 1, out);
            return;
        }

        lambda_t *lambda = &data->lambdas.funcs[lam];
        size_t    length = lambda->body.begin + lambda->body.length + 1 - pos;

        fwrite(source->data + pos, lambda->start - pos, 1, out);
        fprintf(out, "(&lambda_%zu)", lam);

        len -= length;
        pos += length;

        for (++lam; lam != data->lambdas.elements && data->lambdas.funcs[lam].start < pos; ++lam)
            ;
        proto = next_prototype_position(data, lam, proto);
    }
}


static void generate(FILE *out, lambda_source_t *source) {
    parse_data_t data;
    lambda_vector_init(&data.lambdas,   sizeof(data.lambdas.funcs[0]));
    lambda_vector_init(&data.positions, sizeof(data.positions.positions[0]));
    if (parse(source, &data, 0, PARSE_NORMAL, false) == ERROR) {
        lambda_vector_destroy(&data.lambdas);
        lambda_vector_destroy(&data.positions);
        return;
    }

    generate_marker(out, source->file, 1, false);

    generate_code(out, source, 0, source->length, &data, 0, false);

    /* there are cases where we get no newline at the end of the file */
    fprintf(out, "\n");

    lambda_vector_destroy(&data.lambdas);
    lambda_vector_destroy(&data.positions);
}

static void usage(const char *prog, FILE *out) {
    fprintf(out, "usage: %s [options] [<file>]\n", prog);
    fprintf(out,
        "options:\n"
        "  -h, --help          print this help message\n"
        "  -V, --version       show the current program version\n"
        "  -k, --keyword=WORD  change the lambda keyword to WORD\n"
        "  -o, --output=FILE   write to FILE instead of stdout\n"
        "  -s                  enable shortened syntax (default)\n"
        "  -S                  disable shortened syntax\n");
}

static void version(FILE *out) {
    fprintf(out, "lambdapp 0.1\n");
}

/* returns false when the parameter doesn't match,
 * returns true and sets argarg when the parameter does match,
 * returns true and sets arg to -1 on error
 */
static bool isparam(int argc, char **argv, int *arg, char sh, const char *lng, char **argarg) {
    if (argv[*arg][0] != '-')
        return false;
    /* short version */
    if (argv[*arg][1] == sh) {
        if (argv[*arg][2]) {
            *argarg = argv[*arg]+2;
            return true;
        }
        ++*arg;
        if (*arg == argc) {
            fprintf(stderr, "%s: option -%c requires an argument\n", argv[0], sh);
            usage(argv[0], stderr);
            *arg = -1;
            return true;
        }
        *argarg = argv[*arg];
        return true;
    }
    /* long version */
    if (argv[*arg][1] != '-')
        return false;
    size_t len = strlen(lng);
    if (strncmp(argv[*arg]+2, lng, len))
        return false;
    if (argv[*arg][len+2] == '=') {
        *argarg = argv[*arg] + 3 + len;
        return true;
    }
    if (!argv[*arg][len+2]) {
        ++*arg;
        if (*arg == argc) {
            fprintf(stderr, "%s: option --%s requires an argument\n", argv[0], lng);
            usage(argv[0], stderr);
            *arg = -1;
            return true;
        }
        *argarg = argv[*arg];
        return true;
    }
    return false;
}

int main(int argc, char **argv) {
    lambda_source_t source;
    const char *file = NULL;
    const char *output = NULL;
    FILE       *outfile = stdout;

    lambda_source_init(&source);

    int i = 1;
    for (; i != argc; ++i) {
        char *argarg;

        if (!strcmp(argv[i], "--")) {
            ++i;
            break;
        }
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usage(argv[0], stdout);
            return 0;
        }
        if (!strcmp(argv[i], "-V") || !strcmp(argv[i], "--version")) {
            version(stdout);
            return 0;
        }
        if (!strcmp(argv[i], "-s")) {
          source.short_enabled = true;
          continue;
        }
        if (!strcmp(argv[i], "-S")) {
          source.short_enabled = false;
          continue;
        }
        if (isparam(argc, argv, &i, 'k', "keyword", &argarg)) {
            if (i < 0)
                return 1;
            source.keyword = argarg;
            continue;
        }
        if (isparam(argc, argv, &i, 'o', "output", &argarg)) {
            if (i < 0)
                return 1;
            output = argarg;
            continue;
        }
        if (argv[i][0] == '-') {
            fprintf(stderr, "%s: unrecognized option: %s\n", argv[0], argv[i]);
            usage(argv[0], stderr);
            return 1;
        }
        if (file) {
            fprintf(stderr, "%s: only 1 file allowed\n", argv[0]);
            usage(argv[0], stderr);
            return 1;
        }
        file = argv[i];
    }
    if (!file && i != argc)
        file = argv[i++];
    if (i != argc) {
        fprintf(stderr, "%s: only 1 file allowed\n", argv[0]);
        usage(argv[0], stderr);
        return 1;
    }

    source.file = file ? file : "<stdin>";
    if (!parse_open(&source, file ? fopen(file, "r") : stdin)) {
        fprintf(stderr, "failed to open file %s %s\n", source.file, strerror(errno));
        return 1;
    }

    source.keylength = strlen(source.keyword);

    if (output) {
        outfile = fopen(output, "w");
        if (!outfile) {
            fprintf(stderr, "failed to open file %s: %s\n", output, strerror(errno));
            return 1;
        }
    }
    generate(outfile, &source);
    if (outfile != stdout)
      fclose(outfile);
    parse_close(&source);

    return 0;
}
