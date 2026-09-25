#pragma once
// Entities.hpp — 纯数据结构体 (POD), 无任何全局变量, 无行为代码
// 单向依赖 Common.hpp, 不反向包含任何上层头文件, 从源头杜绝循环引用。
#include "Common.hpp"
#include <SDL2/SDL.h>
#include <string>
#include <vector>

struct Player {
  gml::vec2 pos{64, 400}, vel{0, 0}, size{24, 40};
  bool grounded = false, wasGrounded = false;
  float coyote = 0, buffer = 0, anim = 0, squash = 0;
  float invincible = 0, skidT = 0;
  int face = 1; bool big = false; bool jumpHeld = false;
  bool dead = false;
};

struct Goomba {
  gml::vec2 pos, vel, size{28, 28};
  bool alive = true, flyer = false;
  float anim = 0, squashT = 0, baseY = 0, phase = 0;
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
  ST_WIN,
  ST_GAMEOVER
};

struct Game {
  std::vector<int> tiles;
  std::vector<Goomba> foes;
  std::vector<Shroom> shrooms;
  std::vector<CoinItem> coins;
  std::vector<Particle> parts;
  std::vector<Popup> popups;
  std::vector<Bump> bumps;
  std::vector<Platform> plats;
  std::vector<Cloud> clouds;
  int score = 0, coinCount = 0, lives = 3;
  float timeLeft = 300;
  int combo = 0;
  float stateT = 0, winT = 0, shake = 0;
  int flagCol = 116;
  gml::vec2 spawn{64, 400}, checkpoint{64, 400};
  bool checkpointOn = false;
};
