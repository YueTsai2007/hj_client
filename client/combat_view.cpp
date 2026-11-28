#include "combat_view.h"
#include "brand.h"
#include "ui_theme.h"
#include "video_view.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QTimer>
#include <QVariantAnimation>
#include <QtMath>
#include <cmath>

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

    painter.setPen(QPen(QColor(242, 247, 252, 170), 1.6));
    painter.drawLine(QPointF(center.x() - gap - arm, center.y()), QPointF(center.x() - gap, center.y()));
    painter.drawLine(QPointF(center.x() + gap, center.y()), QPointF(center.x() + gap + arm, center.y()));
    painter.drawLine(QPointF(center.x(), center.y() - gap - arm), QPointF(center.x(), center.y() - gap));
    painter.drawLine(QPointF(center.x(), center.y() + gap), QPointF(center.x(), center.y() + gap + arm));

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 201, 113, 235));
    painter.drawEllipse(center, 2.2, 2.2);

    const qreal arcR = side * 0.36;
    const QRectF arcRect(center.x() - arcR, center.y() - arcR, arcR * 2.0, arcR * 2.0);
    const qreal penW = qMax(2.2, side * 0.02);

    painter.setPen(QPen(QColor(255, 255, 255, 42), penW, Qt::SolidLine, Qt::FlatCap));
    painter.drawArc(arcRect, 135 * 16, 90 * 16);
    painter.drawArc(arcRect, -45 * 16, 90 * 16);

    if (heatRatio_ >= 0.0) {
        QColor heatColor = heatRatio_ > 0.78 ? ui_theme::kRed
                         : heatRatio_ > 0.50 ? ui_theme::kGoldLight
                                             : ui_theme::kCyan;
        heatColor.setAlpha(215);
        painter.setPen(QPen(heatColor, penW, Qt::SolidLine, Qt::FlatCap));
        painter.drawArc(arcRect, 225 * 16, qRound(-90.0 * 16.0 * heatRatio_));
    }

    if (hpRatio_ >= 0.0) {
        QColor hpColor = hpRatio_ < 0.30 ? ui_theme::kRed : ui_theme::kGreen;
        hpColor.setAlpha(210);
        painter.setPen(QPen(hpColor, penW, Qt::SolidLine, Qt::FlatCap));
        painter.drawArc(arcRect, -45 * 16, qRound(90.0 * 16.0 * hpRatio_));
    }

    // 顶部与底部悍匠齿轮微刻度
    painter.setPen(QPen(QColor(232, 163, 61, 95), 1.3));
    for (int deg : {-105, -90, -75, 75, 90, 105}) {
        const qreal rad = qDegreesToRadians(qreal(deg));
        painter.drawLine(QPointF(center.x() + std::cos(rad) * (arcR - 2.0), center.y() + std::sin(rad) * (arcR - 2.0)),
                         QPointF(center.x() + std::cos(rad) * (arcR + 3.8), center.y() + std::sin(rad) * (arcR + 3.8)));
    }
}

// ---------------- HudCard (Liquid Glass) ----------------

HudCard::HudCard(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
}

