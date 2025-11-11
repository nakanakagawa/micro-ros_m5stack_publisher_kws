
// キーワードを認識したら，micro-rosでトピックを送信するコード．（製作中）

#include <Arduino.h>
#include <M5Unified.h>
#include <M5ModuleLLM.h>
#include <vector>

// #include <M5Stack.h>
// #include <stdio.h>

// ===== micro-ROS関連 =====
#include <micro_ros_arduino.h>
#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
// #include <std_msgs/msg/string.h>
#include <std_msgs/msg/int32.h>


// ===== LLM関連 =====
M5ModuleLLM module_llm; //LLMモジュール全体（ASR, TTS, LLM など）を統括するクラス
String melotts_work_id;
String wake_up_keyword;
String second_keyword;
String asr_work_id;

String kws_work_id;
String kws_ECHO_id;

// 状態管理
bool is_awake = false;   // ウェイクアップ状態を管理
unsigned long awake_time = 0;  // ウェイクアップした時刻
const unsigned long AWAKE_TIMEOUT = 10000;  // 10秒でタイムアウト

// ===== micro-ROSオブジェクト =====
rcl_publisher_t publisher;
rcl_node_t node;
rclc_support_t support;
rcl_allocator_t allocator;
std_msgs__msg__Int32 msg;
rcl_init_options_t init_options; // ドメインID設定関係のやつ
size_t domain_id = 27; // ROS_DOMAIN_ID指定
bool humble = true;

// std_msgs__msg__String msg;

// ログエントリ構造体（テキストと色をセットで保存）
struct LogEntry {
    String text;
    uint16_t color;
    
    LogEntry(String t, uint16_t c = TFT_WHITE) : text(t), color(c) {}
};

// ===== ログスクロール用 =====
std::vector<LogEntry> logs;
int scroll_index = 0;  // 今表示している行のインデックス
int lines_per_screen = 7;  // 下半分に入る行数
int top = 16*8;               // 下半分のY位置
const int CHAR_PER_LINE = 26;  // 1行に収まる文字数（画面幅やフォントサイズで調整）
bool claude = true; // デバッグ用

// #defineはマクロ定義．右のをマクロ名に置き換え．
#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){error_loop();}} // C言語の簡略エラーチェック
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){}} // fn の実行結果がエラーだったら error_loop() を呼び出す


void error_loop() { // micro-rosに接続エラーが発生した場合
    while (1) { 
        M5.Display.setTextColor(TFT_RED);      // M5にエラー表示
        addLog("micro-ROS Error!");
        delay(1000);
    }
}


// ======== ASRコールバック（音声認識結果）========
// void on_asr_data_input(String data, bool isFinish, int index)
void on_asr_data_input(std_msgs__msg__Int32 &msg)
{
    addLog("SOSHIN!"); // HELLOを検出した時に表示

    msg.data = 1;  // 1を送信（キーワード検出を意味する）
    RCSOFTCHECK(rcl_publish(&publisher, &msg, NULL));  // トピック送信用関数を呼び出す
    // module_llm.melotts.inference(melotts_work_id, "SO SHI NNN SHIT AZO", 2000);
}


// ======== LLMコールバック（AI応答）========
void on_llm_data_input(String data, bool isFinish, int index) {
    addLog(data);
    if (isFinish) {
        addLog("\n"); // LLMの応答を受け取ってM5に表示？
    }
}

bool initMicroROS() { // pub設定
  allocator = rcl_get_default_allocator();

    /* Humble ROS_DOMAIN_ID設定 */

    // create init_options
    init_options = rcl_get_zero_initialized_init_options();
    RCCHECK(rcl_init_options_init(&init_options, allocator)); // <--- This was missing on ur side

    // Set ROS domain id
    RCCHECK(rcl_init_options_set_domain_id(&init_options, domain_id));

    // Setup support structure.
    RCCHECK(rclc_support_init_with_options(&support, 0, NULL, &init_options, &allocator));

    // create node
    RCCHECK(rclc_node_init_default(&node, "micro_ros_arduino_node", "", &support));

  RCCHECK(rclc_publisher_init_best_effort( // create publisher topic設定
    &publisher,
    &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
    "voice_trigger"));

  msg.data = 0;

    return true;
}

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

