#include "Renderer.hpp"
#include "Audio.hpp"
#include "Common.hpp"
#include "Entities.hpp"
#include "Font.hpp"
#include "GameLogic.hpp"
#include "Level.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

// ============================================================================
// 太空跳跃风格贴图 (原创, 不使用马里奥素材)
// 说明: 本文件只负责外观绘制, 不触碰任何碰撞/跳跃/状态机逻辑。
// 映射关系 (逻辑 Tile/实体不变, 仅换皮):
//   T_GROUND   -> 月面合金甲板 (深灰 + 青色能量条)
//   T_BRICK    -> 蓝色能量箱 (宝蓝 + 铆钉 + 琥珀指示灯, 与紫色星云区分)
//   T_QUESTION -> ! 能量舱 (青色脉动 + 白色描边, 顶撞给随机奖励)
//   T_USED     -> 耗尽能量舱 (暗灰蓝)
//   T_STONE    -> 空间站船体板 (银灰 + 高光 + 螺栓)
//   T_FLAG     -> 信标桅杆段 (银杆 + 青色灯点)
//   T_SPIKE    -> 熔岩尖刺 (深红基座 + 亮红刺体 + 橙黄内芯, 高辨识度)
//   Player     -> 宇航员 Nova (白衣 + 橙色喷气背包 + 青色面罩)
//   Goomba     -> 虚空幼体 (紫色 blob + 发光眼 + 天线; 飞行种为品红侦察种)
//   Coin       -> 星能碎片 (青色等离子椭圆 + 白核)
//   Shroom     -> 已移除 (顶撞 ! 方块改为随机奖励, 无掉落物)
//   Castle     -> 月球前哨站 (穹顶 + 太阳能板 + 青色舷窗)
//   Flag/Castle-> 信标塔 + 太阳帆
//   Cloud/Bush -> 电离星云 / 晶簇
//   Sun/Sky    -> 深空星野 + 环带行星 + 远月
// ============================================================================

namespace {
// 深空星点: 确定性哈希, 避免每帧 rand 闪烁
inline int starHash(int i, int m) {
  unsigned u = (unsigned)i * 2654435761u;
  u ^= u >> 13; u *= 2147483647u; u ^= u >> 16;
  return (int)(u % (unsigned)m);
}
} // namespace

void drawCloud(SDL_Renderer *r, int x, int y, float s) {
  // 电离星云团: 深紫基底 + 淡紫高光 + 青色底辉 (原创, 非马里奥白云)
  SDL_SetRenderDrawColor(r, 78, 60, 140, 255);
  SDL_Rect a{x, y, int(52*s), int(16*s)}, b{x+int(10*s), y-int(10*s), int(28*s), int(18*s)}, c{x+int(32*s), y-int(3*s), int(26*s), int(13*s)};
  SDL_RenderFillRect(r, &a); SDL_RenderFillRect(r, &b); SDL_RenderFillRect(r, &c);
  SDL_SetRenderDrawColor(r, 130, 170, 255, 255);
  SDL_Rect hi{x+int(12*s), y-int(8*s), int(22*s), int(5*s)};
  SDL_RenderFillRect(r, &hi);
  SDL_SetRenderDrawColor(r, 80, 230, 255, 255);
  SDL_Rect glow{x, y+int(16*s), int(52*s), 2};
  SDL_RenderFillRect(r, &glow);
  // 星云内微星
  SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
  SDL_Rect s1{x+int(16*s), y+int(4*s), 2, 2}, s2{x+int(38*s), y-int(2*s), 2, 2};
  SDL_RenderFillRect(r, &s1); SDL_RenderFillRect(r, &s2);
}

void drawBush(SDL_Renderer *r, int x, int y, int w) {
  // 外星晶簇: 暗岩基座 + 青/品红交替晶体
  SDL_SetRenderDrawColor(r, 38, 46, 72, 255);
  SDL_Rect base{x, y-4, w, 10};
  SDL_RenderFillRect(r, &base);
  int idx = 0;
  for (int i = 0; i < w; i += 16, ++idx) {
    int bx = x + i + 4;
    int h = (idx % 2 == 0) ? 22 : 16;
    if (idx % 2 == 0) SDL_SetRenderDrawColor(r, 70, 230, 255, 255);
    else SDL_SetRenderDrawColor(r, 220, 110, 255, 255);
    // 晶体: 两条斜边 + 中轴
    SDL_RenderDrawLine(r, bx, y-4, bx+6, y-4-h);
    SDL_RenderDrawLine(r, bx+12, y-4, bx+6, y-4-h);
    SDL_RenderDrawLine(r, bx+6, y-4, bx+6, y-4-h);
    // 尖端高光
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_Rect tip{bx+5, y-5-h, 3, 3};
    SDL_RenderFillRect(r, &tip);
  }
}