void HudCard::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(0.75, 0.75, -0.75, -0.75);
    const QPainterPath path = ui_theme::chamferedRect(r, cut_);

    const QImage backdrop = backdropSource_ ? backdropSource_->blurredBackdrop() : QImage();
    const QSizeF vpSize = backdropSource_ ? QSizeF(backdropSource_->size()) : QSizeF();
    const QPointF posInVp = QPointF(pos());
    const QColor accent = accent_.isValid() ? accent_ : ui_theme::kGold;

    ui_theme::drawLiquidGlass(painter, path, r, backdrop, vpSize, posInVp, accent, alpha_);

    if (watermark_) {
        painter.save();
        painter.setClipPath(path);
        const int sz = qBound(42, qRound(qMin(r.width(), r.height()) * 0.85), 120);
        painter.setOpacity(0.065);
        painter.drawPixmap(QPointF(r.right() - sz * 0.82, r.bottom() - sz * 0.82),
                           brand::logo(sz, devicePixelRatioF()));
        painter.restore();
    }

    // 顶部细流光强调线 + L 型角标
    painter.save();
    painter.setClipPath(path);
    QLinearGradient topBar(r.left(), r.top(), r.right(), r.top());
    QColor c0 = accent; c0.setAlpha(215);
    QColor c1 = accent; c1.setAlpha(25);
    topBar.setColorAt(0.0, c0);
    topBar.setColorAt(0.65, c0);
    topBar.setColorAt(1.0, c1);
    painter.fillRect(QRectF(r.left(), r.top(), r.width(), 2.0), topBar);
    painter.restore();

    QColor bracket = accent;
    bracket.setAlpha(185);
    ui_theme::drawCornerBrackets(painter, r, qMax(6.0, cut_ * 0.88), bracket, 1.4);
}

// ---------------- LargeSkewedHealthBar (Unified HanJiang Liquid Glass Wing) ----------------

LargeSkewedHealthBar::LargeSkewedHealthBar(bool rightAligned, QWidget *parent)
    : QWidget(parent), right_(rightAligned) {
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);

    valueAnim_ = new QVariantAnimation(this);
    valueAnim_->setDuration(200);
    valueAnim_->setEasingCurve(QEasingCurve::OutCubic);
    connect(valueAnim_, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        current_ = v.toReal();
        update();
    });

    trailAnim_ = new QVariantAnimation(this);
    trailAnim_->setDuration(560);
    trailAnim_->setEasingCurve(QEasingCurve::OutQuad);
    connect(trailAnim_, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        trail_ = v.toReal();
        update();
    });

    gearTimer_ = new QTimer(this);
    gearTimer_->setInterval(55);
    connect(gearTimer_, &QTimer::timeout, this, [this] {
        if (!hanjiangEmblem_ || !isVisible()) return;
        gearAngle_ += 0.65;
        if (gearAngle_ >= 360.0) gearAngle_ -= 360.0;
        update();
    });
}

void LargeSkewedHealthBar::setHanjiangEmblem(bool enabled, const QString &roleSub) {
    hanjiangEmblem_ = enabled;
    roleSub_ = roleSub;
    if (enabled) gearTimer_->start();
    else gearTimer_->stop();
    update();
}

void LargeSkewedHealthBar::setStatusBadge(const QString &topText, const QString &bottomText) {
    if (badgeTop_ == topText && badgeBottom_ == bottomText) return;
    badgeTop_ = topText;
    badgeBottom_ = bottomText;
    update();
}

void LargeSkewedHealthBar::setAccent(const QColor &color) {
    if (accent_ == color) return;
    accent_ = color;
    update();
}

void LargeSkewedHealthBar::setTitle(const QString &title) {
    if (title_ == title) return;
    title_ = title;
    update();
}

void LargeSkewedHealthBar::setHealth(const QString &hpText, qreal ratio, bool valid) {
    hpText_ = hpText;
    if (!valid) {
        valid_ = false;
        target_ = current_ = trail_ = 0.0;
        valueAnim_->stop();
        trailAnim_->stop();
        update();
        return;
    }
    const qreal clamped = qBound(0.0, ratio, 1.0);
    if (!valid_) {
        valid_ = true;
        target_ = current_ = trail_ = clamped;
        update();
        return;
    }
    if (qFuzzyCompare(clamped + 1.0, target_ + 1.0)) {
        update();
        return;
    }
    const qreal prev = current_;
    target_ = clamped;
    valueAnim_->stop();
    valueAnim_->setStartValue(current_);
    valueAnim_->setEndValue(target_);
    valueAnim_->start();

    if (target_ < prev) {
        trailAnim_->stop();
        trailAnim_->setStartValue(qMax(trail_, prev));
        trailAnim_->setEndValue(target_);
        trailAnim_->start();
    } else {
        trailAnim_->stop();
        trail_ = target_;
    }
}

