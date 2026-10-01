#include <SDL2/SDL.h>
#include <iostream>
#include <string>

#include "Audio.hpp"
#include "Common.hpp"
#include "Entities.hpp"
#include "GameLogic.hpp"
#include "Level.hpp"
#include "Renderer.hpp"
#include "Save.hpp"

// ============================================================================
// SUPER PLATFORMER - 瘦主函数: 仅负责 SDL 初始化 / 事件分发 / 固定步长调度
// 游戏状态收拢于 App (GameLogic.hpp), 更新逻辑在 GameLogic.cpp,
// 渲染在 Renderer.cpp, 关卡在 Level.cpp, 音效在 Audio.cpp。
// ============================================================================
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

  App app;
  buildLevel(app.game, app.currentLevelIndex);
  app.loadUnlockedOnly(); // 启动即读解锁进度 (标题屏显示 CONTINUE/选关)

  // 烟雾测试模式: 传参 --smoke 则跑 4 秒自动退出(供 CI/无头验证)
  app.smoke = argc > 1 && std::string(argv[1]) == "--smoke";
  float smokeT = 0;

  bool running = true;
  Uint32 last = SDL_GetTicks();
  float acc = 0; const float DT = 1.0f / 60.0f;

  while (running) {
    Uint32 now = SDL_GetTicks();
    float ft = (now - last) / 1000.0f; last = now;
    if (ft > 0.1f) ft = 0.1f;
    if (!app.paused) acc += ft;

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
        if (sym == SDLK_p && app.state == ST_PLAY) app.paused = !app.paused;
        if (sym == SDLK_r) { app.resetRun(); } // 按当前关索引重开 (分数清零, 地形按索引加载)
        // ---- 存档快捷键 ----
        if (sym == SDLK_F5 && (app.state == ST_PLAY || app.state == ST_NEXTLEVEL)) {
          app.saveProgress();
          app.game.popups.push_back({{app.player.pos.x, app.player.pos.y - 40}, "SAVED!", 1.2f, {120,255,120,255}});
          SFX.power();
        }
        if (sym == SDLK_F9) {
          // 任意状态读档继续 (标题/暂停/死亡后均可)
          if (app.loadProgress()) SFX.power();
        }
        if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER) {
          if (app.state == ST_TITLE) { app.resetToFirstLevel(); }
          else if (app.state == ST_WIN || app.state == ST_GAMEOVER) { app.resetToFirstLevel(); } // 通关/失败重试: 归零从第一关起
        }
        // ---- 标题屏: 继续 / 选关 / 清档 ----
        if (app.state == ST_TITLE) {
          if (sym == SDLK_c) { app.continueFromSave(); } // 继续上次进度
          if (sym == SDLK_x) { // 清档 (解锁归零, 本局不动)
            clearSaveFile();
            app.unlockedLevel = 0;
            SFX.bump();
          }
          // 数字选关 1..9,0(=10): 仅已解锁可跳, 全新分数开局
          int sel = -1;
          if (sym >= SDLK_1 && sym <= SDLK_9) sel = sym - SDLK_1;
          else if (sym == SDLK_0) sel = 9;
          if (sel >= 0) {
            if (!app.trySelectLevel(sel)) SFX.bump(); // 未解锁: 顶部反馈
            else SFX.power();
          }
        }
        if ((sym == SDLK_SPACE || sym == SDLK_w || sym == SDLK_UP) && app.state == ST_PLAY && !app.paused) {
          app.player.buffer = BUFFER_TIME; app.player.jumpHeld = true;
        }
        if (sym == SDLK_ESCAPE && app.state == ST_PLAY) app.paused = !app.paused;
      }
      if (ev.type == SDL_KEYUP) {
        auto sym = ev.key.keysym.sym;
        if (sym == SDLK_SPACE || sym == SDLK_w || sym == SDLK_UP) {
          app.player.jumpHeld = false;
          if (app.player.vel.y < JUMP_CUT) app.player.vel.y = JUMP_CUT; // 小跳: 松开即截断上升
        }
      }
    }
    if (app.smoke) { // 自动演示输入, 验证逻辑不崩
      smokeT += ft;
      if (app.state == ST_TITLE && smokeT > 0.3f) app.resetRun();
      if (smokeT > 4.0f) running = false;
    }

    const Uint8 *kb = SDL_GetKeyboardState(nullptr);

    while (acc >= DT) {
      updateFixedStep(app, DT, kb);
      acc -= DT;
    }

    // ================= 渲染 (全部下沉到 Renderer.cpp) =================
    renderAll(ren, app);
    SDL_RenderPresent(ren);
  }

  SDL_DestroyRenderer(ren);
  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}
