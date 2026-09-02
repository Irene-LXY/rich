/*
 * monopoly.exe 交互黑盒测试（C 版，单文件，无第三方依赖）。
 *
 * 职责：启动交互版程序，按用例把若干行喂进 stdin，捕获 stdout，
 *       去掉 ANSI 清屏/颜色码后按子串断言（expect 必须出现、forbid 不得出现），
 *       汇总 PASS / FAIL / ERROR 并写 results JSON。
 *
 * 这是本组本地测试，不进入跨组 JSON 公共测试集。
 *
 * 编译：在上一级 automation 目录执行 "cmake -S . -B build && cmake --build build"。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
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
    const char *t;
    if (b > a) return 0;
    t = s + (a - b);
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
    char *p;
    if (!b->data) return xstrdup("");
    p = b->data;
    b->data = NULL; b->len = 0; b->cap = 0;
    return p;
}

static void sb_free(StrBuf *b) {
    free(b->data);
    b->data = NULL; b->len = 0; b->cap = 0;
}

/* ------------------------------------------------------------------ */
/* JSON value model + parser (读取用例)                                  */
/* ------------------------------------------------------------------ */

typedef enum { J_NULL, J_BOOL, J_INT, J_STRING, J_ARRAY, J_OBJECT } JType;

typedef struct JValue JValue;

typedef struct { char *key; JValue *val; } JMember;

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

static JValue *jv_new(JType t) {
    JValue *v = xmalloc(sizeof(*v));
    memset(v, 0, sizeof(*v));
    v->type = t;
    return v;
}

static void jv_array_add(JValue *arr, JValue *item) {
    if (arr->count == arr->cap) {
        arr->cap = arr->cap ? arr->cap * 2 : 8;
        arr->items = xrealloc(arr->items, arr->cap * sizeof(JValue *));
    }
    arr->items[arr->count++] = item;
}

static void jv_obj_add(JValue *obj, char *key, JValue *val) {
    if (obj->count == obj->cap) {
        obj->cap = obj->cap ? obj->cap * 2 : 8;
        obj->members = xrealloc(obj->members, obj->cap * sizeof(JMember));
    }
    obj->members[obj->count].key = key;
    obj->members[obj->count].val = val;
    obj->count++;
}

static void jv_free(JValue *v) {
    size_t i;
    if (!v) return;
    switch (v->type) {
        case J_STRING: free(v->s); break;
        case J_ARRAY:
            for (i = 0; i < v->count; i++) jv_free(v->items[i]);
            free(v->items);
            break;
        case J_OBJECT:
            for (i = 0; i < v->count; i++) { free(v->members[i].key); jv_free(v->members[i].val); }
            free(v->members);
            break;
        default: break;
    }
    free(v);
}

static const JValue *jv_get(const JValue *o, const char *k) {
    size_t i;
    if (!o || o->type != J_OBJECT) return NULL;
    for (i = 0; i < o->count; i++)
        if (strcmp(o->members[i].key, k) == 0) return o->members[i].val;
    return NULL;
}

static const char *jv_str(const JValue *v) {
    return (v && v->type == J_STRING) ? v->s : NULL;
}

static long long jv_int_or(const JValue *v, long long fallback) {
    return (v && v->type == J_INT) ? v->i : fallback;
}

typedef struct { const char *s; size_t n; size_t pos; int err; } Parser;

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

static char *parse_string(Parser *p) {
    StrBuf b;
    if (p->pos >= p->n || p->s[p->pos] != '"') { p->err = 1; return NULL; }
    p->pos++;
    sb_init(&b);
    while (p->pos < p->n && p->s[p->pos] != '"') {
        unsigned char c = (unsigned char)p->s[p->pos++];
        if (c == '\\') {
            char e;
            if (p->pos >= p->n) { p->err = 1; sb_free(&b); return NULL; }
            e = p->s[p->pos++];
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
                    unsigned cp = 0;
                    int k;
                    if (p->pos + 4 > p->n) { p->err = 1; sb_free(&b); return NULL; }
                    for (k = 0; k < 4; k++) {
                        int h = hexval(p->s[p->pos + k]);
                        if (h < 0) { p->err = 1; sb_free(&b); return NULL; }
                        cp = cp * 16 + (unsigned)h;
                    }
                    p->pos += 4;
                    if (cp >= 0xD800 && cp <= 0xDBFF && p->pos + 1 < p->n &&
                        p->s[p->pos] == '\\' && p->s[p->pos + 1] == 'u') {
                        size_t save = p->pos;
                        unsigned lo = 0;
                        int lok = 1;
                        p->pos += 2;
                        for (k = 0; k < 4; k++) {
                            int h = hexval(p->s[p->pos + k]);
                            if (h < 0) { lok = 0; break; }
                            lo = lo * 16 + (unsigned)h;
                        }
                        if (lok && lo >= 0xDC00 && lo <= 0xDFFF) {
                            p->pos += 4;
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        } else {
                            p->pos = save;
                            cp = 0xFFFD;
                        }
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        cp = 0xFFFD;
                    }
                    sb_append_utf8(&b, cp);
                    break;
                }
                default: p->err = 1; sb_free(&b); return NULL;
            }
        } else {
            sb_append_char(&b, (char)c);
        }
    }
    if (p->pos >= p->n) { p->err = 1; sb_free(&b); return NULL; }
    p->pos++;
    return sb_detach(&b);
}

