/**
 * @file Tetris_Game.ino
 * @brief 基于WIO Terminal的俄罗斯方块游戏
 * @author YourName
 * @date 2023-08-20
 * @version 2.2
 * @note 硬件配置：
 *       - 主控：ATSAMD51 @ 120MHz
 *       - 显示屏：2.4寸TFT (320x240)
 *       - 输入设备：五向摇杆+功能按键
 * @copyright Copyright (c) 2023
 */

#include <TFT_eSPI.h>  // 包含TFT库
TFT_eSPI tft;          // 实例化TFT对象

// ----------------- 硬件引脚定义 -----------------
#define BUTTON_A WIO_KEY_A       ///< A键：保留功能（内部上拉）
#define BUTTON_B WIO_KEY_B       ///< B键：游戏重置（低电平有效）
#define BUTTON_C WIO_KEY_C       ///< C键：保留功能
#define DPAD_UP WIO_5S_UP        ///< 上：旋转方块（MOSI复用）
#define DPAD_DOWN WIO_5S_DOWN    ///< 下：加速下落（MISO复用）
#define DPAD_LEFT WIO_5S_LEFT    ///< 左：左移方块（SCK复用）
#define DPAD_RIGHT WIO_5S_RIGHT  ///< 右：右移方块
#define DPAD_PRESS WIO_5S_PRESS  ///< 按压：保留功能（CS复用）
#define NEXT_BLOCK_X 160  ///< 下一个方块预览区左上角X坐标
#define NEXT_BLOCK_Y 30   ///< 下一个方块预览区左上角Y坐标

// ----------------- 游戏核心参数 -----------------
#define GRID_WIDTH 10     ///< 可见区域水平格子数
#define GRID_HEIGHT 22    ///< 总格子数（含2行顶部缓冲）
#define BLOCK_SIZE 11     ///< 单个方块像素尺寸（含1像素边框）
#define BOARD_X 0         ///< 游戏区左上角X坐标
#define BOARD_Y 5         ///< 游戏区Y坐标（留出顶部状态栏）

// ----------------- UI显示参数 -----------------
#define SCORE_FONT_SIZE 2       ///< 计分板字体大小
#define GAMEOVER_TITLE_FONT 1   ///< 游戏结束标题字体（8x16像素）
#define GAMEOVER_SCORE_FONT 1   ///< 得分显示字体（8x8像素）
#define GAMEOVER_HINT_FONT 1    ///< 提示信息字体（6x8像素）

// 颜色定义（RGB565格式）
#define GAMEOVER_BG_COLOR TFT_NAVY  ///< 弹窗背景：0x000A（R:G:B=0:0:10）
#define TEXT_SHADOW_COLOR 0x18A5    ///< 文字阴影：0x18A5（R:G:B=24:36:5）
#define TFT_GOLD      0xFEA0        ///< 金色：0xFEA0（R:G:B=31:29:8）
#define TEXT_SHADOW   0x3186        ///< 通用阴影颜色

// ----------------- 方块数据结构 -----------------
/**
 * @struct GameState
 * @brief 存储游戏运行时所有状态数据
 * @warning grid[0][]为顶部缓冲行，不可见
 */
struct GameState {
  uint8_t grid[GRID_HEIGHT][GRID_WIDTH]; ///< 游戏网格（0:空，1-7:方块类型）
  int currentPiece;      ///< 当前方块类型索引（0-6）
  int rotation;          ///< 当前旋转状态（0-3）
  int pieceX;            ///< 方块X坐标（0-9，对应网格列）
  int pieceY;            ///< 方块Y坐标（可负数，缓冲行开始）
  int nextPiece;         ///< 下一个方块类型索引
  bool gameOver;         ///< 游戏结束标志（true时锁定输入）
  int score;             ///< 当前得分（100/行）
  unsigned long lastFall; ///< 上次自动下落时间戳（毫秒）
};

// ----------------- 全局变量 -----------------
GameState game;  ///< 游戏状态实例

// 方块颜色配置（RGB565，索引0-6对应7种方块）
const uint16_t COLORS[7] = {
  TFT_CYAN,   // I型（青色）
  TFT_YELLOW, // O型（黄色）
  TFT_MAGENTA,// T型（品红）
  TFT_GREEN,  // S型（绿色）
  TFT_RED,    // Z型（红色）
  TFT_ORANGE, // L型（橙色）
  TFT_BLUE    // J型（蓝色）
};

