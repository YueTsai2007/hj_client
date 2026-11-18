#include "map.h"
#include "brand.h"
#include <QPainter>
#include <QVariant>
#include <QPainterPath>
#include <QPolygonF>
#include <QRadialGradient>

Map::Map(QWidget *parent) : QWidget(parent) { setMinimumSize(185, 115); }
void Map::flip() { flipped_ = !flipped_; schematic_ = {}; update(); }
void Map::setRobotPosition(uint id, QPointF position) {
    const uint number = id % 100;
    if (id > 107 ||
        (number != 1 && number != 3 && number != 4 && number != 7) ||
        !(position.x() >= 0 && position.x() <= 1 && position.y() >= 0 && position.y() <= 1))
        return;
    auto &robot = robots_[id];
    robot.position = position;
    robot.age.start();
    update();
}

void Map::clearRobotPositions() {
    robots_.clear();
    update();
}

void Map::expireRobotPositions() {
    const auto count = robots_.size();
    for (auto it = robots_.begin(); it != robots_.end();) {
        if (it->age.elapsed() >= 3000) it = robots_.erase(it);
        else ++it;
    }
    if (robots_.size() != count) update();
}

void Map::paintEvent(QPaintEvent *) {
    const qreal scale = devicePixelRatioF();
    const QSize pixels = size() * scale;
    if (schematic_.size() != pixels || schematic_.devicePixelRatio() != scale) {
        schematic_ = QPixmap(pixels);
        schematic_.setDevicePixelRatio(scale);
        schematic_.fill(Qt::transparent);
        QPainter cachePainter(&schematic_);
        paintSchematic(cachePainter);
    }
    QPainter painter(this);
    painter.drawPixmap(0, 0, schematic_);
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal fit = qMin(width() / 900.0, height() / 590.0);
    const QPointF center(width() / 2.0, height() / 2.0);
    for (auto it = robots_.cbegin(); it != robots_.cend(); ++it) {
        const auto position = it->position;
        const qreal direction = flipped_ ? -1 : 1;
        const auto point = center + QPointF((position.x() - .5) * 820,
                                           (.5 - position.y()) * 510) * fit * direction;
        painter.setPen(QPen(QColor("#fff4df"), 1.5));
        painter.setBrush(QColor(it.key() < 100 ? "#b93649" : "#2166b4"));
        painter.drawEllipse(point, 11, 11);
        painter.drawText(QRectF(point.x() - 11, point.y() - 11, 22, 22),
                         Qt::AlignCenter, QString::number(it.key() % 100));
    }
}

