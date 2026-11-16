#include <cstdio>
#include <memory>
#include <string>

#include "rm2027/api/client.h"
#include "rm2027/api/messages.h"

// 用法：./score_client [服务器地址] [机器人 ID]
int main(int argc, char **argv) {
    rm2027::Client::Options options;
    options.host = argc > 1 ? argv[1] : "127.0.0.1";
    options.clientId = argc > 2 ? argv[2] : "3";
    options.subscribeAllDownlink = false;
    options.logMessages = false;

    rm2027::Client client(options);
    client.on<rm2027::GameStatus>([](std::unique_ptr<rm2027::GameStatus> status) {
        std::printf("红方 %u : %u 蓝方\n", status->red_score, status->blue_score);
        std::fflush(stdout);
    });
    client.onConnectionChanged([](bool connected) {
        std::fprintf(stderr, "%s\n", connected ? "已连接" : "连接断开，正在重试");
    });

    std::string error;
    if (!client.start(&error)) {
        std::fprintf(stderr, "启动失败：%s\n", error.c_str());
        return 1;
    }
    client.runUntilStopped();
}
