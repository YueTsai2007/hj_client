#include "combat_view.h"
#include "brand.h"
#include "ui_theme.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QTimer>
#include <QVariantAnimation>
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
    // 全屏无黑边：画面层与悬浮 HUD 层完整铺满视口
    content_ = QRectF(rect());
    stack_->setGeometry(rect());
    hud_->setGeometry(rect());
    hud_->raise();
    if (handler_) handler_(content_);
}

void CombatView::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.fillRect(rect(), ui_theme::kBgDeep);
}

// ---------------- Crosshair ----------------

Crosshair::Crosshair(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
}

void Crosshair::setTelemetry(qreal heatRatio, qreal hpRatio) {
    const qreal h = heatRatio < 0 ? -1.0 : qBound(0.0, heatRatio, 1.0);
    const qreal p = hpRatio < 0 ? -1.0 : qBound(0.0, hpRatio, 1.0);
    if (qFuzzyCompare(h + 2.0, heatRatio_ + 2.0) && qFuzzyCompare(p + 2.0, hpRatio_ + 2.0)) return;
    heatRatio_ = h;
    hpRatio_ = p;
    update();
}

void Crosshair::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QPointF center(width() / 2.0, height() / 2.0);
    const qreal side = qMin(width(), height());
    const qreal arm = side * 0.13;
    const qreal gap = side * 0.045;

    // 细十字准星
    painter.setPen(QPen(QColor(241, 246, 252, 165), 1.6));
    painter.drawLine(QPointF(center.x() - gap - arm, center.y()), QPointF(center.x() - gap, center.y()));
    painter.drawLine(QPointF(center.x() + gap, center.y()), QPointF(center.x() + gap + arm, center.y()));
    painter.drawLine(QPointF(center.x(), center.y() - gap - arm), QPointF(center.x(), center.y() - gap));
    painter.drawLine(QPointF(center.x(), center.y() + gap), QPointF(center.x(), center.y() + gap + arm));

    // 悍匠琥珀金中心点
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 201, 113, 230));
    painter.drawEllipse(center, 2.2, 2.2);

    // 外环双弧仪（左：枪口热量，右：本机血量）+ 悍匠齿轮微刻度
    const qreal arcR = side * 0.36;
    const QRectF arcRect(center.x() - arcR, center.y() - arcR, arcR * 2.0, arcR * 2.0);
    const qreal penW = qMax(2.0, side * 0.018);

    // 轨道底色（左右各 90° 弧）
    painter.setPen(QPen(QColor(255, 255, 255, 36), penW, Qt::SolidLine, Qt::FlatCap));
    painter.drawArc(arcRect, 135 * 16, 90 * 16);   // 左侧弧
    painter.drawArc(arcRect, -45 * 16, 90 * 16);   // 右侧弧

    // 左弧：热量（低=青金，中=琥珀金，高=警示红）
    if (heatRatio_ >= 0.0) {
        QColor heatColor = heatRatio_ > 0.78 ? ui_theme::kRed
                         : heatRatio_ > 0.50 ? ui_theme::kGoldLight
                                             : ui_theme::kCyan;
        heatColor.setAlpha(205);
        painter.setPen(QPen(heatColor, penW, Qt::SolidLine, Qt::FlatCap));
        const int span = qRound(-90.0 * 16.0 * heatRatio_);
        painter.drawArc(arcRect, 225 * 16, span);
    }

    // 右弧：本机血量（低血量变红）
    if (hpRatio_ >= 0.0) {
        QColor hpColor = hpRatio_ < 0.30 ? ui_theme::kRed : ui_theme::kGreen;
        hpColor.setAlpha(200);
        painter.setPen(QPen(hpColor, penW, Qt::SolidLine, Qt::FlatCap));
        const int span = qRound(90.0 * 16.0 * hpRatio_);
        painter.drawArc(arcRect, -45 * 16, span);
    }

    // 顶部与底部微弱齿轮刻度（呼应悍匠机械基因）
    painter.setPen(QPen(QColor(232, 163, 61, 85), 1.2));
    for (int deg : {-105, -90, -75, 75, 90, 105}) {
        const qreal rad = qDegreesToRadians(qreal(deg));
        const QPointF p1(center.x() + std::cos(rad) * (arcR - 2.0), center.y() + std::sin(rad) * (arcR - 2.0));
        const QPointF p2(center.x() + std::cos(rad) * (arcR + 3.5), center.y() + std::sin(rad) * (arcR + 3.5));
        painter.drawLine(p1, p2);
    }
}

// ---------------- HudCard ----------------

HudCard::HudCard(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
}

