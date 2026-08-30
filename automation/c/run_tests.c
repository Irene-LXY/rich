/*
 * Rich Monopoly automation test runner (C, single file, no external deps).
 *
 * Contract (test-side defined, see README.md):
 *   <program> <test_case.json> <map.json>
 * The program writes to stdout either:
 *   - the full Actual state object (the value of "actual" in spec table 28), or
 *   - a full result report object containing "result" and "errors".
 *
 * Build: from the automation root run "cmake -S . -B build && cmake --build build".
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#define popen _popen
#define pclose _pclose
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

#ifndef PATHBUF
#define PATHBUF 4096
#endif

/* ------------------------------------------------------------------ */
/* small helpers                                                       */
/* ------------------------------------------------------------------ */

static void *xmalloc(size_t n) {
    void *p = malloc(n ? n : 1);
    if (!p) { fprintf(stderr, "out of memory\n"); exit(1); }
    return p;
}

static void *xrealloc(void *p, size_t n) {
    void *q = realloc(p, n ? n : 1);
    if (!q) { fprintf(stderr, "out of memory\n"); exit(1); }
    return q;
}

static char *xstrdup(const char *s) {
    size_t n = strlen(s);
    char *p = xmalloc(n + 1);
    memcpy(p, s, n + 1);
    return p;
}

static char *xstrndup(const char *s, size_t n) {
    char *p = xmalloc(n + 1);
    memcpy(p, s, n);
    p[n] = 0;
    return p;
}

