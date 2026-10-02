#include "Save.hpp"
#include "Level.hpp"
#include <SDL2/SDL.h>
#include <cstdio>
#include <cstring>
#include <string>

static const unsigned int SAVE_MAGIC = 0x5641534D; // "MSAV" little-endian

const char *saveFilePath() {
  static std::string path;
  if (path.empty()) {
    char *pref = SDL_GetPrefPath("superplatformer", "saves");
    path = pref ? (std::string(pref) + "save.dat") : "save.dat";
    SDL_free(pref);
  }
  return path.c_str();
}

bool saveGameToFile(const SaveData &d) {
  FILE *f = fopen(saveFilePath(), "wb");
  if (!f) return false;

  int fields[5] = {d.unlocked, d.level, d.score, d.coins, d.lives};
  int chk = d.unlocked * 31 + d.level * 17 + d.score * 7 + d.coins * 13 + d.lives * 19;

  fwrite(&SAVE_MAGIC, sizeof(SAVE_MAGIC), 1, f);
  fwrite(fields, sizeof(int), 5, f);
  fwrite(&chk, sizeof(int), 1, f);

  fclose(f);
  return true;
}

bool loadGameFromFile(SaveData &d) {
  FILE *f = fopen(saveFilePath(), "rb");
  if (!f) return false;

  unsigned int magic = 0;
  if (fread(&magic, sizeof(magic), 1, f) != 1) { fclose(f); return false; }
  if (magic != SAVE_MAGIC) { fclose(f); return false; }

  int fields[5] = {0};
  if (fread(fields, sizeof(int), 5, f) != 5) { fclose(f); return false; }

  int chk = 0;
  if (fread(&chk, sizeof(int), 1, f) != 1) { fclose(f); return false; }

  fclose(f);

  int unlocked = fields[0];
  int level    = fields[1];
  int score    = fields[2];
  int coins    = fields[3];
  int lives    = fields[4];

  int expect = unlocked * 31 + level * 17 + score * 7 + coins * 13 + lives * 19;
  if (expect != chk) return false;

  if (unlocked < 0) unlocked = 0;
  if (unlocked > LEVEL_COUNT - 1) unlocked = LEVEL_COUNT - 1;
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
  SaveData d;
  return loadGameFromFile(d);
}

bool clearSaveFile() {
  std::remove(saveFilePath()); // 文件不存在或删除失败均视为已清除
  return true;
}