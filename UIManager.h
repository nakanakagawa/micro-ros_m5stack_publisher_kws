#pragma once
#include <M5Unified.h>

class UIManager {
public:
UIManager();  // ← これを追加（宣言）
    void begin();  // 初期化
    void updateStatus(bool ready); // ステータスバー更新
    void updateHeardText(const String& text); // マイク文字更新
    void updateKeyword(const String& keyword); // キーワード表示更新
    void tickCursor(); // カーソル点滅
    void drawStopButton(bool pressed); // Bボタン押したときの処理
    void drawStartButton(bool pressed); // Aボタンを押した時の処理
    void startRectAnimation(unsigned long duration_ms); // アニメーション用
    void animateRect(); // アニメーション用
private:
    bool cursorVisible;
    unsigned long lastCursorToggle;

    // アニメーション用
    int rectHeight;
    bool growing;
    unsigned long lastUpdate;
    unsigned long animStart;
    unsigned long animDuration;
    bool animRunning;
};
