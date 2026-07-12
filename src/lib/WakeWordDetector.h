#if !defined(LIB_WAKE_WORD_DETECTOR_H)
#define LIB_WAKE_WORD_DETECTOR_H

#include <array>
#include <deque>
#include <vector>
#include <Arduino.h>

/// wakeword 検出 (MFCC 特徴量のテンプレートを DTW で照合する)
class WakeWordDetector {
public:
    static const int SAMPLE_RATE = 16000;
    static const int FRAME_SIZE = 512;
    static const int HOP_SIZE = 256;
    static const int NUM_COEFFS = 13;

    WakeWordDetector();

    /// SPIFFS に保存済みのテンプレートを読み込む。成功時 true
    bool loadTemplate();

    /// WAV (16kHz/16bit/mono) からテンプレートを作成して SPIFFS に保存する
    /// @return エラーメッセージ (nullptr: 成功)
    const char *registerWav(const uint8_t *data, size_t size);

    bool hasTemplate() { return !_templates.empty(); }

    int templateFrames() { return _templates.empty() ? 0 : (int) _templates[0].size(); }

    /// HOP_SIZE 分の音声を追加し、周期的に照合する
    /// @return 正規化 DTW 距離の10倍 (照合しなかったとき -1)
    int feed(const int16_t *samples);

    /// 照合状態をクリアする (検出直後の再照合防止)
    void reset();

private:
    using Frame = std::array<float, NUM_COEFFS>;

    /// 呼びかけ語のテンプレート (ケプストラム平均除去済み)
    /// フレーム位相ずれに強くするため、半 hop ずらした複数系統を持つ
    std::vector<std::vector<Frame>> _templates;

    /// 直近の特徴量と音量
    std::deque<Frame> _window;
    std::deque<float> _rms;

    /// 前回 hop の音声 (フレームは 2 hop 分で構成)
    int16_t _prev[HOP_SIZE]{};

    int _frameCount = 0;

    /// 前処理テーブル
    std::array<float, FRAME_SIZE> _hamming{};
    std::vector<std::pair<int, std::vector<float>>> _melFilters;

    /// 1 フレームの MFCC を計算する。戻り値はフレームの RMS
    float _mfcc(const int16_t *frame, float *coeffs);

    /// テンプレートと直近ウィンドウの正規化 DTW 距離 (全系統の最小)
    float _match();

    float _matchOne(const std::vector<Frame> &tmpl, const float *mean);

    /// PCM からテンプレートを構築する (無音区間の切り落とし込み)
    const char *_buildTemplate(const int16_t *samples, size_t numSamples);

    bool _saveTemplate();
};

#endif // !defined(LIB_WAKE_WORD_DETECTOR_H)
