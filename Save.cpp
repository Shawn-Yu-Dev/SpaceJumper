#include "Save.hpp"
#include <cstdio>

const char *saveFilePath() { return "save.dat"; }

bool saveGameToFile(const SaveData &d) {
  FILE *f = fopen(saveFilePath(), "w");
  if (!f) return false;
  // 魔数 + 5 个整数, 附简单校验和 (异或, 防手改/截断)
  int chk = d.unlocked * 31 + d.level * 17 + d.score * 7 + d.coins * 13 + d.lives * 19;
  fprintf(f, "MARIO1 %d %d %d %d %d %d\n", d.unlocked, d.level, d.score,
          d.coins, d.lives, chk);
  fclose(f);
  return true;
}

bool loadGameFromFile(SaveData &d) {
  FILE *f = fopen(saveFilePath(), "r");
  if (!f) return false;
  char magic[16] = {0};
  int unlocked = 0, level = 0, score = 0, coins = 0, lives = 3, chk = 0;
  int n = fscanf(f, "%15s %d %d %d %d %d %d", magic, &unlocked, &level,
                 &score, &coins, &lives, &chk);
  fclose(f);
  if (n != 7) return false;
  // C++17 起 __func__ 外可用; 此处用普通字符串比较
  bool okMagic = (magic[0] == 'M' && magic[1] == 'A' && magic[2] == 'R');
  if (!okMagic) return false;
  int expect = unlocked * 31 + level * 17 + score * 7 + coins * 13 + lives * 19;
  if (expect != chk) return false;
  // 钳制 (防坏档导致越界)
  if (unlocked < 0) unlocked = 0;
  if (unlocked > 9) unlocked = 9;
  if (level < 0) level = 0;
  if (level > unlocked) level = unlocked;
  if (score < 0) score = 0;
  if (coins < 0) coins = 0;
  if (lives < 1) lives = 3;
  if (lives > 3) lives = 3;
  d.unlocked = unlocked;
  d.level = level;
  d.score = score;
  d.coins = coins;
  d.lives = lives;
  return true;
}

bool hasSaveFile() {
  FILE *f = fopen(saveFilePath(), "r");
  if (!f) return false;
  fclose(f);
  SaveData d;
  return loadGameFromFile(d);
}

bool clearSaveFile() {
  return std::remove(saveFilePath()) == 0;
}