void LargeSkewedHealthBar::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(1.0, 1.0, -1.0, -1.0);
    const qreal cut = qMin(r.height() * 0.44, 28.0 * scale_);
    const qreal outerCut = qMin(r.height() * 0.22, 12.0 * scale_);

    // 统一的一体化大斜切翼轮廓：外侧微切角，内侧大斜切向中央计时核汇聚
    QPolygonF poly;
    if (!right_) {
        poly << r.topLeft()
             << QPointF(r.right() - cut, r.top())
             << r.bottomRight()
             << QPointF(r.left() + outerCut, r.bottom())
             << QPointF(r.left(), r.bottom() - outerCut);
    } else {
        poly << QPointF(r.left() + cut, r.top())
             << r.topRight()
             << QPointF(r.right(), r.bottom() - outerCut)
             << QPointF(r.right() - outerCut, r.bottom())
             << r.bottomLeft();
    }
    QPainterPath shape;
    shape.addPolygon(poly);
    shape.closeSubpath();

    // 1. 整体液态玻璃折射底盘（悍匠徽章区与血条区共享同一块玻璃，彻底消除割裂）
    const QImage backdrop = backdropSource_ ? backdropSource_->blurredBackdrop() : QImage();
    const QSizeF vpSize = backdropSource_ ? QSizeF(backdropSource_->size()) : QSizeF();
    ui_theme::drawLiquidGlass(p, shape, r, backdrop, vpSize, QPointF(pos()), accent_, 132);

    // 2. 划分「内嵌徽标翼区」与「斜切血条区」
    const qreal capW = hanjiangEmblem_ ? qMin(r.width() * 0.30, 198.0 * scale_)
                     : (!badgeTop_.isEmpty() ? qMin(r.width() * 0.24, 166.0 * scale_) : 0.0);
    const qreal slant = 14.0 * scale_;
    const QRectF barZone = !right_
        ? QRectF(r.left() + capW, r.top(), r.width() - capW, r.height())
        : QRectF(r.left(), r.top(), r.width() - capW, r.height());

    // 构造血条区域独立斜切裁剪路径（内侧与徽标区以斜闪电线衔接！）
    QPainterPath barClip;
    if (!right_) {
        QPolygonF bp;
        bp << QPointF(barZone.left() + slant, r.top())
           << QPointF(r.right() - cut, r.top())
           << r.bottomRight()
           << QPointF(barZone.left() - slant * 0.35, r.bottom());
        barClip.addPolygon(bp);
    } else {
        QPolygonF bp;
        bp << QPointF(r.left() + cut, r.top())
           << QPointF(barZone.right() - slant, r.top())
           << QPointF(barZone.right() + slant * 0.35, r.bottom())
           << r.bottomLeft();
        barClip.addPolygon(bp);
    }
    barClip.closeSubpath();
    barClip = barClip.intersected(shape);

    // 3. 血条区填充 + 掉血残影 + 1000 HP 战术刻度线
    p.save();
    p.setClipPath(barClip);
    if (valid_) {
        const auto barRect = [&](qreal frac) {
            const qreal fw = barZone.width() * qBound(0.0, frac, 1.0);
            return right_ ? QRectF(barZone.right() - fw, r.top(), fw, r.height())
                          : QRectF(barZone.left() - slant * 0.35, r.top(), fw + slant * 0.35, r.height());
        };
        if (trail_ > current_ + 0.004) {
            p.fillRect(barRect(trail_), QColor(255, 224, 163, 175));
        }
        if (current_ > 0.002) {
            const QRectF fill = barRect(current_);
            QLinearGradient grad(right_ ? fill.right() : fill.left(), r.top(),
                                 right_ ? fill.left() : fill.right(), r.bottom());
            QColor cStart = accent_.darker(135); cStart.setAlpha(205);
            QColor cMid = accent_; cMid.setAlpha(218);
            QColor cTip = accent_.lighter(132); cTip.setAlpha(238);
            grad.setColorAt(0.0, cStart);
            grad.setColorAt(0.78, cMid);
            grad.setColorAt(1.0, cTip);
            p.fillRect(fill, grad);

            QLinearGradient fillSheen(fill.topLeft(), QPointF(fill.left(), fill.top() + fill.height() * 0.45));
            fillSheen.setColorAt(0.0, QColor(255, 255, 255, 62));
            fillSheen.setColorAt(1.0, QColor(255, 255, 255, 0));
            p.fillRect(QRectF(fill.left(), fill.top(), fill.width(), fill.height() * 0.45), fillSheen);
        }
    }
    // 5 段 1000-HP 刻度线
    p.setPen(QPen(QColor(255, 255, 255, 36), 1.0));
    for (int i = 1; i < 5; ++i) {
        const qreal x = barZone.left() + barZone.width() * i / 5.0;
        const qreal sl = right_ ? -cut * 0.42 : cut * 0.42;
        p.drawLine(QPointF(x - sl * 0.5, r.top() + 4), QPointF(x + sl * 0.5, r.bottom() - 4));
    }
    p.restore();

    // 4. 左侧内嵌悍匠旋转齿轮徽标翼（与血条无缝斜线融合）
    if (hanjiangEmblem_ && !right_) {
        p.save();
        p.setClipPath(shape);
        // 悍匠锻造金柔光从最左端向血条自然过渡
        QLinearGradient goldGlow(r.left(), 0, r.left() + capW + slant, 0);
        goldGlow.setColorAt(0.0, QColor(232, 163, 61, 78));
        goldGlow.setColorAt(0.75, QColor(232, 163, 61, 24));
        goldGlow.setColorAt(1.0, QColor(232, 163, 61, 0));
        p.fillRect(QRectF(r.left(), r.top(), capW + slant, r.height()), goldGlow);

        // 旋转齿轮环 + 悍匠 Logo
        const qreal badgeR = qMin(r.height() * 0.36, 25.0 * scale_);
        const QPointF c(r.left() + 14.0 * scale_ + badgeR, r.center().y());
        QRadialGradient halo(c, badgeR * 1.25);
        halo.setColorAt(0.0, QColor(232, 163, 61, 115));
        halo.setColorAt(0.75, QColor(18, 13, 6, 190));
        halo.setColorAt(1.0, QColor(8, 12, 18, 0));
        p.setPen(Qt::NoPen);
        p.setBrush(halo);
        p.drawEllipse(c, badgeR * 1.15, badgeR * 1.15);

        p.save();
        p.translate(c);
        p.rotate(gearAngle_);
        QPen dash(QColor(255, 201, 113, 220), qMax(1.6, 2.0 * scale_), Qt::CustomDashLine);
        dash.setDashPattern({2.2, 2.6});
        p.setPen(dash);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(0, 0), badgeR, badgeR);
        p.restore();

        const int logoPx = qMax(16, qRound(badgeR * 1.38));
        const QPixmap pm = brand::logo(logoPx, devicePixelRatioF());
        if (!pm.isNull())
            p.drawPixmap(QPointF(c.x() - logoPx / 2.0, c.y() - logoPx / 2.0), pm);

        // 悍匠文字标识
        const QRectF txtBox(c.x() + badgeR + 9.0 * scale_, r.top() + 8.0 * scale_,
                            capW - (badgeR * 2.0 + 22.0 * scale_), r.height() - 16.0 * scale_);
        QFont fTitle = p.font();
        fTitle.setPixelSize(qMax(11, qRound(15.0 * scale_)));
        fTitle.setBold(true);
        p.setFont(fTitle);
        p.setPen(ui_theme::kGoldLight);
        p.drawText(txtBox, Qt::AlignLeft | Qt::AlignTop, QStringLiteral("悍匠 · HJ"));

        QFont fSub = p.font();
        fSub.setPixelSize(qMax(9, qRound(10.5 * scale_)));
        fSub.setBold(false);
        p.setFont(fSub);
        p.setPen(ui_theme::kTextSecondary);
        p.drawText(txtBox, Qt::AlignLeft | Qt::AlignBottom, roleSub_);

        // 闪电斜切金色分界光条（将徽标区与血条区自然衔接）
        p.setPen(QPen(QColor(255, 201, 113, 210), 2.0));
        p.drawLine(QPointF(barZone.left() + slant, r.top() + 2),
                   QPointF(barZone.left() - slant * 0.35, r.bottom() - 2));
        p.restore();
    }

    // 5. 右侧对称内嵌链路/FPS 微翼区
    if (right_ && !badgeTop_.isEmpty()) {
        p.save();
        p.setClipPath(shape);
        const QRectF rightCap(barZone.right() + 4.0 * scale_, r.top() + 8.0 * scale_,
                              capW - 18.0 * scale_, r.height() - 16.0 * scale_);
        QFont fTop = p.font();
        fTop.setPixelSize(qMax(10, qRound(12.0 * scale_)));
        fTop.setBold(true);
        p.setFont(fTop);
        p.setPen(ui_theme::kCyan);
        p.drawText(rightCap, Qt::AlignRight | Qt::AlignTop, badgeTop_);

        QFont fBot = p.font();
        fBot.setPixelSize(qMax(9, qRound(10.5 * scale_)));
        fBot.setBold(false);
        p.setFont(fBot);
        p.setPen(ui_theme::kTextSecondary);
        p.drawText(rightCap, Qt::AlignRight | Qt::AlignBottom, badgeBottom_);

        p.setPen(QPen(QColor(60, 232, 255, 165), 1.8));
        p.drawLine(QPointF(barZone.right() - slant, r.top() + 2),
                   QPointF(barZone.right() + slant * 0.35, r.bottom() - 2));
        p.restore();
    }

    // 6. 统一外描边（左翼带悍匠金向阵营色渐变，真正一体化）
    QLinearGradient borderGrad(r.left(), 0, r.right(), 0);
    if (!right_ && hanjiangEmblem_) {
        borderGrad.setColorAt(0.0, ui_theme::kGoldLight);
        borderGrad.setColorAt(0.28, ui_theme::kGold);
        borderGrad.setColorAt(0.45, accent_);
        borderGrad.setColorAt(1.0, accent_.lighter(120));
    } else if (right_ && !badgeTop_.isEmpty()) {
        borderGrad.setColorAt(0.0, accent_.lighter(120));
        borderGrad.setColorAt(0.65, accent_);
        borderGrad.setColorAt(1.0, ui_theme::kCyan);
    } else {
        borderGrad.setColorAt(0.0, accent_);
        borderGrad.setColorAt(1.0, accent_.lighter(120));
    }
    p.strokePath(shape, QPen(QBrush(borderGrad), 1.6));

    // 7. 血量文字排版（位于血条区内部，左右对称）
    const QRectF textBox = !right_
        ? barZone.adjusted(slant + 14.0 * scale_, 5.0 * scale_, -cut - 10.0 * scale_, -5.0 * scale_)
        : barZone.adjusted(cut + 10.0 * scale_, 5.0 * scale_, -slant - 14.0 * scale_, -5.0 * scale_);
    const Qt::Alignment align = right_ ? Qt::AlignRight : Qt::AlignLeft;

    QFont capFont = p.font();
    capFont.setPixelSize(qMax(9, qRound(11.0 * scale_)));
    capFont.setBold(true);
    p.setFont(capFont);
    p.setPen(QColor(242, 247, 252, 205));
    p.drawText(textBox, align | Qt::AlignTop, title_);

    QFont hpFont = p.font();
    hpFont.setPixelSize(qMax(18, qRound(30.0 * scale_)));
    hpFont.setBold(true);
    p.setFont(hpFont);
    p.setPen(QColor(0, 0, 0, 145));
    p.drawText(textBox.translated(1.2, 1.5), align | Qt::AlignBottom, hpText_ + QStringLiteral(" HP"));
    p.setPen(ui_theme::kTextPrimary);
    p.drawText(textBox, align | Qt::AlignBottom, hpText_ + QStringLiteral(" HP"));
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

    p.fillPath(skewedRect(r.width()), QColor(255, 255, 255, 28));
    if (!valid_) return;

    if (trail_ > current_ + 0.004) {
        p.fillPath(skewedRect(r.width() * trail_), QColor(255, 224, 163, 185));
    }

    if (current_ > 0.002) {
        const qreal fw = r.width() * current_;
        QLinearGradient grad(r.left(), 0, r.left() + fw, 0);
        grad.setColorAt(0.0, accent_.darker(120));
        grad.setColorAt(0.85, accent_);
        grad.setColorAt(1.0, accent_.lighter(135));
        p.fillPath(skewedRect(fw), grad);
    }
}