void HudCard::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(0.75, 0.75, -0.75, -0.75);
    const QPainterPath path = ui_theme::chamferedRect(r, cut_);

    // 玻璃拟态纵向微渐变底
    QLinearGradient fillGrad(r.topLeft(), r.bottomLeft());
    fillGrad.setColorAt(0.0, QColor(14, 23, 36, qMin(235, alpha_ + 18)));
    fillGrad.setColorAt(1.0, QColor(6, 11, 19, alpha_));
    painter.fillPath(path, fillGrad);

    // 可选悍匠淡色水印
    if (watermark_) {
        painter.save();
        painter.setClipPath(path);
        const int sz = qBound(36, qRound(qMin(r.width(), r.height()) * 0.85), 120);
        painter.setOpacity(0.06);
        painter.drawPixmap(QPointF(r.right() - sz * 0.82, r.bottom() - sz * 0.82),
                           brand::logo(sz, devicePixelRatioF()));
        painter.restore();
    }

    // 1px 玻璃细边框
    painter.strokePath(path, QPen(ui_theme::kGlassBorder, 1.0));

    // 顶部霓虹流光强调条
    const QColor glowColor = accent_.isValid() ? accent_ : ui_theme::kGold;
    painter.save();
    painter.setClipPath(path);
    QLinearGradient topBar(r.left(), r.top(), r.right(), r.top());
    QColor c0 = glowColor; c0.setAlpha(210);
    QColor c1 = glowColor; c1.setAlpha(20);
    topBar.setColorAt(0.0, c0);
    topBar.setColorAt(0.65, c0);
    topBar.setColorAt(1.0, c1);
    painter.fillRect(QRectF(r.left(), r.top(), r.width(), 2.2), topBar);
    painter.restore();

    // L 型战术角标
    QColor bracket = glowColor;
    bracket.setAlpha(175);
    ui_theme::drawCornerBrackets(painter, r, qMax(6.0, cut_ * 0.9), bracket, 1.4);
}

// ---------------- TacticalBar ----------------

TacticalBar::TacticalBar(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground, true);
    setMinimumHeight(4);

    valueAnim_ = new QVariantAnimation(this);
    valueAnim_->setDuration(180);
    valueAnim_->setEasingCurve(QEasingCurve::OutCubic);
    connect(valueAnim_, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        current_ = v.toReal();
        update();
    });

    trailAnim_ = new QVariantAnimation(this);
    trailAnim_->setDuration(520);
    trailAnim_->setEasingCurve(QEasingCurve::OutQuad);
    connect(trailAnim_, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        trail_ = v.toReal();
        update();
    });
}

void TacticalBar::setAccent(const QColor &color) {
    if (accent_ == color) return;
    accent_ = color;
    update();
}

void TacticalBar::reset() {
    valid_ = false;
    target_ = current_ = trail_ = 0.0;
    valueAnim_->stop();
    trailAnim_->stop();
    update();
}

void TacticalBar::setRatio(qreal ratio) {
    const qreal clamped = qBound(0.0, ratio, 1.0);
    if (!valid_) {
        valid_ = true;
        target_ = current_ = trail_ = clamped;
        update();
        return;
    }
    if (qFuzzyCompare(clamped + 1.0, target_ + 1.0)) return;
    const qreal prev = current_;
    target_ = clamped;

    valueAnim_->stop();
    valueAnim_->setStartValue(current_);
    valueAnim_->setEndValue(target_);
    valueAnim_->start();

    if (target_ < prev) {
        // 掉血残影延迟追踪
        trailAnim_->stop();
        trailAnim_->setStartValue(qMax(trail_, prev));
        trailAnim_->setEndValue(target_);
        trailAnim_->start();
    } else {
        trailAnim_->stop();
        trail_ = target_;
    }
}

void TacticalBar::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r(rect());
    const qreal sk = qMin(skew_, r.height() * 0.8);

    auto skewedRect = [&](qreal w) {
        QPainterPath path;
        const qreal rw = qBound(0.0, w, r.width());
        if (rw <= 0.5) return path;
        path.moveTo(r.left() + sk, r.top());
        path.lineTo(r.left() + rw, r.top());
        path.lineTo(r.left() + qMax(0.0, rw - sk), r.bottom());
        path.lineTo(r.left(), r.bottom());
        path.closeSubpath();
        return path;
    };

    // 底槽
    p.fillPath(skewedRect(r.width()), QColor(255, 255, 255, 24));
    if (!valid_) return;

    // 掉血白色/金色残影层
    if (trail_ > current_ + 0.004) {
        p.fillPath(skewedRect(r.width() * trail_), QColor(255, 224, 163, 185));
    }

    // 主进度条（横向微发光渐变）
    if (current_ > 0.002) {
        const qreal fw = r.width() * current_;
        QLinearGradient grad(r.left(), 0, r.left() + fw, 0);
        grad.setColorAt(0.0, accent_.darker(120));
        grad.setColorAt(0.85, accent_);
        grad.setColorAt(1.0, accent_.lighter(135));
        p.fillPath(skewedRect(fw), grad);
    }
}

