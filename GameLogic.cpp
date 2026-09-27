#include "GameLogic.hpp"
#include "Audio.hpp"
#include "Level.hpp"
#include "Physics.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

void App::resetRun() {
  if (currentLevelIndex < 0) currentLevelIndex = 0;
  if (currentLevelIndex >= levelCount()) currentLevelIndex = 0;
  game.score = 0; game.coinCount = 0; game.lives = 3; game.timeLeft = 300;
  buildLevel(game, currentLevelIndex);
  player = Player(); player.pos = game.spawn; player.size = {24, 40};
  cam = {0, 0}; state = ST_PLAY; paused = false;
  cpSaved = false;
  cpPos = {(float)levelCheckpointCol(currentLevelIndex) * TILE_SIZE, 12.0f * TILE_SIZE};
}

void App::resetToFirstLevel() {
  currentLevelIndex = 0;
  resetRun();
}

void App::loadLevelKeepScore(int idx) {
  if (idx < 0) idx = 0;
  if (idx >= levelCount()) idx = levelCount() - 1;
  currentLevelIndex = idx;
  // 切关: 保留分数/金币/命 (街机继承制), 时间回满, 检查点/玩家/相机重置
  int sc = game.score, cc = game.coinCount, lv = game.lives;
  buildLevel(game, currentLevelIndex);
  game.score = sc; game.coinCount = cc; game.lives = lv; game.timeLeft = 300;
  cpSaved = false;
  cpPos = {(float)levelCheckpointCol(currentLevelIndex) * TILE_SIZE, 12.0f * TILE_SIZE};
  player = Player(); player.pos = game.spawn; player.size = {24, 40};
  cam = {0, 0};
  paused = false;
  state = ST_PLAY;
}

void updateFx(Game &g, float DT) {
  // bump 动画计时
  for (auto it = g.bumps.begin(); it != g.bumps.end();) {
    it->t -= DT; if (it->t <= 0) it = g.bumps.erase(it); else ++it;
  }
  // 粒子 / 弹窗 (全状态更新, 标题屏也有动效)
  for (auto it = g.parts.begin(); it != g.parts.end();) {
    it->life -= DT;
    if (it->life <= 0) it = g.parts.erase(it);
    else { it->vel.y += it->grav * DT; it->pos += it->vel * DT; ++it; }
  }
  for (auto it = g.popups.begin(); it != g.popups.end();) {
    it->life -= DT; it->pos.y -= 40 * DT;
    if (it->life <= 0) it = g.popups.erase(it); else ++it;
  }
  for (auto &c : g.clouds) { c.x += c.v * DT; if (c.x > WORLD_WIDTH + 200) c.x = -200; }
}

// 新机制: 玩家受伤判定 (无无敌帧, 用位移分离避免同帧连续受伤)
// 返回 true 表示此次受伤致死 (心归零, 调用方负责切 ST_DYING)
static bool hurtPlayer(Game &g, Player &pl, float srcX) {
  // 盾牌优先: 举盾且有剩余格挡次数则消耗一次格挡
  if (pl.shieldUp && pl.shieldCharges > 0) {
    pl.shieldCharges--;
    if (pl.shieldCharges <= 0) {
      SFX.shieldBreak();
      g.popups.push_back({{pl.pos.x, pl.pos.y - 40}, "SHIELD BROKEN!", 1.4f, {255,120,120,255}});
    } else {
      SFX.shieldHit();
      char buf[32]; snprintf(buf, sizeof(buf), "BLOCKED! X%d", pl.shieldCharges);
      g.popups.push_back({{pl.pos.x, pl.pos.y - 40}, buf, 1.0f, {90,235,255,255}});
    }
    burst(g, {pl.pos.x + 12, pl.pos.y + 20}, 10, {90,235,255,255}, 180, 500, 3);
    // 推离伤害源 (无无敌帧, 靠位移分离)
    pl.pos.x += (pl.pos.x + pl.size.x / 2 < srcX) ? -14.0f : 14.0f;
    pl.vel.y = -260.0f;
    return false;
  }
  // 无盾: 掉 1 心
  g.lives--;
  SFX.hurt();
  burst(g, {pl.pos.x + 12, pl.pos.y + 20}, 10, {255,80,80,255}, 180, 500, 3);
  if (g.lives > 0) {
    char buf[32]; snprintf(buf, sizeof(buf), "-1 HEART X%d", g.lives);
    g.popups.push_back({{pl.pos.x, pl.pos.y - 40}, buf, 1.2f, {255,120,120,255}});
  }
  // 推离伤害源 (无无敌帧, 靠位移分离)
  pl.pos.x += (pl.pos.x + pl.size.x / 2 < srcX) ? -14.0f : 14.0f;
  pl.vel.y = -260.0f;
  return g.lives <= 0;
}