void renderTile(SDL_Renderer *r, int tile, int sx, int sy, float pulse, float bumpDy) {
  int y = sy + (int)bumpDy;
  SDL_Rect rc{sx, y, TILE_SIZE, TILE_SIZE};
  if (tile == T_GROUND) {
    // 月面合金甲板
    SDL_SetRenderDrawColor(r, 48, 58, 82, 255); SDL_RenderFillRect(r, &rc);
    SDL_SetRenderDrawColor(r, 55, 225, 255, 255);
    SDL_Rect strip{sx, y, TILE_SIZE, 4}; SDL_RenderFillRect(r, &strip);
    SDL_SetRenderDrawColor(r, 22, 130, 160, 255);
    SDL_Rect strip2{sx, y+4, TILE_SIZE, 2}; SDL_RenderFillRect(r, &strip2);
    SDL_SetRenderDrawColor(r, 100, 115, 145, 255);
    SDL_Rect r1{sx+5, y+12, 3, 3}, r2{sx+24, y+12, 3, 3};
    SDL_RenderFillRect(r, &r1); SDL_RenderFillRect(r, &r2);
    SDL_SetRenderDrawColor(r, 34, 41, 60, 255);
    SDL_Rect d1{sx+6, y+18, 6, 4}, d2{sx+19, y+23, 7, 4}, d3{sx+11, y+25, 4, 3};
    SDL_RenderFillRect(r, &d1); SDL_RenderFillRect(r, &d2); SDL_RenderFillRect(r, &d3);
    SDL_SetRenderDrawColor(r, 28, 34, 52, 255);
    SDL_Rect sh{sx, y+TILE_SIZE-3, TILE_SIZE, 3}; SDL_RenderFillRect(r, &sh);
  } else if (tile == T_BRICK) {
    // 蓝色能量箱 (可踩踏, 与紫色星云区分)
    SDL_SetRenderDrawColor(r, 45, 110, 200, 255); SDL_RenderFillRect(r, &rc);
    SDL_SetRenderDrawColor(r, 20, 55, 120, 255);
    for (int i = 1; i < 4; ++i) { SDL_RenderDrawLine(r, sx, y+i*8, sx+TILE_SIZE, y+i*8); }
    SDL_RenderDrawLine(r, sx+16, y, sx+16, y+8); SDL_RenderDrawLine(r, sx+8, y+8, sx+8, y+16);
    SDL_RenderDrawLine(r, sx+24, y+8, sx+24, y+16); SDL_RenderDrawLine(r, sx+16, y+16, sx+16, y+24);
    SDL_RenderDrawLine(r, sx+8, y+24, sx+8, y+TILE_SIZE); SDL_RenderDrawLine(r, sx+24, y+24, sx+24, y+TILE_SIZE);
    // 铆钉 + 琥珀指示灯 (蓝底上更醒目)
    SDL_SetRenderDrawColor(r, 150, 200, 255, 255);
    SDL_Rect b1{sx+2, y+2, 3, 3}, b2{sx+TILE_SIZE-5, y+2, 3, 3};
    SDL_RenderFillRect(r, &b1); SDL_RenderFillRect(r, &b2);
    SDL_SetRenderDrawColor(r, 255, 170, 60, 255);
    SDL_Rect lamp{sx+13, y+13, 6, 4}; SDL_RenderFillRect(r, &lamp);
  } else if (tile == T_QUESTION) {
    // 补给能量舱 (脉动青色)
    int glow = (int)(185 + 45 * sinf(pulse * 4.0f));
    SDL_SetRenderDrawColor(r, 20, glow, 225, 255); SDL_RenderFillRect(r, &rc);
    SDL_SetRenderDrawColor(r, 220, 250, 255, 255); SDL_RenderDrawRect(r, &rc);
    SDL_Rect in{sx+3, y+3, TILE_SIZE-6, TILE_SIZE-6};
    SDL_SetRenderDrawColor(r, 12, 70, 95, 255); SDL_RenderDrawRect(r, &in);
    SDL_SetRenderDrawColor(r, 160, 240, 255, 255);
    SDL_Rect hi{sx+4, y+4, TILE_SIZE-8, 3}; SDL_RenderFillRect(r, &hi);
    SDL_SetRenderDrawColor(r, 200, 245, 255, 255);
    SDL_Rect bolt1{sx+5, y+TILE_SIZE-6, 3, 3}, bolt2{sx+TILE_SIZE-8, y+TILE_SIZE-6, 3, 3};
    SDL_RenderFillRect(r, &bolt1); SDL_RenderFillRect(r, &bolt2);
    drawText(r, "!", sx+9, y+6, 2, {240,255,255,255});
  } else if (tile == T_USED) {
    // 耗尽能量舱
    SDL_SetRenderDrawColor(r, 62, 72, 98, 255); SDL_RenderFillRect(r, &rc);
    SDL_SetRenderDrawColor(r, 40, 46, 68, 255); SDL_RenderDrawRect(r, &rc);
    SDL_SetRenderDrawColor(r, 80, 92, 120, 255);
    SDL_Rect top{sx, y, TILE_SIZE, 3}; SDL_RenderFillRect(r, &top);
    SDL_SetRenderDrawColor(r, 45, 52, 72, 255);
    for (int i = 0; i < 3; ++i) { SDL_Rect vent{sx+5, y+7+i*8, TILE_SIZE-10, 3}; SDL_RenderFillRect(r, &vent); }
  } else if (tile == T_STONE) {
    // 空间站船体板
    SDL_SetRenderDrawColor(r, 112, 128, 152, 255); SDL_RenderFillRect(r, &rc);
    SDL_SetRenderDrawColor(r, 170, 185, 210, 255);
    SDL_Rect h{sx+3, y+2, TILE_SIZE-6, 3}; SDL_RenderFillRect(r, &h);
    SDL_SetRenderDrawColor(r, 85, 98, 120, 255);
    SDL_Rect mid{sx, y+14, TILE_SIZE, 2}; SDL_RenderFillRect(r, &mid);
    SDL_SetRenderDrawColor(r, 60, 70, 90, 255); SDL_RenderDrawRect(r, &rc);
    SDL_SetRenderDrawColor(r, 55, 62, 80, 255);
    SDL_Rect o1{sx+4, y+4, 3, 3}, o2{sx+TILE_SIZE-7, y+4, 3, 3},
             o3{sx+4, y+TILE_SIZE-7, 3, 3}, o4{sx+TILE_SIZE-7, y+TILE_SIZE-7, 3, 3};
    SDL_RenderFillRect(r, &o1); SDL_RenderFillRect(r, &o2);
    SDL_RenderFillRect(r, &o3); SDL_RenderFillRect(r, &o4);
  } else if (tile == T_FLAG) {
    // 信标桅杆段
    SDL_SetRenderDrawColor(r, 30, 36, 58, 255); SDL_RenderFillRect(r, &rc);
    SDL_SetRenderDrawColor(r, 175, 190, 210, 255);
    SDL_Rect pole{sx+13, y, 6, TILE_SIZE}; SDL_RenderFillRect(r, &pole);
    SDL_SetRenderDrawColor(r, 110, 125, 150, 255);
    SDL_RenderDrawLine(r, sx+13, y, sx+13, y+TILE_SIZE);
    SDL_SetRenderDrawColor(r, 80, 230, 255, 255);
    SDL_Rect lamp{sx+13, y+6, 6, 4}; SDL_RenderFillRect(r, &lamp);
    SDL_Rect lamp2{sx+13, y+20, 6, 4}; SDL_RenderFillRect(r, &lamp2);
  } else if (tile == T_SPIKE) {
    // 熔岩尖刺 (红色高辨识度: 深红基座 + 亮红刺体 + 橙黄内芯)
    SDL_SetRenderDrawColor(r, 90, 20, 20, 255);
    SDL_Rect base{sx, y+22, TILE_SIZE, 10}; SDL_RenderFillRect(r, &base);
    for (int i = 0; i < 4; ++i) {
      int x0 = sx + i*8;
      // 外层亮红刺壁
      SDL_SetRenderDrawColor(r, 255, 60, 60, 255);
      SDL_RenderDrawLine(r, x0, y+22, x0+4, y+8);
      SDL_RenderDrawLine(r, x0+4, y+8, x0+8, y+22);
      // 内芯橙黄 (警示色)
      SDL_SetRenderDrawColor(r, 255, 200, 60, 255);
      SDL_RenderDrawLine(r, x0+4, y+22, x0+4, y+10);
    }
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    for (int i = 0; i < 4; ++i) {
      SDL_Rect tip{sx+i*8+3, y+7, 2, 2};
      SDL_RenderFillRect(r, &tip);
    }
    SDL_SetRenderDrawColor(r, 255, 80, 80, 255);
    SDL_Rect w{sx, y+22, TILE_SIZE, 2}; SDL_RenderFillRect(r, &w);
  }
}

