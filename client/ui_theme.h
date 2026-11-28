#pragma once

#include <QColor>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRectF>
#include <QtMath>
#include <vector>

namespace ui_theme {

// 悍匠（HanJiang）战术科技 × 液态玻璃色板
inline const QColor kBgDeep("#05090f");
inline const QColor kGlassDark(8, 14, 24, 148);
inline const QColor kGlassPanel(10, 17, 28, 200);
inline const QColor kGlassBorder(185, 212, 240, 68);
inline const QColor kGlassHighlight(255, 255, 255, 42);

inline const QColor kGold("#e8a33d");        // 悍匠锻造琥珀金（源自 Logo 主色）
inline const QColor kGoldLight("#ffc971");   // 高亮金
inline const QColor kGoldDim(232, 163, 61, 110);

inline const QColor kRed("#ff4655");         // 现代电竞红
inline const QColor kBlue("#2ea8ff");        // 现代电竞蓝
inline const QColor kCyan("#3ce8ff");        // 战术青
inline const QColor kGreen("#5ce6a0");       // 状态正常绿

inline const QColor kTextPrimary("#f2f7fc");
inline const QColor kTextSecondary("#a2b6ca");
inline const QColor kTextMuted("#6e8399");

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
    p.drawLine(QPointF(r.left(), r.top() + l), QPointF(r.left(), r.top()));
    p.drawLine(QPointF(r.left(), r.top()), QPointF(r.left() + l, r.top()));
    p.drawLine(QPointF(r.right() - l, r.bottom()), QPointF(r.right(), r.bottom()));
    p.drawLine(QPointF(r.right(), r.bottom()), QPointF(r.right(), r.bottom() - l));
    p.restore();
}

// 轻量可分离两趟盒式模糊（用于 1/8 分辨率图传底图，单帧 < 0.3ms）
inline QImage fastBlur(const QImage &input, int radius = 3) {
    if (input.isNull() || radius <= 0) return input;
    QImage img = input.convertToFormat(QImage::Format_ARGB32);
    const int w = img.width();
    const int h = img.height();
    if (w <= 2 || h <= 2) return img;
    QImage tmp(w, h, QImage::Format_ARGB32);
    const int div = radius * 2 + 1;

    // 水平趟
    for (int y = 0; y < h; ++y) {
        const QRgb *src = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        QRgb *dst = reinterpret_cast<QRgb *>(tmp.scanLine(y));
        int r = 0, g = 0, b = 0;
        for (int i = -radius; i <= radius; ++i) {
            const QRgb c = src[qBound(0, i, w - 1)];
            r += qRed(c); g += qGreen(c); b += qBlue(c);
        }
        for (int x = 0; x < w; ++x) {
            dst[x] = qRgb(r / div, g / div, b / div);
            const QRgb outC = src[qBound(0, x - radius, w - 1)];
            const QRgb inC = src[qBound(0, x + radius + 1, w - 1)];
            r += qRed(inC) - qRed(outC);
            g += qGreen(inC) - qGreen(outC);
            b += qBlue(inC) - qBlue(outC);
        }
    }
    // 垂直趟 + 轻微提亮以增强液态玻璃通透度
    for (int x = 0; x < w; ++x) {
        int r = 0, g = 0, b = 0;
        for (int i = -radius; i <= radius; ++i) {
            const QRgb c = reinterpret_cast<const QRgb *>(tmp.constScanLine(qBound(0, i, h - 1)))[x];
            r += qRed(c); g += qGreen(c); b += qBlue(c);
        }
        for (int y = 0; y < h; ++y) {
            const int rr = qMin(255, (r / div) * 112 / 100);
            const int gg = qMin(255, (g / div) * 112 / 100);
            const int bb = qMin(255, (b / div) * 115 / 100);
            reinterpret_cast<QRgb *>(img.scanLine(y))[x] = qRgb(rr, gg, bb);
            const QRgb outC = reinterpret_cast<const QRgb *>(tmp.constScanLine(qBound(0, y - radius, h - 1)))[x];
            const QRgb inC = reinterpret_cast<const QRgb *>(tmp.constScanLine(qBound(0, y + radius + 1, h - 1)))[x];
            r += qRed(inC) - qRed(outC);
            g += qGreen(inC) - qGreen(outC);
            b += qBlue(inC) - qBlue(outC);
        }
    }
    return img;
}

