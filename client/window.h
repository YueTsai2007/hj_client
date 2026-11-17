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
class QLabel;
class QListWidget;
class Map;
class QTimer;
class QDialog;
class QPlainTextEdit;

class Window : public QMainWindow {
public:
    Window(const QString &host, quint16 port, const QString &id, quint16 videoPort);
    ~Window() override;
private:
    void setupUi();
    void start(const QString &host, quint16 port, const QString &id, quint16 videoPort);
    void updateVideo();
    void markStale();
    void showHealth(const std::vector<uint32_t> &health);
    void showHealthPlaceholder(const QString &status);
    void updateAlert();
    void recordRaw(const std::string &topic, const std::string &payload);
    QJsonObject messageFields(const std::string &topic);
    void refreshDetails();
    void refreshSummary();
    void showDetails();
    struct MessageRecord { std::string json; std::string error; uint64_t count = 0; qint64 receivedAt = 0; qint64 receivedTick = 0; };
    std::mutex recordsMutex_;
    std::map<std::string, MessageRecord> records_;
    QDialog *details_ = nullptr;
    QListWidget *topics_ = nullptr;
    QPlainTextEdit *messageDetails_ = nullptr;
    std::unique_ptr<rm2027::Client> client_;
    std::unique_ptr<rm2027::VideoReceiver> video_;
    QLabel *connection_ = nullptr, *scoreRed_ = nullptr, *scoreBlue_ = nullptr, *alert_ = nullptr;
    QLabel *round_ = nullptr, *clock_ = nullptr, *base_ = nullptr, *economy_ = nullptr;
    QLabel *damage_ = nullptr, *videoView_ = nullptr, *videoStatus_ = nullptr, *dataStatus_ = nullptr;
    QListWidget *events_ = nullptr, *ally_ = nullptr, *enemy_ = nullptr, *situation_ = nullptr;
    QLabel *position_ = nullptr, *radar_ = nullptr;
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
