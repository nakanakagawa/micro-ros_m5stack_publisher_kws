
// キーワードを認識したら，micro-rosでトピックを送信するコード．（製作中）

#include <Arduino.h>
#include <M5Unified.h>
#include <M5ModuleLLM.h>
#include <vector>
#include "Logger.h"  // ヘッダファイル読み込み
#include "UIManager.h"

// マイクアイコン関係
extern const unsigned char micro_white[];
extern const unsigned int micro_white_len;

// UI関係
UIManager ui;


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
String kws_work_id; // 


// ===== micro-ROSオブジェクト =====
rcl_publisher_t publisher;
rcl_node_t node;
rclc_support_t support;
rcl_allocator_t allocator;
std_msgs__msg__Int32 msg;
rcl_init_options_t init_options; // ドメインID設定関係のやつ
size_t domain_id = 27; // ROS_DOMAIN_ID指定
bool humble = true;
bool claude = true; // デバッグ用
// std_msgs__msg__String msg;
// ==================================


// ===== ログスクロール用 =====
std::vector<LogEntry> logs;
int scroll_index = 0;  // 今表示している行のインデックス
int lines_per_screen = 7;  // 下半分に入る行数
// ============================


// ============ 命令 ==============
struct Command {  // 構造体を定義
    const char* name;      // キーワード
    const char* log_text;  // 表示メッセージ
    const char* tts_file;  // 音声メッセージ
    int value;             // topicの値
};
// 定義した構造体の配列をつくる
const Command command_table[] = { // キーワードのリスト
    { " go",    "GO!!!",          "go.go",        11  }, // キーワード，表示，音声，トピック
    { " stop",  "STOP!!",         "stop.stop",    0   }, // キーワードを指定する際，キーワードの前に空白を入れないと →
    { " wait",  "WAIT!!",         "wait.wait",    0   }, // 認識をしてくれないため注意
    { " right", "turn right!!",   "turn right",   3   },
    { " left",  "turn left!!",    "turn left",    4   },
    { " back",  "BACK!!",         "back.back",    10  },
    { " slow",  "SLOW !!",        "slow.slow",    1   },
    { " dance", "DANCING",        "dancing",      6   },
    { " spin",  "SPIN!",          "spin.spin",    99  }  // 一回転する
};
const int NUM_COMMANDS = sizeof(command_table) / sizeof(command_table[0]);
// ================================

// ===========================
// ローディングバー管理
// ===========================
int loadingSteps = 10;      // 何分割するか
int currentStep = 0;        // 現在のステップ
int barW = 250;
int barH = 18;
// 中央に配置した座標
int barX = (320 - barW) / 2;
int barY = (240 - barH) / 2 + 20;
String miniLog = "";


// #defineはマクロ定義．右のをマクロ名に置き換え．
#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){error_loop();}} // C言語の簡略エラーチェック
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){}} // fn の実行結果がエラーだったら error_loop() を呼び出す


void error_loop() { // micro-rosに接続エラーが発生した場合
    while (1) { 
        M5.Display.setTextColor(TFT_RED);      // M5にエラー表示
        // addLog("micro-ROS Error!");
        delay(1000);
    }
}

// Now Loading 描画
void drawLoadingText(const char* text) {
    M5.Display.setTextSize(2.8);
    M5.Display.setTextColor(WHITE, BLACK);

    int textWidth = M5.Display.textWidth(text);

    int textX = barX + (barW - textWidth) / 2;
    int textY = barY - 40;  // バーの少し上

    // テキスト部分だけクリア
    M5.Display.fillRect(barX, textY, barW, 12, BLACK);

    M5.Display.setCursor(textX, textY);
    M5.Display.print(text);
}

// ローディングバー初期化
void initLoadingBar(int steps) {
    loadingSteps = steps;
    currentStep = 0;
    M5.Display.drawRect(barX, barY, barW, barH, GREEN); // 外枠だけ描く
    // ここで一度だけ Now Loading を表示（固定）
    drawLoadingText("Now Loading...");
}


// ローディング中 左下ログ
void drawMiniLog(String msg) {
    miniLog = msg;

    int x = 5;
    int y = 220;   
    int w = 310;
    int h = 30;

    // 背景クリア（この領域だけ黒で塗りつぶす）
    M5.Display.fillRect(x, y, w, h, BLACK);

    // 文字描画
    M5.Display.setTextSize(2);
    // M5.Lcd.setTextFont(4);
    M5.Display.setTextColor(WHITE, BLACK);
    M5.Display.setCursor(x, y);
    M5.Display.print(miniLog);
}

