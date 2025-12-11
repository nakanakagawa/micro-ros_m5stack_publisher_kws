
// キーワードを認識したら，micro-rosでトピックを送信するコード．（製作中）

#include <Arduino.h>
#include <M5Unified.h>
#include <M5ModuleLLM.h>
#include <vector>
#include "UIManager.h"

// マイクアイコン関係
extern const unsigned char micro_white[];
extern const unsigned int micro_white_len;

// UI関係
UIManager ui;

// Aボタン系の関数
unsigned long asr_end_time = 0;
bool asr_active = false;



// ===== micro-ROS関連 =====
#include <micro_ros_arduino.h>
#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
// #include <std_msgs/msg/string.h>
#include <std_msgs/msg/int32.h>
// ==========================

// ========= KWS関連 =========
bool kws_enabled = true;        // KWSが有効かどうか
unsigned long kws_resume_time = 0; // KWS再開予定時間
// ===========================


// ===== LLM関連 =====
M5ModuleLLM module_llm; //LLMモジュール全体（ASR, TTS, LLM など）を統括するクラス
String melotts_work_id;
String wake_up_keyword;
String second_keyword;
String asr_work_id;
String kws_work_id; 
// ===================

// ===== micro-ROSオブジェクト =====
rcl_publisher_t publisher;
rcl_node_t node;
rclc_support_t support;
rcl_allocator_t allocator;
std_msgs__msg__Int32 msg;
rcl_init_options_t init_options; // ドメインID設定関係のやつ
size_t domain_id = 27; // ROS_DOMAIN_ID指定
// ==================================



// ============ 命令 ==============
struct Command {  // 構造体を定義
    const char* name;      // キーワード
    const char* log_text;  // 表示メッセージ
    const char* tts_file;  // 音声メッセージ
    int value;             // topicの値
};

// 定義した構造体の配列をつくる
const Command command_table[] = { // キーワードのリスト
    { " go",    "GO ",          "go",        11  }, // キーワード，表示，音声，トピック
    { " stop",  "STOP ",         "stop",    0   }, // キーワードを指定する際，キーワードの前に空白を入れないと →
    { " wait",  "WAIT ",         "wait",    0   }, // 認識をしてくれないため注意
    { " right", "turn right ",   "turn right",   3   },
    { " left",  "turn left ",    "turn left",    4   },
    { " back",  "BACK ",         "back",    10  },
    { " slow",  "SLOW ",        "slow",    1   },
    { " dance", "DANCING",        "dancing",      6   },
    { " spin",  "SPIN",          "spin.spin",    99  }  // 一回転する
};
const int NUM_COMMANDS = sizeof(command_table) / sizeof(command_table[0]);
// ================================

// ===== ローディングバー管理 =====
int loadingSteps = 10;      // 何分割するか
int currentStep = 0;        // 現在のステップ
int barW = 250;
int barH = 18;
// 中央に配置した座標
int barX = (320 - barW) / 2;
int barY = (240 - barH) / 2 + 20;
String miniLog = "";
// =================================

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


