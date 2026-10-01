#include "Physics.hpp"
#include "Entities.hpp"
#include "Level.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>

bool boxHitTile(const Game &g, const gml::vec2 &pos, const gml::vec2 &sz) {
  int c0 = (int)floor(pos.x / TILE_SIZE), c1 = (int)floor((pos.x + sz.x - 0.01f) / TILE_SIZE);
  int r0 = (int)floor(pos.y / TILE_SIZE), r1 = (int)floor((pos.y + sz.y - 0.01f) / TILE_SIZE);
  c0 = std::max(0, std::min(c0, MAP_COLS-1)); c1 = std::max(0, std::min(c1, MAP_COLS-1));
  r0 = std::max(0, std::min(r0, MAP_ROWS-1)); r1 = std::max(0, std::min(r1, MAP_ROWS-1));
  for (int r = r0; r <= r1; ++r)
    for (int c = c0; c <= c1; ++c) {
      int t = g.tiles[tidx(c, r)];
      if (solidTile(t)) return true;
    }
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