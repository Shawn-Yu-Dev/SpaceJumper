#pragma once
// Physics.hpp — 碰撞 / 粒子生成接口
// 使用前置声明 struct Game, 不包含 Entities.hpp, 避免循环引用与重依赖。
#include "Common.hpp"
#include <SDL2/SDL.h>

struct Game; // 前置声明 (完整定义见 Entities.hpp)

bool boxHitTile(const Game &g, const gml::vec2 &pos, const gml::vec2 &sz);
bool aabb(gml::vec2 a, gml::vec2 as, gml::vec2 b, gml::vec2 bs);
void burst(Game &g, gml::vec2 p, int n, SDL_Color col, float spd, float grav, int sz);