void setTextScroll() {
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


void setup()
{
    M5.begin();
    setTextScroll(); // テキスト設定
    addLog("Voice ROS Pub", TFT_CYAN); 

    // ===== Module LLM 初期化 =====
    int rxd = M5.getPin(m5::pin_name_t::port_c_rxd);
    int txd = M5.getPin(m5::pin_name_t::port_c_txd);
    Serial2.begin(115200, SERIAL_8N1, rxd, txd);

    delay(100);  // 100ms 程度待つ
    module_llm.begin(&Serial2);

    /* LLMmodule接続チェック */ 
    addLog(">> Check ModuleLLM connection..\n"); 
    while (!module_llm.checkConnection()) {
        delay(500);
        addLog(".");
    }
    addLog("ModuleLLM connected!");


    /* Reset ModuleLLM */
    module_llm.sys.reset();
    delay(500);  // 少し待つ


    // ===== micro-ROS接続 =====
    set_microros_wifi_transports("Buffalo-2G-0768", "h33833p5wu8k6", "192.168.11.16", 8888);


    // Wi-Fi接続待機（確実に接続完了を待つ）📡
    int wifi_wait = 0;
    while (WiFi.status() != WL_CONNECTED && wifi_wait < 20) {
        delay(200);
        addLog(".");
        wifi_wait++;
    }
    addLog("Wi-Fi ready");

  delay(2000);


    // =====  micro-ROS 初期化 ===== ⚡
    RCSOFTCHECK(rclc_executor_spin_some(NULL, RCL_MS_TO_NS(100)));

    if (!initMicroROS()) {
        addLog("\nmicro-ROS init failed!");
        while(1); // 止める
    }
    addLog("micro-ROS OK!");

    while (!module_llm.checkConnection()) {
        delay(500);
        addLog(".");
    }

    // ===== KWSセットアップ ===== 🔑 キーワード設定
    m5_module_llm::ApiKwsSetupConfig_t kws_config;

    kws_config.kws = "HELLO"; // キーワードはここで変更可能
    kws_work_id = module_llm.kws.setup(kws_config, "kws_setup", "en_US");
    wake_up_keyword = kws_work_id;
    if (kws_work_id.isEmpty()) { // モデルの接続チェック
        addLog("\nKWS setup failed!");
        while (1);
    }


    // Setup ASR
    m5_module_llm::ApiAsrSetupConfig_t asr_config;
    asr_config.input = {"sys.pcm", kws_work_id};
    asr_work_id = module_llm.asr.setup(asr_config, "asr_setup", "en_US");
    if (asr_work_id.isEmpty()) { // エラー
    }


    /* Setup Audio module */
    addLog(">> Setup audio..");
    module_llm.audio.setup();
   
    delay(500);  // 少し待つ

    /* Setup TTS module and save returned work id 📝→🎤*/ 
    addLog(">> Setup tts..");
    m5_module_llm::ApiMelottsSetupConfig_t melotts_config;
    melotts_work_id = module_llm.melotts.setup(melotts_config, "melotts_setup", "en_US");


    delay(2000);  // 少し待つ
 
    addLog("junbe kanryou!", TFT_GREEN);
    /* TTSで音声出力（10秒タイムアウト） */ 
    // module_llm.melotts.inference(melotts_work_id, "junvie kannriyoh!", 5000);
}

void loop()
{
    M5.update(); 
    module_llm.update();

    /* 受信したメッセージを1つずつ処理 */
    for (auto& llm_msg : module_llm.msg.responseMsgList) { //responseMsgList: LLMモジュールから送られてきたメッセージのリスト
        
        if (llm_msg.work_id == kws_work_id) { /* ウェイクワード検出 HELLO */
            addLog(">> Keyword detected", TFT_GREENYELLOW);
        }

        /* If ASR module message */
        if (llm_msg.work_id == asr_work_id) {
            /* Check message object type */
            if (llm_msg.object == "asr.utf-8.stream") {
                /* ASR結果の取り出し */
                JsonDocument doc;
                deserializeJson(doc, llm_msg.raw_msg);
                String asr_result = doc["data"]["delta"].as<String>();

                // M5.Display.printf(">> %s\n", asr_result.c_str()); 
                addLog(asr_result.c_str(), TFT_YELLOW); // 検出した文字を表示

                if (asr_result == " echo"){ 
                    addLog("ECHO", TFT_GREENYELLOW);
                    module_llm.melotts.inference(melotts_work_id, "ECHO.ECHO",2000);
                    
                    // グローバルのmsgを使用
                    msg.data = 10;
                    RCSOFTCHECK(rcl_publish(&publisher, &msg, NULL));
                    addLog("Topic sent: 10", TFT_CYAN);
                }

                if (asr_result == " yes"){ 
                    addLog("yes");
                    module_llm.melotts.inference(melotts_work_id, "yeah. very good. ",2000);
                    
                    msg.data = 20;  // 値を変えて区別できるようにする
                    RCSOFTCHECK(rcl_publish(&publisher, &msg, NULL));
                    addLog("Topic sent: 20", TFT_CYAN);
                    delay(500);
                }

                // if (asr_result == " hello"){ // OKと一致
                //   addLog("hello");
                //   module_llm.melotts.inference(melotts_work_id, "yeah. good ",8000);
                // }
            }
        }
    }
    
    

    // ボタン操作でスクロール 🔘
    if (M5.BtnA.wasPressed()) {
        scroll_index = max(0, scroll_index - 1);
        drawLogs();
    }
    if (M5.BtnC.wasPressed()) {
        int max_scroll = max(0, (int)logs.size() - lines_per_screen);
        scroll_index = min(max_scroll, scroll_index + 1);
        drawLogs();
    }
    // テスト：Bボタンで新しいログ追加
    if (M5.BtnB.wasPressed()) {
        static int n = 0;
        addLog("Log %d", n++);
    }
    // RCSOFTCHECK(rcl_publish(&publisher, &msg, NULL));
    module_llm.msg.responseMsgList.clear();
}