void drawPlayer(SDL_Renderer *r, const Player &p, int sx, int sy, float time) {
  // 宇航员 Nova: 白衣 + 橙色喷气背包 + 青色面罩 (无红帽/背带裤/肤色)
  int w = (int)p.size.x, h = (int)p.size.y;
  float sq = p.squash;
  int dw = w + (int)(sq * 8), dh = h - (int)(sq * 10);
  int dx = sx - (dw - w) / 2, dy = sy + (h - dh);
  bool running = fabsf(p.vel.x) > 30 && p.grounded;
  int legF = running ? (int)(time * 12) % 2 : 0;

  // 无无敌帧: 受伤无闪烁, 直接扣心 (靠位移分离避免连续受伤)

  // 磁力靴
  SDL_SetRenderDrawColor(r, 55, 65, 90, 255);
  if (!p.grounded) {
    SDL_Rect l1{dx+3, dy+dh-8, 7, 8}, l2{dx+dw-10, dy+dh-8, 7, 8};
    SDL_RenderFillRect(r, &l1); SDL_RenderFillRect(r, &l2);
  } else if (legF == 0) {
    SDL_Rect l1{dx+3, dy+dh-8, 7, 8}, l2{dx+dw-10, dy+dh-8, 7, 8};
    SDL_RenderFillRect(r, &l1); SDL_RenderFillRect(r, &l2);
  } else {
    SDL_Rect l1{dx+2, dy+dh-6, 7, 6}, l2{dx+dw-9, dy+dh-9, 7, 9};
    SDL_RenderFillRect(r, &l1); SDL_RenderFillRect(r, &l2);
  }
  // 靴底青色磁条
  SDL_SetRenderDrawColor(r, 80, 230, 255, 255);
  {
    SDL_Rect s1{dx+3, dy+dh-2, 7, 2}, s2{dx+dw-10, dy+dh-2, 7, 2};
    SDL_RenderFillRect(r, &s1); SDL_RenderFillRect(r, &s2);
  }
  // 喷气背包 (背向面罩一侧)
  {
    int bx = (p.face > 0) ? dx - 2 : dx + dw - 6;
    SDL_SetRenderDrawColor(r, 255, 140, 45, 255);
    SDL_Rect pack{bx, dy+15, 8, dh-26};
    SDL_RenderFillRect(r, &pack);
    SDL_SetRenderDrawColor(r, 180, 90, 20, 255);
    SDL_RenderDrawRect(r, &pack);
    // 喷口辉光 (上升/奔跑时更亮)
    bool thrust = !p.grounded || fabsf(p.vel.x) > 200;
    SDL_SetRenderDrawColor(r, thrust ? 120, 240, 255, 255 : 90, 160, 180, 255);
    SDL_Rect noz{bx+1, dy+dh-12, 6, 3};
    SDL_RenderFillRect(r, &noz);
  }
  // 宇航服躯干 (白色)
  SDL_SetRenderDrawColor(r, 228, 236, 246, 255);
  SDL_Rect body{dx+2, dy+14, dw-4, dh-22};
  SDL_RenderFillRect(r, &body);
  SDL_SetRenderDrawColor(r, 160, 175, 195, 255);
  SDL_RenderDrawRect(r, &body);
  // 胸口能量核
  SDL_SetRenderDrawColor(r, 70, 220, 255, 255);
  SDL_Rect core{dx+dw/2-3, dy+20, 6, 6}; SDL_RenderFillRect(r, &core);
  SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
  SDL_Rect coreHi{dx+dw/2-1, dy+21, 2, 2}; SDL_RenderFillRect(r, &coreHi);
  // 腰带
  SDL_SetRenderDrawColor(r, 255, 150, 50, 255);
  SDL_Rect belt{dx+2, dy+dh-12, dw-4, 3}; SDL_RenderFillRect(r, &belt);
  // 头盔 (白色) + 青色面罩 (朝向 face 一侧)
  SDL_SetRenderDrawColor(r, 235, 242, 252, 255);
  SDL_Rect helm{dx+2, dy, dw-4, 15}; SDL_RenderFillRect(r, &helm);
  SDL_SetRenderDrawColor(r, 150, 165, 190, 255);
  SDL_RenderDrawRect(r, &helm);
  {
    int vx = p.face > 0 ? dx+dw-11 : dx+3;
    SDL_SetRenderDrawColor(r, 40, 110, 150, 255);
    SDL_Rect visor{vx-1, dy+4, 9, 9}; SDL_RenderFillRect(r, &visor);
    SDL_SetRenderDrawColor(r, 90, 225, 255, 255);
    SDL_Rect glass{vx, dy+5, 7, 7}; SDL_RenderFillRect(r, &glass);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_Rect shine{vx+1, dy+6, 2, 3}; SDL_RenderFillRect(r, &shine);
  }
  // 天线
  SDL_SetRenderDrawColor(r, 160, 175, 195, 255);
  SDL_RenderDrawLine(r, dx+dw/2, dy, dx+dw/2, dy-5);
  SDL_SetRenderDrawColor(r, 255, 120, 50, 255);
  SDL_Rect ant{dx+dw/2-1, dy-7, 3, 3}; SDL_RenderFillRect(r, &ant);
  // 举盾形态: 按住 Q 展开青色护盾 (剩余格挡次数越多越亮)
  if (p.shieldUp && p.shieldCharges > 0) {
    int a = 140 + 60 * p.shieldCharges / SHIELD_MAX_CHARGES;
    SDL_SetRenderDrawColor(r, 90, 240, 255, 255);
    SDL_Rect o{dx-4, dy-10, dw+8, dh+12};
    SDL_RenderDrawRect(r, &o);
    (void)a;
    // 四角加固线以示护盾
    SDL_RenderDrawLine(r, o.x, o.y, o.x+7, o.y);
    SDL_RenderDrawLine(r, o.x, o.y, o.x, o.y+7);
    SDL_RenderDrawLine(r, o.x+o.w, o.y, o.x+o.w-7, o.y);
    SDL_RenderDrawLine(r, o.x+o.w, o.y, o.x+o.w, o.y+7);
    SDL_RenderDrawLine(r, o.x, o.y+o.h, o.x+7, o.y+o.h);
    SDL_RenderDrawLine(r, o.x, o.y+o.h, o.x, o.y+o.h-7);
    SDL_RenderDrawLine(r, o.x+o.w, o.y+o.h, o.x+o.w-7, o.y+o.h);
    SDL_RenderDrawLine(r, o.x+o.w, o.y+o.h, o.x+o.w, o.y+o.h-7);
  }
}

