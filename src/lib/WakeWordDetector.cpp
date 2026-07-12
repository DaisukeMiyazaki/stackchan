#include <algorithm>
#include <cmath>
#include <cstring>
#include <Arduino.h>
#include <SPIFFS.h>

#include "lib/WakeWordDetector.h"

/// File path for wakeword template
static const char *TEMPLATE_PATH = "/wakeword.mfcc";
static const uint32_t TEMPLATE_MAGIC = 0x32574b57; // "WKW2"

/// 登録できる呼びかけ語の最大数
static const int MAX_WORDS = 8;

static const int NUM_MEL_FILTERS = 26;
static const int FFT_BINS = WakeWordDetector::FRAME_SIZE / 2 + 1;
static const int MIN_TEMPLATE_FRAMES = 10;
static const int MAX_TEMPLATE_FRAMES = 250;

/// 発話区間とみなす RMS のしきい値 (int16 振幅)
static const float VAD_RMS = 250.0f;
/// 照合を実行する間隔 (フレーム数)
static const int DETECT_INTERVAL = 4;

static float melScale(float hz) {
    return 2595.0f * log10f(1.0f + hz / 700.0f);
}

static float melToHz(float mel) {
    return 700.0f * (powf(10.0f, mel / 2595.0f) - 1.0f);
}

WakeWordDetector::WakeWordDetector() {
    for (int i = 0; i < FRAME_SIZE; i++) {
        _hamming[i] = 0.54f - 0.46f * cosf(2.0f * (float) M_PI * (float) i / (FRAME_SIZE - 1));
    }
    // メルフィルタバンク (100Hz-8000Hz の三角窓)
    float melLo = melScale(100.0f), melHi = melScale((float) SAMPLE_RATE / 2);
    float points[NUM_MEL_FILTERS + 2];
    for (int i = 0; i < NUM_MEL_FILTERS + 2; i++) {
        float hz = melToHz(melLo + (melHi - melLo) * (float) i / (NUM_MEL_FILTERS + 1));
        points[i] = hz * FRAME_SIZE / SAMPLE_RATE;
    }
    for (int m = 0; m < NUM_MEL_FILTERS; m++) {
        int start = (int) ceilf(points[m]);
        int end = std::min((int) floorf(points[m + 2]), FFT_BINS - 1);
        std::vector<float> weights;
        for (int k = start; k <= end; k++) {
            float w = (k <= points[m + 1])
                      ? ((float) k - points[m]) / (points[m + 1] - points[m])
                      : (points[m + 2] - (float) k) / (points[m + 2] - points[m + 1]);
            weights.push_back(std::max(w, 0.0f));
        }
        _melFilters.emplace_back(start, std::move(weights));
    }
}

/**
 * In-place radix-2 FFT
 */
static void fft(float *re, float *im, int n) {
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            std::swap(re[i], re[j]);
            std::swap(im[i], im[j]);
        }
    }
    for (int len = 2; len <= n; len <<= 1) {
        float ang = -2.0f * (float) M_PI / (float) len;
        float wRe = cosf(ang), wIm = sinf(ang);
        for (int i = 0; i < n; i += len) {
            float curRe = 1.0f, curIm = 0.0f;
            for (int k = 0; k < len / 2; k++) {
                int a = i + k, b = i + k + len / 2;
                float tRe = re[b] * curRe - im[b] * curIm;
                float tIm = re[b] * curIm + im[b] * curRe;
                re[b] = re[a] - tRe;
                im[b] = im[a] - tIm;
                re[a] += tRe;
                im[a] += tIm;
                float nextRe = curRe * wRe - curIm * wIm;
                curIm = curRe * wIm + curIm * wRe;
                curRe = nextRe;
            }
        }
    }
}

