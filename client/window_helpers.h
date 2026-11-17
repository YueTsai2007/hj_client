#pragma once

#include <QLabel>
#include <chrono>

inline qint64 monotonicMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
inline QLabel *label(const QString &text, const char *object = nullptr) {
    auto *result = new QLabel(text);
    if (object) result->setObjectName(object);
    return result;
}

inline QString teamColor(bool ally, int ownSide) {
    if (ownSide < 0) return QStringLiteral("#a6b7bd");
    const bool red = ally ? ownSide == 0 : ownSide == 1;
    return red ? QStringLiteral("#ff6670") : QStringLiteral("#61c8ff");
}
