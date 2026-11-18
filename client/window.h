#pragma once
#include <QMainWindow>
#include <QElapsedTimer>
#include <QJsonObject>
#include <array>
#include <memory>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include "rm2027/api/client.h"
#include "rm2027/video/video_receiver.h"
namespace google::protobuf { class Message; }
class QLabel;
class QProgressBar;
class QListWidget;
class Map;
class QTimer;
class QVBoxLayout;
class QSplitter;
class QCloseEvent;
class VideoView;
class QPlainTextEdit;

class Window : public QMainWindow {
public:
    Window(const QString &host, quint16 port, const QString &id, quint16 videoPort);
    ~Window() override;
private:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void setupUi();
    void applyWindowPreferences();
    void setupDetails();
    void focusPrimaryView();
    QWidget *addModule(const char *id, const QString &title, QWidget *content);
    void arrangeModules();
    void moveModule(QWidget *source, QWidget *target);
    void addEvent(const QString &text);
    void start(const QString &host, quint16 port, const QString &id, quint16 videoPort);
    void updateVideo();
    void markStale();
    void setBaseHealth(QLabel *label, QProgressBar *bar, const QString &health);
    void showHealth(const std::vector<uint32_t> &health);
    void showHealthPlaceholder(const QString &status);
    void updateAlert();
    void recordRaw(const std::string &topic, const std::string &payload);
    QJsonObject messageFields(const std::string &topic);
    void refreshDetails();
    void refreshSummary();
    void showDetails();
    struct MessageRecord { std::shared_ptr<const google::protobuf::Message> message; std::string error; uint64_t count = 0; qint64 receivedAt = 0; qint64 receivedTick = 0; };
    static QJsonObject messageFields(const MessageRecord &record);
    std::mutex recordsMutex_;
    std::map<std::string, MessageRecord> records_;
    QWidget *mapDock_ = nullptr, *videoDock_ = nullptr, *detailsDock_ = nullptr;
    QVBoxLayout *primaryLayout_ = nullptr;
    QSplitter *workspace_ = nullptr, *sidebar_ = nullptr;
    QList<QWidget *> sidebarModules_, defaultSidebar_;
    QWidget *metricsPage_ = nullptr;
    bool demoMode_ = false;
    bool mapPriority_ = true;
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
};
