#include <TFT_eSPI.h>
TFT_eSPI tft;

// 硬件定义
#define BUTTON_A WIO_KEY_A
#define BUTTON_B WIO_KEY_B
#define BUTTON_C WIO_KEY_C
#define DPAD_UP WIO_5S_UP
#define DPAD_DOWN WIO_5S_DOWN
#define DPAD_LEFT WIO_5S_LEFT
#define DPAD_RIGHT WIO_5S_RIGHT
#define DPAD_PRESS WIO_5S_PRESS

// 游戏参数
#define GRID_WIDTH 10
#define GRID_HEIGHT 22
#define BLOCK_SIZE 11
#define BOARD_X (21 + 2 * BLOCK_SIZE)  // 修复坐标计算
#define BOARD_Y 5

// UI参数
#define SCORE_FONT_SIZE 1
#define GAMEOVER_TITLE_FONT 2
#define GAMEOVER_SCORE_FONT 2
#define GAMEOVER_HINT_FONT 1
#define SCORE_X 160
#define SCORE_Y 3
#define NEXT_BLOCK_X 160
#define NEXT_BLOCK_Y 30
#define GAMEOVER_X_CENTER 160
#define GAMEOVER_Y_TITLE 80
#define GAMEOVER_Y_SCORE 115
#define GAMEOVER_Y_HINT 145

// 方块形状定义
const uint16_t TETROMINOES[7][4] = {
  { 0x0F00, 0x2222, 0x00F0, 0x4444 }, // I型
  { 0x0660, 0x0660, 0x0660, 0x0660 }, // O型
  { 0x0470, 0x2320, 0x0740, 0x0262 }, // T型
  { 0x0360, 0x4620, 0x0360, 0x4620 }, // S型
  { 0x0630, 0x2640, 0x0630, 0x2640 }, // Z型
  { 0x0720, 0x6220, 0x0270, 0x2230 }, // L型
  { 0x0740, 0x2260, 0x0170, 0x6220 }  // J型
};

struct GameState {
  uint8_t grid[GRID_HEIGHT][GRID_WIDTH];
  int currentPiece;
  int rotation;
  int pieceX;
  int pieceY;
  int nextPiece;
  bool gameOver;
  int score;
  unsigned long lastFall;
};

GameState game;

const uint16_t COLORS[7] = {
  TFT_CYAN, TFT_YELLOW, TFT_MAGENTA,
  TFT_GREEN, TFT_RED, TFT_ORANGE, TFT_BLUE
};

void newGame();
void drawBoard();
void drawBlock(int x, int y, uint16_t color);
void drawNextPiece();
bool movePiece(int dx, int dy = 0);
void rotatePiece();
void lockPiece();
void checkLines();
void drawGameOver();
void spawnPiece();
void drawCurrentPiece();
bool collisionCheck(int x, int y, int rotation);
void updateScoreDisplay();

void setup() {
  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);
  pinMode(DPAD_UP, INPUT_PULLUP);
  pinMode(DPAD_DOWN, INPUT_PULLUP);
  pinMode(DPAD_LEFT, INPUT_PULLUP);
  pinMode(DPAD_RIGHT, INPUT_PULLUP);
  pinMode(DPAD_PRESS, INPUT_PULLUP);

  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  newGame();
}

void loop() {
  static unsigned long lastUpdate = millis();
  static unsigned long lastRotateTime = 0;
  const int debounceDelay = 200;

  // 游戏运行状态处理移动
  if (!game.gameOver) {
    if (digitalRead(DPAD_LEFT) == LOW)  movePiece(-1);
    if (digitalRead(DPAD_RIGHT) == LOW) movePiece(1);
    if (digitalRead(DPAD_DOWN) == LOW)  movePiece(0, 1);
  }

  // 重启按钮
  if (digitalRead(BUTTON_B) == LOW) newGame();

  // 旋转按钮带状态检查
  if (digitalRead(DPAD_PRESS) == LOW && 
     (millis() - lastRotateTime) > debounceDelay &&
     !game.gameOver) {
    rotatePiece();
    lastRotateTime = millis();
  }

  // 自动下落逻辑
  if (!game.gameOver && millis() - lastUpdate > 500) {
    if (!movePiece(0, 1)) {
      lockPiece();
      checkLines();
      spawnPiece();
    }
    lastUpdate = millis();
  }
  delay(50);
}

