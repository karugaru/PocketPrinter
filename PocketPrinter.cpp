#include "PocketPrinter.h"

//コンストラクタ
PocketPrinter::PocketPrinter(int in, int out, int clock) {
  POCKETPRINTER_DEBUG = 0;
  printerClock = clock;
  printerIn = in;
  printerOut = out;
  pinMode(printerClock, OUTPUT);
  pinMode(printerOut, OUTPUT);
  pinMode(printerIn, INPUT);
}

//1バイト送信
uint8_t PocketPrinter::SerialOut(uint8_t data) {
  uint8_t reply = 0;
  for (int i = 0; i < 8; i++) {
    //クロックは立ち上がりエッジっぽい
    digitalWrite(printerClock, 0);
    digitalWrite(printerOut, bitRead(data, 7 - i));
    delayMicroseconds(10);
    digitalWrite(printerClock, 1);
    bitWrite(reply, 7 - i, digitalRead(printerIn));
    delayMicroseconds(clockDelayMicroseconds);
  }
  //仕様書によると270らしいが、コレでも動く
  delayMicroseconds(20);
  return reply;
}

//子機の識別チェック 0x81か0x80でOK
//OKならtrueを返す
bool PocketPrinter::CheckAcknowledgement(uint8_t value) {
  if(POCKETPRINTER_DEBUG){
    if (value != 0x81 && value != 0x80) {
      Serial.println("CheckAcknowledgement : Error");
    }
  }
  return value == 0x81 || value == 0x80;
}

//致命的エラーチェック
//OKならtrueを返す
bool PocketPrinter::CheckFatalError(uint8_t value) {
if(POCKETPRINTER_DEBUG){
  if (value & POCKETPRINTER_ERR_LOWBAT) {
    Serial.println("CheckFatal : ERR_LOWBAT");
  }
  if (value & POCKETPRINTER_ERR_TEMPBAD) {
    Serial.println("CheckFatal : ERR_TEMPBAD");
  }
  if (value & POCKETPRINTER_ERR_JAM) {
    Serial.println("CheckFatal : ERR_JAM");
  }
  if (value & POCKETPRINTER_ERR_PACKET) {
    Serial.println("CheckFatal : ERR_PACKET");
  }
  if (value & POCKETPRINTER_ERR_CHECKSUM) {
    Serial.println("CheckFatal : ERR_CHECKSUM");
  }
  if (value & POCKETPRINTER_STATUS_UNPROCESSED) {
    Serial.println("CheckFatal : STATUS_UNPROCESSED");
  }
  if (value & POCKETPRINTER_STATUS_DATAFULL) {
    Serial.println("CheckFatal : STATUS_DATAFULL");
  }
  if (value & POCKETPRINTER_STATUS_BUSY) {
    Serial.println("CheckFatal : STATUS_BUSY");
  }
}
  return (value &
          (POCKETPRINTER_ERR_LOWBAT |
          POCKETPRINTER_ERR_TEMPBAD |
          POCKETPRINTER_ERR_JAM |
          POCKETPRINTER_ERR_PACKET |
          POCKETPRINTER_ERR_CHECKSUM)) == 0;
}

//コネクション初期化
bool PocketPrinter::SendInit() {
  //PA : Header : Data : CheckSum : Dummy(status check)
  //88 33 : 01 00 00 00 : : 01 00 : 00 00
  SerialOut(0x88);
  SerialOut(0x33);
  SerialOut(0x01);
  SerialOut(0x00);
  SerialOut(0x00);
  SerialOut(0x00);
  SerialOut(0x01);
  SerialOut(0x00);
  uint8_t reply1 = SerialOut(0x00);
  uint8_t reply2 = SerialOut(0x00);

if(POCKETPRINTER_DEBUG){
  if (CheckAcknowledgement(reply1) && reply2 == 0x00) {
    Serial.println("Init : Connected");
  } else if (reply1 == 0xFF && reply2 == 0xFF) {
    Serial.println("Init : Not Connected");
  } else {
    Serial.println("Init : Other Device");
  }
}

  if ((reply1 == 0x81 || reply1 == 0x80) && reply2 == 0x00) {
    return true;
  } else {
    return false;
  }

}

