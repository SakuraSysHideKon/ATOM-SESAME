//----------------------------------------------------------------------
//  Arukas Amart Keys
//    Atom Liteでセサミスマートロックを操作
//    有限会社さくらシステム 近藤秀尚
//    https://sakura-system.com
//    Ver.1.0 2023.06.05
//----------------------------------------------------------------------

#include <M5Atom.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <Ticker.h>
#include <ArduinoJson.h>
#include <AES_CMAC.h>
#include <AES.h>
#include <arduino_base64.hpp>
#include <driver/adc.h>

//#define DEBUG 1     // デバッグ用

//  LED関連
#define LED_WAIT CRGB::Red              // WiFi準備待ち受け時の点滅色1
#define LED_WAITBLINK CRGB::Black       // WiFi準備待ち受け時の点滅色2
#define LED_PUSH CRGB::Navy             // ボタンプッシュ時の点滅色1
#define LED_PUSHBLINK CRGB::Black       // ボタンプッシュ時の点滅色2
#define LED_WIFIOK CRGB::Navy           // WiFi正常時点灯色
#define LED_WIFING CRGB::Yellow         // WiFi異常時点灯色
#define LED_LOCK CRGB::Maroon           // スマートロック施錠時点灯色
#define LED_UNLOCK CRGB::Green          // スマートロック解錠時点灯色
#define LED_MOVE CRGB::Yellow           // スマートロック移動時(施錠・解錠の途中)点灯色
#define LED_OFF CRGB::Black             // 消灯時の色
#define LED_WAITBLINK_TIME 1000         // WiFi準備待ち受け時の点滅間隔
#define LED_PUSHBLINK_TIME 300          // ボタンプッシュ時の点滅間隔
int LEDBlink = LOW;

// 時間関連
#define UTC 0                           // UTC
#define JST 3600 * 9                    // 日本標準時
const int NTP_INTVAL = 1 * 6000;        // 1分毎にNTPサーバをチェック
// NTP関連
WiFiUDP NTP_udp ;
NTPClient ntp(NTP_udp, "ntp.nict.jp", UTC, NTP_INTVAL );
// NTP結果保持
struct NTPTime {
  time_t utc;
  time_t t;
  struct tu *tu;
  struct tm *tm;
  const char *wd[7] = {"Sun","Mon","Tue","Wed","Thr","Fri","Sat"};
  String strDate;
  String strTime;
  String strWeek;
  String strDateTime;
  int year, mon, mday, hour, min, sec;
};
struct NTPTime ntpTime;

// セサミWeb API関連
// API Keyと履歴表示名の設定 複数台の機器(ATOM Lite)でAPI Keyと履歴表示名を機器毎に設定する。
const int MACs_NUM = 2;                 // 機器(ATOM Lite)の台数
const String MACs[] = {"一台目の標準MACアドレス","二台目の標準MACアドレス"};   // 機器(ATOM Lite)の標準MACアドレス (MACアドレスで機器判定する。)
const String APIKEYs[] = {"一台目のAPIKEY","二台目のAPIKEY"};   // 各機器のAPIKey 
const String HISTORYs[] = {"一台目の履歴表示名","二台目の履歴表示名"};      // 各機器の履歴表示名
int MyMAC;
String MyAPIKEY;    // APIKey
String MyHISTORY;   // 履歴表示名
String SESAME_APIKEY;
String SESAME_HISTORY;
const String SESAME_UUID = "スマートロックのUUID";  // スマートロックのUUID
const char SESAME_SECRETKEY[] = "スマートロックのシークレットキー";   // スマートロックのシークレットキー
const int SESAME_CHECK_INTVAL = 6 * 60000 ;   // セサミスマートロック状態チェックインターバル
const int SESAME_CHECK_OFF_START = 2 ;        // セサミスマートロック状態チェック停止開始時間 2時から5時まで状態チェックは停止
const int SESAME_CHECK_OFF_END = 5 ;          // セサミスマートロック状態チェック提示終了時間
const String SESAME_LOCK = "locked";          // セサミスマートロック状態チェック施錠時のステータス文字列
const String SESAME_UNLOCK = "unlocked";      // セサミスマートロック状態チェック解錠時のステータス文字列
const String SESAME_MOVE = "moved";           // セサミスマートロック状態チェック解錠から施錠へ移動中のステータス文字列
struct SesameAPIOperation {
  int command = 88;           // Web API 操作コマンド
  String history;             // 履歴表示名
  String sign;                // 暗号化済みシークレットキー
  int batteryPercentage;      // バッテリー残量
  double batteryVoltage;      // バッテリー電圧
  int keyPosition;            // キーポジション
  String keyStatus;           // キーの状況
  long timeStamp;             // タイムスタンプ
  bool wm2State;              // ステータス
};
struct SesameAPIOperation sesameAPI;