// 俄罗斯方块形状定义（16位掩码，4x4矩阵）
const uint16_t TETROMINOES[7][4] = {
  // I型（4种旋转状态）
  { 0x0F00, // 0: 横向排列 ████
    0x2222, // 1: 纵向排列 █
             //          █
             //          █
             //          █
    0x00F0, // 2: 横向排列（偏移）
    0x4444  // 3: 纵向排列（偏移）
  },
  // O型（单形态）
  { 0x0660, // ██
            // ██
    0x0660, 0x0660, 0x0660 },
  // T型（4方向）
  { 0x0470, 0x2320, 0x0740, 0x0262 },
  // S型（双形态）
  { 0x0360, 0x4620, 0x0360, 0x4620 },
  // Z型（双形态）
  { 0x0630, 0x2640, 0x0630, 0x2640 },
  // L型（4方向）
  { 0x0720, 0x6220, 0x0270, 0x2230 },
  // J型（4方向）
  { 0x0740, 0x2260, 0x0170, 0x6220 }
};

// ----------------- 函数声明 -----------------
void newGame();         ///< 初始化新游戏
void drawBoard();       ///< 绘制游戏面板（双缓冲）
void drawBlock(int x, int y, uint16_t color); ///< 绘制单个方块
void drawNextPiece();   ///< 更新下一个方块预览
bool movePiece(int dx, int dy = 0); ///< 移动方块（返回是否成功）
void rotatePiece();     ///< 旋转方块（含墙面踢出补偿）
void lockPiece();       ///< 固定当前方块到网格
void checkLines();      ///< 消除满行并更新分数
void drawGameOver();    ///< 显示游戏结束界面
void spawnPiece();      ///< 生成新方块（含游戏结束检测）
void drawCurrentPiece();///< 绘制当前下落方块
bool collisionCheck(int x, int y, int rotation); ///< 碰撞检测
void updateScoreDisplay(); ///< 更新计分板显示

// ----------------- 硬件初始化 -----------------
void setup() {
  // 初始化输入引脚（启用内部上拉电阻）
  const uint8_t buttons[] = {BUTTON_A, BUTTON_B, BUTTON_C, 
                            DPAD_UP, DPAD_DOWN, DPAD_LEFT, 
                            DPAD_RIGHT, DPAD_PRESS};
  for (auto pin : buttons) {
    pinMode(pin, INPUT_PULLUP);  // 配置为输入模式，启用内部上拉
  }

  // 显示初始化
  tft.begin();          // 初始化TFT驱动
  tft.setRotation(3);   // 设置屏幕方向（0-3，3为USB口朝下）
  tft.fillScreen(TFT_BLACK); // 清屏
  randomSeed(analogRead(0)); // 初始化随机种子（使用悬空引脚）
  newGame();            // 初始化游戏状态
}

// ----------------- 主循环 -----------------
void loop() {
  static unsigned long lastUpdate = millis();  // 自动下落计时器
  static unsigned long lastRotateTime = 0;     // 旋转防抖计时器
  const int debounceDelay = 200;              // 防抖时间（毫秒）

  // 方向控制检测（非阻塞）
  if (!game.gameOver) {
    if (digitalRead(DPAD_LEFT) == LOW)  movePiece(-1);  // 左移
    if (digitalRead(DPAD_RIGHT) == LOW) movePiece(1);   // 右移
    if (digitalRead(DPAD_DOWN) == LOW)  movePiece(0, 1);// 加速下落
  }

  // 游戏重置检测（B键按下）
  if (digitalRead(BUTTON_B) == LOW) newGame();

  // 旋转控制（带防抖）
  if (digitalRead(DPAD_PRESS) == LOW && 
     (millis() - lastRotateTime) > debounceDelay &&
     !game.gameOver) {
    rotatePiece();             // 执行旋转
    lastRotateTime = millis(); // 更新防抖计时器
  }

  // 自动下落逻辑（500ms间隔）
  if (!game.gameOver && millis() - lastUpdate > 500) {
    if (!movePiece(0, 1)) {   // 尝试向下移动
      lockPiece();            // 固定方块
      checkLines();           // 消除检测
      spawnPiece();           // 生成新方块
    }
    lastUpdate = millis();    // 重置计时器
  }
  delay(50);  // 主循环节流（约20FPS）
}

// ----------------- 游戏逻辑函数 -----------------
/**
 * @brief 初始化新游戏状态
 * @details 重置所有参数并绘制初始界面
 */
