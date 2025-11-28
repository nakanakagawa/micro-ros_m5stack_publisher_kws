
// キーワードを認識したら，micro-rosでトピックを送信するコード．（製作中）

#include <Arduino.h>
#include <M5Unified.h>
#include <M5ModuleLLM.h>
#include <vector>
#include "Logger.h"  // ヘッダファイル読み込み


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
    { " back",  "stop and back!!","back",         10  },
    { " slow",  "SLOW !!",        "slow.slow",    1   },
    { " dance", "DANCING",        "dancing",      6   }
};
const int NUM_COMMANDS = sizeof(command_table) / sizeof(command_table[0]);
// ================================


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
    module_llm.melotts.inference(melotts_work_id, "OK!", 5000);
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

                // if (asr_result == " echo"){ 
                //     addLog("ECHO", TFT_GREENYELLOW);
                //     module_llm.melotts.inference(melotts_work_id, "ECHO.ECHO",2000);
                    
                //     // グローバルのmsgを使用
                //     msg.data = 10;
                //     RCSOFTCHECK(rcl_publish(&publisher, &msg, NULL));
                //     addLog("Topic sent: 10", TFT_CYAN);
                // }

                // if (asr_result == " yes"){ 
                //     addLog("yes");
                //     module_llm.melotts.inference(melotts_work_id, "yeah. very good. ",2000);
                    
                //     msg.data = 20;  // 値を変えて区別できるようにする
                //     RCSOFTCHECK(rcl_publish(&publisher, &msg, NULL));
                //     addLog("Topic sent: 20", TFT_CYAN);
                //     delay(500);
                // }

for (int i = 0; i < NUM_COMMANDS; i++) {
    if (asr_result == command_table[i].name) {

        addLog(command_table[i].log_text);

        module_llm.melotts.inference(
            melotts_work_id,
            command_table[i].tts_file,
            2000
        );

        msg.data = command_table[i].value;
        RCSOFTCHECK(rcl_publish(&publisher, &msg, NULL));

        addLog(String("Topic sent: ") + msg.data, TFT_CYAN);
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
        addLog("Log %d", n++);
        msg.data = 0;  // 停止！！！
        RCSOFTCHECK(rcl_publish(&publisher, &msg, NULL));
        addLog("Topic sent: 0", TFT_CYAN);
        delay(500);
    }
    // RCSOFTCHECK(rcl_publish(&publisher, &msg, NULL));
    module_llm.msg.responseMsgList.clear();
}