static int ends_with_ci(const char *s, const char *suffix) {
    size_t a = strlen(s), b = strlen(suffix);
    if (b > a) return 0;
    const char *t = s + (a - b);
    while (*t && *suffix) {
        if (tolower((unsigned char)*t) != tolower((unsigned char)*suffix)) return 0;
        t++; suffix++;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* growable string buffer                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} StrBuf;

static void sb_init(StrBuf *b) { b->data = NULL; b->len = 0; b->cap = 0; }

static void sb_reserve(StrBuf *b, size_t extra) {
    if (b->len + extra + 1 > b->cap) {
        size_t nc = b->cap ? b->cap : 64;
        while (nc < b->len + extra + 1) nc *= 2;
        b->data = xrealloc(b->data, nc);
        b->cap = nc;
    }
}

static void sb_append_char(StrBuf *b, char c) {
    sb_reserve(b, 1);
    b->data[b->len++] = c;
    b->data[b->len] = 0;
}

static void sb_append_bytes(StrBuf *b, const char *s, size_t n) {
    sb_reserve(b, n);
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = 0;
}

static void sb_append_str(StrBuf *b, const char *s) {
    sb_append_bytes(b, s, strlen(s));
}

static void sb_append_utf8(StrBuf *b, unsigned cp) {
    if (cp < 0x80) {
        sb_append_char(b, (char)cp);
    } else if (cp < 0x800) {
        sb_append_char(b, (char)(0xC0 | (cp >> 6)));
        sb_append_char(b, (char)(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        sb_append_char(b, (char)(0xE0 | (cp >> 12)));
        sb_append_char(b, (char)(0x80 | ((cp >> 6) & 0x3F)));
        sb_append_char(b, (char)(0x80 | (cp & 0x3F)));
    } else {
        sb_append_char(b, (char)(0xF0 | (cp >> 18)));
        sb_append_char(b, (char)(0x80 | ((cp >> 12) & 0x3F)));
        sb_append_char(b, (char)(0x80 | ((cp >> 6) & 0x3F)));
        sb_append_char(b, (char)(0x80 | (cp & 0x3F)));
    }
}

static char *sb_detach(StrBuf *b) {
    if (!b->data) return xstrdup("");
    char *p = b->data;
    b->data = NULL; b->len = 0; b->cap = 0;
    return p;
}

static void sb_free(StrBuf *b) {
    free(b->data);
    b->data = NULL; b->len = 0; b->cap = 0;
}

/* ------------------------------------------------------------------ */
/* JSON value model                                                    */
/* ------------------------------------------------------------------ */

typedef enum { J_NULL, J_BOOL, J_INT, J_STRING, J_ARRAY, J_OBJECT } JType;

typedef struct JValue JValue;

typedef struct {
    char *key;
    JValue *val;
} JMember;

struct JValue {
    JType type;
    long long i;
    int b;
    char *s;
    JValue **items;
    size_t count;
    size_t cap;
    JMember *members;
};

static JValue *json_new(JType t) {
    JValue *v = xmalloc(sizeof(*v));
    memset(v, 0, sizeof(*v));
    v->type = t;
    return v;
}

static JValue *json_new_string(const char *s) {
    JValue *v = json_new(J_STRING);
    v->s = xstrdup(s ? s : "");
    return v;
}

static void array_append(JValue *arr, JValue *item) {
    if (arr->count == arr->cap) {
        arr->cap = arr->cap ? arr->cap * 2 : 8;
        arr->items = xrealloc(arr->items, arr->cap * sizeof(JValue *));
    }
    arr->items[arr->count++] = item;
}

static void obj_append_move(JValue *obj, char *key, JValue *val) {
    if (obj->count == obj->cap) {
        obj->cap = obj->cap ? obj->cap * 2 : 8;
        obj->members = xrealloc(obj->members, obj->cap * sizeof(JMember));
    }
    obj->members[obj->count].key = key;
    obj->members[obj->count].val = val;
    obj->count++;
}

static void json_free(JValue *v);

static JValue *json_clone(const JValue *v) {
    if (!v) return json_new(J_NULL);
    JValue *c = json_new(v->type);
    switch (v->type) {
        case J_NULL: break;
        case J_BOOL: c->b = v->b; break;
        case J_INT: c->i = v->i; break;
        case J_STRING: c->s = xstrdup(v->s); break;
        case J_ARRAY:
            for (size_t i = 0; i < v->count; i++) array_append(c, json_clone(v->items[i]));
            break;
        case J_OBJECT:
            for (size_t i = 0; i < v->count; i++)
                obj_append_move(c, xstrdup(v->members[i].key), json_clone(v->members[i].val));
            break;
    }
    return c;
}

static void json_free(JValue *v) {
    if (!v) return;
    switch (v->type) {
        case J_STRING: free(v->s); break;
        case J_ARRAY:
            for (size_t i = 0; i < v->count; i++) json_free(v->items[i]);
            free(v->items);
            break;
        case J_OBJECT:
            for (size_t i = 0; i < v->count; i++) { free(v->members[i].key); json_free(v->members[i].val); }
            free(v->members);
            break;
        default: break;
    }
    free(v);
}

static const JValue *object_get(const JValue *o, const char *key) {
    if (!o || o->type != J_OBJECT) return NULL;
    for (size_t i = 0; i < o->count; i++)
        if (strcmp(o->members[i].key, key) == 0) return o->members[i].val;
    return NULL;
}

static const char *object_get_string(const JValue *o, const char *key) {
    const JValue *v = object_get(o, key);
    return (v && v->type == J_STRING) ? v->s : NULL;
}

/* ------------------------------------------------------------------ */
/* JSON parser                                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *s;
    size_t n;
    size_t pos;
    int err;
    char errmsg[256];
} Parser;

static void set_err(Parser *p, const char *msg) {
    if (!p->err) {
        p->err = 1;
        snprintf(p->errmsg, sizeof(p->errmsg), "%s", msg);
    }
}

static void skip_ws(Parser *p) {
    while (p->pos < p->n && (p->s[p->pos] == ' ' || p->s[p->pos] == '\t' ||
           p->s[p->pos] == '\n' || p->s[p->pos] == '\r'))
        p->pos++;
}

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int parse_hex4(Parser *p, unsigned *out) {
    if (p->pos + 4 > p->n) { set_err(p, "bad unicode escape"); return 0; }
    unsigned v = 0;
    for (int i = 0; i < 4; i++) {
        int h = hexval(p->s[p->pos + i]);
        if (h < 0) { set_err(p, "bad unicode escape"); return 0; }
        v = v * 16 + (unsigned)h;
    }
    p->pos += 4;
    *out = v;
    return 1;
}

static char *parse_string(Parser *p) {
    if (p->pos >= p->n || p->s[p->pos] != '"') { set_err(p, "expected string"); return NULL; }
    p->pos++;
    StrBuf b; sb_init(&b);
    while (p->pos < p->n && p->s[p->pos] != '"') {
        unsigned char c = (unsigned char)p->s[p->pos++];
        if (c == '\\') {
            if (p->pos >= p->n) { set_err(p, "unterminated string"); sb_free(&b); return NULL; }
            char e = p->s[p->pos++];
            switch (e) {
                case '"': sb_append_char(&b, '"'); break;
                case '\\': sb_append_char(&b, '\\'); break;
                case '/': sb_append_char(&b, '/'); break;
                case 'b': sb_append_char(&b, '\b'); break;
                case 'f': sb_append_char(&b, '\f'); break;
                case 'n': sb_append_char(&b, '\n'); break;
                case 'r': sb_append_char(&b, '\r'); break;
                case 't': sb_append_char(&b, '\t'); break;
                case 'u': {
                    unsigned cp;
                    if (!parse_hex4(p, &cp)) { sb_free(&b); return NULL; }
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (p->pos + 1 < p->n && p->s[p->pos] == '\\' && p->s[p->pos + 1] == 'u') {
                            size_t save = p->pos;
                            p->pos += 2;
                            unsigned lo;
                            if (parse_hex4(p, &lo) && lo >= 0xDC00 && lo <= 0xDFFF) {
                                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                            } else {
                                p->pos = save;
                                cp = 0xFFFD;
                            }
                        } else {
                            cp = 0xFFFD;
                        }
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        cp = 0xFFFD;
                    }
                    sb_append_utf8(&b, cp);
                    break;
                }
                default: set_err(p, "invalid escape"); sb_free(&b); return NULL;
            }
        } else {
            sb_append_char(&b, (char)c);
        }
    }
    if (p->pos >= p->n) { set_err(p, "unterminated string"); sb_free(&b); return NULL; }
    p->pos++; /* closing quote */
    return sb_detach(&b);
}

static JValue *parse_value(Parser *p);
static JValue *parse_object(Parser *p);
static JValue *parse_array(Parser *p);

static JValue *parse_literal(Parser *p, const char *lit, JType t, long long i, int b) {
    size_t len = strlen(lit);
    if (p->pos + len > p->n || strncmp(p->s + p->pos, lit, len) != 0) {
        set_err(p, "invalid literal");
        return NULL;
    }
    p->pos += len;
    JValue *v = json_new(t);
    if (t == J_INT) v->i = i;
    if (t == J_BOOL) v->b = b;
    return v;
}

static JValue *parse_number(Parser *p) {
    size_t start = p->pos;
    if (p->s[p->pos] == '-') p->pos++;
    if (p->pos >= p->n || !isdigit((unsigned char)p->s[p->pos])) {
        set_err(p, "invalid number");
        return NULL;
    }
    while (p->pos < p->n && isdigit((unsigned char)p->s[p->pos])) p->pos++;
    if (p->pos < p->n && (p->s[p->pos] == '.' || p->s[p->pos] == 'e' || p->s[p->pos] == 'E')) {
        set_err(p, "floating point numbers are not allowed");
        return NULL;
    }
    size_t len = p->pos - start;
    char *tmp = xstrndup(p->s + start, len);
    char *endp = NULL;
    errno = 0;
    long long v = strtoll(tmp, &endp, 10);
    int ok = (endp && *endp == 0 && errno != ERANGE);
    free(tmp);
    if (!ok) { set_err(p, "number out of range"); return NULL; }
    JValue *j = json_new(J_INT);
    j->i = v;
    return j;
}

static JValue *parse_object(Parser *p) {
    p->pos++; /* { */
    JValue *obj = json_new(J_OBJECT);
    skip_ws(p);
    if (p->pos < p->n && p->s[p->pos] == '}') { p->pos++; return obj; }
    for (;;) {
        skip_ws(p);
        if (p->pos >= p->n || p->s[p->pos] != '"') { set_err(p, "expected object key"); json_free(obj); return NULL; }
        char *key = parse_string(p);
        if (!key) { json_free(obj); return NULL; }
        skip_ws(p);
        if (p->pos >= p->n || p->s[p->pos] != ':') { set_err(p, "expected ':'"); free(key); json_free(obj); return NULL; }
        p->pos++;
        JValue *val = parse_value(p);
        if (!val) { free(key); json_free(obj); return NULL; }
        obj_append_move(obj, key, val);
        skip_ws(p);
        if (p->pos >= p->n) { set_err(p, "unterminated object"); json_free(obj); return NULL; }
        char c = p->s[p->pos];
        if (c == ',') { p->pos++; continue; }
        if (c == '}') { p->pos++; return obj; }
        set_err(p, "expected ',' or '}'");
        json_free(obj);
        return NULL;
    }
}

static JValue *parse_array(Parser *p) {
    p->pos++; /* [ */
    JValue *arr = json_new(J_ARRAY);
    skip_ws(p);
    if (p->pos < p->n && p->s[p->pos] == ']') { p->pos++; return arr; }
    for (;;) {
        JValue *val = parse_value(p);
        if (!val) { json_free(arr); return NULL; }
        array_append(arr, val);
        skip_ws(p);
        if (p->pos >= p->n) { set_err(p, "unterminated array"); json_free(arr); return NULL; }
        char c = p->s[p->pos];
        if (c == ',') { p->pos++; continue; }
        if (c == ']') { p->pos++; return arr; }
        set_err(p, "expected ',' or ']'");
        json_free(arr);
        return NULL;
    }
}

static JValue *parse_value(Parser *p) {
    skip_ws(p);
    if (p->pos >= p->n) { set_err(p, "unexpected end of input"); return NULL; }
    char c = p->s[p->pos];
    if (c == '{') return parse_object(p);
    if (c == '[') return parse_array(p);
    if (c == '"') {
        char *s = parse_string(p);
        if (!s) return NULL;
        JValue *v = json_new(J_STRING);
        v->s = s;
        return v;
    }
    if (c == 't') return parse_literal(p, "true", J_BOOL, 0, 1);
    if (c == 'f') return parse_literal(p, "false", J_BOOL, 0, 0);
    if (c == 'n') return parse_literal(p, "null", J_NULL, 0, 0);
    if (c == '-' || (c >= '0' && c <= '9')) return parse_number(p);
    set_err(p, "unexpected character");
    return NULL;
}

static JValue *json_load(const char *text, char *err, size_t errsz) {
    Parser p;
    p.s = text;
    p.n = strlen(text);
    p.pos = 0;
    p.err = 0;
    p.errmsg[0] = 0;
    JValue *v = parse_value(&p);
    if (p.err) {
        if (err) snprintf(err, errsz, "%s", p.errmsg);
        json_free(v);
        return NULL;
    }
    skip_ws(&p);
    if (p.pos != p.n) {
        if (err) snprintf(err, errsz, "trailing data at byte %zu", p.pos);
        json_free(v);
        return NULL;
    }
    return v;
}

/* ------------------------------------------------------------------ */
/* JSON writer                                                         */
/* ------------------------------------------------------------------ */

static void json_write_string(FILE *f, const char *s) {
    fputc('"', f);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
            case '"': fputs("\\\"", f); break;
            case '\\': fputs("\\\\", f); break;
            case '\b': fputs("\\b", f); break;
            case '\f': fputs("\\f", f); break;
            case '\n': fputs("\\n", f); break;
            case '\r': fputs("\\r", f); break;
            case '\t': fputs("\\t", f); break;
            default:
                if (*p < 0x20) fprintf(f, "\\u%04x", (unsigned)*p);
                else fputc(*p, f);
        }
    }
    fputc('"', f);
}

