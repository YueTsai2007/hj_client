#pragma once
#include <QImage>
#include <QWidget>

class QTimer;

class VideoView : public QWidget {
public:
    explicit VideoView(QWidget *parent = nullptr);
    void setFrame(QImage frame);
    void setPlaceholder(const QString &text);
    void setFillMode(bool expandToFill);
    bool fillMode() const { return fillMode_; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QImage frame_;
    QString placeholder_;
    bool fillMode_ = true;     // 默认铺满全屏（无黑边）；F5 可切回等比完整显示
    qreal pulsePhase_ = 0.0;
    QTimer *standbyTimer_ = nullptr;
};
