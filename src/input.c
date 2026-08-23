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
    const char *p;

    if (*s == '\0') {
        return 0;
    }

    /* 纯数字校验：拒绝小数、文字、正负号或其他字符
       （strtol 本身会容忍 '+'/'-' 号，必须先拦住，与初始资金同一标准） */
    for (p = s; *p != '\0'; ++p) {
        if (!isdigit((unsigned char)*p)) {
            return 0;
        }
    }

    /* 全数字串解析（strtol 溢出会得到 LONG_MAX，调用方的范围检查必然拦截） */
    *out = strtol(s, NULL, 10);
    return 1;
}
