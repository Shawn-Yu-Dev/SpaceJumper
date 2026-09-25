#pragma once
// Audio.hpp — 复古音效合成接口 (SDL QueueAudio, 无外部资源)
// SFX 全局实例仅在此声明为 extern, 定义在 Audio.cpp。
#include <SDL2/SDL.h>

struct Sfx {
  SDL_AudioDeviceID dev = 0;
  bool ok = false, muted = false;
  void init();
  void tone(int f0, int f1, int ms, int vol = 7000, int wave = 0);
  void jump();
  void coin();
  void stomp();
  void bump();
  void brk();
  void sprout();
  void power();
  void hurt();
  void die();
  void kick();
  void oneup();
  void skid();
  void spring();
  void win();
};

extern Sfx SFX;