void newGame() {
  tft.fillScreen(TFT_BLACK);
  
  // 绘制游戏边框
  tft.drawFastHLine(0, 0, tft.width(), TFT_WHITE);
  tft.drawFastHLine(0, tft.height()-1, tft.width(), TFT_WHITE);
  tft.drawFastVLine(0, 0, tft.height(), TFT_WHITE);
  tft.drawFastVLine(tft.width()-1, 0, tft.height(), TFT_WHITE);

  // 初始化游戏状态
  memset(game.grid, 0, sizeof(game.grid));
  game.currentPiece = random(7);
  game.nextPiece = random(7);
  game.rotation = 0;
  game.pieceX = 3;
  game.pieceY = 0;
  game.score = 0;
  game.gameOver = false;
  
  drawBoard();
  drawNextPiece();
  updateScoreDisplay();
}

bool movePiece(int dx, int dy) {
  int newX = game.pieceX + dx;
  int newY = game.pieceY + dy;
  
  if (!collisionCheck(newX, newY, game.rotation)) {
    game.pieceX = newX;
    game.pieceY = newY;
    drawBoard();
    drawCurrentPiece();
    return true;
  }
  return false;
}

void rotatePiece() {
  if (game.gameOver) return;
  
  int newRotation = (game.rotation + 1) % 4;
  if (!collisionCheck(game.pieceX, game.pieceY, newRotation)) {
    game.rotation = newRotation;
  } else {
    for (int offset = 1; offset <= 2; offset++) {
      if (!collisionCheck(game.pieceX + offset, game.pieceY, newRotation)) {
        game.pieceX += offset;
        game.rotation = newRotation;
        break;
      }
      if (!collisionCheck(game.pieceX - offset, game.pieceY, newRotation)) {
        game.pieceX -= offset;
        game.rotation = newRotation;
        break;
      }
    }
  }
  drawBoard();
  drawCurrentPiece();
}

void drawBoard() {
  tft.fillRect(BOARD_X, BOARD_Y, 
              GRID_WIDTH*BLOCK_SIZE, 
              (GRID_HEIGHT-1)*BLOCK_SIZE,
              TFT_DARKGREY);

  for (int y = 1; y < GRID_HEIGHT; y++) {
    for (int x = 0; x < GRID_WIDTH; x++) {
      if (game.grid[y][x]) {
        drawBlock(x, y-1, COLORS[game.grid[y][x]-1]);
      }
    }
  }
}

void drawBlock(int x, int y, uint16_t color) {
  int px = BOARD_X + x * BLOCK_SIZE;
  int py = BOARD_Y + y * BLOCK_SIZE;
  tft.fillRect(px+1, py+1, BLOCK_SIZE-2, BLOCK_SIZE-2, color);
}

void drawCurrentPiece() {
  uint16_t shape = TETROMINOES[game.currentPiece][game.rotation];
  for (int i = 0; i < 16; i++) {
    if (shape & 0x8000) {
      int x = game.pieceX + (i % 4);
      int y = game.pieceY + (i / 4);
      if (y >= 1) drawBlock(x, y-1, COLORS[game.currentPiece]);
    }
    shape <<= 1;
  }
}

bool collisionCheck(int x, int y, int rotation) {
  uint16_t shape = TETROMINOES[game.currentPiece][rotation];
  for (int i = 0; i < 16; i++) {
    if (shape & 0x8000) {
      int gridX = x + (i % 4);
      int gridY = y + (i / 4);
      
      if (gridX < 0 || gridX >= GRID_WIDTH) return true;
      if (gridY >= GRID_HEIGHT) return true;
      if (gridY >= 1 && game.grid[gridY][gridX]) return true;
    }
    shape <<= 1;
  }
  return false;
}