static JValue *parse_value(Parser *p);

static JValue *parse_literal(Parser *p, const char *lit, JType t, long long i, int b) {
    size_t l = strlen(lit);
    if (p->pos + l > p->n || strncmp(p->s + p->pos, lit, l) != 0) {
        p->err = 1;
        return NULL;
    }
    p->pos += l;
    {
        JValue *v = jv_new(t);
        if (t == J_INT) v->i = i;
        if (t == J_BOOL) v->b = b;
        return v;
    }
}

static JValue *parse_number(Parser *p) {
    size_t start = p->pos;
    char *tmp, *end = NULL;
    long long v;
    int ok;
    JValue *j;
    if (p->s[p->pos] == '-') p->pos++;
    if (p->pos >= p->n || !isdigit((unsigned char)p->s[p->pos])) { p->err = 1; return NULL; }
    while (p->pos < p->n && isdigit((unsigned char)p->s[p->pos])) p->pos++;
    if (p->pos < p->n && (p->s[p->pos] == '.' || p->s[p->pos] == 'e' || p->s[p->pos] == 'E')) {
        p->err = 1;
        return NULL;
    }
    tmp = xstrndup(p->s + start, p->pos - start);
    v = strtoll(tmp, &end, 10);
    ok = (end != NULL && *end == 0);
    free(tmp);
    if (!ok) { p->err = 1; return NULL; }
    j = jv_new(J_INT);
    j->i = v;
    return j;
}

static JValue *parse_object(Parser *p) {
    JValue *obj = jv_new(J_OBJECT);
    p->pos++;
    skip_ws(p);
    if (p->pos < p->n && p->s[p->pos] == '}') { p->pos++; return obj; }
    for (;;) {
        char *key;
        JValue *val;
        char c;
        skip_ws(p);
        if (p->pos >= p->n || p->s[p->pos] != '"') { p->err = 1; jv_free(obj); return NULL; }
        key = parse_string(p);
        if (!key) { jv_free(obj); return NULL; }
        skip_ws(p);
        if (p->pos >= p->n || p->s[p->pos] != ':') { p->err = 1; free(key); jv_free(obj); return NULL; }
        p->pos++;
        val = parse_value(p);
        if (!val) { free(key); jv_free(obj); return NULL; }
        jv_obj_add(obj, key, val);
        skip_ws(p);
        if (p->pos >= p->n) { p->err = 1; jv_free(obj); return NULL; }
        c = p->s[p->pos];
        if (c == ',') { p->pos++; continue; }
        if (c == '}') { p->pos++; return obj; }
        p->err = 1;
        jv_free(obj);
        return NULL;
    }
}

static JValue *parse_array(Parser *p) {
    JValue *arr = jv_new(J_ARRAY);
    p->pos++;
    skip_ws(p);
    if (p->pos < p->n && p->s[p->pos] == ']') { p->pos++; return arr; }
    for (;;) {
        JValue *val = parse_value(p);
        char c;
        if (!val) { jv_free(arr); return NULL; }
        jv_array_add(arr, val);
        skip_ws(p);
        if (p->pos >= p->n) { p->err = 1; jv_free(arr); return NULL; }
        c = p->s[p->pos];
        if (c == ',') { p->pos++; continue; }
        if (c == ']') { p->pos++; return arr; }
        p->err = 1;
        jv_free(arr);
        return NULL;
    }
}

static JValue *parse_value(Parser *p) {
    char c;
    skip_ws(p);
    if (p->pos >= p->n) { p->err = 1; return NULL; }
    c = p->s[p->pos];
    if (c == '{') return parse_object(p);
    if (c == '[') return parse_array(p);
    if (c == '"') {
        char *s = parse_string(p);
        JValue *v;
        if (!s) return NULL;
        v = jv_new(J_STRING);
        v->s = s;
        return v;
    }
    if (c == 't') return parse_literal(p, "true", J_BOOL, 0, 1);
    if (c == 'f') return parse_literal(p, "false", J_BOOL, 0, 0);
    if (c == 'n') return parse_literal(p, "null", J_NULL, 0, 0);
    if (c == '-' || (c >= '0' && c <= '9')) return parse_number(p);
    p->err = 1;
    return NULL;
}

static JValue *json_parse(const char *text) {
    Parser p;
    JValue *v;
    p.s = text;
    p.n = strlen(text);
    p.pos = 0;
    p.err = 0;
    v = parse_value(&p);
    if (p.err) { jv_free(v); return NULL; }
    skip_ws(&p);
    if (p.pos != p.n) { jv_free(v); return NULL; }
    return v;
}

/* ------------------------------------------------------------------ */
/* 文件读取 / 目录扫描                                                  */
/* ------------------------------------------------------------------ */