//印刷開始
//beforeFeeds,afterFeedsは0x00-0x0Fまで
//densityは0x00-0x7Fまで
//densityの標準は0x40
bool PocketPrinter::SendPrint(uint8_t beforeFeeds, uint8_t afterFeeds, uint8_t density) {
  beforeFeeds = min(max(beforeFeeds, 0x00), 0x0F);
  afterFeeds = min(max(afterFeeds, 0x00), 0x0F);
  density = min(max(density, 0x00), 0x7F);

  uint16_t checkSum = 0;
  //PA : Header : Data : CheckSum : Dummy(status check)
  //88 33 : 02 00 04 00 : NumberOfSheet Feeds Palette Density : CheckSum1 CheckSum2 : 00 00
  SerialOut(0x88);
  SerialOut(0x33);

  SerialOut(0x02);
  checkSum += 0x02;
  SerialOut(0x00);
  SerialOut(0x04);
  checkSum += 0x04;
  SerialOut(0x00);

  //NumberOfSheetは1で固定でもOKっぽい
  SerialOut(0x01);
  checkSum += 0x01;
  SerialOut((beforeFeeds << 4) | afterFeeds);
  checkSum += (beforeFeeds << 4) | afterFeeds;
  //Paletteは黒、濃い灰色、薄い灰色、白を、どのビットパターンに割り当てるか
  //11 10 01 00 = 0xE4を当てている
  SerialOut(0xE4);
  checkSum += 0xE4;
  SerialOut(density);
  checkSum += density;

  //下位8ビット
  SerialOut(lowByte(checkSum));
  //上位8ビット
  SerialOut(highByte(checkSum));

  uint8_t reply1 = SerialOut(0x00);
  uint8_t reply2 = SerialOut(0x00);

  if (!CheckAcknowledgement(reply1)) {
    return false;
  }
  if (!CheckFatalError(reply2)) {
    return false;
  }

if(POCKETPRINTER_DEBUG){
  if (reply2 & POCKETPRINTER_STATUS_BUSY) {
    Serial.println("Print : Printer is busy");
  }
}

  if (reply2 & POCKETPRINTER_STATUS_BUSY) {
    return false;
  }
  return true;
}

//印刷開始を待つ
void PocketPrinter::WaitPrintBegin() {
  while (true) {
    uint16_t reply = SendInquiry();
    if (!CheckAcknowledgement(highByte(reply))) {
      break;
    }
    if (!CheckFatalError(lowByte(reply))) {
      break;
    }
    if (lowByte(reply) & POCKETPRINTER_STATUS_BUSY) {
      break;
    }
    delay(100);
  }
if(POCKETPRINTER_DEBUG){
  Serial.println("WaitPrintBegin : Print begun");
}
}

//印刷終了を待つ
void PocketPrinter::WaitPrintEnd() {
  while (true) {
    uint16_t reply = SendInquiry();
    if (!CheckAcknowledgement(highByte(reply))) {
      break;
    }
    if (!CheckFatalError(lowByte(reply))) {
      break;
    }
    if (!(lowByte(reply) & POCKETPRINTER_STATUS_BUSY)) {
      break;
    }
    delay(100);
  }
if(POCKETPRINTER_DEBUG){
  Serial.println("WaitPrintEnd : Print ended");
}
}

//印刷が開始して終了するのを待つ
void PocketPrinter::WaitPrinting() {
  WaitPrintBegin();
  WaitPrintEnd();
}

