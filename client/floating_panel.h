#pragma once
#include <QFrame>
#include <QPoint>

class QVBoxLayout;
class QLabel;
class QMouseEvent;

// 可拖动、可缩放的悬浮面板：标题栏拖动，右下角 QSizeGrip 缩放。
class FloatingPanel : public QFrame {
public:
    explicit FloatingPanel(const QString &title, QWidget *parent = nullptr);
    QVBoxLayout *body() const { return body_; }
    void setStatus(const QString &text, bool ok);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QVBoxLayout *body_ = nullptr;
    QLabel *status_ = nullptr;
    bool dragging_ = false;
    QPoint dragOffset_;
};