static void json_write(FILE *f, const JValue *v) {
    if (!v) { fputs("null", f); return; }
    switch (v->type) {
        case J_NULL: fputs("null", f); break;
        case J_BOOL: fputs(v->b ? "true" : "false", f); break;
        case J_INT: fprintf(f, "%lld", v->i); break;
        case J_STRING: json_write_string(f, v->s); break;
        case J_ARRAY:
            fputc('[', f);
            for (size_t i = 0; i < v->count; i++) {
                if (i) fputc(',', f);
                json_write(f, v->items[i]);
            }
            fputc(']', f);
            break;
        case J_OBJECT:
            fputc('{', f);
            for (size_t i = 0; i < v->count; i++) {
                if (i) fputc(',', f);
                json_write_string(f, v->members[i].key);
                fputc(':', f);
                json_write(f, v->members[i].val);
            }
            fputc('}', f);
            break;
    }
}

/* ------------------------------------------------------------------ */
/* partial matching (spec section 11)                                  */
/* ------------------------------------------------------------------ */

static int value_equal_scalar(const JValue *a, const JValue *b) {
    if (!a || !b || a->type != b->type) return 0;
    switch (a->type) {
        case J_NULL: return 1;
        case J_BOOL: return a->b == b->b;
        case J_INT: return a->i == b->i;
        case J_STRING: return strcmp(a->s, b->s) == 0;
        default: return 0;
    }
}

