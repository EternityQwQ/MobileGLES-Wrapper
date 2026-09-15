#pragma once
#define ANDROID_LOG_DEBUG 3
#define ANDROID_LOG_WARN 5
#define ANDROID_LOG_ERROR 6
static inline int __android_log_print(int, const char*, const char*, ...) { return 0; }
#define ANDROID_LOG_INFO 4
#define ANDROID_LOG_VERBOSE 2
#define ANDROID_LOG_FATAL 7
#define ANDROID_LOG_SILENT 8
