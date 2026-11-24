#pragma once
#include <cstring>

// 下行消息的频率与“过期”阈值集中定义，避免散落在各处。
// 频率依据：2027 通信协议 V1.0.0 表 1-5（命令码一览）中对应数据的发送频率。
// 过期阈值 = max(3 × 周期, 1.2s)，触发类消息用 3s。
namespace topics {

struct Spec {
    const char *topic;
    const char *label;
    int hz;          // 官方发送频率（Hz），0 表示触发式
    int timeoutMs;   // 超过该时长视为过期
};

inline constexpr Spec kSpecs[] = {
    {"GameStatus", "比赛状态", 1, 3000},
    {"GlobalUnitStatus", "机器人血量", 3, 1500},
    {"GlobalLogisticsStatus", "经济", 10, 800},
    {"RobotDynamicStatus", "本机状态", 10, 800},
    {"RobotStaticStatus", "本机上限", 3, 1500},
    {"RobotRespawnStatus", "复活状态", 3, 1500},
    {"RobotModuleStatus", "模块状态", 3, 1500},
    {"RobotPosition", "机器人位置", 1, 3000},
    {"RadarInfoToClient", "雷达", 5, 1200},
    {"Buff", "增益", 3, 1500},
    {"GlobalSpecialMechanism", "特殊机制", 1, 3000},
    {"Event", "事件", 1, 3000},
    {"PenaltyInfo", "判罚", 1, 3000},
};

inline int timeoutMs(const char *topic, int fallback = 3000) {
    for (const auto &spec : kSpecs) {
        if (std::strcmp(spec.topic, topic) == 0) return spec.timeoutMs;
    }
    return fallback;
}

inline int hz(const char *topic) {
    for (const auto &spec : kSpecs) {
        if (std::strcmp(spec.topic, topic) == 0) return spec.hz;
    }
    return 0;
}

} // namespace topics
