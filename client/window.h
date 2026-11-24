#pragma once
#include <QMainWindow>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QRect>
#include <QVector>
#include <array>
#include <memory>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include "rm2027/api/client.h"
#include "rm2027/video/video_receiver.h"
class QLabel;
class QProgressBar;
class QListWidget;
class Map;
class CombatView;
class HudCard;
class FloatingPanel;
class Crosshair;
class QTimer;
class QStackedWidget;
class QVBoxLayout;
class QAction;
class QSpinBox;
class QSplitter;
class QCloseEvent;
class VideoView;
class QPlainTextEdit;

class Window : public QMainWindow {
public:
    Window(const QString &host, quint16 port, const QString &id, quint16 videoPort);
    ~Window() override;
private:
    enum class Role { Hero, Infantry, Engineer, Aerial, Sentry, Dart, Unknown };
    static Role roleOfId(uint number);
    QString roleLabel() const;
    bool roleNeedsPanel() const;
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void setupUi();
    void buildCombatView();
    void layoutCombatHud(const QRectF &content);
    void updateCombatHud();
    void buildExchangePanel();
    void buildModulePanelDock();
    void toggleExchangePanel();
    void updateExchangePanel();
    void sendExchange();
    void sendRespawn();
    void applyWindowPreferences();
    void setupDetails();
    void focusPrimaryView();
    QWidget *addModule(const char *id, const QString &title, QWidget *content);
    void arrangeModules();
    void applyDefaultLayout();
    void buildCompactPage();
    void moveModule(QWidget *source, QWidget *target);
    void addEvent(const QString &text);
    void start(const QString &host, quint16 port, const QString &id, quint16 videoPort);
    void updateVideo();
    void markStale();
    qint64 dataAgeMs(const char *topic);
    void setBaseHealth(QLabel *label, QProgressBar *bar, const QString &health);
    void showHealth(const std::vector<uint32_t> &health);
    void showHealthPlaceholder(const QString &status);
    void updateAlert();
    void recordMessage(std::unique_ptr<rm2027::Message> message);
    static QJsonObject toFields(const rm2027::Message &message);
    QJsonObject messageFields(const std::string &topic);
    void refreshDetails();
    void refreshSummary();
    void showDetails();
    struct MessageRecord { std::shared_ptr<const rm2027::Message> message; QJsonObject fields; std::string error; uint64_t count = 0; qint64 receivedAt = 0; qint64 receivedTick = 0; };
    static QJsonObject messageFields(const MessageRecord &record);
    std::mutex recordsMutex_;
    std::map<std::string, MessageRecord> records_;
    QWidget *mapDock_ = nullptr, *videoDock_ = nullptr, *detailsDock_ = nullptr, *roleDock_ = nullptr;
    QWidget *allyModule_ = nullptr, *enemyModule_ = nullptr, *eventsModule_ = nullptr, *analysisModule_ = nullptr;
    QWidget *bottomLeft_ = nullptr;
    class QHBoxLayout *bottomLeftLayout_ = nullptr;
    void buildDockPage();
    QLabel *roleInfo_ = nullptr;
    Role role_ = Role::Unknown;
    uint number_ = 0;
    int side_ = -1;
    QVBoxLayout *primaryLayout_ = nullptr;
    QSplitter *workspace_ = nullptr, *sidebar_ = nullptr;
    QList<QWidget *> sidebarModules_, defaultSidebar_;
    QWidget *metricsPage_ = nullptr;
    bool demoMode_ = false;
    bool mapPriority_ = true;
    bool videoPriority_ = false;   // 数据视图默认：地图占主视图，F2 可临时换成图传
    QWidget *details_ = nullptr;
    QListWidget *topics_ = nullptr;
    QPlainTextEdit *messageDetails_ = nullptr;
    std::unique_ptr<rm2027::Client> client_;
    std::unique_ptr<rm2027::VideoReceiver> video_;
    QLabel *connection_ = nullptr, *scoreRed_ = nullptr, *scoreBlue_ = nullptr, *alert_ = nullptr;
    QLabel *round_ = nullptr, *clock_ = nullptr, *baseRed_ = nullptr, *baseBlue_ = nullptr, *economy_ = nullptr;
    QLabel *damage_ = nullptr, *videoStatus_ = nullptr, *dataStatus_ = nullptr;
    QListWidget *events_ = nullptr, *ally_ = nullptr, *enemy_ = nullptr, *situation_ = nullptr;
    QLabel *position_ = nullptr, *radar_ = nullptr;
    QLabel *analysis_ = nullptr;
    QLabel *topBar_ = nullptr;
    QWidget *modulePanel_ = nullptr;
    FloatingPanel *modulePanelDock_ = nullptr;
    std::array<QLabel *, 8> moduleDots_{};
    QProgressBar *baseRedBar_ = nullptr, *baseBlueBar_ = nullptr;
    VideoView *videoView_ = nullptr;
    QLabel *videoLink_ = nullptr;
    std::array<QLabel *, 4> localStatus_{};
    QString lastStage_;
    bool gotVideo_ = false;
    Map *map_ = nullptr;
    QTimer *timer_ = nullptr;
    QElapsedTimer gameAge_, unitAge_, economyAge_, frameAge_;
    QElapsedTimer positionAge_, radarAge_;
    QElapsedTimer maxClock_;
    std::array<uint32_t, 8> maxHealth_{};
    std::array<qint64, 8> maxHealthAt_{{-1, -1, -1, -1, -1, -1, -1, -1}};
    std::vector<uint32_t> health_;
    int ownSide_ = -1;
    bool connected_ = false;
    qint64 connectedAtTick_ = 0;
    uint64_t videoSequence_ = 0;
    std::vector<uint8_t> frame_;

