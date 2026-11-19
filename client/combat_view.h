#pragma once

#include <QWidget>
#include <QRectF>
#include <functional>

class QStackedWidget;

// 16:9 letterbox 舞台：内容区（画面 + HUD 层）等比缩放居中，两侧/上下留黑边。
class CombatView : public QWidget {
public:
    explicit CombatView(QWidget *parent = nullptr);

    QStackedWidget *stack() const { return stack_; }
    QWidget *hud() const { return hud_; }
    void setLayoutHandler(std::function<void(const QRectF &)> handler) { handler_ = std::move(handler); }

    QRectF contentRect() const { return content_; }
    qreal scale() const { return content_.width() / 1920.0; }

protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QStackedWidget *stack_ = nullptr;
    QWidget *hud_ = nullptr;
    QRectF content_;
    std::function<void(const QRectF &)> handler_;
};

// 中心准星：十字 + 中心点，半透明，不响应鼠标。
class Crosshair : public QWidget {
public:
    explicit Crosshair(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
};

// 半透明圆角卡片（HUD 容器），圆角按 scale 缩放。
class HudCard : public QWidget {
public:
    explicit HudCard(QWidget *parent = nullptr);
    void setRadius(qreal radius) { radius_ = radius; update(); }
    void setAccent(const QColor &color) { accent_ = color; update(); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    qreal radius_ = 12.0;
    QColor accent_;
};