//印刷データ送信
//データフォーマットに関してはかなり複雑
//2バイトが最小構成単位、2バイト(B0B1)で
//P0P1P2P3P4P5P6P7 (Pnは1ピクセル)を構成する
//1ピクセルは4階調(2ビット)で表現される。
//B0 abcdefgh B1 ijklnmop
//とするとP0-P7は
//P0 ia : P1 jb : P2 kc : P3 ld : P4 ne : P5 mf : P6 og : P7 ph
//と構成される。この構成単位をチップと(勝手に)呼ぶ
//8チップで8x8ピクセルのタイルを以下のように構成する
//P0P1...P7
//P8P9...P15
//.
//.
//P56P57...P63
//40タイルで160x16ピクセルのバンドを以下のように構成する
//T21T22.....T39
//T0T2T3T4...T19
//ポケットプリンタの横解像度は160ピクセルなので
//9バンドでGBの1画面を構成できる。
//9バンドをプリンタに送ったら、一度印刷を実行しないとプリンタに側のメモリがいっぱいになってしまうので注意
//ただしSendPrintの印刷前と後のフィードを0にすることで紙が続く限り無限に長く印刷可能ではある。
//
//lengthは最大640まで。これを超える場合は複数回SendDataを実行する。
//640未満の場合、空白でバンドを埋める。
bool PocketPrinter::SendData(uint8_t* data, uint16_t length) {
  if (length > 640) {
    return false;
  }
  uint16_t checkSum = 0;

  SerialOut(0x88);
  SerialOut(0x33);

  SerialOut(0x04);
  checkSum += 0x04;
  //圧縮は常に無し
  SerialOut(0x00);
  SerialOut(lowByte(length));
  checkSum += lowByte(length);
  SerialOut(highByte(length));
  checkSum += highByte(length);

  for (uint16_t i = 0; i < length; i++) {
    SerialOut(data[i]);
    checkSum += data[i];
  }

  SerialOut(lowByte(checkSum));
  SerialOut(highByte(checkSum));

  uint8_t reply1 = SerialOut(0x00);
  uint8_t reply2 = SerialOut(0x00);

  if (!CheckAcknowledgement(reply1)) {
    return false;
  }
  if (!CheckFatalError(reply2)) {
    return false;
  }
  return true;
}

//データ終了通知
bool PocketPrinter::SendDataEnd() {
  SerialOut(0x88);
  SerialOut(0x33);

  SerialOut(0x04);
  SerialOut(0x00);
  SerialOut(0x00);
  SerialOut(0x00);

  SerialOut(0x04);
  SerialOut(0x00);

  uint8_t reply1 = SerialOut(0x00);
  uint8_t reply2 = SerialOut(0x00);

  if (!CheckAcknowledgement(reply1)) {
    return false;
  }
  if (!CheckFatalError(reply2)) {
    return false;
  }
  return true;
}

//印刷中断送信
bool PocketPrinter::SendBreak() {
  uint16_t reply = SendInquiry();
  if (!(lowByte(reply) & POCKETPRINTER_STATUS_BUSY)) {
    return false;
  }

  SerialOut(0x88);
  SerialOut(0x33);
  SerialOut(0x08);
  SerialOut(0x00);
  SerialOut(0x00);
  SerialOut(0x00);
  SerialOut(0x08);
  SerialOut(0x00);
  uint8_t reply1 = SerialOut(0x00);
  uint8_t reply2 = SerialOut(0x00);

  if (!CheckAcknowledgement(reply1)) {
    return false;
  }
  if (!CheckFatalError(reply2)) {
    return false;
  }
  return true;
}

//ヌルパケット、問い合わせパケット送信
//返り値の
//上位8ビットはCheckAcknowledgement
//下位8ビットはCheckFatalErrorでチェックできる
uint16_t PocketPrinter::SendInquiry() {
  SerialOut(0x88);
  SerialOut(0x33);
  SerialOut(0x0F);
  SerialOut(0x00);
  SerialOut(0x00);
  SerialOut(0x00);
  SerialOut(0x0F);
  SerialOut(0x00);
  uint16_t reply1 = SerialOut(0x00);
  uint16_t reply2 = SerialOut(0x00);
  return (reply1 << 8) + reply2;
}