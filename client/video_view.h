#pragma once
#include <QElapsedTimer>
#include <QImage>
#include <QWidget>
#include <functional>

class QTimer;

class VideoView : public QWidget {
public:
    explicit VideoView(QWidget *parent = nullptr);
    void setFrame(QImage frame);
    void setPlaceholder(const QString &text);
    void setFillMode(bool expandToFill);
    bool fillMode() const { return fillMode_; }

    // 共享给液态玻璃悬浮层的 1/8 分辨率模糊底图
    const QImage &blurredBackdrop() const { return blurredBackdrop_; }
    double videoFps() const { return fps_; }
    void setBackdropUpdatedHandler(std::function<void()> cb) { onBackdropUpdated_ = std::move(cb); }

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void rebuildBlurredBackdrop();
    QImage frame_;
    QImage blurredBackdrop_;
    QString placeholder_;
    bool fillMode_ = true;     // 默认铺满全屏（无黑边）；F5 可切回等比完整显示
    qreal pulsePhase_ = 0.0;
    QTimer *standbyTimer_ = nullptr;
    QElapsedTimer fpsClock_;
    int fpsFrames_ = 0;
    double fps_ = 0.0;
    std::function<void()> onBackdropUpdated_;
};
