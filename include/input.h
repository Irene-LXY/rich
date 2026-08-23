#ifndef INPUT_H
#define INPUT_H

#include <stddef.h>

/* 输入行缓冲区大小 */
#define INPUT_LINE_MAX 128

/*
 * 读取一行输入。
 * 成功返回 1；遇到 EOF / 读错误返回 0。
 * 行长度超过缓冲区时丢弃本行剩余内容，保留截断后的内容，
 * 交由后续校验按非法输入处理（超长行绝不等同于空行）。
 */
int input_read_line(char *buf, size_t size);

/* 去掉首尾空白字符（空格、制表符、换行等），返回指向有效内容的指针 */
char *input_trim(char *s);

/*
 * 解析“整行必须恰好是一个整数”（允许前导 +/-）。
 * 成功返回 1 并写入 *out；空串或含任何非数字字符均返回 0。
 */
int input_parse_int(const char *s, long *out);

#endif /* INPUT_H */