void drawBullet(SDL_Renderer *r, const Bullet &b, int camX, int camY) {
  // 子弹: 黄白等离子弹 (攻击用)
  int sx = (int)(b.pos.x - camX), sy = (int)(b.pos.y - camY);
  SDL_SetRenderDrawColor(r, 255, 180, 60, 255);
  SDL_Rect glow{sx-2, sy-2, (int)b.size.x+4, (int)b.size.y+4};
  SDL_RenderFillRect(r, &glow);
  SDL_SetRenderDrawColor(r, 255, 245, 200, 255);
  SDL_Rect core{sx, sy, (int)b.size.x, (int)b.size.y};
  SDL_RenderFillRect(r, &core);
}

void drawTrajectory(SDL_Renderer *r, const Game &g, const Player &pl, int camX, int camY) {
  // 预瞄虚线: 从玩家中心沿发射方向步进, 遇实心方块即止
  float dx = (float)pl.face, dy = (float)pl.aimY * 0.6f;
  float len = sqrtf(dx*dx + dy*dy);
  if (len < 0.01f) return;
  dx /= len; dy /= len;
  float ox = pl.pos.x + pl.size.x / 2, oy = pl.pos.y + pl.size.y * 0.4f;
  SDL_SetRenderDrawColor(r, 255, 220, 120, 255);
  for (float t = 16.0f; t < TRAJ_PREVIEW_LEN; t += 14.0f) {
    float px = ox + dx * t, py = oy + dy * t;
    int c = (int)floorf(px / TILE_SIZE), rr = (int)floorf(py / TILE_SIZE);
    if (c < 0 || c >= MAP_COLS || rr < 0 || rr >= MAP_ROWS) break;
    int tile = g.tiles[tidx(c, rr)];
    if (tile == T_GROUND || tile == T_QUESTION || tile == T_USED ||
        tile == T_BRICK || tile == T_STONE) break;
    SDL_Rect dot{(int)(px - camX), (int)(py - camY), 3, 3};
    SDL_RenderFillRect(r, &dot);
  }
}

void drawGoomba(SDL_Renderer *r, const Goomba &e, int sx, int sy, float time) {
  // 虚空幼体: 紫色凝胶 blob + 发光复眼 + 头顶触须 (无棕色栗子/怒眉)
  int w = (int)e.size.x, h = (int)e.size.y;
  if (e.squashT > 0) { // 被踩扁: 能量水洼
    SDL_SetRenderDrawColor(r, 95, 70, 160, 255);
    SDL_Rect flat{sx, sy+h-10, w, 10}; SDL_RenderFillRect(r, &flat);
    SDL_SetRenderDrawColor(r, 120, 240, 255, 255);
    SDL_Rect goo{sx+4, sy+h-9, 8, 3}; SDL_RenderFillRect(r, &goo);
    SDL_Rect goo2{sx+w-12, sy+h-8, 6, 2}; SDL_RenderFillRect(r, &goo2);
    return;
  }
  float bob = sinf(time * 10 + e.phase) * 1.5f;
  int by = sy + (int)bob;
  // 身体 (受伤后变红提示: 还需 1 发)
  if (e.hp <= 1) SDL_SetRenderDrawColor(r, 235, 90, 90, 255);
  else if (e.flyer) SDL_SetRenderDrawColor(r, 225, 110, 190, 255); // 品红侦察种
  else SDL_SetRenderDrawColor(r, 135, 105, 230, 255);              // 紫色幼体
  SDL_Rect body{sx, by, w, h-6}; SDL_RenderFillRect(r, &body);
  // 肚皮高光
  SDL_SetRenderDrawColor(r, 190, 170, 255, 255);
  SDL_Rect belly{sx+5, by+h-12, w-10, 4}; SDL_RenderFillRect(r, &belly);
  // 悬浮足 (暗色 + 青色辉光)
  int step = (int)(time * 8 + e.phase) % 2;
  SDL_SetRenderDrawColor(r, 40, 35, 70, 255);
  SDL_Rect f1{sx+1+(step?2:0), sy+h-6, 10, 6}, f2{sx+w-11-(step?2:0), sy+h-6, 10, 6};
  SDL_RenderFillRect(r, &f1); SDL_RenderFillRect(r, &f2);
  SDL_SetRenderDrawColor(r, 90, 235, 255, 255);
  SDL_Rect g1{sx+1+(step?2:0), sy+h-2, 10, 2}, g2{sx+w-11-(step?2:0), sy+h-2, 10, 2};
  SDL_RenderFillRect(r, &g1); SDL_RenderFillRect(r, &g2);
  // 复眼 (青白发光, 无怒眉)
  SDL_SetRenderDrawColor(r, 225, 255, 255, 255);
  SDL_Rect e1{sx+4, by+8, 7, 8}, e2{sx+w-11, by+8, 7, 8};
  SDL_RenderFillRect(r, &e1); SDL_RenderFillRect(r, &e2);
  SDL_SetRenderDrawColor(r, 30, 60, 90, 255);
  SDL_Rect p1{sx+6, by+11, 3, 5}, p2{sx+w-9, by+11, 3, 5};
  SDL_RenderFillRect(r, &p1); SDL_RenderFillRect(r, &p2);
  SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
  SDL_Rect h1{sx+6, by+11, 1, 2}, h2{sx+w-9, by+11, 1, 2};
  SDL_RenderFillRect(r, &h1); SDL_RenderFillRect(r, &h2);
  // 头顶触须 + 发光端
  SDL_SetRenderDrawColor(r, 90, 70, 150, 255);
  SDL_RenderDrawLine(r, sx+w/2-4, by, sx+w/2-6, by-6);
  SDL_RenderDrawLine(r, sx+w/2+4, by, sx+w/2+6, by-6);
  SDL_SetRenderDrawColor(r, 150, 255, 140, 255);
  SDL_Rect t1{sx+w/2-7, by-8, 3, 3}, t2{sx+w/2+5, by-8, 3, 3};
  SDL_RenderFillRect(r, &t1); SDL_RenderFillRect(r, &t2);
  if (e.flyer) { // 离子翼
    SDL_SetRenderDrawColor(r, 120, 240, 255, 255);
    int flap = (int)(sinf(time*16)*4);
    SDL_Rect w1{sx-8, by+4+flap, 10, 6}, w2{sx+w-2, by+4-flap, 10, 6};
    SDL_RenderFillRect(r, &w1); SDL_RenderFillRect(r, &w2);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_Rect wh1{sx-8, by+4+flap, 10, 2}, wh2{sx+w-2, by+4-flap, 10, 2};
    SDL_RenderFillRect(r, &wh1); SDL_RenderFillRect(r, &wh2);
  }
}

