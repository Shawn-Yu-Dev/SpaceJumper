#pragma once
// Level.hpp — 关卡构建接口
// 使用前置声明 struct Game, 不包含 Entities.hpp, 保持低耦合。

struct Game; // 前置声明 (完整定义见 Entities.hpp)

void setT(Game &g, int c, int r, int v);
int getT(const Game &g, int c, int r);
bool solidTile(int t);
void buildLevel(Game &g);
