#include <Arduino.h>
#include <TFT_eSPI.h>

// 初始化TFT屏幕
TFT_eSPI tft;

void setup() {
  // 初始化串口，用于调试
  Serial.begin(115200);
  
  // 初始化TFT屏幕
  tft.begin();
  tft.setRotation(3);  // 横屏显示
  
  // 清屏，设置为黑色背景
  tft.fillScreen(TFT_BLACK);
  
  // 设置文本颜色为白色，背景为黑色
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  
  // 设置文本大小
  tft.setTextSize(2);
  
  // 设置文本对齐方式为居中
  tft.setTextDatum(MC_DATUM);
  
  // 在屏幕中心显示"Hello World"
  tft.drawString("Hello World", tft.width()/2, tft.height()/2);
  
  // 在屏幕底部显示欢迎消息
  tft.setTextSize(1);
  tft.setTextDatum(BC_DATUM);
  tft.drawString("Welcome to Wio Terminal", tft.width()/2, tft.height() - 10);
  
  Serial.println("Hello World displayed on the screen");
}

void loop() {
  // 主循环为空，因为我们只需要在启动时显示一次文本
  // 您可以在这里添加其他功能
}