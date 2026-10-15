#include "modules/TimeSync.h"

#include <Arduino.h>
#include <sys/time.h>
#include <time.h>

#include "config.h"

namespace {
constexpr time_t kMinValidEpoch = 1700000000;  // 11/2023: trước mốc này là chưa đồng bộ
}

void TimeSync::begin() {
    // SNTP tự chạy nền khi có mạng
    configTzTime(TIMEZONE, NTP_SERVER_1, NTP_SERVER_2);
}

bool TimeSync::isSynced() const { return time(nullptr) > kMinValidEpoch; }

int64_t TimeSync::nowEpochMs() const {
    timeval tv;
    gettimeofday(&tv, nullptr);
    return static_cast<int64_t>(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
}