// ---------------- FloatingMessageFeed (Liquid Glass Toasts) ----------------

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

    const QImage backdrop = backdropSource_ ? backdropSource_->blurredBackdrop() : QImage();
    const QSizeF vpSize = backdropSource_ ? QSizeF(backdropSource_->size()) : QSizeF();
    const qreal rowH = qMax(24.0, 31.0 * scale_);
    const qreal gap = qMax(4.0, 6.0 * scale_);
    const qreal cut = qMax(5.0, 7.0 * scale_);
    qreal y = height();

    auto drawPill = [&](const QString &prefix, const QString &body, const QColor &accent, qreal opacity) {
        if (opacity <= 0.02 || y - rowH < 0) return;
        y -= rowH;
        const QRectF r(0.5, y + 0.5, width() - 1.0, rowH - 1.0);
        p.save();
        p.setOpacity(opacity);

        const QPainterPath path = ui_theme::chamferedRect(r, cut);
        ui_theme::drawLiquidGlass(p, path, r, backdrop, vpSize, QPointF(pos()), accent, 148);

        // 左侧语义霓虹竖条
        p.fillRect(QRectF(r.left() + 1.5, r.top() + 3.5, 3.0, r.height() - 7.0), accent);

        QFont f = p.font();
        f.setPixelSize(qMax(10, qRound(12.5 * scale_)));
        f.setBold(true);
        p.setFont(f);
        p.setPen(accent.lighter(125));
        const QRectF prefixRect(r.left() + 12.0 * scale_, r.top(), 68.0 * scale_, r.height());
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

    if (!mechanismText_.isEmpty()) {
        drawPill(QStringLiteral("机制激活"), mechanismText_, mechanismAccent_, 0.98);
    }

    for (const auto &toast : toasts_) {
        const qint64 elapsed = toast.age.elapsed();
        qreal opacity = 0.95;
        if (elapsed < 220) {
            opacity = 0.95 * (elapsed / 220.0);
        } else if (!toast.sticky && elapsed > 8000) {
            opacity = qMax(0.38, 0.95 - (elapsed - 8000) / 5000.0 * 0.57);
        }
        drawPill(toast.timeStr, toast.text, toast.accent, opacity);
    }
}