// ASRを停止，起動する関数
void sendAsrCommand(const String& workId, const String& action) {
    String cmd = "{\"request_id\":\"3\",\"work_id\":\"" + workId +
                 "\",\"action\":\"" + action + "\"}";
    Serial2.println(cmd);
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
    drawLoadingBarStep("Check ModuleLLM connection..");

    // ===== Module LLM 初期化 =====
    int rxd = M5.getPin(m5::pin_name_t::port_c_rxd);
    int txd = M5.getPin(m5::pin_name_t::port_c_txd);
    Serial2.begin(115200, SERIAL_8N1, rxd, txd);

    delay(100);  // 100ms 程度待つ
    module_llm.begin(&Serial2);

    /* LLMmodule接続チェック */ 
    while (!module_llm.checkConnection()) {
        delay(500);
    }
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
        wifi_wait++;
    }
    drawLoadingBarStep("micro-ROS setup..");

    delay(2000);


    // =====  micro-ROS 初期化 ===== ⚡
    RCSOFTCHECK(rclc_executor_spin_some(NULL, RCL_MS_TO_NS(100)));

    if (!initMicroROS()) { // micro-rosが起動していない場合
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

    // Setup ASR 
    m5_module_llm::ApiAsrSetupConfig_t asr_config;
    asr_config.input = {"sys.pcm", kws_work_id};
    asr_work_id = module_llm.asr.setup(asr_config, "asr_setup", "en_US");
    if (asr_work_id.isEmpty()) { // エラー
    }
    sendAsrCommand(asr_work_id, "pause"); // ASRを停止
    drawLoadingBarStep("Setup Audio mdule..");

    /* Setup Audio module */
    module_llm.audio.setup();
    drawLoadingBarStep("setup TTS..");
    delay(500);  // 少し待つ

    /* Setup TTS module and save returned work id 📝→🎤*/ 
    m5_module_llm::ApiMelottsSetupConfig_t melotts_config;
    melotts_work_id = module_llm.melotts.setup(melotts_config, "melotts_setup", "en_US");

    delay(2000);  // 少し待つ
 
    /* TTSで音声出力（10秒タイムアウト） */ 
    // module_llm.melotts.inference(melotts_work_id, "OK!", 5000);

    ui.begin();
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
    ui.animateRect();   // アニメーション用

    
    /* 受信したメッセージを1つずつ処理 */
    for (auto& llm_msg : module_llm.msg.responseMsgList) { //responseMsgList: LLMモジュールから送られてきたメッセージのリスト


        /* If ASR module message */
        if (llm_msg.work_id == asr_work_id) { // キーワード検知
            /* Check message object type */
            if (llm_msg.object == "asr.utf-8.stream") {
                /* ASR結果の取り出し */
                JsonDocument doc;
                deserializeJson(doc, llm_msg.raw_msg);
                String asr_result = doc["data"]["delta"].as<String>();

                ui.updateHeardText(asr_result.c_str()); // 聞き取り音声表示

                for (int i = 0; i < NUM_COMMANDS; i++) { // キーワードごとの処理を実行
                    if (asr_result == command_table[i].name) {
                        
                        ui.updateKeyword(command_table[i].log_text); // キーワード表示

                        // ASR受付停止
                        sendAsrCommand(asr_work_id, "pause");
                        ui.drawStartButton(false);
                        ui.updateStatus(false);
                        asr_active = false;  // フラグをオフ
                        

                        module_llm.melotts.inference( // 声で知らせる
                            melotts_work_id,
                            command_table[i].tts_file,
                            0 // これで非同期にできた
                        );

                        ui.startRectAnimation(1500);  // 口を動かす ttsに阻まれる？

                        

                        msg.data = command_table[i].value;
                        RCSOFTCHECK(rcl_publish(&publisher, &msg, NULL)); // topicの送信

                        delay(500);

                        break;
                    }
                }
            }
        }
    }
    
    // ASR受付停止
    if (asr_active && millis() > asr_end_time) {
        sendAsrCommand(asr_work_id, "pause");
        ui.drawStartButton(false);
        ui.updateStatus(false);

        asr_active = false;  // フラグをオフ
    }

    // Aボタン操作
    if (M5.BtnA.wasPressed()) { // 左ボタン

        if (!asr_active) { // ASRが起動中でない場合
            // ASR開始
            sendAsrCommand(asr_work_id, "work");
            ui.drawStartButton(true);
            ui.updateStatus(true); 
            asr_active = true;
            asr_end_time = millis() + 7000;  // 7秒後にASR受付停止
        }
        else {
            // ASR動作中にもう一度押された → 停止 & リセット 色を白に．
            sendAsrCommand(asr_work_id, "pause");
            ui.drawStartButton(false);
            ui.updateStatus(false);
            asr_active = false;   // フラグをオフ
        }

        // sendAsrCommand(asr_work_id, "work");
        // M5.Speaker.setVolume(10);
        // ui.drawStartButton(true);

        // M5.Speaker.tone(440, 200);  //800Hzの音を200msec鳴らす
        // delay(200);
        // M5.Speaker.tone( 400, 100, 0, true);
        // delay(200);
        // M5.Speaker.tone(1200, 100, 0, false);
        // delay(200);
        // M5.Speaker.stop();          //音を止める

    }

    if (M5.BtnC.wasPressed()) {
    }

    // テスト：Bボタンで🐢停止
    if (M5.BtnB.wasPressed()) {
        ui.drawStopButton(true);   // 黄色にする
        msg.data = 0;  // 停止！！！
        RCSOFTCHECK(rcl_publish(&publisher, &msg, NULL));
        delay(500);
    }
    // Bボタン離したとき
    if (M5.BtnB.wasReleased()) {
        ui.drawStopButton(false);  // 白に戻す
    }

    module_llm.msg.responseMsgList.clear();


}