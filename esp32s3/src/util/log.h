#pragma once

#ifndef LOG_LEVEL
#define LOG_LEVEL 3     // 0=关  1=ERROR  2=WARN  3=INFO  4=DEBUG
#endif

void log_print(char level, const char* tag, const char* fmt, ...)
    __attribute__((format(printf, 3, 4)));      //让编译器检查写的 %d 和后面的参数类型对不对。%lu 配了 uint32_t 这类错误，会在编译时报警告，而不是运行时输出乱码。

#define LOG_E(tag, ...) do { if (LOG_LEVEL >= 1) log_print('E', tag, __VA_ARGS__); } while (0)
#define LOG_W(tag, ...) do { if (LOG_LEVEL >= 2) log_print('W', tag, __VA_ARGS__); } while (0)
#define LOG_I(tag, ...) do { if (LOG_LEVEL >= 3) log_print('I', tag, __VA_ARGS__); } while (0)
#define LOG_D(tag, ...) do { if (LOG_LEVEL >= 4) log_print('D', tag, __VA_ARGS__); } while (0)