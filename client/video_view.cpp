#include "video_view.h"
#include "brand.h"
#include "ui_theme.h"

#include <QPainter>
#include <QRadialGradient>
#include <QResizeEvent>
#include <QTimer>
#include <cmath>
#include <utility>

VideoView::VideoView(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(180, 110);
    setPlaceholder(QStringLiteral("等待图传首帧"));
    fpsClock_.start();
    standbyTimer_ = new QTimer(this);
    standbyTimer_->setInterval(50);
    connect(standbyTimer_, &QTimer::timeout, this, [this] {
        if (!isVisible() || !frame_.isNull()) return;
        pulsePhase_ += 0.07;
        if (pulsePhase_ > 628.318) pulsePhase_ -= 628.318;
        update();
    });
    standbyTimer_->start();

    // 可选验收辅助：HJ_DEMO_VIDEO_BG=1 时自动载入一张模拟彩色赛场图传帧，便于验收液态玻璃折射效果
    if (qEnvironmentVariableIntValue("HJ_DEMO_VIDEO_BG") == 1) {
        QImage sim(1280, 720, QImage::Format_RGB32);
        QPainter p(&sim);
        p.setRenderHint(QPainter::Antialiasing);
        QLinearGradient sky(0, 0, 1280, 720);
        sky.setColorAt(0.0, QColor("#18283a"));
        sky.setColorAt(0.45, QColor("#2a3d52"));
        sky.setColorAt(1.0, QColor("#151e2b"));
        p.fillRect(sim.rect(), sky);
        // 模拟赛场地面透视网格与红/蓝灯条光斑，突出液态玻璃实时模糊折射
        QRadialGradient redLight(260, 180, 260);
        redLight.setColorAt(0.0, QColor(255, 78, 96, 175));
        redLight.setColorAt(1.0, QColor(255, 78, 96, 0));
        p.fillRect(sim.rect(), redLight);
        QRadialGradient blueLight(1020, 190, 280);
        blueLight.setColorAt(0.0, QColor(46, 168, 255, 180));
        blueLight.setColorAt(1.0, QColor(46, 168, 255, 0));
        p.fillRect(sim.rect(), blueLight);
        QRadialGradient goldCore(640, 420, 320);
        goldCore.setColorAt(0.0, QColor(232, 163, 61, 165));
        goldCore.setColorAt(1.0, QColor(232, 163, 61, 0));
        p.fillRect(sim.rect(), goldCore);
        QRadialGradient cyanLow(980, 580, 250);
        cyanLow.setColorAt(0.0, QColor(60, 232, 255, 130));
        cyanLow.setColorAt(1.0, QColor(60, 232, 255, 0));
        p.fillRect(sim.rect(), cyanLow);
        p.setPen(QPen(QColor(255, 255, 255, 45), 2));
        for (int x = 0; x <= 1280; x += 80) p.drawLine(x, 380, 640 + (x - 640) * 2, 720);
        for (int y = 400; y <= 720; y += 40) p.drawLine(0, y, 1280, y);
        // 模拟赛场装甲板/高台轮廓
        p.setPen(QPen(QColor(255, 201, 113, 180), 3));
        p.setBrush(QColor(20, 30, 44, 140));
        p.drawPolygon(QPolygonF(QVector<QPointF>{{420, 470}, {860, 470}, {960, 560}, {320, 560}}));
        setFrame(sim);
    }
}

void VideoView::setFrame(QImage frame) {
    frame_ = std::move(frame);
    ++fpsFrames_;
    const qint64 ms = fpsClock_.elapsed();
    if (ms >= 800) {
        fps_ = fpsFrames_ * 1000.0 / ms;
        fpsFrames_ = 0;
        fpsClock_.restart();
    }
    rebuildBlurredBackdrop();
    update();
}

void VideoView::setPlaceholder(const QString &text) {
    if (frame_.isNull() && placeholder_ == text) return;
    frame_ = {};
    fps_ = 0.0;
    placeholder_ = text;
    setAccessibleName(text);
    rebuildBlurredBackdrop();
    update();
}

void VideoView::setFillMode(bool expandToFill) {
    if (fillMode_ == expandToFill) return;
    fillMode_ = expandToFill;
    rebuildBlurredBackdrop();
    update();
}

void VideoView::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    rebuildBlurredBackdrop();
}