void lockPiece() {
  uint16_t shape = TETROMINOES[game.currentPiece][game.rotation];
  for (int i = 0; i < 16; i++) {
    if (shape & 0x8000) {
      int x = game.pieceX + (i % 4);
      int y = game.pieceY + (i / 4);
      if (y >= 1) game.grid[y][x] = game.currentPiece + 1;
    }
    shape <<= 1;
  }
}

void checkLines() {
  for (int y = GRID_HEIGHT-1; y >= 1; y--) {
    bool full = true;
    for (int x = 0; x < GRID_WIDTH; x++) {
      if (!game.grid[y][x]) {
        full = false;
        break;
      }
    }
    
    if (full) {
      game.score += 100;
      updateScoreDisplay();
      for (int yy = y; yy > 1; yy--) {
        memcpy(game.grid[yy], game.grid[yy-1], GRID_WIDTH);
      }
      memset(game.grid[1], 0, GRID_WIDTH);
      drawBoard();
      y++;
    }
  }
}

void spawnPiece() {
  game.currentPiece = game.nextPiece;
  game.nextPiece = random(7);
  game.pieceX = 3;
  game.pieceY = 0;
  game.rotation = 0;
  
  if (collisionCheck(game.pieceX, game.pieceY, 0)) {
    game.gameOver = true;
    drawGameOver();
    return;
  }
  drawNextPiece();
}

void drawNextPiece() {
  tft.fillRect(NEXT_BLOCK_X, NEXT_BLOCK_Y, 80, 80, TFT_BLACK);
  tft.drawRect(NEXT_BLOCK_X, NEXT_BLOCK_Y, 80, 80, TFT_WHITE);
  
  uint16_t shape = TETROMINOES[game.nextPiece][0];
  int startX = NEXT_BLOCK_X + (80 - 4*BLOCK_SIZE)/2;
  int startY = NEXT_BLOCK_Y + (80 - 4*BLOCK_SIZE)/2;
  
  for (int i = 0; i < 16; i++) {
    if (shape & 0x8000) {
      int x = startX + (i % 4) * BLOCK_SIZE;
      int y = startY + (i / 4) * BLOCK_SIZE;
      tft.fillRect(x+1, y+1, BLOCK_SIZE-2, BLOCK_SIZE-2, COLORS[game.nextPiece]);
    }
    shape <<= 1;
  }
}

void updateScoreDisplay() {
  tft.fillRect(SCORE_X, SCORE_Y, 80, 20, TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(SCORE_FONT_SIZE);
  
  tft.setCursor(SCORE_X, SCORE_Y);
  tft.print("SCORE:");
  tft.setCursor(SCORE_X + 40, SCORE_Y);
  tft.print(game.score);
}

void drawGameOver() {
  // 全屏半透明遮  tft.fillRect(0, 0, tft.width(), tft.height(), 0x10A5);
  
  // 绘制弹窗
  tft.fillRoundRect(40, 60, 240, 120, 10, TFT_RED);
  
  // 文字内容
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(GAMEOVER_TITLE_FONT);
  tft.drawCentreString("GAME OVER", GAMEOVER_X_CENTER, GAMEOVER_Y_TITLE, GAMEOVER_TITLE_FONT);
  
  tft.setTextSize(GAMEOVER_SCORE_FONT);
  tft.drawCentreString("SCORE", GAMEOVER_X_CENTER, GAMEOVER_Y_SCORE - 15, GAMEOVER_SCORE_FONT);
  
  tft.setTextColor(TFT_YELLOW);
  tft.drawCentreString(String(game.score), GAMEOVER_X_CENTER, GAMEOVER_Y_SCORE, GAMEOVER_SCORE_FONT);
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(GAMEOVER_HINT_FONT);
  tft.drawCentreString("Press B to restart", GAMEOVER_X_CENTER, GAMEOVER_Y_HINT, GAMEOVER_HINT_FONT);
}