static int json_strict_equal(const JValue *a, const JValue *b) {
    if (!a || !b || a->type != b->type) return 0;
    switch (a->type) {
        case J_NULL: return 1;
        case J_BOOL: return a->b == b->b;
        case J_INT: return a->i == b->i;
        case J_STRING: return strcmp(a->s, b->s) == 0;
        case J_ARRAY: {
            if (a->count != b->count) return 0;
            for (size_t i = 0; i < a->count; i++)
                if (!json_strict_equal(a->items[i], b->items[i])) return 0;
            return 1;
        }
        case J_OBJECT: {
            if (a->count != b->count) return 0;
            for (size_t i = 0; i < a->count; i++) {
                const JValue *bv = object_get(b, a->members[i].key);
                if (!bv || !json_strict_equal(a->members[i].val, bv)) return 0;
            }
            return 1;
        }
    }
    return 0;
}

static void append_error(JValue *errors, const char *code, const char *path,
                         const JValue *expected, const JValue *actual, const char *message) {
    JValue *e = json_new(J_OBJECT);
    obj_append_move(e, xstrdup("code"), json_new_string(code));
    obj_append_move(e, xstrdup("path"), json_new_string(path));
    obj_append_move(e, xstrdup("expected"), expected ? json_clone(expected) : json_new(J_NULL));
    obj_append_move(e, xstrdup("actual"), actual ? json_clone(actual) : json_new(J_NULL));
    obj_append_move(e, xstrdup("message"), json_new_string(message));
    array_append(errors, e);
}

static void match_object(const JValue *exp, const JValue *act, const char *path, JValue *errors);

static void match_object(const JValue *exp, const JValue *act, const char *path, JValue *errors) {
    if (!exp || exp->type != J_OBJECT) return;
    for (size_t mi = 0; mi < exp->count; mi++) {
        const char *key = exp->members[mi].key;
        const JValue *ev = exp->members[mi].val;

        if (strcmp(key, "properties_absent") == 0 || strcmp(key, "map_items_absent") == 0) {
            const char *arrname = (strcmp(key, "properties_absent") == 0) ? "properties" : "map_items";
            const JValue *arr = object_get(act, arrname);
            if (ev->type == J_ARRAY) {
                for (size_t j = 0; j < ev->count; j++) {
                    const JValue *pos = ev->items[j];
                    if (pos->type != J_INT) continue;
                    int found = 0;
                    if (arr && arr->type == J_ARRAY) {
                        for (size_t k = 0; k < arr->count; k++) {
                            const JValue *it = arr->items[k];
                            const JValue *pv = object_get(it, "position");
                            if (pv && pv->type == J_INT && pv->i == pos->i) { found = 1; break; }
                        }
                    }
                    if (found) {
                        char p[PATHBUF];
                        snprintf(p, sizeof(p), "%s.%s[position=%lld]", path, arrname, pos->i);
                        append_error(errors, "ASSERT_NOT_ABSENT", p, NULL, pos, "item should not exist");
                    }
                }
            }
            continue;
        }

        const char *pk = NULL;
        if (strcmp(key, "players") == 0) pk = "id";
        else if (strcmp(key, "properties") == 0 || strcmp(key, "map_items") == 0 ||
                 strcmp(key, "display_players") == 0) pk = "position";

        if (pk) {
            const JValue *arr = object_get(act, key);
            if (ev->type == J_ARRAY) {
                for (size_t j = 0; j < ev->count; j++) {
                    const JValue *exp_item = ev->items[j];
                    if (exp_item->type != J_OBJECT) {
                        char p[PATHBUF];
                        snprintf(p, sizeof(p), "%s.%s", path, key);
                        append_error(errors, "ASSERT_NOT_EQUAL", p, exp_item, NULL, "array element must be an object");
                        continue;
                    }
                    const JValue *exp_pk = object_get(exp_item, pk);
                    const JValue *found = NULL;
                    if (arr && arr->type == J_ARRAY) {
                        for (size_t k = 0; k < arr->count; k++) {
                            const JValue *ait = arr->items[k];
                            const JValue *apk = object_get(ait, pk);
                            if (apk && exp_pk && value_equal_scalar(apk, exp_pk)) { found = ait; break; }
                        }
                    }
                    char p[PATHBUF];
                    if (exp_pk && exp_pk->type == J_STRING)
                        snprintf(p, sizeof(p), "%s.%s[%s=%s]", path, key, pk, exp_pk->s);
                    else if (exp_pk && exp_pk->type == J_INT)
                        snprintf(p, sizeof(p), "%s.%s[%s=%lld]", path, key, pk, exp_pk->i);
                    else
                        snprintf(p, sizeof(p), "%s.%s", path, key);
                    if (!found) append_error(errors, "ASSERT_NOT_FOUND", p, exp_item, NULL, "record not found");
                    else match_object(exp_item, found, p, errors);
                }
            }
            continue;
        }

        char p[PATHBUF];
        snprintf(p, sizeof(p), "%s.%s", path, key);
        const JValue *av = object_get(act, key);
        if (!av) {
            append_error(errors, "ASSERT_NOT_EQUAL", p, ev, NULL, "field missing in actual");
            continue;
        }
        if (ev->type == J_OBJECT) {
            if (av->type != J_OBJECT) append_error(errors, "ASSERT_NOT_EQUAL", p, ev, av, "expected object, actual is not object");
            else match_object(ev, av, p, errors);
        } else if (ev->type == J_ARRAY) {
            if (!json_strict_equal(ev, av)) append_error(errors, "ASSERT_NOT_EQUAL", p, ev, av, "array not equal (ordered, strict)");
        } else {
            if (!json_strict_equal(ev, av)) append_error(errors, "ASSERT_NOT_EQUAL", p, ev, av, "scalar not equal");
        }
    }
}