// ローディングバー 伸ばす
void drawLoadingBarStep(String logMessage) {
    if (currentStep >= loadingSteps) return;

    float progress = (float)(currentStep + 1) / loadingSteps;
    int filled = barW * progress;

    // 塗りつぶし部分を更新
    M5.Display.fillRect(barX, barY, filled, barH, GREEN);

    currentStep++;

        // 2) 左下に一行ログ
    drawMiniLog(logMessage);
}


// ======== ASRコールバック（音声認識結果）========
// void on_asr_data_input(String data, bool isFinish, int index)
void on_asr_data_input(std_msgs__msg__Int32 &msg)
{
    // addLog("SOSHIN!"); // HELLOを検出した時に表示

    msg.data = 1;  // 1を送信（キーワード検出を意味する）
    RCSOFTCHECK(rcl_publish(&publisher, &msg, NULL));  // トピック送信用関数を呼び出す
}


// ======== LLMコールバック（AI応答）========
void on_llm_data_input(String data, bool isFinish, int index) {
    // addLog(data);
    if (isFinish) {
        // addLog("\n"); // LLMの応答を受け取ってM5に表示？
    }
}

// micro-ros 初期化
bool initMicroROS() { // pub設定
  allocator = rcl_get_default_allocator();

    /* Humble ROS_DOMAIN_ID設定 */

    // create init_options
    init_options = rcl_get_zero_initialized_init_options();
    if (rcl_init_options_init(&init_options, allocator) != RCL_RET_OK) {
        return false;
    }

    // Set ROS domain id
    if (rcl_init_options_set_domain_id(&init_options, domain_id) != RCL_RET_OK) {
        return false;
    }

    // Setup support structure.
    if (rclc_support_init_with_options(&support, 0, NULL, &init_options, &allocator)
        != RCL_RET_OK) {
        return false;
    }

    // create node
    if (rclc_node_init_default(&node, "micro_ros_arduino_node", "", &support)
        != RCL_RET_OK) {
        return false;
    }

    if (rclc_publisher_init_best_effort(
            &publisher, &node,
            ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
            "voice_trigger") != RCL_RET_OK) {
        return false;
    }  

    return true;
}



