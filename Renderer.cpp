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

void drawCloud(SDL_Renderer *r, int x, int y, float s) {
  SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
  SDL_Rect a{x, y, int(46*s), int(18*s)}, b{x+int(10*s), y-int(10*s), int(26*s), int(18*s)}, c{x+int(30*s), y-int(4*s), int(24*s), int(14*s)};
  SDL_RenderFillRect(r, &a); SDL_RenderFillRect(r, &b); SDL_RenderFillRect(r, &c);
}

void drawBush(SDL_Renderer *r, int x, int y, int w) {
  SDL_SetRenderDrawColor(r, 60, 180, 80, 255);
  for (int i = 0; i < w; i += 16) {
    SDL_Rect rc{x+i, y - (i%32==0?10:0), 18, 18};
    SDL_RenderFillRect(r, &rc);
  }
}

void renderTile(SDL_Renderer *r, int tile, int sx, int sy, float pulse, float bumpDy) {
  int y = sy + (int)bumpDy;
  SDL_Rect rc{sx, y, TILE_SIZE, TILE_SIZE};
  if (tile == T_GROUND) {
    SDL_SetRenderDrawColor(r, 196, 110, 40, 255); SDL_RenderFillRect(r, &rc);
    SDL_SetRenderDrawColor(r, 70, 190, 70, 255);
    SDL_Rect grass{sx, y, TILE_SIZE, 9}; SDL_RenderFillRect(r, &grass);
    SDL_SetRenderDrawColor(r, 40, 130, 40, 255);
    SDL_Rect gl{sx, y+9, TILE_SIZE, 2}; SDL_RenderFillRect(r, &gl);
    SDL_SetRenderDrawColor(r, 150, 80, 25, 255);
    SDL_Rect d1{sx+6, y+16, 5, 5}, d2{sx+20, y+22, 6, 4}, d3{sx+12, y+24, 4, 4};
    SDL_RenderFillRect(r, &d1); SDL_RenderFillRect(r, &d2); SDL_RenderFillRect(r, &d3);
  } else if (tile == T_BRICK) {
    SDL_SetRenderDrawColor(r, 200, 90, 30, 255); SDL_RenderFillRect(r, &rc);
    SDL_SetRenderDrawColor(r, 120, 45, 15, 255);
    for (int i = 1; i < 4; ++i) { SDL_RenderDrawLine(r, sx, y+i*8, sx+TILE_SIZE, y+i*8); }
    SDL_RenderDrawLine(r, sx+16, y, sx+16, y+8); SDL_RenderDrawLine(r, sx+8, y+8, sx+8, y+16);
    SDL_RenderDrawLine(r, sx+24, y+8, sx+24, y+16); SDL_RenderDrawLine(r, sx+16, y+16, sx+16, y+24);
    SDL_RenderDrawLine(r, sx+8, y+24, sx+8, y+TILE_SIZE); SDL_RenderDrawLine(r, sx+24, y+24, sx+24, y+TILE_SIZE);
  } else if (tile == T_QUESTION) {
    int glow = (int)(200 + 55 * sinf(pulse * 4.0f));
    SDL_SetRenderDrawColor(r, 250, glow, 30, 255); SDL_RenderFillRect(r, &rc);
    SDL_SetRenderDrawColor(r, 140, 70, 0, 255); SDL_RenderDrawRect(r, &rc);
    SDL_Rect in{sx+3, y+3, TILE_SIZE-6, TILE_SIZE-6};
    SDL_SetRenderDrawColor(r, 180, 100, 10, 255); SDL_RenderDrawRect(r, &in);
    drawText(r, "?", sx+9, y+6, 2, {255,255,255,255});
  } else if (tile == T_USED) {
    SDL_SetRenderDrawColor(r, 150, 100, 55, 255); SDL_RenderFillRect(r, &rc);
    SDL_SetRenderDrawColor(r, 100, 65, 30, 255); SDL_RenderDrawRect(r, &rc);
    SDL_SetRenderDrawColor(r, 110, 72, 35, 255);
    for (int i = 0; i < 3; ++i) { SDL_Rect riv{sx+5, y+6+i*8, TILE_SIZE-10, 3}; SDL_RenderFillRect(r, &riv); }
  } else if (tile == T_STONE) {
    SDL_SetRenderDrawColor(r, 130, 130, 140, 255); SDL_RenderFillRect(r, &rc);
    SDL_SetRenderDrawColor(r, 95, 95, 105, 255);
    SDL_Rect t{sx, y, TILE_SIZE, 4}; SDL_RenderFillRect(r, &t);
    SDL_SetRenderDrawColor(r, 160, 160, 175, 255);
    SDL_Rect h{sx+3, y+3, TILE_SIZE-6, 2}; SDL_RenderFillRect(r, &h);
    SDL_SetRenderDrawColor(r, 80, 80, 90, 255); SDL_RenderDrawRect(r, &rc);
  } else if (tile == T_FLAG) {
    SDL_SetRenderDrawColor(r, 30, 160, 50, 255);
    SDL_Rect pole{sx+13, y, 6, TILE_SIZE}; SDL_RenderFillRect(r, &pole);
    SDL_SetRenderDrawColor(r, 20, 110, 35, 255);
    SDL_RenderDrawLine(r, sx+13, y, sx+13, y+TILE_SIZE);
  } else if (tile == T_SPIKE) {
    SDL_SetRenderDrawColor(r, 90, 90, 100, 255);
    SDL_Rect base{sx, y+22, TILE_SIZE, 10}; SDL_RenderFillRect(r, &base);
    SDL_SetRenderDrawColor(r, 200, 200, 210, 255);
    for (int i = 0; i < 4; ++i) {
      int x0 = sx + i*8;
      SDL_RenderDrawLine(r, x0, y+22, x0+4, y+8);
      SDL_RenderDrawLine(r, x0+4, y+8, x0+8, y+22);
    }
    SDL_SetRenderDrawColor(r, 255, 80, 80, 255);
    SDL_Rect w{sx, y+22, TILE_SIZE, 2}; SDL_RenderFillRect(r, &w);
  }
}

