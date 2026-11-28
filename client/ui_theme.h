#pragma once

#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QRectF>

namespace ui_theme {

// 悍匠（HanJiang）战术科技视觉色板
inline const QColor kBgDeep("#05090f");
inline const QColor kGlassDark(7, 13, 22, 168);
inline const QColor kGlassPanel(9, 16, 26, 215);
inline const QColor kGlassBorder(154, 186, 218, 56);
inline const QColor kGlassHighlight(255, 255, 255, 22);

inline const QColor kGold("#e8a33d");        // 悍匠锻造琥珀金（源自 Logo 主色）
inline const QColor kGoldLight("#ffc971");   // 高亮金
inline const QColor kGoldDim(232, 163, 61, 110);

inline const QColor kRed("#ff4655");         // 现代电竞红
inline const QColor kBlue("#2ea8ff");        // 现代电竞蓝
inline const QColor kCyan("#3ce8ff");        // 战术青
inline const QColor kGreen("#5ce6a0");       // 状态正常绿

inline const QColor kTextPrimary("#f1f6fc");
inline const QColor kTextSecondary("#9cb0c5");
inline const QColor kTextMuted("#6b7f94");

// 45° 斜切角战术面板轮廓（右上与左下切角，呼应悍匠闪电斜线）
inline QPainterPath chamferedRect(const QRectF &r, qreal cut) {
    const qreal c = qBound(0.0, cut, qMin(r.width(), r.height()) * 0.35);
    QPainterPath path;
    path.moveTo(r.left(), r.top());
    path.lineTo(r.right() - c, r.top());
    path.lineTo(r.right(), r.top() + c);
    path.lineTo(r.right(), r.bottom());
    path.lineTo(r.left() + c, r.bottom());
    path.lineTo(r.left(), r.bottom() - c);
    path.closeSubpath();
    return path;
}

// 四角 L 型瞄准角标
inline void drawCornerBrackets(QPainter &p, const QRectF &r, qreal len, const QColor &color, qreal width = 1.5) {
    const qreal l = qBound(3.0, len, qMin(r.width(), r.height()) * 0.25);
    p.save();
    p.setPen(QPen(color, width, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    // 左上
    p.drawLine(QPointF(r.left(), r.top() + l), QPointF(r.left(), r.top()));
    p.drawLine(QPointF(r.left(), r.top()), QPointF(r.left() + l, r.top()));
    // 右下
    p.drawLine(QPointF(r.right() - l, r.bottom()), QPointF(r.right(), r.bottom()));
    p.drawLine(QPointF(r.right(), r.bottom()), QPointF(r.right(), r.bottom() - l));
    p.restore();
}

} // namespace ui_theme
