#include "floating_panel.h"
#include "brand.h"
#include "ui_theme.h"
#include "video_view.h"

#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QVBoxLayout>

namespace {
class ResizeGrip : public QWidget {
public:
    explicit ResizeGrip(FloatingPanel *panel) : QWidget(panel), panel_(panel) {
        setFixedSize(18, 18);
        setCursor(Qt::SizeFDiagCursor);
        setToolTip(QStringLiteral("拖动缩放"));
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setPen(QPen(QColor(232, 163, 61, 185), 1.5));
        for (int i = 0; i < 3; ++i) {
            const int offset = 4 + i * 4;
            painter.drawLine(width() - offset, height() - 2, width() - 2, height() - offset);
        }
    }
    void mousePressEvent(QMouseEvent *event) override {
        dragStart_ = event->globalPos();
        startSize_ = panel_->size();
        event->accept();
    }
    void mouseMoveEvent(QMouseEvent *event) override {
        if (!(event->buttons() & Qt::LeftButton)) return;
        const QPoint delta = event->globalPos() - dragStart_;
        panel_->resize(qMax(panel_->minimumWidth(), startSize_.width() + delta.x()),
                       qMax(panel_->minimumHeight(), startSize_.height() + delta.y()));
        event->accept();
    }

private:
    FloatingPanel *panel_ = nullptr;
    QPoint dragStart_;
    QSize startSize_;
};
} // namespace

FloatingPanel::FloatingPanel(const QString &title, QWidget *parent) : QFrame(parent) {
    setObjectName("floatingPanel");
    setStyleSheet(QStringLiteral(
        "QFrame#floatingPanel{background:transparent;border:none;}"
        "QLabel{color:#f2f7fc;background:transparent;}"));
    setMinimumSize(320, 220);
    setAttribute(Qt::WA_TranslucentBackground, true);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(16, 12, 16, 12);
    outer->setSpacing(8);

    auto *head = new QHBoxLayout;
    head->setSpacing(8);
    auto *badge = new LogoBadge(26, this);
    badge->setAnimated(false);
    auto *titleLabel = new QLabel(title, this);
    titleLabel->setStyleSheet(QStringLiteral("color:#ffc971;font-size:17px;font-weight:700;"));
    auto *tag = new QLabel(QStringLiteral("HANJIANG // LIQUID GLASS"), this);
    tag->setStyleSheet(QStringLiteral("color:rgba(232,163,61,160);font-size:10px;font-weight:700;"));
    head->addWidget(badge);
    head->addWidget(titleLabel);
    head->addStretch();
    head->addWidget(tag);
    outer->addLayout(head);

    body_ = new QVBoxLayout;
    body_->setContentsMargins(0, 0, 0, 0);
    body_->setSpacing(8);
    outer->addLayout(body_, 1);

    status_ = new QLabel(this);
    status_->setStyleSheet(QStringLiteral("color:#5ce6a0;font-size:13px;font-weight:700;"));
    outer->addWidget(status_);

    auto *grip = new ResizeGrip(this);
    auto *bottom = new QHBoxLayout;
    bottom->addStretch();
    bottom->addWidget(grip);
    outer->addLayout(bottom);
}

void FloatingPanel::showAnimated() {
    show();
    raise();
    auto *effect = qobject_cast<QGraphicsOpacityEffect *>(graphicsEffect());
    if (!effect) {
        effect = new QGraphicsOpacityEffect(this);
        setGraphicsEffect(effect);
    }
    auto *anim = new QPropertyAnimation(effect, "opacity", this);
    anim->setDuration(160);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void FloatingPanel::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(1.0, 1.0, -1.0, -1.0);
    const QPainterPath path = ui_theme::chamferedRect(r, 14.0);

    const QImage backdrop = backdropSource_ ? backdropSource_->blurredBackdrop() : QImage();
    const QSizeF vpSize = backdropSource_ ? QSizeF(backdropSource_->size()) : QSizeF();
    ui_theme::drawLiquidGlass(p, path, r, backdrop, vpSize, QPointF(pos()), ui_theme::kGold, 190);

    p.save();
    p.setClipPath(path);
    const int wm = qBound(80, qMin(width(), height()) / 2, 170);
    p.setOpacity(0.06);
    p.drawPixmap(QPointF(r.right() - wm * 0.8, r.bottom() - wm * 0.8),
                 brand::logo(wm, devicePixelRatioF()));
    p.restore();

    ui_theme::drawCornerBrackets(p, r, 12.0, ui_theme::kGoldLight, 1.8);
}

void FloatingPanel::setStatus(const QString &text, bool ok) {
    if (!status_) return;
    status_->setStyleSheet(QStringLiteral("color:%1;font-size:13px;font-weight:700;")
                               .arg(ok ? QStringLiteral("#5ce6a0") : QStringLiteral("#ff6b77")));
    status_->setText(text);
}

void FloatingPanel::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && event->pos().y() < 42) {
        dragging_ = true;
        dragOffset_ = event->pos();
        raise();
        event->accept();
        return;
    }
    QFrame::mousePressEvent(event);
}

void FloatingPanel::mouseMoveEvent(QMouseEvent *event) {
    if (dragging_) {
        move(mapToParent(event->pos()) - dragOffset_);
        event->accept();
        return;
    }
    QFrame::mouseMoveEvent(event);
}

void FloatingPanel::mouseReleaseEvent(QMouseEvent *event) {
    dragging_ = false;
    QFrame::mouseReleaseEvent(event);
}