void drawPlayer(SDL_Renderer *r, const Player &p, int sx, int sy, float time) {
  int w = (int)p.size.x, h = (int)p.size.y;
  float sq = p.squash;
  int dw = w + (int)(sq * 8), dh = h - (int)(sq * 10);
  int dx = sx - (dw - w) / 2, dy = sy + (h - dh);
  bool running = fabsf(p.vel.x) > 30 && p.grounded;
  int legF = running ? (int)(time * 12) % 2 : 0;

  // 无敌闪烁
  if (p.invincible > 0 && ((int)(time * 14) % 2 == 0)) return;

  // 腿
  SDL_SetRenderDrawColor(r, 30, 60, 180, 255);
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
  // 身体(背带裤)
  SDL_SetRenderDrawColor(r, 30, 90, 220, 255);
  SDL_Rect body{dx+2, dy+14, dw-4, dh-22};
  SDL_RenderFillRect(r, &body);
  SDL_SetRenderDrawColor(r, 250, 200, 40, 255);
  SDL_Rect btn{dx+dw/2-2, dy+20, 4, 4}; SDL_RenderFillRect(r, &btn);
  // 头+帽子
  SDL_SetRenderDrawColor(r, 255, 200, 150, 255);
  SDL_Rect head{dx+2, dy, dw-4, 15}; SDL_RenderFillRect(r, &head);
  SDL_SetRenderDrawColor(r, 230, 30, 30, 255);
  SDL_Rect cap{dx, dy-3, dw, 7}; SDL_RenderFillRect(r, &cap);
  SDL_Rect brim{p.face > 0 ? dx+dw-8 : dx, dy+3, 8, 3};
  SDL_SetRenderDrawColor(r, 200, 20, 20, 255); SDL_RenderFillRect(r, &brim);
  // 眼睛
  SDL_SetRenderDrawColor(r, 20, 20, 20, 255);
  int ex = p.face > 0 ? dx+dw-8 : dx+4;
  SDL_Rect eye{ex, dy+6, 4, 6}; SDL_RenderFillRect(r, &eye);
  // 变大形态: 金色描边
  if (p.big) {
    SDL_SetRenderDrawColor(r, 255, 215, 60, 255);
    SDL_Rect o{dx-1, dy-4, dw+2, dh+5};
    SDL_RenderDrawRect(r, &o);
  }
}