void renderAll(SDL_Renderer *ren, App &app) {
  Game &g = app.game;
  Player &pl = app.player;
  State state = app.state;
  float globalT = app.globalT;
  bool paused = app.paused;
  gml::vec2 cam = app.cam;

  float shx = 0, shy = 0;
  if (g.shake > 0) { shx = (rand()%7-3); shy = (rand()%7-3); }
  int camX = (int)(cam.x + shx), camY = (int)(cam.y + shy);

  // 深空渐变: 顶部近黑 -> 底部深靛蓝
  for (int y = 0; y < WINDOW_HEIGHT; y += 4) {
    int t = y * 255 / WINDOW_HEIGHT;
    SDL_SetRenderDrawColor(ren, 6 + t/10, 12 + t/8, 30 + t/4, 255);
    SDL_Rect rc{0, y, WINDOW_WIDTH, 4}; SDL_RenderFillRect(ren, &rc);
  }
  // 星野 (确定性分布 + 呼吸闪烁)
  for (int i = 0; i < 140; ++i) {
    int sx = starHash(i, WINDOW_WIDTH);
    int sy = HUD_HEIGHT + starHash(i + 1000, WINDOW_HEIGHT - HUD_HEIGHT);
    float tw = 0.55f + 0.45f * sinf(globalT * 2.0f + (float)i * 1.7f);
    int b = (int)(140 + 115 * tw);
    if (i % 9 == 0) SDL_SetRenderDrawColor(ren, b, 240, 255, 255);
    else if (i % 13 == 0) SDL_SetRenderDrawColor(ren, 255, b - 40 > 0 ? b - 40 : 0, 230, 255);
    else SDL_SetRenderDrawColor(ren, b, b, 255, 255);
    int sz = (i % 17 == 0) ? 3 : 2;
    SDL_Rect st{sx, sy, sz, sz}; SDL_RenderFillRect(ren, &st);
  }
  // 环带行星 (右上)
  {
    int px = WINDOW_WIDTH - 130, py = 58;
    SDL_SetRenderDrawColor(ren, 50, 140, 180, 255);
    SDL_Rect planet{px, py, 62, 62}; SDL_RenderFillRect(ren, &planet);
    SDL_SetRenderDrawColor(ren, 110, 215, 235, 255);
    SDL_Rect hl{px+8, py+8, 26, 26}; SDL_RenderFillRect(ren, &hl);
    SDL_SetRenderDrawColor(ren, 40, 100, 135, 255);
    SDL_Rect sh{px+10, py+40, 42, 8}; SDL_RenderFillRect(ren, &sh);
    // 光环
    SDL_SetRenderDrawColor(ren, 140, 200, 220, 255);
    SDL_RenderDrawLine(ren, px-22, py+44, px+84, py+22);
    SDL_RenderDrawLine(ren, px-22, py+47, px+84, py+25);
    SDL_SetRenderDrawColor(ren, 90, 150, 175, 255);
    SDL_RenderDrawLine(ren, px-22, py+50, px+84, py+28);
  }
  // 远月 (左上, 带陨石坑)
  {
    SDL_SetRenderDrawColor(ren, 165, 175, 195, 255);
    SDL_Rect moon{84, 86, 30, 30}; SDL_RenderFillRect(ren, &moon);
    SDL_SetRenderDrawColor(ren, 130, 140, 165, 255);
    SDL_Rect c1{90, 92, 7, 7}, c2{100, 102, 5, 5};
    SDL_RenderFillRect(ren, &c1); SDL_RenderFillRect(ren, &c2);
  }

  // 远景星云带 (视差 0.3)
  for (int i = -1; i < 12; ++i) {
    int bx = (int)(i * 340 - camX * 0.3f) % (WORLD_WIDTH) ;
    int mx = bx; if (mx < -200) mx += 4000;
    int base = WINDOW_HEIGHT - 110 - HUD_HEIGHT + camY/4;
    if (mx < -260 || mx > WINDOW_WIDTH + 60) continue;
    if ((i & 1) == 0) SDL_SetRenderDrawColor(ren, 62, 44, 118, 255);
    else SDL_SetRenderDrawColor(ren, 38, 78, 138, 255);
    SDL_Rect neb{mx, base - 40, 190, 36}; SDL_RenderFillRect(ren, &neb);
    SDL_SetRenderDrawColor(ren, 45, 55, 90, 255);
    // 远处小行星剪影 (三角)
    SDL_RenderDrawLine(ren, mx+30, base, mx+70, base-52);
    SDL_RenderDrawLine(ren, mx+70, base-52, mx+110, base);
  }
  // 近景环形山脊 (视差 0.6)
  for (int i = 0; i < 20; ++i) {
    int hx = (int)(i * 420 - camX * 0.6f);
    if (hx < -140 || hx > WINDOW_WIDTH+140) continue;
    SDL_SetRenderDrawColor(ren, 36, 46, 72, 255);
    SDL_Rect h{hx, WINDOW_HEIGHT-110+HUD_HEIGHT/2 + camY/3, 120, 60};
    SDL_RenderFillRect(ren, &h);
    SDL_SetRenderDrawColor(ren, 70, 225, 255, 255);
    SDL_Rect rim{hx, WINDOW_HEIGHT-110+HUD_HEIGHT/2 + camY/3, 120, 2};
    SDL_RenderFillRect(ren, &rim);
    SDL_SetRenderDrawColor(ren, 26, 34, 56, 255);
    SDL_Rect crater{hx+30, WINDOW_HEIGHT-104+HUD_HEIGHT/2 + camY/3, 26, 8};
    SDL_RenderFillRect(ren, &crater);
  }
  // 星云团 (视差 0.5 + 自身漂移)
  for (auto &c : g.clouds) {
    int sx = (int)(c.x - camX * 0.5f);
    if (sx < -120 || sx > WINDOW_WIDTH+120) continue;
    drawCloud(ren, sx % (WINDOW_WIDTH+240) < 0 ? sx + WINDOW_WIDTH+240 : sx, (int)c.y + 30, c.s);
  }

  if (state != ST_TITLE) {
    // bump 偏移查询 (逻辑不变)
    auto bumpDy = [&](int c, int r)->float {
      for (auto &b : g.bumps) if (b.c==c && b.r==r) return -sinf((0.22f-b.t)/0.22f*3.14159f)*10.0f;
      return 0;
    };
    // 瓦片 (剔除, 逻辑不变)
    int c0 = std::max(0, camX / TILE_SIZE - 1), c1 = std::min(MAP_COLS-1, (camX+WINDOW_WIDTH)/TILE_SIZE + 1);
    for (int r = 0; r < MAP_ROWS; ++r) for (int c = c0; c <= c1; ++c) {
      int t = getT(g, c, r);
      if (t == T_EMPTY) continue;
      if (t == T_FLAG) continue; // 信标桅杆统一绘制
      int sx = c*TILE_SIZE - camX, sy = r*TILE_SIZE - camY;
      renderTile(ren, t, sx, sy, globalT + c*0.3f, bumpDy(c, r));
    }
    // 信标塔 + 太阳帆 + 月球前哨站
    {
      int fx = g.flagCol*TILE_SIZE - camX;
      SDL_SetRenderDrawColor(ren, 185, 195, 215, 255);
      SDL_Rect pole{fx+13, 5*TILE_SIZE-camY, 6, 12*TILE_SIZE};
      SDL_RenderFillRect(ren, &pole);
      SDL_SetRenderDrawColor(ren, 110, 125, 150, 255);
      SDL_RenderDrawLine(ren, fx+13, 5*TILE_SIZE-camY, fx+13, 17*TILE_SIZE-camY);
      // 横向能量环
      SDL_SetRenderDrawColor(ren, 80, 230, 255, 255);
      for (int yy = 6*TILE_SIZE; yy < 16*TILE_SIZE; yy += TILE_SIZE) {
        SDL_Rect ring{fx+11, yy-camY, 10, 2};
        SDL_RenderFillRect(ren, &ring);
      }
      // 顶部信标灯 (呼吸)
      int pulse = (int)(200 + 55 * sinf(globalT * 5.0f));
      SDL_SetRenderDrawColor(ren, 255, 175, 70, 255);
      SDL_Rect ball{fx+8, 5*TILE_SIZE-10-camY, 16, 16}; SDL_RenderFillRect(ren, &ball);
      SDL_SetRenderDrawColor(ren, 255, pulse, 120, 255);
      SDL_Rect hot{fx+11, 5*TILE_SIZE-7-camY, 10, 10}; SDL_RenderFillRect(ren, &hot);
      float wv = sinf(globalT*6)*4;
      int fy = 5*TILE_SIZE+20 - camY + ((state==ST_WINANIM || state==ST_NEXTLEVEL) ? (int)(g.stateT*60) : 0);
      if (fy > 12*TILE_SIZE-camY) fy = 12*TILE_SIZE-camY;
      // 太阳帆
      SDL_SetRenderDrawColor(ren, 35, 55, 120, 255);
      SDL_Rect flag{fx-44, fy, 50, 32}; SDL_RenderFillRect(ren, &flag);
      SDL_SetRenderDrawColor(ren, 90, 230, 255, 255); SDL_RenderDrawRect(ren, &flag);
      SDL_SetRenderDrawColor(ren, 90, 230, 255, 255);
      SDL_RenderDrawLine(ren, fx-44, fy, fx+6, fy+32);
      SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
      drawText(ren, "*", fx-28, fy+2, 2, {180,240,255,255});
      (void)wv;
      // 月球前哨站 (穹顶 + 太阳能板, 与老城堡同包络)
      int cx = (g.flagCol+3)*TILE_SIZE - camX, cy = 16*TILE_SIZE - camY;
      SDL_SetRenderDrawColor(ren, 150, 165, 190, 255);
      SDL_Rect cb{cx-30, cy-90, 110, 90}; SDL_RenderFillRect(ren, &cb);
      SDL_SetRenderDrawColor(ren, 110, 125, 155, 255);
      SDL_Rect dome{cx-18, cy-108, 86, 20}; SDL_RenderFillRect(ren, &dome);
      // 太阳能板
      SDL_SetRenderDrawColor(ren, 40, 60, 120, 255);
      for (int i = 0; i < 4; ++i) { SDL_Rect sp{cx-30+i*30, cy-104, 20, 12}; SDL_RenderFillRect(ren, &sp); }
      SDL_SetRenderDrawColor(ren, 90, 200, 255, 255);
      for (int i = 0; i < 4; ++i) { SDL_RenderDrawLine(ren, cx-30+i*30, cy-98, cx-10+i*30, cy-98); }
      // 舷窗
      SDL_Rect win1{cx-14, cy-70, 14, 14}, win2{cx+50, cy-70, 14, 14};
      SDL_SetRenderDrawColor(ren, 90, 230, 255, 255);
      SDL_RenderFillRect(ren, &win1); SDL_RenderFillRect(ren, &win2);
      SDL_SetRenderDrawColor(ren, 230, 250, 255, 255);
      SDL_Rect wg1{cx-14, cy-70, 14, 3}, wg2{cx+50, cy-70, 14, 3};
      SDL_RenderFillRect(ren, &wg1); SDL_RenderFillRect(ren, &wg2);
      // 气闸门
      SDL_SetRenderDrawColor(ren, 45, 55, 80, 255);
      SDL_Rect door{cx+12, cy-44, 26, 44}; SDL_RenderFillRect(ren, &door);
      SDL_SetRenderDrawColor(ren, 90, 230, 255, 255); SDL_RenderDrawRect(ren, &door);
      SDL_SetRenderDrawColor(ren, 255, 170, 60, 255);
      SDL_Rect lamp{cx+22, cy-40, 6, 6}; SDL_RenderFillRect(ren, &lamp);
      // 天线
      SDL_SetRenderDrawColor(ren, 185, 195, 215, 255);
      SDL_RenderDrawLine(ren, cx+25, cy-108, cx+25, cy-126);
      SDL_SetRenderDrawColor(ren, 255, 120, 80, 255);
      SDL_Rect tip{cx+23, cy-130, 5, 5}; SDL_RenderFillRect(ren, &tip);
      // 检查点信号塔 (跟随配表 checkpointCol+1, 逻辑不变)
      if (true) {
        int cpCol = levelCheckpointCol(app.currentLevelIndex);
        int qx = (cpCol+1)*TILE_SIZE - camX, qy = 16*TILE_SIZE - camY;
        SDL_SetRenderDrawColor(ren, 120, 130, 155, 255);
        SDL_Rect qp{qx, qy-110, 4, 110}; SDL_RenderFillRect(ren, &qp);
        if (g.checkpointOn) {
          int bl = (int)(180 + 75 * sinf(globalT * 6.0f));
          SDL_SetRenderDrawColor(ren, 80, bl, 220, 255);
        }
        else SDL_SetRenderDrawColor(ren, 110, 120, 140, 255);
        SDL_Rect qf{qx+4, qy-110, 30, 20}; SDL_RenderFillRect(ren, &qf);
        if (g.checkpointOn) {
          SDL_SetRenderDrawColor(ren, 230, 255, 255, 255);
          SDL_Rect sig{qx+8, qy-106, 12, 3}; SDL_RenderFillRect(ren, &sig);
          SDL_Rect sig2{qx+8, qy-100, 8, 2}; SDL_RenderFillRect(ren, &sig2);
        } else {
          SDL_SetRenderDrawColor(ren, 70, 78, 95, 255); SDL_RenderDrawRect(ren, &qf);
        }
      }
    }
    // 晶簇 (地面装饰, 位置逻辑不变)
    for (int c = c0; c <= c1; ++c) {
      if (getT(g,c,17)==T_GROUND && getT(g,c,16)!=T_SPIKE && c%7==3) {
        drawBush(ren, c*TILE_SIZE-camX, 17*TILE_SIZE-camY, 40);
      }
    }
    // 悬浮平台 (磁悬浮甲板)
    for (auto &p : g.plats) {
      SDL_Rect rc{(int)(p.pos.x-camX), (int)(p.pos.y-camY), (int)p.size.x, (int)p.size.y};
      SDL_SetRenderDrawColor(ren, 52, 68, 102, 255); SDL_RenderFillRect(ren, &rc);
      SDL_SetRenderDrawColor(ren, 30, 40, 70, 255); SDL_RenderDrawRect(ren, &rc);
      SDL_SetRenderDrawColor(ren, 80, 235, 255, 255);
      SDL_Rect hi{rc.x+3, rc.y+2, rc.w-6, 4}; SDL_RenderFillRect(ren, &hi);
      // 底部推进辉光 (呼吸)
      int th = (int)(140 + 80 * sinf(globalT * 8.0f + p.pos.x * 0.05f));
      SDL_SetRenderDrawColor(ren, 255, 150, 60, 255);
      SDL_Rect t1{rc.x+8, rc.y+rc.h-3, 10, 3}, t2{rc.x+rc.w-18, rc.y+rc.h-3, 10, 3};
      SDL_RenderFillRect(ren, &t1); SDL_RenderFillRect(ren, &t2);
      (void)th;
      SDL_SetRenderDrawColor(ren, 160, 175, 200, 255);
      SDL_Rect riv{rc.x+rc.w/2-2, rc.y+8, 4, 4}; SDL_RenderFillRect(ren, &riv);
    }
    // 星能碎片 (旋转闪烁, 逻辑不变)
    for (auto &cn : g.coins) {
      if (cn.taken) continue;
      int sx = (int)(cn.pos.x - camX), sy = (int)(cn.pos.y + sinf(cn.anim*3)*3 - camY);
      if (sx < -40 || sx > WINDOW_WIDTH+40) continue;
      float sq = fabsf(sinf(cn.anim*4));
      int w = 6 + (int)(sq*16);
      // 外辉光
      SDL_SetRenderDrawColor(ren, 30, 120, 150, 255);
      SDL_Rect glow{sx+14-w/2-2, sy-2, w+4, 30}; SDL_RenderFillRect(ren, &glow);
      SDL_SetRenderDrawColor(ren, 70, 220, 255, 255);
      SDL_Rect coin{sx+14-w/2, sy, w, 26}; SDL_RenderFillRect(ren, &coin);
      SDL_SetRenderDrawColor(ren, 230, 255, 255, 255);
      SDL_Rect hi{sx+14-w/2+3, sy+3, std::max(2,w-6), 5}; SDL_RenderFillRect(ren, &hi);
      SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
      SDL_Rect core{sx+12, sy+11, 4, 4}; SDL_RenderFillRect(ren, &core);
    }
    // 蘑菇/燃料电池机制已移除 (顶撞 ! 方块改为随机奖励, 无掉落物)
    // 敌人
    for (auto &e : g.foes) {
      if (!e.alive && e.squashT <= 0) continue;
      int sx=(int)(e.pos.x-camX), sy=(int)(e.pos.y-camY);
      if (sx < -60 || sx > WINDOW_WIDTH+60) continue;
      drawGoomba(ren, e, sx, sy, globalT);
    }
    // 预瞄虚线 (仅按住 E 时显示, 松开即消失; 遇方块即止)
    if (state == ST_PLAY && !paused && pl.firing) {
      drawTrajectory(ren, g, pl, camX, camY);
    }
    // 子弹
    for (auto &b : g.bullets) {
      if (b.dead) continue;
      drawBullet(ren, b, camX, camY);
    }
    // 玩家
    if (state != ST_GAMEOVER) {
      int sx=(int)(pl.pos.x-camX), sy=(int)(pl.pos.y-camY);
      drawPlayer(ren, pl, sx, sy, globalT);
    }
    // 粒子 (颜色由逻辑事件决定, 此处原样绘制)
    for (auto &pt : g.parts) {
      float a = pt.life / pt.maxLife;
      SDL_SetRenderDrawColor(ren, pt.col.r, pt.col.g, pt.col.b, (Uint8)(255*a));
      SDL_Rect rc{(int)(pt.pos.x-camX), (int)(pt.pos.y-camY), pt.size, pt.size};
      SDL_RenderFillRect(ren, &rc);
    }
    // 弹窗分数 (原样绘制)
    for (auto &pp : g.popups) {
      int sx=(int)(pp.pos.x-camX), sy=(int)(pp.pos.y-camY);
      drawText(ren, pp.txt, sx, sy, 2, pp.col);
    }
  }

  // ---- HUD (深空 navy + 青色分隔) ----
  SDL_SetRenderDrawColor(ren, 10, 16, 38, 255);
  SDL_Rect hud{0, 0, WINDOW_WIDTH, HUD_HEIGHT}; SDL_RenderFillRect(ren, &hud);
  SDL_SetRenderDrawColor(ren, 70, 220, 255, 255);
  SDL_RenderDrawLine(ren, 0, HUD_HEIGHT, WINDOW_WIDTH, HUD_HEIGHT);
  {
    char buf[64];
    // 左上角心数 (初始 3 颗, 怪物攻击 -1)
    {
      int hp = std::max(0, g.lives);
      char stars[4] = {' ', ' ', ' ', '\0'};
      for (int i = 0; i < hp && i < 3; ++i) stars[i] = '*';
      snprintf(buf, sizeof(buf), "HP %s", stars);
      SDL_Color hc = hp >= 3 ? SDL_Color{140,255,180,255}
                   : hp == 2 ? SDL_Color{255,230,130,255}
                             : SDL_Color{255,120,120,255};
      drawText(ren, buf, 12, 6, 2, hc);
    }
    snprintf(buf, sizeof(buf), "SCORE %06d", g.score);
    drawText(ren, buf, 130, 6, 2, {255,255,255,255});
    snprintf(buf, sizeof(buf), "SHARDS X%02d", g.coinCount);
    drawText(ren, buf, 340, 6, 2, {120,240,255,255});
    snprintf(buf, sizeof(buf), "SECTOR 1-%d", app.currentLevelIndex + 1);
    drawText(ren, buf, 540, 6, 2, {150,220,255,255});
    snprintf(buf, sizeof(buf), "TIME %03d", (int)ceil(g.timeLeft));
    SDL_Color tc = g.timeLeft < 60 ? SDL_Color{255,120,120,255} : SDL_Color{255,255,255,255};
    drawText(ren, buf, 720, 6, 2, tc);
    snprintf(buf, sizeof(buf), "SHD X%d", std::max(0, pl.shieldCharges));
    drawText(ren, buf, 860, 6, 2, {90,235,255,255});
    // 第二行小提示
    SDL_Color dim{130,150,180,255};
    drawText(ren, "Q SHIELD  E FIRE  I/K AIM  SPACE JUMP", 12, 26, 1, dim);
    if (SFX.muted) drawText(ren, "MUTED", WINDOW_WIDTH-70, 26, 1, {255,120,120,255});
  }

  // ---- 状态覆盖层 ----
  if (state == ST_TITLE) {
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 160);
    SDL_Rect o{0,0,WINDOW_WIDTH,WINDOW_HEIGHT}; SDL_RenderFillRect(ren, &o);
    drawTextCenter(ren, "SPACE JUMPER", WINDOW_WIDTH/2, 130, 6, {120,240,255,255});
    drawTextCenter(ren, "TAI KONG TIAO YUE", WINDOW_WIDTH/2, 180, 2, {170,200,255,255});
    // 演示宇航员跳跃
    {
      Player demo; demo.pos = {0,0}; demo.size={24,40}; demo.face=1;
      int bx = WINDOW_WIDTH/2 - 60, by = 300;
      int hop = (int)(fabsf(sinf(globalT*3))*40);
      drawPlayer(ren, demo, bx, by - hop, globalT);
      // 月面演示台
      SDL_SetRenderDrawColor(ren, 48, 58, 82, 255);
      SDL_Rect gr{bx-80, by+42, 220, 14}; SDL_RenderFillRect(ren, &gr);
      SDL_SetRenderDrawColor(ren, 55, 225, 255, 255);
      SDL_Rect strip{bx-80, by+42, 220, 3}; SDL_RenderFillRect(ren, &strip);
    }
    drawTextCenter(ren, "A/D MOVE  SPACE JUMP  SHIFT BOOST", WINDOW_WIDTH/2, 380, 2, {255,255,255,255});
    drawTextCenter(ren, "E FIRE  I/K AIM  Q SHIELD  REACH BEACON", WINDOW_WIDTH/2, 404, 2, {170,240,255,255});
    drawTextCenter(ren, "3 HEARTS  SHIELD X2  FOES X2 HITS", WINDOW_WIDTH/2, 428, 2, {255,220,150,255});
    if ((int)(globalT*2)%2==0)
      drawTextCenter(ren, "PRESS ENTER TO START", WINDOW_WIDTH/2, 480, 3, {255,255,255,255});
    drawTextCenter(ren, "M MUTE  R RESTART", WINDOW_WIDTH/2, 530, 1, {150,150,150,255});
  } else if (paused) {
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 150);
    SDL_Rect o{0,0,WINDOW_WIDTH,WINDOW_HEIGHT}; SDL_RenderFillRect(ren, &o);
    drawTextCenter(ren, "PAUSED", WINDOW_WIDTH/2, 260, 5, {255,255,255,255});
    drawTextCenter(ren, "PRESS P TO RESUME", WINDOW_WIDTH/2, 320, 2, {200,200,200,255});
  } else if (state == ST_DYING) {
    // 红闪 (通用受击反馈, 非马里奥专属)
    if (g.stateT < 0.2f) {
      SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
      SDL_SetRenderDrawColor(ren, 255, 0, 0, 90);
      SDL_Rect o{0,0,WINDOW_WIDTH,WINDOW_HEIGHT}; SDL_RenderFillRect(ren, &o);
      SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    }
  } else if (state == ST_NEXTLEVEL) {
    // 切关过渡: 世界定格为背景, 中央提示下一关 (2 秒倒计时, 逻辑不变)
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 130);
    SDL_Rect o{0,0,WINDOW_WIDTH,WINDOW_HEIGHT}; SDL_RenderFillRect(ren, &o);
    drawTextCenter(ren, "LOAD NEXT SECTOR", WINDOW_WIDTH/2, 220, 4, {120,240,255,255});
    {
      char buf[64];
      snprintf(buf, sizeof(buf), "SECTOR %d", app.currentLevelIndex + 1);
      drawTextCenter(ren, buf, WINDOW_WIDTH/2, 280, 3, {150,220,255,255});
      float remain = 2.0f - g.stateT;
      if (remain < 0) remain = 0;
      snprintf(buf, sizeof(buf), "START IN %d", (int)ceil(remain));
      drawTextCenter(ren, buf, WINDOW_WIDTH/2, 330, 2, {255,255,255,255});
    }
  } else if (state == ST_WIN) {
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 170);
    SDL_Rect o{0,0,WINDOW_WIDTH,WINDOW_HEIGHT}; SDL_RenderFillRect(ren, &o);
    drawTextCenter(ren, "YOU WIN!", WINDOW_WIDTH/2, 150, 6, {140,255,200,255});
    char buf[64];
    snprintf(buf, sizeof(buf), "CLEAR ALL %d SECTORS!", levelCount());
    drawTextCenter(ren, buf, WINDOW_WIDTH/2, 205, 2, {150,220,255,255});
    snprintf(buf, sizeof(buf), "SCORE %06d", g.score);
    drawTextCenter(ren, buf, WINDOW_WIDTH/2, 240, 3, {120,240,255,255});
    snprintf(buf, sizeof(buf), "SHARDS X%d  TIME %d", g.coinCount, (int)g.timeLeft);
    drawTextCenter(ren, buf, WINDOW_WIDTH/2, 280, 2, {255,255,255,255});
    drawTextCenter(ren, "PRESS ENTER TO PLAY AGAIN", WINDOW_WIDTH/2, 340, 2, {200,220,255,255});
  } else if (state == ST_GAMEOVER) {
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 180);
    SDL_Rect o{0,0,WINDOW_WIDTH,WINDOW_HEIGHT}; SDL_RenderFillRect(ren, &o);
    drawTextCenter(ren, "GAME OVER", WINDOW_WIDTH/2, 200, 6, {255,100,120,255});
    char buf[64]; snprintf(buf, sizeof(buf), "SCORE %06d", g.score);
    drawTextCenter(ren, buf, WINDOW_WIDTH/2, 290, 3, {255,255,255,255});
    drawTextCenter(ren, "PRESS ENTER TO RETRY", WINDOW_WIDTH/2, 350, 2, {200,200,200,255});
  }
}
