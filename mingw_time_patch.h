#pragma once
#ifdef __MINGW32__
#include <time.h>

// 模擬微軟的安全時間函式
inline int gmtime_s(struct tm* _tm, const time_t* _time) {
    struct tm* res = gmtime(_time);
    if (res) { *_tm = *res; return 0; }
    return 1;
}

inline int localtime_s(struct tm* _tm, const time_t* _time) {
    struct tm* res = localtime(_time);
    if (res) { *_tm = *res; return 0; }
    return 1;
}

// 將 _mkgmtime 對齊標準的 mktime
#define _mkgmtime mktime

#endif