// ---------------- VignetteOverlay (Low-HP / Hit Edge Pulse) ----------------

VignetteOverlay::VignetteOverlay(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    animTimer_ = new QTimer(this);
    animTimer_->setInterval(40);
    connect(animTimer_, &QTimer::timeout, this, [this] {
        bool active = false;
        if (hitFlash_ > 0.01) {
            hitFlash_ = qMax(0.0, hitFlash_ - 0.08);
            active = true;
        }
        if (lowHpIntensity_ > 0.01) {
            phase_ += 0.14;
            if (phase_ > 628.318) phase_ -= 628.318;
            active = true;
        }
        if (active) update();
        else animTimer_->stop();
    });
}

void VignetteOverlay::setLowHealthRatio(qreal hpRatio) {
    const qreal next = (hpRatio >= 0.0 && hpRatio < 0.30) ? qBound(0.15, (0.30 - hpRatio) / 0.30, 1.0) : 0.0;
    if (qFuzzyCompare(next + 1.0, lowHpIntensity_ + 1.0)) return;
    lowHpIntensity_ = next;
    if (lowHpIntensity_ > 0.01 && !animTimer_->isActive()) animTimer_->start();
    update();
}

void VignetteOverlay::triggerHitFlash() {
    hitFlash_ = 1.0;
    if (!animTimer_->isActive()) animTimer_->start();
    update();
}