float WakeWordDetector::_mfcc(const int16_t *frame, float *coeffs) {
    static float re[FRAME_SIZE], im[FRAME_SIZE];
    float sq = 0.0f;
    re[0] = (float) frame[0] * _hamming[0];
    im[0] = 0.0f;
    for (int i = 1; i < FRAME_SIZE; i++) {
        sq += (float) frame[i] * (float) frame[i];
        // プリエンファシスと窓掛け
        re[i] = ((float) frame[i] - 0.97f * (float) frame[i - 1]) * _hamming[i];
        im[i] = 0.0f;
    }
    fft(re, im, FRAME_SIZE);

    float mel[NUM_MEL_FILTERS];
    for (int m = 0; m < NUM_MEL_FILTERS; m++) {
        float sum = 0.0f;
        int start = _melFilters[m].first;
        auto &weights = _melFilters[m].second;
        for (size_t k = 0; k < weights.size(); k++) {
            int bin = start + (int) k;
            float power = re[bin] * re[bin] + im[bin] * im[bin];
            sum += power * weights[k];
        }
        mel[m] = logf(sum + 1e-6f);
    }
    // DCT-II (c0 はエネルギー項のため除外)
    for (int k = 1; k <= NUM_COEFFS; k++) {
        float sum = 0.0f;
        for (int m = 0; m < NUM_MEL_FILTERS; m++) {
            sum += mel[m] * cosf((float) M_PI * (float) k * ((float) m + 0.5f) / NUM_MEL_FILTERS);
        }
        coeffs[k - 1] = sum;
    }
    return sqrtf(sq / FRAME_SIZE);
}

int WakeWordDetector::feed(const int16_t *samples) {
    // 直前 hop と合わせて 1 フレーム分を作る
    int16_t frame[FRAME_SIZE];
    memcpy(frame, _prev, sizeof(_prev));
    memcpy(frame + HOP_SIZE, samples, HOP_SIZE * sizeof(int16_t));
    memcpy(_prev, samples, sizeof(_prev));

    Frame coeffs{};
    float rms = _mfcc(frame, coeffs.data());
    _window.push_back(coeffs);
    _rms.push_back(rms);
    size_t windowSize = _maxTemplateSize() * 3 / 2;
    while (_window.size() > windowSize) {
        _window.pop_front();
        _rms.pop_front();
    }

    if (_window.size() < _minTemplateSize()) return -1;
    if (++_frameCount % DETECT_INTERVAL != 0) return -1;
    if (*std::max_element(_rms.begin(), _rms.end()) < VAD_RMS) return -1;
    return (int) lroundf(_match() * 10);
}

float WakeWordDetector::_match() {
    // ウィンドウ側のケプストラム平均除去 (テンプレート登録時と同様、発話区間のみで計算)
    float mean[NUM_COEFFS] = {};
    int numActive = 0;
    for (size_t j = 0; j < _window.size(); j++) {
        if (_rms[j] < VAD_RMS / 2) continue;
        for (int k = 0; k < NUM_COEFFS; k++) mean[k] += _window[j][k];
        numActive++;
    }
    if (numActive > 0) {
        for (float &m: mean) m /= (float) numActive;
    }

    float best = INFINITY;
    for (const auto &tmpl: _templates) {
        best = std::min(best, _matchOne(tmpl, mean));
    }
    return best;
}

size_t WakeWordDetector::_maxTemplateSize() {
    size_t result = 0;
    for (const auto &tmpl: _templates) result = std::max(result, tmpl.size());
    return result;
}

size_t WakeWordDetector::_minTemplateSize() {
    size_t result = SIZE_MAX;
    for (const auto &tmpl: _templates) result = std::min(result, tmpl.size());
    return result;
}

float WakeWordDetector::_matchOne(const std::vector<Frame> &tmpl, const float *mean) {
    int t = (int) tmpl.size();
    int w = (int) _window.size();
    if (w < t) return INFINITY;

    // 部分列 DTW (ウィンドウ内の開始・終了位置は自由)
    std::vector<float> prev(w + 1, 0.0f), cur(w + 1);
    for (int i = 1; i <= t; i++) {
        cur[0] = INFINITY;
        for (int j = 1; j <= w; j++) {
            float d = 0.0f;
            for (int k = 0; k < NUM_COEFFS; k++) {
                float diff = tmpl[i - 1][k] - (_window[j - 1][k] - mean[k]);
                d += diff * diff;
            }
            cur[j] = sqrtf(d) + std::min({prev[j], cur[j - 1], prev[j - 1]});
        }
        std::swap(prev, cur);
    }
    return *std::min_element(prev.begin() + 1, prev.end()) / (float) t;
}