static char *read_file(const char *path, char *err, size_t errsz) {
    FILE *f = fopen(path, "rb");
    long sz;
    char *buf;
    size_t got;
    if (!f) { if (err) snprintf(err, errsz, "cannot open: %s", path); return NULL; }
    if (fseek(f, 0, SEEK_END) != 0 || (sz = ftell(f)) < 0) { if (err) snprintf(err, errsz, "cannot seek: %s", path); fclose(f); return NULL; }
    rewind(f);
    buf = xmalloc((size_t)sz + 1);
    got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[got] = 0;
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
    return _stat(path, &st) == 0 && (st.st_mode & _S_IFDIR);
#else
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

typedef struct { char **items; size_t count, cap; } FileList;

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
    struct _finddata_t info;
    intptr_t h;
    snprintf(pattern, sizeof(pattern), "%s\\*", dir);
    h = _findfirst(pattern, &info);
    if (h == -1) return;
    do {
        char full[PATHBUF];
        if (strcmp(info.name, ".") == 0 || strcmp(info.name, "..") == 0) continue;
        snprintf(full, sizeof(full), "%s\\%s", dir, info.name);
        if (info.attrib & _A_SUBDIR) scan_dir(full, fl);
        else if (ends_with_ci(full, ".json")) fl_add(fl, full);
    } while (_findnext(h, &info) == 0);
    _findclose(h);
}
#else
#include <dirent.h>
static void scan_dir(const char *dir, FileList *fl) {
    DIR *d = opendir(dir);
    struct dirent *e;
    if (!d) return;
    while ((e = readdir(d)) != NULL) {
        char full[PATHBUF];
        struct stat st;
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        snprintf(full, sizeof(full), "%s/%s", dir, e->d_name);
        if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) scan_dir(full, fl);
        else if (ends_with_ci(full, ".json")) fl_add(fl, full);
    }
    closedir(d);
}
#endif

/* ------------------------------------------------------------------ */
/* 进程交互：写 stdin，读 stdout                                        */
/* ------------------------------------------------------------------ */