/* ------------------------------------------------------------------ */
/* file reading                                                        */
/* ------------------------------------------------------------------ */

static char *read_file(const char *path, char *err, size_t errsz) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        if (err) snprintf(err, errsz, "cannot open file: %s", path);
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        if (err) snprintf(err, errsz, "cannot seek file: %s", path);
        fclose(f);
        return NULL;
    }
    long sz = ftell(f);
    if (sz < 0) {
        if (err) snprintf(err, errsz, "cannot get file size: %s", path);
        fclose(f);
        return NULL;
    }
    rewind(f);
    char *buf = xmalloc((size_t)sz + 1);
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[got] = 0;
    if (got >= 3 && (unsigned char)buf[0] == 0xEF && (unsigned char)buf[1] == 0xBB &&
        (unsigned char)buf[2] == 0xBF) {
        memmove(buf, buf + 3, got - 3 + 1);
    }
    return buf;
}

static int file_exists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

static int path_is_dir(const char *path) {
#ifdef _WIN32
    struct _stat st;
    if (_stat(path, &st) != 0) return 0;
    return (st.st_mode & _S_IFDIR) != 0;
#else
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
#endif
}

/* ------------------------------------------------------------------ */
/* directory scan                                                      */
/* ------------------------------------------------------------------ */

typedef struct {
    char **items;
    size_t count;
    size_t cap;
} FileList;

static void fl_add(FileList *fl, const char *s) {
    if (fl->count == fl->cap) {
        fl->cap = fl->cap ? fl->cap * 2 : 16;
        fl->items = xrealloc(fl->items, fl->cap * sizeof(char *));
    }
    fl->items[fl->count++] = xstrdup(s);
}

static void scan_dir(const char *dir, FileList *fl);

#ifdef _WIN32
static void scan_dir(const char *dir, FileList *fl) {
    char pattern[PATHBUF];
    snprintf(pattern, sizeof(pattern), "%s\\*", dir);
    struct _finddata_t info;
    intptr_t h = _findfirst(pattern, &info);
    if (h == -1) return;
    do {
        if (strcmp(info.name, ".") == 0 || strcmp(info.name, "..") == 0) continue;
        char full[PATHBUF];
        snprintf(full, sizeof(full), "%s\\%s", dir, info.name);
        if (info.attrib & _A_SUBDIR) scan_dir(full, fl);
        else if (ends_with_ci(full, ".json")) fl_add(fl, full);
    } while (_findnext(h, &info) == 0);
    _findclose(h);
}
#else
static void scan_dir(const char *dir, FileList *fl) {
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        char full[PATHBUF];
        snprintf(full, sizeof(full), "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) scan_dir(full, fl);
        else if (ends_with_ci(full, ".json")) fl_add(fl, full);
    }
    closedir(d);
}
#endif

/* ------------------------------------------------------------------ */
/* process execution                                                   */
/* ------------------------------------------------------------------ */

static char *quote_arg(const char *s) {
    size_t n = strlen(s);
    char *out = xmalloc(n + 3);
    out[0] = '"';
    memcpy(out + 1, s, n);
    out[n + 1] = '"';
    out[n + 2] = 0;
    return out;
}

static char *run_program(const char *program, const char *case_path, const char *map_path,
                         char *err, size_t errsz) {
    char *qcase = quote_arg(case_path);
    char *qmap = quote_arg(map_path);
    size_t len = strlen(program) + 1 + strlen(qcase) + 1 + strlen(qmap) + 1;
    char *cmd = xmalloc(len);
    snprintf(cmd, len, "%s %s %s", program, qcase, qmap);

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        snprintf(err, errsz, "failed to start program: %s", cmd);
        free(cmd);
        free(qcase);
        free(qmap);
        return NULL;
    }

    StrBuf out;
    sb_init(&out);
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) sb_append_bytes(&out, buf, n);
    pclose(fp);
    free(cmd);
    free(qcase);
    free(qmap);
    return sb_detach(&out);
}

/* ------------------------------------------------------------------ */
/* argument parsing                                                    */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *program;
    const char *cases;
    const char *map;
    const char *map_dir;
    const char *out;
    const char *junit;
    int quiet;
} Options;

static void usage(FILE *f, const char *prog) {
    fprintf(f,
        "usage: %s --program <cmd> --cases <file|dir> [options]\n"
        "  --program <cmd>   program command (executed verbatim, then two file args are appended)\n"
        "  --cases <path>    test case JSON file or directory (recursive *.json)\n"
        "  --map <path>      map file (overrides case map_file)\n"
        "  --map-dir <dir>   directory used to resolve map_file (default: spec/ under cwd)\n"
        "  --out <file>      result output file (default: results.json)\n"
        "  --junit <file>    optional JUnit XML output\n"
        "  --quiet           print summary only\n",
        prog);
}

