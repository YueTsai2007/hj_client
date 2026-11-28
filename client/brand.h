#pragma once
#include <QColor>
#include <QFrame>
#include <QPixmap>
#include <QWidget>

class QTimer;

namespace brand {
inline const char *kGold = "#e8a33d";
inline const char *kGoldLight = "#ffc971";
// 战队 Logo（透明 PNG，橙金色），按设备像素比缩放
QPixmap logo(int size, qreal devicePixelRatio = 1.0);
}

// 战队徽标：金色光晕 + 动态旋转齿轮虚线环 + 悍匠 Logo
class LogoBadge : public QWidget {
public:
    explicit LogoBadge(int size = 72, QWidget *parent = nullptr);
    void setBadgeSize(int size);
    void setAnimated(bool on);
    QSize sizeHint() const override { return {size_, size_}; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    int size_;
    qreal angle_ = 0.0;
    QTimer *spinTimer_ = nullptr;
};

// 带顶部流光强调线、战术角标与可选悍匠水印的面板
class GoldFrame : public QFrame {
public:
    explicit GoldFrame(QWidget *parent = nullptr);
    void setAccent(const QColor &color);   // 顶部强调线颜色（阵营色或悍匠金）
    void setWatermark(bool on) { watermark_ = on; update(); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QColor accent_ = QColor(brand::kGold);
    bool watermark_ = false;
};