static int run_interactive(const char *program, const char *input, size_t input_len,
                           unsigned timeout_ms, char **out_text, char *err, size_t errsz) {
#ifdef _WIN32
    HANDLE in_read = INVALID_HANDLE_VALUE, in_write = INVALID_HANDLE_VALUE;
    HANDLE out_read = INVALID_HANDLE_VALUE, out_write = INVALID_HANDLE_VALUE;
    SECURITY_ATTRIBUTES sa;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    StrBuf sb;
    char buf[4096];
    DWORD n, written, exit_code = 0;

    memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    if (!CreatePipe(&in_read, &in_write, &sa, 0)) { snprintf(err, errsz, "CreatePipe failed"); return -1; }
    if (!CreatePipe(&out_read, &out_write, &sa, 0)) {
        CloseHandle(in_read); CloseHandle(in_write);
        snprintf(err, errsz, "CreatePipe failed");
        return -1;
    }
    SetHandleInformation(in_write, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(out_read, HANDLE_FLAG_INHERIT, 0);

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = in_read;
    si.hStdOutput = out_write;
    si.hStdError = out_write;

    memset(&pi, 0, sizeof(pi));
    if (!CreateProcessA(program, NULL, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(in_read); CloseHandle(in_write);
        CloseHandle(out_read); CloseHandle(out_write);
        snprintf(err, errsz, "cannot start program: %s", program);
        return -1;
    }

    CloseHandle(in_read);
    CloseHandle(out_write);

    {
        const char *p = input;
        DWORD remaining = (DWORD)input_len;
        while (remaining > 0) {
            DWORD chunk = remaining > 4096 ? 4096 : remaining;
            if (!WriteFile(in_write, p, chunk, &written, NULL) || written == 0) break;
            p += written;
            remaining -= written;
        }
    }
    CloseHandle(in_write);

    sb_init(&sb);
    {
        ULONGLONG started = GetTickCount64();
        int finished = 0;
        while (!finished) {
            DWORD available = 0;
            while (PeekNamedPipe(out_read, NULL, 0, NULL, &available, NULL) && available > 0) {
                DWORD want = available > sizeof(buf) ? (DWORD)sizeof(buf) : available;
                if (!ReadFile(out_read, buf, want, &n, NULL) || n == 0) break;
                sb_append_bytes(&sb, buf, n);
                available = 0;
            }
            if (WaitForSingleObject(pi.hProcess, 0) == WAIT_OBJECT_0) {
                while (PeekNamedPipe(out_read, NULL, 0, NULL, &available, NULL) && available > 0) {
                    DWORD want = available > sizeof(buf) ? (DWORD)sizeof(buf) : available;
                    if (!ReadFile(out_read, buf, want, &n, NULL) || n == 0) break;
                    sb_append_bytes(&sb, buf, n);
                    available = 0;
                }
                finished = 1;
            } else if (GetTickCount64() - started >= timeout_ms) {
                TerminateProcess(pi.hProcess, 124);
                WaitForSingleObject(pi.hProcess, 5000);
                snprintf(err, errsz, "process timed out after %u ms", timeout_ms);
                CloseHandle(out_read);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                *out_text = sb_detach(&sb);
                return -2;
            } else {
                Sleep(5);
            }
        }
    }
    CloseHandle(out_read);
    if (!GetExitCodeProcess(pi.hProcess, &exit_code)) exit_code = 0;
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    *out_text = sb_detach(&sb);
    return (int)exit_code;
#else
    int in_pipe[2], out_pipe[2];
    pid_t pid;
    StrBuf sb;
    char buf[4096];
    ssize_t n;
    int status = 0;

    if (pipe(in_pipe) != 0) { snprintf(err, errsz, "pipe failed"); return -1; }
    if (pipe(out_pipe) != 0) { close(in_pipe[0]); close(in_pipe[1]); snprintf(err, errsz, "pipe failed"); return -1; }

    pid = fork();
    if (pid < 0) { snprintf(err, errsz, "fork failed"); close(in_pipe[0]); close(in_pipe[1]); close(out_pipe[0]); close(out_pipe[1]); return -1; }
    if (pid == 0) {
        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(out_pipe[1], STDERR_FILENO);
        close(in_pipe[0]); close(in_pipe[1]);
        close(out_pipe[0]); close(out_pipe[1]);
        execlp(program, program, (char *)NULL);
        _exit(127);
    }

    close(in_pipe[0]);
    close(out_pipe[1]);

    {
        const char *p = input;
        size_t remaining = input_len;
        while (remaining > 0) {
            ssize_t w = write(in_pipe[1], p, remaining);
            if (w <= 0) break;
            p += w;
            remaining -= (size_t)w;
        }
    }
    close(in_pipe[1]);

    sb_init(&sb);
    while ((n = read(out_pipe[0], buf, sizeof(buf))) > 0) {
        sb_append_bytes(&sb, buf, (size_t)n);
    }
    close(out_pipe[0]);

    waitpid(pid, &status, 0);
    *out_text = sb_detach(&sb);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

/* 同一用例的多个实例必须先全部启动，再分别收集输出，避免把并发隔离案例降级为顺序重跑。 */
static int run_interactive_instances(const char *program, char *const *inputs, const size_t *input_lens,
                                     unsigned timeout_ms, size_t instance_count, char ***out_texts,
                                     char *err, size_t errsz) {
    size_t i;
    char **outputs;
    int combined_exit = 0;

    if (instance_count == 0 || instance_count > 16) {
        snprintf(err, errsz, "instances must be 1..16");
        return -1;
    }
    outputs = xmalloc(instance_count * sizeof(char *));
    memset(outputs, 0, instance_count * sizeof(char *));

    if (instance_count == 1) {
        int code = run_interactive(program, inputs[0], input_lens[0], timeout_ms, &outputs[0], err, errsz);
        if (!outputs[0]) { free(outputs); return -1; }
        *out_texts = outputs;
        return code;
    }

#ifdef _WIN32
    {
        typedef struct {
            HANDLE in_read, in_write, out_read, out_write;
            PROCESS_INFORMATION pi;
            int started;
            int finished;
            StrBuf output;
        } Child;
        Child *children = xmalloc(instance_count * sizeof(Child));
        SECURITY_ATTRIBUTES sa;
        memset(children, 0, instance_count * sizeof(Child));
        memset(&sa, 0, sizeof(sa));
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        for (i = 0; i < instance_count; i++) {
            STARTUPINFOA si;
            DWORD written;
            const char *p = inputs[i];
            DWORD remaining = (DWORD)input_lens[i];
            Child *c = &children[i];
            c->in_read = c->in_write = c->out_read = c->out_write = INVALID_HANDLE_VALUE;
            sb_init(&c->output);
            memset(&c->pi, 0, sizeof(c->pi));
            if (!CreatePipe(&c->in_read, &c->in_write, &sa, 0) ||
                !CreatePipe(&c->out_read, &c->out_write, &sa, 0)) {
                snprintf(err, errsz, "CreatePipe failed for instance %u", (unsigned)(i + 1));
                goto win_fail;
            }
            SetHandleInformation(c->in_write, HANDLE_FLAG_INHERIT, 0);
            SetHandleInformation(c->out_read, HANDLE_FLAG_INHERIT, 0);
            memset(&si, 0, sizeof(si));
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESTDHANDLES;
            si.hStdInput = c->in_read;
            si.hStdOutput = c->out_write;
            si.hStdError = c->out_write;
            if (!CreateProcessA(program, NULL, NULL, NULL, TRUE, 0, NULL, NULL, &si, &c->pi)) {
                snprintf(err, errsz, "cannot start instance %u: %s", (unsigned)(i + 1), program);
                goto win_fail;
            }
            c->started = 1;
            CloseHandle(c->in_read); c->in_read = INVALID_HANDLE_VALUE;
            CloseHandle(c->out_write); c->out_write = INVALID_HANDLE_VALUE;
            while (remaining > 0) {
                DWORD chunk = remaining > 4096 ? 4096 : remaining;
                if (!WriteFile(c->in_write, p, chunk, &written, NULL) || written == 0) break;
                p += written;
                remaining -= written;
            }
            CloseHandle(c->in_write); c->in_write = INVALID_HANDLE_VALUE;
        }
        {
            ULONGLONG started = GetTickCount64();
            size_t finished_count = 0;
            while (finished_count < instance_count) {
                for (i = 0; i < instance_count; i++) {
                    Child *c = &children[i];
                    char buf[4096];
                    DWORD n, available = 0, exit_code = 0;
                    if (c->finished) continue;
                    while (PeekNamedPipe(c->out_read, NULL, 0, NULL, &available, NULL) && available > 0) {
                        DWORD want = available > sizeof(buf) ? (DWORD)sizeof(buf) : available;
                        if (!ReadFile(c->out_read, buf, want, &n, NULL) || n == 0) break;
                        sb_append_bytes(&c->output, buf, n);
                        available = 0;
                    }
                    if (WaitForSingleObject(c->pi.hProcess, 0) == WAIT_OBJECT_0) {
                        while (PeekNamedPipe(c->out_read, NULL, 0, NULL, &available, NULL) && available > 0) {
                            DWORD want = available > sizeof(buf) ? (DWORD)sizeof(buf) : available;
                            if (!ReadFile(c->out_read, buf, want, &n, NULL) || n == 0) break;
                            sb_append_bytes(&c->output, buf, n);
                            available = 0;
                        }
                        if (!GetExitCodeProcess(c->pi.hProcess, &exit_code)) exit_code = 0;
                        if (exit_code != 0 && combined_exit == 0) combined_exit = (int)exit_code;
                        c->finished = 1;
                        finished_count++;
                    }
                }
                if (finished_count < instance_count && GetTickCount64() - started >= timeout_ms) {
                    for (i = 0; i < instance_count; i++) {
                        Child *c = &children[i];
                        if (!c->finished) {
                            TerminateProcess(c->pi.hProcess, 124);
                            WaitForSingleObject(c->pi.hProcess, 5000);
                            c->finished = 1;
                        }
                    }
                    snprintf(err, errsz, "one or more instances timed out after %u ms", timeout_ms);
                    combined_exit = -2;
                    break;
                }
                if (finished_count < instance_count) Sleep(5);
            }
        }
        for (i = 0; i < instance_count; i++) {
            Child *c = &children[i];
            if (c->out_read != INVALID_HANDLE_VALUE) CloseHandle(c->out_read);
            CloseHandle(c->pi.hProcess);
            CloseHandle(c->pi.hThread);
            c->out_read = INVALID_HANDLE_VALUE;
            c->pi.hProcess = c->pi.hThread = NULL;
            outputs[i] = sb_detach(&c->output);
        }
        free(children);
        *out_texts = outputs;
        return combined_exit;

win_fail:
        for (i = 0; i < instance_count; i++) {
            Child *c = &children[i];
            if (c->started && c->pi.hProcess) {
                TerminateProcess(c->pi.hProcess, 1);
                WaitForSingleObject(c->pi.hProcess, 5000);
            }
            if (c->in_read != INVALID_HANDLE_VALUE) CloseHandle(c->in_read);
            if (c->in_write != INVALID_HANDLE_VALUE) CloseHandle(c->in_write);
            if (c->out_read != INVALID_HANDLE_VALUE) CloseHandle(c->out_read);
            if (c->out_write != INVALID_HANDLE_VALUE) CloseHandle(c->out_write);
            if (c->pi.hProcess) CloseHandle(c->pi.hProcess);
            if (c->pi.hThread) CloseHandle(c->pi.hThread);
            sb_free(&c->output);
        }
        free(children);
    }
#else
    {
        typedef struct { int in_write, out_read; pid_t pid; } Child;
        Child *children = xmalloc(instance_count * sizeof(Child));
        memset(children, 0, instance_count * sizeof(Child));
        for (i = 0; i < instance_count; i++) {
            int in_pipe[2], out_pipe[2];
            const char *p = inputs[i];
            size_t remaining = input_lens[i];
            if (pipe(in_pipe) != 0 || pipe(out_pipe) != 0) {
                snprintf(err, errsz, "pipe failed for instance %u", (unsigned)(i + 1));
                goto posix_fail;
            }
            children[i].pid = fork();
            if (children[i].pid < 0) {
                snprintf(err, errsz, "fork failed for instance %u", (unsigned)(i + 1));
                close(in_pipe[0]); close(in_pipe[1]); close(out_pipe[0]); close(out_pipe[1]);
                goto posix_fail;
            }
            if (children[i].pid == 0) {
                dup2(in_pipe[0], STDIN_FILENO);
                dup2(out_pipe[1], STDOUT_FILENO);
                dup2(out_pipe[1], STDERR_FILENO);
                close(in_pipe[0]); close(in_pipe[1]); close(out_pipe[0]); close(out_pipe[1]);
                execlp(program, program, (char *)NULL);
                _exit(127);
            }
            close(in_pipe[0]); close(out_pipe[1]);
            children[i].in_write = in_pipe[1];
            children[i].out_read = out_pipe[0];
            while (remaining > 0) {
                ssize_t w = write(children[i].in_write, p, remaining);
                if (w <= 0) break;
                p += w;
                remaining -= (size_t)w;
            }
            close(children[i].in_write);
            children[i].in_write = -1;
        }
        for (i = 0; i < instance_count; i++) {
            StrBuf sb;
            char buf[4096];
            ssize_t n;
            int status = 0;
            sb_init(&sb);
            while ((n = read(children[i].out_read, buf, sizeof(buf))) > 0)
                sb_append_bytes(&sb, buf, (size_t)n);
            close(children[i].out_read);
            children[i].out_read = -1;
            waitpid(children[i].pid, &status, 0);
            if ((!WIFEXITED(status) || WEXITSTATUS(status) != 0) && combined_exit == 0)
                combined_exit = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
            outputs[i] = sb_detach(&sb);
        }
        free(children);
        *out_texts = outputs;
        return combined_exit;

posix_fail:
        for (i = 0; i < instance_count; i++) {
            if (children[i].pid > 0) kill(children[i].pid, SIGTERM);
            if (children[i].in_write > 0) close(children[i].in_write);
            if (children[i].out_read > 0) close(children[i].out_read);
            if (children[i].pid > 0) waitpid(children[i].pid, NULL, 0);
        }
        free(children);
    }
#endif
    for (i = 0; i < instance_count; i++) free(outputs[i]);
    free(outputs);
    return -1;
}

/* ------------------------------------------------------------------ */
/* ANSI 剥离 + 结果模型                                                 */
/* ------------------------------------------------------------------ */

static char *strip_ansi(const char *text) {
    StrBuf out;
    const char *p = text;
    sb_init(&out);
    while (*p) {
        if (*p == '\x1b' && p[1] == '[') {
            p += 2;
            while (*p && !((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z'))) p++;
            if (*p) p++;
            continue;
        }
        if (*p == '\r') { p++; continue; }
        sb_append_char(&out, *p++);
    }
    return sb_detach(&out);
}

typedef struct {
    char *name;
    char result[8];
    char **errors;
    size_t error_count;
    char *stdout_tail;
} CaseResult;

typedef struct {
    CaseResult *items;
    size_t count, cap;
} CaseList;

static void cl_add(CaseList *cl, CaseResult r) {
    if (cl->count == cl->cap) {
        cl->cap = cl->cap ? cl->cap * 2 : 16;
        cl->items = xrealloc(cl->items, cl->cap * sizeof(CaseResult));
    }
    cl->items[cl->count++] = r;
}

static void cr_free(CaseResult *r) {
    size_t i;
    free(r->name);
    for (i = 0; i < r->error_count; i++) free(r->errors[i]);
    free(r->errors);
    free(r->stdout_tail);
    r->errors = NULL;
    r->stdout_tail = NULL;
}

static void add_error(CaseResult *r, const char *msg) {
    r->errors = xrealloc(r->errors, (r->error_count + 1) * sizeof(char *));
    r->errors[r->error_count++] = xstrdup(msg);
}

static void json_write_string(FILE *f, const char *s) {
    fputc('"', f);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
            case '"': fputs("\\\"", f); break;
            case '\\': fputs("\\\\", f); break;
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

/* ------------------------------------------------------------------ */
/* 命令行参数                                                          */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *program;
    const char *cases;
    const char *out;
    int quiet;
} Options;

static void usage(FILE *f, const char *prog) {
    fprintf(f,
        "usage: %s --program <monopoly.exe> --cases <dir|file> [--out results.json] [--quiet]\n",
        prog);
}

static int parse_args(int argc, char **argv, Options *o) {
    int i;
    memset(o, 0, sizeof(*o));
    o->out = "results_interactive.json";
    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) { usage(stdout, argv[0]); exit(0); }
        else if (strcmp(a, "--quiet") == 0) o->quiet = 1;
        else if (strcmp(a, "--program") == 0) { if (++i >= argc) { fprintf(stderr, "missing value for %s\n", a); return -1; } o->program = argv[i]; }
        else if (strcmp(a, "--cases") == 0) { if (++i >= argc) { fprintf(stderr, "missing value for %s\n", a); return -1; } o->cases = argv[i]; }
        else if (strcmp(a, "--out") == 0) { if (++i >= argc) { fprintf(stderr, "missing value for %s\n", a); return -1; } o->out = argv[i]; }
        else { fprintf(stderr, "unknown option: %s\n\n", a); usage(stderr, argv[0]); return -1; }
    }
    if (!o->program || !o->cases) { usage(stderr, argv[0]); return -1; }
    return 0;
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv) {
    Options o;
    FileList files;
    CaseList list;
    size_t fi;
    size_t total, pass = 0, fail = 0, errcnt = 0;

    memset(&files, 0, sizeof(files));
    memset(&list, 0, sizeof(list));

    if (parse_args(argc, argv, &o) != 0) return 2;

    if (path_is_dir(o.cases)) scan_dir(o.cases, &files);
    else if (file_exists(o.cases)) fl_add(&files, o.cases);
    else { fprintf(stderr, "cases path not found: %s\n", o.cases); return 2; }

    if (files.count == 0) { fprintf(stderr, "no .json cases found under: %s\n", o.cases); return 2; }

    for (fi = 0; fi < files.count; fi++) {
        const char *path = files.items[fi];
        char err[256] = "";
        char *text = read_file(path, err, sizeof(err));
        CaseResult r;
        JValue *casej = NULL;
        const char *name;

        memset(&r, 0, sizeof(r));
        snprintf(r.result, sizeof(r.result), "ERROR");

        if (!text) {
            r.name = xstrdup(path);
            add_error(&r, err);
            cl_add(&list, r);
            continue;
        }

        casej = json_parse(text);
        free(text);
        if (!casej) {
            r.name = xstrdup(path);
            add_error(&r, "case is not valid JSON");
            cl_add(&list, r);
            continue;
        }

        name = jv_str(jv_get(casej, "name"));
        r.name = xstrdup(name ? name : path);

        {
            const JValue *input = jv_get(casej, "input");
            const JValue *instance_inputs = jv_get(casej, "instance_inputs");
            const JValue *instance_expect = jv_get(casej, "instance_expect");
            const JValue *instance_forbid = jv_get(casej, "instance_forbid");
            const JValue *instances_value = jv_get(casej, "instances");
            const JValue *repetitions_value = jv_get(casej, "repetitions");
            const JValue *timeout_value = jv_get(casej, "timeout_ms");
            const JValue *exit_code_value = jv_get(casej, "exit_code");
            long long instances_ll = jv_int_or(instances_value, 1);
            long long repetitions_ll = jv_int_or(repetitions_value, 1);
            long long timeout_ll = jv_int_or(timeout_value, 5000);
            long long expected_exit_ll = jv_int_or(exit_code_value, 0);
            StrBuf inbuf;
            StrBuf fail_tails;
            char **run_inputs = NULL;
            size_t *run_input_lens = NULL;
            size_t i;
            size_t repetition;
            int ok = 1;
            int launch_error = 0;

            sb_init(&inbuf);
            sb_init(&fail_tails);
            if (input && input->type == J_ARRAY) {
                for (i = 0; i < input->count; i++) {
                    const char *line = jv_str(input->items[i]);
                    if (line) sb_append_str(&inbuf, line);
                    sb_append_char(&inbuf, '\n');
                }
            }

            if (instances_ll < 1 || instances_ll > 16 || repetitions_ll < 1 || repetitions_ll > 100 ||
                timeout_ll < 100 || timeout_ll > 300000 || expected_exit_ll < 0 || expected_exit_ll > 255) {
                add_error(&r, "instances must be 1..16, repetitions 1..100, timeout_ms 100..300000, exit_code 0..255");
                launch_error = 1;
            }

            if (!launch_error) {
                size_t instance_count = (size_t)instances_ll;
                run_inputs = xmalloc(instance_count * sizeof(char *));
                run_input_lens = xmalloc(instance_count * sizeof(size_t));
                for (i = 0; i < instance_count; i++) {
                    run_inputs[i] = xstrdup(inbuf.data ? inbuf.data : "");
                    run_input_lens[i] = inbuf.len;
                }
                if (instance_inputs) {
                    if (instance_inputs->type != J_ARRAY || instance_inputs->count != instance_count) {
                        add_error(&r, "instance_inputs must contain one input array per instance");
                        launch_error = 1;
                    } else {
                        for (i = 0; i < instance_count; i++) {
                            const JValue *lines = instance_inputs->items[i];
                            StrBuf per_instance;
                            size_t line_index;
                            if (!lines || lines->type != J_ARRAY) {
                                add_error(&r, "each instance_inputs entry must be an array of strings");
                                launch_error = 1;
                                break;
                            }
                            sb_init(&per_instance);
                            for (line_index = 0; line_index < lines->count; line_index++) {
                                const char *line = jv_str(lines->items[line_index]);
                                if (!line) {
                                    add_error(&r, "each instance_inputs value must be a string");
                                    launch_error = 1;
                                    break;
                                }
                                sb_append_str(&per_instance, line);
                                sb_append_char(&per_instance, '\n');
                            }
                            if (!launch_error) {
                                free(run_inputs[i]);
                                run_inputs[i] = sb_detach(&per_instance);
                                run_input_lens[i] = strlen(run_inputs[i]);
                            }
                            sb_free(&per_instance);
                            if (launch_error) break;
                        }
                    }
                }
                if (!launch_error && instance_expect &&
                    (instance_expect->type != J_ARRAY || instance_expect->count != instance_count)) {
                    add_error(&r, "instance_expect must contain one matcher array per instance");
                    launch_error = 1;
                }
                if (!launch_error && instance_forbid &&
                    (instance_forbid->type != J_ARRAY || instance_forbid->count != instance_count)) {
                    add_error(&r, "instance_forbid must contain one matcher array per instance");
                    launch_error = 1;
                }
                if (!launch_error && instance_expect) {
                    for (i = 0; i < instance_count; i++) {
                        if (!instance_expect->items[i] || instance_expect->items[i]->type != J_ARRAY) {
                            add_error(&r, "each instance_expect entry must be an array of strings");
                            launch_error = 1;
                            break;
                        }
                    }
                }
                if (!launch_error && instance_forbid) {
                    for (i = 0; i < instance_count; i++) {
                        if (!instance_forbid->items[i] || instance_forbid->items[i]->type != J_ARRAY) {
                            add_error(&r, "each instance_forbid entry must be an array of strings");
                            launch_error = 1;
                            break;
                        }
                    }
                }
            }

            for (repetition = 0; !launch_error && repetition < (size_t)repetitions_ll; repetition++) {
                char **out_texts = NULL;
                const JValue *expect = jv_get(casej, "expect");
                const JValue *forbid = jv_get(casej, "forbid");
                size_t instance;
                int run_code = run_interactive_instances(
                    o.program, run_inputs, run_input_lens,
                    (unsigned)timeout_ll, (size_t)instances_ll, &out_texts, err, sizeof(err));
                if (!out_texts) {
                    char msg[512];
                    snprintf(msg, sizeof(msg), "repetition %u launch failed: %s",
                             (unsigned)(repetition + 1), err);
                    add_error(&r, msg);
                    launch_error = 1;
                    break;
                }
                if (run_code == -2) {
                    char msg[512];
                    snprintf(msg, sizeof(msg), "repetition %u timeout: %s",
                             (unsigned)(repetition + 1), err);
                    add_error(&r, msg);
                    launch_error = 1;
                } else if (run_code != (int)expected_exit_ll) {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "repetition %u exit code %d, expected %d",
                             (unsigned)(repetition + 1), run_code, (int)expected_exit_ll);
                    add_error(&r, msg);
                    ok = 0;
                }
                for (instance = 0; instance < (size_t)instances_ll; instance++) {
                    char *clean = strip_ansi(out_texts[instance]);
                    const JValue *effective_expect = instance_expect ? instance_expect->items[instance] : expect;
                    const JValue *effective_forbid = instance_forbid ? instance_forbid->items[instance] : forbid;
                    int instance_ok = 1;
                    if (effective_expect && effective_expect->type == J_ARRAY) {
                        for (i = 0; i < effective_expect->count; i++) {
                            const char *s = jv_str(effective_expect->items[i]);
                            if (s && strstr(clean, s) == NULL) {
                                char msg[640];
                                snprintf(msg, sizeof(msg), "repetition %u instance %u missing substring: %s",
                                         (unsigned)(repetition + 1), (unsigned)(instance + 1), s);
                                add_error(&r, msg);
                                instance_ok = 0;
                            }
                        }
                    }
                    if (effective_forbid && effective_forbid->type == J_ARRAY) {
                        for (i = 0; i < effective_forbid->count; i++) {
                            const char *s = jv_str(effective_forbid->items[i]);
                            if (s && strstr(clean, s) != NULL) {
                                char msg[640];
                                snprintf(msg, sizeof(msg), "repetition %u instance %u forbidden substring present: %s",
                                         (unsigned)(repetition + 1), (unsigned)(instance + 1), s);
                                add_error(&r, msg);
                                instance_ok = 0;
                            }
                        }
                    }
                    if (!instance_ok) {
                        char label[96];
                        size_t len = strlen(clean);
                        size_t from = len > 1200 ? len - 1200 : 0;
                        snprintf(label, sizeof(label), "\n--- repetition %u instance %u ---\n",
                                 (unsigned)(repetition + 1), (unsigned)(instance + 1));
                        sb_append_str(&fail_tails, label);
                        sb_append_str(&fail_tails, clean + from);
                        ok = 0;
                    }
                    free(clean);
                    free(out_texts[instance]);
                }
                free(out_texts);
                if (launch_error) break;
            }
            if (run_inputs) {
                for (i = 0; i < (size_t)instances_ll; i++) free(run_inputs[i]);
            }
            free(run_inputs);
            free(run_input_lens);
            sb_free(&inbuf);

            if (launch_error) {
                snprintf(r.result, sizeof(r.result), "ERROR");
            } else if (ok) {
                snprintf(r.result, sizeof(r.result), "PASS");
            } else {
                snprintf(r.result, sizeof(r.result), "FAIL");
                r.stdout_tail = sb_detach(&fail_tails);
            }
            sb_free(&fail_tails);
        }

        jv_free(casej);
        cl_add(&list, r);
    }

    total = list.count;
    for (fi = 0; fi < list.count; fi++) {
        const char *res = list.items[fi].result;
        if (!o.quiet) {
            const char *mark = strcmp(res, "PASS") == 0 ? "PASS" : (strcmp(res, "FAIL") == 0 ? "FAIL" : "ERR ");
            size_t i;
            printf("[%s] %s\n", mark, list.items[fi].name);
            for (i = 0; i < list.items[fi].error_count; i++)
                printf("       - %s\n", list.items[fi].errors[i]);
        }
        if (strcmp(res, "PASS") == 0) pass++;
        else if (strcmp(res, "FAIL") == 0) fail++;
        else errcnt++;
    }

    printf("\ntotal %zu | PASS %zu | FAIL %zu | ERROR %zu\n", total, pass, fail, errcnt);

    {
        FILE *of = fopen(o.out, "wb");
        if (of) {
            fprintf(of, "{\n  \"summary\": {\"total\": %zu, \"pass\": %zu, \"fail\": %zu, \"error\": %zu},\n  \"cases\": [\n",
                    total, pass, fail, errcnt);
            for (fi = 0; fi < list.count; fi++) {
                size_t i;
                fprintf(of, "    {\"name\": ");
                json_write_string(of, list.items[fi].name);
                fprintf(of, ", \"result\": ");
                json_write_string(of, list.items[fi].result);
                if (list.items[fi].error_count) {
                    fprintf(of, ", \"errors\": [");
                    for (i = 0; i < list.items[fi].error_count; i++) {
                        if (i) fprintf(of, ", ");
                        json_write_string(of, list.items[fi].errors[i]);
                    }
                    fprintf(of, "]");
                }
                if (list.items[fi].stdout_tail) {
                    fprintf(of, ", \"stdout_tail\": ");
                    json_write_string(of, list.items[fi].stdout_tail);
                }
                fprintf(of, "}%s\n", (fi + 1 < list.count) ? "," : "");
            }
            fprintf(of, "  ]\n}\n");
            fclose(of);
            printf("results written to: %s\n", o.out);
        } else {
            fprintf(stderr, "cannot write results to: %s\n", o.out);
        }
    }

    for (fi = 0; fi < list.count; fi++) cr_free(&list.items[fi]);
    free(list.items);
    for (fi = 0; fi < files.count; fi++) free(files.items[fi]);
    free(files.items);

    return (fail == 0 && errcnt == 0) ? 0 : 1;
}