void drawGoomba(SDL_Renderer *r, const Goomba &e, int sx, int sy, float time) {
  int w = (int)e.size.x, h = (int)e.size.y;
  if (e.squashT > 0) { // 被踩扁
    SDL_SetRenderDrawColor(r, 140, 70, 20, 255);
    SDL_Rect flat{sx, sy+h-10, w, 10}; SDL_RenderFillRect(r, &flat);
    return;
  }
  float bob = sinf(time * 10 + e.phase) * 1.5f;
  if (e.flyer) SDL_SetRenderDrawColor(r, 150, 80, 200, 255);
  else SDL_SetRenderDrawColor(r, 165, 85, 30, 255);
  SDL_Rect body{sx, sy+(int)bob, w, h-6}; SDL_RenderFillRect(r, &body);
  // 脚
  int step = (int)(time * 8 + e.phase) % 2;
  SDL_SetRenderDrawColor(r, 60, 30, 10, 255);
  SDL_Rect f1{sx+1+(step?2:0), sy+h-6, 10, 6}, f2{sx+w-11-(step?2:0), sy+h-6, 10, 6};
  SDL_RenderFillRect(r, &f1); SDL_RenderFillRect(r, &f2);
  // 怒眼 + 眉
  SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
  SDL_Rect e1{sx+4, sy+8+(int)bob, 7, 8}, e2{sx+w-11, sy+8+(int)bob, 7, 8};
  SDL_RenderFillRect(r, &e1); SDL_RenderFillRect(r, &e2);
  SDL_SetRenderDrawColor(r, 20, 20, 20, 255);
  SDL_Rect p1{sx+6, sy+12+(int)bob, 3, 4}, p2{sx+w-9, sy+12+(int)bob, 3, 4};
  SDL_RenderFillRect(r, &p1); SDL_RenderFillRect(r, &p2);
  SDL_RenderDrawLine(r, sx+3, sy+6+(int)bob, sx+11, sy+8+(int)bob);
  SDL_RenderDrawLine(r, sx+w-3, sy+6+(int)bob, sx+w-11, sy+8+(int)bob);
  if (e.flyer) { // 小翅膀
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    int flap = (int)(sinf(time*16)*4);
    SDL_Rect w1{sx-8, sy+4+flap, 10, 6}, w2{sx+w-2, sy+4-flap, 10, 6};
    SDL_RenderFillRect(r, &w1); SDL_RenderFillRect(r, &w2);
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

  // 天空渐变
  for (int y = 0; y < WINDOW_HEIGHT; y += 4) {
    int t = y * 255 / WINDOW_HEIGHT;
    SDL_SetRenderDrawColor(ren, 90 - t/8, 150 + t/10, 250, 255);
    SDL_Rect rc{0, y, WINDOW_WIDTH, 4}; SDL_RenderFillRect(ren, &rc);
  }
  // 太阳
  SDL_SetRenderDrawColor(ren, 255, 240, 150, 255);
  SDL_Rect sun{WINDOW_WIDTH-120, 60, 56, 56}; SDL_RenderFillRect(ren, &sun);
  SDL_SetRenderDrawColor(ren, 255, 250, 200, 255);
  SDL_Rect sun2{WINDOW_WIDTH-114, 66, 44, 44}; SDL_RenderFillRect(ren, &sun2);

  // 远山 (视差 0.3)
  SDL_SetRenderDrawColor(ren, 110, 160, 220, 255);
  for (int i = -1; i < 12; ++i) {
    int bx = (int)(i * 340 - camX * 0.3f) % (WORLD_WIDTH) ;
    int mx = bx; if (mx < -200) mx += 4000;
    int base = WINDOW_HEIGHT - 90 - HUD_HEIGHT + camY/4;
    SDL_RenderDrawLine(ren, mx, base, mx+90, base-110);
    SDL_RenderDrawLine(ren, mx+90, base-110, mx+180, base);
  }
  // 近山丘 (视差 0.6)
  for (int i = 0; i < 20; ++i) {
    int hx = (int)(i * 420 - camX * 0.6f);
    if (hx < -120 || hx > WINDOW_WIDTH+120) continue;
    SDL_SetRenderDrawColor(ren, 80, 190, 100, 255);
    SDL_Rect h{hx, WINDOW_HEIGHT-110+HUD_HEIGHT/2 + camY/3, 110, 60};
    SDL_RenderFillRect(ren, &h);
  }
  // 云 (视差 0.5 + 自身漂移)
  for (auto &c : g.clouds) {
    int sx = (int)(c.x - camX * 0.5f);
    if (sx < -120 || sx > WINDOW_WIDTH+120) continue;
    drawCloud(ren, sx % (WINDOW_WIDTH+240) < 0 ? sx + WINDOW_WIDTH+240 : sx, (int)c.y + 30, c.s);
  }

  if (state != ST_TITLE) {
    // bump 偏移查询
    auto bumpDy = [&](int c, int r)->float {
      for (auto &b : g.bumps) if (b.c==c && b.r==r) return -sinf((0.22f-b.t)/0.22f*3.14159f)*10.0f;
      return 0;
    };
    // 瓦片 (剔除)
    int c0 = std::max(0, camX / TILE_SIZE - 1), c1 = std::min(MAP_COLS-1, (camX+WINDOW_WIDTH)/TILE_SIZE + 1);
    for (int r = 0; r < MAP_ROWS; ++r) for (int c = c0; c <= c1; ++c) {
      int t = getT(g, c, r);
      if (t == T_EMPTY) continue;
      if (t == T_FLAG) continue; // 旗杆统一绘制
      int sx = c*TILE_SIZE - camX, sy = r*TILE_SIZE - camY;
      renderTile(ren, t, sx, sy, globalT + c*0.3f, bumpDy(c, r));
    }
    // 旗杆 + 旗帜 + 城堡
    {
      int fx = g.flagCol*TILE_SIZE - camX;
      SDL_SetRenderDrawColor(ren, 30, 160, 50, 255);
      SDL_Rect pole{fx+13, 5*TILE_SIZE-camY, 6, 12*TILE_SIZE};
      SDL_RenderFillRect(ren, &pole);
      SDL_SetRenderDrawColor(ren, 240, 240, 240, 255);
      SDL_Rect ball{fx+8, 5*TILE_SIZE-10-camY, 16, 16}; SDL_RenderFillRect(ren, &ball);
      float wv = sinf(globalT*6)*4;
      int fy = 5*TILE_SIZE+20 - camY + ((state==ST_WINANIM || state==ST_NEXTLEVEL) ? (int)(g.stateT*60) : 0);
      if (fy > 12*TILE_SIZE-camY) fy = 12*TILE_SIZE-camY;
      SDL_SetRenderDrawColor(ren, 40, 200, 60, 255);
      SDL_Rect flag{fx-44, fy, 50, 32}; SDL_RenderFillRect(ren, &flag);
      SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
      drawText(ren, "*", fx-28, fy+2, 2, {255,255,255,255});
      (void)wv;
      // 城堡 (跟随配表 flagCol+3, 关卡1即 120, 与老逻辑一致)
      int cx = (g.flagCol+3)*TILE_SIZE - camX, cy = 16*TILE_SIZE - camY;
      SDL_SetRenderDrawColor(ren, 200, 120, 60, 255);
      SDL_Rect cb{cx-30, cy-90, 110, 90}; SDL_RenderFillRect(ren, &cb);
      SDL_SetRenderDrawColor(ren, 160, 90, 40, 255);
      for (int i = 0; i < 4; ++i) { SDL_Rect mer{cx-30+i*30, cy-104, 20, 16}; SDL_RenderFillRect(ren, &mer); }
      SDL_SetRenderDrawColor(ren, 90, 50, 20, 255);
      SDL_Rect door{cx+12, cy-44, 26, 44}; SDL_RenderFillRect(ren, &door);
      SDL_Rect win1{cx-14, cy-70, 14, 14}, win2{cx+50, cy-70, 14, 14};
      SDL_SetRenderDrawColor(ren, 200, 230, 255, 255);
      SDL_RenderFillRect(ren, &win1); SDL_RenderFillRect(ren, &win2);
      // 检查点小旗 (跟随配表 checkpointCol+1, 关卡1即 74, 与老逻辑一致)
      if (true) {
        int cpCol = levelCheckpointCol(app.currentLevelIndex);
        int qx = (cpCol+1)*TILE_SIZE - camX, qy = 16*TILE_SIZE - camY;
        SDL_SetRenderDrawColor(ren, 120, 120, 120, 255);
        SDL_Rect qp{qx, qy-110, 4, 110}; SDL_RenderFillRect(ren, &qp);
        if (g.checkpointOn) SDL_SetRenderDrawColor(ren, 60, 220, 60, 255);
        else SDL_SetRenderDrawColor(ren, 180, 180, 180, 255);
        SDL_Rect qf{qx+4, qy-110, 30, 20}; SDL_RenderFillRect(ren, &qf);
      }
    }
    // 灌木 (地面装饰)
    for (int c = c0; c <= c1; ++c) {
      if (getT(g,c,17)==T_GROUND && getT(g,c,16)!=T_SPIKE && c%7==3) {
        drawBush(ren, c*TILE_SIZE-camX, 17*TILE_SIZE-camY, 40);
      }
    }
    // 移动平台
    for (auto &p : g.plats) {
      SDL_Rect rc{(int)(p.pos.x-camX), (int)(p.pos.y-camY), (int)p.size.x, (int)p.size.y};
      SDL_SetRenderDrawColor(ren, 230, 150, 50, 255); SDL_RenderFillRect(ren, &rc);
      SDL_SetRenderDrawColor(ren, 150, 90, 20, 255); SDL_RenderDrawRect(ren, &rc);
      SDL_SetRenderDrawColor(ren, 255, 220, 150, 255);
      SDL_Rect hi{rc.x+3, rc.y+2, rc.w-6, 4}; SDL_RenderFillRect(ren, &hi);
    }
    // 金币 (旋转闪烁)
    for (auto &cn : g.coins) {
      if (cn.taken) continue;
      int sx = (int)(cn.pos.x - camX), sy = (int)(cn.pos.y + sinf(cn.anim*3)*3 - camY);
      if (sx < -40 || sx > WINDOW_WIDTH+40) continue;
      float sq = fabsf(sinf(cn.anim*4));
      int w = 6 + (int)(sq*16);
      SDL_SetRenderDrawColor(ren, 255, 200, 30, 255);
      SDL_Rect coin{sx+14-w/2, sy, w, 26}; SDL_RenderFillRect(ren, &coin);
      SDL_SetRenderDrawColor(ren, 255, 245, 160, 255);
      SDL_Rect hi{sx+14-w/2+3, sy+3, std::max(2,w-6), 5}; SDL_RenderFillRect(ren, &hi);
    }
    // 蘑菇
    for (auto &s : g.shrooms) {
      if (!s.active || s.dead) continue;
      int sx=(int)(s.pos.x-camX), sy=(int)(s.pos.y-camY);
      SDL_SetRenderDrawColor(ren, 240, 60, 60, 255);
      SDL_Rect cap{sx, sy, 28, 16}; SDL_RenderFillRect(ren, &cap);
      SDL_SetRenderDrawColor(ren, 255,255,255,255);
      SDL_Rect d1{sx+4,sy+3,6,6}, d2{sx+18,sy+3,6,6}; SDL_RenderFillRect(ren,&d1); SDL_RenderFillRect(ren,&d2);
      SDL_SetRenderDrawColor(ren, 255, 220, 180, 255);
      SDL_Rect stem{sx+7, sy+16, 14, 12}; SDL_RenderFillRect(ren, &stem);
      SDL_SetRenderDrawColor(ren, 20,20,20,255);
      SDL_Rect e1{sx+9,sy+18,3,5}, e2{sx+16,sy+18,3,5}; SDL_RenderFillRect(ren,&e1); SDL_RenderFillRect(ren,&e2);
    }
    // 敌人
    for (auto &e : g.foes) {
      if (!e.alive && e.squashT <= 0) continue;
      int sx=(int)(e.pos.x-camX), sy=(int)(e.pos.y-camY);
      if (sx < -60 || sx > WINDOW_WIDTH+60) continue;
      drawGoomba(ren, e, sx, sy, globalT);
    }
    // 玩家
    if (state != ST_GAMEOVER) {
      int sx=(int)(pl.pos.x-camX), sy=(int)(pl.pos.y-camY);
      drawPlayer(ren, pl, sx, sy, globalT);
    }
    // 粒子
    for (auto &pt : g.parts) {
      float a = pt.life / pt.maxLife;
      SDL_SetRenderDrawColor(ren, pt.col.r, pt.col.g, pt.col.b, (Uint8)(255*a));
      SDL_Rect rc{(int)(pt.pos.x-camX), (int)(pt.pos.y-camY), pt.size, pt.size};
      SDL_RenderFillRect(ren, &rc);
    }
    // 弹窗分数
    for (auto &pp : g.popups) {
      int sx=(int)(pp.pos.x-camX), sy=(int)(pp.pos.y-camY);
      drawText(ren, pp.txt, sx, sy, 2, pp.col);
    }
  }

  // ---- HUD ----
  SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
  SDL_Rect hud{0, 0, WINDOW_WIDTH, HUD_HEIGHT}; SDL_RenderFillRect(ren, &hud);
  SDL_SetRenderDrawColor(ren, 255,255,255,255);
  SDL_RenderDrawLine(ren, 0, HUD_HEIGHT, WINDOW_WIDTH, HUD_HEIGHT);
  {
    char buf[64];
    snprintf(buf, sizeof(buf), "SCORE %06d", g.score);
    drawText(ren, buf, 12, 6, 2, {255,255,255,255});
    snprintf(buf, sizeof(buf), "COINS X%02d", g.coinCount);
    drawText(ren, buf, 250, 6, 2, {255,220,80,255});
    snprintf(buf, sizeof(buf), "WORLD 1-%d", app.currentLevelIndex + 1);
    drawText(ren, buf, 470, 6, 2, {150,220,255,255});
    snprintf(buf, sizeof(buf), "TIME %03d", (int)ceil(g.timeLeft));
    SDL_Color tc = g.timeLeft < 60 ? SDL_Color{255,80,80,255} : SDL_Color{255,255,255,255};
    drawText(ren, buf, 650, 6, 2, tc);
    snprintf(buf, sizeof(buf), "LIVES X%d", std::max(0, g.lives));
    drawText(ren, buf, 810, 6, 2, {140,255,140,255});
    // 第二行小提示
    SDL_Color dim{170,170,170,255};
    drawText(ren, pl.big ? "POWER BIG  SHIFT RUN  P PAUSE" : "SHIFT RUN  SPACE JUMP  P PAUSE", 12, 26, 1, dim);
    if (SFX.muted) drawText(ren, "MUTED", WINDOW_WIDTH-70, 26, 1, {255,120,120,255});
  }

  // ---- 状态覆盖层 ----
  if (state == ST_TITLE) {
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 160);
    SDL_Rect o{0,0,WINDOW_WIDTH,WINDOW_HEIGHT}; SDL_RenderFillRect(ren, &o);
    drawTextCenter(ren, "SUPER PLATFORMER", WINDOW_WIDTH/2, 130, 6, {255,220,60,255});
    drawTextCenter(ren, "CHAO JI PING TAI TIAO YUE", WINDOW_WIDTH/2, 180, 2, {180,220,255,255});
    // 演示小人跳
    {
      Player demo; demo.pos = {0,0}; demo.size={24,40}; demo.face=1;
      int bx = WINDOW_WIDTH/2 - 60, by = 300;
      int hop = (int)(fabsf(sinf(globalT*3))*40);
      drawPlayer(ren, demo, bx, by - hop, globalT);
      SDL_SetRenderDrawColor(ren, 70,190,70,255);
      SDL_Rect gr{bx-80, by+42, 220, 14}; SDL_RenderFillRect(ren, &gr);
    }
    drawTextCenter(ren, "A/D MOVE  SPACE JUMP  SHIFT RUN", WINDOW_WIDTH/2, 380, 2, {255,255,255,255});
    drawTextCenter(ren, "STOMP FOES  BUMP BLOCKS  REACH FLAG", WINDOW_WIDTH/2, 404, 2, {200,255,200,255});
    drawTextCenter(ren, "30 COINS = 1UP  MUSHROOM = GROW", WINDOW_WIDTH/2, 428, 2, {255,230,150,255});
    if ((int)(globalT*2)%2==0)
      drawTextCenter(ren, "PRESS ENTER TO START", WINDOW_WIDTH/2, 480, 3, {255,255,255,255});
    drawTextCenter(ren, "M MUTE  R RESTART", WINDOW_WIDTH/2, 530, 1, {150,150,150,255});
  } else if (paused) {
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 150);
    SDL_Rect o{0,0,WINDOW_WIDTH,WINDOW_HEIGHT}; SDL_RenderFillRect(ren, &o);
    drawTextCenter(ren, "PAUSED", WINDOW_WIDTH/2, 260, 5, {255,255,255,255});
    drawTextCenter(ren, "PRESS P TO RESUME", WINDOW_WIDTH/2, 320, 2, {200,200,200,255});
  } else if (state == ST_DYING) {
    // 红闪
    if (g.stateT < 0.2f) {
      SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
      SDL_SetRenderDrawColor(ren, 255, 0, 0, 90);
      SDL_Rect o{0,0,WINDOW_WIDTH,WINDOW_HEIGHT}; SDL_RenderFillRect(ren, &o);
      SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    }
  } else if (state == ST_NEXTLEVEL) {
    // 切关过渡: 世界定格为背景, 中央提示下一关 (2 秒倒计时)
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 130);
    SDL_Rect o{0,0,WINDOW_WIDTH,WINDOW_HEIGHT}; SDL_RenderFillRect(ren, &o);
    drawTextCenter(ren, "LOAD NEXT LEVEL", WINDOW_WIDTH/2, 220, 4, {255,230,90,255});
    {
      char buf[64];
      snprintf(buf, sizeof(buf), "LEVEL %d", app.currentLevelIndex + 1);
      drawTextCenter(ren, buf, WINDOW_WIDTH/2, 280, 3, {150,220,255,255});
      float remain = 2.0f - g.stateT;
      if (remain < 0) remain = 0;
      snprintf(buf, sizeof(buf), "START IN %d", (int)ceil(remain));
      drawTextCenter(ren, buf, WINDOW_WIDTH/2, 330, 2, {255,255,255,255});
    }
  } else if (state == ST_WIN) {
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 170);
    SDL_Rect o{0,0,WINDOW_WIDTH,WINDOW_HEIGHT}; SDL_RenderFillRect(ren, &o);
    drawTextCenter(ren, "YOU WIN!", WINDOW_WIDTH/2, 150, 6, {120,255,120,255});
    char buf[64];
    snprintf(buf, sizeof(buf), "CLEAR ALL %d LEVELS!", levelCount());
    drawTextCenter(ren, buf, WINDOW_WIDTH/2, 205, 2, {150,220,255,255});
    snprintf(buf, sizeof(buf), "SCORE %06d", g.score);
    drawTextCenter(ren, buf, WINDOW_WIDTH/2, 240, 3, {255,230,90,255});
    snprintf(buf, sizeof(buf), "COINS X%d  TIME %d", g.coinCount, (int)g.timeLeft);
    drawTextCenter(ren, buf, WINDOW_WIDTH/2, 280, 2, {255,255,255,255});
    drawTextCenter(ren, "PRESS ENTER TO PLAY AGAIN", WINDOW_WIDTH/2, 340, 2, {200,220,255,255});
  } else if (state == ST_GAMEOVER) {
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 180);
    SDL_Rect o{0,0,WINDOW_WIDTH,WINDOW_HEIGHT}; SDL_RenderFillRect(ren, &o);
    drawTextCenter(ren, "GAME OVER", WINDOW_WIDTH/2, 200, 6, {255,80,80,255});
    char buf[64]; snprintf(buf, sizeof(buf), "SCORE %06d", g.score);
    drawTextCenter(ren, buf, WINDOW_WIDTH/2, 290, 3, {255,255,255,255});
    drawTextCenter(ren, "PRESS ENTER TO RETRY", WINDOW_WIDTH/2, 350, 2, {200,200,200,255});
  }
}
