#include "window.h"
#include "window_helpers.h"
#include "combat_view.h"
#include "map.h"
#include "brand.h"
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
    auto *toolbar = addToolBar(QStringLiteral("窗口布局"));
    toolbar->setObjectName("layoutToolbar");
    toolbar->setMovable(false);
    auto *switchView = toolbar->addAction(QStringLiteral("切换地图 / 图传  F2"));
    switchView->setShortcut(QKeySequence(Qt::Key_F2));
    switchView->setAutoRepeat(false);
    connect(switchView, &QAction::triggered, this, [this] {
        if (combat_ && mainStack_ && mainStack_->currentWidget() == combat_)
            combat_->stack()->setCurrentIndex(1 - combat_->stack()->currentIndex());
        else
            focusPrimaryView();
    });
    auto *crosshairToggle = toolbar->addAction(QStringLiteral("准星  F4"));
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
    connect(dataViewAction, &QAction::triggered, this, [this] {
        if (!mainStack_ || !combat_) return;
        mainStack_->setCurrentIndex(mainStack_->currentWidget() == combat_ ? 1 : 0);
        updateCombatHud();
    });
    connect(toolbar->addAction(QStringLiteral("紧凑布局  F5")), &QAction::triggered, this, [this] {
        if (!mainStack_) return;
        if (compactPage_ && mainStack_->currentWidget() == compactPage_) {
            arrangeModules();
            mainStack_->setCurrentWidget(dataPage_);
            return;
        }
        if (!compactPage_) {
            buildCompactPage();
            mainStack_->addWidget(compactPage_);
        }
        mainStack_->setCurrentWidget(compactPage_);
    });
    connect(toolbar->addAction(QStringLiteral("消息详情")), &QAction::triggered, this, &Window::showDetails);
    connect(toolbar->addAction(QStringLiteral("恢复默认布局")), &QAction::triggered, this, [this] {
        sidebarModules_ = defaultSidebar_;
        mapPriority_ = true;
        detailsDock_->hide();
        arrangeModules();
        workspace_->setSizes({width() * 2 / 3, width() / 3});
        sidebar_->setSizes({1, 1, 1, 1, 0});
    });

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
    auto *flip = new QPushButton(QStringLiteral("切换视角"));
    connect(flip, &QPushButton::clicked, map_, &Map::flip);
    mapLayout->addWidget(flip);
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
    analysisLayout->addWidget(modulePanel_);

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
    sidebar_->setMinimumWidth(360);
    sidebar_->setMaximumWidth(460);
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
    sidebarModules_.append(detailsDock_);
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
    workspace_->setSizes({860, 490});

    buildCombatView();
    buildExchangePanel();
    mainStack_->addWidget(combat_);
    mainStack_->addWidget(dataPage_);
    mainStack_->setCurrentWidget(combat_);
    if (qEnvironmentVariableIntValue("HJ_START_DATA_VIEW") == 1) mainStack_->setCurrentWidget(dataPage_);
    if (qEnvironmentVariableIntValue("HJ_COMPACT") == 1) {
        buildCompactPage();
        mainStack_->addWidget(compactPage_);
        mainStack_->setCurrentWidget(compactPage_);
    }
    if (qEnvironmentVariableIntValue("HJ_DOCKS") == 1) {
        buildDockPage();
        mainStack_->addWidget(dockPage_);
        mainStack_->setCurrentWidget(dockPage_);
    }
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
    mapLayout->addWidget(mapHud_);
    stack->addWidget(mapPage);
    stack->setCurrentIndex(0);

    QWidget *hud = combat_->hud();
    auto makeCard = [this, hud](const QRect &base) {
        auto *card = new HudCard(hud);
        hudGeometry_.append({card, base});
        return card;
    };
    const QColor muted("#8da0b3");
    const QColor primary("#eaf1f7");
    const QColor red("#ff5a67");
    const QColor blue("#3ea8ff");
    const QColor allyAccent = ownSide_ == 1 ? blue : red;
    const QColor enemyAccent = ownSide_ == 1 ? red : blue;
    const QColor brand = allyAccent;

    // 己方 / 敌方基地卡
    auto buildBase = [&](const QRect &rect, bool ally) {
        auto *card = makeCard(rect);
        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(16, 10, 16, 12);
        layout->setSpacing(2);
        auto *cap = hudLabel(ally ? QStringLiteral("己方基地 · RED BASE") : QStringLiteral("敌方基地 · BLUE BASE"), 11, muted, card);
        hudFonts_.append({cap, 11});
        auto *row = new QHBoxLayout;
        row->setSpacing(6);
        auto *value = hudLabel(QStringLiteral("--"), 40, primary, card, true);
        hudFonts_.append({value, 40});
        auto *unit = hudLabel(QStringLiteral("HP"), 15, muted, card);
        hudFonts_.append({unit, 15});
        row->addWidget(value);
        row->addWidget(unit);
        row->addStretch();
        auto *bar = new QProgressBar(card);
        bar->setRange(0, 1000);
        bar->setTextVisible(false);
        bar->setFixedHeight(5);
        const QString color = (ally ? allyAccent : enemyAccent).name();
        bar->setStyleSheet(QStringLiteral(
            "QProgressBar{background:rgba(255,255,255,26);border:none;border-radius:2px;}"
            "QProgressBar::chunk{background:%1;border-radius:2px;}").arg(color));
        layout->addWidget(cap);
        layout->addLayout(row);
        layout->addWidget(bar);
        if (ally) { hudAllyBase_ = value; hudAllyBaseName_ = cap; hudAllyBar_ = bar; }
        else { hudEnemyBase_ = value; hudEnemyBaseName_ = cap; hudEnemyBar_ = bar; }
    };
    buildBase(QRect(36, 32, 320, 96), true);
    buildBase(QRect(1564, 32, 320, 96), false);

    // 计时卡
    auto *clockCard = makeCard(QRect(830, 28, 260, 100));
    auto *clockLayout = new QVBoxLayout(clockCard);
    clockLayout->setContentsMargins(8, 6, 8, 8);
    clockLayout->setSpacing(0);
    hudClock_ = hudLabel(QStringLiteral("--:--"), 56, primary, clockCard, true);
    hudClock_->setAlignment(Qt::AlignCenter);
    hudStage_ = hudLabel(QStringLiteral("等待数据"), 12, brand, clockCard);
    hudStage_->setAlignment(Qt::AlignCenter);
    hudScore_ = hudLabel(QStringLiteral("红 -- : -- 蓝"), 13, muted, clockCard);
    hudScore_->setAlignment(Qt::AlignCenter);
    hudScore_->setTextFormat(Qt::RichText);
    hudFonts_.append({hudClock_, 56});
    hudFonts_.append({hudStage_, 12});
    hudFonts_.append({hudScore_, 13});
    clockLayout->addWidget(hudClock_);
    clockLayout->addWidget(hudStage_);
    clockLayout->addWidget(hudScore_);
    clockLayout->addStretch();

    // 双方机体血条（贴边、低饱和、不抢画面）；前哨站并入本区块，行内一条略粗的条
    const QList<int> squadIds{1, 3, 4, 7};
    const QStringList squadNames{QStringLiteral("重装"), QStringLiteral("步兵"), QStringLiteral("步兵"), QStringLiteral("哨兵")};
    auto buildSquad = [&](const QRect &rect, bool ally, std::array<QLabel *, 4> &values,
                          std::array<QProgressBar *, 4> &bars) {
        auto *strip = new QWidget(hud);
        strip->setAttribute(Qt::WA_TranslucentBackground, true);
        strip->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        hudGeometry_.append({strip, rect});
        auto *column = new QVBoxLayout(strip);
        column->setContentsMargins(2, 0, 2, 0);
        column->setSpacing(3);
        const QString accent = (ally ? allyAccent : enemyAccent).name();
        // 前哨站行（条略粗）
        {
            auto *row = new QHBoxLayout;
            row->setSpacing(7);
            auto *name = hudLabel(QStringLiteral("前哨站"), 10, muted, strip);
            name->setFixedWidth(58);
            hudFonts_.append({name, 10});
            auto *bar = new QProgressBar(strip);
            bar->setRange(0, 1000);
            bar->setTextVisible(false);
            bar->setFixedHeight(6);
            auto *value = hudLabel(QStringLiteral("--"), 10, muted, strip);
            value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            value->setFixedWidth(38);
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
            row->setSpacing(7);
            auto *name = hudLabel(QStringLiteral("%1 %2").arg(squadIds.at(i)).arg(squadNames.at(i)), 10, muted, strip);
            name->setFixedWidth(58);
            hudFonts_.append({name, 10});
            auto *bar = new QProgressBar(strip);
            bar->setRange(0, 1000);
            bar->setTextVisible(false);
            bar->setFixedHeight(4);
            bar->setStyleSheet(QStringLiteral(
                "QProgressBar{background:rgba(255,255,255,22);border:none;border-radius:2px;}"
                "QProgressBar::chunk{background:%1;border-radius:2px;}").arg(accent));
            values[static_cast<size_t>(i)] = hudLabel(QStringLiteral("--"), 10, muted, strip);
            values[static_cast<size_t>(i)]->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            values[static_cast<size_t>(i)]->setFixedWidth(38);
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
    buildSquad(QRect(36, 138, 300, 124), true, hudAllyHp_, hudAllyHpBar_);
    buildSquad(QRect(1584, 138, 300, 124), false, hudEnemyHp_, hudEnemyHpBar_);

    // 链路卡
    auto *linkCard = makeCard(QRect(1564, 276, 320, 36));
    auto *linkLayout = new QHBoxLayout(linkCard);
    linkLayout->setContentsMargins(14, 0, 14, 0);
    hudLink_ = hudLabel(QStringLiteral("● 未连接"), 12, muted, linkCard);
    hudFonts_.append({hudLink_, 12});
    linkLayout->addWidget(hudLink_);
    linkLayout->addStretch();

    // 本机卡
    auto *selfCard = makeCard(QRect(36, 944, 430, 100));
    auto *selfLayout = new QVBoxLayout(selfCard);
    selfLayout->setContentsMargins(16, 10, 16, 12);
    selfLayout->setSpacing(4);
    auto *selfTop = new QHBoxLayout;
    selfTop->setSpacing(10);
    hudSelfId_ = hudLabel(QStringLiteral("--"), 26, brand, selfCard, true);
    auto *selfRole = hudLabel(roleLabel(), 13, muted, selfCard);
    hudSelfHp_ = hudLabel(QStringLiteral("-- / --"), 34, primary, selfCard, true);
    hudSelfHp_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    hudFonts_.append({hudSelfId_, 26});
    hudFonts_.append({selfRole, 13});
    hudFonts_.append({hudSelfHp_, 34});
    selfTop->addWidget(hudSelfId_);
    selfTop->addWidget(selfRole);
    selfTop->addStretch();
    selfTop->addWidget(hudSelfHp_);
    hudSelfBar_ = new QProgressBar(selfCard);
    hudSelfBar_->setRange(0, 1000);
    hudSelfBar_->setTextVisible(false);
    hudSelfBar_->setFixedHeight(5);
    hudSelfBar_->setStyleSheet(QStringLiteral(
        "QProgressBar{background:rgba(255,255,255,26);border:none;border-radius:2px;}"
        "QProgressBar::chunk{background:#ff5a67;border-radius:2px;}"));
    hudSelfKv_ = hudLabel(QStringLiteral("弹量 --　热量 --　复活 --"), 13, muted, selfCard);
    hudFonts_.append({hudSelfKv_, 13});
    selfLayout->addLayout(selfTop);
    selfLayout->addWidget(hudSelfBar_);
    selfLayout->addWidget(hudSelfKv_);

    // 小地图卡
    auto *mapCard = makeCard(QRect(1584, 788, 300, 256));
    auto *mapCardLayout = new QVBoxLayout(mapCard);
    mapCardLayout->setContentsMargins(10, 8, 10, 8);
    mapCardLayout->setSpacing(4);
    auto *mapHead = new QHBoxLayout;
    auto *mapTitle = hudLabel(QStringLiteral("战术地图"), 11, primary, mapCard);
    hudLegend_ = hudLabel(QStringLiteral("雷达 --"), 11, muted, mapCard);
    hudLegend_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    hudFonts_.append({mapTitle, 11});
    hudFonts_.append({hudLegend_, 11});
    mapHead->addWidget(mapTitle);
    mapHead->addStretch();
    mapHead->addWidget(hudLegend_);
    mapCardLayout->addLayout(mapHead);
    mapCardLayout->addWidget(mapHud_, 1);

    // 事件条
    auto *tickerCard = makeCard(QRect(490, 998, 1020, 46));
    auto *tickerLayout = new QHBoxLayout(tickerCard);
    tickerLayout->setContentsMargins(16, 0, 16, 0);
    tickerLayout->setSpacing(12);
    hudTicker_ = hudLabel(QStringLiteral("● 等待数据"), 13, brand, tickerCard);
    hudTickerEvent_ = hudLabel(QString(), 12, muted, tickerCard);
    hudFonts_.append({hudTicker_, 13});
    hudFonts_.append({hudTickerEvent_, 12});
    tickerLayout->addWidget(hudTicker_);
    tickerLayout->addWidget(hudTickerEvent_);
    tickerLayout->addStretch();

    // 准星（F4 开关，默认开）
    crosshair_ = new Crosshair(hud);
    hudGeometry_.append({crosshair_, QRect(900, 480, 120, 120)});
    crosshair_->setVisible(crosshairAction_ ? crosshairAction_->isChecked() : true);

    combat_->setLayoutHandler([this](const QRectF &content) { layoutCombatHud(content); });
    updateCombatHud();
}

void Window::layoutCombatHud(const QRectF &content) {
    if (!combat_) return;
    hudScale_ = content.width() / 1920.0;
    if (hudScale_ <= 0.0) return;
    for (const auto &entry : hudGeometry_) {
        const QRect &base = entry.second;
        entry.first->setGeometry(QRect(qRound(base.x() * hudScale_), qRound(base.y() * hudScale_),
                                       qRound(base.width() * hudScale_), qRound(base.height() * hudScale_)));
    }
    for (const auto &entry : hudFonts_) {
        const int px = qMax(9, qRound(entry.second * hudScale_));
        QFont font = entry.first->font();
        font.setPixelSize(px);
        entry.first->setFont(font);
        // 全局 QSS 里有 QWidget{font-size:16px}，会覆盖 setFont；这里用控件级样式表盖回去。
        entry.first->setStyleSheet(QStringLiteral("color:%1;background:transparent;font-size:%2px;")
                                       .arg(entry.first->property("hudColor").toString())
                                       .arg(px));
    }
    for (const auto &entry : hudGeometry_)
        static_cast<HudCard *>(entry.first)->setRadius(12.0 * hudScale_);
    // 卡片内部边距同步缩放，避免小窗口下内容溢出卡片（如计时卡压字）。
    for (const auto &entry : hudGeometry_) {
        if (auto *layout = entry.first->layout()) {
            layout->setContentsMargins(qRound(12 * hudScale_), qRound(6 * hudScale_),
                                       qRound(12 * hudScale_), qRound(8 * hudScale_));
            layout->setSpacing(qMax(0, qRound(3 * hudScale_)));
        }
    }
    if (hudSelfBar_) hudSelfBar_->setFixedHeight(qMax(3, qRound(5 * hudScale_)));
    if (hudAllyBar_) hudAllyBar_->setFixedHeight(qMax(3, qRound(5 * hudScale_)));
    if (hudEnemyBar_) hudEnemyBar_->setFixedHeight(qMax(3, qRound(5 * hudScale_)));
}

void Window::updateCombatHud() {
    if (!combat_) return;
    const auto number = [](const QJsonObject &object, const char *key, const QString &fallback) {
        const auto value = object.value(QLatin1String(key));
        return value.isDouble() ? QString::number(value.toInt()) : fallback;
    };
    const bool blue = ownSide_ == 1;

    // 阵营配色：己方永远用自己阵营色（蓝方时左列变蓝）
    const QString allyColor = blue ? QStringLiteral("#3ea8ff") : QStringLiteral("#ff5a67");
    const QString enemyColor = blue ? QStringLiteral("#ff5a67") : QStringLiteral("#3ea8ff");
    const auto paintBar = [](QProgressBar *bar, const QString &color) {
        if (!bar) return;
        bar->setStyleSheet(QStringLiteral(
            "QProgressBar{background:rgba(255,255,255,22);border:none;border-radius:2px;}"
            "QProgressBar::chunk{background:%1;border-radius:2px;}").arg(color));
    };
    const auto paintLabel = [](QLabel *l, const QString &c) {
        if (!l) return;
        l->setStyleSheet(QStringLiteral("color:%1;background:transparent;").arg(c));
        l->setProperty("hudColor", c);
    };
    paintLabel(hudAllyBase_, QStringLiteral("#eaf1f7"));
    paintLabel(hudEnemyBase_, QStringLiteral("#eaf1f7"));
    paintLabel(hudOutpostAlly_, QStringLiteral("#eaf1f7"));
    paintLabel(hudOutpostEnemy_, QStringLiteral("#eaf1f7"));
    paintLabel(hudSelfId_, QStringLiteral("#eaf1f7"));
    paintLabel(hudTicker_, QStringLiteral("#8da0b3"));
    paintBar(hudAllyBar_, allyColor);
    paintBar(hudEnemyBar_, enemyColor);
    paintBar(hudSelfBar_, allyColor);
    paintBar(hudOutpostAllyBar_, allyColor);
    paintBar(hudOutpostEnemyBar_, enemyColor);
    if (hudSelfHp_) hudSelfHp_->setStyleSheet(QStringLiteral("color:#eaf1f7;background:transparent;"));
    for (auto *bar : hudAllyHpBar_) paintBar(bar, allyColor);
    for (auto *bar : hudEnemyHpBar_) paintBar(bar, enemyColor);

    // 协议字段基于“己方/敌方”命名：base_health=己方基地，enemy_base_health=敌方基地，
    // 不随红蓝变化，直接对应左（己方）右（敌方）。只有 outpost_R/B 是按红蓝命名的，需要按阵营翻转。
    const auto unit = messageFields("GlobalUnitStatus");
    const qint64 unitAge = dataAgeMs("GlobalUnitStatus");
    const bool unitStale = unitAge < 0 || unitAge > 3000;
    const auto baseAlly = number(unit, "baseHealth", QStringLiteral("--"));
    const auto baseEnemy = number(unit, "enemyBaseHealth", QStringLiteral("--"));
    if (hudAllyBase_) hudAllyBase_->setText(baseAlly);
    if (hudEnemyBase_) hudEnemyBase_->setText(baseEnemy);
    if (hudAllyBaseName_) hudAllyBaseName_->setText(QStringLiteral("己方基地 · %1").arg(blue ? QStringLiteral("BLUE BASE") : QStringLiteral("RED BASE")));
    if (hudEnemyBaseName_) hudEnemyBaseName_->setText(QStringLiteral("敌方基地 · %1").arg(blue ? QStringLiteral("RED BASE") : QStringLiteral("BLUE BASE")));
    const int baseMax = qEnvironmentVariableIntValue("HJ_BASE_MAX_HP") > 0
        ? qEnvironmentVariableIntValue("HJ_BASE_MAX_HP") : demoMode_ ? 5000 : 0;
    for (auto *bar : {hudAllyBar_, hudEnemyBar_}) {
        if (!bar) continue;
        const QString text = bar == hudAllyBar_ ? baseAlly : baseEnemy;
        bool ok = false;
        const uint value = text.toUInt(&ok);
        if (ok && baseMax > 0 && value <= static_cast<uint>(baseMax)) bar->setValue(static_cast<int>(qint64(value) * 1000 / baseMax));
        else bar->reset();
    }
    const auto outpostAlly = number(unit, blue ? "outpostBHealth" : "outpostRHealth", QStringLiteral("--"));
    const auto outpostEnemy = number(unit, blue ? "outpostRHealth" : "outpostBHealth", QStringLiteral("--"));
    if (hudOutpostAlly_) hudOutpostAlly_->setText(outpostAlly);
    if (hudOutpostEnemy_) hudOutpostEnemy_->setText(outpostEnemy);
    const int outpostMax = qEnvironmentVariableIntValue("HJ_OUTPOST_MAX_HP") > 0
        ? qEnvironmentVariableIntValue("HJ_OUTPOST_MAX_HP") : 700;
    const auto paintOutpostBar = [outpostMax](QProgressBar *bar, const QString &text) {
        if (!bar) return;
        bool ok = false;
        const uint value = text.toUInt(&ok);
        if (ok && value <= static_cast<uint>(outpostMax)) bar->setValue(static_cast<int>(qint64(value) * 1000 / outpostMax));
        else bar->reset();
    };
    paintOutpostBar(hudOutpostAllyBar_, outpostAlly);
    paintOutpostBar(hudOutpostEnemyBar_, outpostEnemy);
    // 控制度/伤害不再放在作战视图（移到数据视图的数据计算区）

    // 计时 / 比分
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
    if (hudStage_) {
        hudStage_->clear();
        hudStage_->hide();
    }
    if (hudScore_) {
        const auto red = number(game, "redScore", QStringLiteral("--"));
        const auto blueScore = number(game, "blueScore", QStringLiteral("--"));
        hudScore_->setText(QStringLiteral("%1 : %2").arg(red, blueScore));
    }

    // 链路
    if (hudLink_) {
        hudLink_->setText(QStringLiteral("%1 · 图传 %2").arg(connected_ ? QStringLiteral("● 已连接") : QStringLiteral("● 未连接"),
                                                           gotVideo_ ? QStringLiteral("正常") : QStringLiteral("等待首帧")));
        hudLink_->setStyleSheet(QStringLiteral("color:#8da0b3;background:transparent;"));
    }

    // 本机
    const auto dynamic = messageFields("RobotDynamicStatus");
    const auto respawn = messageFields("RobotRespawnStatus");
    const auto hp = number(dynamic, "currentHealth", QStringLiteral("--"));
    int maxHp = 0;
    if (ownSide_ >= 0) {
        const int index = (ownSide_ == 0 ? 0 : 4);
        maxHp = static_cast<int>(maxHealth_[static_cast<size_t>(index)]);
        if (maxHealthAt_[static_cast<size_t>(index)] < 0) maxHp = 0;
    }
    if (hudSelfId_) hudSelfId_->setText(number_ > 0 ? QStringLiteral("%1").arg(number_, 2, 10, QLatin1Char('0')) : QStringLiteral("--"));
    if (hudSelfHp_) hudSelfHp_->setText(maxHp > 0 ? QStringLiteral("%1 / %2").arg(hp).arg(maxHp) : QStringLiteral("%1 / --").arg(hp));
    if (hudSelfBar_) {
        bool ok = false;
        const int value = hp.toInt(&ok);
        if (ok && maxHp > 0 && value <= maxHp) hudSelfBar_->setValue(value * 1000 / maxHp);
        else hudSelfBar_->reset();
    }
    if (hudSelfKv_) {
        const auto ammo = number(dynamic, "remainingAmmo", QStringLiteral("--"));
        const auto heat = number(dynamic, "currentHeat", QStringLiteral("--"));
        const auto maxHeat = number(messageFields("RobotStaticStatus"), "maxHeat", QStringLiteral("--"));
        const bool pending = respawn.value(QStringLiteral("isPendingRespawn")).toBool();
        hudSelfKv_->setText(QStringLiteral("弹量 %1　热量 %2 / %3　复活 %4")
                                .arg(ammo, heat, maxHeat, pending ? QStringLiteral("等待") : QStringLiteral("无需")));
    }

    // 双方机体血条
    const auto setSquadRow = [this](std::array<QLabel *, 4> &values, std::array<QProgressBar *, 4> &bars, int i, int index) {
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
        bar->setValue(static_cast<int>(static_cast<uint64_t>(health_[idx]) * 1000 / maxHealth_[idx]));
        value->setText(QString::number(health_[idx]));
    };
    for (int i = 0; i < 4; ++i) {
        setSquadRow(hudAllyHp_, hudAllyHpBar_, i, i);
        setSquadRow(hudEnemyHp_, hudEnemyHpBar_, i, i + 4);
    }

    // 地图标记：自己的 id + 双方血量
    const uint ownId = number_ > 0 ? static_cast<uint>(number_ + (ownSide_ == 1 ? 100 : 0)) : 0;
    const std::vector<uint32_t> maxHealth(maxHealth_.begin(), maxHealth_.end());
    for (auto *m : {map_, mapHud_})
        if (m) m->setStatus(ownId, health_, maxHealth);

    // 事件条
    if (hudTicker_ && alert_) hudTicker_->setText(alert_->text().isEmpty() ? QStringLiteral("● 等待数据") : alert_->text());
    if (hudTickerEvent_ && events_ && events_->count() > 0) {
        QString detail = events_->item(events_->count() - 1)->text();
        detail.remove(QRegularExpression(QStringLiteral("第 \\d+ / \\d+ 局 · ")));
        detail.remove(QStringLiteral("· 比赛中"));
        hudTickerEvent_->setText(QStringLiteral("｜　") + detail);
    }
    if (hudLegend_) {
        const qint64 radarAge = dataAgeMs("RadarInfoToClient");
        hudLegend_->setText(radarAge < 0 ? QStringLiteral("雷达 --") : QStringLiteral("雷达 %1s 前").arg(radarAge / 1000));
    }
    Q_UNUSED(unitStale);
}

void Window::buildExchangePanel() {
    exchangePanel_ = new QFrame(this);
    exchangePanel_->setObjectName("exchangePanel");
    exchangePanel_->setStyleSheet(QStringLiteral(
        "QFrame#exchangePanel{background:rgba(9,14,21,0.96);border:1px solid rgba(232,163,61,0.45);"
        "border-radius:12px;} QLabel{color:#eaf1f7;}"));
    exchangePanel_->setFixedSize(460, 306);
    exchangePanel_->hide();

    auto *layout = new QVBoxLayout(exchangePanel_);
    layout->setContentsMargins(22, 18, 22, 18);
    layout->setSpacing(10);
    auto *title = new QLabel(QStringLiteral("弹丸兑换"), exchangePanel_);
    title->setStyleSheet(QStringLiteral("color:#ffc971;font-size:18px;font-weight:700;"));
    layout->addWidget(title);
    exchangeKind_ = new QLabel(exchangePanel_);
    exchangeKind_->setStyleSheet(QStringLiteral("color:#eaf1f7;font-size:14px;"));
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
    exchangeHint_ = new QLabel(QStringLiteral("数量须为 10 的倍数；发送后由客户端上行"), exchangePanel_);
    exchangeHint_->setStyleSheet(QStringLiteral("color:#ffc46a;font-size:12px;"));
    layout->addWidget(exchangeHint_);
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
    exchangePanel_->show();
    exchangePanel_->raise();
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
        ? QStringLiteral("当前兵种不支持远程兑换")
        : QStringLiteral("类型：%1 弹丸（远程兑换）").arg(kind));
    const auto number = [](const QJsonObject &object, const char *key) {
        const auto value = object.value(QLatin1String(key));
        return value.isDouble() ? QString::number(value.toInt()) : QStringLiteral("--");
    };
    exchangeOwned_->setText(number(messageFields("RobotDynamicStatus"), "remainingAmmo"));
    exchangeLimit_->setText(number(messageFields("Buff"), "buffLevel"));
    exchangeGold_->setText(number(messageFields("GlobalLogisticsStatus"), "remainingEconomy"));
    exchangeQty_->setEnabled(exchangeCmdType_ != 0);
    exchangeHint_->setText(exchangeCmdType_ != 0
        ? QStringLiteral("数量须为 10 的倍数；发送后由客户端上行")
        : QStringLiteral("英雄=42mm，步兵=17mm；其他兵种不支持"));
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
    command->cmd_type = static_cast<uint32_t>(exchangeCmdType_);
    command->param = static_cast<uint32_t>(quantity);
    if (client_->send(std::move(command))) {
        exchangeHint_->setText(QStringLiteral("已发送：兑换 %1 发 %2").arg(quantity).arg(exchangeCmdType_ == 2 ? QStringLiteral("42mm") : QStringLiteral("17mm")));
        addEvent(QStringLiteral("兑换请求：%1 发（%2）").arg(quantity).arg(exchangeCmdType_ == 2 ? QStringLiteral("42mm") : QStringLiteral("17mm")));
    } else {
        exchangeHint_->setText(QStringLiteral("发送失败（未连接或被限流）"));
    }
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
    compactPage_ = new QWidget;
    auto *grid = new QGridLayout(compactPage_);
    grid->setContentsMargins(8, 8, 8, 8);
    grid->setSpacing(8);
    grid->addWidget(allyModule_, 0, 0);
    grid->addWidget(mapDock_, 0, 1);
    grid->addWidget(enemyModule_, 0, 2);
    grid->addWidget(eventsModule_, 1, 0);
    grid->addWidget(analysisModule_, 1, 1);
    grid->addWidget(videoDock_, 1, 2);
    grid->setColumnStretch(0, 2);
    grid->setColumnStretch(1, 5);
    grid->setColumnStretch(2, 2);
    grid->setRowStretch(0, 5);
    grid->setRowStretch(1, 2);
}

