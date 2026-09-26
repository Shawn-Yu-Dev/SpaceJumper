#include "Level.hpp"
#include "Common.hpp"
#include "Entities.hpp"
#include <cmath>
#include <cstdlib>
#include <type_traits>

// 编译期约束: 配表必须保持 POD / 标准布局 / 可平凡拷贝 (零拷贝 memcpy 安全)
static_assert(std::is_standard_layout_v<PitData>);
static_assert(std::is_standard_layout_v<BrickRowData>);
static_assert(std::is_standard_layout_v<SpecialTileData>);
static_assert(std::is_standard_layout_v<CoinData>);
static_assert(std::is_standard_layout_v<GoombaSpawnData>);
static_assert(std::is_standard_layout_v<PlatformSpawnData>);
static_assert(std::is_standard_layout_v<LevelConfig>);
static_assert(std::is_trivially_copyable_v<LevelConfig>);

void setT(Game &g, int c, int r, int v) {
  if (c >= 0 && c < MAP_COLS && r >= 0 && r < MAP_ROWS) g.tiles[tidx(c, r)] = v;
}

int getT(const Game &g, int c, int r) {
  if (c < 0 || c >= MAP_COLS || r < 0 || r >= MAP_ROWS) return T_STONE; // 世界外视为墙
  return g.tiles[tidx(c, r)];
}

bool solidTile(int t) { return t==T_GROUND||t==T_QUESTION||t==T_USED||t==T_BRICK||t==T_STONE; }