// ---------------- FloatingMessageFeed ----------------

FloatingMessageFeed::FloatingMessageFeed(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    fadeTimer_ = new QTimer(this);
    fadeTimer_->setInterval(250);
    connect(fadeTimer_, &QTimer::timeout, this, [this] {
        if (isVisible() && (!toasts_.isEmpty() || !mechanismText_.isEmpty())) update();
    });
    fadeTimer_->start();
}

void FloatingMessageFeed::pushMessage(const QString &timeStr, const QString &text, const QColor &accent, bool sticky) {
    if (text.isEmpty()) return;
    if (!toasts_.isEmpty() && toasts_.first().text == text) return;
    Toast t;
    t.timeStr = timeStr;
    t.text = text;
    t.accent = accent;
    t.sticky = sticky;
    t.age.start();
    toasts_.prepend(t);
    while (toasts_.size() > 4) toasts_.removeLast();
    update();
}

void FloatingMessageFeed::setMechanismBanner(const QString &text, const QColor &accent) {
    if (mechanismText_ == text && mechanismAccent_ == accent) return;
    mechanismText_ = text;
    mechanismAccent_ = accent;
    update();
}

void FloatingMessageFeed::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const qreal rowH = qMax(24.0, 30.0 * scale_);
    const qreal gap = qMax(4.0, 6.0 * scale_);
    const qreal cut = qMax(5.0, 7.0 * scale_);
    qreal y = height();

    // 从底部向上堆叠渲染半透明消息胶囊，保证紧贴底边且不遮挡中心
    auto drawPill = [&](const QString &prefix, const QString &body, const QColor &accent, qreal opacity) {
        if (opacity <= 0.02 || y - rowH < 0) return;
        y -= rowH;
        const QRectF r(0.5, y + 0.5, width() - 1.0, rowH - 1.0);
        p.save();
        p.setOpacity(opacity);

        const QPainterPath path = ui_theme::chamferedRect(r, cut);
        QLinearGradient bg(r.left(), 0, r.right(), 0);
        bg.setColorAt(0.0, QColor(8, 15, 24, 195));
        bg.setColorAt(0.75, QColor(8, 15, 24, 155));
        bg.setColorAt(1.0, QColor(8, 15, 24, 95));
        p.fillPath(path, bg);
        p.strokePath(path, QPen(QColor(accent.red(), accent.green(), accent.blue(), 110), 1.0));

        // 左侧语义色条
        p.fillRect(QRectF(r.left() + 1.0, r.top() + 3.0, 3.0, r.height() - 6.0), accent);

        QFont f = p.font();
        f.setPixelSize(qMax(10, qRound(12.5 * scale_)));
        f.setBold(true);
        p.setFont(f);
        p.setPen(accent.lighter(120));
        const QRectF prefixRect(r.left() + 12.0 * scale_, r.top(), 66.0 * scale_, r.height());
        p.drawText(prefixRect, Qt::AlignVCenter | Qt::AlignLeft, prefix);

        f.setBold(false);
        p.setFont(f);
        p.setPen(ui_theme::kTextPrimary);
        const QRectF textRect(prefixRect.right() + 6.0 * scale_, r.top(), r.width() - prefixRect.width() - 24.0 * scale_, r.height());
        const QString elided = p.fontMetrics().elidedText(body, Qt::ElideRight, qRound(textRect.width()));
        p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, elided);

        p.restore();
        y -= gap;
    };

    // 最优先显示生效中的战场特殊机制（常驻高亮）
    if (!mechanismText_.isEmpty()) {
        drawPill(QStringLiteral("机制激活"), mechanismText_, mechanismAccent_, 0.96);
    }

    for (const auto &toast : toasts_) {
        const qint64 elapsed = toast.age.elapsed();
        qreal opacity = 0.94;
        if (elapsed < 220) {
            opacity = 0.94 * (elapsed / 220.0);
        } else if (!toast.sticky && elapsed > 8000) {
            // 8 秒后缓慢淡化为半透明静默态（保留 0.36 可读性，不再刺眼）
            opacity = qMax(0.36, 0.94 - (elapsed - 8000) / 5000.0 * 0.58);
        }
        drawPill(toast.timeStr, toast.text, toast.accent, opacity);
    }
}
