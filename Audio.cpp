#include "Audio.hpp"
#include <cmath>
#include <cstdlib>
#include <vector>

// SFX 的唯一实例化点 (对应 Audio.hpp 中的 extern 声明)
Sfx SFX;

void Sfx::init() {
  SDL_AudioSpec want{}, have{};
  want.freq = 22050; want.format = AUDIO_S16SYS;
  want.channels = 1; want.samples = 1024;
  dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
  if (dev) { ok = true; SDL_PauseAudioDevice(dev, 0); }
}

void Sfx::tone(int f0, int f1, int ms, int vol, int wave) {
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

void Sfx::jump()   { tone(280, 720, 130); }
void Sfx::coin()   { tone(988, 988, 70); tone(1319, 1319, 220); }
void Sfx::stomp()  { tone(420, 90, 160); }
void Sfx::bump()   { tone(180, 110, 110); }
void Sfx::brk()    { tone(300, 60, 220, 7000, 2); }
void Sfx::sprout() { tone(300, 900, 320); }
void Sfx::power()  { tone(500, 1100, 260); tone(700, 1400, 200); }
void Sfx::hurt()   { tone(500, 90, 450); }
void Sfx::die()    { tone(600, 80, 700); }
void Sfx::kick()   { tone(600, 1200, 120); }
void Sfx::oneup()  { tone(660,660,90); tone(880,880,90); tone(1320,1320,260); }
void Sfx::skid()   { tone(900, 500, 60, 2500); }
void Sfx::spring() { tone(200, 900, 180); }
void Sfx::win()    { tone(523,523,120); tone(659,659,120); tone(784,784,120); tone(1046,1046,320); }
