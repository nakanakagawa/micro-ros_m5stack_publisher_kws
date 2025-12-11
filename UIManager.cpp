#include "UIManager.h"

// 縦幅240？
#define UI_STATUS_H     20
#define UI_HEARD_H      60
#define UI_KEYWORD_H    90
#define UI_STATE_H      90
#define UI_BUTTON_H     60

String lastHeardText;

bool startOn = false;  // ボタン状態

UIManager::UIManager() {
    // アニメーション用変数 定義
    rectHeight = 5;
    growing = true;
    lastUpdate = 0;
    animStart = 0;
    animDuration = 0;
    animRunning = false;
}

// 初期状態
void UIManager::begin() {
    M5.Display.clear(BLACK); // 全消し

    // ★ 枠線
    M5.Display.fillRect(0,UI_STATUS_H,320,UI_HEARD_H,0x0200);// 黒っぽい緑

    updateStatus(false); // Recognized text 表示


    // ★ command 枠
    int y1 =  UI_STATUS_H + UI_HEARD_H ;
    M5.Display.fillRect(80, y1+8, 120, 20, WHITE);
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(BLACK);
    M5.Display.setCursor(100, y1+12);
    M5.Display.print("Command");
    M5.Display.drawRect(80, y1+28, 240, UI_KEYWORD_H-28, WHITE);

    // 状態表示[左下] x:10-70 y:95-155
    M5.Display.drawRect(10, 108, 60, 60, WHITE);
    M5.Display.fillCircle(30, 133, 2, WHITE);
    M5.Display.fillCircle(50, 133, 2, WHITE);
    M5.Display.fillRect(34, 142, 14, 2, WHITE);

    // ボタン表示[真ん中]
    int y2 = UI_STATUS_H + UI_HEARD_H + UI_KEYWORD_H  ;
    drawStopButton(false);

    // ボタン表示[左]
    drawStartButton(false);

}

// 一番上の帯 表示 true: 緑， false: 白
void UIManager::updateStatus(bool ready) {

    uint16_t color = ready ? GREEN : WHITE;  // 緑 or 白を選択

    // 上部ステータス背景
    M5.Display.fillRect(0, 0, 320, UI_STATUS_H, color);

    // “Recognized Text” の枠線
    M5.Display.drawRect(0, UI_STATUS_H, 320, UI_HEARD_H, color);
    M5.Display.drawRect(1, UI_STATUS_H + 1, 318, UI_HEARD_H - 2, color);

    // 文字表示
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(BLACK);
    M5.Display.setCursor(10, 4);
    M5.Display.print("Recognized Text");
}

// 認識文字 表示 
void UIManager::updateHeardText(const String& text) {
    lastHeardText = text;
    M5.Display.fillRect(42, UI_STATUS_H+2, 276, UI_HEARD_H-4, BLACK);

    M5.Display.setCursor(45, UI_STATUS_H + 10);
    M5.Display.setTextSize(3);
    
    M5.Display.setTextColor(WHITE);

    M5.Display.print(text);
    if (cursorVisible) M5.Display.print("|");

}


// カーソル表示
void UIManager::tickCursor() {
    if (millis() - lastCursorToggle > 500) {
        cursorVisible = !cursorVisible;
        lastCursorToggle = millis();
        updateHeardText(lastHeardText);
    }
}

// キーワードの表示 [右下]
void UIManager::updateKeyword(const String& keyword) {
    int y1 =  UI_STATUS_H + UI_HEARD_H ;
    M5.Display.fillRect(81, y1 + 29 , 238, UI_KEYWORD_H-30, BLACK);

    M5.Display.setTextColor(WHITE);
    M5.Display.setTextSize(1.5);
    M5.Lcd.setTextFont(4);
    M5.Display.setCursor(95, UI_STATUS_H + UI_HEARD_H + 45);

    M5.Display.print(keyword);
    M5.Lcd.setTextFont(1);
}


// アニメーション開始
void UIManager::startRectAnimation(unsigned long duration_ms) {
    animStart = millis();
    animDuration = duration_ms;
    animRunning = true;
    rectHeight = 2;
    growing = true;
}


// アニメーション更新
void UIManager::animateRect() {
    if (!animRunning) return;

    // アニメーション終了判定
    if (millis() - animStart > animDuration) {
        animRunning = false;
        // 四角を消す
        M5.Display.fillRect(34, 144, 14, 6, BLACK);
        return;
    }

    // 更新間隔
    const int interval = 50;
    if (millis() - lastUpdate < interval) return;
    lastUpdate = millis();

    // 前の描画を消す
    M5.Display.fillRect(34, 144, 14, 6, BLACK);

    // 高さの更新
    if (growing) {
        rectHeight += 2;
        if (rectHeight >= 6) {
            rectHeight = 6;
            growing = false;
        }
    } else {
        rectHeight -= 2;
        if (rectHeight <= 2) {
            rectHeight = 2;
            growing = true;
        }
    }

    // 四角の描画
    M5.Display.fillRect(34, 142, 14, rectHeight, WHITE);
}

// Aボタン表示関数
void UIManager::drawStartButton(bool on) {
    uint16_t color = on ? GREEN : WHITE;

    int x = 34;
    int y = 198;
    int w = 60;
    int h = 32;

    M5.Display.fillRect(x, y, w, h, color);

    // ▼マーク（三角）
    M5.Display.fillTriangle(
        x + 30, y + 42,   // 頂点（元の 64, 240）
        x + 25, y + 32,   // 左下（元の 59, 230）
        x + 35, y + 32,   // 右下（元の 69, 230）
        color
    );

    M5.Display.setTextColor(BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(x + 1, y + 8);
    M5.Display.print("START");
}


// Bボタンを押したときの処理
void UIManager::drawStopButton(bool pressed) {
    uint16_t color = pressed ? YELLOW : WHITE;

    // STOPボタン位置
    int x = 130;int y = 198;int w = 60;int h = 32;

    // 背景
    M5.Display.fillRect(x, y, w, h, color);

    // ▼ボタン（三角）
    M5.Display.fillTriangle(
        160, 240,  // 頂点
        155, 230,  // 左下
        165, 230,  // 右下
        color
    );

    // STOP文字
    M5.Display.setTextColor(BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(x + 8, y + 8);
    M5.Display.print("STOP");
}





