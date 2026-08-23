#include "input.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int input_read_line(char *buf, size_t size)
{
    if (fgets(buf, (int)size, stdin) == NULL) {
        return 0; /* EOF */
    }
    if (strchr(buf, '\n') == NULL && !feof(stdin)) {
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF) {
            /* 丢弃超长部分 */
        }
        /* 保留已截断的内容：交由后续校验按非法输入处理，
           不能清空——否则超长行会被误当作“空行”（如默认初始资金） */
        return 1;
    }
    return 1;
}

char *input_trim(char *s)
{
    char *end;

    while (*s != '\0' && isspace((unsigned char)*s)) {
        ++s;
    }
    end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) {
        --end;
    }
    *end = '\0';
    return s;
}

int input_parse_int(const char *s, long *out)
{
    char *end = NULL;
    long  v;

    if (*s == '\0') {
        return 0;
    }
    v = strtol(s, &end, 10);
    if (end == s || *end != '\0') {
        return 0; /* 含非法字符 */
    }
    *out = v;
    return 1;
}
