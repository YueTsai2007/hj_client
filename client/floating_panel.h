#pragma once
#include <QFrame>
#include <QPoint>

class QVBoxLayout;
class QLabel;
class QMouseEvent;
class QPaintEvent;
class VideoView;

// 可拖动、可缩放的悍匠液态玻璃悬浮面板：标题栏拖动，右下角握把缩放，支持平滑淡入。
class FloatingPanel : public QFrame {
public:
    explicit FloatingPanel(const QString &title, QWidget *parent = nullptr);
    QVBoxLayout *body() const { return body_; }
    void setStatus(const QString &text, bool ok);
    void setBackdropSource(VideoView *source) { backdropSource_ = source; }
    void showAnimated();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QVBoxLayout *body_ = nullptr;
    QLabel *status_ = nullptr;
    bool dragging_ = false;
    QPoint dragOffset_;
    VideoView *backdropSource_ = nullptr;
};