// AESCMAC暗号化関連
uint8_t AesCmac_mac[16];
uint8_t AesCmac_key[16];
uint8_t AesCmac_mes[3];
AESTiny128 AesCmac_aes128;
AES_CMAC AesCmac_cmac(AesCmac_aes128);

// タイマー関連
Ticker TickerBlink ;
Ticker TickerCheck ;

//----------------------------------------------------------------------
//  NTPから日付時間の取得
//----------------------------------------------------------------------
void getNTPDateTime(){
  char s[30];
  if (WiFi.status() == WL_CONNECTED) { ntp.update(); }    // NTPから時間の取得
  ntpTime.utc = ntp.getEpochTime();                 // UTC 秒数
  ntpTime.t = ntp.getEpochTime() + JST;             // 日本標準時秒数
  // 日本標準時年月日時分秒を取得
  ntpTime.tm = localtime(&ntpTime.t);
  ntpTime.year = ntpTime.tm->tm_year + 1900;
  ntpTime.mon = ntpTime.tm->tm_mon + 1;
  ntpTime.mday = ntpTime.tm->tm_mday;
  ntpTime.hour = ntpTime.tm->tm_hour;
  ntpTime.min = ntpTime.tm->tm_min;
  ntpTime.sec = ntpTime.tm->tm_sec;
  // 表示用日付
  sprintf(s, "%04d.%02d.%02d", ntpTime.year, ntpTime.mon, ntpTime.mday);
  ntpTime.strDate = String(s);
  // 表示用時間
  sprintf(s, "%02d:%02d:%02d", ntpTime.hour, ntpTime.min, ntpTime.sec);
  ntpTime.strTime = String(s);
  // 表示用曜日
  sprintf(s, "%s", ntpTime.wd[ ntpTime.tm->tm_wday]);
  ntpTime.strWeek = String(s);
  // 表示用日時
  ntpTime.strDateTime = ntpTime.strDate + "(" + ntpTime.strWeek + ")" + ntpTime.strTime;
}
//----------------------------------------------------------------------
//  シリアルモニタへの表示
//----------------------------------------------------------------------
void printScreenSl( String argText ){
//  シリアルモニタ表示
///*
  #if defined(DEBUG)
    if (argText == "" || WiFi.status() != WL_CONNECTED ){
      Serial.println(argText);
    } else {
      getNTPDateTime();
      Serial.println(ntpTime.strDateTime + ":" + argText) ;
    }
  #else
    delay(10);
  #endif
//*/
//  Serial.println(argText) ;
  return;
}
void printScreen( String argText ){
//  シリアルモニタ表示
///*
  #if defined(DEBUG)
    Serial.print(argText) ;
  #else
    delay(10);
  #endif
//*/
  return;
}
//----------------------------------------------------------------------
//  LEDコントロール
//----------------------------------------------------------------------
void ledControl( CRGB argColor ) {
  M5.dis.drawpix(0, argColor);
}
//----------------------------------------------------------------------
//  セサミスマートロック状態チェック停止判定
//    true : チェック停止時間内
//    false : チェック停止時間外
//----------------------------------------------------------------------
bool sesameStatusCkStop() {
  bool retSts = false;
  int stH, enH, nowH;
  
  getNTPDateTime();
  stH = SESAME_CHECK_OFF_START;
  enH = SESAME_CHECK_OFF_END ;
  nowH = ntpTime.hour;
  if ( stH > enH ) {  // 指定時間が日をまたいでいる ex.) 22時から3時
    if ( nowH <= enH ) {
      nowH += 24;
    }
    enH += 24;
  }
  if ( nowH >= stH && nowH <= enH ) {  retSts = true; }
  return retSts;
}
//----------------------------------------------------------------------
//  セサミスマートロック状態取得
//----------------------------------------------------------------------
void sesameGetStatus() {

  if ( sesameStatusCkStop() ){
    printScreenSl("SESAME Status Check is Offtime.");
    return;
  } else {
    printScreenSl("SESAME Status Check is Ontime.");
  }
  // Web API セサミスマートロック状態取得
  HTTPClient http;
  http.begin("https://app.candyhouse.co/api/sesame2/" + SESAME_UUID );
  http.addHeader("x-api-key", SESAME_APIKEY);
  http.GET();
  String response = http.getString();
  http.end();
  // JSONから配列へデコード
  printScreenSl("SESAME Json:" + response) ;
  StaticJsonDocument<300> doc;
  DeserializationError error = deserializeJson(doc, response);
  if (error) {
    printScreen("deserializeJson() failed: ");
    printScreenSl(error.c_str());
    return;
  }
  // 状態取得情報の保存
  sesameAPI.batteryPercentage = doc["batteryPercentage"];
  sesameAPI.batteryVoltage = doc["batteryVoltage"];
  sesameAPI.keyPosition = doc["position"];
  String SesameStatus = doc["CHSesame2Status"];
  sesameAPI.keyStatus = SesameStatus;
  sesameAPI.timeStamp = doc["timestamp"];
  sesameAPI.wm2State = doc["wm2State"];
  // スマートロックの状況により、LEDの色を変更する。
  printScreenSl("SESAME Status:" + sesameAPI.keyStatus);
  if ( sesameAPI.keyStatus.equals(SESAME_LOCK)) {
    ledControl(LED_LOCK);
  } else if ( sesameAPI.keyStatus.equals(SESAME_UNLOCK)) {
    ledControl(LED_UNLOCK);
  } else if ( sesameAPI.keyStatus.equals(SESAME_MOVE)) {
    ledControl(LED_MOVE);
  } else {
    ledControl(LED_OFF);
  }
}
//----------------------------------------------------------------------
//  ボタン操作LED点滅
//----------------------------------------------------------------------
void buttonPushLEDBlink() {
  if ( LEDBlink == HIGH ) {
    M5.dis.drawpix(0, LED_PUSHBLINK);
    LEDBlink = LOW ;
  } else {
    M5.dis.drawpix(0, LED_PUSH);
    LEDBlink = HIGH ;
  }
}
//----------------------------------------------------------------------
//  WiFi設定待ち受けLED点滅
//----------------------------------------------------------------------
void wifiWaitLEDBlink() {
  if ( LEDBlink == HIGH ) {
    M5.dis.drawpix(0, LED_WAITBLINK);
    LEDBlink = LOW ;
  } else {
    M5.dis.drawpix(0, LED_WAIT);
    LEDBlink = HIGH ;
  }
}
//----------------------------------------------------------------------
//  WiFi Connect 待ち処理
//----------------------------------------------------------------------
bool WiFiConnectWait(int argRetry=60){
  bool csts = false ;
  for( int i = 0 ; i < argRetry; ++i) {
    if ( WiFi.status() == WL_CONNECTED ) {
      csts = true ;
      break ;
    }
    delay(500);
    printScreen(".");
  }
  printScreenSl("");
  return csts ;
}
//----------------------------------------------------------------------
//  WiFi Smart Connect 接続先検索処理 (スマホアプリ「EspTouch」により設定する。)
//----------------------------------------------------------------------
bool lookingForWiFiWithSmartConnect(){
  printScreenSl("Smart Connect Start.");
  LEDBlink = LOW;
  TickerBlink.attach_ms(LED_WAITBLINK_TIME, wifiWaitLEDBlink );   // LED 点滅
  if ( WiFi.isConnected()) {    // ボタン長押しでWiFi再設定に備えて、接続済みであれば一旦切断する
    WiFi.disconnect();
    delay(100);
    ntp.end();
    delay(100);
  }
  WiFi.mode(WIFI_AP_STA);
  delay(100);
  WiFi.beginSmartConfig();  // Smart Config 開始
  printScreenSl("Waitting Smart Connect.");
  bool ret = WiFiConnectWait(200);
  // 接続できたら接続モードを変更します。
  if ( WiFi.isConnected()) {
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.reconnect();
    WiFiConnectWait();
  }
  printScreenSl("Smart Connect Terminated.");
  TickerBlink.detach();     // LED 点滅停止
  return ret ;
}
//----------------------------------------------------------------------
//  WiFi開始処理
//----------------------------------------------------------------------
void WiFiBegin() {
  printScreenSl("Start setting WiFi connection.") ;
  WiFi.mode(WIFI_STA);
  WiFi.begin() ;  //  WiFi 接続開始
  WiFiConnectWait();
  if (WiFi.status() != WL_CONNECTED) {
    lookingForWiFiWithSmartConnect();   // WiFi 接続できなかった時はSmartConnectにて接続先を設定する。
  }
  if (WiFi.status() == WL_CONNECTED) {
    //  WiFi接続に成功
    //printScreenSl("connect:" + WiFi.SSID() + " " + String(WiFi.psk()) + " " + WiFi.localIP().toString()) ;
    printScreenSl("connect:" + WiFi.SSID() + " " + WiFi.localIP().toString()) ;
    ledControl(LED_WIFIOK);
    ntp.begin();    // NTP 開始
    ntp.update();
  } else {
    //  WiFi接続に失敗
    printScreenSl("Failed to connect") ;
    ledControl(LED_WIFING);
  }
}
//----------------------------------------------------------------------
//  デバッグ用配列の表示
//----------------------------------------------------------------------
void arrayPrint(uint8_t argArry[], int argSize){
  for (int i = 0; i < argSize; ++i) {
    #if defined(DEBUG)
    if (argArry[i] < 0x10) {
      printScreen("0");
    }
    printScreen(String(argArry[i], HEX) + " ");
    #else
    delay(10);
    #endif
  }
}
//----------------------------------------------------------------------
//  AES-CMAC用Keyの作成
//    SESAME Secret KeyをAES-CMAC用にuint8_tの配列にセット
//----------------------------------------------------------------------
void genarateAESCMACKey(){
  for(int i=0,j=0 ; i < 32 ; ++i,++j){
    char str[3] ;
    str[0] = SESAME_SECRETKEY[i];
    ++i;
    str[1] = SESAME_SECRETKEY[i];
    unsigned long keynum = strtoul(str,NULL,16);
    AesCmac_key[j] = keynum;
  }

  printScreen("Key: ");
  arrayPrint(AesCmac_key, sizeof(AesCmac_key) );
  printScreenSl("");
}
//----------------------------------------------------------------------
//  SESAME API 履歴表示名のBase62エンコード
//----------------------------------------------------------------------
void genarateSesameHistory(){
  const char* rawData = SESAME_HISTORY.c_str();
  size_t rawLength = strlen(rawData);
//  char encoded[BASE64::encodeLength(rawLength)];
// BASE64::encode((const uint8_t*)rawData, rawLength, encoded);
  char encoded[base64::encodeLength(rawLength)];
  base64::encode((const uint8_t*)rawData, rawLength, encoded);

  sesameAPI.history = encoded;
  printScreenSl("Histroy:" + sesameAPI.history);
}
//----------------------------------------------------------------------
//  スマートロックの操作 Toggle
//----------------------------------------------------------------------
void keyToggle(){
  printScreenSl("Key Toggle Command 88.");
  //  AES-CMAC暗号化messageの生成
  getNTPDateTime();
  printScreenSl(String(ntpTime.utc));   // 1. timestamp 1684745692 (UTC))
  String d = String(ntpTime.utc,HEX);   // 2. timestamp to Hex String "646b2ddc"
  printScreenSl(d);
  for(int i=0,j=2; i < 6; ++i,--j){     // 3. Little endian to uint8_t[] And remove most-significant byte. uint8_t[] = { 0x2D, 0x6b, 0x64 }
    char str[3] ;
    str[0] = d[i];
    ++i;
    str[1] = d[i];
    unsigned long m = strtoul(str,NULL,16);   // String to unsigned long "2d"->0x2d "6b"->0x6b "64"->0x64
    AesCmac_mes[j] = m;
  }
  printScreen("message: ");
  arrayPrint(AesCmac_mes, sizeof(AesCmac_mes) );
  printScreenSl("");
  //  AES-CMAC暗号化
  AesCmac_cmac.generateMAC(AesCmac_mac, AesCmac_key, AesCmac_mes, sizeof(AesCmac_mes));
  delay(10);
  printScreen("AES-CMAC: ");
  arrayPrint(AesCmac_mac, sizeof(AesCmac_mac) );
  printScreenSl("");
  //  暗号化したデータを文字列に変換
  sesameAPI.sign = "";
  for( int i=0; i < sizeof(AesCmac_mac); ++i) {
    sesameAPI.sign += String(AesCmac_mac[i], HEX);
  }
  //  JSONへの変換
  StaticJsonDocument<300> doc;
  doc["cmd"] = sesameAPI.command ;
  doc["history"] = sesameAPI.history;
  doc["sign"] = sesameAPI.sign;
  String oJson = "";
  serializeJson(doc, oJson);
  printScreenSl(oJson);
  // Web APIへPOST
  HTTPClient http;
  if (!http.begin("https://app.candyhouse.co/api/sesame2/" + SESAME_UUID + "/cmd")) {
    Serial.println("Failed HTTPClient begin!");
    return;
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-api-key", SESAME_APIKEY);
  int responseCode = http.POST(oJson);
  String body = http.getString();
  printScreenSl(String(responseCode));
  printScreenSl(body);
  http.end();
}
//----------------------------------------------------------------------
//  MACアドレスからAPIKeyとHISTORYを求める。
//----------------------------------------------------------------------
void getAPIKey() {
  printScreenSl("getAPIKey");
  uint8_t mac[6];
  //  MACアドレスの取得
  esp_efuse_mac_get_default(mac);
  char mymac[13];
  sprintf(mymac,"%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  String myMAC = mymac;
  printScreenSl("MACアドレス:" + myMAC);
  printScreenSl("MACs Size:" + String(MACs_NUM));
  bool MacFound = false;
  for(int i=0; i < MACs_NUM; ++i){
    if (myMAC.equals(MACs[i])) {
      MacFound = true;
      MyMAC = i;
    }
  }
  if (!MacFound) {MyMAC = 0;}   // 該当のMACアドレスが見つからない場合は、最初に設定されているAPIKeyと履歴表示名を使用する。
  MyAPIKEY = APIKEYs[MyMAC];
  SESAME_APIKEY = MyAPIKEY;
  MyHISTORY = HISTORYs[MyMAC];
  SESAME_HISTORY = MyHISTORY.c_str();
  printScreenSl("MACs Index:" + String(MyMAC));
  printScreenSl("APIKEY:" + SESAME_APIKEY);
  printScreenSl("HISTORY:" + SESAME_HISTORY);
}
//----------------------------------------------------------------------
//  開始処理
//----------------------------------------------------------------------
void mySetup() {
  WiFiBegin();
  getAPIKey();
  genarateAESCMACKey();
  genarateSesameHistory();
}
//----------------------------------------------------------------------
//  初期設定処理
//----------------------------------------------------------------------
void setup() {
  //----おまじない　ATOM Liteの不具合(ノイズによるボタン連打)対策
  pinMode(0, OUTPUT);
  digitalWrite(0, LOW);
  adc_power_acquire();
  //----おまじない ここまで
  // 本体初期化（UART, I2C, LED）
  M5.begin(true, false, true);
  // LED全消灯（赤, 緑, 青）
  M5.dis.drawpix(0, CRGB::Black);
  //
  Serial.begin(115200);
  mySetup();
  printScreenSl("Start");
  sesameGetStatus();
  TickerCheck.attach_ms(SESAME_CHECK_INTVAL, sesameGetStatus );   // スマートロックの状態を指定時間毎にチェックする
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFiBegin();
  } else {
    ntp.update();
    M5.update();
    if ( M5.Btn.pressedFor(5000)){    // ボタン長押し (WiFiの再設定)
      lookingForWiFiWithSmartConnect();  
    }
    if ( M5.Btn.wasPressed()){    // ボタンが押された (スマートロックの操作)
      LEDBlink = LOW;
      TickerBlink.attach_ms(LED_PUSHBLINK_TIME, buttonPushLEDBlink );   // LED 点滅開始
      keyToggle();              // Web APIにてスマートロック操作
      delay(1500);
      TickerBlink.detach();     // LED 点滅停止
      sesameGetStatus();        // Web APIにてスマートロックの状態取得
    }
  }
  delay(100);
}