void VignetteOverlay::paintEvent(QPaintEvent *) {
    const qreal pulse = lowHpIntensity_ > 0.01 ? lowHpIntensity_ * (0.55 + 0.45 * std::sin(phase_)) : 0.0;
    const qreal strength = qMax(pulse * 0.65, hitFlash_ * 0.85);
    if (strength <= 0.01) return;

    QPainter p(this);
    const QPointF center(width() / 2.0, height() / 2.0);
    const qreal radius = std::hypot(width(), height()) * 0.52;
    QRadialGradient grad(center, radius);
    grad.setColorAt(0.0, QColor(255, 45, 65, 0));
    grad.setColorAt(0.72, QColor(255, 45, 65, 0));
    grad.setColorAt(1.0, QColor(255, 45, 65, qRound(135 * strength)));
    p.fillRect(rect(), grad);
}

// ---------------- ModuleStrip (Embedded 8 Micro-Capsules) ----------------

ModuleStrip::ModuleStrip(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    blinkTimer_ = new QTimer(this);
    blinkTimer_->setInterval(450);
    connect(blinkTimer_, &QTimer::timeout, this, [this] {
        bool hasFault = false;
        for (int s : states_) if (s == 0) hasFault = true;
        if (!hasFault) return;
        blinkOn_ = !blinkOn_;
        update();
    });
    blinkTimer_->start();
}

