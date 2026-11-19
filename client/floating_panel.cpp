#include "floating_panel.h"

#include <QLabel>
#include <QPainter>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace {
// 用普通 QWidget 做缩放握把：QSizeGrip 只能缩放顶层窗口，对子控件无效
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
        painter.setPen(QPen(QColor(232, 163, 61, 160), 1.5));
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
        "QFrame#floatingPanel{background:rgba(9,14,21,0.97);border:1px solid rgba(232,163,61,0.45);"
        "border-radius:12px;} QLabel{color:#eaf1f7;}"));
    setMinimumSize(320, 220);
    setAttribute(Qt::WA_StyledBackground, true);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(14, 10, 14, 12);
    outer->setSpacing(8);

    auto *head = new QHBoxLayout;
    auto *titleLabel = new QLabel(title, this);
    titleLabel->setStyleSheet(QStringLiteral("color:#ffc971;font-size:18px;font-weight:700;"));
    head->addWidget(titleLabel);
    head->addStretch();
    outer->addLayout(head);

    body_ = new QVBoxLayout;
    body_->setContentsMargins(0, 0, 0, 0);
    body_->setSpacing(8);
    outer->addLayout(body_, 1);

    status_ = new QLabel(this);
    status_->setStyleSheet(QStringLiteral("color:#7fe0a8;font-size:13px;font-weight:700;"));
    outer->addWidget(status_);

    auto *grip = new ResizeGrip(this);
    auto *bottom = new QHBoxLayout;
    bottom->addStretch();
    bottom->addWidget(grip);
    outer->addLayout(bottom);
}

void FloatingPanel::setStatus(const QString &text, bool ok) {
    if (!status_) return;
    status_->setStyleSheet(QStringLiteral("color:%1;font-size:13px;font-weight:700;")
                               .arg(ok ? QStringLiteral("#7fe0a8") : QStringLiteral("#ff8a93")));
    status_->setText(text);
}

void FloatingPanel::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && event->pos().y() < 40) {
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