// 液态玻璃渲染器：底层实时折射模糊 + 深色透光膜 + 曲面高光反射 + 双层镜面折射描边
inline void drawLiquidGlass(QPainter &p,
                            const QPainterPath &clip,
                            const QRectF &r,
                            const QImage &backdrop,
                            const QSizeF &viewportSize,
                            const QPointF &posInViewport,
                            const QColor &accent,
                            int darkAlpha = 140) {
    p.save();
    p.setClipPath(clip);

    // 1. 实时背景折射模糊采样（略带 2% 边缘放大，模拟曲面透镜折射）
    if (!backdrop.isNull() && viewportSize.width() > 1.0 && viewportSize.height() > 1.0) {
        const qreal sx = backdrop.width() / viewportSize.width();
        const qreal sy = backdrop.height() / viewportSize.height();
        QRectF src((posInViewport.x() + r.left()) * sx,
                   (posInViewport.y() + r.top()) * sy,
                   r.width() * sx,
                   r.height() * sy);
        // 轻微内缩采样实现微妙折射感
        src.adjust(src.width() * 0.015, src.height() * 0.015, -src.width() * 0.015, -src.height() * 0.015);
        src = src.intersected(QRectF(0, 0, backdrop.width(), backdrop.height()));
        if (src.width() >= 1.0 && src.height() >= 1.0) {
            p.setRenderHint(QPainter::SmoothPixmapTransform, true);
            p.drawImage(r, backdrop, src);
        }
    }

    // 2. 深色玻璃染色膜（保证前景数字高对比可读）
    QLinearGradient tint(r.topLeft(), r.bottomLeft());
    tint.setColorAt(0.0, QColor(14, 24, 38, qBound(60, darkAlpha, 225)));
    tint.setColorAt(0.55, QColor(8, 15, 25, qBound(70, darkAlpha + 18, 235)));
    tint.setColorAt(1.0, QColor(5, 10, 18, qBound(80, darkAlpha + 30, 242)));
    p.fillRect(r, tint);

    // 3. 顶部液态曲面高光反射
    QLinearGradient sheen(r.topLeft(), QPointF(r.left(), r.top() + r.height() * 0.42));
    sheen.setColorAt(0.0, QColor(255, 255, 255, 42));
    sheen.setColorAt(0.35, QColor(225, 240, 255, 14));
    sheen.setColorAt(1.0, QColor(255, 255, 255, 0));
    p.fillRect(QRectF(r.left(), r.top(), r.width(), r.height() * 0.42), sheen);

    // 4. 阵营 / 悍匠金环境柔光渗透
    if (accent.isValid()) {
        QLinearGradient glow(r.topLeft(), QPointF(r.left(), r.top() + r.height() * 0.45));
        QColor c0 = accent; c0.setAlpha(46);
        QColor c1 = accent; c1.setAlpha(0);
        glow.setColorAt(0.0, c0);
        glow.setColorAt(1.0, c1);
        p.fillRect(QRectF(r.left(), r.top(), r.width(), r.height() * 0.45), glow);
    }
    p.restore();

    // 5. 液态玻璃双层镜面高光描边（左上亮冷白、右下微金/阵营反光）
    QLinearGradient rim(r.topLeft(), r.bottomRight());
    QColor bright(255, 255, 255, 125);
    QColor mid(175, 205, 235, 48);
    QColor end = accent.isValid() ? accent : kGoldLight;
    end.setAlpha(115);
    rim.setColorAt(0.0, bright);
    rim.setColorAt(0.45, mid);
    rim.setColorAt(1.0, end);
    p.strokePath(clip, QPen(QBrush(rim), 1.15));
}

} // namespace ui_theme
