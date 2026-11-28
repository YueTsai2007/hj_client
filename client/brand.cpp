#include "brand.h"
#include "ui_theme.h"
#include <QConicalGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QTimer>

namespace brand {
QPixmap logo(int size, qreal dpr) {
    static const QPixmap source(QStringLiteral(":/images/logo.png"));
    if (size <= 0 || source.isNull()) return {};
    QPixmap scaled = source.scaled(QSize(size, size) * dpr, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(dpr);
    return scaled;
}
}

LogoBadge::LogoBadge(int size, QWidget *parent) : QWidget(parent), size_(size) {
    setFixedSize(size, size);
    setAttribute(Qt::WA_TranslucentBackground, true);
    spinTimer_ = new QTimer(this);
    spinTimer_->setInterval(50);
    connect(spinTimer_, &QTimer::timeout, this, [this] {
        if (!isVisible()) return;
        angle_ += 0.6;
        if (angle_ >= 360.0) angle_ -= 360.0;
        update();
    });
    spinTimer_->start();
}

void LogoBadge::setBadgeSize(int size) {
    const int clamped = qMax(24, size);
    if (size_ == clamped) return;
    size_ = clamped;
    setFixedSize(size_, size_);
    update();
}

void LogoBadge::setAnimated(bool on) {
    if (!spinTimer_) return;
    if (on) spinTimer_->start();
    else spinTimer_->stop();
}

void LogoBadge::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(2, 2, -2, -2);
    const QPointF c = r.center();
    const qreal radius = qMin(r.width(), r.height()) / 2.0;

    // 锻造琥珀金光晕底盘
    QRadialGradient glow(c, radius);
    glow.setColorAt(0.0, QColor(232, 163, 61, 105));
    glow.setColorAt(0.68, QColor(24, 17, 8, 210));
    glow.setColorAt(1.0, QColor(7, 11, 18, 235));
    p.setPen(Qt::NoPen);
    p.setBrush(glow);
    p.drawEllipse(c, radius - 3.0, radius - 3.0);

    // 内圈细线
    p.setPen(QPen(QColor(232, 163, 61, 150), 1.1));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(c, radius - 4.0, radius - 4.0);

    // 动态旋转齿轮虚线环
    p.save();
    p.translate(c);
    p.rotate(angle_);
    QPen dash(QColor(255, 201, 113, 210), qMax(1.6, radius * 0.08), Qt::CustomDashLine);
    dash.setDashPattern({2.2, 2.8});
    dash.setCapStyle(Qt::FlatCap);
    p.setPen(dash);
    const qreal rr = radius - 1.2;
    p.drawEllipse(QRectF(-rr, -rr, rr * 2.0, rr * 2.0));
    p.restore();

    // 悍匠 Logo
    const int inner = qMax(12, qRound(radius * 1.32));
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
    // 淡色悍匠 Logo 水印（右下）
    if (watermark_) {
        const int size = qMin(width(), height()) * 3 / 4;
        p.setOpacity(0.05);
        p.drawPixmap(QPointF(width() - size - 6, height() - size - 6), brand::logo(size, devicePixelRatioF()));
        p.setOpacity(1.0);
    }
    // 顶部流光强调线（阵营色/悍匠金，两端渐隐）
    QLinearGradient top(r.left(), 0, r.right(), 0);
    QColor a = accent_; a.setAlpha(0);
    QColor b = accent_; b.setAlpha(185);
    top.setColorAt(0.0, a); top.setColorAt(0.16, b); top.setColorAt(0.84, b); top.setColorAt(1.0, a);
    p.setPen(QPen(QBrush(top), 1.4));
    p.drawLine(QPointF(r.left() + 12, r.top() + 1), QPointF(r.right() - 12, r.top() + 1));

    // 微型战术角标
    QColor corner = accent_;
    corner.setAlpha(135);
    ui_theme::drawCornerBrackets(p, r.adjusted(2, 2, -2, -2), 8.0, corner, 1.2);
}
