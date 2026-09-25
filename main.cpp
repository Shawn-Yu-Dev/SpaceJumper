#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

// ============================================================================
// SUPER PLATFORMER - 高可玩性平台跳跃游戏 (单文件, 仅依赖 SDL2)
// 操作: A/D 或 ←/→ 移动, Space/W/↑ 跳跃(支持土狼时间+预输入+大小跳),
//       Shift/J 疾跑, P 暂停, M 静音, R 重开, Enter 开始/确认
// ============================================================================
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
}

// 固定视野缩放: 玩法/渲染一律用 960x600 逻辑坐标, SDL 自动缩放到任意窗口.
// winActualW/H 仅记录 OS 实际窗口像素, 不参与任何相机/绘制, 避免放大后 HUD/背景错位.
constexpr int LOGICAL_WIDTH = 960;
constexpr int LOGICAL_HEIGHT = 600;
constexpr int WINDOW_WIDTH = 960;
constexpr int WINDOW_HEIGHT = 600;
int winActualW = 960;
int winActualH = 600;
constexpr int HUD_HEIGHT = 44;

constexpr int TILE_SIZE = 32;
constexpr int MAP_COLS = 124;
constexpr int MAP_ROWS = 19;
constexpr int WORLD_WIDTH = MAP_COLS * TILE_SIZE;
constexpr int WORLD_HEIGHT = MAP_ROWS * TILE_SIZE;

// 物理手感参数
constexpr float GRAVITY = 1500.0f;
constexpr float JUMP_VEL = 620.0f;
constexpr float JUMP_CUT = -240.0f;   // 松开跳跃键后的最小上升速度(小跳)
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

// ----------------------------------------------------------------------------
// 3x5 像素字体 (HUD / 标题 / 弹窗)
// ----------------------------------------------------------------------------
static std::unordered_map<char, std::vector<std::string>> FONT = {
  {'0',{"111","101","101","101","111"}},{'1',{"010","110","010","010","111"}},
  {'2',{"111","001","111","100","111"}},{'3',{"111","001","111","001","111"}},
  {'4',{"101","101","111","001","001"}},{'5',{"111","100","111","001","111"}},
  {'6',{"111","100","111","101","111"}},{'7',{"111","001","001","010","010"}},
  {'8',{"111","101","111","101","111"}},{'9',{"111","101","111","001","111"}},
  {'A',{"010","101","111","101","101"}},{'B',{"110","101","110","101","110"}},
  {'C',{"011","100","100","100","011"}},{'D',{"110","101","101","101","110"}},
  {'E',{"111","100","110","100","111"}},{'F',{"111","100","110","100","100"}},
  {'G',{"011","100","101","101","011"}},{'H',{"101","101","111","101","101"}},
  {'I',{"111","010","010","010","111"}},{'J',{"001","001","001","101","010"}},
  {'K',{"101","101","110","101","101"}},{'L',{"100","100","100","100","111"}},
  {'M',{"101","111","111","101","101"}},{'N',{"101","111","101","101","101"}},
  {'O',{"010","101","101","101","010"}},{'P',{"110","101","110","100","100"}},
  {'Q',{"010","101","101","110","011"}},{'R',{"110","101","110","101","101"}},
  {'S',{"011","100","010","001","110"}},{'T',{"111","010","010","010","010"}},
  {'U',{"101","101","101","101","111"}},{'V',{"101","101","101","101","010"}},
  {'W',{"101","101","111","111","101"}},{'X',{"101","101","010","101","101"}},
  {'Y',{"101","101","010","010","010"}},{'Z',{"111","001","010","100","111"}},
  {' ',{"000","000","000","000","000"}},{'!',{"010","010","010","000","010"}},
  {'?',{"111","001","010","000","010"}},{'.',{"000","000","000","000","010"}},
  {',',{"000","000","000","010","100"}},{':',{"000","010","000","010","000"}},
  {'-',{"000","000","111","000","000"}},{'+',{"000","010","111","010","000"}},
  {'/',{"001","001","010","100","100"}},{'(',{"001","010","010","010","001"}},
  {')',{"100","010","010","010","100"}},{'<',{"001","010","100","010","001"}},
  {'>',{"100","010","001","010","100"}},{'=',{"000","111","000","111","000"}},
  {'_',{"000","000","000","000","111"}},{'*',{"000","101","010","101","000"}},
  {'\'',{"010","010","000","000","000"}},{'"',{"101","101","000","000","000"}},
};