void setup()
{
    M5.begin();
    initLoadingBar(10); 
    // setTextScroll(); // テキスト設定
    // addLog("Voice ROS Pub", TFT_CYAN); 
    drawLoadingBarStep("Check ModuleLLM connection..");

    // ===== Module LLM 初期化 =====
    int rxd = M5.getPin(m5::pin_name_t::port_c_rxd);
    int txd = M5.getPin(m5::pin_name_t::port_c_txd);
    Serial2.begin(115200, SERIAL_8N1, rxd, txd);

    delay(100);  // 100ms 程度待つ
    module_llm.begin(&Serial2);

    /* LLMmodule接続チェック */ 
    // addLog(">> Check ModuleLLM connection..\n"); 
    while (!module_llm.checkConnection()) {
        delay(500);
        // addLog(".");
    }
    // addLog("ModuleLLM connected!");
    drawLoadingBarStep("Reset ModuleLLM.." );

    /* Reset ModuleLLM */
    module_llm.sys.reset();
    delay(500);  // 少し待つ

drawLoadingBarStep("micro-ROS connection..");

    // ===== micro-ROS接続 =====
    int target_agent = 0; // 0 = PC ; 1= Jetson

    if (target_agent == 0) {
        set_microros_wifi_transports("Buffalo-2G-0768", "h33833p5wu8k6", "192.168.11.16", 8888);
    } else if (target_agent == 1) {
        set_microros_wifi_transports("GL-AR750S-064", "goodlife", "192.168.8.233", 8888);
    }

drawLoadingBarStep("Wi-Fi connection..");
    // Wi-Fi接続待機（確実に接続完了を待つ）📡
    int wifi_wait = 0;
    while (WiFi.status() != WL_CONNECTED && wifi_wait < 20) {
        delay(200);
        // addLog(".");
        wifi_wait++;
    }
    // addLog("Wi-Fi ready");
drawLoadingBarStep("micro-ROS setup..");
    delay(2000);



    // =====  micro-ROS 初期化 ===== ⚡
    RCSOFTCHECK(rclc_executor_spin_some(NULL, RCL_MS_TO_NS(100)));

    if (!initMicroROS()) {
        // 一回だけ表示
        M5.Display.fillScreen(BLACK);
        M5.Display.setCursor(10, 40);
        M5.Display.println("micro-ros-agent not found!/n");
        M5.Display.println("Please start micro-ros-agent,");
        M5.Display.println("then press the M5 reboot button.");

        // 完全停止して待つ
        while (true) {
            delay(100);
        }
    }

drawLoadingBarStep("LLM module connection..");

    while (!module_llm.checkConnection()) {
        delay(500);
    }

drawLoadingBarStep("KWS setup..");
    // ===== KWSセットアップ ===== 🔑 キーワード設定
    m5_module_llm::ApiKwsSetupConfig_t kws_config;

    kws_config.kws = "HELLO"; // キーワードはここで変更可能
    kws_work_id = module_llm.kws.setup(kws_config, "kws_setup", "en_US");
    wake_up_keyword = kws_work_id;
    if (kws_work_id.isEmpty()) { // モデルの接続チェック
        // addLog("\nKWS setup failed!");
        while (1);
    }
drawLoadingBarStep("Setup ASR..");

    // Setup ASR 
    m5_module_llm::ApiAsrSetupConfig_t asr_config;
    asr_config.input = {"sys.pcm", kws_work_id};
    asr_work_id = module_llm.asr.setup(asr_config, "asr_setup", "en_US");
    if (asr_work_id.isEmpty()) { // エラー
    }
drawLoadingBarStep("Setup Audio mdule..");

    /* Setup Audio module */
    // addLog(">> Setup audio..");
    module_llm.audio.setup();
   drawLoadingBarStep("setup TTS..");
    delay(500);  // 少し待つ

    /* Setup TTS module and save returned work id 📝→🎤*/ 
    // addLog(">> Setup tts..");
    m5_module_llm::ApiMelottsSetupConfig_t melotts_config;
    melotts_work_id = module_llm.melotts.setup(melotts_config, "melotts_setup", "en_US");


    delay(2000);  // 少し待つ
 
    // addLog("junbe kanryou!", TFT_GREEN);
    /* TTSで音声出力（10秒タイムアウト） */ 
    // module_llm.melotts.inference(melotts_work_id, "OK!", 5000);
    ui.begin();

    ui.updateStatus(true);
    // M5.Display.drawPngFile("/micro_white.png", 600,60,30);
    M5.Display.drawPng(micro_white,micro_white_len, 3, 30, // マイクアイコン表示
        0, 0,            // maxWidth, maxHeight（0 なら無視）
        0, 0,            // offX, offY
        0.08f, 0.08f       // ← 画像サイズ縮小！！
    );
}


void loop()
{
    M5.update(); 
    module_llm.update();
    ui.tickCursor();


    
    /* 受信したメッセージを1つずつ処理 */
    for (auto& llm_msg : module_llm.msg.responseMsgList) { //responseMsgList: LLMモジュールから送られてきたメッセージのリスト
        
        if (llm_msg.work_id == kws_work_id) { /* ウェイクワード検出 HELLO */
            // addLog(">> Keyword detected", TFT_GREENYELLOW);
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
                // addLog(asr_result.c_str(), TFT_YELLOW); // 検出した文字を表示
                ui.updateHeardText(asr_result.c_str());

                for (int i = 0; i < NUM_COMMANDS; i++) { // キーワードごとの処理を実行
                    if (asr_result == command_table[i].name) {

                        // addLog(command_table[i].log_text); // ログ記述
                        ui.updateKeyword(command_table[i].log_text);

                        module_llm.melotts.inference( // 声で知らせる
                            melotts_work_id,
                            command_table[i].tts_file,
                            2000
                        );

                        msg.data = command_table[i].value;
                        RCSOFTCHECK(rcl_publish(&publisher, &msg, NULL)); // topicの送信

                        // ui.updateRobotState("nanikashira");

                        // addLog(String("Topic sent: ") + msg.data, TFT_CYAN);
                        delay(500);

                        break;
                    }
                }
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
    // テスト：Bボタンで🐢停止
    if (M5.BtnB.wasPressed()) {
        static int n = 0;
        ui.drawStopButton(true);   // 黄色にする
        // addLog("Log %d", n++);
        msg.data = 0;  // 停止！！！
        RCSOFTCHECK(rcl_publish(&publisher, &msg, NULL));
        // addLog("Topic sent: 0", TFT_CYAN);
        delay(500);
    }
    // Bボタン離したとき
    if (M5.BtnB.wasReleased()) {
        ui.drawStopButton(false);  // 白に戻す
    }

    // RCSOFTCHECK(rcl_publish(&publisher, &msg, NULL));
    module_llm.msg.responseMsgList.clear();
}