#include "video_view.h"
#include <QPainter>
#include <utility>

VideoView::VideoView(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(180, 110);
    setPlaceholder(QStringLiteral("等待图传首帧"));
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

void VideoView::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#080b10"));
    if (frame_.isNull()) {
        painter.setPen(QColor("#b8c4d0"));
        painter.drawText(rect(), Qt::AlignCenter, placeholder_);
        return;
    }
    const QSize size = frame_.size().scaled(this->size(), Qt::KeepAspectRatio);
    const QRect target(QPoint((width() - size.width()) / 2, (height() - size.height()) / 2), size);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(target, frame_);
}