// ============================================================================
// 静态只读关卡配置表: 单一 .rodata 实例, 启动即就绪, 运行时零拷贝只读访问.
// 关卡 1 == 老 buildLevel 的 100% 无损抽象 (逐 tile/逐实体核对).
// 关卡 2 == 中难度丘陵缓坡关 (中等宽坑 + 坑上砖桥 + 中速平台, 过渡关).
// 关卡 3 == 原第二关, 高难度空中跳跃关 (宽坑 + 浮空石桥 + 垂直电梯 + 飞敌压制).
// 难度曲线: 1 (宽3~5坑/慢平台/2飞敌) < 2 (宽3~5坑加密/中速平台/2飞敌) < 3 (宽4~8坑/高速平台/5飞敌).
// ============================================================================
const LevelConfig ALL_LEVELS[] = {
  // ---------------- 关卡 1 (老关卡无损迁移) ----------------
  {
    {{22,25},{38,41},{57,62},{77,82},{94,99},{106,109}}, 6,          // pits
    {{30,33,12,1}}, 1,                                               // brickRows
    { // stairs
      {27,16,4,T_STONE,1,-1},{27,17,4,T_STONE,1,-1},
      {68,16,5,T_GROUND,1,-1},{84,12,5,T_GROUND,1,1},
      {73,16,3,T_STONE,1,0},{110,16,4,T_STONE,1,-1}
    }, 6,
    { // specials (35)
      {8,13,T_QUESTION},
      {11,13,T_BRICK},{12,13,T_QUESTION},{13,13,T_BRICK},{12,9,T_QUESTION},
      {44,14,T_STONE},{45,14,T_STONE},{46,14,T_STONE},
      {45,10,T_QUESTION},{46,10,T_QUESTION},
      {48,16,T_SPIKE},{49,16,T_SPIKE},
      {48,13,T_BRICK},{49,13,T_BRICK},{50,13,T_QUESTION},
      {58,12,T_BRICK},{59,12,T_BRICK},{60,12,T_QUESTION},
      {63,14,T_STONE},{64,14,T_STONE},{65,14,T_STONE},{64,10,T_QUESTION},
      {70,9,T_QUESTION},{72,9,T_BRICK},{86,9,T_QUESTION},
      {88,14,T_STONE},{89,14,T_STONE},{90,16,T_SPIKE},
      {91,13,T_BRICK},{92,13,T_QUESTION},{93,13,T_BRICK},
      {100,13,T_STONE},{101,13,T_STONE},{102,13,T_STONE},{101,9,T_QUESTION}
    }, 35,
    { // coins direct (16): x=c*32+2, y=r*32, 与老逻辑逐位一致
      {14,12,0.0f},{15,12,0.3f},{16,12,0.6f},{17,12,0.9f},
      {30,11,0.0f},{31,11,0.0f},{32,11,0.0f},
      {44,9,0.0f},{45,9,0.2f},{46,9,0.4f},{47,9,0.6f},
      {57,8,0.0f},{58,8,0.25f},{59,8,0.5f},{60,8,0.75f},{61,8,1.0f}
    }, 16,
    { // coinArcs (6)
      {22,3,13},{38,3,13},{57,5,12},{77,5,12},{94,5,12},{106,3,13}
    }, 6,
    { // goombas (13)
      {16,16,0},{28,15,0},{33,16,0},{46,13,0},{52,16,0},{54,16,0},
      {60,11,1},{66,16,0},{71,15,0},{79,10,1},{89,13,0},{103,16,0},{104,16,0}
    }, 13,
    { // plats (3): 像素坐标已预乘 TILE_SIZE, 与老 p1/p2/p3 一致
      {58.0f*32,14.5f*32, 61.0f*32,14.5f*32, 64,16,70,0.0f},
      {78.0f*32,15.0f*32, 78.0f*32,11.5f*32, 64,16,55,1.2f},
      {95.0f*32,14.0f*32, 98.0f*32,14.0f*32, 64,16,90,0.5f}
    }, 3,
    117, // flagCol
    73   // checkpointCol
  },
  // ---------------- 关卡 2 (丘陵缓坡·中难度) ----------------
  // 设计: 7 个中等坑 (3~5 列, 比关卡1更密、比关卡3更窄),
  // 坑上砖块/石桥作落脚点降低坠落惩罚, 3 个中速平台辅助,
  // 地刺仅 3 处、飞敌仅 2 只, 跑跳节奏介于关卡1与关卡3之间.
  {
    {{18,22},{35,39},{52,57},{68,73},{84,89},{96,101},{107,110}}, 7,
    {{28,31,12,1},{60,63,11,1}}, 2,
    {
      {24,16,3,T_STONE,1,-1},{24,17,3,T_STONE,1,-1},
      {62,16,4,T_GROUND,1,-1},{79,12,4,T_GROUND,1,1},
      {74,16,2,T_STONE,1,0},{110,16,4,T_STONE,1,-1}
    }, 6,
    { // specials (35)
      {8,13,T_QUESTION},
      {11,13,T_BRICK},{12,13,T_QUESTION},{13,13,T_BRICK},{12,9,T_QUESTION},
      {23,11,T_BRICK},{24,11,T_QUESTION},{25,11,T_BRICK},
      {33,14,T_STONE},{34,14,T_STONE},
      {36,10,T_QUESTION},{37,10,T_QUESTION},
      {43,13,T_BRICK},{44,13,T_QUESTION},{45,13,T_BRICK},
      {49,16,T_SPIKE},{50,16,T_SPIKE},
      {54,10,T_STONE},{55,10,T_STONE},{56,10,T_STONE},{55,7,T_QUESTION},
      {62,9,T_QUESTION},
      {70,12,T_BRICK},{71,12,T_QUESTION},{72,12,T_BRICK},
      {86,11,T_STONE},{87,11,T_STONE},{86,8,T_QUESTION},
      {91,13,T_BRICK},{92,13,T_QUESTION},{93,13,T_BRICK},
      {92,16,T_SPIKE},
      {98,12,T_BRICK},{99,12,T_QUESTION},{103,9,T_QUESTION}
    }, 35,
    { // coins direct (14): 地面引导 + 高空奖励线
      {14,12,0.0f},{15,12,0.3f},
      {33,9,0.0f},{34,9,0.2f},
      {43,9,0.0f},{44,9,0.2f},{45,9,0.4f},
      {60,8,0.0f},{61,8,0.2f},{62,8,0.4f},
      {86,10,0.0f},{87,10,0.2f},
      {98,11,0.0f},{103,8,0.0f}
    }, 14,
    { // coinArcs (7): 每个中等坑上一条弧线
      {18,4,13},{35,4,13},{52,5,12},{68,5,12},{84,5,12},{96,5,12},{107,3,13}
    }, 7,
    { // goombas (12): 地面为主 + 2 只飞敌点缀
      {16,16,0},{27,16,0},{32,16,0},{40,16,0},{47,16,0},
      {58,11,1},{64,16,0},{76,16,0},{83,12,1},{90,16,0},
      {102,16,0},{104,16,0}
    }, 12,
    { // plats (3): 中速横移 + 缓慢垂直电梯
      {53.0f*32,14.5f*32, 56.0f*32,14.5f*32, 64,16,75,0.0f},
      {69.0f*32,15.0f*32, 69.0f*32,11.5f*32, 64,16,60,0.5f},
      {85.0f*32,14.0f*32, 88.0f*32,14.0f*32, 64,16,80,0.0f}
    }, 3,
    117, // flagCol (三关统一, 城堡/渲染复用)
    66   // checkpointCol (中点偏后, 介于关卡1=73 与关卡3=60 之间)
  },
  // ---------------- 关卡 3 (空中跳跃·高难度, 原第二关) ----------------
  // 设计: 8 个宽坑 (5~8 列), 浮空石桥 + 垂直电梯衔接, 高空问号链,
  // 窄台地刺守卫, 飞敌空中压制. 必须疾跑 + 满跳 + 平台借力才能通过.
  {
    {{10,14},{20,27},{33,41},{48,56},{63,71},{78,86},{93,101},{108,112}}, 8,
    {{15,18,12,1},{50,53,11,1}}, 2,
    {
      {5,16,3,T_STONE,1,-1},
      {28,14,5,T_STONE,1,0},
      {43,12,4,T_STONE,1,0},
      {66,11,5,T_STONE,1,0},
      {88,13,4,T_STONE,1,0},
      {110,16,4,T_STONE,1,-1}
    }, 6,
    { // specials (36)
      {6,13,T_QUESTION},
      {11,12,T_BRICK},{12,12,T_QUESTION},{13,12,T_BRICK},{16,9,T_QUESTION},
      {22,11,T_BRICK},{23,11,T_BRICK},{24,11,T_QUESTION},
      {35,10,T_STONE},{36,10,T_STONE},{38,9,T_QUESTION},
      {45,14,T_STONE},{46,14,T_STONE},
      {49,16,T_SPIKE},{50,16,T_SPIKE},
      {52,10,T_QUESTION},{53,10,T_BRICK},
      {60,9,T_STONE},{61,9,T_STONE},{62,9,T_STONE},{65,7,T_QUESTION},
      {72,12,T_BRICK},{73,12,T_QUESTION},{74,12,T_BRICK},
      {80,16,T_SPIKE},
      {81,13,T_BRICK},{82,13,T_QUESTION},{83,13,T_BRICK},
      {95,11,T_STONE},{96,11,T_STONE},{97,11,T_STONE},{96,8,T_QUESTION},
      {103,14,T_STONE},{104,14,T_STONE},{105,16,T_SPIKE},{106,12,T_QUESTION}
    }, 36,
    { // coins direct (12): 天空路径引导线
      {16,8,0.0f},{17,8,0.2f},
      {35,9,0.0f},{36,9,0.2f},
      {60,8,0.0f},{61,8,0.2f},{62,8,0.4f},{65,6,0.0f},
      {95,10,0.0f},{96,10,0.2f},{97,10,0.4f},{106,11,0.0f}
    }, 12,
    { // coinArcs (8): 每个宽坑上一条弧线
      {10,4,12},{20,5,11},{33,5,11},{48,5,10},
      {63,5,9},{78,5,11},{93,5,12},{108,4,13}
    }, 8,
    { // goombas (14): 地面 + 飞敌混合压制
      {12,16,0},{18,16,0},{30,13,0},{37,16,0},{44,13,0},
      {54,11,1},{62,8,1},{68,10,1},{75,16,0},{84,12,1},
      {92,16,0},{99,10,1},{105,16,0},{107,16,0}
    }, 14,
    { // plats (4): 高速横移 + 垂直电梯
      {21.0f*32,14.5f*32, 26.0f*32,14.5f*32, 64,16,90,0.0f},
      {49.0f*32,15.0f*32, 49.0f*32,10.5f*32, 64,16,75,0.5f},
      {64.0f*32,13.0f*32, 69.0f*32,13.0f*32, 64,16,100,0.0f},
      {79.0f*32,15.0f*32, 79.0f*32,11.0f*32, 64,16,80,1.0f}
    }, 4,
    117, // flagCol (与关卡1一致, 城堡/渲染复用)
    60   // checkpointCol (天空段中点)
  }
};

