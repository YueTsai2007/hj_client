#include "brand.h"
#include <QConicalGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>

namespace brand {
QPixmap logo(int size, qreal dpr) {
    static const QPixmap source(QStringLiteral(":/images/logo.png"));
    QPixmap scaled = source.scaled(QSize(size, size) * dpr, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(dpr);
    return scaled;
}
}

LogoBadge::LogoBadge(int size, QWidget *parent) : QWidget(parent), size_(size) {
    setFixedSize(size, size);
}

void LogoBadge::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(2, 2, -2, -2);
    const QPointF c = r.center();
    // 金色光晕底盘
    QRadialGradient glow(c, r.width() / 2);
    glow.setColorAt(0.0, QColor(232, 163, 61, 90));
    glow.setColorAt(0.75, QColor(20, 14, 6, 200));
    glow.setColorAt(1.0, QColor(10, 12, 16, 220));
    p.setPen(Qt::NoPen);
    p.setBrush(glow);
    p.drawEllipse(r.adjusted(5, 5, -5, -5));
    // 内圈细线
    p.setPen(QPen(QColor(232, 163, 61, 140), 1.2));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(r.adjusted(5, 5, -5, -5));
    // 虚线齿轮环
    p.save();
    p.translate(c);
    QPen dash(QColor(255, 201, 113, 190), 2.2, Qt::CustomDashLine);
    dash.setDashPattern({2.0, 3.2});
    dash.setCapStyle(Qt::RoundCap);
    p.setPen(dash);
    const qreal rr = r.width() / 2 - 1;
    p.drawEllipse(QRectF(-rr, -rr, rr * 2, rr * 2));
    p.restore();
    // Logo
    const int inner = int(r.width() * 0.62);
    const QPixmap pm = brand::logo(inner, devicePixelRatioF());
    p.drawPixmap(QPointF(c.x() - inner / 2.0, c.y() - inner / 2.0), pm);
}

GoldFrame::GoldFrame(QWidget *parent) : QFrame(parent) {}

void GoldFrame::setAccent(const QColor &color) {
    accent_ = color;
    update();
}

void GoldFrame::paintEvent(QPaintEvent *event) {
    QFrame::paintEvent(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    // 淡色 Logo 水印（右下）
    if (watermark_) {
        const int size = qMin(width(), height()) * 3 / 4;
        p.setOpacity(0.045);
        p.drawPixmap(QPointF(width() - size - 6, height() - size - 6), brand::logo(size, devicePixelRatioF()));
        p.setOpacity(1.0);
    }
    // 顶部强调线（阵营色，两端渐隐）
    QLinearGradient top(r.left(), 0, r.right(), 0);
    QColor a = accent_; a.setAlpha(0);
    QColor b = accent_; b.setAlpha(145);
    top.setColorAt(0.0, a); top.setColorAt(0.18, b); top.setColorAt(0.82, b); top.setColorAt(1.0, a);
    p.setPen(QPen(QBrush(top), 1));
    p.drawLine(QPointF(r.left() + 14, r.top() + 1), QPointF(r.right() - 14, r.top() + 1));
}