void WakeWordDetector::reset() {
    _window.clear();
    _rms.clear();
}

const char *WakeWordDetector::registerWav(const uint8_t *data, size_t size, bool append) {
    // RIFF/WAVE ヘッダを解析
    if (size < 44 || memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WAVE", 4) != 0) {
        return "not a WAV file";
    }
    const int16_t *samples = nullptr;
    size_t numSamples = 0;
    bool fmtOk = false;
    size_t pos = 12;
    while (pos + 8 <= size) {
        uint32_t chunkSize;
        memcpy(&chunkSize, data + pos + 4, 4);
        if (chunkSize > size - pos - 8 && memcmp(data + pos, "data", 4) != 0) {
            return "not a WAV file"; // 壊れたチャンクサイズ
        }
        if (memcmp(data + pos, "fmt ", 4) == 0 && chunkSize >= 16) {
            uint16_t format, channels, bits;
            uint32_t rate;
            memcpy(&format, data + pos + 8, 2);
            memcpy(&channels, data + pos + 10, 2);
            memcpy(&rate, data + pos + 12, 4);
            memcpy(&bits, data + pos + 22, 2);
            if (format != 1 || channels != 1 || rate != SAMPLE_RATE || bits != 16) {
                return "WAV must be PCM 16kHz/16bit/mono";
            }
            fmtOk = true;
        } else if (memcmp(data + pos, "data", 4) == 0) {
            samples = (const int16_t *) (data + pos + 8);
            numSamples = std::min((size_t) chunkSize, size - pos - 8) / sizeof(int16_t);
        }
        pos += 8 + (size_t) chunkSize + (chunkSize & 1);
        if (pos < (size_t) chunkSize) break; // オーバーフロー
    }
    if (!fmtOk || samples == nullptr) {
        return "WAV must be PCM 16kHz/16bit/mono";
    }
    if (!append) {
        _templates.clear();
        _numWords = 0;
    } else if (_numWords >= MAX_WORDS) {
        return "too many words registered";
    }
    auto error = _buildTemplate(samples, numSamples);
    if (error != nullptr) {
        if (!append) {
            // 置き換えで途中失敗した場合は保存済みテンプレートに戻す
            _templates.clear();
            _numWords = 0;
            loadTemplate();
        }
        return error;
    }
    if (!_saveTemplate()) {
        return "failed to save template";
    }
    reset();
    return nullptr;
}

const char *WakeWordDetector::_buildTemplate(const int16_t *samples, size_t numSamples) {
    if (numSamples < FRAME_SIZE) {
        return "audio too short";
    }
    int numFrames = (int) ((numSamples - FRAME_SIZE) / HOP_SIZE) + 1;

    // フレームごとの音量から発話区間を切り出す
    std::vector<float> rms(numFrames);
    float maxRms = 0.0f;
    for (int i = 0; i < numFrames; i++) {
        float sq = 0.0f;
        const int16_t *frame = samples + (size_t) i * HOP_SIZE;
        for (int j = 0; j < FRAME_SIZE; j++) sq += (float) frame[j] * (float) frame[j];
        rms[i] = sqrtf(sq / FRAME_SIZE);
        maxRms = std::max(maxRms, rms[i]);
    }
    float threshold = std::max(maxRms * 0.1f, VAD_RMS / 2);
    int first = 0, last = numFrames - 1;
    while (first < numFrames && rms[first] < threshold) first++;
    while (last > first && rms[last] < threshold) last--;
    first = std::max(first - 3, 0);
    last = std::min(last + 3, numFrames - 1);
    if (last - first + 1 < MIN_TEMPLATE_FRAMES) {
        return "audio too short";
    }
    if (last - first + 1 > MAX_TEMPLATE_FRAMES) {
        return "audio too long (max 4s of speech)";
    }

    // 半 hop ずつずらした系統を作り、フレーム位相ずれに強くする
    for (size_t offset = 0; offset < HOP_SIZE; offset += HOP_SIZE / 2) {
        if ((size_t) last * HOP_SIZE + offset + FRAME_SIZE > numSamples) break;
        std::vector<Frame> frames;
        float mean[NUM_COEFFS] = {};
        for (int i = first; i <= last; i++) {
            Frame coeffs{};
            _mfcc(samples + (size_t) i * HOP_SIZE + offset, coeffs.data());
            for (int k = 0; k < NUM_COEFFS; k++) mean[k] += coeffs[k];
            frames.push_back(coeffs);
        }
        for (float &m: mean) m /= (float) frames.size();
        for (auto &f: frames) {
            for (int k = 0; k < NUM_COEFFS; k++) f[k] -= mean[k];
        }
        _templates.push_back(std::move(frames));
    }
    _numWords++;
    Serial.printf("wakeword: template built (%d words, %d sequences)\n",
                  _numWords, (int) _templates.size());
    return nullptr;
}

