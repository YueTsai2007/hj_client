#pragma once
#include <QWidget>
#include <QPixmap>
#include <QHash>
#include <QElapsedTimer>

class QPainter;

class Map : public QWidget {
public:
    explicit Map(QWidget *parent = nullptr);
    void flip();
    void setRobotPosition(uint id, QPointF position);
    void clearRobotPositions();
    void expireRobotPositions();
protected:
    void paintEvent(QPaintEvent *) override;
private:
    void paintSchematic(QPainter &p);
    QPixmap schematic_;
    struct RobotMarker { QPointF position; QElapsedTimer age; };
    QHash<uint, RobotMarker> robots_;
    bool flipped_ = false;
};
