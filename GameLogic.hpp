#pragma once
// GameLogic.hpp — 应用上下文 + 固定步长更新接口
// App 聚合 Game / Player / 相机 / 状态机 / 关卡计数器, 替代原来 main() 中的局部变量堆。
// 本头文件单向包含 Entities.hpp, 不被 Entities.hpp 反向包含, 无循环引用。
// 所有更新函数按引用传参, 无虚函数、无额外堆分配, 性能与单文件一致。
#include "Common.hpp"
#include "Entities.hpp"
#include <SDL2/SDL.h>

struct App {
  Game game;
  Player player;
  gml::vec2 cam{0, 0};
  State state = ST_TITLE;
  float globalT = 0.0f;
  bool paused = false;
  bool cpSaved = false;
  gml::vec2 cpPos{73.0f * TILE_SIZE, 12.0f * TILE_SIZE};
  bool smoke = false;

  // 多关卡: 当前关索引 (0..LEVEL_COUNT-1). resetRun 按此索引加载地形.
  int currentLevelIndex = 0;

  // 死亡原因: true = 掉坑/超时 (当前关卡重开), false = 心归零 (直接 GAMEOVER)
  bool respawnLevel = false;

  void resetRun();          // 全重置 (分数/心/盾牌/时间清零) 并按 currentLevelIndex 加载
  void resetToFirstLevel(); // 回到第一关并全重置 (WIN/GAMEOVER 重试用)
  void loadLevelKeepScore(int idx); // 切关加载: 保留分数/金币/心, 重置时间/检查点/玩家/相机
};

// 单个固定步长 (DT = 1/60) 的状态推进; kb 来自 SDL_GetKeyboardState
// 物理手感 (土狼/预输入/变高跳) 与单关版本逐行一致, 本函数只增状态机分支.
void updateFixedStep(App &app, float DT, const Uint8 *kb);

// 全状态通用的特效推进: 砖块弹跳 / 粒子 / 飘分 / 云
void updateFx(Game &g, float DT);
