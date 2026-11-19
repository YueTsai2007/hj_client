#pragma once
#include <QWidget>
#include <QPixmap>
#include <QHash>
#include <QElapsedTimer>
#include <vector>

class QPainter;

class Map : public QWidget {
public:
    explicit Map(QWidget *parent = nullptr);
    void flip();
    void setRobotPosition(uint id, QPointF position, qreal yaw = qQNaN());
    void clearRobotPositions();
    void expireRobotPositions();
    // 己方/敌方血量（0..3 己方，4..7 敌方，顺序 1/3/4/7），以及“自己”的 id
    void setStatus(uint ownId, const std::vector<uint32_t> &health, const std::vector<uint32_t> &maxHealth);
protected:
    void paintEvent(QPaintEvent *) override;
private:
    void paintSchematic(QPainter &p);
    QPixmap schematic_;
    struct RobotMarker { QPointF position; qreal yaw = qQNaN(); QElapsedTimer age; };
    QHash<uint, RobotMarker> robots_;
    std::vector<uint32_t> health_, maxHealth_;
    uint ownId_ = 0;
    bool flipped_ = false;
};