void newGame() {
  tft.fillScreen(TFT_BLACK);  // 清空屏幕
  
  // 绘制游戏区边框（1像素白边）
  tft.drawFastHLine(0, 0, tft.width(), TFT_WHITE);         // 顶部边框
  tft.drawFastHLine(0, tft.height()-1, tft.width(), TFT_WHITE); // 底部边框
  tft.drawFastVLine(0, 0, tft.height(), TFT_WHITE);       // 左侧边框
  tft.drawFastVLine(tft.width()-1, 0, tft.height(), TFT_WHITE); // 右侧边框

  // 初始化游戏状态
  memset(game.grid, 0, sizeof(game.grid));  // 清空网格
  game.currentPiece = random(7);    // 随机初始方块
  game.nextPiece = random(7);       // 随机下一个方块
  game.rotation = 0;                // 初始旋转状态
  game.pieceX = 3;                  // 居中位置（10列中第4列）
  game.pieceY = 0;                  // 从缓冲行开始
  game.score = 0;                   // 初始得分
  game.gameOver = false;            // 重置结束标志
  
  // 初始渲染
  drawBoard();          // 绘制空面板
  drawNextPiece();      // 绘制下一个方块预览
  updateScoreDisplay(); // 更新计分板
}

/**
 * @brief 移动当前方块
 * @param dx X轴移动方向（-1左移，0不动，+1右移）
 * @param dy Y轴移动方向（0不动，+1下移）
 * @return 是否移动成功（失败表示碰撞）
 */
bool movePiece(int dx, int dy) {
  int newX = game.pieceX + dx;  // 计算新X坐标
  int newY = game.pieceY + dy;  // 计算新Y坐标
  
  // 碰撞检测（新位置是否合法）
  if (!collisionCheck(newX, newY, game.rotation)) {
    game.pieceX = newX;  // 更新X坐标
    game.pieceY = newY;  // 更新Y坐标
    drawBoard();         // 重绘面板（擦除旧位置）
    drawCurrentPiece();  // 绘制新位置
    return true;         // 移动成功
  }
  return false;  // 碰撞发生，移动失败
}

/**
 * @brief 旋转当前方块
 * @details 实现墙面踢出（Wall Kick）机制：
 *          当旋转导致碰撞时，尝试左右偏移1-2格
 */
void rotatePiece() {
  if (game.gameOver) return;  // 游戏结束锁定输入
  
  int newRotation = (game.rotation + 1) % 4;  // 计算新旋转状态
  
  // 尝试直接旋转
  if (!collisionCheck(game.pieceX, game.pieceY, newRotation)) {
    game.rotation = newRotation;
  } else {
    // 墙面踢出补偿（左右各尝试2格）
    for (int offset = 1; offset <= 2; offset++) {
      // 尝试右偏移
      if (!collisionCheck(game.pieceX + offset, game.pieceY, newRotation)) {
        game.pieceX += offset;
        game.rotation = newRotation;
        break;
      }
      // 尝试左偏移
      if (!collisionCheck(game.pieceX - offset, game.pieceY, newRotation)) {
        game.pieceX -= offset;
        game.rotation = newRotation;
        break;
      }
    }
  }
  
  // 更新显示
  drawBoard();
  drawCurrentPiece();
}

/**
 * @brief 绘制游戏面板（双缓冲实现）
 * @details 1. 先绘制深灰色背景 
 *          2. 绘制所有已固定方块
 */
void drawBoard() {
  // 绘制背景（GRID_HEIGHT-1跳过最后一行缓冲）
  tft.fillRect(BOARD_X, BOARD_Y, 
              GRID_WIDTH*BLOCK_SIZE, 
              (GRID_HEIGHT-1)*BLOCK_SIZE,
              TFT_DARKGREY);

  // 遍历可见区域（y从1开始，跳过顶部缓冲）
  for (int y = 1; y < GRID_HEIGHT; y++) {
    for (int x = 0; x < GRID_WIDTH; x++) {
      if (game.grid[y][x]) {  // 存在方块
        // 转换为可见坐标（y-1）
        drawBlock(x, y-1, COLORS[game.grid[y][x]-1]);
      }
    }
  }
}

/**
 * @brief 绘制单个方块到指定网格位置
 * @param x 网格X坐标（0-9）
 * @param y 网格Y坐标（0-20，可见区域）
 * @param color 方块颜色（RGB565）
 */
