/*
 * 自动化测试黑盒入口（新增，不改动交互版 main.c 与游戏逻辑）。
 *
 * 调用契约：<本程序> <用例JSON> <地图JSON>
 *  - 读取用例 JSON，按 preset 初始化，按 actions 执行；
 *  - 成功时向 stdout 输出完整 Actual 状态 JSON；
 *  - 失败时向 stdout 输出 {"result":"ERROR","errors":[...]}。
 *
 * 地图内容以内置 game_map_init 为准（与 spec/map.json 一致），故 argv[2] 仅接受、不解析。
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "map/map.h"
#include "monopoly/automation.h"
#include "monopoly/runtime.h"

/* ------------------------------------------------------------------ */
/* 极简 JSON 值模型与解析（仅用于读取测试用例）                          */
/* ------------------------------------------------------------------ */

typedef enum { J_NULL, J_BOOL, J_INT, J_STR, J_ARR, J_OBJ } JType;

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

static void *xmalloc(size_t n) {
    void *p = malloc(n ? n : 1);
    if (p == NULL) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    return p;
}

static void *xrealloc(void *p, size_t n) {
    void *q = realloc(p, n ? n : 1);
    if (q == NULL) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    return q;
}

static char *xstrndup(const char *s, size_t n) {
    char *p = xmalloc(n + 1);
    memcpy(p, s, n);
    p[n] = 0;
    return p;
}

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
    if (v == NULL) return;
    switch (v->type) {
        case J_STR: free(v->s); break;
        case J_ARR:
            for (i = 0; i < v->count; ++i) jv_free(v->items[i]);
            free(v->items);
            break;
        case J_OBJ:
            for (i = 0; i < v->count; ++i) {
                free(v->members[i].key);
                jv_free(v->members[i].val);
            }
            free(v->members);
            break;
        default: break;
    }
    free(v);
}

static const JValue *jv_get(const JValue *o, const char *k) {
    size_t i;
    if (o == NULL || o->type != J_OBJ) return NULL;
    for (i = 0; i < o->count; ++i) {
        if (strcmp(o->members[i].key, k) == 0) return o->members[i].val;
    }
    return NULL;
}

static const char *jv_str(const JValue *v) {
    return (v != NULL && v->type == J_STR) ? v->s : NULL;
}

static long long jv_int(const JValue *v, long long def) {
    return (v != NULL && v->type == J_INT) ? v->i : def;
}

typedef struct {
    const char *s;
    size_t n;
    size_t pos;
    int err;
} Parser;

