#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>  // ← String 型を使うために必要！
#include <M5Unified.h>  // ← TFT_WHITEなどの色定義に必要！
#include <vector>


// ログエントリ構造体（テキストと色をセットで保存）
struct LogEntry {
    String text;
    uint16_t color;
    
    LogEntry(String t, uint16_t c = TFT_WHITE) : text(t), color(c) {}
};

// ===== ログスクロール用 =====
extern std::vector<LogEntry> logs;
extern int scroll_index;  // 今表示している行のインデックス
extern int lines_per_screen;  // 下半分に入る行数
extern int top;               // 下半分のY位置
extern const int CHAR_PER_LINE;  // 1行に収まる文字数（画面幅やフォントサイズで調整）
// ============================

// ヘッダー描画
void drawHeader(); 

// ログのスクロール設定
void setTextScroll(); 

// ログの描画
void drawLogs(); 

// ログの保存
void addLog(const char *format, ...);

// ログの保存 2
void addLog(const String &msg); 

// 色指定時のログ保存
void addLog(const char *format, uint16_t color, ...); 

// 色指定時のログ 2
void addLog(const String &msg, uint16_t color);

#endif