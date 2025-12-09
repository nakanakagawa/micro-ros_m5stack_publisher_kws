#include "UIManager.h"

// 縦幅240？
#define UI_STATUS_H     20
#define UI_HEARD_H      60
#define UI_KEYWORD_H    90
#define UI_STATE_H      90
#define UI_BUTTON_H     60

String lastHeardText;


void UIManager::begin() {
    M5.Display.clear(BLACK);
    // M5.Display.clear(M5.Display.color565(30, 30, 30));

    // ★ 枠線
    M5.Display.drawRect(0, 0, 320, UI_STATUS_H, WHITE);

    // ★ 枠線
    M5.Display.fillRect(
        0,
        UI_STATUS_H,
        320,
        UI_HEARD_H,
        0x0200   // 黒っぽい緑
    );
    M5.Display.drawRect(0, UI_STATUS_H , 320, UI_HEARD_H , GREEN);
    M5.Display.drawRect(1, UI_STATUS_H+1 , 318, UI_HEARD_H-2 , GREEN);

    // ★ 枠線[右下] 
    int y1 =  UI_STATUS_H + UI_HEARD_H ;
    M5.Display.fillRect(80, y1+8, 120, 20, WHITE);
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(BLACK);
    M5.Display.setCursor(100, y1+12);
    M5.Display.print("Keyword");
    M5.Display.drawRect(80, y1+28, 240, UI_KEYWORD_H-28, WHITE);

    // 状態表示[左下] x:10-70 y:95-155
    M5.Display.drawRect(10, 108, 60, 60, WHITE);
    M5.Display.fillCircle(30, 133, 2, WHITE);
    M5.Display.fillCircle(50, 133, 2, WHITE);
    M5.Display.fillRect(34, 142, 14, 2, WHITE);

    // ボタン表示[下]
    int y2 = UI_STATUS_H + UI_HEARD_H + UI_KEYWORD_H  ;
    // M5.Display.drawRect(0, y2 + 8, 320, UI_BUTTON_H - 8, WHITE);
    M5.Display.fillRect(130, 198, 60, 32, WHITE);
    M5.Display.fillTriangle(
        160, 240,   // 頂点
        155, 230,   // 左下
        165, 230,   // 右下
        WHITE
    );
    M5.Display.setCursor(138, 206);
    M5.Display.print("STOP");

}

// 一番上の帯 表示
void UIManager::updateStatus(bool ready) {
    M5.Display.fillRect(0, 0, 320, UI_STATUS_H,
                        ready ? GREEN : RED);
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(BLACK);
    M5.Display.setCursor(10, 4);
    M5.Display.print(ready ? "Recognized Text" : "LOADING...");
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

// 一致したキーワードの表示 [右下]
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

// ロボットの状態を表すやつ [左下]
void UIManager::updateRobotState(const String& state) {

}

// ボタンを押したときの処理
void UIManager::drawStopButton(bool pressed) {
    uint16_t color = pressed ? YELLOW : WHITE;

    // STOPボタン位置
    int x = 130;
    int y = 198;
    int w = 60;
    int h = 32;

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

