#include "combat_view.h"

#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QtMath>

CombatView::CombatView(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground, false);
    setAutoFillBackground(false);

    stack_ = new QStackedWidget(this);
    stack_->setObjectName("combatStack");
    hud_ = new QWidget(this);
    hud_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    hud_->setAttribute(Qt::WA_NoSystemBackground, true);
}

void CombatView::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    const qreal scale = qMin(width() / 16.0, height() / 9.0);
    const qreal w = scale * 16.0;
    const qreal h = scale * 9.0;
    content_ = QRectF((width() - w) / 2.0, (height() - h) / 2.0, w, h);
    stack_->setGeometry(content_.toRect());
    hud_->setGeometry(content_.toRect());
    hud_->raise();
    if (handler_) handler_(content_);
}

void CombatView::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#05080d"));
}

Crosshair::Crosshair(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
}

void Crosshair::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QPointF center(width() / 2.0, height() / 2.0);
    const qreal arm = qMin(width(), height()) * 0.16;
    const qreal gap = qMin(width(), height()) * 0.045;
    painter.setPen(QPen(QColor(255, 255, 255, 150), 2.0));
    painter.drawLine(QPointF(center.x() - gap - arm, center.y()), QPointF(center.x() - gap, center.y()));
    painter.drawLine(QPointF(center.x() + gap, center.y()), QPointF(center.x() + gap + arm, center.y()));
    painter.drawLine(QPointF(center.x(), center.y() - gap - arm), QPointF(center.x(), center.y() - gap));
    painter.drawLine(QPointF(center.x(), center.y() + gap), QPointF(center.x(), center.y() + gap + arm));
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 201, 113, 220));
    painter.drawEllipse(center, 2.5, 2.5);
}

HudCard::HudCard(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
}

void HudCard::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    path.addRoundedRect(r, radius_, radius_);
    painter.fillPath(path, QColor(8, 13, 20, 190));
    painter.strokePath(path, QPen(QColor(148, 176, 205, 56), 1.0));
    if (accent_.isValid()) {
        QPainterPath accent;
        accent.addRoundedRect(QRectF(r.left(), r.top(), r.width(), 3.0 * radius_ / 12.0), radius_, radius_);
        painter.fillPath(accent, accent_);
    }
}