void ModuleStrip::setStates(const std::array<int, 8> &states) {
    if (states_ == states) return;
    states_ = states;
    update();
}

void ModuleStrip::paintEvent(QPaintEvent *) {
    static const char *labels[8] = {"电源", "RFID", "灯带", "小发", "大发", "UWB", "装甲", "图传"};
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const qreal gap = qMax(3.0, 4.0 * scale_);
    const qreal cellW = (width() - gap * 7.0) / 8.0;
    const qreal h = height();

    QFont f = p.font();
    f.setPixelSize(qMax(8, qRound(9.5 * scale_)));
    f.setBold(true);
    p.setFont(f);

    for (int i = 0; i < 8; ++i) {
        const QRectF cell(i * (cellW + gap), 0.5, cellW, h - 1.0);
        const int st = states_[static_cast<size_t>(i)];
        QColor bg, border, txt;
        if (st == 1) {
            bg = QColor(92, 230, 160, 26);
            border = QColor(92, 230, 160, 110);
            txt = QColor(145, 240, 190, 215);
        } else if (st == 0) {
            bg = QColor(255, 70, 85, blinkOn_ ? 145 : 65);
            border = QColor(255, 70, 85, 235);
            txt = QColor(255, 240, 242, 255);
        } else {
            bg = QColor(255, 255, 255, 14);
            border = QColor(154, 186, 218, 45);
            txt = QColor(125, 145, 165, 175);
        }
        p.setPen(QPen(border, 0.9));
        p.setBrush(bg);
        p.drawRoundedRect(cell, 3.0, 3.0);
        p.setPen(txt);
        p.drawText(cell, Qt::AlignCenter, QString::fromUtf8(labels[i]));
    }
}
