#pragma once
// Entities.hpp — 纯数据结构体 (POD), 无任何全局变量, 无行为代码
// 单向依赖 Common.hpp, 不反向包含任何上层头文件, 从源头杜绝循环引用。
#include "Common.hpp"
#include <SDL2/SDL.h>
#include <string>
#include <vector>

struct Bullet {
  gml::vec2 pos, vel, size{10, 6};
  float life = 0;
  bool dead = false;
};

struct Player {
  gml::vec2 pos{64, 400}, vel{0, 0}, size{24, 40};
  bool grounded = false, wasGrounded = false;
  float coyote = 0, buffer = 0, anim = 0, squash = 0;
  float skidT = 0;
  int face = 1; bool jumpHeld = false;
  bool dead = false;
  // 受伤无敌: 受伤后 1 秒内免伤 (HURT_INVINCIBLE_TIME), 渲染层据此做红色闪烁
  float invincT = 0;
  // 新机制: 盾牌 (按住 Q 展开, 松开收回, 最多格挡 2 次后损毁)
  bool shieldUp = false;
  int shieldCharges = 2;
  // 新机制: 射击 (E 发射, I/K 控制上下分量, 无限弹药 + 冷却限制)
  float fireCd = 0;
  int aimY = 0; // -1 上, 0 平, +1 下
  bool firing = false; // E 是否按住 (渲染层据此决定是否画预瞄虚线)
};

struct Goomba {
  gml::vec2 pos, vel, size{28, 28};
  bool alive = true, flyer = false;
  float anim = 0, squashT = 0, baseY = 0, phase = 0;
  int hp = 2; // 怪物血量: 需 2 发子弹消灭
  // ---- 智能 AI 新增字段 (存档无关, 运行时重建即可) ----
  // ai: 地面怪 0=巡逻 1=追击者 2=跳跃者; 飞行怪 0=巡逻 1=追击者
  int ai = 0;
  float baseSpeed = 70.0f; // 巡逻速度 (配表驱动, 随关卡递增)
  float homeX = 0.0f;      // 出生点 X (飞行怪巡逻中心, 追击超出范围即返巢)
  bool grounded = false;   // 本帧是否着地 (取代旧的 vel.y==0 误判)
  float thinkT = 0.0f;     // 感知节流计时 (0.12s 决策一次, 防抖动)
  int moveDir = -1;        // 当前巡逻/追击方向 (-1 左, +1 右)
};

struct Shroom {
  gml::vec2 pos, vel{90, 0}, size{28, 28};
  bool active = false, dead = false;
};

struct CoinItem {
  gml::vec2 pos;
  bool taken = false;
  float anim = 0;
};

struct Particle {
  gml::vec2 pos, vel;
  float life = 0, maxLife = 0;
  SDL_Color col{0, 0, 0, 255};
  int size = 0;
  float grav = 0;
};

struct Popup {
  gml::vec2 pos;
  std::string txt;
  float life = 0;
  SDL_Color col{255, 255, 255, 255};
};

struct Bump {
  int c = 0, r = 0;
  float t = 0;
};

struct Platform {
  gml::vec2 pos, size, a, b;
  float speed = 60, t = 0;
  gml::vec2 prev;
  gml::vec2 delta() const { return pos - prev; }
};

struct Cloud {
  float x = 0, y = 0, s = 1, v = 0;
};

enum State {
  ST_TITLE,
  ST_PLAY,
  ST_DYING,
  ST_WINANIM,
  ST_NEXTLEVEL, // 关卡间过渡: 显示 LOAD NEXT LEVEL, 2 秒后无缝载入下一关
  ST_WIN,
  ST_GAMEOVER
};

struct Game {
  std::vector<int> tiles;
  std::vector<Goomba> foes;
  std::vector<Shroom> shrooms; // 已弃用: 蘑菇机制移除, 保留字段避免破坏存档/关卡加载
  std::vector<CoinItem> coins;
  std::vector<Bullet> bullets; // 新机制: 玩家子弹 (攻击用, 无法穿透方块)
  std::vector<Particle> parts;
  std::vector<Popup> popups;
  std::vector<Bump> bumps;
  std::vector<Platform> plats;
  std::vector<Cloud> clouds;
  // lives 即心数: 初始 3, 怪物攻击 -1, 归零 GAMEOVER; 掉坑/超时直接 GAMEOVER
  int score = 0, coinCount = 0, lives = 3;
  float timeLeft = 300;
  int combo = 0;
  float stateT = 0, winT = 0, shake = 0;
  int flagCol = 116;
  gml::vec2 spawn{64, 400}, checkpoint{64, 400};
  bool checkpointOn = false;
};