void drawText(SDL_Renderer *r, const std::string &text, int x, int y, int scale,
              SDL_Color col) {
  SDL_SetRenderDrawColor(r, col.r, col.g, col.b, col.a);
  int cx = x;
  for (char ch : text) {
    char u = ch;
    if (u >= 'a' && u <= 'z') u = u - 'a' + 'A';
    auto it = FONT.find(u);
    if (it == FONT.end()) { cx += (3 + 1) * scale; continue; }
    const auto &g = it->second;
    for (int row = 0; row < 5; ++row)
      for (int c = 0; c < 3; ++c)
        if (g[row][c] == '1') {
          SDL_Rect rc{cx + c * scale, y + row * scale, scale, scale};
          SDL_RenderFillRect(r, &rc);
        }
    cx += (3 + 1) * scale;
  }
}
int textWidth(const std::string &t, int scale) { return (int)t.size() * 4 * scale; }
void drawTextCenter(SDL_Renderer *r, const std::string &t, int cx, int y, int scale,
                    SDL_Color col) {
  drawText(r, t, cx - textWidth(t, scale) / 2, y, scale, col);
}

// ----------------------------------------------------------------------------
// 复古音效合成 (无需外部资源, SDL QueueAudio)
// ----------------------------------------------------------------------------
struct Sfx {
  SDL_AudioDeviceID dev = 0;
  bool ok = false, muted = false;
  void init() {
    SDL_AudioSpec want{}, have{};
    want.freq = 22050; want.format = AUDIO_S16SYS;
    want.channels = 1; want.samples = 1024;
    dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (dev) { ok = true; SDL_PauseAudioDevice(dev, 0); }
  }
  void tone(int f0, int f1, int ms, int vol = 7000, int wave = 0) {
    if (!ok || muted || ms <= 0) return;
    int sr = 22050, n = sr * ms / 1000;
    std::vector<Sint16> buf(n);
    float phase = 0;
    for (int i = 0; i < n; ++i) {
      float f = f0 + (f1 - f0) * (float)i / (float)n;
      phase += 2.0f * 3.1415926f * f / sr;
      float s = 0;
      if (wave == 0) s = (sinf(phase) > 0 ? 1.0f : -1.0f) * 0.6f; // 方波
      else if (wave == 1) s = sinf(phase);                        // 正弦
      else s = ((rand() % 2000) / 1000.0f - 1.0f) * 0.6f;         // 噪声
      float env = 1.0f - (float)i / (float)n;  // 衰减包络防爆音
      buf[i] = (Sint16)(s * vol * (0.3f + 0.7f * env));
    }
    SDL_QueueAudio(dev, buf.data(), buf.size() * sizeof(Sint16));
  }
  void jump()   { tone(280, 720, 130); }
  void coin()   { tone(988, 988, 70); tone(1319, 1319, 220); }
  void stomp()  { tone(420, 90, 160); }
  void bump()   { tone(180, 110, 110); }
  void brk()    { tone(300, 60, 220, 7000, 2); }
  void sprout() { tone(300, 900, 320); }
  void power()  { tone(500, 1100, 260); tone(700, 1400, 200); }
  void hurt()   { tone(500, 90, 450); }
  void die()    { tone(600, 80, 700); }
  void kick()   { tone(600, 1200, 120); }
  void oneup()  { tone(660,660,90); tone(880,880,90); tone(1320,1320,260); }
  void skid()   { tone(900, 500, 60, 2500); }
  void spring() { tone(200, 900, 180); }
  void win()    { tone(523,523,120); tone(659,659,120); tone(784,784,120); tone(1046,1046,320); }
};
static Sfx SFX;

