#if !defined(APP_IR_H)
#define APP_IR_H

#include <memory>
#include <utility>

#include <IRrecv.h>
#include <IRsend.h>

#include "app/AppSettings.h"

/// 赤外線リモコンの学習と送信 (M5Stack Unit IR などを想定)
class AppIr {
public:
    explicit AppIr(
            std::shared_ptr<AppSettings> settings
    ) : _settings(std::move(settings)) {};

    void setup();

    /// 受信待ちして名前を付けて保存する。成功時 true
    bool learn(const String &name, uint32_t timeoutMs = 10000);

    /// 保存済みコードを送信する。成功時 true
    bool send(const String &name);

    /// 保存済みコード名の一覧
    std::vector<String> codeNames();

private:
    std::shared_ptr<AppSettings> _settings;

    std::unique_ptr<IRsend> _irSend;
    std::unique_ptr<IRrecv> _irRecv;
};

#endif // !defined(APP_IR_H)
