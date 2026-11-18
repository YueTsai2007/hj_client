#pragma once
#include <QImage>
#include <QWidget>

class VideoView : public QWidget {
public:
    explicit VideoView(QWidget *parent = nullptr);
    void setFrame(QImage frame);
    void setPlaceholder(const QString &text);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QImage frame_;
    QString placeholder_;
};
