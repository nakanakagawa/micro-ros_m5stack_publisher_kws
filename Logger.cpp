#include "Logger.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <vector>

// ===== ログスクロール用 =====
extern std::vector<LogEntry> logs;
extern int scroll_index;  // 今表示している行のインデックス
extern int lines_per_screen;  // 下半分に入る行数
int top = 16*8;               // 下半分のY位置 
const int CHAR_PER_LINE = 26;  // 1行に収まる文字数（画面幅やフォントサイズで調整）
// ============================

void drawHeader() { //ヘッダー描画
    // ヘッダー領域を描画（固定表示）
    M5.Display.fillRect(0, 0, M5.Display.width(), top, TFT_DARKGREEN); // 背景色
    M5.Display.setTextColor(TFT_WHITE); // 文字色
    M5.Display.setCursor(4, 4); // 
    M5.Display.setTextSize(2); // 文字色
    M5.Display.println("Voice ROS Pub");
    M5.Display.setCursor(4, 24);

    // M5.Lcd.setTextFont(&fonts::efontJA_16);
    // M5.Display.setTextSize(2);
    // M5.Lcd.println("こんにちは");

    M5.Display.setTextSize(1);
    M5.Display.printf("Logs: %d/%d", 
        min(scroll_index + lines_per_screen, (int)logs.size()), 
        (int)logs.size());
}


void setTextScroll() { // ログのスクロール設定
    auto cfg = M5.config();  
    M5.begin(cfg);// M5初期化
    M5.Display.fillScreen(TFT_BLACK); // 画面全体を塗りつぶす
    M5.Display.setTextSize(2); // 文字サイズ：16. 15行入る計算

    M5.Display.setTextScroll(false);// 手動スクロール制御のため、自動スクロールは無効化
    drawHeader();

}


/* Log表示，保存用 */
void drawLogs() {
    // スクロール領域のみクリア
    M5.Display.fillRect(0, top, M5.Display.width(), M5.Display.height() - top, TFT_BLACK);
    
    M5.Display.setTextSize(2);
    
    // スクロール位置の調整
    int max_scroll = max(0, (int)logs.size() - lines_per_screen);
    scroll_index = constrain(scroll_index, 0, max_scroll);
    
    // ログを描画
    int y = top;
    int display_count = 0;
    for (int i = scroll_index; i < (int)logs.size() && display_count < lines_per_screen; i++) {
        // 保存されている色を使用
        M5.Display.setTextColor(logs[i].color, TFT_BLACK);
        M5.Display.setCursor(0, y);
        M5.Display.println(logs[i].text);
        y += 16;
        display_count++;
    }
    
    drawHeader(); // ヘッダーを再描画（ログ数表示を更新）
}


void addLog(const char *format, ...) {
    char buf[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    String line = String(buf);
    
    // \nで分割して処理
    int start = 0;
    int pos = 0;
    while (pos <= line.length()) {
        if (pos == line.length() || line[pos] == '\n') {
            String segment = line.substring(start, pos);
            
            // 空行も保存（\nだけの場合）
            if (segment.length() == 0) {
                logs.push_back(LogEntry("", TFT_WHITE));
            } else {
                // 折り返して保存
                for (int i = 0; i < segment.length(); i += CHAR_PER_LINE) {
                    String sub = segment.substring(i, min(i + CHAR_PER_LINE, (int)segment.length()));
                    logs.push_back(LogEntry(sub, TFT_WHITE));
                }
            }
            start = pos + 1;
        }
        pos++;
    }

    // 自動的に最新ログにスクロール
    if ((int)logs.size() > lines_per_screen) {
        scroll_index = logs.size() - lines_per_screen;
    } else {
        scroll_index = 0;
    }

    drawLogs();
}


void addLog(const String &msg) {
    addLog(msg.c_str());
}


// 色指定版のaddLog
void addLog(const char *format, uint16_t color, ...) {
    char buf[256];
    va_list args;
    va_start(args, color);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    String line = String(buf);
    
    // \nで分割して処理
    int start = 0;
    int pos = 0;
    while (pos <= line.length()) {
        if (pos == line.length() || line[pos] == '\n') {
            String segment = line.substring(start, pos);
            
            if (segment.length() == 0) {
                logs.push_back(LogEntry("", color));
            } else {
                for (int i = 0; i < segment.length(); i += CHAR_PER_LINE) {
                    String sub = segment.substring(i, min(i + CHAR_PER_LINE, (int)segment.length()));
                    logs.push_back(LogEntry(sub, color));
                }
            }
            start = pos + 1;
        }
        pos++;
    }

    if ((int)logs.size() > lines_per_screen) {
        scroll_index = logs.size() - lines_per_screen;
    } else {
        scroll_index = 0;
    }

    drawLogs();
}


void addLog(const String &msg, uint16_t color) {
    addLog(msg.c_str(), color);
}


