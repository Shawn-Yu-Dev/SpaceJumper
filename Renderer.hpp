#pragma once
// Renderer.hpp — 绘制接口
// 军规示范: 仅前置声明 Player / Goomba / Game / App, 绝不 #include "Entities.hpp"
// 或 "GameLogic.hpp", 从根本上避免循环引用。完整定义只在 Renderer.cpp 中可见。
#include "Common.hpp"
#include <SDL2/SDL.h>

struct Player; // 前置声明 (定义见 Entities.hpp)
struct Goomba; // 前置声明 (定义见 Entities.hpp)
struct Game;   // 前置声明 (定义见 Entities.hpp)
struct App;    // 前置声明 (定义见 GameLogic.hpp)

void drawCloud(SDL_Renderer *r, int x, int y, float s);
void drawBush(SDL_Renderer *r, int x, int y, int w);
void renderTile(SDL_Renderer *r, int tile, int sx, int sy, float pulse, float bumpDy);
void drawPlayer(SDL_Renderer *r, const Player &p, int sx, int sy, float time);
void drawGoomba(SDL_Renderer *r, const Goomba &e, int sx, int sy, float time);

// 主渲染入口: 一次调用完成世界 + HUD + 覆盖层绘制
// 参数按引用传递, 无动态分配、无虚函数, 性能与单文件版本一致。
void renderAll(SDL_Renderer *ren, App &app);