void updateFixedStep(App &app, float DT, const Uint8 *kb) {
  Game &g = app.game;
  Player &pl = app.player;
  gml::vec2 &cam = app.cam;
  State &state = app.state;
  bool smoke = app.smoke;

  app.globalT += DT;
  if (state == ST_PLAY && !app.paused) {
    // ---- 计时 ----
    g.timeLeft -= DT;
    if (g.timeLeft <= 0) { g.timeLeft = 0; SFX.die(); state = ST_DYING; g.stateT = 0; pl.dead = true; pl.vel = {0, -650}; app.respawnLevel = true; }

    // ---- 检查点 (x 超过本关中点即激活, 配表驱动) ----
    {
      int cpCol = levelCheckpointCol(app.currentLevelIndex);
      if (!app.cpSaved && pl.pos.x > cpCol * TILE_SIZE) {
        app.cpSaved = true; app.cpPos = {(float)cpCol * TILE_SIZE, 12.0f * TILE_SIZE};
      g.checkpointOn = true; g.checkpoint = app.cpPos;
      g.popups.push_back({{pl.pos.x, pl.pos.y - 30}, "CHECKPOINT!", 1.4f, {120, 255, 120, 255}});
      SFX.power();
      }
    }

    // ---- 新机制: 盾牌 / 瞄准 / 射击输入 ----
    // 盾牌: 按住 Q 展开, 松开收回;  charges 耗尽后无法展开
    pl.shieldUp = (kb[SDL_SCANCODE_Q] != 0) && pl.shieldCharges > 0;
    // 瞄准: I 上, K 下 (与角色朝向组合成发射方向)
    pl.aimY = 0;
    if (kb[SDL_SCANCODE_I] != 0) pl.aimY -= 1;
    if (kb[SDL_SCANCODE_K] != 0) pl.aimY += 1;
    // 射击: 按住 E 以冷却间隔连发 (无限弹药); firing 供渲染层决定是否画预瞄虚线
    pl.firing = (kb[SDL_SCANCODE_E] != 0) && !pl.dead;
    if (pl.fireCd > 0) pl.fireCd -= DT;
    if (pl.firing && pl.fireCd <= 0) {
      Bullet b;
      b.pos = {pl.pos.x + pl.size.x / 2 - 5,
               pl.pos.y + pl.size.y * 0.4f - 3};
      if (pl.face > 0) b.pos.x += pl.size.x / 2;
      else b.pos.x -= pl.size.x / 2;
      b.vel = {(float)pl.face * BULLET_SPEED,
               (float)pl.aimY * BULLET_SPEED * 0.6f};
      b.life = BULLET_LIFE;
      g.bullets.push_back(b);
      pl.fireCd = BULLET_COOLDOWN;
      SFX.shoot();
    }

    // ---- 玩家横向 (加速度+摩擦, 疾跑; 举盾时变慢) ----
    int dir = 0;
    if (kb[SDL_SCANCODE_A] || kb[SDL_SCANCODE_LEFT]) dir -= 1;
    if (kb[SDL_SCANCODE_D] || kb[SDL_SCANCODE_RIGHT]) dir += 1;
    if (smoke) dir = 1;
    bool wantRun = kb[SDL_SCANCODE_LSHIFT] || kb[SDL_SCANCODE_RSHIFT] || kb[SDL_SCANCODE_J] || smoke;
    float maxSpd = wantRun ? RUN_MAX : WALK_MAX;
    if (pl.shieldUp) maxSpd *= SHIELD_SLOW; // 举盾减速
    float accel = pl.grounded ? GROUND_ACCEL : AIR_ACCEL;
    if (dir != 0) {
      pl.face = dir;
      float target = dir * maxSpd;
      float dv = target - pl.vel.x;
      float step = accel * DT * ( (dir * pl.vel.x < 0) ? 1.6f : 1.0f ); // 急停/转身更快
      if (fabsf(dv) <= step) pl.vel.x = target;
      else pl.vel.x += (dv > 0 ? step : -step);
      // 跑动扬尘 + 急转弯 skid
      pl.anim += DT * (fabsf(pl.vel.x) / 60.0f);
      if (pl.grounded && fabsf(pl.vel.x) > 260 && (rand() % 8 == 0))
        burst(g, {pl.pos.x + 12, pl.pos.y + pl.size.y}, 1, {230,230,230,255}, 60, 300, 3);
      if (pl.grounded && dir * pl.vel.x < -60 && fabsf(pl.vel.x) > 180) {
        if (pl.skidT <= 0) { SFX.skid(); pl.skidT = 0.25f; }
        burst(g, {pl.pos.x + 12, pl.pos.y + pl.size.y}, 2, {255,255,255,255}, 90, 400, 3);
      }
    } else {
      float fr = (pl.grounded ? FRICTION : 500.0f) * DT;
      if (fabsf(pl.vel.x) <= fr) pl.vel.x = 0;
      else pl.vel.x -= (pl.vel.x > 0 ? fr : -fr);
    }
    if (pl.skidT > 0) pl.skidT -= DT;

    // ---- 跳跃 (土狼 + 预输入 + 可变高度) ----
    pl.wasGrounded = pl.grounded;
    if (pl.grounded) { pl.coyote = COYOTE_TIME; g.combo = 0; }
    else pl.coyote -= DT;
    pl.buffer -= DT;
    // smoke 模式自动跳过坑
    if (smoke && pl.grounded && (rand() % 40 == 0)) pl.buffer = BUFFER_TIME;
    if (pl.buffer > 0 && (pl.grounded || pl.coyote > 0)) {
      pl.vel.y = JUMP_VEL * -1.0f;
      if (!pl.grounded) { /* 土狼跳 */ }
      pl.grounded = false; pl.coyote = 0; pl.buffer = 0;
      pl.squash = -0.45f;
      SFX.jump();
      burst(g, {pl.pos.x + 12, pl.pos.y + pl.size.y}, 6, {255,255,255,255}, 110, 500, 3);
    }
    // 长按跳更高: 上升段重力减半
    float grav = GRAVITY;
    if (pl.vel.y < 0 && pl.jumpHeld) grav *= 0.55f;
    if (pl.vel.y > 0) grav *= 1.25f; // 下落更干脆
    pl.vel.y += grav * DT;
    if (pl.vel.y > MAX_FALL) pl.vel.y = MAX_FALL;

    // ---- X 轴移动+碰撞 ----
    pl.pos.x += pl.vel.x * DT;
    if (boxHitTile(g, pl.pos, pl.size)) {
      if (pl.vel.x > 0) pl.pos.x = floorf((pl.pos.x + pl.size.x) / TILE_SIZE) * TILE_SIZE - pl.size.x - 0.01f;
      else if (pl.vel.x < 0) pl.pos.x = floorf(pl.pos.x / TILE_SIZE) * TILE_SIZE + TILE_SIZE + 0.01f;
      pl.vel.x = 0;
    }
    if (pl.pos.x < 0) pl.pos.x = 0;
    if (pl.pos.x > WORLD_WIDTH - pl.size.x) pl.pos.x = WORLD_WIDTH - pl.size.x;

    // ---- Y 轴移动+碰撞 (含顶头砖块交互) ----
    float prevBottom = pl.pos.y + pl.size.y;
    pl.pos.y += pl.vel.y * DT;
    pl.grounded = false;
    if (boxHitTile(g, pl.pos, pl.size)) {
      if (pl.vel.y > 0) {
        pl.pos.y = floorf((pl.pos.y + pl.size.y) / TILE_SIZE) * TILE_SIZE - pl.size.y;
        float impact = pl.vel.y;
        pl.vel.y = 0; pl.grounded = true; pl.coyote = COYOTE_TIME;
        if (!pl.wasGrounded && impact > 350) { // 落地反馈
          pl.squash = 0.5f;
          burst(g, {pl.pos.x + 12, pl.pos.y + pl.size.y}, (int)(impact/120), {220,220,220,255}, 120, 600, 3);
        }
      } else if (pl.vel.y < 0) {
        int headR = (int)floor(pl.pos.y / TILE_SIZE);
        pl.pos.y = headR * TILE_SIZE + TILE_SIZE + 0.01f;
        pl.vel.y = 0;
        // 找出头顶撞到的砖
        int c0 = (int)floor((pl.pos.x+2) / TILE_SIZE), c1 = (int)floor((pl.pos.x + pl.size.x-2) / TILE_SIZE);
        for (int c = c0; c <= c1; ++c) {
          int t = getT(g, c, headR);
          if (t == T_QUESTION || t == T_BRICK || t == T_USED || t == T_STONE) {
            g.bumps.push_back({c, headR, 0.22f});
            if (t == T_QUESTION) {
              // ! 能量舱: 顶撞给随机奖励 (金币/心/盾牌/星), 蘑菇机制已移除
              setT(g, c, headR, T_USED);
              int roll = rand() % 4;
              if (roll == 0) {
                g.score += 200; g.coinCount++;
                SFX.coin();
                burst(g, {float(c*TILE_SIZE+16), float(headR*TILE_SIZE-6)}, 8, {255,215,60,255}, 160, 700, 3);
                g.popups.push_back({{float(c*TILE_SIZE), float(headR*TILE_SIZE-24)}, "+200", 0.8f, {255,230,90,255}});
                if (g.coinCount % 30 == 0 && g.lives < MAX_HEARTS) {
                  g.lives++; SFX.heart();
                  g.popups.push_back({{pl.pos.x, pl.pos.y-40}, "+1 HEART", 1.4f, {120,255,120,255}});
                }
              } else if (roll == 1) {
                if (g.lives < MAX_HEARTS) { g.lives++; SFX.heart(); }
                else { g.score += 500; SFX.coin(); }
                burst(g, {float(c*TILE_SIZE+16), float(headR*TILE_SIZE-6)}, 10, {120,255,120,255}, 160, 500, 3);
                g.popups.push_back({{float(c*TILE_SIZE), float(headR*TILE_SIZE-24)}, "+1 HEART", 1.2f, {120,255,120,255}});
              } else if (roll == 2) {
                pl.shieldCharges = SHIELD_MAX_CHARGES; SFX.power();
                burst(g, {float(c*TILE_SIZE+16), float(headR*TILE_SIZE-6)}, 10, {90,235,255,255}, 160, 500, 3);
                g.popups.push_back({{float(c*TILE_SIZE), float(headR*TILE_SIZE-24)}, "SHIELD!", 1.2f, {90,235,255,255}});
              } else {
                g.score += 1000; SFX.power();
                burst(g, {float(c*TILE_SIZE+16), float(headR*TILE_SIZE-6)}, 12, {255,220,80,255}, 180, 500, 4);
                g.popups.push_back({{float(c*TILE_SIZE), float(headR*TILE_SIZE-24)}, "+1000 STAR", 1.2f, {255,220,80,255}});
              }
            } else if (t == T_BRICK) {
              // 无变大形态: 砖块顶不碎, 仅反馈 (子弹用于攻击, 盾牌用于防卫)
              SFX.bump();
            } else { SFX.bump(); }
            break;
          }
        }
      }
    }
    (void)prevBottom;

    // ---- 移动平台 (先更新, 再承载+碰撞) ----
    for (auto &p : g.plats) {
      p.prev = p.pos;
      p.t += DT;
      float total = fabsf(p.b.x - p.a.x) + fabsf(p.b.y - p.a.y);
      float span = total < 1 ? 1 : total;
      float ph = fmodf(p.t * p.speed / span, 2.0f);
      float k = ph < 1 ? ph : 2 - ph;
      p.pos.x = p.a.x + (p.b.x - p.a.x) * k;
      p.pos.y = p.a.y + (p.b.y - p.a.y) * k;
    }
    // 站在平台上 -> 跟随
    for (auto &p : g.plats) {
      bool onTop = fabsf((pl.pos.y + pl.size.y) - p.pos.y) < 8.0f &&
                   pl.pos.x + pl.size.x > p.pos.x + 2 && pl.pos.x < p.pos.x + p.size.x - 2 &&
                   pl.vel.y >= 0;
      if (onTop) {
        gml::vec2 d = p.delta();
        pl.pos.x += d.x; pl.pos.y = p.pos.y - pl.size.y;
        pl.vel.y = 0; pl.grounded = true; pl.coyote = COYOTE_TIME;
      } else if (aabb(pl.pos, pl.size, p.pos, p.size) && pl.vel.y > 0 &&
                 (pl.pos.y + pl.size.y - pl.vel.y * DT) <= p.pos.y + 6) {
        pl.pos.y = p.pos.y - pl.size.y; pl.vel.y = 0;
        pl.grounded = true; pl.coyote = COYOTE_TIME;
      }
    }

    // ---- 尖刺 ----
    {
      int c0=(int)floor(pl.pos.x/TILE_SIZE), c1=(int)floor((pl.pos.x+pl.size.x)/TILE_SIZE);
      int r0=(int)floor(pl.pos.y/TILE_SIZE), r1=(int)floor((pl.pos.y+pl.size.y)/TILE_SIZE);
      for (int r=r0;r<=r1;++r) for(int c=c0;c<=c1;++c)
        if (getT(g,c,r)==T_SPIKE) {
          SDL_Rect a{(int)pl.pos.x+4,(int)pl.pos.y+8,(int)pl.size.x-8,(int)pl.size.y-8};
          SDL_Rect b{c*TILE_SIZE+4, r*TILE_SIZE+10, TILE_SIZE-8, TILE_SIZE-10};
          if (a.x < b.x+b.w && a.x+a.w > b.x && a.y < b.y+b.h && a.y+a.h > b.y) {
            // 新机制: 尖刺受伤 = 盾牌格挡优先, 否则 -1 心; 心归零直接 GAMEOVER
            if (!pl.dead && hurtPlayer(g, pl, (float)(c * TILE_SIZE + 16))) {
              SFX.die(); state = ST_DYING; g.stateT = 0; pl.dead = true; pl.vel = {0,-650};
              app.respawnLevel = false;
            }
          }
        }
    }

    // ---- 蘑菇机制已移除 (顶撞 ! 方块改为随机奖励: 金币/心/盾牌/星) ----

    // ---- 金币收集 ----
    for (auto &cn : g.coins) {
      if (cn.taken) continue;
      cn.anim += DT;
      gml::vec2 cp{cn.pos.x, cn.pos.y}, cs{28, 28};
      if (aabb(pl.pos, pl.size, cp, cs)) {
        cn.taken = true; g.score += 200; g.coinCount++; SFX.coin();
        burst(g, {cp.x+14, cp.y+14}, 8, {255,215,60,255}, 150, 600, 3);
        g.popups.push_back({{cp.x, cp.y-16}, "+200", 0.7f, {255,230,90,255}});
        if (g.coinCount % 30 == 0 && g.lives < MAX_HEARTS) { g.lives++; SFX.heart(); g.popups.push_back({{pl.pos.x, pl.pos.y-40}, "+1 HEART", 1.4f, {120,255,120,255}}); }
      }
    }

    // ---- 敌人 ----
    for (auto &e : g.foes) {
      if (!e.alive) { if (e.squashT > 0) e.squashT -= DT; continue; }
      e.anim += DT;
      if (e.flyer) {
        e.phase += DT * 2.0f;
        e.pos.x += -60 * DT;
        if (e.pos.x < 0) e.pos.x = WORLD_WIDTH - 30;
        e.pos.y = e.baseY + sinf(e.phase) * 40.0f;
        // 飞出屏幕左则绕回右(保持关卡压力)
        if (e.pos.x + 30 < cam.x - 100) { e.pos.x = cam.x + WINDOW_WIDTH + 100; e.baseY = 200 + rand()%250; }
      } else {
        e.vel.y += GRAVITY * DT; if (e.vel.y > MAX_FALL) e.vel.y = MAX_FALL;
        // 远处敌人休眠省性能 + 避免乱跑
        if (fabsf(e.pos.x - pl.pos.x) < 700) {
          e.pos.x += e.vel.x * DT;
          if (boxHitTile(g, e.pos, e.size)) {
            if (e.vel.x > 0) e.pos.x = floorf((e.pos.x+e.size.x)/TILE_SIZE)*TILE_SIZE - e.size.x - 0.5f;
            else e.pos.x = floorf(e.pos.x/TILE_SIZE)*TILE_SIZE + TILE_SIZE + 0.5f;
            e.vel.x = -e.vel.x;
          }
          // 悬崖回头 (更聪明)
          int aheadX = (int)((e.vel.x > 0 ? e.pos.x + e.size.x + 4 : e.pos.x - 4) / TILE_SIZE);
          int belowR = (int)((e.pos.y + e.size.y + 6) / TILE_SIZE);
          if (e.vel.y == 0 && !solidTile(getT(g, aheadX, belowR)) && getT(g, aheadX, belowR) != T_FLAG)
            e.vel.x = -e.vel.x;
          e.pos.y += e.vel.y * DT;
          if (boxHitTile(g, e.pos, e.size)) {
            if (e.vel.y > 0) e.pos.y = floorf((e.pos.y+e.size.y)/TILE_SIZE)*TILE_SIZE - e.size.y;
            else e.pos.y = floorf(e.pos.y/TILE_SIZE)*TILE_SIZE + TILE_SIZE;
            e.vel.y = 0;
          }
        } else {
          e.pos.y += e.vel.y * DT;
          if (boxHitTile(g, e.pos, e.size)) {
            if (e.vel.y > 0) e.pos.y = floorf((e.pos.y+e.size.y)/TILE_SIZE)*TILE_SIZE - e.size.y;
            e.vel.y = 0;
          }
        }
      }
      if (e.pos.y > WORLD_HEIGHT + 80) { e.alive = false; continue; }
      // 与玩家交互 (新机制: 踩踏已移除, 接触即受伤; 子弹用于攻击, 盾牌用于防卫)
      if (!pl.dead && aabb(pl.pos, pl.size, e.pos, e.size)) {
        // 弹开敌人避免同帧连续受伤 (无无敌帧, 靠位移分离)
        e.vel.x = -e.vel.x;
        if (hurtPlayer(g, pl, e.pos.x + e.size.x / 2)) {
          SFX.die(); state = ST_DYING; g.stateT = 0; pl.dead = true; pl.vel = {60,-650};
          app.respawnLevel = false;
        }
      }
    }

    // ---- 子弹 (攻击用, 无法穿透方块: 命中方块即销毁) ----
    for (auto &b : g.bullets) {
      if (b.dead) continue;
      b.life -= DT;
      if (b.life <= 0) { b.dead = true; continue; }
      b.pos.x += b.vel.x * DT;
      b.pos.y += b.vel.y * DT;
      if (b.pos.x < -40 || b.pos.x > WORLD_WIDTH + 40 ||
          b.pos.y < -40 || b.pos.y > WORLD_HEIGHT + 40) { b.dead = true; continue; }
      if (boxHitTile(g, b.pos, b.size)) {
        b.dead = true;
        burst(g, {b.pos.x + 5, b.pos.y + 3}, 4, {255,220,120,255}, 120, 400, 2);
        continue;
      }
      for (auto &e : g.foes) {
        if (!e.alive) continue;
        if (aabb(b.pos, b.size, e.pos, e.size)) {
          b.dead = true;
          e.hp--;
          if (e.hp <= 0) {
            e.alive = false; e.squashT = 0.4f;
            g.score += 100; SFX.stomp();
            burst(g, {e.pos.x + 14, e.pos.y + 10}, 10, {255,220,120,255}, 170, 700, 4);
            g.popups.push_back({{e.pos.x, e.pos.y - 18}, "+100", 0.9f, {255,220,120,255}});
          } else {
            // 第 1 击: 受伤反馈 + 击退 (还需 1 发)
            SFX.bump();
            burst(g, {e.pos.x + 14, e.pos.y + 10}, 6, {255,90,90,255}, 150, 500, 3);
            g.popups.push_back({{e.pos.x, e.pos.y - 18}, "HIT!", 0.7f, {255,120,120,255}});
            e.pos.x += (b.vel.x > 0) ? 10.0f : -10.0f;
            if (!e.flyer) e.vel.x = -e.vel.x;
          }
          break;
        }
      }
    }
    for (auto it = g.bullets.begin(); it != g.bullets.end();) {
      if (it->dead) it = g.bullets.erase(it); else ++it;
    }

    // ---- 掉坑 (当前关卡重开, 不扣心) ----
    if (pl.pos.y > WORLD_HEIGHT + 40 && !pl.dead) {
      SFX.die(); state = ST_DYING; g.stateT = 0; pl.dead = true; pl.vel = {0, 0};
      app.respawnLevel = true;
    }
    // ---- 到旗 ----
    float flagX = g.flagCol * TILE_SIZE + 6;
    if (pl.pos.x + pl.size.x >= flagX && pl.pos.y + pl.size.y > 5 * TILE_SIZE) {
      state = ST_WINANIM; g.stateT = 0; g.winT = 0;
      pl.pos.x = flagX - pl.size.x - 2;
      SFX.win();
    }

    if (fabsf(pl.squash) > 0.01f) pl.squash *= (1.0f - DT*10); else pl.squash = 0;

    // ---- 相机 (平滑 + 预判 + 震屏) ----
    float targetX = pl.pos.x + pl.size.x/2 - WINDOW_WIDTH/2 + pl.face * 60 + pl.vel.x * 0.15f;
    float targetY = pl.pos.y + pl.size.y/2 - WINDOW_HEIGHT/2 - 40;
    targetX = std::max(0.0f, std::min(targetX, float(WORLD_WIDTH - WINDOW_WIDTH)));
    targetY = std::max(float(WORLD_HEIGHT - WINDOW_HEIGHT), std::min(targetY, 0.0f));
    cam.x += (targetX - cam.x) * std::min(1.0f, DT * 6);
    cam.y += (targetY - cam.y) * std::min(1.0f, DT * 4);
    if (g.shake > 0) g.shake -= DT;

  } else if (state == ST_DYING) {
    g.stateT += DT;
    pl.vel.y += GRAVITY * DT;
    pl.pos.y += pl.vel.y * DT;
    if (g.stateT > 1.8f) {
      if (app.respawnLevel) {
        // 掉坑/超时: 当前关卡重开 (保留分数/金币/心, 时间回满到至少 60, 出生点为检查点)
        int sc = g.score, cc = g.coinCount, lv = g.lives;
        float tm = std::max(g.timeLeft, 60.0f);
        buildLevel(g, app.currentLevelIndex);
        g.score = sc; g.coinCount = cc; g.lives = lv; g.timeLeft = tm;
        if (app.cpSaved) { g.checkpointOn = true; g.checkpoint = app.cpPos; }
        pl = Player();
        pl.pos = app.cpSaved ? app.cpPos : g.spawn;
        pl.size = {24, 40};
        cam.x = std::max(0.0f, std::min(pl.pos.x - WINDOW_WIDTH/2.0f, float(WORLD_WIDTH - WINDOW_WIDTH)));
        app.respawnLevel = false;
        state = ST_PLAY;
      } else {
        // 心归零: 直接 GAMEOVER
        state = ST_GAMEOVER;
      }
    }
  } else if (state == ST_WINANIM) {
    g.stateT += DT;
    // 沿旗下滑
    if (pl.pos.y + pl.size.y < 16 * TILE_SIZE) {
      pl.pos.y += 220 * DT;
    } else {
      pl.pos.y = 16*TILE_SIZE - pl.size.y;
      pl.pos.x += 120 * DT; // 自动向右走向城堡
      pl.anim += DT * 8;
    }
    cam.x += ((pl.pos.x - WINDOW_WIDTH/2) - cam.x) * std::min(1.0f, DT*4);
    cam.x = std::max(0.0f, std::min(cam.x, float(WORLD_WIDTH - WINDOW_WIDTH)));
    if (g.stateT > 3.2f) {
      int bonus = (int)g.timeLeft * 10;
      g.score += 2000 + bonus;
      // 多关卡结算: 还有后续关卡则进入过渡态, 否则大结局
      app.currentLevelIndex++;
      if (app.currentLevelIndex >= levelCount()) {
        state = ST_WIN;
      } else {
        state = ST_NEXTLEVEL; g.stateT = 0;
        g.popups.push_back({{pl.pos.x, pl.pos.y - 60}, "LOAD NEXT LEVEL", 2.0f, {255, 230, 90, 255}});
        SFX.power();
      }
    }
  } else if (state == ST_NEXTLEVEL) {
    // 关卡间过渡: 定格 2 秒展示提示, 然后无缝载入下一关
    // (保留分数/金币/命, 时间回满, 玩家/相机重置; 物理逻辑冻结, 特效照常推进)
    g.stateT += DT;
    if (g.stateT > 2.0f) {
      app.loadLevelKeepScore(app.currentLevelIndex);
    }
  }
  // 特效推进 (与原单文件一致: 每个固定步长都跑一次)
  updateFx(g, DT);
}