bool WakeWordDetector::_saveTemplate() {
    bool result = false;
    if (!SPIFFS.begin(true)) {
        Serial.println("ERROR: Failed to begin SPIFFS");
        return false;
    }
    File f = SPIFFS.open(TEMPLATE_PATH, "w");
    if (!f) {
        Serial.printf("ERROR: Failed to open SPIFFS for writing (path=%s)\n", TEMPLATE_PATH);
    } else {
        uint16_t numWords = _numWords, numSequences = _templates.size(), numCoeffs = NUM_COEFFS;
        f.write((uint8_t *) &TEMPLATE_MAGIC, sizeof(TEMPLATE_MAGIC));
        f.write((uint8_t *) &numWords, sizeof(numWords));
        f.write((uint8_t *) &numSequences, sizeof(numSequences));
        f.write((uint8_t *) &numCoeffs, sizeof(numCoeffs));
        for (const auto &tmpl: _templates) {
            uint16_t numFrames = tmpl.size();
            f.write((uint8_t *) &numFrames, sizeof(numFrames));
            for (const auto &frame: tmpl) {
                f.write((uint8_t *) frame.data(), sizeof(float) * NUM_COEFFS);
            }
        }
        f.close();
        result = true;
    }
    SPIFFS.end();
    return result;
}

bool WakeWordDetector::loadTemplate() {
    bool result = false;
    if (!SPIFFS.begin(true)) {
        Serial.println("ERROR: Failed to begin SPIFFS");
        return false;
    }
    File f = SPIFFS.open(TEMPLATE_PATH, "r");
    if (f && f.size() > 10) {
        uint32_t magic;
        uint16_t numWords, numSequences, numCoeffs;
        f.read((uint8_t *) &magic, sizeof(magic));
        f.read((uint8_t *) &numWords, sizeof(numWords));
        f.read((uint8_t *) &numSequences, sizeof(numSequences));
        f.read((uint8_t *) &numCoeffs, sizeof(numCoeffs));
        if (magic == TEMPLATE_MAGIC && numCoeffs == NUM_COEFFS
            && numWords >= 1 && numWords <= MAX_WORDS
            && numSequences >= numWords && numSequences <= numWords * 2) {
            std::vector<std::vector<Frame>> templates;
            bool valid = true;
            for (int i = 0; i < numSequences; i++) {
                uint16_t numFrames;
                if (f.read((uint8_t *) &numFrames, sizeof(numFrames)) != sizeof(numFrames)
                    || numFrames < MIN_TEMPLATE_FRAMES || numFrames > MAX_TEMPLATE_FRAMES) {
                    valid = false;
                    break;
                }
                std::vector<Frame> tmpl(numFrames);
                for (auto &frame: tmpl) {
                    valid &= f.read((uint8_t *) frame.data(), sizeof(float) * NUM_COEFFS)
                             == sizeof(float) * NUM_COEFFS;
                }
                templates.push_back(std::move(tmpl));
            }
            if (valid) {
                _templates = std::move(templates);
                _numWords = numWords;
                Serial.printf("wakeword: template loaded (%d words, %d sequences)\n",
                              _numWords, (int) _templates.size());
                result = true;
            }
        }
        if (!result) {
            Serial.println("ERROR: Invalid wakeword template");
        }
    }
    if (f) f.close();
    SPIFFS.end();
    return result;
}
