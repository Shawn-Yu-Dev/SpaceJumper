#pragma once
// Font.hpp — 3x5 像素字体绘制接口
// FONT 点阵表封装在 Font.cpp 内部 (static), 不暴露为全局变量。
#include <SDL2/SDL.h>
#include <string>

void drawText(SDL_Renderer *r, const std::string &text, int x, int y, int scale,
              SDL_Color col);
int textWidth(const std::string &t, int scale);
void drawTextCenter(SDL_Renderer *r, const std::string &t, int cx, int y,
                    int scale, SDL_Color col);
