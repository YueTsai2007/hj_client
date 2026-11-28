#include "window.h"
#include "window_helpers.h"
#include "floating_panel.h"
#include "combat_view.h"
#include "map.h"
#include "brand.h"
#include "ui_theme.h"
#include "video_view.h"
#include <QSettings>
#include <QStackedWidget>
#include <QShortcut>
#include <QDockWidget>
#include <QMainWindow>
#include <QFrame>
#include <QSpinBox>
#include <QGridLayout>
#include <QPushButton>
#include <QCheckBox>
#include <QRegularExpression>
#include "rm2027/api/messages.h"
#include <QApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QKeyEvent>
#include <functional>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QSplitter>
#include <QToolBar>
#include <QToolButton>
#include <QMenu>
#include <QIcon>
#include <QStyle>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QProgressBar>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QVBoxLayout>

namespace {
class ModuleTitle final : public QLabel {
public:
    explicit ModuleTitle(const QString &title) : QLabel(title) {
        setObjectName("moduleTitle");
        setCursor(Qt::OpenHandCursor);
        setAcceptDrops(true);
        setToolTip(QStringLiteral("拖到另一个模块的标题栏"));
    }

    std::function<bool(QWidget *)> canDrop;
    std::function<void(QWidget *)> dropped;

protected:
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) press_ = event->pos();
        QLabel::mousePressEvent(event);
    }
    void mouseMoveEvent(QMouseEvent *event) override {
        if (!(event->buttons() & Qt::LeftButton) ||
            (event->pos() - press_).manhattanLength() < QApplication::startDragDistance())
            return;
        QDrag drag(this);
        auto *data = new QMimeData;
        data->setData("application/x-hj-module", QByteArray());
        drag.setMimeData(data);
        drag.exec(Qt::MoveAction);
    }
    void dragEnterEvent(QDragEnterEvent *event) override {
        auto *title = dynamic_cast<ModuleTitle *>(event->source());
        if (!title || !canDrop(title->parentWidget())) return;
        setStyleSheet(QStringLiteral("background:#745127;"));
        event->acceptProposedAction();
    }
    void dragLeaveEvent(QDragLeaveEvent *) override { setStyleSheet({}); }
    void dropEvent(QDropEvent *event) override {
        setStyleSheet({});
        auto *title = dynamic_cast<ModuleTitle *>(event->source());
        if (!title || !canDrop(title->parentWidget())) return;
        dropped(title->parentWidget());
        event->acceptProposedAction();
    }

private:
    QPoint press_;
};

class WatermarkedList final : public QListWidget {
protected:
    void scrollContentsBy(int dx, int dy) override {
        QListWidget::scrollContentsBy(dx, dy);
        viewport()->update();
    }

    void paintEvent(QPaintEvent *event) override {
        const int size = qMin(viewport()->width(), viewport()->height()) * 2 / 3;
        const qreal scale = viewport()->devicePixelRatioF();
        if (size > 0) {
            if (size != logoSize_ || scale != logo_.devicePixelRatio()) {
                logo_ = brand::logo(size, scale);
                logoSize_ = size;
            }
            QPainter painter(viewport());
            painter.setOpacity(0.08);
            const QSizeF logicalSize = QSizeF(logo_.size()) / scale;
            painter.drawPixmap(QPointF((viewport()->width() - logicalSize.width()) / 2,
                                       (viewport()->height() - logicalSize.height()) / 2), logo_);
        }
        QListWidget::paintEvent(event);
    }

private:
    QPixmap logo_;
    int logoSize_ = 0;
};

class BaseHealthBar final : public QProgressBar {
public:
    BaseHealthBar(const QString &title, const QColor &color, bool right)
        : title_(title), color_(color), right_(right) {
        setRange(0, 1000);
        reset();
        setFormat(QStringLiteral("--"));
        setAccessibleName(title);
        setFixedHeight(70);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF bounds = QRectF(rect()).adjusted(1, 1, -1, -1);
        const qreal cut = 17;
        const QPolygonF outline(QVector<QPointF>{
            bounds.topLeft() + QPointF(right_ ? cut : 0, 0),
            bounds.topRight() - QPointF(right_ ? 0 : cut, 0),
            bounds.bottomRight() - QPointF(right_ ? cut : 0, 0),
            bounds.bottomLeft() + QPointF(right_ ? 0 : cut, 0)
        });
        QPainterPath shape;
        shape.addPolygon(outline);
        shape.closeSubpath();
        painter.fillPath(shape, QColor("#212a36"));
        if (value() >= 0) {
            const qreal filled = bounds.width() * value() / maximum();
            const QRectF fill(right_ ? bounds.right() - filled : bounds.left(),
                              bounds.top(), filled, bounds.height());
            QLinearGradient gradient(bounds.topLeft(), bounds.bottomLeft());
            gradient.setColorAt(0, color_.darker(140));
            gradient.setColorAt(1, color_.darker(255));
            painter.save();
            painter.setClipPath(shape);
            painter.fillRect(fill, gradient);
            painter.restore();
        }
        painter.setPen(QPen(color_, 1.5));
        painter.drawPath(shape);
        QFont type = font();
        type.setPixelSize(14);
        painter.setFont(type);
        painter.setPen(QColor("#fff4e9"));
        painter.drawText(bounds.adjusted(0, 7, 0, 0), Qt::AlignTop | Qt::AlignHCenter, title_);
        type.setPixelSize(31);
        type.setBold(true);
        painter.setFont(type);
        painter.drawText(bounds.adjusted(0, 22, 0, -3), Qt::AlignCenter, format() + QStringLiteral(" HP"));
    }

private:
    QString title_;
    QColor color_;
    bool right_;
};

QSettings preferences() {
    return QSettings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Hanjiang"), QStringLiteral("HJ_Client"));
}

QFrame *panel(const QString &title, QVBoxLayout *&layout, const char *name = "panel") {
    auto *frame = new GoldFrame;
    frame->setObjectName(name);
    layout = new QVBoxLayout(frame);
    layout->setContentsMargins(14, 10, 14, 12);
    layout->setSpacing(8);
    auto *head = new QHBoxLayout;
    head->setSpacing(8);
    head->addWidget(label(title, "panelTitle"));
    head->addStretch();
    layout->addLayout(head);
    auto *rule = new QFrame;
    rule->setObjectName("goldRule");
    rule->setFixedHeight(1);
    layout->addWidget(rule);
    return frame;
}
} // namespace