void Window::buildDockPage() {
    auto *host = new QMainWindow;
    host->setObjectName("dockHost");
    host->setDockNestingEnabled(true);
    host->setCentralWidget(mapDock_);
    const auto makeDock = [host](const QString &title, QWidget *content, Qt::DockWidgetArea area) {
        auto *dock = new QDockWidget(title, host);
        dock->setObjectName(title);
        dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
        dock->setWidget(content);
        host->addDockWidget(area, dock);
        content->show();
        return dock;
    };
    makeDock(QStringLiteral("图传"), videoDock_, Qt::RightDockWidgetArea);
    makeDock(QStringLiteral("双方机器人"), allyModule_, Qt::RightDockWidgetArea);
    makeDock(QStringLiteral("数据计算"), analysisModule_, Qt::RightDockWidgetArea);
    makeDock(QStringLiteral("战场事件"), eventsModule_, Qt::RightDockWidgetArea);
    dockPage_ = host;
}

void Window::sendRespawn() {
    if (!client_) return;
    auto command = std::make_unique<rm2027::CommonCommand>();
    command->cmd_type = 4;   // TODO: 以官方协议表核对“立即复活”的 cmd_type
    command->param = 0;
    if (client_->send(std::move(command))) {
        addEvent(QStringLiteral("买活请求已发送"));
        if (exchangeHint_) exchangeHint_->setText(QStringLiteral("已发送买活请求（cmd_type=4，待协议核对）"));
    } else if (exchangeHint_) {
        exchangeHint_->setText(QStringLiteral("买活发送失败（未连接）"));
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
    // 左列：上地图、下图传（原分屏保留，仅改为上下叠放）
    primaryLayout_->addWidget(mapDock_);
    primaryLayout_->addWidget(bottomLeft_);
    primaryLayout_->setStretch(0, 7);
    primaryLayout_->setStretch(1, 2);
    mapDock_->show();
    videoDock_->show();
    for (int index = 0; index < sidebarModules_.size(); ++index) {
        auto *module = sidebarModules_.at(index);
        sidebar_->insertWidget(index, module);
        sidebar_->setStretchFactor(index, index == 0 ? 7 : 2);
        if (module != detailsDock_) module->show();
    }
    if (previousSizes.size() == sidebar_->count())
        sidebar_->setSizes(previousSizes);
    // 右列：双方机器人给足高度（4 台各一行），数据/事件分剩下的
    const int columnHeight = qMax(sidebar_->height(), 720);
    sidebar_->setSizes({columnHeight * 62 / 100, columnHeight * 38 / 100});
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
    QStringList order;
    for (auto *module : sidebarModules_) order << module->objectName();
    settings.setValue("view/workspaceOrder", order);
    settings.setValue("view/workspaceSizes", workspace_->saveState());
    settings.setValue("view/sidebarSizes", sidebar_->saveState());
    QMainWindow::closeEvent(event);
}

void Window::applyWindowPreferences() {
    const auto settings = preferences();
    const auto order = settings.value("view/workspaceOrder").toStringList();
    QList<QWidget *> restored;
    for (const auto &name : order) {
        auto *module = findChild<QWidget *>(name);
        if (module && (defaultSidebar_.contains(module) || module == mapDock_) &&
            !restored.contains(module))
            restored.append(module);
    }
    if (restored.size() == defaultSidebar_.size() &&
        restored.contains(mapDock_) != restored.contains(videoDock_) &&
        restored.contains(detailsDock_) &&
        restored.contains(defaultSidebar_[1]) && restored.contains(defaultSidebar_[2]) &&
        restored.contains(defaultSidebar_[3])) {
        sidebarModules_ = restored;
        mapPriority_ = !restored.contains(mapDock_);
        arrangeModules();
    }
    workspace_->restoreState(settings.value("view/workspaceSizes").toByteArray());
    sidebar_->restoreState(settings.value("view/sidebarSizes").toByteArray());
    if (settings.value("view/fullScreen", false).toBool())
        setWindowState(windowState() | Qt::WindowFullScreen);
}