static int parse_args(int argc, char **argv, Options *o) {
    memset(o, 0, sizeof(*o));
    o->out = "results.json";
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) { usage(stdout, argv[0]); exit(0); }
        else if (strcmp(a, "--quiet") == 0) o->quiet = 1;
        else if (strcmp(a, "--program") == 0) { if (++i >= argc) { fprintf(stderr, "missing value for %s\n", a); return -1; } o->program = argv[i]; }
        else if (strcmp(a, "--cases") == 0) { if (++i >= argc) { fprintf(stderr, "missing value for %s\n", a); return -1; } o->cases = argv[i]; }
        else if (strcmp(a, "--map") == 0) { if (++i >= argc) { fprintf(stderr, "missing value for %s\n", a); return -1; } o->map = argv[i]; }
        else if (strcmp(a, "--map-dir") == 0) { if (++i >= argc) { fprintf(stderr, "missing value for %s\n", a); return -1; } o->map_dir = argv[i]; }
        else if (strcmp(a, "--out") == 0) { if (++i >= argc) { fprintf(stderr, "missing value for %s\n", a); return -1; } o->out = argv[i]; }
        else if (strcmp(a, "--junit") == 0) { if (++i >= argc) { fprintf(stderr, "missing value for %s\n", a); return -1; } o->junit = argv[i]; }
        else { fprintf(stderr, "unknown option: %s\n\n", a); usage(stderr, argv[0]); return -1; }
    }
    if (!o->program || !o->cases) { usage(stderr, argv[0]); return -1; }
    return 0;
}

/* ------------------------------------------------------------------ */
/* case result + reporting                                             */
/* ------------------------------------------------------------------ */

typedef struct {
    char *case_id;
    char *file;
    char result[8];
    JValue *errors; /* J_ARRAY, may be empty */
} CaseResult;

typedef struct {
    CaseResult *items;
    size_t count;
    size_t cap;
} CaseList;

static void cl_add(CaseList *cl, CaseResult r) {
    if (cl->count == cl->cap) {
        cl->cap = cl->cap ? cl->cap * 2 : 16;
        cl->items = xrealloc(cl->items, cl->cap * sizeof(CaseResult));
    }
    cl->items[cl->count++] = r;
}

static void result_free(CaseResult *r) {
    free(r->case_id);
    free(r->file);
    json_free(r->errors);
    r->errors = NULL;
}

static const char *file_stem(const char *path) {
    const char *base = strrchr(path, '\\');
    if (!base) base = strrchr(path, '/');
    base = base ? base + 1 : path;
    static char buf[PATHBUF];
    snprintf(buf, sizeof(buf), "%s", base);
    char *dot = strrchr(buf, '.');
    if (dot) *dot = 0;
    return buf;
}

static void xml_escape(FILE *f, const char *s) {
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
            case '&': fputs("&amp;", f); break;
            case '<': fputs("&lt;", f); break;
            case '>': fputs("&gt;", f); break;
            case '"': fputs("&quot;", f); break;
            case '\'': fputs("&apos;", f); break;
            default: fputc(*p, f);
        }
    }
}

static void print_errors(const JValue *errors) {
    if (!errors || errors->type != J_ARRAY) return;
    for (size_t i = 0; i < errors->count; i++) {
        const JValue *e = errors->items[i];
        const char *code = object_get_string(e, "code");
        const char *path = object_get_string(e, "path");
        const char *msg = object_get_string(e, "message");
        printf("       - [%s] %s: %s\n", code ? code : "?", path ? path : "", msg ? msg : "");
    }
}

/* ------------------------------------------------------------------ */
/* judging                                                             */
/* ------------------------------------------------------------------ */

static int error_code_present(const JValue *errors_array, const char *code) {
    size_t i;
    if (errors_array == NULL || errors_array->type != J_ARRAY) {
        return 0;
    }
    for (i = 0; i < errors_array->count; i++) {
        const char *c = object_get_string(errors_array->items[i], "code");
        if (c != NULL && strcmp(c, code) == 0) {
            return 1;
        }
    }
    return 0;
}