void VideoView::rebuildBlurredBackdrop() {
    const int bw = qBound(120, width() / 8, 260);
    const int bh = qBound(68, height() / 8, 150);
    QImage sample(bw, bh, QImage::Format_ARGB32);
    sample.fill(ui_theme::kBgDeep);
    QPainter sp(&sample);
    if (!frame_.isNull()) {
        const Qt::AspectRatioMode mode = fillMode_ ? Qt::KeepAspectRatioByExpanding : Qt::KeepAspectRatio;
        const QSize sz = frame_.size().scaled(sample.size(), mode);
        const QRect tgt(QPoint((bw - sz.width()) / 2, (bh - sz.height()) / 2), sz);
        sp.setRenderHint(QPainter::SmoothPixmapTransform);
        sp.drawImage(tgt, frame_);
    } else {
        // 待机态环境微光底图（供液态玻璃折射）
        QRadialGradient bg(QPointF(bw * 0.5, bh * 0.5), qMax(bw, bh) * 0.65);
        bg.setColorAt(0.0, QColor(42, 58, 78, 255));
        bg.setColorAt(0.55, QColor(14, 23, 36, 255));
        bg.setColorAt(1.0, QColor(6, 10, 17, 255));
        sp.fillRect(sample.rect(), bg);
        QRadialGradient goldSpot(QPointF(bw * 0.14, bh * 0.14), bw * 0.25);
        goldSpot.setColorAt(0.0, QColor(232, 163, 61, 80));
        goldSpot.setColorAt(1.0, QColor(232, 163, 61, 0));
        sp.fillRect(sample.rect(), goldSpot);
    }
    sp.end();
    blurredBackdrop_ = ui_theme::fastBlur(sample, 4);
    if (onBackdropUpdated_) onBackdropUpdated_();
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

    QRadialGradient bgGlow(center, qMax(width(), height()) * 0.55);
    bgGlow.setColorAt(0.0, QColor(24, 36, 52, 225));
    bgGlow.setColorAt(0.55, QColor(8, 13, 21, 255));
    bgGlow.setColorAt(1.0, QColor(4, 7, 12, 255));
    painter.fillRect(rect(), bgGlow);

    const int step = qMax(36, qMin(width(), height()) / 16);
    painter.setPen(QPen(QColor(110, 148, 184, 20), 1.0));
    for (int x = (width() / 2) % step; x < width(); x += step)
        painter.drawLine(x, 0, x, height());
    for (int y = (height() / 2) % step; y < height(); y += step)
        painter.drawLine(0, y, width(), y);

    const qreal emblemR = qBound(42.0, qMin(width(), height()) * 0.095, 96.0);
    const qreal pulse = 0.5 + 0.5 * std::sin(pulsePhase_);

    QRadialGradient goldHalo(center, emblemR * 1.9);
    goldHalo.setColorAt(0.0, QColor(232, 163, 61, qRound(38 + 26 * pulse)));
    goldHalo.setColorAt(0.55, QColor(232, 163, 61, qRound(12 + 10 * pulse)));
    goldHalo.setColorAt(1.0, QColor(232, 163, 61, 0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(goldHalo);
    painter.drawEllipse(center, emblemR * 1.9, emblemR * 1.9);

    painter.save();
    painter.translate(center);
    painter.rotate(pulsePhase_ * 12.0);
    QPen outerDash(QColor(232, 163, 61, 115), 2.0, Qt::CustomDashLine);
    outerDash.setDashPattern({3.0, 4.0});
    painter.setPen(outerDash);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QPointF(0, 0), emblemR * 1.18, emblemR * 1.18);
    painter.restore();

    painter.save();
    painter.translate(center);
    painter.rotate(-pulsePhase_ * 18.0);
    painter.setPen(QPen(QColor(154, 186, 218, 70), 1.2, Qt::DashLine));
    painter.drawEllipse(QPointF(0, 0), emblemR * 0.94, emblemR * 0.94);
    painter.restore();

    const int logoPx = qRound(emblemR * 1.25);
    const QPixmap pm = brand::logo(logoPx, devicePixelRatioF());
    if (!pm.isNull()) {
        painter.setOpacity(0.86);
        painter.drawPixmap(QPointF(center.x() - logoPx / 2.0, center.y() - logoPx / 2.0), pm);
        painter.setOpacity(1.0);
    }

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
                         placeholder_ + QStringLiteral("  ·  F5 画面适配  ·  M 地图缩放  ·  H 专注模式"));
    } else {
        painter.setPen(QColor(184, 196, 208));
        painter.drawText(rect().adjusted(0, qRound(emblemR * 0.9), 0, 0), Qt::AlignHCenter | Qt::AlignTop, placeholder_);
    }
}
