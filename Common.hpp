#pragma once
// Common.hpp — gml 命名空间 + 全局常量 + 可变全局的 extern 声明
// 军规: 本头文件只含 constexpr/内联函数/extern 声明，绝不定义非常量全局变量。

namespace gml {
struct vec2 {
  float x = 0.0f, y = 0.0f;
  vec2() = default;
  vec2(float _x, float _y) : x(_x), y(_y) {}
  vec2 operator+(const vec2 &v) const { return {x + v.x, y + v.y}; }
  vec2 operator-(const vec2 &v) const { return {x - v.x, y - v.y}; }
  vec2 operator*(float s) const { return {x * s, y * s}; }
  vec2 &operator+=(const vec2 &v) { x += v.x; y += v.y; return *this; }
};
} // namespace gml

// 固定视野缩放: 玩法/渲染一律用 960x600 逻辑坐标, SDL 自动缩放到任意窗口.
// winActualW/H 仅记录 OS 实际窗口像素, 不参与任何相机/绘制.
constexpr int LOGICAL_WIDTH = 960;
constexpr int LOGICAL_HEIGHT = 600;
constexpr int WINDOW_WIDTH = 960;
constexpr int WINDOW_HEIGHT = 600;
constexpr int HUD_HEIGHT = 44;

constexpr int TILE_SIZE = 32;
constexpr int MAP_COLS = 124;
constexpr int MAP_ROWS = 19;
constexpr int WORLD_WIDTH = MAP_COLS * TILE_SIZE;
constexpr int WORLD_HEIGHT = MAP_ROWS * TILE_SIZE;

// 物理手感参数
constexpr float GRAVITY = 1500.0f;
constexpr float JUMP_VEL = 620.0f;
constexpr float JUMP_CUT = -240.0f; // 松开跳跃键后的最小上升速度(小跳)
constexpr float MAX_FALL = 760.0f;
constexpr float WALK_MAX = 230.0f;
constexpr float RUN_MAX = 360.0f;
constexpr float GROUND_ACCEL = 2400.0f;
constexpr float AIR_ACCEL = 1700.0f;
constexpr float FRICTION = 2600.0f;
constexpr float COYOTE_TIME = 0.12f;
constexpr float BUFFER_TIME = 0.16f;

// Tile 类型
enum Tile : int {
  T_EMPTY = 0, T_GROUND = 1, T_QUESTION = 2, T_FLAG = 3,
  T_USED = 4, T_BRICK = 5, T_STONE = 7, T_SPIKE = 9
};

inline int tidx(int c, int r) { return r * MAP_COLS + c; }

// 可变全局: 仅声明, 定义在 Common.cpp (消除头文件全局污染)
extern int winActualW;
extern int winActualH;
