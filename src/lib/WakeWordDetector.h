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
    /// @param append true: 既存テンプレートに追加、false: 置き換え
    /// @return エラーメッセージ (nullptr: 成功)
    const char *registerWav(const uint8_t *data, size_t size, bool append = false);

    bool hasTemplate() { return !_templates.empty(); }

    /// 登録済みの呼びかけ語の数
    int templateWords() { return _numWords; }

    /// HOP_SIZE 分の音声を追加し、周期的に照合する
    /// @return 正規化 DTW 距離の10倍 (照合しなかったとき -1)
    int feed(const int16_t *samples);

    /// 照合状態をクリアする (検出直後の再照合防止)
    void reset();

    /// 直近フレームの音量 (RMS, int16 振幅)
    float lastRms() { return _rms.empty() ? 0.0f : _rms.back(); }

private:
    using Frame = std::array<float, NUM_COEFFS>;

    /// 呼びかけ語のテンプレート (ケプストラム平均除去済み)
    /// 登録ごとに、フレーム位相ずれ対策の半 hop ずらしを含む複数系統を持つ
    std::vector<std::vector<Frame>> _templates;

    /// 登録された呼びかけ語の数
    int _numWords = 0;

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

    /// PCM からテンプレートを構築して追加する (無音区間の切り落とし込み)
    const char *_buildTemplate(const int16_t *samples, size_t numSamples);

    size_t _maxTemplateSize();

    size_t _minTemplateSize();

    bool _saveTemplate();
};

#endif // !defined(LIB_WAKE_WORD_DETECTOR_H)
