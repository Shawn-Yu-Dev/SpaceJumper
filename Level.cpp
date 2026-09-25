#include "Level.hpp"
#include "Common.hpp"
#include "Entities.hpp"
#include <cmath>
#include <cstdlib>
#include <utility>
#include <vector>

void setT(Game &g, int c, int r, int v) {
  if (c >= 0 && c < MAP_COLS && r >= 0 && r < MAP_ROWS) g.tiles[tidx(c, r)] = v;
}

int getT(const Game &g, int c, int r) {
  if (c < 0 || c >= MAP_COLS || r < 0 || r >= MAP_ROWS) return T_STONE; // 世界外视为墙
  return g.tiles[tidx(c, r)];
}

bool solidTile(int t) { return t==T_GROUND||t==T_QUESTION||t==T_USED||t==T_BRICK||t==T_STONE; }

void buildLevel(Game &g) {
  g.tiles.assign(MAP_COLS * MAP_ROWS, T_EMPTY);
  g.foes.clear(); g.shrooms.clear(); g.coins.clear();
  g.parts.clear(); g.popups.clear(); g.bumps.clear(); g.plats.clear();
  g.checkpointOn = false; g.checkpoint = g.spawn = {64, 400};
  g.combo = 0;

  // 地面: 底部两行, 留出深坑
  std::vector<std::pair<int,int>> pits = {{22,25},{38,41},{57,62},{77,82},{94,99},{106,109}};
  auto inPit = [&](int c){ for(auto &p:pits) if(c>=p.first&&c<p.second) return true; return false; };
  for (int c = 0; c < MAP_COLS; ++c) {
    if (inPit(c)) continue;
    setT(g, c, 17, T_GROUND); setT(g, c, 18, T_GROUND);
  }
  // 起点小土丘装饰下的实地已足够

  auto brickRow = [&](int c0, int c1, int r, bool withQ){
    for (int c = c0; c <= c1; ++c) setT(g, c, r, withQ && (c== (c0+c1)/2) ? T_QUESTION : T_BRICK);
  };

  // 第一组问号砖 (教学跳)
  setT(g, 8, 13, T_QUESTION);
  setT(g, 11, 13, T_BRICK); setT(g, 12, 13, T_QUESTION); setT(g, 13, 13, T_BRICK);
  setT(g, 12, 9, T_QUESTION); // 高处奖励(蘑菇在高处问号里, 鼓励探索)
  // 金币弧线
  for (int i = 0; i < 4; ++i) g.coins.push_back({{float((14+i)*TILE_SIZE+2), float(12*TILE_SIZE)}, false, float(i)*0.3f});

  // 楼梯 + 石台教学跑跳
  for (int i = 0; i < 4; ++i) { setT(g, 27+i, 16-i, T_STONE); setT(g, 27+i, 17-i, T_STONE); }
  brickRow(30, 33, 12, true);
  for (int i = 0; i < 3; ++i) g.coins.push_back({{float((30+i)*TILE_SIZE+2), float(11*TILE_SIZE)}, false, 0});

  // 中段: 高低石台 + 尖刺
  setT(g, 44, 14, T_STONE); setT(g, 45, 14, T_STONE); setT(g, 46, 14, T_STONE);
  setT(g, 45, 10, T_QUESTION); setT(g, 46, 10, T_QUESTION);
  setT(g, 48, 16, T_SPIKE); setT(g, 49, 16, T_SPIKE);
  setT(g, 48, 13, T_BRICK); setT(g, 49, 13, T_BRICK); setT(g, 50, 13, T_QUESTION);
  for (int i = 0; i < 4; ++i) g.coins.push_back({{float((44+i)*TILE_SIZE+2), float(9*TILE_SIZE)}, false, float(i)*0.2f});

  // 大坑区: 上方浮空砖给落脚点
  setT(g, 58, 12, T_BRICK); setT(g, 59, 12, T_BRICK); setT(g, 60, 12, T_QUESTION);
  setT(g, 63, 14, T_STONE); setT(g, 64, 14, T_STONE); setT(g, 65, 14, T_STONE);
  setT(g, 64, 10, T_QUESTION);
  for (int i = 0; i < 5; ++i) g.coins.push_back({{float((57+i)*TILE_SIZE+2), float(8*TILE_SIZE)}, false, float(i)*0.25f});

  // 双层楼梯峡谷
  for (int i = 0; i < 5; ++i) { setT(g, 68+i, 16-i, T_GROUND); }
  for (int i = 0; i < 5; ++i) { setT(g, 84+i, 12+i, T_GROUND); }
  setT(g, 70, 9, T_QUESTION); setT(g, 72, 9, T_BRICK); setT(g, 86, 9, T_QUESTION);

  // 检查点 (中点小旗台)
  for (int i = 0; i < 3; ++i) setT(g, 73+i, 16, T_STONE);

  // 后段高难度: 窄台 + 尖刺 + 问号
  setT(g, 88, 14, T_STONE); setT(g, 89, 14, T_STONE);
  setT(g, 90, 16, T_SPIKE);
  setT(g, 91, 13, T_BRICK); setT(g, 92, 13, T_QUESTION); setT(g, 93, 13, T_BRICK);
  setT(g, 100, 13, T_STONE); setT(g, 101, 13, T_STONE); setT(g, 102, 13, T_STONE);
  setT(g, 101, 9, T_QUESTION);

  // 终点前楼梯
  for (int i = 0; i < 4; ++i) setT(g, 110+i, 16-i, T_STONE);

  // 旗杆
  g.flagCol = 117;
  for (int r = 5; r <= 16; ++r) setT(g, g.flagCol, r, T_FLAG);
  // 城堡地基
  for (int c = 119; c < 124; ++c) { setT(g, c, 16, T_STONE); setT(g, c, 17, T_GROUND); setT(g, c, 18, T_GROUND); }

  // 金币: 坑上弧线
  auto coinArc = [&](int c0, int n, int rBase){
    for (int i = 0; i < n; ++i) {
      float arc = -fabsf(i - (n-1)/2.0f) * 14.0f;
      g.coins.push_back({{float((c0+i)*TILE_SIZE+2), float(rBase*TILE_SIZE + arc)}, false, float(i)*0.2f});
    }
  };
  coinArc(22, 3, 13); coinArc(38, 3, 13); coinArc(57, 5, 12);
  coinArc(77, 5, 12); coinArc(94, 5, 12); coinArc(106, 3, 13);

  // 敌人
  auto addG = [&](int c, int r, bool flyer){
    Goomba e; e.pos = {float(c*TILE_SIZE+2), float(r*TILE_SIZE+4)};
    e.vel = {flyer ? 0.0f : -70.0f, 0.0f}; e.flyer = flyer; e.baseY = e.pos.y; e.phase = c*0.7f;
    g.foes.push_back(e);
  };
  addG(16, 16, false); addG(28, 15, false); addG(33, 16, false);
  addG(46, 13, false); addG(52, 16, false); addG(54, 16, false);
  addG(60, 11, true);  addG(66, 16, false); addG(71, 15, false);
  addG(79, 10, true);  addG(89, 13, false); addG(103, 16, false);
  addG(104, 16, false);

  // 移动平台: 大坑电梯 + 终点前横移平台
  Platform p1; p1.a = {58.0f*TILE_SIZE, 14.5f*TILE_SIZE}; p1.b = {61.0f*TILE_SIZE, 14.5f*TILE_SIZE};
  p1.pos = p1.a; p1.prev = p1.a; p1.size = {64, 16}; p1.speed = 70; p1.t = 0; g.plats.push_back(p1);
  Platform p2; p2.a = {78.0f*TILE_SIZE, 15.0f*TILE_SIZE}; p2.b = {78.0f*TILE_SIZE, 11.5f*TILE_SIZE};
  p2.pos = p2.a; p2.prev = p2.a; p2.size = {64, 16}; p2.speed = 55; p2.t = 1.2f; g.plats.push_back(p2);
  Platform p3; p3.a = {95.0f*TILE_SIZE, 14.0f*TILE_SIZE}; p3.b = {98.0f*TILE_SIZE, 14.0f*TILE_SIZE};
  p3.pos = p3.a; p3.prev = p3.a; p3.size = {64, 16}; p3.speed = 90; p3.t = 0.5f; g.plats.push_back(p3);

  // 云
  g.clouds.clear();
  for (int i = 0; i < 14; ++i)
    g.clouds.push_back({float(i*320 + (rand()%160)), float(50 + rand()%160), float(0.7+ (rand()%40)/50.0f), float(6+rand()%10)});
}
