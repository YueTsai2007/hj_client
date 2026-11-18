#include "window.h"
#include "window_helpers.h"
#include "map.h"
#include "brand.h"
#include "video_view.h"
#include <QSettings>
#include <QShortcut>
#include <QApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QMouseEvent>
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
    setWindowTitle(QStringLiteral("悍匠 · 战场终端"));
    setMinimumSize(780, 520);
    auto *shell = new QWidget;
    setCentralWidget(shell);
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
    auto *toolbar = addToolBar(QStringLiteral("窗口布局"));
    toolbar->setObjectName("layoutToolbar");
    toolbar->setMovable(false);
    auto *switchView = toolbar->addAction(QStringLiteral("切换地图 / 图传  F2"));
    switchView->setShortcut(QKeySequence(Qt::Key_F2));
    switchView->setAutoRepeat(false);
    connect(switchView, &QAction::triggered, this, &Window::focusPrimaryView);
    connect(toolbar->addAction(QStringLiteral("消息详情")), &QAction::triggered, this, &Window::showDetails);
    connect(toolbar->addAction(QStringLiteral("恢复默认布局")), &QAction::triggered, this, [this] {
        sidebarModules_ = defaultSidebar_;
        mapPriority_ = true;
        detailsDock_->hide();
        arrangeModules();
        workspace_->setSizes({width() * 2 / 3, width() / 3});
        sidebar_->setSizes({1, 1, 1, 1, 0});
    });

    QVBoxLayout *videoLayout, *mapLayout, *allyLayout, *enemyLayout, *eventLayout, *metricsLayout;
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
    sidebarModules_ = {
        videoDock_,
        addModule("ally", QStringLiteral("我方机器人"), allyPanel),
        addModule("enemy", QStringLiteral("对方机器人"), enemyPanel),
        addModule("events", QStringLiteral("战场事件"), eventPanel)
    };
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
    workspace_->setSizes({860, 490});
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
    auto *primary = mapPriority_ ? mapDock_ : videoDock_;
    const auto previousSizes = sidebar_->sizes();
    primaryLayout_->addWidget(primary);
    primary->show();
    for (int index = 0; index < sidebarModules_.size(); ++index) {
        auto *module = sidebarModules_.at(index);
        sidebar_->insertWidget(index, module);
        if (module != detailsDock_) module->show();
    }
    if (previousSizes.size() == sidebar_->count())
        sidebar_->setSizes(previousSizes);
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
