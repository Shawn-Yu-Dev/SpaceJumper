#pragma once
// Save.hpp — 游戏进度存档 (10 关解锁进度 + 分数/碎片/心)
// 文本格式 save.dat: "MARIO1 unlocked level score coins lives\n"
// 无外部依赖, 无动态分配, 失败静默 (不弹窗, 不崩溃).

struct SaveData {
  int unlocked = 0; // 已解锁的最大关索引 (0..LEVEL_COUNT-1)
  int level = 0;    // 下次继续的关索引
  int score = 0;
  int coins = 0;
  int lives = 3;
};

const char *saveFilePath();
bool saveGameToFile(const SaveData &d);
bool loadGameFromFile(SaveData &d);
bool hasSaveFile();
bool clearSaveFile();
