#pragma once
// Level.hpp == LevelManager.hpp (本工程的关卡配表管理器)
// 静态内存配表多关卡架构: 只读、固定大小、无指针、标准布局、可 constexpr,
// 单一 .rodata 实例, 无 new/malloc、无虚函数, for 循环铺设, 缓存友好。
// 使用前置声明 struct Game, 不包含 Entities.hpp, 保持低耦合。

struct Game; // 前置声明 (完整定义见 Entities.hpp)

// ---- 配表上限 (固定大小, 栈上/静态数组, 绝不动态增长) ----
// 10 关规模: 最多 10 坑 / 32 怪 / 96 特殊砖, 仍为 .rodata 静态只读
constexpr int MAX_PITS     = 12;
constexpr int MAX_BRICKROW = 6;
constexpr int MAX_STAIR    = 10;
constexpr int MAX_SPECIAL  = 96;
constexpr int MAX_COIN     = 40;
constexpr int MAX_COINARC  = 12;
constexpr int MAX_GOOMBA   = 32;
constexpr int MAX_PLAT     = 6;

// ---- 只读配置结构体: 无指针, 内存对齐, 可平凡拷贝 ----
struct PitData         { int c0, c1; };            // 半开区间 [c0,c1), 与老 inPit 语义一致
struct BrickRowData    { int c0, c1, r, withQ; };  // withQ: 0/1, 中点变问号 (mid=(c0+c1)/2)
struct StairData       { int c0, rBase, steps, tile, dx, dy; };
struct SpecialTileData { int c, r, tile; };        // 零散 Q/BRICK/STONE/SPIKE
struct CoinData        { int c, r; float anim; };  // 像素 x=c*32+2, y=r*32 (与老逻辑逐位一致)
struct CoinArcData     { int c0, n, rBase; };      // 坑上弧线, 弧高公式与老 coinArc 一致
// ai: 地面 0=巡逻 1=追击 2=跳跃; 飞行 0=巡逻 1=追击. speed<=0 则用默认 70+关卡加成
struct GoombaSpawnData { int c, r, flyer; int ai; float speed; };
struct PlatformSpawnData {
  float ax, ay, bx, by;   // 像素坐标 (老逻辑 58.0f*TILE_SIZE 等已预乘)
  float w, h, speed, t;
};

struct LevelConfig {
  PitData          pits[MAX_PITS];       int pitCount;
  BrickRowData     brickRows[MAX_BRICKROW]; int brickRowCount;
  StairData        stairs[MAX_STAIR];    int stairCount;
  SpecialTileData  specials[MAX_SPECIAL]; int specialCount;
  CoinData         coins[MAX_COIN];      int coinCount;
  CoinArcData      coinArcs[MAX_COINARC]; int coinArcCount;
  GoombaSpawnData  goombas[MAX_GOOMBA];  int goombaCount;
  PlatformSpawnData plats[MAX_PLAT];     int platCount;
  int flagCol;        // 旗杆列 (旗杆 r=5..16, 城堡在 flagCol+3)
  int checkpointCol;  // 检查点列 (激活阈值 pl.x > checkpointCol*32)
};

// ---- 只读关卡表: 定义在 Level.cpp, 单一 .rodata 实例 (extern, 非头文件静态拷贝) ----
extern const LevelConfig ALL_LEVELS[];
extern const int LEVEL_COUNT;

// 轻量访问器 (inline, 零开销, 无动态分配)
inline int levelCount() { return LEVEL_COUNT; }
inline const LevelConfig &levelConfig(int i) {
  if (i < 0) i = 0;
  if (i >= LEVEL_COUNT) i = LEVEL_COUNT - 1;
  return ALL_LEVELS[i];
}
inline int levelFlagCol(int i) { return levelConfig(i).flagCol; }
inline int levelCheckpointCol(int i) { return levelConfig(i).checkpointCol; }

void setT(Game &g, int c, int r, int v);
int getT(const Game &g, int c, int r);
bool solidTile(int t);

// 关卡加载: 先 clear 运行时数组 (保留 vector 容量, 无新分配),
// 再按 levelIndex 用 for 循环铺设. levelIndex 越界自动钳制.
// 默认参数 0 保持老 buildLevel(g) 调用可编译, 新代码请显式传索引.
void buildLevel(Game &g, int levelIndex = 0);