void drawBlock(int x, int y, uint16_t color) {
  // 计算实际像素坐标（考虑边框）
  int px = BOARD_X + x * BLOCK_SIZE;
  int py = BOARD_Y + y * BLOCK_SIZE;
  // 绘制方块（尺寸-2实现1像素边框）
  tft.fillRect(px+1, py+1, BLOCK_SIZE-2, BLOCK_SIZE-2, color);
}

/**
 * @brief 绘制当前下落的方块
 * @details 解析4x4位掩码，绘制有效方块
 */
void drawCurrentPiece() {
  // 获取当前形状的位掩码
  uint16_t shape = TETROMINOES[game.currentPiece][game.rotation];
  
  // 遍历4x4矩阵（从高位开始）
  for (int i = 0; i < 16; i++) {
    if (shape & 0x8000) {  // 当前位有效
      // 计算相对位置
      int x = game.pieceX + (i % 4);
      int y = game.pieceY + (i / 4);
      
      // 仅在可见区域绘制（y>=1）
      if (y >= 1) {
        drawBlock(x, y-1, COLORS[game.currentPiece]);
      }
    }
    shape <<= 1;  // 左移检查下一位
  }
}

/**
 * @brief 碰撞检测
 * @param x 方块中心X坐标
 * @param y 方块中心Y坐标
 * @param rotation 旋转状态
 * @return true 发生碰撞（边界/已有方块）
 */
bool collisionCheck(int x, int y, int rotation) {
  uint16_t shape = TETROMINOES[game.currentPiece][rotation];
  for (int i = 0; i < 16; i++) {
    if (shape & 0x8000) {  // 当前位有效
      // 计算实际网格坐标
      int gridX = x + (i % 4);
      int gridY = y + (i / 4);
      
      // 左/右边界检测
      if (gridX < 0 || gridX >= GRID_WIDTH) return true;
      
      // 底部边界检测
      if (gridY >= GRID_HEIGHT) return true;
      
      // 方块重叠检测（跳过顶部缓冲行）
      if (gridY >= 1 && game.grid[gridY][gridX]) return true;
    }
    shape <<= 1;  // 检查下一位
  }
  return false;
}

/**
 * @brief 固定当前方块到网格
 * @details 将当前方块位置写入网格数组
 */
void lockPiece() {
  uint16_t shape = TETROMINOES[game.currentPiece][game.rotation];
  for (int i = 0; i < 16; i++) {
    if (shape & 0x8000) {  // 当前位有效
      // 计算网格坐标
      int x = game.pieceX + (i % 4);
      int y = game.pieceY + (i / 4);
      
      // 仅在可见区域存储（y>=1）
      if (y >= 1) {
        // 存储方块类型+1（0表示空）
        game.grid[y][x] = game.currentPiece + 1;
      }
    }
    shape <<= 1;
  }
}

/**
 * @brief 消除满行并更新分数
 * @details 采用经典下落算法：
 *          1. 从下往上检测满行
 *          2. 消除后上方方块整体下移
 *          3. 支持连击得分
 */
void checkLines() {
  for (int y = GRID_HEIGHT-1; y >= 1; y--) {  // 从底部开始
    bool full = true;
    
    // 检测整行是否填满
    for (int x = 0; x < GRID_WIDTH; x++) {
      if (!game.grid[y][x]) {
        full = false;
        break;
      }
    }
    
    if (full) {
      game.score += 100;  // 增加得分
      updateScoreDisplay();
      
      // 上方所有行下移
      for (int yy = y; yy > 1; yy--) {
        memcpy(game.grid[yy], game.grid[yy-1], GRID_WIDTH);
      }
      
      // 清空最顶行（保留缓冲行）
      memset(game.grid[1], 0, GRID_WIDTH);
      
      // 重绘面板
      drawBoard();
      
      y++;  // 重新检测当前行（因内容变化）
    }
  }
}

/**
 * @brief 生成新方块
 * @details 检测新方块能否生成，否则触发游戏结束
 */
void spawnPiece() {
  game.currentPiece = game.nextPiece;  // 使用预览方块
  game.nextPiece = random(7);         // 生成新预览
  game.pieceX = 3;                    // 重置位置
  game.pieceY = 0;
  game.rotation = 0;
  
  // 初始位置碰撞检测
  if (collisionCheck(game.pieceX, game.pieceY, 0)) {
    game.gameOver = true;  // 触发游戏结束
    drawGameOver();        // 显示结束界面
    return;
  }
  
  drawNextPiece();  // 更新预览显示
}

/**
 * @brief 绘制下一个方块预览
 * @details 在右侧80x80区域居中显示
 */