void Map::paintSchematic(QPainter &p) {
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath clip;
    clip.addRoundedRect(QRectF(rect()), 10, 10);
    p.setClipPath(clip);
    QLinearGradient floor(0, 0, width(), height());
    floor.setColorAt(0, QColor("#0e222b"));
    floor.setColorAt(0.5, QColor("#12232b"));
    floor.setColorAt(1, QColor("#0c1d28"));
    p.fillRect(rect(), floor);

    p.save();
    p.translate(width() / 2.0, height() / 2.0);
    if (flipped_) p.rotate(180);
    const qreal scale = qMin(width() / 900.0, height() / 590.0);
    p.scale(scale, scale);
    p.translate(-450, -295);

    // 网格
    p.setPen(QPen(QColor(72, 97, 106, 55), 1));
    for (int x = 0; x <= 900; x += 36) p.drawLine(x, 0, x, 590);
    for (int y = 0; y <= 590; y += 36) p.drawLine(0, y, 900, y);

    // 场地外框（金色）与中线
    p.setPen(QPen(QColor(232, 163, 61, 170), 3));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(24, 25, 852, 540), 8, 8);
    p.setPen(QPen(QColor(232, 163, 61, 90), 2, Qt::DashLine));
    p.drawLine(450, 25, 450, 565);

    // 高地与障碍
    p.setPen(QPen(QColor(80, 105, 103), 3));
    p.setBrush(QColor("#25383d"));
    p.drawPolygon(QPolygonF(QVector<QPointF>{{24,105},{225,105},{295,32},{376,32},{300,138},{190,138},{150,192},{24,192}}));
    p.drawPolygon(QPolygonF(QVector<QPointF>{{876,485},{675,485},{605,558},{524,558},{600,452},{710,452},{750,398},{876,398}}));
    p.setBrush(QColor("#26363a"));
    p.drawPolygon(QPolygonF(QVector<QPointF>{{25,385},{160,385},{240,490},{360,490},{325,552},{120,552},{25,475}}));
    p.drawPolygon(QPolygonF(QVector<QPointF>{{875,205},{740,205},{660,100},{540,100},{575,38},{780,38},{875,115}}));

    // 中央能量机关菱形
    QLinearGradient core(450, 183, 450, 407);
    core.setColorAt(0, QColor("#2d4a4f")); core.setColorAt(1, QColor("#1b2f35"));
    p.setPen(QPen(QColor(232, 163, 61, 190), 3));
    p.setBrush(core);
    p.drawPolygon(QPolygonF(QVector<QPointF>{{295,295},{450,183},{605,295},{450,407}}));
    p.setPen(QPen(QColor(255, 201, 113, 160), 2));
    p.setBrush(QColor("#0e1c24"));
    p.drawPolygon(QPolygonF(QVector<QPointF>{{364,295},{450,232},{536,295},{450,358}}));

    // 障碍块
    p.setPen(QPen(QColor(124, 147, 120), 2));
    p.setBrush(QColor("#344a45"));
    for (const QRect &r : {QRect(180,95,70,35), QRect(650,460,70,35), QRect(238,235,28,120), QRect(634,235,28,120), QRect(315,400,53,38), QRect(532,152,53,38)}) p.drawRoundedRect(r, 3, 3);

    // 红蓝基地（光晕 + 实心圆）
    const struct { QPointF c; QColor fill, glow; } bases[] = {
        {QPointF(82, 295), QColor("#d85c60"), QColor(255, 90, 103, 140)},
        {QPointF(818, 295), QColor("#38a6d8"), QColor(62, 168, 255, 140)}};
    for (const auto &b : bases) {
        QRadialGradient g(b.c, 70);
        g.setColorAt(0, b.glow); g.setColorAt(1, QColor(0, 0, 0, 0));
        p.setPen(Qt::NoPen); p.setBrush(g);
        p.drawEllipse(b.c, 70, 70);
        p.setPen(QPen(QColor(255, 255, 255, 190), 3));
        p.setBrush(b.fill);
        p.drawEllipse(b.c, 30, 30);
    }
    p.restore();

    // 中央 Logo 水印
    const int mark = qMin(width(), height()) * 2 / 3;
    p.setOpacity(0.08);
    p.drawPixmap(QPointF((width() - mark) / 2.0, (height() - mark) / 2.0), brand::logo(mark, devicePixelRatioF()));
    p.setOpacity(1.0);

    // 金色四角标记
    p.setPen(QPen(QColor(brand::kGoldLight), 2, Qt::SolidLine, Qt::SquareCap));
    const int L = 20, m = 6;
    p.drawLine(m, m + L, m, m); p.drawLine(m, m, m + L, m);
    p.drawLine(width() - m, m + L, width() - m, m); p.drawLine(width() - m, m, width() - m - L, m);
    p.drawLine(m, height() - m - L, m, height() - m); p.drawLine(m, height() - m, m + L, height() - m);
    p.drawLine(width() - m, height() - m - L, width() - m, height() - m); p.drawLine(width() - m, height() - m, width() - m - L, height() - m);

    // 底部说明胶囊
    const QString note = property("demoMode").toBool() ? QStringLiteral("模拟坐标") : QStringLiteral("坐标待校准");
    QFont f = font(); f.setPointSize(9); p.setFont(f);
    const QRect tr = p.fontMetrics().boundingRect(note).adjusted(-10, -4, 10, 4);
    const QRect box(14, height() - tr.height() - 14, tr.width(), tr.height());
    p.setPen(QPen(QColor(232, 163, 61, 90), 1));
    p.setBrush(QColor(8, 14, 21, 190));
    p.drawRoundedRect(box, box.height() / 2.0, box.height() / 2.0);
    p.setPen(QColor("#ffd9a0"));
    p.drawText(box, Qt::AlignCenter, note);
}