const int LEVEL_COUNT = sizeof(ALL_LEVELS) / sizeof(ALL_LEVELS[0]);

void buildLevel(Game &g, int levelIndex) {
  if (levelIndex < 0) levelIndex = 0;
  if (levelIndex >= LEVEL_COUNT) levelIndex = LEVEL_COUNT - 1;
  const LevelConfig &L = ALL_LEVELS[levelIndex];

  // 先 clear 运行时数组 (保留 vector 容量, 不产生新分配; 与老逻辑一致)
  g.tiles.assign(MAP_COLS * MAP_ROWS, T_EMPTY);
  g.foes.clear(); g.shrooms.clear(); g.coins.clear();
  g.parts.clear(); g.popups.clear(); g.bumps.clear(); g.plats.clear();
  g.checkpointOn = false; g.checkpoint = g.spawn = {64, 400};
  g.combo = 0;

  // 地面: 底部两行, 按配表 pits 留坑 (半开区间, 缓存友好的线性扫描, 无 vector 分配)
  for (int c = 0; c < MAP_COLS; ++c) {
    bool inPit = false;
    for (int i = 0; i < L.pitCount; ++i) {
      if (c >= L.pits[i].c0 && c < L.pits[i].c1) { inPit = true; break; }
    }
    if (inPit) continue;
    setT(g, c, 17, T_GROUND); setT(g, c, 18, T_GROUND);
  }

  // 连续砖块行 (中点变问号, 公式与老 brickRow lambda 一致)
  for (int i = 0; i < L.brickRowCount; ++i) {
    const BrickRowData &b = L.brickRows[i];
    int mid = (b.c0 + b.c1) / 2;
    for (int c = b.c0; c <= b.c1; ++c)
      setT(g, c, b.r, (b.withQ && c == mid) ? T_QUESTION : T_BRICK);
  }

  // 楼梯 / 浮空桥 (含双层厚楼梯的拆分条目, 与老 for 循环逐 tile 一致)
  for (int i = 0; i < L.stairCount; ++i) {
    const StairData &s = L.stairs[i];
    for (int k = 0; k < s.steps; ++k)
      setT(g, s.c0 + s.dx * k, s.rBase + s.dy * k, s.tile);
  }

  // 零散特殊砖 (问号/砖块/石头/地刺)
  for (int i = 0; i < L.specialCount; ++i)
    setT(g, L.specials[i].c, L.specials[i].r, L.specials[i].tile);

  // 旗杆 + 城堡地基 (由 flagCol 程序化生成, 老逻辑: 旗 r=5..16, 城堡 c=flag+2..flag+6)
  g.flagCol = L.flagCol;
  for (int r = 5; r <= 16; ++r) setT(g, g.flagCol, r, T_FLAG);
  for (int c = g.flagCol + 2; c <= g.flagCol + 6 && c < MAP_COLS; ++c) {
    setT(g, c, 16, T_STONE); setT(g, c, 17, T_GROUND); setT(g, c, 18, T_GROUND);
  }

  // 金币直线 (像素公式与老逻辑逐位一致: x=c*32+2, y=r*32)
  for (int i = 0; i < L.coinCount; ++i) {
    const CoinData &cd = L.coins[i];
    g.coins.push_back({{(float)(cd.c * TILE_SIZE + 2), (float)(cd.r * TILE_SIZE)}, false, cd.anim});
  }

  // 金币坑上弧线 (弧高公式与老 coinArc 一致)
  for (int i = 0; i < L.coinArcCount; ++i) {
    const CoinArcData &a = L.coinArcs[i];
    for (int k = 0; k < a.n; ++k) {
      float arc = -fabsf(k - (a.n - 1) / 2.0f) * 14.0f;
      g.coins.push_back({{(float)((a.c0 + k) * TILE_SIZE + 2), (float)(a.rBase * TILE_SIZE + arc)}, false, (float)k * 0.2f});
    }
  }

  // 敌人 (位置/速度/相位公式与老 addG 一致)
  for (int i = 0; i < L.goombaCount; ++i) {
    const GoombaSpawnData &sp = L.goombas[i];
    Goomba e;
    e.pos = {(float)(sp.c * TILE_SIZE + 2), (float)(sp.r * TILE_SIZE + 4)};
    e.vel = {sp.flyer ? 0.0f : -70.0f, 0.0f};
    e.flyer = sp.flyer != 0;
    e.baseY = e.pos.y; e.phase = sp.c * 0.7f;
    g.foes.push_back(e);
  }

  // 移动平台 (像素坐标已预乘, 与老 p1/p2/p3 一致)
  for (int i = 0; i < L.platCount; ++i) {
    const PlatformSpawnData &sp = L.plats[i];
    Platform p;
    p.a = {sp.ax, sp.ay}; p.b = {sp.bx, sp.by};
    p.pos = p.a; p.prev = p.a;
    p.size = {sp.w, sp.h}; p.speed = sp.speed; p.t = sp.t;
    g.plats.push_back(p);
  }

  // 云 (运行时随机装饰, 非配表, 与老逻辑一致)
  g.clouds.clear();
  for (int i = 0; i < 14; ++i)
    g.clouds.push_back({float(i*320 + (rand()%160)), float(50 + rand()%160), float(0.7+ (rand()%40)/50.0f), float(6+rand()%10)});
}
