#pragma once
#include <QWidget>

class Map : public QWidget {
public:
    explicit Map(QWidget *parent = nullptr);
    void flip();
protected:
    void paintEvent(QPaintEvent *) override;
private:
    bool flipped_ = false;
};
