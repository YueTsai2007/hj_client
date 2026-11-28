#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QRectF>
#include <QString>
#include <QVector>
#include <QWidget>
#include <functional>

class QStackedWidget;
class QTimer;
class QVariantAnimation;

// 全屏无黑边作战舞台：画面层与半透明悬浮 HUD 层铺满整个视口。
class CombatView : public QWidget {
public:
    explicit CombatView(QWidget *parent = nullptr);

    QStackedWidget *stack() const { return stack_; }
    QWidget *hud() const { return hud_; }
    void setLayoutHandler(std::function<void(const QRectF &)> handler) { handler_ = std::move(handler); }

    QRectF contentRect() const { return content_; }
    qreal scale() const { return qMin(content_.width() / 1920.0, content_.height() / 1080.0); }

protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QStackedWidget *stack_ = nullptr;
    QWidget *hud_ = nullptr;
    QRectF content_;
    std::function<void(const QRectF &)> handler_;
};

// 悍匠动态战术准星：中心细十字 + 琥珀金中心点 + 外环双弧仪（左热量 / 右血量，带齿轮刻度）。
class Crosshair : public QWidget {
public:
    explicit Crosshair(QWidget *parent = nullptr);
    void setTelemetry(qreal heatRatio, qreal hpRatio);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    qreal heatRatio_ = -1.0;   // 0..1，负值表示暂无数据
    qreal hpRatio_ = -1.0;     // 0..1，负值表示暂无数据
};

// 半透明斜切角战术卡片（HUD 容器）：玻璃拟态 + L 型瞄准角标 + 顶部阵营/悍匠金流光边 + 可选水印。
class HudCard : public QWidget {
public:
    explicit HudCard(QWidget *parent = nullptr);
    void setRadius(qreal cut) { cut_ = cut; update(); }
    void setAccent(const QColor &color) { accent_ = color; update(); }
    void setWatermark(bool on) { watermark_ = on; update(); }
    void setOpacityAlpha(int alpha) { alpha_ = qBound(40, alpha, 240); update(); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    qreal cut_ = 10.0;
    int alpha_ = 172;
    QColor accent_;
    bool watermark_ = false;
};

// 竞技级斜切残影进度条：平滑插值 + 掉血白色/金色延迟尾迹 + 末端微光。
class TacticalBar : public QWidget {
public:
    explicit TacticalBar(QWidget *parent = nullptr);
    void setAccent(const QColor &color);
    void setRatio(qreal ratio);     // 0..1
    void reset();                   // 无数据占位态
    void setSkew(qreal skew) { skew_ = skew; update(); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QColor accent_{"#ff4655"};
    qreal target_ = 0.0;
    qreal current_ = 0.0;
    qreal trail_ = 0.0;
    qreal skew_ = 4.0;
    bool valid_ = false;
    QVariantAnimation *valueAnim_ = nullptr;
    QVariantAnimation *trailAnim_ = nullptr;
};

// 半透明悬浮战场消息流：新事件淡入上浮、带语义色条、随时间渐隐不挡画面。
class FloatingMessageFeed : public QWidget {
public:
    explicit FloatingMessageFeed(QWidget *parent = nullptr);
    void pushMessage(const QString &timeStr, const QString &text, const QColor &accent, bool sticky = false);
    void setMechanismBanner(const QString &text, const QColor &accent);
    void setScale(qreal scale) { scale_ = qBound(0.65, scale, 1.4); update(); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    struct Toast {
        QString timeStr;
        QString text;
        QColor accent;
        bool sticky = false;
        QElapsedTimer age;
    };
    QVector<Toast> toasts_;
    QString mechanismText_;
    QColor mechanismAccent_{"#e8a33d"};
    qreal scale_ = 1.0;
    QTimer *fadeTimer_ = nullptr;
};
