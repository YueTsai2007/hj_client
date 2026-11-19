#include "map.h"
#include "brand.h"
#include <QPainter>
#include <QVariant>
#include <QPainterPath>
#include <QPolygonF>
#include <QRadialGradient>
#include <cmath>

Map::Map(QWidget *parent) : QWidget(parent) {
    setMinimumSize(185, 115);
    // 上赛场后可用环境变量微调底图偏置（单位：画布像素，900×482 坐标系）
    calX_ = qEnvironmentVariable("HJ_MAP_CAL_X", "0").toDouble();
    calY_ = qEnvironmentVariable("HJ_MAP_CAL_Y", "0").toDouble();
    calW_ = qEnvironmentVariable("HJ_MAP_CAL_W", "0").toDouble();
    calH_ = qEnvironmentVariable("HJ_MAP_CAL_H", "0").toDouble();
}
void Map::flip() { flipped_ = !flipped_; schematic_ = {}; update(); }
void Map::setRobotPosition(uint id, QPointF position, qreal yaw) {
    const uint number = id % 100;
    if (id > 107 ||
        (number != 1 && number != 3 && number != 4 && number != 7) ||
        !(position.x() >= 0 && position.x() <= 1 && position.y() >= 0 && position.y() <= 1))
        return;
    auto &robot = robots_[id];
    robot.position = position;
    robot.yaw = yaw;
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

void Map::setStatus(uint ownId, const std::vector<uint32_t> &health, const std::vector<uint32_t> &maxHealth) {
    ownId_ = ownId;
    health_ = health;
    maxHealth_ = maxHealth;
    update();
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
    const qreal fit = qMin(width() / 900.0, height() / 482.0);
    // 几何映射：画布内场地矩形 (20,11) 860×460 精确对应 28m×15m；
    // calX_/calY_ 只用于“上赛场后”对齐底图与坐标，平时为 0。
    const QPointF fieldOrigin(20 * fit + (width() - 900 * fit) / 2.0,
                              11 * fit + (height() - 482 * fit) / 2.0);
    const qreal fieldWidth = (860 + calW_) * fit;
    const qreal fieldHeight = (460 + calH_) * fit;
    const QList<int> numberOrder{1, 3, 4, 7};
    // 标记尺寸随控件大小缩放：小地图上不再是大圆点
    const qreal radius = qBound(3.0, qMin(width(), height()) / 46.0, 11.0);
    const qreal barWidth = qMax(10.0, radius * 2.6);
    const qreal barHeight = qMax(2.0, radius * 0.34);
    for (auto it = robots_.cbegin(); it != robots_.cend(); ++it) {
        const auto position = it->position;
        const qreal nx = (flipped_ != mirror_) ? 1.0 - position.x() : position.x();
        const auto point = fieldOrigin + QPointF(nx * fieldWidth + calX_ * fit,
                                                 (1.0 - position.y()) * fieldHeight + calY_ * fit);
        const uint id = it.key();
        const int index = numberOrder.indexOf(static_cast<int>(id % 100));
        const size_t hpIndex = static_cast<size_t>((id >= 100 ? 4 : 0) + qMax(0, index));
        const uint hp = hpIndex < health_.size() ? health_[hpIndex] : 0;
        const uint maxHp = hpIndex < maxHealth_.size() ? maxHealth_[hpIndex] : 0;
        const bool self = ownId_ != 0 && id == ownId_;
        const QColor fill = self ? QColor("#7fe0a8") : (id < 100 ? QColor("#ff5a67") : QColor("#3ea8ff"));

        // 朝向箭头（有 yaw 时按角度旋转，无则朝上）
        painter.save();
        painter.translate(point);
        if (!std::isnan(it->yaw)) painter.rotate(it->yaw);
        painter.setPen(Qt::NoPen);
        painter.setBrush(fill);
        painter.drawPolygon(QPolygonF(QVector<QPointF>{QPointF(0, -radius * 1.9),
                                                       QPointF(-radius * 0.55, -radius * 1.05),
                                                       QPointF(radius * 0.55, -radius * 1.05)}));
        painter.restore();

        // 机体圆点（自己不变大，只靠颜色区分）
        painter.setPen(QPen(QColor(255, 255, 255, self ? 200 : 150), qMax(1.0, radius * 0.16)));
        painter.setBrush(fill);
        painter.drawEllipse(point, radius, radius);
        // 编号：始终绘制，字号不小于 8px，加深色描边保证小地图上也看得清
        {
            QFont f = painter.font();
            f.setPixelSize(qMax(8, qRound(radius * 1.5)));
            f.setBold(true);
            painter.setFont(f);
            const QRectF textRect(point.x() - radius * 1.6, point.y() - radius * 1.6, radius * 3.2, radius * 3.2);
            painter.setPen(QColor(0, 0, 0, 190));
            painter.drawText(textRect.translated(0.8, 0.8), Qt::AlignCenter, QString::number(id % 100));
            painter.setPen(QColor("#ffffff"));
            painter.drawText(textRect, Qt::AlignCenter, QString::number(id % 100));
        }

        // 血条
        if (maxHp > 0) {
            const QRectF bar(point.x() - barWidth / 2, point.y() + radius + 1.5, barWidth, barHeight);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0, 0, 0, 150));
            painter.drawRoundedRect(bar.adjusted(-0.5, -0.5, 0.5, 0.5), 1, 1);
            painter.setBrush(fill);
            painter.drawRoundedRect(QRectF(bar.left(), bar.top(), barWidth * qMin(1.0, double(hp) / maxHp), barHeight), 1, 1);
        }
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
    const qreal scale = qMin(width() / 900.0, height() / 482.0);
    p.scale(scale, scale);
    p.translate(-450, -241);
    // 换边镜像：整幅底图与标记一起左右翻转（红蓝互换）
    if (mirror_) {
        p.translate(900, 0);
        p.scale(-1, 1);
    }

    // 网格
    p.setPen(QPen(QColor(72, 97, 106, 55), 1));
    for (int x = 0; x <= 900; x += 36) p.drawLine(x, 0, x, 482);
    for (int y = 0; y <= 482; y += 36) p.drawLine(0, y, 900, y);

    // 场地轮廓：八边形切角（保留官方轮廓）
    p.setPen(QPen(QColor(232, 163, 61, 175), 3));
    p.setBrush(Qt::NoBrush);
    p.drawPolygon(QPolygonF(QVector<QPointF>{{94,20},{806,20},{876,90},{876,392},{806,462},{94,462},{24,392},{24,90}}));
    p.setPen(QPen(QColor(232, 163, 61, 90), 2, Qt::DashLine));
    p.drawLine(450, 20, 450, 462);

    // 公路区（上下通道）
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(52, 74, 69, 110));
    p.drawRoundedRect(QRectF(36, 28, 828, 30), 5, 5);
    p.drawRoundedRect(QRectF(36, 424, 828, 30), 5, 5);

    // 启动区（红左上 / 蓝右下，对角）
    p.setBrush(QColor(216, 92, 96, 40));
    p.drawRoundedRect(QRectF(30, 30, 150, 84), 6, 6);
    p.setBrush(QColor(56, 166, 216, 40));
    p.drawRoundedRect(QRectF(720, 368, 150, 84), 6, 6);

    // 中央高台（八边形制高点）
    QLinearGradient plateau(450, 91, 450, 391);
    plateau.setColorAt(0, QColor("#22383d"));
    plateau.setColorAt(1, QColor("#1a2c33"));
    p.setPen(QPen(QColor(150, 170, 168, 210), 3));
    p.setBrush(plateau);
    p.drawPolygon(QPolygonF(QVector<QPointF>{{340,91},{560,91},{615,146},{615,336},{560,391},{340,391},{285,336},{285,146}}));

    // 高台四向坡道
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#2c4247"));
    p.drawRoundedRect(QRectF(420, 80, 60, 14), 3, 3);
    p.drawRoundedRect(QRectF(420, 388, 60, 14), 3, 3);
    p.drawRoundedRect(QRectF(272, 211, 14, 60), 3, 3);
    p.drawRoundedRect(QRectF(614, 211, 14, 60), 3, 3);

    const auto hexagon = [](QPointF c, qreal r) {
        QPolygonF poly;
        for (int i = 0; i < 6; ++i) {
            const qreal a = M_PI / 3.0 * i + M_PI / 6.0;
            poly << c + QPointF(r * std::cos(a), r * std::sin(a));
        }
        return poly;
    };

    // 中央科技核心（菱形）
    QLinearGradient core(450, 183, 450, 299);
    core.setColorAt(0, QColor("#2d4a4f"));
    core.setColorAt(1, QColor("#1b2f35"));
    p.setPen(QPen(QColor(232, 163, 61, 190), 3));
    p.setBrush(core);
    p.drawPolygon(QPolygonF(QVector<QPointF>{{450,183},{508,241},{450,299},{392,241}}));
    p.setPen(QPen(QColor(255, 201, 113, 160), 2));
    p.setBrush(QColor("#0e1c24"));
    p.drawPolygon(QPolygonF(QVector<QPointF>{{450,211},{480,241},{450,271},{420,241}}));

    // 高台前后增益点（六边形，中性）
    p.setPen(QPen(QColor(124, 147, 120), 2));
    p.setBrush(QColor("#33474a"));
    p.drawPolygon(hexagon(QPointF(450, 130), 26));
    p.drawPolygon(hexagon(QPointF(450, 352), 26));

    // 前哨站（场地两侧，靠近启动区；左红右蓝）
    for (const auto &post : {std::make_pair(QPointF(105, 241), QColor("#d85c60")),
                             std::make_pair(QPointF(795, 241), QColor("#38a6d8"))}) {
        p.setPen(QPen(QColor(post.second.red(), post.second.green(), post.second.blue(), 180), 2));
        p.setBrush(QColor("#24383c"));
        p.drawPolygon(hexagon(post.first, 48));
        p.setBrush(post.second);
        p.setOpacity(0.4);
        p.drawPolygon(hexagon(post.first, 24));
        p.setOpacity(1.0);
    }

    // 高台两侧的环形高地增益点（中性六边形）
    p.setPen(QPen(QColor(124, 147, 120), 2));
    p.setBrush(QColor("#2e4245"));
    p.drawPolygon(hexagon(QPointF(200, 241), 30));
    p.drawPolygon(hexagon(QPointF(700, 241), 30));

    // 红蓝基地（对角，带光晕）
    const struct { QPointF c; QColor fill, glow; } bases[] = {
        {QPointF(88, 66), QColor("#d85c60"), QColor(255, 90, 103, 140)},
        {QPointF(812, 416), QColor("#38a6d8"), QColor(62, 168, 255, 140)}};
    for (const auto &b : bases) {
        QRadialGradient g(b.c, 76);
        g.setColorAt(0, b.glow);
        g.setColorAt(1, QColor(0, 0, 0, 0));
        p.setPen(Qt::NoPen);
        p.setBrush(g);
        p.drawEllipse(b.c, 76, 76);
        p.setPen(QPen(QColor(255, 255, 255, 190), 3));
        p.setBrush(b.fill);
        p.drawPolygon(hexagon(b.c, 34));
    }
    // 官方场地渲染图（压暗抽象化）作为底图，覆盖手绘结构，只保留轮廓与中线
    QPixmap field(":/images/field.png");
    if (!field.isNull()) {
        const QRectF target(10, 8, 880, 466);
        const QSize targetSize = target.size().toSize();
        const QPixmap scaled = field.scaled(targetSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        const QRectF source((scaled.width() - targetSize.width()) / 2.0,
                            (scaled.height() - targetSize.height()) / 2.0,
                            targetSize.width(), targetSize.height());
        p.setOpacity(0.92);
        p.drawPixmap(target, scaled, source);
        p.setOpacity(1.0);
        p.fillRect(target, QColor(6, 12, 18, 76));
        p.setPen(QPen(QColor(232, 163, 61, 150), 2));
        p.setBrush(Qt::NoBrush);
        p.drawRect(target);
        p.setPen(QPen(QColor(232, 163, 61, 80), 2, Qt::DashLine));
        p.drawLine(QPointF(450, target.top()), QPointF(450, target.bottom()));
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
