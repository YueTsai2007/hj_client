#pragma once
#include <QColor>
#include <QFrame>
#include <QPixmap>
#include <QWidget>

namespace brand {
inline const char *kGold = "#e8a33d";
inline const char *kGoldLight = "#ffc971";
// 战队 Logo（透明 PNG，橙金色），按设备像素比缩放
QPixmap logo(int size, qreal devicePixelRatio = 1.0);
}

// 战队徽标：金色光晕 + 缓慢旋转的虚线齿轮环 + Logo
class LogoBadge : public QWidget {
public:
    explicit LogoBadge(int size = 72, QWidget *parent = nullptr);
    QSize sizeHint() const override { return {size_, size_}; }
protected:
    void paintEvent(QPaintEvent *) override;
private:
    int size_;
    int angle_ = 0;
};

// 带金色四角折线、顶部强调线与淡色 Logo 水印的面板
class GoldFrame : public QFrame {
public:
    explicit GoldFrame(QWidget *parent = nullptr);
    void setAccent(const QColor &color);   // 顶部强调线颜色（阵营色）
    void setWatermark(bool on) { watermark_ = on; update(); }
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    QColor accent_ = QColor(brand::kGold);
    bool watermark_ = true;
};