    // ---- 作战视图 v3 ----
    CombatView *combat_ = nullptr;
    Crosshair *crosshair_ = nullptr;
    QAction *crosshairAction_ = nullptr;
    QStackedWidget *mainStack_ = nullptr;
    QWidget *dataPage_ = nullptr;
    Map *mapHud_ = nullptr;
    VideoView *combatVideo_ = nullptr;
    QLabel *hudAllyBase_ = nullptr, *hudEnemyBase_ = nullptr;
    QLabel *hudAllyBaseName_ = nullptr, *hudEnemyBaseName_ = nullptr;
    QLabel *hudClock_ = nullptr, *hudStage_ = nullptr, *hudScore_ = nullptr;
    QLabel *hudOutpostAlly_ = nullptr, *hudOutpostEnemy_ = nullptr;
    QProgressBar *hudOutpostAllyBar_ = nullptr, *hudOutpostEnemyBar_ = nullptr;
    QLabel *hudStats_ = nullptr;
    QLabel *hudLink_ = nullptr;
    QLabel *hudSelfId_ = nullptr, *hudSelfHp_ = nullptr, *hudSelfKv_ = nullptr;
    QLabel *hudTicker_ = nullptr, *hudTickerEvent_ = nullptr, *hudLegend_ = nullptr;
    QProgressBar *hudAllyBar_ = nullptr, *hudEnemyBar_ = nullptr, *hudSelfBar_ = nullptr;
    QWidget *hudAllySquad_ = nullptr, *hudEnemySquad_ = nullptr;
    std::array<QLabel *, 4> hudAllyHp_{}, hudEnemyHp_{};
    std::array<QProgressBar *, 4> hudAllyHpBar_{}, hudEnemyHpBar_{};
    QVector<QPair<QWidget *, QRect>> hudGeometry_;
    QVector<HudCard *> hudCards_;
    QVector<QPair<QLabel *, int>> hudFonts_;
    qreal hudScale_ = 1.0;
    std::map<std::string, uint64_t> hudCounts_;
    bool refreshPending_ = false;

    // 兑换面板（O 键）
    QWidget *exchangePanel_ = nullptr;
    FloatingPanel *detailsPanel_ = nullptr;
    QLabel *exchangeKind_ = nullptr, *exchangeOwned_ = nullptr, *exchangeLimit_ = nullptr;
    QLabel *exchangeGold_ = nullptr, *exchangeHint_ = nullptr;
    QLabel *exchangeStatus_ = nullptr;
    QSpinBox *exchangeQty_ = nullptr;
    class QCheckBox *exchangeRemote_ = nullptr;
    int exchangeCmdType_ = 0;
};
