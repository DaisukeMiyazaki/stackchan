#include <Arduino.h>
#if !defined(WITHOUT_AVATAR)
#include <Avatar.h>
#endif // !defined(WITHOUT_AVATAR)

#include "app/AppFace.h"
#include "app/AppVoice.h"

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"

bool AppFace::init() {
    return true;
}

void AppFace::setup() {
#if !defined(WITHOUT_AVATAR)
    _avatar.init();
    _avatar.setBatteryIcon(true);
    _avatar.setSpeechFont(&fonts::efontJA_16);
#endif // !defined(WITHOUT_AVATAR)
}

void AppFace::start() {
#if !defined(WITHOUT_AVATAR)
    static auto face = this;
    _avatar.addTask([](void *args) { face->lipSync(args); }, "lipSync");
#endif // !defined(WITHOUT_AVATAR)
}

void AppFace::loop() {
#if !defined(WITHOUT_AVATAR)
    if (_lastBatteryStatus == 0 || millis() - _lastBatteryStatus > 5000) {
        _avatar.setBatteryStatus(M5.Power.isCharging(), M5.Power.getBatteryLevel());
        _lastBatteryStatus = millis();
    }
#endif // !defined(WITHOUT_AVATAR)
}

#if !defined(WITHOUT_AVATAR)
/**
 * Task to open mouth to match the voice.
 */
void AppFace::lipSync(void *args) {
    if (((m5avatar::DriveContext *) args)->getAvatar() != &_avatar) return;

    while (true) {
        _avatar.setMouthOpenRatio(_voice->getAudioLevel());
        delay(50);
    }
}
#endif // !defined(WITHOUT_AVATAR)

#pragma clang diagnostic pop

/**
 * Set text to speech bubble
 *
 * @param text text
 */
void AppFace::setText(const char *text) {
#if !defined(WITHOUT_AVATAR)
    _avatar.setSpeechText(text);
#endif // !defined(WITHOUT_AVATAR)
}

/**
 * Set face expression
 *
 * @param expression expression
 */
bool AppFace::setExpression(Expression expression) {
#if !defined(WITHOUT_AVATAR)
    static const m5avatar::Expression EXPRESSIONS[] = {
            m5avatar::Expression::Neutral,
            m5avatar::Expression::Happy,
            m5avatar::Expression::Sleepy,
            m5avatar::Expression::Doubt,
            m5avatar::Expression::Sad,
            m5avatar::Expression::Angry,
    };
    int numExpressions = sizeof(EXPRESSIONS) / sizeof(EXPRESSIONS[0]);
    if (expression >= numExpressions) {
        Serial.printf("ERROR: Unknown expression: %d", expression);
        return false;
    }
    Serial.printf("Setting expression: %d\n", expression);
    _avatar.setExpression(EXPRESSIONS[expression]);
#endif // !defined(WITHOUT_AVATAR)
    return true;
}