Window::Window(const QString &host, quint16 port, const QString &id, quint16 videoPort) {
    const bool loopback = host == QStringLiteral("127.0.0.1") ||
                          host.compare(QStringLiteral("localhost"), Qt::CaseInsensitive) == 0 ||
                          host == QStringLiteral("::1");
    demoMode_ = qEnvironmentVariableIsSet("HJ_DEMO_MAP")
        ? qEnvironmentVariableIntValue("HJ_DEMO_MAP") == 1 : loopback;
    bool idOk = false;
    const uint clientId = id.toUInt(&idOk);
    if (idOk && clientId > 0 && clientId < 200) {
        side_ = clientId >= 100 ? 1 : 0;
        number_ = clientId % 100;
        role_ = roleOfId(number_);
    }
    ownSide_ = side_;
    setupUi();
    applyWindowPreferences();
    start(host, port, id, videoPort);
}
Window::~Window() {
    if (timer_) timer_->stop();
    if (client_) client_->stop();
    if (video_) video_->stop();
}
void Window::setupUi() {
    setWindowTitle(QStringLiteral("悍匠 · 战场终端 · %1").arg(roleLabel()));
    setMinimumSize(780, 520);
    auto *shell = new QWidget;
    dataPage_ = shell;
    mainStack_ = new QStackedWidget;
    setCentralWidget(mainStack_);
    auto *shellLayout = new QVBoxLayout(shell);
    shellLayout->setContentsMargins(8, 4, 8, 6);
    shellLayout->setSpacing(6);
    auto *scorePanel = new QWidget;
    auto *outer = new QVBoxLayout(scorePanel);
    outer->setContentsMargins(12, 6, 12, 8);
    outer->setSpacing(6);

    auto *header = new QGridLayout;
    header->setColumnStretch(1, 1);
    header->setColumnStretch(5, 1);
    auto *identity = new QHBoxLayout;
    identity->addWidget(new LogoBadge(32));
    auto *team = label(QStringLiteral("深圳技术大学悍匠战队"), "brand");
    identity->addWidget(team);
    identity->addStretch();
    header->addLayout(identity, 0, 0, 1, 3);
    const auto score = [header](int column, const QString &title, const char *style) {
        auto *group = new QVBoxLayout;
        auto *caption = label(title, "caption");
        auto *value = label(QStringLiteral("--"), style);
        caption->setAlignment(Qt::AlignCenter);
        value->setAlignment(Qt::AlignCenter);
        group->addWidget(caption);
        group->addWidget(value);
        header->addLayout(group, 1, column);
        return value;
    };
    baseRed_ = label(QStringLiteral("--"));
    baseRed_->setParent(scorePanel);
    baseRed_->hide();
    baseRedBar_ = new BaseHealthBar(QStringLiteral("红方基地"), QColor("#ff7284"), false);
    header->addWidget(baseRedBar_, 1, 0, 1, 2);
    scoreRed_ = score(2, QStringLiteral("红方比分"), "redScore");
    auto *clockGroup = new QVBoxLayout;
    round_ = label(QStringLiteral("等待比赛状态"), "caption");
    clock_ = label(QStringLiteral("--:--"), "clock");
    round_->setAlignment(Qt::AlignCenter);
    clock_->setAlignment(Qt::AlignCenter);
    clockGroup->addWidget(round_);
    clockGroup->addWidget(clock_);
    header->addLayout(clockGroup, 1, 3);
    scoreBlue_ = score(4, QStringLiteral("蓝方比分"), "blueScore");
    baseBlue_ = label(QStringLiteral("--"));
    baseBlue_->setParent(scorePanel);
    baseBlue_->hide();
    baseBlueBar_ = new BaseHealthBar(QStringLiteral("蓝方基地"), QColor("#72b7ff"), true);
    header->addWidget(baseBlueBar_, 1, 5, 1, 2);
    auto *links = new QHBoxLayout;
    links->addStretch();
    connection_ = label(QStringLiteral("裁判 · 正在连接"), "caption");
    dataStatus_ = label(QStringLiteral("等待比赛数据"), "caption");
    videoLink_ = label(QStringLiteral("图传 · 等待首帧"), "caption");
    for (auto *link : {connection_, dataStatus_, videoLink_}) {
        link->setAlignment(Qt::AlignRight);
        links->addWidget(link);
    }
    header->addLayout(links, 0, 3, 1, 4);
    outer->addLayout(header);

    alert_ = label(QStringLiteral("等待连接"), "alert");
    alert_->setFixedHeight(34);
    outer->addWidget(alert_);
    scorePanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    shellLayout->addWidget(scorePanel);
    workspace_ = new QSplitter(Qt::Horizontal);
    workspace_->setObjectName("workspace");
    workspace_->setChildrenCollapsible(false);
    workspace_->setHandleWidth(9);
    auto *primary = new QWidget;
    primaryLayout_ = new QVBoxLayout(primary);
    primaryLayout_->setContentsMargins(0, 0, 0, 0);
    sidebar_ = new QSplitter(Qt::Vertical);
    sidebar_->setObjectName("moduleSidebar");
    sidebar_->setChildrenCollapsible(false);
    sidebar_->setHandleWidth(9);
    workspace_->addWidget(primary);
    workspace_->addWidget(sidebar_);
    shellLayout->addWidget(workspace_, 1);
    // ClearWei 式顶部状态条：前哨 / 经济 / 堡垒 / 买活金币
    topBar_ = new QLabel(QStringLiteral("等待数据"));
    topBar_->setObjectName("topBar");
    topBar_->setTextFormat(Qt::RichText);
    topBar_->setStyleSheet(QStringLiteral(
        "QLabel#topBar{background:#111a24;border:1px solid #2b3644;border-radius:6px;"
        "padding:5px 12px;color:#eaf1f7;font-size:13px;}"));
    shellLayout->insertWidget(1, topBar_);
    toolbar_ = addToolBar(QStringLiteral("窗口布局"));
    auto *toolbar = toolbar_;
    toolbar->setObjectName("layoutToolbar");
    toolbar->setMovable(false);
    auto *switchView = toolbar->addAction(QStringLiteral("图传 / 地图  F2"));
    addAction(switchView);
    switchView->setShortcut(QKeySequence(Qt::Key_F2));
    switchView->setAutoRepeat(false);
    connect(switchView, &QAction::triggered, this, [this] {
        if (!combat_ || !mainStack_) return;
        if (mainStack_->currentWidget() == combat_) {
            // 作战视图：切图传 / 地图页
            combat_->stack()->setCurrentIndex(1 - combat_->stack()->currentIndex());
        } else {
            // 数据视图：主视图与右下小视图互换（临时看图传细节）
            videoPriority_ = !videoPriority_;
            arrangeModules();
        }
    });
    auto *crosshairToggle = toolbar->addAction(QStringLiteral("准星  F4"));
    addAction(crosshairToggle);
    crosshairToggle->setCheckable(true);
    crosshairToggle->setShortcut(QKeySequence(Qt::Key_F4));
    crosshairToggle->setAutoRepeat(false);
    crosshairAction_ = crosshairToggle;
    const bool crosshairOn = preferences().value("view/crosshair", true).toBool();
    crosshairToggle->setChecked(crosshairOn);
    connect(crosshairToggle, &QAction::toggled, this, [this](bool on) {
        if (crosshair_) crosshair_->setVisible(on);
        auto settings = preferences();
        settings.setValue("view/crosshair", on);
    });
    auto *dataViewAction = toolbar->addAction(QStringLiteral("作战 / 数据视图  F3"));
    dataViewAction->setShortcut(QKeySequence(Qt::Key_F3));
    dataViewAction->setAutoRepeat(false);
    addAction(dataViewAction);
    connect(dataViewAction, &QAction::triggered, this, [this] {
        if (!mainStack_ || !combat_) return;
        mainStack_->setCurrentIndex(mainStack_->currentWidget() == combat_ ? 1 : 0);
        syncCombatChrome();
        updateCombatHud();
    });
    auto *fillModeAction = toolbar->addAction(QStringLiteral("画面铺满 / 等比  F5"));
    fillModeAction->setShortcut(QKeySequence(Qt::Key_F5));
    fillModeAction->setAutoRepeat(false);
    addAction(fillModeAction);
    connect(fillModeAction, &QAction::triggered, this, [this] {
        if (!combatVideo_) return;
        combatVideo_->setFillMode(!combatVideo_->fillMode());
        if (hudFeed_) {
            hudFeed_->pushMessage(
                QStringLiteral("画面"),
                combatVideo_->fillMode() ? QStringLiteral("图传切换为全屏铺满模式") : QStringLiteral("图传切换为等比完整模式"),
                ui_theme::kGoldLight, false);
        }
    });
    auto *minimapAction = toolbar->addAction(QStringLiteral("小地图缩放  M"));
    minimapAction->setShortcut(QKeySequence(Qt::Key_M));
    minimapAction->setAutoRepeat(false);
    addAction(minimapAction);
    connect(minimapAction, &QAction::triggered, this, [this] {
        minimapMode_ = (minimapMode_ + 1) % 3;
        if (combat_) layoutCombatHud(combat_->contentRect());
    });
    auto *focusAction = toolbar->addAction(QStringLiteral("专注模式  H"));
    focusAction->setShortcut(QKeySequence(Qt::Key_H));
    focusAction->setAutoRepeat(false);
    addAction(focusAction);
    connect(focusAction, &QAction::triggered, this, [this] {
        focusMode_ = !focusMode_;
        if (combat_) layoutCombatHud(combat_->contentRect());
        if (hudFeed_ && !focusMode_) {
            hudFeed_->pushMessage(QStringLiteral("模式"), QStringLiteral("已退出专注模式，恢复完整 HUD"), ui_theme::kGoldLight, false);
        }
    });
    connect(toolbar->addAction(QStringLiteral("兑换 / 买活  O")), &QAction::triggered, this, &Window::toggleExchangePanel);
    connect(toolbar->addAction(QStringLiteral("状态检测")), &QAction::triggered, this, [this] {
        if (!modulePanelDock_) return;
        if (modulePanelDock_->isVisible()) {
            modulePanelDock_->hide();
        } else {
            modulePanelDock_->move(qMax(20, width() - modulePanelDock_->width() - 40), 160);
            modulePanelDock_->show();
            modulePanelDock_->raise();
        }
    });
    connect(toolbar->addAction(QStringLiteral("消息详情")), &QAction::triggered, this, &Window::showDetails);
    connect(toolbar->addAction(QStringLiteral("恢复默认布局")), &QAction::triggered, this, &Window::applyDefaultLayout);

    QVBoxLayout *videoLayout, *mapLayout, *allyLayout, *enemyLayout, *eventLayout, *metricsLayout, *analysisLayout, *hudLayout;
    auto *videoPanel = panel(QStringLiteral("图传画面"), videoLayout);
    videoView_ = new VideoView;
    videoLayout->addWidget(videoView_, 1);
    videoStatus_ = label(QStringLiteral("等待首帧"), "caption");
    videoLayout->addWidget(videoStatus_);

    auto *mapPanel = panel(QStringLiteral("战场地图 · 示意图"), mapLayout);
    map_ = new Map;
    map_->setProperty("demoMode", demoMode_);
    mapLayout->addWidget(map_, 1);
    position_ = label(QStringLiteral("机器人坐标：等待数据"), "caption");
    radar_ = label(QStringLiteral("雷达坐标：等待数据"), "caption");
    position_->setWordWrap(true);
    radar_->setWordWrap(true);
    position_->hide();
    radar_->hide();
    position_->setParent(scorePanel);
    radar_->setParent(scorePanel);

    auto *allyPanel = panel(QStringLiteral("我方"), allyLayout, "allyPanel");
    auto *enemyPanel = panel(QStringLiteral("对方"), enemyLayout, "enemyPanel");
    ally_ = new WatermarkedList;
    enemy_ = new WatermarkedList;
    for (auto *list : {ally_, enemy_}) {
        list->setSelectionMode(QAbstractItemView::NoSelection);
        list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        list->setSpacing(1);
    }
    allyLayout->addWidget(ally_);
    enemyLayout->addWidget(enemy_);
    showHealthPlaceholder(QStringLiteral("等待数据"));
    auto *eventPanel = panel(QStringLiteral("战场事件"), eventLayout);
    events_ = new WatermarkedList;
    eventLayout->addWidget(events_);

    auto *analysisPanel = panel(QStringLiteral("即时战术数据"), analysisLayout);
    analysis_ = new QLabel(QStringLiteral("等待数据"));
    analysis_->setObjectName("analysisLabel");
    analysis_->setTextFormat(Qt::RichText);
    analysis_->setWordWrap(true);
    analysis_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    analysisLayout->addWidget(analysis_);

    // 模块自检：8 个圆点，红=异常 / 绿=正常 / 灰=无数据
    modulePanel_ = new QWidget;
    auto *moduleGrid = new QGridLayout(modulePanel_);
    moduleGrid->setContentsMargins(0, 6, 0, 0);
    moduleGrid->setHorizontalSpacing(10);
    moduleGrid->setVerticalSpacing(4);
    const QStringList moduleNames{QStringLiteral("电源"), QStringLiteral("RFID"), QStringLiteral("灯带"),
                                  QStringLiteral("小发射"), QStringLiteral("大发射"), QStringLiteral("UWB"),
                                  QStringLiteral("装甲"), QStringLiteral("图传")};
    for (int i = 0; i < moduleNames.size(); ++i) {
        auto *dot = new QLabel(QStringLiteral("● %1").arg(moduleNames.at(i)));
        dot->setStyleSheet(QStringLiteral("color:#6d7885;font-size:12px;"));
        moduleDots_[static_cast<size_t>(i)] = dot;
        moduleGrid->addWidget(dot, i / 4, i % 4);
    }
    // 模块自检独立成悬浮面板（见 buildModulePanelDock），不占战术数据位置

    auto *localPanel = new QWidget;
    auto *local = new QHBoxLayout(localPanel);
    const QStringList names{QStringLiteral("本机血量"), QStringLiteral("可用弹量"), QStringLiteral("热量"), QStringLiteral("复活")};
    for (int i = 0; i < static_cast<int>(localStatus_.size()); ++i) {
        auto *metric = new QVBoxLayout;
        metric->addWidget(label(names.at(i), "caption"));
        localStatus_[i] = label(QStringLiteral("--"), "localValue");
        metric->addWidget(localStatus_[i]);
        local->addLayout(metric, 1);
    }
    mapDock_ = addModule("map", QStringLiteral("战场地图 · 示意图"), mapPanel);
    videoDock_ = addModule("video", QStringLiteral("图传"), videoPanel);
    auto *hudPanel = panel(QStringLiteral("双方机器人"), hudLayout);
    hudLayout->addWidget(label(QStringLiteral("我方"), "caption"));
    hudLayout->addWidget(ally_, 1);
    hudLayout->addWidget(label(QStringLiteral("对方"), "caption"));
    hudLayout->addWidget(enemy_, 1);
    allyModule_ = addModule("hud", QStringLiteral("双方机器人"), hudPanel);
    analysisModule_ = addModule("analysis", QStringLiteral("即时战术数据"), analysisPanel);
    eventsModule_ = addModule("events", QStringLiteral("战场事件"), eventPanel);
    enemyModule_ = allyModule_;
    bottomLeft_ = new QWidget;
    bottomLeftLayout_ = new QHBoxLayout(bottomLeft_);
    bottomLeftLayout_->setContentsMargins(0, 0, 0, 0);
    bottomLeftLayout_->setSpacing(8);
    bottomLeftLayout_->addWidget(eventsModule_, 1);
    bottomLeftLayout_->addWidget(analysisModule_, 1);
    sidebarModules_ = {allyModule_, videoDock_};
    sidebar_->setMinimumWidth(500);
    sidebar_->setMaximumWidth(640);
    localPanel->setFixedHeight(58);
    shellLayout->addWidget(localPanel);

    metricsPage_ = panel(QStringLiteral("场地与设备"), metricsLayout);
    economy_ = label(QStringLiteral("剩余经济：等待数据"), "caption");
    damage_ = label(QStringLiteral("累计伤害：等待数据"), "caption");
    situation_ = new QListWidget;
    metricsLayout->addWidget(economy_);
    metricsLayout->addWidget(damage_);
    metricsLayout->addWidget(situation_);
    setWindowIcon(QIcon(brand::logo(128)));

    setupDetails();
    if (roleNeedsPanel()) {
        auto *rolePanel = new QWidget;
        auto *roleLayout = new QVBoxLayout(rolePanel);
        roleInfo_ = label(QStringLiteral("等待数据"), "caption");
        roleInfo_->setWordWrap(true);
        roleLayout->addWidget(roleInfo_);
        roleLayout->addStretch();
        roleDock_ = addModule("role", roleLabel(), rolePanel);
        sidebarModules_.append(roleDock_);
    }
    defaultSidebar_ = sidebarModules_;
    arrangeModules();
    detailsDock_->installEventFilter(this);
    auto *fullShortcut = new QShortcut(QKeySequence(Qt::Key_F11), this);
    fullShortcut->setAutoRepeat(false);
    connect(fullShortcut, &QShortcut::activated, this, [this] {
        const bool full = !isFullScreen();
        setWindowState(full ? windowState() | Qt::WindowFullScreen
                            : windowState() & ~Qt::WindowFullScreen);
        preferences().setValue("view/fullScreen", full);
    });

    setStyleSheet(R"(
        QWidget { color:#e5ebf2; font-family:"Microsoft YaHei","Noto Sans CJK SC",sans-serif; font-size:16px; }
        QMainWindow, QToolBar { background:#10141b; }
        QLabel#moduleTitle { background:#273240; padding:4px 8px; border-radius:4px; }
        QSplitter#workspace::handle, QSplitter#moduleSidebar::handle { background:#334052; border-radius:3px; }
        QSplitter#workspace::handle:hover, QSplitter#moduleSidebar::handle:hover { background:#cd994d; }
        QMenu::item:selected, QToolButton:hover { background:#323e50; }
        QToolButton:checked { background:#2c3a4a; border:1px solid #ffd18b; color:#ffe0a3; }
        QFrame#panel, QFrame#allyPanel, QFrame#enemyPanel { background:#171d26; border:1px solid #303946; border-radius:8px; }
        QFrame#goldRule { background:#323c48; border:none; }
        QLabel#caption { color:#b8c4d1; }
        QLabel#brand, QLabel#panelTitle { font-weight:700; }
        QLabel#brand, QLabel#clock { color:#ffd18b; }
        QLabel#redScore, QLabel#baseRed { color:#ff7e8b; }
        QLabel#blueScore, QLabel#baseBlue { color:#78bbff; }
        QLabel#clock, QLabel#redScore, QLabel#blueScore { font-size:38px; font-weight:700; min-width:75px; }
        QLabel#baseRed, QLabel#baseBlue, QLabel#localValue { font-size:20px; font-weight:700; }
        QLabel#alert { background:#342719; color:#ffe0a3; padding:6px 10px; border-radius:5px; }
        QLabel#alert[ok="true"] { background:#1b2927; color:#badbd1; }
        QPushButton { background:#242d39; border:1px solid #424e60; border-radius:5px; padding:5px 12px; }
        QPushButton:hover { background:#323e50; }
        QPushButton:focus, QComboBox:focus { border:1px solid #ffd18b; }
        QListWidget, QPlainTextEdit { background:#111720; border:1px solid #303946; }
        QListWidget::item { padding:2px; }
        QScrollBar:vertical { background:#111720; width:8px; }
        QScrollBar::handle:vertical { background:#526376; border-radius:3px; min-height:22px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
    )");
    resize(1366, 768);
    const int envWidth = qEnvironmentVariableIntValue("HJ_WIN_W");
    const int envHeight = qEnvironmentVariableIntValue("HJ_WIN_H");
    if (envWidth > 0 && envHeight > 0) resize(envWidth, envHeight);
    // 左列：主视图 + 底部事件/数据；右列固定宽一些，避免地图区域太扁
    workspace_->setSizes({qMax(width() - 560, 680), 560});

    buildCombatView();
    buildExchangePanel();
    buildModulePanelDock();
    mainStack_->addWidget(combat_);
    mainStack_->addWidget(dataPage_);
    mainStack_->setCurrentWidget(combat_);
    if (qEnvironmentVariableIntValue("HJ_START_DATA_VIEW") == 1) mainStack_->setCurrentWidget(dataPage_);
    syncCombatChrome();
}

void Window::syncCombatChrome() {
    if (!toolbar_ || !mainStack_) return;
    // 作战视图隐藏顶部传统工具栏，实现真正全屏无黑边图传；快捷键全部绑定在 Window 上持续有效
    toolbar_->setVisible(mainStack_->currentWidget() != combat_);
}

namespace {
QLabel *hudLabel(const QString &text, int pixelSize, const QColor &color, QWidget *parent, bool bold = false) {
    auto *item = new QLabel(text, parent);
    QFont font = item->font();
    font.setPixelSize(pixelSize);
    font.setBold(bold);
    item->setFont(font);
    item->setStyleSheet(QStringLiteral("color:%1;background:transparent;").arg(color.name()));
    item->setProperty("hudColor", color.name());
    item->setAttribute(Qt::WA_TranslucentBackground);
    return item;
}
} // namespace

void Window::buildCombatView() {
    combat_ = new CombatView;
    auto *stack = combat_->stack();

    auto *videoPage = new QWidget;
    auto *videoLayout = new QVBoxLayout(videoPage);
    videoLayout->setContentsMargins(0, 0, 0, 0);
    combatVideo_ = new VideoView;
    videoLayout->addWidget(combatVideo_);
    stack->addWidget(videoPage);

    auto *mapPage = new QWidget;
    auto *mapLayout = new QVBoxLayout(mapPage);
    mapLayout->setContentsMargins(0, 0, 0, 0);
    mapHud_ = new Map;
    mapHud_->setProperty("demoMode", demoMode_);
    auto *fullMap = new Map;
    fullMap->setProperty("demoMode", demoMode_);
    fullMap->setObjectName("combatFullMap");
    mapLayout->addWidget(fullMap);
    stack->addWidget(mapPage);
    stack->setCurrentIndex(0);

    QWidget *hud = combat_->hud();
    // 当图传模糊底图更新时，触发液态玻璃悬浮层重绘以实现实时折射
    combatVideo_->setBackdropUpdatedHandler([hud] {
        if (hud && hud->isVisible()) hud->update();
    });

    // 最底层：受击 / 低血量边缘微暗角警示层
    vignette_ = new VignetteOverlay(hud);

    auto makeCard = [this, hud](const QColor &accent, bool watermark = false, int alpha = 136) {
        auto *card = new HudCard(hud);
        card->setAccent(accent);
        card->setWatermark(watermark);
        card->setOpacityAlpha(alpha);
        card->setBackdropSource(combatVideo_);
        hudCards_.append(card);
        return card;
    };

    const QColor muted = ui_theme::kTextSecondary;
    const QColor primary = ui_theme::kTextPrimary;
    const QColor allyAccent = ownSide_ == 1 ? ui_theme::kBlue : ui_theme::kRed;
    const QColor enemyAccent = ownSide_ == 1 ? ui_theme::kRed : ui_theme::kBlue;

    // 1. 左上角：悍匠战队 Logo + 战队身份液态玻璃徽标卡
    hudBrandCard_ = makeCard(ui_theme::kGold, false, 150);
    auto *brandLayout = new QHBoxLayout(hudBrandCard_);
    brandLayout->setContentsMargins(10, 8, 12, 8);
    brandLayout->setSpacing(9);
    hudBrandLogo_ = new LogoBadge(48, hudBrandCard_);
    auto *brandTextCol = new QVBoxLayout;
    brandTextCol->setContentsMargins(0, 0, 0, 0);
    brandTextCol->setSpacing(2);
    hudBrandTitle_ = hudLabel(QStringLiteral("HANJIANG // 悍匠"), 15, ui_theme::kGoldLight, hudBrandCard_, true);
    hudBrandSub_ = hudLabel(QStringLiteral("%1 · RM27").arg(roleLabel()), 11, muted, hudBrandCard_);
    hudFonts_.append({hudBrandTitle_, 15});
    hudFonts_.append({hudBrandSub_, 11});
    brandTextCol->addWidget(hudBrandTitle_);
    brandTextCol->addWidget(hudBrandSub_);
    brandLayout->addWidget(hudBrandLogo_);
    brandLayout->addLayout(brandTextCol, 1);

    // 2. 经典大长条斜切基地总血条（液态玻璃外壳 + 18° 对称斜切 + 1000HP 刻度 + 掉血残影）
    hudAllyBigBar_ = new LargeSkewedHealthBar(false, hud);
    hudAllyBigBar_->setAccent(allyAccent);
    hudAllyBigBar_->setTitle(QStringLiteral("己方基地 // RED BASE"));
    hudAllyBigBar_->setBackdropSource(combatVideo_);

    hudEnemyBigBar_ = new LargeSkewedHealthBar(true, hud);
    hudEnemyBigBar_->setAccent(enemyAccent);
    hudEnemyBigBar_->setTitle(QStringLiteral("敌方基地 // BLUE BASE"));
    hudEnemyBigBar_->setBackdropSource(combatVideo_);

    // 3. 顶部居中：液态玻璃计时核心
    hudClockCard_ = makeCard(ui_theme::kGold, false, 156);
    auto *clockLayout = new QVBoxLayout(hudClockCard_);
    clockLayout->setContentsMargins(10, 5, 10, 5);
    clockLayout->setSpacing(0);
    hudStage_ = hudLabel(QStringLiteral("HANJIANG TELEMETRY"), 9, ui_theme::kGold, hudClockCard_, true);
    hudStage_->setAlignment(Qt::AlignCenter);
    hudClock_ = hudLabel(QStringLiteral("--:--"), 44, primary, hudClockCard_, true);
    hudClock_->setAlignment(Qt::AlignCenter);
    hudScore_ = hudLabel(QStringLiteral("-- : --"), 12, ui_theme::kGoldLight, hudClockCard_, true);
    hudScore_->setAlignment(Qt::AlignCenter);
    hudFonts_.append({hudStage_, 9});
    hudFonts_.append({hudClock_, 44});
    hudFonts_.append({hudScore_, 12});
    clockLayout->addWidget(hudStage_);
    clockLayout->addWidget(hudClock_);
    clockLayout->addWidget(hudScore_);

    // 4. 双方前哨与 4 机紧凑编队液态玻璃条
    const QList<int> squadIds{1, 3, 4, 7};
    const QStringList squadNames{QStringLiteral("重装"), QStringLiteral("步兵"), QStringLiteral("步兵"), QStringLiteral("哨兵")};
    auto buildSquad = [&](bool ally, std::array<QLabel *, 4> &values, std::array<TacticalBar *, 4> &bars) {
        auto *strip = makeCard(ally ? allyAccent : enemyAccent, false, 118);
        auto *column = new QVBoxLayout(strip);
        column->setContentsMargins(10, 6, 10, 6);
        column->setSpacing(3);
        const QColor accent = ally ? allyAccent : enemyAccent;
        {
            auto *row = new QHBoxLayout;
            row->setSpacing(6);
            auto *name = hudLabel(QStringLiteral("前哨站"), 10, muted, strip);
            name->setFixedWidth(50);
            hudFonts_.append({name, 10});
            auto *bar = new TacticalBar(strip);
            bar->setAccent(accent);
            bar->setFixedHeight(6);
            auto *value = hudLabel(QStringLiteral("--"), 10, primary, strip, true);
            value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            value->setFixedWidth(36);
            hudFonts_.append({value, 10});
            row->addWidget(name);
            row->addWidget(bar, 1);
            row->addWidget(value);
            column->addLayout(row);
            if (ally) { hudOutpostAlly_ = value; hudOutpostAllyBar_ = bar; }
            else { hudOutpostEnemy_ = value; hudOutpostEnemyBar_ = bar; }
        }
        for (int i = 0; i < 4; ++i) {
            auto *row = new QHBoxLayout;
            row->setSpacing(6);
            auto *name = hudLabel(QStringLiteral("%1 %2").arg(squadIds.at(i)).arg(squadNames.at(i)), 10, muted, strip);
            name->setFixedWidth(50);
            hudFonts_.append({name, 10});
            auto *bar = new TacticalBar(strip);
            bar->setAccent(accent);
            bar->setFixedHeight(4);
            values[static_cast<size_t>(i)] = hudLabel(QStringLiteral("--"), 10, primary, strip);
            values[static_cast<size_t>(i)]->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            values[static_cast<size_t>(i)]->setFixedWidth(36);
            hudFonts_.append({values[static_cast<size_t>(i)], 10});
            bars[static_cast<size_t>(i)] = bar;
            row->addWidget(name);
            row->addWidget(bar, 1);
            row->addWidget(values[static_cast<size_t>(i)]);
            column->addLayout(row);
        }
        if (ally) hudAllySquad_ = strip;
        else hudEnemySquad_ = strip;
    };
    buildSquad(true, hudAllyHp_, hudAllyHpBar_);
    buildSquad(false, hudEnemyHp_, hudEnemyHpBar_);

    // 5. 右上角链路状态 + 实时图传 FPS 胶囊
    hudLinkCard_ = makeCard(ui_theme::kCyan, false, 132);
    auto *linkLayout = new QHBoxLayout(hudLinkCard_);
    linkLayout->setContentsMargins(12, 0, 12, 0);
    hudLink_ = hudLabel(QStringLiteral("● 未连接 · H 专注 · O 兑换"), 11, muted, hudLinkCard_);
    hudFonts_.append({hudLink_, 11});
    linkLayout->addWidget(hudLink_);
    linkLayout->addStretch();

    // 6. 左下角：本机液态玻璃战术卡（悍匠水印 + 残影血条 + 内嵌 8 模块自检微胶囊）
    hudSelfCard_ = makeCard(ui_theme::kGold, true, 148);
    auto *selfLayout = new QVBoxLayout(hudSelfCard_);
    selfLayout->setContentsMargins(14, 9, 14, 9);
    selfLayout->setSpacing(5);
    auto *selfTop = new QHBoxLayout;
    selfTop->setSpacing(8);
    hudSelfId_ = hudLabel(QStringLiteral("--"), 26, ui_theme::kGoldLight, hudSelfCard_, true);
    auto *selfRole = hudLabel(roleLabel(), 12, muted, hudSelfCard_);
    hudSelfHp_ = hudLabel(QStringLiteral("-- / --"), 30, primary, hudSelfCard_, true);
    hudSelfHp_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    hudFonts_.append({hudSelfId_, 26});
    hudFonts_.append({selfRole, 12});
    hudFonts_.append({hudSelfHp_, 30});
    selfTop->addWidget(hudSelfId_);
    selfTop->addWidget(selfRole);
    selfTop->addStretch();
    selfTop->addWidget(hudSelfHp_);
    hudSelfBar_ = new TacticalBar(hudSelfCard_);
    hudSelfBar_->setAccent(allyAccent);
    hudSelfBar_->setFixedHeight(7);
    hudSelfKv_ = hudLabel(QStringLiteral("弹量 --  ｜  热量 -- / --  ｜  复活 --"), 12, muted, hudSelfCard_);
    hudFonts_.append({hudSelfKv_, 12});
    hudModuleStrip_ = new ModuleStrip(hudSelfCard_);
    hudModuleStrip_->setFixedHeight(19);
    selfLayout->addLayout(selfTop);
    selfLayout->addWidget(hudSelfBar_);
    selfLayout->addWidget(hudSelfKv_);
    selfLayout->addWidget(hudModuleStrip_);

    // 7. 中下方：液态玻璃半透明悬浮消息流
    hudFeed_ = new FloatingMessageFeed(hud);
    hudFeed_->setBackdropSource(combatVideo_);

    // 8. 右下角：液态玻璃悬浮战术小地图（含己方视野扇形锥，M 键缩放）
    hudMapCard_ = makeCard(ui_theme::kGold, false, 146);
    auto *mapCardLayout = new QVBoxLayout(hudMapCard_);
    mapCardLayout->setContentsMargins(10, 8, 10, 8);
    mapCardLayout->setSpacing(4);
    auto *mapHead = new QHBoxLayout;
    auto *mapTitle = hudLabel(QStringLiteral("TACTICAL MAP // 战术地图"), 10, ui_theme::kGoldLight, hudMapCard_, true);
    hudLegend_ = hudLabel(QStringLiteral("雷达 -- · M 缩放"), 10, muted, hudMapCard_);
    hudLegend_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    hudFonts_.append({mapTitle, 10});
    hudFonts_.append({hudLegend_, 10});
    mapHead->addWidget(mapTitle);
    mapHead->addStretch();
    mapHead->addWidget(hudLegend_);
    mapCardLayout->addLayout(mapHead);
    mapCardLayout->addWidget(mapHud_, 1);

    // 9. 中心动态战术准星
    crosshair_ = new Crosshair(hud);
    crosshair_->setVisible(crosshairAction_ ? crosshairAction_->isChecked() : true);

    combat_->setLayoutHandler([this](const QRectF &content) { layoutCombatHud(content); });
    updateCombatHud();
}

void Window::layoutCombatHud(const QRectF &content) {
    if (!combat_ || content.width() <= 0 || content.height() <= 0) return;
    const int W = qRound(content.width());
    const int H = qRound(content.height());
    hudScale_ = qBound(0.65, qMin(W / 1920.0, H / 1080.0), 1.30);
    const auto sc = [this](int px) { return qMax(1, qRound(px * hudScale_)); };

    if (vignette_) vignette_->setGeometry(0, 0, W, H);

    const int margin = sc(22);
    const int gap = sc(10);
    const int topH = sc(74);
    const int clockW = sc(244);
    const int clockH = sc(84);
    const int brandW = sc(248);

    // 顶部一体化战术眉条：悍匠 Logo 锚点 + 己方大斜切血条 + 中央计时核心 + 敌方大斜切血条
    const int clockX = (W - clockW) / 2;
    if (hudBrandCard_) hudBrandCard_->setGeometry(margin, margin, brandW, topH);
    if (hudBrandLogo_) hudBrandLogo_->setBadgeSize(sc(46));
    if (hudClockCard_) hudClockCard_->setGeometry(clockX, margin - sc(2), clockW, clockH);

    const int allyBarX = margin + brandW + sc(10);
    const int allyBarW = qMax(sc(220), clockX - allyBarX - sc(6));
    if (hudAllyBigBar_) {
        hudAllyBigBar_->setGeometry(allyBarX, margin, allyBarW, topH);
        hudAllyBigBar_->setScale(hudScale_);
    }

    const int enemyBarX = clockX + clockW + sc(6);
    const int enemyBarW = qMax(sc(220), W - margin - enemyBarX);
    if (hudEnemyBigBar_) {
        hudEnemyBigBar_->setGeometry(enemyBarX, margin, enemyBarW, topH);
        hudEnemyBigBar_->setScale(hudScale_);
    }

    // 两侧编队条与链路胶囊（专注模式下隐藏）
    const int squadW = sc(284);
    const int squadH = sc(126);
    const int linkH = sc(34);
    if (hudAllySquad_) {
        hudAllySquad_->setVisible(!focusMode_);
        hudAllySquad_->setGeometry(margin, margin + topH + gap, squadW, squadH);
    }
    if (hudEnemySquad_) {
        hudEnemySquad_->setVisible(!focusMode_);
        hudEnemySquad_->setGeometry(W - margin - squadW, margin + topH + gap, squadW, squadH);
    }
    if (hudLinkCard_) {
        hudLinkCard_->setVisible(!focusMode_);
        hudLinkCard_->setGeometry(W - margin - squadW, margin + topH + gap + squadH + gap, squadW, linkH);
    }

    // 左下角：本机液态玻璃卡（含 8 模块微胶囊）
    const int selfW = sc(416);
    const int selfH = sc(130);
    if (hudSelfCard_) hudSelfCard_->setGeometry(margin, H - margin - selfH, selfW, selfH);
    if (hudModuleStrip_) {
        hudModuleStrip_->setFixedHeight(qMax(15, sc(20)));
        hudModuleStrip_->setScale(hudScale_);
    }

    // 右下角：液态玻璃悬浮战术小地图（专注模式或 M=2 时隐藏）
    int mapW = 0;
    if (hudMapCard_) {
        if (focusMode_ || minimapMode_ == 2) {
            hudMapCard_->hide();
        } else {
            hudMapCard_->show();
            mapW = minimapMode_ == 1 ? qMin(W - margin * 2 - selfW - sc(40), sc(640)) : sc(356);
            const int mapH = minimapMode_ == 1 ? qMin(H - margin * 2 - sc(140), sc(410)) : sc(238);
            hudMapCard_->setGeometry(W - margin - mapW, H - margin - mapH, mapW, mapH);
        }
    }

    // 中下方：液态玻璃悬浮消息流
    if (hudFeed_) {
        hudFeed_->setVisible(!focusMode_);
        const int feedLeft = margin + selfW + sc(22);
        const int feedRight = W - margin - (mapW > 0 ? mapW + sc(22) : 0);
        const int feedW = qMax(sc(260), qMin(sc(660), feedRight - feedLeft));
        const int feedH = sc(168);
        hudFeed_->setGeometry(feedLeft, H - margin - feedH, feedW, feedH);
        hudFeed_->setScale(hudScale_);
    }

    // 中心动态战术准星
    if (crosshair_) {
        const int ch = sc(156);
        crosshair_->setGeometry((W - ch) / 2, (H - ch) / 2, ch, ch);
    }

    for (const auto &entry : hudFonts_) {
        const int px = qMax(9, qRound(entry.second * hudScale_));
        QFont font = entry.first->font();
        font.setPixelSize(px);
        entry.first->setFont(font);
        entry.first->setStyleSheet(QStringLiteral("color:%1;background:transparent;font-size:%2px;")
                                       .arg(entry.first->property("hudColor").toString())
                                       .arg(px));
    }
    for (auto *card : hudCards_)
        card->setRadius(qMax(6.0, 10.0 * hudScale_));
    if (hudSelfBar_) hudSelfBar_->setFixedHeight(qMax(4, sc(7)));
}

void Window::updateCombatHud() {
    if (!combat_) return;
    const auto number = [](const QJsonObject &object, const char *key, const QString &fallback) {
        const auto value = object.value(QLatin1String(key));
        return value.isDouble() ? QString::number(value.toInt()) : fallback;
    };
    const bool blue = ownSide_ == 1;
    const QColor allyColor = blue ? ui_theme::kBlue : ui_theme::kRed;
    const QColor enemyColor = blue ? ui_theme::kRed : ui_theme::kBlue;

    if (hudAllyBigBar_) {
        hudAllyBigBar_->setAccent(allyColor);
        hudAllyBigBar_->setTitle(QStringLiteral("己方基地 // %1").arg(blue ? QStringLiteral("BLUE BASE") : QStringLiteral("RED BASE")));
    }
    if (hudEnemyBigBar_) {
        hudEnemyBigBar_->setAccent(enemyColor);
        hudEnemyBigBar_->setTitle(QStringLiteral("敌方基地 // %1").arg(blue ? QStringLiteral("RED BASE") : QStringLiteral("BLUE BASE")));
    }
    if (hudSelfBar_) hudSelfBar_->setAccent(allyColor);
    if (hudOutpostAllyBar_) hudOutpostAllyBar_->setAccent(allyColor);
    if (hudOutpostEnemyBar_) hudOutpostEnemyBar_->setAccent(enemyColor);
    for (auto *bar : hudAllyHpBar_) if (bar) bar->setAccent(allyColor);
    for (auto *bar : hudEnemyHpBar_) if (bar) bar->setAccent(enemyColor);

    // 基地大斜切总血条
    const auto unit = messageFields("GlobalUnitStatus");
    const auto baseAlly = number(unit, "baseHealth", QStringLiteral("--"));
    const auto baseEnemy = number(unit, "enemyBaseHealth", QStringLiteral("--"));
    const int baseMax = qEnvironmentVariableIntValue("HJ_BASE_MAX_HP") > 0
        ? qEnvironmentVariableIntValue("HJ_BASE_MAX_HP") : demoMode_ ? 5000 : 0;
    const auto updateBigBar = [baseMax](LargeSkewedHealthBar *bar, const QString &text) {
        if (!bar) return;
        bool ok = false;
        const uint v = text.toUInt(&ok);
        const bool valid = ok && baseMax > 0 && v <= static_cast<uint>(baseMax);
        bar->setHealth(text, valid ? static_cast<qreal>(v) / baseMax : 0.0, valid);
    };
    updateBigBar(hudAllyBigBar_, baseAlly);
    updateBigBar(hudEnemyBigBar_, baseEnemy);

    // 前哨站血量
    const auto outpostAlly = number(unit, blue ? "outpostBHealth" : "outpostRHealth", QStringLiteral("--"));
    const auto outpostEnemy = number(unit, blue ? "outpostRHealth" : "outpostBHealth", QStringLiteral("--"));
    if (hudOutpostAlly_) hudOutpostAlly_->setText(outpostAlly);
    if (hudOutpostEnemy_) hudOutpostEnemy_->setText(outpostEnemy);
    const int outpostMax = qEnvironmentVariableIntValue("HJ_OUTPOST_MAX_HP") > 0
        ? qEnvironmentVariableIntValue("HJ_OUTPOST_MAX_HP") : 700;
    const auto updateSmallBar = [](TacticalBar *bar, const QString &text, int maxVal) {
        if (!bar) return;
        bool ok = false;
        const uint v = text.toUInt(&ok);
        if (ok && maxVal > 0 && v <= static_cast<uint>(maxVal)) bar->setRatio(static_cast<qreal>(v) / maxVal);
        else bar->reset();
    };
    updateSmallBar(hudOutpostAllyBar_, outpostAlly, outpostMax);
    updateSmallBar(hudOutpostEnemyBar_, outpostEnemy, outpostMax);

    // 计时与比分
    const auto game = messageFields("GameStatus");
    const qint64 gameAge = dataAgeMs("GameStatus");
    const bool gameStale = gameAge < 0 || gameAge > 3000;
    if (hudClock_) {
        const auto seconds = game.value(QStringLiteral("stageCountdownSec"));
        if (!gameStale && seconds.isDouble()) {
            const int total = qMax(0, seconds.toInt());
            hudClock_->setText(QStringLiteral("%1:%2").arg(total / 60, 2, 10, QLatin1Char('0')).arg(total % 60, 2, 10, QLatin1Char('0')));
        } else {
            hudClock_->setText(QStringLiteral("--:--"));
        }
    }
    if (hudScore_) {
        const auto red = number(game, "redScore", QStringLiteral("--"));
        const auto blueScore = number(game, "blueScore", QStringLiteral("--"));
        hudScore_->setText(QStringLiteral("RED %1  :  %2 BLUE").arg(red, blueScore));
    }

    // 链路与图传实时帧率
    if (hudLink_) {
        const double fps = combatVideo_ ? combatVideo_->videoFps() : 0.0;
        const QString sig = connected_ ? QStringLiteral("▂▄▆█") : QStringLiteral("▂▁▁▁");
        const QString vid = gotVideo_ && fps > 0.5
            ? QStringLiteral("%1 FPS").arg(qRound(fps))
            : gotVideo_ ? QStringLiteral("正常") : QStringLiteral("待机");
        hudLink_->setText(QStringLiteral("%1 %2 · 图传 %3 · H 专注 · O 兑换")
                              .arg(connected_ ? QStringLiteral("● 在线") : QStringLiteral("● 离线"), sig, vid));
    }

    // 本机状态 + 中心准星双弧遥测 + 低血量/受击暗角
    const auto dynamic = messageFields("RobotDynamicStatus");
    const auto respawn = messageFields("RobotRespawnStatus");
    const auto staticSt = messageFields("RobotStaticStatus");
    const auto hp = number(dynamic, "currentHealth", QStringLiteral("--"));
    int maxHp = 0;
    if (ownSide_ >= 0) {
        const int ids[] = {1, 3, 4, 7};
        int slot = 0;
        for (int i = 0; i < 4; ++i) if (ids[i] == static_cast<int>(number_)) slot = i;
        const int index = (ownSide_ == 0 ? 0 : 4) + slot;
        maxHp = static_cast<int>(maxHealth_[static_cast<size_t>(index)]);
        if (maxHealthAt_[static_cast<size_t>(index)] < 0) maxHp = 0;
    }
    if (hudSelfId_) hudSelfId_->setText(number_ > 0 ? QStringLiteral("%1").arg(number_, 2, 10, QLatin1Char('0')) : QStringLiteral("--"));
    if (hudSelfHp_) hudSelfHp_->setText(maxHp > 0 ? QStringLiteral("%1 / %2").arg(hp).arg(maxHp) : QStringLiteral("%1 / --").arg(hp));

    qreal hpRatio = -1.0;
    if (hudSelfBar_) {
        bool ok = false;
        const int value = hp.toInt(&ok);
        if (ok && maxHp > 0 && value <= maxHp) {
            hpRatio = static_cast<qreal>(value) / maxHp;
            hudSelfBar_->setRatio(hpRatio);
            if (vignette_) {
                if (lastSelfHp_ > 0 && value < lastSelfHp_) vignette_->triggerHitFlash();
                vignette_->setLowHealthRatio(hpRatio);
            }
            lastSelfHp_ = value;
        } else {
            hudSelfBar_->reset();
            if (vignette_) vignette_->setLowHealthRatio(-1.0);
        }
    }

    qreal heatRatio = -1.0;
    if (hudSelfKv_) {
        const auto ammo = number(dynamic, "remainingAmmo", QStringLiteral("--"));
        const auto heatVal = dynamic.value(QStringLiteral("currentHeat"));
        const auto maxHeatVal = staticSt.value(QStringLiteral("maxHeat"));
        const QString heatStr = heatVal.isDouble() ? QString::number(heatVal.toDouble(), 'f', 0) : QStringLiteral("--");
        const QString maxHeatStr = maxHeatVal.isDouble() ? QString::number(maxHeatVal.toInt()) : QStringLiteral("--");
        if (heatVal.isDouble() && maxHeatVal.isDouble() && maxHeatVal.toDouble() > 0.0)
            heatRatio = heatVal.toDouble() / maxHeatVal.toDouble();
        const bool pending = respawn.value(QStringLiteral("isPendingRespawn")).toBool();
        hudSelfKv_->setText(QStringLiteral("弹量 %1  ｜  热量 %2 / %3  ｜  复活 %4")
                                .arg(ammo, heatStr, maxHeatStr, pending ? QStringLiteral("等待") : QStringLiteral("就绪")));
    }
    if (crosshair_) crosshair_->setTelemetry(heatRatio, hpRatio);

    // 双方编队血条
    const auto setSquadRow = [this](std::array<QLabel *, 4> &values, std::array<TacticalBar *, 4> &bars, int i, int index) {
        auto *bar = bars[static_cast<size_t>(i)];
        auto *value = values[static_cast<size_t>(i)];
        if (!bar || !value) return;
        const size_t idx = static_cast<size_t>(index);
        const bool valid = idx < health_.size() && maxHealthAt_[idx] >= 0 && maxHealth_[idx] > 0 &&
                           health_[idx] <= maxHealth_[idx];
        if (!valid) {
            bar->reset();
            value->setText(idx < health_.size() ? QString::number(health_[idx]) : QStringLiteral("--"));
            return;
        }
        bar->setRatio(static_cast<qreal>(health_[idx]) / maxHealth_[idx]);
        value->setText(QString::number(health_[idx]));
    };
    for (int i = 0; i < 4; ++i) {
        setSquadRow(hudAllyHp_, hudAllyHpBar_, i, i);
        setSquadRow(hudEnemyHp_, hudEnemyHpBar_, i, i + 4);
    }

    // 地图标记同步
    const uint ownId = number_ > 0 ? static_cast<uint>(number_ + (ownSide_ == 1 ? 100 : 0)) : 0;
    const std::vector<uint32_t> maxHealth(maxHealth_.begin(), maxHealth_.end());
    for (auto *m : {map_, mapHud_})
        if (m) m->setStatus(ownId, health_, maxHealth);

    if (hudLegend_) {
        const qint64 radarAge = dataAgeMs("RadarInfoToClient");
        hudLegend_->setText(radarAge < 0 ? QStringLiteral("雷达 -- · M 缩放")
                                         : QStringLiteral("雷达 %1s 前 · M 缩放").arg(radarAge / 1000));
    }
}

void Window::buildExchangePanel() {
    auto *panel = new FloatingPanel(QStringLiteral("弹丸兑换"), this);
    panel->setBackdropSource(combatVideo_);
    exchangePanel_ = panel;
    panel->hide();
    panel->resize(520, 390);

    auto *layout = panel->body();
    layout->setContentsMargins(4, 0, 4, 0);
    layout->setSpacing(10);
    exchangeKind_ = new QLabel(panel);
    exchangeKind_->setStyleSheet(QStringLiteral("color:#eaf1f7;font-size:16px;"));
    layout->addWidget(exchangeKind_);
    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(8);
    const auto addRow = [&](int row, const QString &key, QLabel *&value) {
        auto *k = new QLabel(key, exchangePanel_);
        k->setStyleSheet(QStringLiteral("color:#8da0b3;font-size:12px;"));
        value = new QLabel(QStringLiteral("--"), exchangePanel_);
        value->setStyleSheet(QStringLiteral("color:#eaf1f7;font-size:15px;font-weight:700;"));
        grid->addWidget(k, row, 0);
        grid->addWidget(value, row, 1);
    };
    addRow(0, QStringLiteral("已有弹量"), exchangeOwned_);
    addRow(1, QStringLiteral("可兑换上限"), exchangeLimit_);
    addRow(2, QStringLiteral("当前金币"), exchangeGold_);
    layout->addLayout(grid);
    auto *qtyRow = new QHBoxLayout;
    auto *qtyLabel = new QLabel(QStringLiteral("兑换数量"), exchangePanel_);
    qtyLabel->setStyleSheet(QStringLiteral("color:#8da0b3;font-size:12px;"));
    exchangeQty_ = new QSpinBox(exchangePanel_);
    exchangeQty_->setRange(0, 1000);
    exchangeQty_->setSingleStep(10);
    exchangeQty_->setValue(100);
    exchangeQty_->setSuffix(QStringLiteral(" 发"));
    exchangeQty_->setStyleSheet(QStringLiteral("QSpinBox{background:#131c26;color:#eaf1f7;border:1px solid #3a4656;"
                                               "border-radius:6px;padding:4px 8px;font-size:14px;}"));
    qtyRow->addWidget(qtyLabel);
    qtyRow->addWidget(exchangeQty_, 1);
    layout->addLayout(qtyRow);
    // 一键快捷弹量胶囊按钮（赛场免微调）
    auto *quickRow = new QHBoxLayout;
    quickRow->setSpacing(6);
    for (int preset : {50, 100, 200}) {
        auto *btn = new QPushButton(QStringLiteral("+%1 发").arg(preset), panel);
        btn->setStyleSheet(QStringLiteral(
            "QPushButton{background:rgba(232,163,61,32);color:#ffc971;border:1px solid rgba(232,163,61,130);"
            "border-radius:6px;padding:4px 10px;font-size:12px;font-weight:700;}"
            "QPushButton:hover{background:rgba(232,163,61,72);}"));
        connect(btn, &QPushButton::clicked, this, [this, preset] {
            if (exchangeQty_) exchangeQty_->setValue(preset);
        });
        quickRow->addWidget(btn);
    }
    quickRow->addStretch();
    layout->addLayout(quickRow);
    exchangeRemote_ = new QCheckBox(QStringLiteral("远程兑换（协议 cmd_type 5 / 6）"), panel);
    exchangeRemote_->setStyleSheet(QStringLiteral("color:#8da0b3;font-size:13px;"));
    layout->addWidget(exchangeRemote_);
    exchangeHint_ = new QLabel(QStringLiteral("数量须为 10 的倍数；发送后由客户端上行"), exchangePanel_);
    exchangeHint_->setStyleSheet(QStringLiteral("color:#ffc46a;font-size:13px;"));
    layout->addWidget(exchangeHint_);
    exchangeStatus_ = new QLabel(exchangePanel_);
    exchangeStatus_->setStyleSheet(QStringLiteral("color:#7fe0a8;font-size:14px;font-weight:700;"));
    layout->addWidget(exchangeStatus_);
    layout->addStretch();
    auto *buttons = new QHBoxLayout;
    auto *close = new QPushButton(QStringLiteral("关闭"), exchangePanel_);
    auto *send = new QPushButton(QStringLiteral("发送兑换"), exchangePanel_);
    send->setStyleSheet(QStringLiteral("QPushButton{background:#e8a33d;color:#1a1206;font-weight:700;"
                                       "border:none;border-radius:7px;padding:8px 18px;}"));
    close->setStyleSheet(QStringLiteral("QPushButton{background:#131c26;color:#eaf1f7;border:1px solid #3a4656;"
                                        "border-radius:7px;padding:8px 18px;}"));
    connect(close, &QPushButton::clicked, exchangePanel_, &QWidget::hide);
    connect(send, &QPushButton::clicked, this, &Window::sendExchange);
    auto *respawn = new QPushButton(QStringLiteral("立即买活"), exchangePanel_);
    respawn->setStyleSheet(QStringLiteral("QPushButton{background:#2b3a4d;color:#ffd9a0;border:1px solid #4a5c70;"
                                          "border-radius:7px;padding:8px 14px;}"));
    connect(respawn, &QPushButton::clicked, this, &Window::sendRespawn);
    buttons->addWidget(respawn);
    buttons->addStretch();
    buttons->addWidget(close);
    buttons->addWidget(send);
    layout->addLayout(buttons);

    auto *shortcut = new QShortcut(QKeySequence(Qt::Key_O), this);
    connect(shortcut, &QShortcut::activated, this, &Window::toggleExchangePanel);
    if (qEnvironmentVariableIntValue("HJ_OPEN_EXCHANGE") == 1)
        QTimer::singleShot(500, this, [this] { toggleExchangePanel(); });
}

void Window::toggleExchangePanel() {
    if (!exchangePanel_) return;
    if (exchangePanel_->isVisible()) {
        exchangePanel_->hide();
        return;
    }
    updateExchangePanel();
    exchangePanel_->move((width() - exchangePanel_->width()) / 2, (height() - exchangePanel_->height()) / 2);
    if (auto *fp = dynamic_cast<FloatingPanel *>(exchangePanel_)) fp->showAnimated();
    else { exchangePanel_->show(); exchangePanel_->raise(); }
}

void Window::updateExchangePanel() {
    if (!exchangePanel_) return;
    switch (role_) {
    case Role::Hero: exchangeCmdType_ = 2; break;
    case Role::Infantry: exchangeCmdType_ = 1; break;
    default: exchangeCmdType_ = 0; break;
    }
    const QString kind = exchangeCmdType_ == 2 ? QStringLiteral("42mm") : exchangeCmdType_ == 1 ? QStringLiteral("17mm") : QString();
    exchangeKind_->setText(exchangeCmdType_ == 0
        ? QStringLiteral("当前兵种不支持兑换")
        : QStringLiteral("类型：%1 弹丸（普通 cmd_type %2 / 远程 %3）")
              .arg(kind)
              .arg(exchangeCmdType_ == 2 ? 2 : 1)
              .arg(exchangeCmdType_ == 2 ? 6 : 5));
    const auto number = [](const QJsonObject &object, const char *key) {
        const auto value = object.value(QLatin1String(key));
        return value.isDouble() ? QString::number(value.toInt()) : QStringLiteral("--");
    };
    exchangeOwned_->setText(number(messageFields("RobotDynamicStatus"), "remainingAmmo"));
    exchangeLimit_->setText(number(messageFields("Buff"), "buffLevel"));
    exchangeGold_->setText(number(messageFields("GlobalLogisticsStatus"), "remainingEconomy"));
    exchangeQty_->setEnabled(exchangeCmdType_ != 0);
    if (exchangeCmdType_ == 0) exchangeHint_->setText(QStringLiteral("英雄=42mm，步兵=17mm；其他兵种不支持"));
    // 刷新只更新数值，不覆盖提示/结果（避免“发送成功”被立刻刷掉）
}

void Window::sendExchange() {
    if (!exchangePanel_ || exchangeCmdType_ == 0) return;
    if (!client_) {
        exchangeHint_->setText(QStringLiteral("未连接，无法发送"));
        return;
    }
    const int quantity = exchangeQty_->value();
    if (quantity <= 0 || quantity % 10 != 0) {
        exchangeHint_->setText(QStringLiteral("数量必须是 10 的正整数倍"));
        return;
    }
    auto command = std::make_unique<rm2027::CommonCommand>();
    // 协议 cmd_type：1=兑换17mm 2=兑换42mm 5=远程兑换发弹量 6=远程兑换血量
    int cmdType = exchangeCmdType_;
    if (exchangeRemote_ && exchangeRemote_->isChecked()) cmdType = exchangeCmdType_ == 2 ? 6 : 5;
    command->cmd_type = static_cast<uint32_t>(cmdType);
    command->param = static_cast<uint32_t>(quantity);
    if (client_->send(std::move(command))) {
        // 协议中 CommonCommand 没有结果回执消息，这里只确认“已发送”
        if (exchangeStatus_) {
            exchangeStatus_->setStyleSheet(QStringLiteral("color:#7fe0a8;font-size:14px;font-weight:700;"));
            exchangeStatus_->setText(QStringLiteral("✔ 已发送：兑换 %1 发 %2")
                                         .arg(quantity)
                                         .arg(exchangeCmdType_ == 2 ? QStringLiteral("42mm") : QStringLiteral("17mm")));
            QTimer::singleShot(4000, this, [this] { if (exchangeStatus_) exchangeStatus_->clear(); });
        }
        addEvent(QStringLiteral("兑换请求：%1 发（%2）").arg(quantity).arg(exchangeCmdType_ == 2 ? QStringLiteral("42mm") : QStringLiteral("17mm")));
    } else if (exchangeStatus_) {
        exchangeStatus_->setStyleSheet(QStringLiteral("color:#ff8a93;font-size:14px;font-weight:700;"));
        exchangeStatus_->setText(QStringLiteral("✘ 发送失败（未连接或被限流）"));
        QTimer::singleShot(4000, this, [this] { if (exchangeStatus_) { exchangeStatus_->clear(); exchangeStatus_->setStyleSheet(QStringLiteral("color:#7fe0a8;font-size:14px;font-weight:700;")); } });
    }
}

void Window::buildModulePanelDock() {
    modulePanelDock_ = new FloatingPanel(QStringLiteral("状态检测"), this);
    modulePanelDock_->setBackdropSource(combatVideo_);
    modulePanelDock_->body()->addWidget(modulePanel_);
    modulePanelDock_->resize(360, 200);
    modulePanelDock_->hide();
}

void Window::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape && isFullScreen()) {
        showNormal();
        return;
    }
    if (event->key() == Qt::Key_F11) {
        isFullScreen() ? showNormal() : showFullScreen();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void Window::buildCompactPage() {
    // 已废弃：会把同一批控件重新挂到另一个页面，导致原页面为空；保留空实现避免旧调用点报错。
}

void Window::buildDockPage() {
    // 已废弃：同上，dock 方案不再启用。
}

void Window::sendRespawn() {
    if (!client_) return;
    auto command = std::make_unique<rm2027::CommonCommand>();
    command->cmd_type = 4;   // TODO: 以官方协议表核对“立即复活”的 cmd_type
    command->param = 0;
    if (client_->send(std::move(command))) {
        addEvent(QStringLiteral("买活请求已发送"));
        if (exchangeStatus_) {
            exchangeStatus_->setStyleSheet(QStringLiteral("color:#7fe0a8;font-size:14px;font-weight:700;"));
            exchangeStatus_->setText(QStringLiteral("✔ 已发送：兑换立即复活（cmd_type=4）"));
            QTimer::singleShot(4000, this, [this] { if (exchangeStatus_) exchangeStatus_->clear(); });
        }
    } else if (exchangeStatus_) {
        exchangeStatus_->setText(QStringLiteral("✘ 买活发送失败（未连接）"));
        QTimer::singleShot(4000, this, [this] { if (exchangeStatus_) exchangeStatus_->clear(); });
    }
}

QWidget *Window::addModule(const char *id, const QString &title, QWidget *content) {
    auto *module = new QWidget(workspace_->widget(0));
    module->setObjectName(QString::fromLatin1(id));
    auto *layout = new QVBoxLayout(module);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    auto *handle = new ModuleTitle(title);
    layout->addWidget(handle);
    layout->addWidget(content, 1);
    if (auto *heading = content->findChild<QWidget *>("panelTitle")) heading->hide();
    if (auto *rule = content->findChild<QWidget *>("goldRule")) rule->hide();
    handle->canDrop = [this, module](QWidget *source) {
        if (source == module) return false;
        const auto *primary = mapPriority_ ? mapDock_ : videoDock_;
        if (source == primary || module == primary)
            return (source == mapDock_ || source == videoDock_) &&
                   (module == mapDock_ || module == videoDock_);
        return sidebarModules_.contains(source) && sidebarModules_.contains(module);
    };
    handle->dropped = [this, module](QWidget *source) { moveModule(source, module); };
    return module;
}

void Window::arrangeModules() {
    const auto previousSizes = sidebar_->sizes();
    // 左列：主视图（地图或图传，F2 互换）+ 下方事件/数据
    QWidget *primaryView = videoPriority_ ? videoDock_ : mapDock_;
    QWidget *secondaryView = videoPriority_ ? mapDock_ : videoDock_;
    primaryLayout_->addWidget(primaryView);
    primaryLayout_->addWidget(bottomLeft_);
    primaryLayout_->setStretch(0, 8);
    primaryLayout_->setStretch(1, 2);
    primaryView->show();
    secondaryView->show();
    // 右列：双方机器人（上）+ 副视图（下）
    QList<QWidget *> rightColumn{allyModule_, secondaryView};
    for (int index = 0; index < rightColumn.size(); ++index) {
        sidebar_->insertWidget(index, rightColumn.at(index));
        sidebar_->setStretchFactor(index, index == 0 ? 2 : 3);
        rightColumn.at(index)->show();
    }
    if (previousSizes.size() == sidebar_->count())
        sidebar_->setSizes(previousSizes);
    const int columnHeight = qMax(sidebar_->height(), 720);
    sidebar_->setSizes({columnHeight * 55 / 100, columnHeight * 45 / 100});
}

void Window::applyDefaultLayout() {
    // 启动与“恢复默认布局”共用同一套比例，避免两处数值不一致
    videoPriority_ = false;
    if (detailsDock_) detailsDock_->hide();
    arrangeModules();
    workspace_->setSizes({qMax(width() - 560, 680), 560});
    const int columnHeight = qMax(sidebar_->height(), 720);
    sidebar_->setSizes({columnHeight * 55 / 100, columnHeight * 45 / 100});
}

void Window::moveModule(QWidget *source, QWidget *target) {
    auto *primary = mapPriority_ ? mapDock_ : videoDock_;
    if (source == primary || target == primary) {
        focusPrimaryView();
        return;
    }
    sidebarModules_.move(sidebarModules_.indexOf(source), sidebarModules_.indexOf(target));
    arrangeModules();
}

void Window::focusPrimaryView() {
    auto *primary = mapPriority_ ? mapDock_ : videoDock_;
    auto *alternate = mapPriority_ ? videoDock_ : mapDock_;
    sidebarModules_[sidebarModules_.indexOf(alternate)] = primary;
    mapPriority_ = !mapPriority_;
    arrangeModules();
}

bool Window::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::Resize) {
        const bool narrow = static_cast<QResizeEvent *>(event)->size().width() < 460;
        if (watched == detailsDock_) {
            if (auto *split = detailsDock_->findChild<QSplitter *>())
                split->setOrientation(narrow ? Qt::Vertical : Qt::Horizontal);
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

Window::Role Window::roleOfId(uint number) {
    switch (number) {
    case 1: return Role::Hero;
    case 3: case 4: return Role::Infantry;
    case 6: return Role::Aerial;
    case 7: return Role::Sentry;
    case 8: return Role::Dart;
    default: return Role::Unknown;
    }
}

QString Window::roleLabel() const {
    QString name;
    switch (role_) {
    case Role::Hero: name = QStringLiteral("英雄"); break;
    case Role::Infantry: name = QStringLiteral("步兵"); break;
    case Role::Aerial: name = QStringLiteral("空中"); break;
    case Role::Sentry: name = QStringLiteral("哨兵"); break;
    case Role::Dart: name = QStringLiteral("飞镖"); break;
    default: name = QStringLiteral("未识别兵种"); break;
    }
    if (side_ < 0) return name;
    return QStringLiteral("%1 %2 · %3").arg(name).arg(number_)
        .arg(side_ == 0 ? QStringLiteral("红方") : QStringLiteral("蓝方"));
}

bool Window::roleNeedsPanel() const {
    return role_ != Role::Infantry && role_ != Role::Unknown;
}

void Window::closeEvent(QCloseEvent *event) {
    auto settings = preferences();
    settings.setValue("view/fullScreen", isFullScreen());
    QMainWindow::closeEvent(event);
}

void Window::applyWindowPreferences() {
    const auto settings = preferences();
    if (settings.value("view/fullScreen", false).toBool())
        setWindowState(windowState() | Qt::WindowFullScreen);
}