// ----------------------------------------------------------------------------
// 实体定义
// ----------------------------------------------------------------------------
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
struct Shroom { gml::vec2 pos, vel{90,0}, size{28,28}; bool active = false, dead=false; };
struct CoinItem { gml::vec2 pos; bool taken = false; float anim = 0; };
struct Particle { gml::vec2 pos, vel; float life, maxLife; SDL_Color col; int size; float grav; };
struct Popup { gml::vec2 pos; std::string txt; float life; SDL_Color col; };
struct Bump { int c, r; float t; };
struct Platform {
  gml::vec2 pos, size, a, b; float speed = 60, t = 0; gml::vec2 prev;
  gml::vec2 delta() const { return pos - prev; }
};
struct Cloud { float x, y, s, v; };

enum State { ST_TITLE, ST_PLAY, ST_DYING, ST_WINANIM, ST_WIN, ST_GAMEOVER };

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

// 关卡构建辅助
static void setT(Game &g, int c, int r, int v) {
  if (c >= 0 && c < MAP_COLS && r >= 0 && r < MAP_ROWS) g.tiles[tidx(c, r)] = v;
}
static int getT(const Game &g, int c, int r) {
  if (c < 0 || c >= MAP_COLS || r < 0 || r >= MAP_ROWS) return T_STONE; // 世界外视为墙
  return g.tiles[tidx(c, r)];
}
static bool solidTile(int t) { return t==T_GROUND||t==T_QUESTION||t==T_USED||t==T_BRICK||t==T_STONE; }

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

// ----------------------------------------------------------------------------
// 碰撞
// ----------------------------------------------------------------------------
bool boxHitTile(const Game &g, const gml::vec2 &pos, const gml::vec2 &sz) {
  int c0 = (int)floor(pos.x / TILE_SIZE), c1 = (int)floor((pos.x + sz.x - 0.01f) / TILE_SIZE);
  int r0 = (int)floor(pos.y / TILE_SIZE), r1 = (int)floor((pos.y + sz.y - 0.01f) / TILE_SIZE);
  c0 = std::max(0, std::min(c0, MAP_COLS-1)); c1 = std::max(0, std::min(c1, MAP_COLS-1));
  r0 = std::max(0, std::min(r0, MAP_ROWS-1)); r1 = std::max(0, std::min(r1, MAP_ROWS-1));
  for (int r = r0; r <= r1; ++r)
    for (int c = c0; c <= c1; ++c)
      if (solidTile(getT(g, c, r))) return true;
  return false;
}
bool aabb(gml::vec2 a, gml::vec2 as, gml::vec2 b, gml::vec2 bs) {
  return a.x < b.x + bs.x && a.x + as.x > b.x && a.y < b.y + bs.y && a.y + as.y > b.y;
}

void burst(Game &g, gml::vec2 p, int n, SDL_Color col, float spd, float grav, int sz) {
  for (int i = 0; i < n; ++i) {
    if (g.parts.size() > 350) break;
    float a = (rand() % 360) * 3.14159f / 180.0f;
    float s = spd * (0.4f + (rand() % 100) / 100.0f);
    Particle pt{{p.x, p.y}, {cosf(a)*s, sinf(a)*s - spd*0.4f}, 0, 0,
      col, sz, grav};
    pt.maxLife = pt.life = 0.4f + (rand() % 40) / 100.0f;
    g.parts.push_back(pt);
  }
}

// ----------------------------------------------------------------------------
// 绘制辅助: 背景 / 瓦片 / 角色
// ----------------------------------------------------------------------------
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

