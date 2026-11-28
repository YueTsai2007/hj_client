#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QImage>
#include <QRectF>
#include <QString>
#include <QVector>
#include <QWidget>
#include <array>
#include <functional>

class QStackedWidget;
class QTimer;
class QVariantAnimation;
class VideoView;

// 全屏无黑边作战舞台：画面层、受击暗角层与液态玻璃悬浮 HUD 层铺满整个视口。
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
    qreal heatRatio_ = -1.0;
    qreal hpRatio_ = -1.0;
};

// 液态玻璃斜切角战术卡片（HUD 容器）：实时折射模糊 + 曲面反光 + 双层镜面描边 + L 型角标。
class HudCard : public QWidget {
public:
    explicit HudCard(QWidget *parent = nullptr);
    void setRadius(qreal cut) { cut_ = cut; update(); }
    void setAccent(const QColor &color) { accent_ = color; update(); }
    void setWatermark(bool on) { watermark_ = on; update(); }
    void setOpacityAlpha(int alpha) { alpha_ = qBound(40, alpha, 235); update(); }
    void setBackdropSource(VideoView *source) { backdropSource_ = source; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    qreal cut_ = 10.0;
    int alpha_ = 138;
    QColor accent_;
    bool watermark_ = false;
    VideoView *backdropSource_ = nullptr;
};

// 经典大长条斜切基地总血条（液态玻璃外壳 + 18° 大斜切 + 每 1000 HP 刻度 + 掉血残影尾迹）。
class LargeSkewedHealthBar : public QWidget {
public:
    explicit LargeSkewedHealthBar(bool rightAligned, QWidget *parent = nullptr);
    void setAccent(const QColor &color);
    void setTitle(const QString &title);
    void setHealth(const QString &hpText, qreal ratio, bool valid);
    void setScale(qreal scale) { scale_ = qBound(0.65, scale, 1.4); update(); }
    void setBackdropSource(VideoView *source) { backdropSource_ = source; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    bool right_ = false;
    QColor accent_{"#ff4655"};
    QString title_;
    QString hpText_{"--"};
    qreal target_ = 0.0;
    qreal current_ = 0.0;
    qreal trail_ = 0.0;
    qreal scale_ = 1.0;
    bool valid_ = false;
    QVariantAnimation *valueAnim_ = nullptr;
    QVariantAnimation *trailAnim_ = nullptr;
    VideoView *backdropSource_ = nullptr;
};

// 竞技级斜切残影进度条：平滑插值 + 掉血白色/金色延迟尾迹 + 末端微光。
class TacticalBar : public QWidget {
public:
    explicit TacticalBar(QWidget *parent = nullptr);
    void setAccent(const QColor &color);
    void setRatio(qreal ratio);
    void reset();
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

// 液态玻璃半透明悬浮战场消息流：新事件淡入上浮、带语义色条、随时间渐隐不挡画面。
class FloatingMessageFeed : public QWidget {
public:
    explicit FloatingMessageFeed(QWidget *parent = nullptr);
    void pushMessage(const QString &timeStr, const QString &text, const QColor &accent, bool sticky = false);
    void setMechanismBanner(const QString &text, const QColor &accent);
    void setScale(qreal scale) { scale_ = qBound(0.65, scale, 1.4); update(); }
    void setBackdropSource(VideoView *source) { backdropSource_ = source; }

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
    VideoView *backdropSource_ = nullptr;
};

// 低血量 / 受击边缘微暗角警示层（纯边缘柔光，绝不遮挡中心准星）。
class VignetteOverlay : public QWidget {
public:
    explicit VignetteOverlay(QWidget *parent = nullptr);
    void setLowHealthRatio(qreal hpRatio);
    void triggerHitFlash();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    qreal lowHpIntensity_ = 0.0;
    qreal hitFlash_ = 0.0;
    qreal phase_ = 0.0;
    QTimer *animTimer_ = nullptr;
};

// 本机卡内嵌 8 模块微胶囊自检条（电源/RFID/灯带/小发射/大发射/UWB/装甲/图传）。
class ModuleStrip : public QWidget {
public:
    explicit ModuleStrip(QWidget *parent = nullptr);
    // status: -1=无数据, 0=异常(红闪), 1=正常(绿)
    void setStates(const std::array<int, 8> &states);
    void setScale(qreal scale) { scale_ = qBound(0.65, scale, 1.4); update(); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    std::array<int, 8> states_{{-1, -1, -1, -1, -1, -1, -1, -1}};
    qreal scale_ = 1.0;
    bool blinkOn_ = true;
    QTimer *blinkTimer_ = nullptr;
};