static const char *judge(const JValue *case_obj, const JValue *out_obj, JValue **errors_out) {
    JValue *errors = json_new(J_ARRAY);
    const char *expected_result = object_get_string(case_obj, "expected_result");
    const JValue *actual = object_get(out_obj, "actual");
    const JValue *resultv = object_get(out_obj, "result");

    if (expected_result != NULL && strcmp(expected_result, "ERROR") == 0) {
        const JValue *program_errors = object_get(out_obj, "errors");
        const JValue *expected_errors = object_get(case_obj, "expected_errors");
        size_t i;

        if (resultv == NULL || resultv->type != J_STRING || strcmp(resultv->s, "ERROR") != 0) {
            append_error(errors, "EXPECTED_ERROR", "actual.result", NULL, NULL,
                         "case expects ERROR but the program did not report ERROR");
            *errors_out = errors;
            return "FAIL";
        }

        if (expected_errors != NULL && expected_errors->type == J_ARRAY) {
            for (i = 0; i < expected_errors->count; i++) {
                const char *exp_code = object_get_string(expected_errors->items[i], "code");
                if (exp_code == NULL) {
                    continue;
                }
                if (!error_code_present(program_errors, exp_code)) {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "expected error code '%s' was not produced", exp_code);
                    append_error(errors, "ASSERT_NOT_FOUND", "actual.errors", NULL, NULL, msg);
                }
            }
        }

        *errors_out = errors;
        return (errors->count == 0) ? "PASS" : "FAIL";
    }

    if (actual) {
        const JValue *expected = object_get(case_obj, "expected");
        JValue *empty = json_new(J_OBJECT);
        match_object(expected ? expected : empty, actual, "actual", errors);
        json_free(empty);
        *errors_out = errors;
        return (errors->count == 0) ? "PASS" : "FAIL";
    }

    if (resultv && resultv->type == J_STRING) {
        const JValue *errs = object_get(out_obj, "errors");
        if (errs && errs->type == J_ARRAY)
            for (size_t i = 0; i < errs->count; i++) array_append(errors, json_clone(errs->items[i]));
        *errors_out = errors;
        if (strcmp(resultv->s, "PASS") == 0 || strcmp(resultv->s, "FAIL") == 0 ||
            strcmp(resultv->s, "ERROR") == 0)
            return resultv->s;
        return "ERROR";
    }

    if (object_get(out_obj, "users") || object_get(out_obj, "players") ||
        object_get(out_obj, "current_user") || object_get(out_obj, "game_status")) {
        const JValue *expected = object_get(case_obj, "expected");
        JValue *empty = json_new(J_OBJECT);
        match_object(expected ? expected : empty, out_obj, "actual", errors);
        json_free(empty);
        *errors_out = errors;
        return (errors->count == 0) ? "PASS" : "FAIL";
    }

    append_error(errors, "INVALID_JSON", "actual", NULL, NULL,
                 "program output is neither Actual state nor a result report");
    *errors_out = errors;
    return "ERROR";
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv) {
    Options o;
    int rc = parse_args(argc, argv, &o);
    if (rc != 0) return 2;

    FileList files;
    memset(&files, 0, sizeof(files));
    if (path_is_dir(o.cases)) scan_dir(o.cases, &files);
    else if (file_exists(o.cases)) fl_add(&files, o.cases);
    else { fprintf(stderr, "cases path not found: %s\n", o.cases); return 2; }
    if (files.count == 0) {
        fprintf(stderr, "no .json test cases found under: %s\n", o.cases);
        return 2;
    }

    CaseList list;
    memset(&list, 0, sizeof(list));

    for (size_t fi = 0; fi < files.count; fi++) {
        const char *case_path = files.items[fi];
        char err[512] = "";

        char *text = read_file(case_path, err, sizeof(err));
        if (!text) {
            CaseResult r;
            r.case_id = xstrdup(file_stem(case_path));
            r.file = xstrdup(case_path);
            snprintf(r.result, sizeof(r.result), "ERROR");
            r.errors = json_new(J_ARRAY);
            append_error(r.errors, "INVALID_JSON", "", NULL, NULL, err);
            cl_add(&list, r);
            continue;
        }

        JValue *case_obj = json_load(text, err, sizeof(err));
        free(text);
        if (!case_obj) {
            CaseResult r;
            r.case_id = xstrdup(file_stem(case_path));
            r.file = xstrdup(case_path);
            snprintf(r.result, sizeof(r.result), "ERROR");
            r.errors = json_new(J_ARRAY);
            append_error(r.errors, "INVALID_JSON", "", NULL, NULL, err);
            cl_add(&list, r);
            continue;
        }

        const char *cid = object_get_string(case_obj, "case_id");
        const char *map_file = object_get_string(case_obj, "map_file");
        char map_path[PATHBUF];
        map_path[0] = 0;

        if (o.map) {
            if (!file_exists(o.map)) snprintf(err, sizeof(err), "map file not found: %s", o.map);
            else snprintf(map_path, sizeof(map_path), "%s", o.map);
        } else {
            char case_dir[PATHBUF];
            snprintf(case_dir, sizeof(case_dir), "%s", case_path);
            char *sep = strrchr(case_dir, '\\');
            if (!sep) sep = strrchr(case_dir, '/');
            if (sep) *sep = 0; else strcpy(case_dir, ".");
            char cand[PATHBUF];
            if (map_file) {
                snprintf(cand, sizeof(cand), "%s%c%s", case_dir,
#ifdef _WIN32
                    '\\',
#else
                    '/',
#endif
                    map_file);
                if (file_exists(cand)) snprintf(map_path, sizeof(map_path), "%s", cand);
            }
            if (!map_path[0] && o.map_dir && map_file) {
                snprintf(cand, sizeof(cand), "%s%c%s", o.map_dir,
#ifdef _WIN32
                    '\\',
#else
                    '/',
#endif
                    map_file);
                if (file_exists(cand)) snprintf(map_path, sizeof(map_path), "%s", cand);
            }
            if (!map_path[0] && map_file) {
                snprintf(cand, sizeof(cand), "spec%c%s",
#ifdef _WIN32
                    '\\',
#else
                    '/',
#endif
                    map_file);
                if (file_exists(cand)) snprintf(map_path, sizeof(map_path), "%s", cand);
            }
        }

        CaseResult r;
        r.case_id = xstrdup(cid ? cid : file_stem(case_path));
        r.file = xstrdup(case_path);
        r.errors = json_new(J_ARRAY);

        if (!map_path[0]) {
            snprintf(r.result, sizeof(r.result), "ERROR");
            append_error(r.errors, "INVALID_MAP", "", NULL, NULL,
                         map_file ? "map file not found" : "case is missing map_file");
        } else {
            char *out_text = run_program(o.program, case_path, map_path, err, sizeof(err));
            if (!out_text) {
                snprintf(r.result, sizeof(r.result), "ERROR");
                append_error(r.errors, "PROCESS_ERROR", "", NULL, NULL, err);
            } else {
                JValue *out_obj = json_load(out_text, err, sizeof(err));
                free(out_text);
                if (!out_obj) {
                    snprintf(r.result, sizeof(r.result), "ERROR");
                    append_error(r.errors, "INVALID_JSON", "", NULL, NULL, err);
                } else {
                    JValue *errors = NULL;
                    const char *verdict = judge(case_obj, out_obj, &errors);
                    snprintf(r.result, sizeof(r.result), "%s", verdict);
                    json_free(r.errors);
                    r.errors = errors;
                    json_free(out_obj);
                }
            }
        }

        cl_add(&list, r);
        json_free(case_obj);
    }

    size_t total = list.count, pass = 0, fail = 0, errcnt = 0;
    for (size_t i = 0; i < list.count; i++) {
        const char *res = list.items[i].result;
        if (!o.quiet) {
            const char *mark = strcmp(res, "PASS") == 0 ? "PASS" : (strcmp(res, "FAIL") == 0 ? "FAIL" : "ERR ");
            printf("[%s] %s  (%s)\n", mark, list.items[i].case_id, list.items[i].file);
            if (strcmp(res, "PASS") != 0) print_errors(list.items[i].errors);
        }
        if (strcmp(res, "PASS") == 0) pass++;
        else if (strcmp(res, "FAIL") == 0) fail++;
        else errcnt++;
    }

    printf("\ntotal %zu | PASS %zu | FAIL %zu | ERROR %zu\n", total, pass, fail, errcnt);

    FILE *of = fopen(o.out, "wb");
    if (of) {
        fprintf(of, "{\n  \"schema_version\": \"1.0\",\n");
        fprintf(of, "  \"summary\": {\"total\": %zu, \"pass\": %zu, \"fail\": %zu, \"error\": %zu},\n",
                total, pass, fail, errcnt);
        fprintf(of, "  \"cases\": [\n");
        for (size_t i = 0; i < list.count; i++) {
            fprintf(of, "    {\"case_id\": ");
            json_write_string(of, list.items[i].case_id);
            fprintf(of, ", \"file\": ");
            json_write_string(of, list.items[i].file);
            fprintf(of, ", \"result\": ");
            json_write_string(of, list.items[i].result);
            if (list.items[i].errors && list.items[i].errors->count) {
                fprintf(of, ", \"errors\": ");
                json_write(of, list.items[i].errors);
            }
            fprintf(of, "}%s\n", (i + 1 < list.count) ? "," : "");
        }
        fprintf(of, "  ]\n}\n");
        fclose(of);
        printf("results written to: %s\n", o.out);
    } else {
        fprintf(stderr, "cannot write results to: %s\n", o.out);
    }

    if (o.junit) {
        FILE *jf = fopen(o.junit, "wb");
        if (jf) {
            fprintf(jf, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
            fprintf(jf, "<testsuites tests=\"%zu\" failures=\"%zu\" errors=\"0\">\n", total, fail + errcnt);
            fprintf(jf, "  <testsuite name=\"rich-automation\" tests=\"%zu\" failures=\"%zu\" errors=\"0\">\n", total, fail + errcnt);
            for (size_t i = 0; i < list.count; i++) {
                fprintf(jf, "    <testcase name=\"");
                xml_escape(jf, list.items[i].case_id);
                fprintf(jf, "\" classname=\"");
                xml_escape(jf, list.items[i].file);
                if (strcmp(list.items[i].result, "PASS") == 0) {
                    fprintf(jf, "\" />\n");
                } else {
                    fprintf(jf, "\">\n");
                    fprintf(jf, "      <failure message=\"");
                    xml_escape(jf, list.items[i].result);
                    fprintf(jf, "\">");
                    /* compact error text */
                    StrBuf sb; sb_init(&sb);
                    JValue *errors = list.items[i].errors;
                    if (errors && errors->type == J_ARRAY) {
                        for (size_t k = 0; k < errors->count; k++) {
                            const JValue *e = errors->items[k];
                            const char *code = object_get_string(e, "code");
                            const char *path = object_get_string(e, "path");
                            const char *msg = object_get_string(e, "message");
                            if (k) sb_append_str(&sb, "\n");
                            sb_append_str(&sb, "[");
                            sb_append_str(&sb, code ? code : "?");
                            sb_append_str(&sb, "] ");
                            sb_append_str(&sb, path ? path : "");
                            sb_append_str(&sb, ": ");
                            sb_append_str(&sb, msg ? msg : "");
                        }
                    }
                    xml_escape(jf, sb.data ? sb.data : "");
                    sb_free(&sb);
                    fprintf(jf, "</failure>\n");
                    fprintf(jf, "    </testcase>\n");
                }
            }
            fprintf(jf, "  </testsuite>\n");
            fprintf(jf, "</testsuites>\n");
            fclose(jf);
            printf("junit report written to: %s\n", o.junit);
        } else {
            fprintf(stderr, "cannot write junit report to: %s\n", o.junit);
        }
    }

    for (size_t i = 0; i < list.count; i++) result_free(&list.items[i]);
    free(list.items);
    for (size_t i = 0; i < files.count; i++) free(files.items[i]);
    free(files.items);

    return (fail == 0 && errcnt == 0) ? 0 : 1;
}