// ----------------------------------------------------------------------------
// 主函数
// ----------------------------------------------------------------------------
int main(int argc, char *argv[]) {
  srand((unsigned)SDL_GetTicks());
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
    std::cerr << "SDL init failed: " << SDL_GetError() << std::endl;
    return -1;
  }
  SDL_Window *win = SDL_CreateWindow("SUPER PLATFORMER - 超级平台跳跃",
    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
    LOGICAL_WIDTH, LOGICAL_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (!win) { std::cerr << SDL_GetError() << std::endl; return -1; }
  SDL_SetWindowMinimumSize(win, 480, 300);
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
  SDL_Renderer *ren = SDL_CreateRenderer(win, -1,
    SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
  // 固定逻辑分辨率, SDL 自动等比缩放渲染内容到任意窗口大小(含黑边保持宽高比)
  SDL_RenderSetLogicalSize(ren, LOGICAL_WIDTH, LOGICAL_HEIGHT);
  SFX.init();

  Game g; buildLevel(g);
  Player pl;
  gml::vec2 cam{0, 0};
  State state = ST_TITLE;
  float globalT = 0;
  bool paused = false;
  // 检查点持久化(死亡重建地图不丢失)
  bool cpSaved = false;
  gml::vec2 cpPos{73.0f * TILE_SIZE, 12.0f * TILE_SIZE};

  auto resetRun = [&]() {
    g.score = 0; g.coinCount = 0; g.lives = 3; g.timeLeft = 300;
    buildLevel(g);
    pl = Player(); pl.pos = g.spawn; pl.size = {24, 40};
    cam = {0, 0}; state = ST_PLAY; paused = false;
    cpSaved = false;
  };
  bool running = true;
  Uint32 last = SDL_GetTicks();
  float acc = 0; const float DT = 1.0f / 60.0f;

  // 烟雾测试模式: 传参 --smoke 则跑 3 秒自动退出(供 CI/无头验证)
  bool smoke = argc > 1 && std::string(argv[1]) == "--smoke";
  float smokeT = 0;

  while (running) {
    Uint32 now = SDL_GetTicks();
    float ft = (now - last) / 1000.0f; last = now;
    if (ft > 0.1f) ft = 0.1f;
    if (!paused) acc += ft;

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_QUIT) running = false;
      if (ev.type == SDL_WINDOWEVENT &&
          (ev.window.event == SDL_WINDOWEVENT_RESIZED ||
           ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)) {
        // 仅记录实际窗口像素, 不碰逻辑视口(玩法/渲染保持 960x600, 由 SDL 自动缩放)
        winActualW = ev.window.data1;
        winActualH = ev.window.data2;
      }
      if (ev.type == SDL_KEYDOWN && ev.key.repeat == 0) {
        auto sym = ev.key.keysym.sym;
        if (sym == SDLK_m) SFX.muted = !SFX.muted;
        if (sym == SDLK_p && state == ST_PLAY) paused = !paused;
        if (sym == SDLK_r) { resetRun(); }
        if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER) {
          if (state == ST_TITLE) { resetRun(); }
          else if (state == ST_WIN || state == ST_GAMEOVER) { resetRun(); }
        }
        if ((sym == SDLK_SPACE || sym == SDLK_w || sym == SDLK_UP) && state == ST_PLAY && !paused) {
          pl.buffer = BUFFER_TIME; pl.jumpHeld = true;
        }
        if (sym == SDLK_ESCAPE && state == ST_PLAY) paused = !paused;
      }
      if (ev.type == SDL_KEYUP) {
        auto sym = ev.key.keysym.sym;
        if (sym == SDLK_SPACE || sym == SDLK_w || sym == SDLK_UP) {
          pl.jumpHeld = false;
          if (pl.vel.y < JUMP_CUT) pl.vel.y = JUMP_CUT; // 小跳: 松开即截断上升
        }
      }
    }
    if (smoke) { // 自动演示输入, 验证逻辑不崩
      smokeT += ft;
      if (state == ST_TITLE && smokeT > 0.3f) resetRun();
      if (smokeT > 4.0f) running = false;
    }

    const Uint8 *kb = SDL_GetKeyboardState(nullptr);

    while (acc >= DT) {
      globalT += DT;
      if (state == ST_PLAY && !paused) {
        // ---- 计时 ----
        g.timeLeft -= DT;
        if (g.timeLeft <= 0) { g.timeLeft = 0; SFX.die(); state = ST_DYING; g.stateT = 0; pl.dead = true; pl.vel = {0, -650}; }

        // ---- 检查点 (x 超过中点即激活) ----
        if (!cpSaved && pl.pos.x > 73 * TILE_SIZE) {
          cpSaved = true; cpPos = {73.0f * TILE_SIZE, 12.0f * TILE_SIZE};
          g.checkpointOn = true; g.checkpoint = cpPos;
          g.popups.push_back({{pl.pos.x, pl.pos.y - 30}, "CHECKPOINT!", 1.4f, {120, 255, 120, 255}});
          SFX.power();
        }

        // ---- 玩家横向 (加速度+摩擦, 疾跑) ----
        int dir = 0;
        if (kb[SDL_SCANCODE_A] || kb[SDL_SCANCODE_LEFT]) dir -= 1;
        if (kb[SDL_SCANCODE_D] || kb[SDL_SCANCODE_RIGHT]) dir += 1;
        if (smoke) dir = 1;
        bool wantRun = kb[SDL_SCANCODE_LSHIFT] || kb[SDL_SCANCODE_RSHIFT] || kb[SDL_SCANCODE_J] || smoke;
        float maxSpd = wantRun ? RUN_MAX : WALK_MAX;
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
                  // 高处问号给蘑菇, 其余给金币 (鼓励探索)
                  bool giveShroom = (headR <= 10);
                  setT(g, c, headR, T_USED);
                  if (giveShroom) {
                    Shroom s; s.pos = {float(c*TILE_SIZE+2), float(headR*TILE_SIZE-28)};
                    s.active = true; g.shrooms.push_back(s); SFX.sprout();
                    g.popups.push_back({{float(c*TILE_SIZE), float(headR*TILE_SIZE-20)}, "POWER!", 1.2f, {255,220,80,255}});
                  } else {
                    g.score += 200; g.coinCount++;
                    SFX.coin();
                    burst(g, {float(c*TILE_SIZE+16), float(headR*TILE_SIZE-6)}, 8, {255,215,60,255}, 160, 700, 3);
                    g.popups.push_back({{float(c*TILE_SIZE), float(headR*TILE_SIZE-24)}, "+200", 0.8f, {255,230,90,255}});
                    if (g.coinCount % 30 == 0) { g.lives++; SFX.oneup(); g.popups.push_back({{pl.pos.x, pl.pos.y-40}, "1UP!", 1.4f, {120,255,120,255}}); }
                  }
                } else if (t == T_BRICK) {
                  if (pl.big) { setT(g, c, headR, T_EMPTY); g.score += 50; SFX.brk(); g.shake = 0.18f;
                    burst(g, {float(c*TILE_SIZE+16), float(headR*TILE_SIZE+16)}, 10, {200,90,30,255}, 220, 900, 4);
                  } else { SFX.bump(); }
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
                if (pl.invincible <= 0) {
                  if (pl.big) { pl.big = false; pl.size = {24,40}; pl.invincible = 2.0f; SFX.hurt();
                    burst(g,{pl.pos.x+12,pl.pos.y+20},10,{255,80,80,255},180,500,3);
                  } else { SFX.die(); state = ST_DYING; g.stateT = 0; pl.dead = true; pl.vel = {0,-650}; }
                }
              }
            }
        }

        // ---- 蘑菇 ----
        for (auto &s : g.shrooms) {
          if (!s.active || s.dead) continue;
          s.vel.y += GRAVITY * DT; if (s.vel.y > MAX_FALL) s.vel.y = MAX_FALL;
          s.pos.x += s.vel.x * DT;
          if (boxHitTile(g, s.pos, s.size)) {
            s.pos.x -= s.vel.x * DT;
            s.vel.x = -s.vel.x;
            s.pos.x += s.vel.x * DT;
          }
          s.pos.y += s.vel.y * DT;
          if (boxHitTile(g, s.pos, s.size)) {
            if (s.vel.y > 0) s.pos.y = floorf((s.pos.y+s.size.y)/TILE_SIZE)*TILE_SIZE - s.size.y;
            else s.pos.y = floorf(s.pos.y/TILE_SIZE)*TILE_SIZE + TILE_SIZE;
            s.vel.y = 0;
          }
          if (s.pos.y > WORLD_HEIGHT + 60) s.dead = true;
          if (aabb(pl.pos, pl.size, s.pos, s.size)) {
            s.dead = true; g.score += 1000; SFX.power();
            if (!pl.big) { pl.big = true; pl.size = {28, 52}; }
            pl.invincible = 1.0f;
            burst(g, {s.pos.x+14, s.pos.y}, 12, {255,80,80,255}, 180, 500, 4);
            g.popups.push_back({{s.pos.x, s.pos.y-20}, "+1000", 1.0f, {255,150,150,255}});
          }
        }

        // ---- 金币收集 ----
        for (auto &cn : g.coins) {
          if (cn.taken) continue;
          cn.anim += DT;
          gml::vec2 cp{cn.pos.x, cn.pos.y}, cs{28, 28};
          if (aabb(pl.pos, pl.size, cp, cs)) {
            cn.taken = true; g.score += 200; g.coinCount++; SFX.coin();
            burst(g, {cp.x+14, cp.y+14}, 8, {255,215,60,255}, 150, 600, 3);
            g.popups.push_back({{cp.x, cp.y-16}, "+200", 0.7f, {255,230,90,255}});
            if (g.coinCount % 30 == 0) { g.lives++; SFX.oneup(); g.popups.push_back({{pl.pos.x, pl.pos.y-40}, "1UP!", 1.4f, {120,255,120,255}}); }
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
          // 与玩家交互
          if (!pl.dead && aabb(pl.pos, pl.size, e.pos, e.size)) {
            bool stomp = pl.vel.y > 60 && (pl.pos.y + pl.size.y - e.pos.y) < 22;
            if (stomp) {
              e.alive = false; e.squashT = 0.4f;
              g.combo++;
              int pts = 100 * (1 << std::min(g.combo-1, 3)); // 连踩加倍 100/200/400/800
              g.score += pts; SFX.stomp();
              pl.pos.y = e.pos.y - pl.size.y - 1;
              pl.vel.y = (pl.jumpHeld ? -560 : -400); // 按住跳更高连跳
              pl.grounded = false;
              burst(g, {e.pos.x+14, e.pos.y+10}, 10, {255,255,255,255}, 170, 700, 4);
              char buf[32]; snprintf(buf, sizeof(buf), "+%d", pts);
              g.popups.push_back({{e.pos.x, e.pos.y-18}, buf, 0.9f, {255,255,255,255}});
              if (g.combo >= 2) g.popups.push_back({{e.pos.x, e.pos.y-38}, "COMBO!", 1.0f, {255,220,80,255}});
            } else {
              if (pl.invincible > 0) { /* 无敌穿过 */ }
              else if (pl.big) { pl.big = false; pl.size = {24,40}; pl.invincible = 2.0f; SFX.hurt();
                // 弹开敌人避免连续受伤
                e.vel.x = -e.vel.x;
                burst(g, {pl.pos.x+12, pl.pos.y+20}, 10, {255,80,80,255}, 180, 500, 3);
              } else { SFX.die(); state = ST_DYING; g.stateT = 0; pl.dead = true; pl.vel = {60,-650}; }
            }
          }
        }

        // ---- 掉坑 ----
        if (pl.pos.y > WORLD_HEIGHT + 40 && !pl.dead) {
          SFX.die(); state = ST_DYING; g.stateT = 0; pl.dead = true; pl.vel = {0, 0};
        }
        // ---- 到旗 ----
        float flagX = g.flagCol * TILE_SIZE + 6;
        if (pl.pos.x + pl.size.x >= flagX && pl.pos.y + pl.size.y > 5 * TILE_SIZE) {
          state = ST_WINANIM; g.stateT = 0; g.winT = 0;
          pl.pos.x = flagX - pl.size.x - 2;
          SFX.win();
        }

        if (pl.invincible > 0) pl.invincible -= DT;
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
          g.lives--;
          if (g.lives < 0) { state = ST_GAMEOVER; }
          else {
            // 回档: 重建地图, 保留分数/金币/命/时间, 出生点为检查点
            int sc = g.score, cc = g.coinCount, lv = g.lives;
            float tm = std::max(g.timeLeft, 60.0f);
            buildLevel(g);
            g.score = sc; g.coinCount = cc; g.lives = lv; g.timeLeft = tm;
            if (cpSaved) { g.checkpointOn = true; g.checkpoint = cpPos; }
            pl = Player();
            pl.pos = cpSaved ? cpPos : g.spawn;
            pl.size = {24, 40};
            cam.x = std::max(0.0f, std::min(pl.pos.x - WINDOW_WIDTH/2.0f, float(WORLD_WIDTH - WINDOW_WIDTH)));
            state = ST_PLAY;
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
          state = ST_WIN;
        }
      }
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

      acc -= DT;
    }

    // ================= 渲染 =================
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
        int fy = 5*TILE_SIZE+20 - camY + (state==ST_WINANIM ? (int)(g.stateT*60) : 0);
        if (fy > 12*TILE_SIZE-camY) fy = 12*TILE_SIZE-camY;
        SDL_SetRenderDrawColor(ren, 40, 200, 60, 255);
        SDL_Rect flag{fx-44, fy, 50, 32}; SDL_RenderFillRect(ren, &flag);
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        drawText(ren, "*", fx-28, fy+2, 2, {255,255,255,255});
        (void)wv;
        // 城堡
        int cx = 120*TILE_SIZE - camX, cy = 16*TILE_SIZE - camY;
        SDL_SetRenderDrawColor(ren, 200, 120, 60, 255);
        SDL_Rect cb{cx-30, cy-90, 110, 90}; SDL_RenderFillRect(ren, &cb);
        SDL_SetRenderDrawColor(ren, 160, 90, 40, 255);
        for (int i = 0; i < 4; ++i) { SDL_Rect mer{cx-30+i*30, cy-104, 20, 16}; SDL_RenderFillRect(ren, &mer); }
        SDL_SetRenderDrawColor(ren, 90, 50, 20, 255);
        SDL_Rect door{cx+12, cy-44, 26, 44}; SDL_RenderFillRect(ren, &door);
        SDL_Rect win1{cx-14, cy-70, 14, 14}, win2{cx+50, cy-70, 14, 14};
        SDL_SetRenderDrawColor(ren, 200, 230, 255, 255);
        SDL_RenderFillRect(ren, &win1); SDL_RenderFillRect(ren, &win2);
        // 检查点小旗
        if (true) {
          int qx = 74*TILE_SIZE - camX, qy = 16*TILE_SIZE - camY;
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
      snprintf(buf, sizeof(buf), "WORLD 1-1");
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
    } else if (state == ST_WIN) {
      SDL_SetRenderDrawColor(ren, 0, 0, 0, 170);
      SDL_Rect o{0,0,WINDOW_WIDTH,WINDOW_HEIGHT}; SDL_RenderFillRect(ren, &o);
      drawTextCenter(ren, "YOU WIN!", WINDOW_WIDTH/2, 150, 6, {120,255,120,255});
      char buf[64];
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

    SDL_RenderPresent(ren);
  }

  SDL_DestroyRenderer(ren);
  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}