static void skip_ws(Parser *p) {
    while (p->pos < p->n && (p->s[p->pos] == ' ' || p->s[p->pos] == '\t' ||
           p->s[p->pos] == '\n' || p->s[p->pos] == '\r')) {
        p->pos++;
    }
}

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void sb_utf8(char **buf, size_t *len, size_t *cap, unsigned cp) {
    if (*len + 4 + 1 >= *cap) {
        *cap = (*cap ? *cap : 32) * 2;
        while (*len + 4 + 1 >= *cap) *cap *= 2;
        *buf = xrealloc(*buf, *cap);
    }
    if (cp < 0x80) {
        (*buf)[(*len)++] = (char)cp;
    } else if (cp < 0x800) {
        (*buf)[(*len)++] = (char)(0xC0 | (cp >> 6));
        (*buf)[(*len)++] = (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        (*buf)[(*len)++] = (char)(0xE0 | (cp >> 12));
        (*buf)[(*len)++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        (*buf)[(*len)++] = (char)(0x80 | (cp & 0x3F));
    } else {
        (*buf)[(*len)++] = (char)(0xF0 | (cp >> 18));
        (*buf)[(*len)++] = (char)(0x80 | ((cp >> 12) & 0x3F));
        (*buf)[(*len)++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        (*buf)[(*len)++] = (char)(0x80 | (cp & 0x3F));
    }
}

static char *parse_string(Parser *p) {
    char *buf;
    size_t len = 0, cap = 32;
    if (p->pos >= p->n || p->s[p->pos] != '"') { p->err = 1; return NULL; }
    p->pos++;
    buf = xmalloc(cap);
    while (p->pos < p->n && p->s[p->pos] != '"') {
        unsigned char c = (unsigned char)p->s[p->pos++];
        if (c == '\\') {
            char e;
            char ch;
            if (p->pos >= p->n) { p->err = 1; free(buf); return NULL; }
            e = p->s[p->pos++];
            switch (e) {
                case '"': ch = '"'; break;
                case '\\': ch = '\\'; break;
                case '/': ch = '/'; break;
                case 'b': ch = '\b'; break;
                case 'f': ch = '\f'; break;
                case 'n': ch = '\n'; break;
                case 'r': ch = '\r'; break;
                case 't': ch = '\t'; break;
                case 'u': {
                    unsigned cp = 0;
                    int k, ok = 1;
                    if (p->pos + 4 > p->n) { p->err = 1; free(buf); return NULL; }
                    for (k = 0; k < 4; ++k) {
                        int h = hexval(p->s[p->pos + k]);
                        if (h < 0) { ok = 0; break; }
                        cp = cp * 16 + (unsigned)h;
                    }
                    if (!ok) { p->err = 1; free(buf); return NULL; }
                    p->pos += 4;
                    if (cp >= 0xD800 && cp <= 0xDBFF && p->pos + 1 < p->n &&
                        p->s[p->pos] == '\\' && p->s[p->pos + 1] == 'u') {
                        size_t save = p->pos;
                        unsigned lo = 0;
                        int lok = 1;
                        p->pos += 2;
                        for (k = 0; k < 4; ++k) {
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
                    }
                    sb_utf8(&buf, &len, &cap, cp);
                    continue;
                }
                default: p->err = 1; free(buf); return NULL;
            }
            if (len + 1 >= cap) { cap *= 2; buf = xrealloc(buf, cap); }
            buf[len++] = ch;
        } else {
            if (len + 1 >= cap) { cap *= 2; buf = xrealloc(buf, cap); }
            buf[len++] = (char)c;
        }
    }
    if (p->pos >= p->n) { p->err = 1; free(buf); return NULL; }
    p->pos++;
    buf[len] = 0;
    return buf;
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
    char *tmp;
    char *end = NULL;
    long long v;
    int end_ok;
    JValue *j;
    if (p->s[p->pos] == '-') p->pos++;
    if (p->pos >= p->n || !isdigit((unsigned char)p->s[p->pos])) {
        p->err = 1;
        return NULL;
    }
    while (p->pos < p->n && isdigit((unsigned char)p->s[p->pos])) p->pos++;
    if (p->pos < p->n && (p->s[p->pos] == '.' || p->s[p->pos] == 'e' ||
        p->s[p->pos] == 'E')) {
        p->err = 1;
        return NULL;
    }
    tmp = xstrndup(p->s + start, p->pos - start);
    v = strtoll(tmp, &end, 10);
    end_ok = (end != NULL && *end == 0);
    free(tmp);
    if (!end_ok) { p->err = 1; return NULL; }
    j = jv_new(J_INT);
    j->i = v;
    return j;
}

static JValue *parse_object(Parser *p) {
    JValue *obj = jv_new(J_OBJ);
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
        if (key == NULL) { jv_free(obj); return NULL; }
        skip_ws(p);
        if (p->pos >= p->n || p->s[p->pos] != ':') { p->err = 1; free(key); jv_free(obj); return NULL; }
        p->pos++;
        val = parse_value(p);
        if (val == NULL) { free(key); jv_free(obj); return NULL; }
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
    JValue *arr = jv_new(J_ARR);
    p->pos++;
    skip_ws(p);
    if (p->pos < p->n && p->s[p->pos] == ']') { p->pos++; return arr; }
    for (;;) {
        JValue *val = parse_value(p);
        char c;
        if (val == NULL) { jv_free(arr); return NULL; }
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
        if (s == NULL) return NULL;
        v = jv_new(J_STR);
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
/* 预设构造与动作执行                                                   */
/* ------------------------------------------------------------------ */

static void seterr(char *err, size_t n, const char *msg) {
    if (err != NULL && n > 0) snprintf(err, n, "%s", msg);
}

static int find_symbol(const char *const syms[4], int n, const char *s) {
    int i;
    if (s == NULL) return -1;
    for (i = 0; i < n; ++i) {
        if (syms[i] != NULL && strcmp(syms[i], s) == 0) return i;
    }
    return -1;
}

static int parse_status(const char *s) {
    if (s == NULL || strcmp(s, "NORMAL") == 0) return AUTOMATION_STATUS_NORMAL;
    if (strcmp(s, "HOSPITAL") == 0) return AUTOMATION_STATUS_HOSPITAL;
    if (strcmp(s, "JAIL") == 0) return AUTOMATION_STATUS_JAIL;
    if (strcmp(s, "BANKRUPT") == 0) return AUTOMATION_STATUS_BANKRUPT;
    return -1;
}

static int parse_item_type(const char *s) {
    if (s == NULL) return -1;
    if (strcmp(s, "BLOCK") == 0) return AUTOMATION_ITEM_BLOCK;
    if (strcmp(s, "BOMB") == 0) return AUTOMATION_ITEM_BOMB;
    return -1;
}

static int build_preset(const JValue *case_obj, AutomationPreset *p, char *err, size_t errsz) {
    const JValue *preset = jv_get(case_obj, "preset");
    const JValue *users;
    const JValue *players;
    const JValue *props;
    const JValue *mis;
    const char *syms[4] = {NULL, NULL, NULL, NULL};
    int i;

    memset(p, 0, sizeof(*p));
    p->current_user_index = -1;

    if (preset == NULL || preset->type != J_OBJ) { seterr(err, errsz, "missing preset"); return -1; }
    users = jv_get(preset, "users");
    if (users == NULL || users->type != J_ARR || users->count < 2 || users->count > 4) {
        seterr(err, errsz, "invalid users");
        return -1;
    }
    p->player_count = (int)users->count;
    for (i = 0; i < p->player_count; ++i) {
        const char *s = jv_str(users->items[i]);
        if (s == NULL || s[0] == '\0' || s[1] != '\0') { seterr(err, errsz, "invalid user symbol"); return -1; }
        syms[i] = s;
        p->players[i].symbol = s[0];
        p->players[i].status = AUTOMATION_STATUS_NORMAL;
    }

    players = jv_get(preset, "players");
    if (players != NULL && players->type == J_ARR) {
        size_t k;
        for (k = 0; k < players->count; ++k) {
            const JValue *it = players->items[k];
            const char *id = jv_str(jv_get(it, "id"));
            const JValue *items;
            int idx = find_symbol(syms, p->player_count, id);
            int st;
            if (idx < 0) { seterr(err, errsz, "player id not in users"); return -1; }
            p->players[idx].fund = (int)jv_int(jv_get(it, "fund"), 0);
            p->players[idx].credit = (int)jv_int(jv_get(it, "credit"), 0);
            p->players[idx].position = (int)jv_int(jv_get(it, "position"), 0);
            p->players[idx].god_of_wealth_rounds =
                (int)jv_int(jv_get(it, "god_of_wealth_rounds"), 0);
            p->players[idx].remaining_rounds =
                (int)jv_int(jv_get(it, "remaining_rounds"), 0);
            st = parse_status(jv_str(jv_get(it, "status")));
            if (st < 0) { seterr(err, errsz, "invalid status"); return -1; }
            p->players[idx].status = (AutomationPlayerStatus)st;
            items = jv_get(it, "items");
            p->players[idx].block = (int)jv_int(jv_get(items, "BLOCK"), 0);
            p->players[idx].bomb = (int)jv_int(jv_get(items, "BOMB"), 0);
            p->players[idx].robot = (int)jv_int(jv_get(items, "ROBOT"), 0);
        }
    }

    {
        const char *cu = jv_str(jv_get(preset, "current_user"));
        if (cu != NULL) {
            p->current_user_index = find_symbol(syms, p->player_count, cu);
            if (p->current_user_index < 0) { seterr(err, errsz, "current_user not in users"); return -1; }
        }
    }

    props = jv_get(preset, "properties");
    if (props != NULL && props->type == J_ARR) {
        size_t k;
        for (k = 0; k < props->count && p->property_count < AUTOMATION_MAX_PROPERTIES; ++k) {
            const JValue *it = props->items[k];
            int pos = (int)jv_int(jv_get(it, "position"), -1);
            const char *owner = jv_str(jv_get(it, "owner"));
            int oi = find_symbol(syms, p->player_count, owner);
            if (pos < 0 || pos >= RICH_MAP_SIZE || oi < 0) { seterr(err, errsz, "invalid property"); return -1; }
            p->properties[p->property_count].position = pos;
            p->properties[p->property_count].owner_index = oi;
            p->properties[p->property_count].level = (int)jv_int(jv_get(it, "level"), 0);
            p->property_count++;
        }
    }

    mis = jv_get(preset, "map_items");
    if (mis != NULL && mis->type == J_ARR) {
        size_t k;
        for (k = 0; k < mis->count && p->map_item_count < AUTOMATION_MAX_MAP_ITEMS; ++k) {
            const JValue *it = mis->items[k];
            int pos = (int)jv_int(jv_get(it, "position"), -1);
            int ty = parse_item_type(jv_str(jv_get(it, "type")));
            if (pos < 0 || pos >= RICH_MAP_SIZE || ty < 0) { seterr(err, errsz, "invalid map item"); return -1; }
            p->map_items[p->map_item_count].position = pos;
            p->map_items[p->map_item_count].type = (AutomationMapItemType)ty;
            p->map_item_count++;
        }
    }

    return 0;
}

static int execute_actions(GameRuntime *rt, const JValue *actions,
                           const int *dice, size_t dice_count,
                           char *err, size_t errsz) {
    char msg[4096];
    size_t ai;
    size_t dice_index = 0;

    if (actions == NULL || actions->type != J_ARR) return 0;

    for (ai = 0; ai < actions->count; ++ai) {
        const JValue *act = actions->items[ai];
        const char *cmd = jv_str(jv_get(act, "command"));
        const JValue *params = jv_get(act, "params");

        if (cmd == NULL) { seterr(err, errsz, "action missing command"); return -1; }

        if (strcmp(cmd, "ROLL") == 0) {
            int steps;
            if (dice_index >= dice_count) { seterr(err, errsz, "DICE_SEQUENCE_EMPTY"); return -1; }
            steps = dice[dice_index++];
            if (steps < 1 || steps > 6) { seterr(err, errsz, "invalid dice value"); return -1; }
            if (runtime_step(rt, steps, msg, sizeof(msg)) != 0) { seterr(err, errsz, "move failed"); return -1; }
        } else if (strcmp(cmd, "STEP") == 0) {
            int steps = (int)jv_int(jv_get(params, "steps"), 0);
            if (steps <= 0) { seterr(err, errsz, "invalid steps"); return -1; }
            if (runtime_step(rt, steps, msg, sizeof(msg)) != 0) { seterr(err, errsz, "move failed"); return -1; }
        } else if (strcmp(cmd, "SELL") == 0) {
            int pos = (int)jv_int(jv_get(params, "position"), -1);
            if (pos < 0 || pos >= RICH_MAP_SIZE) { seterr(err, errsz, "invalid position"); return -1; }
            (void)runtime_sell(rt, pos, msg, sizeof(msg));
        } else if (strcmp(cmd, "BLOCK") == 0) {
            int off = (int)jv_int(jv_get(params, "offset"), 999);
            if (off < -10 || off > 10) { seterr(err, errsz, "offset out of range"); return -1; }
            (void)runtime_place_tool(rt, 1, off, msg, sizeof(msg));
        } else if (strcmp(cmd, "BOMB") == 0) {
            int off = (int)jv_int(jv_get(params, "offset"), 999);
            if (off < -10 || off > 10) { seterr(err, errsz, "offset out of range"); return -1; }
            (void)runtime_place_tool(rt, 3, off, msg, sizeof(msg));
        } else if (strcmp(cmd, "ROBOT") == 0) {
            (void)runtime_use_robot(rt, msg, sizeof(msg));
        } else if (strcmp(cmd, "QUERY") == 0) {
            (void)runtime_query(rt, msg, sizeof(msg));
        } else if (strcmp(cmd, "HELP") == 0) {
            (void)runtime_help(rt, msg, sizeof(msg));
        } else if (strcmp(cmd, "ANSWER") == 0) {
            const char *value = jv_str(jv_get(params, "value"));
            if (value == NULL) { seterr(err, errsz, "answer missing value"); return -1; }
            if (runtime_answer(rt, value, msg, sizeof(msg)) < 0) { seterr(err, errsz, "answer failed"); return -1; }
            /* 正式命令层允许拒绝买地/升级后再输入一次 Help/Quit；JSON 的
             * ANSWER 动作表示该落地事件已经处理完，因此用现有正式接口把
             * 过渡态推进到下一名玩家，不复制或绕过任何业务逻辑。 */
            if (runtime_post_roll_transition_pending(rt) &&
                runtime_complete_post_roll_transition(rt, msg, sizeof(msg)) != 0) {
                seterr(err, errsz, "post-roll transition failed");
                return -1;
            }
        } else if (strcmp(cmd, "QUIT") == 0) {
            (void)runtime_finish(rt);
            break;
        } else {
            seterr(err, errsz, "unknown command");
            return -1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Actual 序列化                                                        */
/* ------------------------------------------------------------------ */

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

static const char *status_name(AutomationPlayerStatus s) {
    switch (s) {
        case AUTOMATION_STATUS_HOSPITAL: return "HOSPITAL";
        case AUTOMATION_STATUS_JAIL: return "JAIL";
        case AUTOMATION_STATUS_BANKRUPT: return "BANKRUPT";
        default: return "NORMAL";
    }
}

static const char *phase_name(AutomationPhase p) {
    switch (p) {
        case AUTOMATION_PHASE_PROMPT: return "PROMPT";
        case AUTOMATION_PHASE_ENDED: return "ENDED";
        default: return "COMMAND";
    }
}

static void emit_actual(const AutomationSnapshot *snap) {
    FILE *f = stdout;
    int i;

    fprintf(f, "{\n");
    fprintf(f, "  \"users\": [");
    for (i = 0; i < snap->player_count; i++) {
        if (i) fprintf(f, ",");
        fprintf(f, "\"%c\"", snap->players[i].symbol);
    }
    fprintf(f, "],\n");

    fprintf(f, "  \"current_user\": ");
    if (snap->current_user_index >= 0) {
        fprintf(f, "\"%c\"", snap->players[snap->current_user_index].symbol);
    } else {
        fprintf(f, "null");
    }
    fprintf(f, ",\n");

    fprintf(f, "  \"phase\": \"%s\",\n", phase_name(snap->phase));

    fprintf(f, "  \"pending_prompt\": ");
    if (snap->pending_prompt != NULL) {
        json_write_string(f, snap->pending_prompt);
    } else {
        fprintf(f, "null");
    }
    fprintf(f, ",\n");

    fprintf(f, "  \"game_status\": \"%s\",\n", snap->game_status ? "FINISHED" : "RUNNING");

    fprintf(f, "  \"winner\": ");
    if (snap->winner_index >= 0) {
        fprintf(f, "\"%c\"", snap->players[snap->winner_index].symbol);
    } else {
        fprintf(f, "null");
    }
    fprintf(f, ",\n");

    fprintf(f, "  \"players\": [");
    for (i = 0; i < snap->player_count; i++) {
        const AutomationPlayer *p = &snap->players[i];
        if (i) fprintf(f, ",");
        fprintf(f,
            "{\"id\":\"%c\",\"fund\":%d,\"credit\":%d,\"position\":%d,"
            "\"status\":\"%s\",\"remaining_rounds\":%d,"
            "\"items\":{\"BLOCK\":%d,\"BOMB\":%d,\"ROBOT\":%d},"
            "\"god_of_wealth_rounds\":%d}",
            p->symbol, p->fund, p->credit, p->position,
            status_name(p->status), p->remaining_rounds,
            p->block, p->bomb, p->robot, p->god_of_wealth_rounds);
    }
    fprintf(f, "],\n");

    fprintf(f, "  \"properties\": [");
    for (i = 0; i < snap->property_count; i++) {
        const AutomationProperty *p = &snap->properties[i];
        if (i) fprintf(f, ",");
        fprintf(f, "{\"position\":%d,\"owner\":\"%c\",\"level\":%d}",
                p->position, snap->players[p->owner_index].symbol, p->level);
    }
    fprintf(f, "],\n");

    fprintf(f, "  \"map_items\": [");
    for (i = 0; i < snap->map_item_count; i++) {
        const AutomationMapItem *m = &snap->map_items[i];
        if (i) fprintf(f, ",");
        fprintf(f, "{\"position\":%d,\"type\":\"%s\"}",
                m->position, m->type == AUTOMATION_ITEM_BLOCK ? "BLOCK" : "BOMB");
    }
    fprintf(f, "],\n");

    fprintf(f, "  \"display_players\": [");
    {
        int first = 1;
        int pos;
        for (pos = 0; pos < RICH_MAP_SIZE; pos++) {
            int first_idx = -1;
            int has_current = 0;
            int j;
            for (j = 0; j < snap->player_count; j++) {
                if (snap->players[j].status == AUTOMATION_STATUS_BANKRUPT) continue;
                if (snap->players[j].position != pos) continue;
                if (first_idx < 0) first_idx = j;
                if (j == snap->current_user_index) has_current = 1;
            }
            if (first_idx >= 0) {
                int vis = has_current ? snap->current_user_index : first_idx;
                if (!first) fprintf(f, ",");
                fprintf(f, "{\"position\":%d,\"visible_user\":\"%c\"}",
                        pos, snap->players[vis].symbol);
                first = 0;
            }
        }
    }
    fprintf(f, "]\n");
    fprintf(f, "}\n");
}

static void emit_error(const char *case_id, const char *code, const char *msg) {
    fprintf(stdout, "{\"schema_version\":\"1.0\",\"case_id\":");
    if (case_id != NULL) {
        json_write_string(stdout, case_id);
    } else {
        fprintf(stdout, "null");
    }
    fprintf(stdout, ",\"result\":\"ERROR\",\"errors\":[{\"code\":\"%s\","
            "\"path\":\"\",\"expected\":null,\"actual\":null,\"message\":",
            code);
    json_write_string(stdout, msg);
    fprintf(stdout, "}]}\n");
}

/* ------------------------------------------------------------------ */
/* 入口                                                                */
/* ------------------------------------------------------------------ */

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    long sz;
    char *buf;
    size_t got;
    if (f == NULL) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    rewind(f);
    buf = xmalloc((size_t)sz + 1);
    got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[got] = 0;
    return buf;
}

int main(int argc, char **argv) {
    char *text;
    JValue *case_obj;
    const char *case_id;
    AutomationPreset preset;
    int dice[256];
    size_t dice_count = 0;
    const JValue *dicej;
    const JValue *actions;
    GameRuntime *rt;
    AutomationSnapshot snap;
    char err[256] = "";

    if (argc != 3) {
        emit_error(NULL, "INVALID_ARGUMENT", "usage: <program> <case.json> <map.json>");
        return 0;
    }

    text = read_file(argv[1]);
    if (text == NULL) {
        emit_error(NULL, "INVALID_JSON", "cannot read case file");
        return 0;
    }
    case_obj = json_parse(text);
    free(text);
    if (case_obj == NULL) {
        emit_error(NULL, "INVALID_JSON", "case is not valid JSON");
        return 0;
    }
    case_id = jv_str(jv_get(case_obj, "case_id"));

    if (build_preset(case_obj, &preset, err, sizeof(err)) != 0) {
        emit_error(case_id, "INVALID_PRESET", err);
        jv_free(case_obj);
        return 0;
    }

    dicej = jv_get(jv_get(case_obj, "preset"), "dice_sequence");
    if (dicej != NULL && dicej->type == J_ARR) {
        size_t i;
        for (i = 0; i < dicej->count && dice_count < 256; ++i) {
            dice[dice_count++] = (int)jv_int(dicej->items[i], 0);
        }
    }
    actions = jv_get(case_obj, "actions");

    rt = runtime_load_preset(&preset);
    if (rt == NULL) {
        emit_error(case_id, "INVALID_PRESET", "failed to load preset");
        jv_free(case_obj);
        return 0;
    }

    if (execute_actions(rt, actions, dice, dice_count, err, sizeof(err)) != 0) {
        emit_error(case_id, "INVALID_ACTION", err);
        runtime_destroy(rt);
        jv_free(case_obj);
        return 0;
    }

    runtime_snapshot(rt, &snap);
    emit_actual(&snap);

    runtime_destroy(rt);
    jv_free(case_obj);
    return 0;
}
