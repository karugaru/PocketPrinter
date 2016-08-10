#ifndef PocketPrinter_h
#define PocketPrinter_h

#include <avr/pgmspace.h>
#include "Arduino.h"
#define POCKETPRINTER_ERR_LOWBAT 0x80
#define POCKETPRINTER_ERR_TEMPBAD 0x40
#define POCKETPRINTER_ERR_JAM 0x20
#define POCKETPRINTER_ERR_PACKET 0x10
#define POCKETPRINTER_STATUS_UNPROCESSED 0x08
#define POCKETPRINTER_STATUS_DATAFULL 0x04
#define POCKETPRINTER_STATUS_BUSY 0x02
#define POCKETPRINTER_ERR_CHECKSUM 0x01

class PocketPrinter {
  private:
    static const int clockDelayMicroseconds = 20;
    uint8_t printerClock;
    //プリンタ側のoutをつなぐ。15kΩで5vにプルアップ
    uint8_t printerIn;
    //プリンタ側のinをつなぐ
    uint8_t printerOut;

  public:
	int POCKETPRINTER_DEBUG;
    //コンストラクタ
    PocketPrinter(int in, int out, int clock);
    //1バイト送信
    uint8_t SerialOut(uint8_t command);
    //子機の識別チェック 0x81か0x80でOK
    bool CheckAcknowledgement(uint8_t value);
    //致命的エラーチェック
    bool CheckFatalError(uint8_t value);
    //コネクション初期化
    bool SendInit();
    //印刷開始
    bool SendPrint(uint8_t beforeFeeds, uint8_t afterFeeds, uint8_t density);
    //印刷開始を待つ
    void WaitPrintBegin();
    //印刷終了を待つ
    void WaitPrintEnd();
    //印刷が開始して終了するのを待つ
    void WaitPrinting();
    //印刷データ送信
    bool SendData(uint8_t* data, uint16_t length);
    //データ終了通知
    bool SendDataEnd();
    //印刷中断送信
    bool SendBreak();
    //ヌルパケット、問い合わせパケット送信
    uint16_t SendInquiry();

};
#endif