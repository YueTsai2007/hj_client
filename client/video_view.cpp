#include "video_view.h"
#include "brand.h"
#include "ui_theme.h"

#include <QPainter>
#include <QRadialGradient>
#include <QTimer>
#include <cmath>
#include <utility>

VideoView::VideoView(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(180, 110);
    setPlaceholder(QStringLiteral("等待图传首帧"));
    standbyTimer_ = new QTimer(this);
    standbyTimer_->setInterval(50);
    connect(standbyTimer_, &QTimer::timeout, this, [this] {
        if (!isVisible() || !frame_.isNull()) return;
        pulsePhase_ += 0.07;
        if (pulsePhase_ > 628.318) pulsePhase_ -= 628.318;
        update();
    });
    standbyTimer_->start();
}

void VideoView::setFrame(QImage frame) {
    frame_ = std::move(frame);
    update();
}

void VideoView::setPlaceholder(const QString &text) {
    if (frame_.isNull() && placeholder_ == text) return;
    frame_ = {};
    placeholder_ = text;
    setAccessibleName(text);
    update();
}

void VideoView::setFillMode(bool expandToFill) {
    if (fillMode_ == expandToFill) return;
    fillMode_ = expandToFill;
    update();
}

void VideoView::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.fillRect(rect(), ui_theme::kBgDeep);

    if (!frame_.isNull()) {
        const Qt::AspectRatioMode mode = fillMode_ ? Qt::KeepAspectRatioByExpanding : Qt::KeepAspectRatio;
        const QSize size = frame_.size().scaled(this->size(), mode);
        const QRect target(QPoint((width() - size.width()) / 2, (height() - size.height()) / 2), size);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.drawImage(target, frame_);
        return;
    }

    // 图传待机态：悍匠战术科技网格 + 脉冲徽标
    painter.setRenderHint(QPainter::Antialiasing);
    const QPointF center(width() / 2.0, height() / 2.0);

    // 深空微光背景
    QRadialGradient bgGlow(center, qMax(width(), height()) * 0.55);
    bgGlow.setColorAt(0.0, QColor(24, 34, 48, 220));
    bgGlow.setColorAt(0.55, QColor(8, 13, 21, 255));
    bgGlow.setColorAt(1.0, QColor(4, 7, 12, 255));
    painter.fillRect(rect(), bgGlow);

    // 战术细网格
    const int step = qMax(36, qMin(width(), height()) / 16);
    painter.setPen(QPen(QColor(110, 148, 184, 18), 1.0));
    for (int x = (width() / 2) % step; x < width(); x += step)
        painter.drawLine(x, 0, x, height());
    for (int y = (height() / 2) % step; y < height(); y += step)
        painter.drawLine(0, y, width(), y);

    // 中心悍匠脉冲徽标与旋转齿轮环
    const qreal emblemR = qBound(42.0, qMin(width(), height()) * 0.095, 96.0);
    const qreal pulse = 0.5 + 0.5 * std::sin(pulsePhase_);

    QRadialGradient goldHalo(center, emblemR * 1.9);
    goldHalo.setColorAt(0.0, QColor(232, 163, 61, qRound(38 + 26 * pulse)));
    goldHalo.setColorAt(0.55, QColor(232, 163, 61, qRound(12 + 10 * pulse)));
    goldHalo.setColorAt(1.0, QColor(232, 163, 61, 0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(goldHalo);
    painter.drawEllipse(center, emblemR * 1.9, emblemR * 1.9);

    // 外圈机械齿轮虚线环
    painter.save();
    painter.translate(center);
    painter.rotate(pulsePhase_ * 12.0);
    QPen outerDash(QColor(232, 163, 61, 115), 2.0, Qt::CustomDashLine);
    outerDash.setDashPattern({3.0, 4.0});
    painter.setPen(outerDash);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QPointF(0, 0), emblemR * 1.18, emblemR * 1.18);
    painter.restore();

    // 内圈反向细环
    painter.save();
    painter.translate(center);
    painter.rotate(-pulsePhase_ * 18.0);
    painter.setPen(QPen(QColor(154, 186, 218, 70), 1.2, Qt::DashLine));
    painter.drawEllipse(QPointF(0, 0), emblemR * 0.94, emblemR * 0.94);
    painter.restore();

    // 悍匠 Logo
    const int logoPx = qRound(emblemR * 1.25);
    const QPixmap pm = brand::logo(logoPx, devicePixelRatioF());
    if (!pm.isNull()) {
        painter.setOpacity(0.86);
        painter.drawPixmap(QPointF(center.x() - logoPx / 2.0, center.y() - logoPx / 2.0), pm);
        painter.setOpacity(1.0);
    }

    // 品牌标识与待机状态
    if (height() >= 240) {
        QFont brandFont = painter.font();
        brandFont.setPixelSize(qBound(12, qRound(emblemR * 0.24), 18));
        brandFont.setBold(true);
        brandFont.setLetterSpacing(QFont::PercentageSpacing, 118);
        painter.setFont(brandFont);
        painter.setPen(QColor(255, 201, 113, 215));
        painter.drawText(QRectF(0, center.y() + emblemR * 1.38, width(), 28),
                         Qt::AlignHCenter | Qt::AlignTop,
                         QStringLiteral("HANJIANG // 深圳技术大学悍匠战队"));

        QFont subFont = painter.font();
        subFont.setPixelSize(qBound(11, qRound(emblemR * 0.19), 15));
        subFont.setBold(false);
        subFont.setLetterSpacing(QFont::PercentageSpacing, 105);
        painter.setFont(subFont);
        painter.setPen(QColor(156, 176, 197, 190));
        painter.drawText(QRectF(0, center.y() + emblemR * 1.38 + 26, width(), 24),
                         Qt::AlignHCenter | Qt::AlignTop,
                         placeholder_ + QStringLiteral("  ·  F5 画面适配  ·  M 地图缩放"));
    } else {
        painter.setPen(QColor(184, 196, 208));
        painter.drawText(rect().adjusted(0, qRound(emblemR * 0.9), 0, 0), Qt::AlignHCenter | Qt::AlignTop, placeholder_);
    }
}
