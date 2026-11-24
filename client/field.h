#pragma once

// 场地坐标的唯一定义处（协议 0x020B / 0x0203）
//   原点：红方补给站附近的场地围挡交点
//   X 轴：沿场地长边，向蓝方为正（0 → 28m）
//   Y 轴：沿场地短边，向**红方停机坪**为正（0 → 15m）
// 单位：米。转换到 0~1 归一化后再交给视图，视图只负责“画”。
namespace field {

inline constexpr double kLengthM = 28.0;   // 长边
inline constexpr double kWidthM = 15.0;    // 短边

inline double normX(double metersX) { return metersX / kLengthM; }
inline double normY(double metersY) { return metersY / kWidthM; }

} // namespace field