void drawNextPiece() {
  tft.fillRect(NEXT_BLOCK_X, NEXT_BLOCK_Y, 80, 80, TFT_BLACK);
  tft.drawRect(NEXT_BLOCK_X, NEXT_BLOCK_Y, 80, 80, TFT_WHITE);
  
  // 计算居中位置（4个方块总宽44px）
  int startX = NEXT_BLOCK_X + (80 - 4*BLOCK_SIZE)/2;
  int startY = NEXT_BLOCK_Y + (80 - 4*BLOCK_SIZE)/2;
  
  // 获取预览方块形状（默认旋转0）
  uint16_t shape = TETROMINOES[game.nextPiece][0];
  
  // 绘制4x4方块
  for (int i = 0; i < 16; i++) {
    if (shape & 0x8000) {  // 当前位有效
      // 计算每个小方块的位置
      int x = startX + (i % 4) * BLOCK_SIZE;
      int y = startY + (i / 4) * BLOCK_SIZE;
      
      // 绘制带边框的小方块
      tft.fillRect(x+1, y+1, BLOCK_SIZE-2, BLOCK_SIZE-2, COLORS[game.nextPiece]);
    }
    shape <<= 1;  // 检查下一位
  }
}

/**
 * @brief 更新计分板显示
 * @details 在屏幕右上角显示分数（防残影处理）
 */
void updateScoreDisplay() {
  // 清空区域（右对齐优化）
  tft.fillRect(160, 5, tft.width()-165, 20, TFT_BLACK);
  
  // 设置文本属性
  tft.setTextColor(TFT_WHITE);  // 白色文字
  tft.setTextSize(1);           // 8pt字体
  tft.setTextFont(1);           // 标准字体

  // 绘制"Score:"标签
  tft.setCursor(160, 5);
  tft.print("Score: ");
  
  // 右对齐分数数值
  String scoreText = String(game.score);
  int textWidth = scoreText.length() * 6;  // 每个字符约6像素宽
  tft.setCursor(240 - textWidth, 5);      // 240为屏幕宽度
  tft.print(scoreText);
}

/**
 * @brief 绘制游戏结束界面
 * @details 包含半透明遮罩、弹窗和闪烁提示
 */
void drawGameOver() {
  // 全屏半透明遮罩（0x0821深灰色）
  tft.fillRect(0, 0, tft.width(), tft.height(), 0x0821);

  // 弹窗参数（居中显示）
  const int popupWidth = 200;
  const int popupHeight = 120;
  int popupX = (tft.width() - popupWidth)/2;
  int popupY = (tft.height() - popupHeight)/2 - 10;
  
  // 绘制弹窗背景（圆角矩形）
  tft.fillRoundRect(popupX, popupY, popupWidth, popupHeight, 8, TFT_NAVY);
  tft.drawRoundRect(popupX, popupY, popupWidth, popupHeight, 8, TFT_WHITE);

  // 带阴影的标题文字
  int textY = popupY + 15;
  tft.setTextSize(GAMEOVER_TITLE_FONT);
  tft.setTextColor(TEXT_SHADOW);
  tft.drawCentreString("GAME OVER", tft.width()/2 + 2, textY + 2, 1); // 阴影
  tft.setTextColor(TFT_RED);
  tft.drawCentreString("GAME OVER", tft.width()/2, textY, 1);         // 前景

  // 得分显示
  textY += tft.fontHeight(GAMEOVER_TITLE_FONT) + 10;
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(GAMEOVER_SCORE_FONT);
  tft.drawCentreString("SCORE:", tft.width()/2, textY, 1);
  
  textY += tft.fontHeight(GAMEOVER_SCORE_FONT) + 5;
  tft.setTextColor(TFT_GOLD);  // 使用金色显示分数
  tft.drawCentreString(String(game.score), tft.width()/2, textY, 1);

  // 闪烁提示逻辑（500ms间隔）
  static uint32_t lastBlink = 0;
  static bool blinkState = false;
  if (millis() - lastBlink > 500) {
    blinkState = !blinkState;
    lastBlink = millis();
    
    // 清除提示区域
    tft.fillRect(popupX, popupY + popupHeight - 25, popupWidth, 20, GAMEOVER_BG_COLOR);
    
    if (blinkState) {
      tft.setTextColor(TFT_WHITE);
      tft.setTextSize(GAMEOVER_HINT_FONT);
      tft.drawCentreString("Press B to Restart", tft.width()/2, 
                          popupY + popupHeight - 20, 1);
    }
  }
}
