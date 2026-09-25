#pragma once
// GameLogic.hpp — 应用上下文 + 固定步长更新接口
// App 聚合 Game / Player / 相机 / 状态机, 替代原来 main() 中的局部变量堆。
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

  void resetRun();
};

// 单个固定步长 (DT = 1/60) 的状态推进; kb 来自 SDL_GetKeyboardState
void updateFixedStep(App &app, float DT, const Uint8 *kb);

// 全状态通用的特效推进: 砖块弹跳 / 粒子 / 飘分 / 云
void updateFx(Game &g, float DT);
