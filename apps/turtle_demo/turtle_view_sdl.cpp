/*
 * Copyright (c) 2022-2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 */

#include "turtle_view_sdl.hpp"

#include <SDL.h>

#include <cmath>
#include <iostream>

namespace {

constexpr std::size_t kMaxTrailPoints = 800;

void SetColor(SDL_Renderer* renderer, Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255) {
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
}

void DrawFilledCircle(SDL_Renderer* renderer, int cx, int cy, int radius) {
    for (int dy = -radius; dy <= radius; ++dy) {
        const int dx = static_cast<int>(
            std::lround(std::sqrt(static_cast<double>(radius * radius - dy * dy))));
        SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

}  // namespace

TurtleViewSdl::TurtleViewSdl(double world_half_extent, int window_size_px)
    : world_half_extent_(world_half_extent), window_size_(window_size_px) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return;
    }

    // Prefer software rendering: some remote/embedded X displays reject GLX and
    // abort with an X error instead of a clean SDL failure.
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "0");

    SDL_Window* window = SDL_CreateWindow(
        "OpenEmber TurtleSim",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        window_size_,
        window_size_,
        SDL_WINDOW_SHOWN);
    if (window == nullptr) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (renderer == nullptr) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    }
    if (renderer == nullptr) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }

    window_ = window;
    renderer_ = renderer;
    ok_ = true;
}

TurtleViewSdl::~TurtleViewSdl() {
    if (renderer_ != nullptr) {
        SDL_DestroyRenderer(static_cast<SDL_Renderer*>(renderer_));
        renderer_ = nullptr;
    }
    if (window_ != nullptr) {
        SDL_DestroyWindow(static_cast<SDL_Window*>(window_));
        window_ = nullptr;
    }
    if (ok_) {
        SDL_Quit();
    }
    ok_ = false;
}

bool TurtleViewSdl::Pump() {
    if (!ok_) {
        return false;
    }

    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
        if (event.type == SDL_QUIT) {
            ok_ = false;
            return false;
        }
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
            ok_ = false;
            return false;
        }
    }
    return true;
}

void TurtleViewSdl::WorldToScreen(double x, double y, int* sx, int* sy) const {
    const double span = 2.0 * world_half_extent_;
    const double u = (x + world_half_extent_) / span;
    const double v = (world_half_extent_ - y) / span;
    *sx = static_cast<int>(std::lround(u * (window_size_ - 1)));
    *sy = static_cast<int>(std::lround(v * (window_size_ - 1)));
}

void TurtleViewSdl::DrawGrid() {
    auto* renderer = static_cast<SDL_Renderer*>(renderer_);
    SetColor(renderer, 55, 65, 80);

    for (int i = -static_cast<int>(world_half_extent_);
         i <= static_cast<int>(world_half_extent_);
         ++i) {
        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
        WorldToScreen(static_cast<double>(i), -world_half_extent_, &x0, &y0);
        WorldToScreen(static_cast<double>(i), world_half_extent_, &x1, &y1);
        SDL_RenderDrawLine(renderer, x0, y0, x1, y1);

        WorldToScreen(-world_half_extent_, static_cast<double>(i), &x0, &y0);
        WorldToScreen(world_half_extent_, static_cast<double>(i), &x1, &y1);
        SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
    }

    SetColor(renderer, 90, 110, 140);
    int cx = 0;
    int cy = 0;
    WorldToScreen(0.0, 0.0, &cx, &cy);
    SDL_RenderDrawLine(renderer, 0, cy, window_size_ - 1, cy);
    SDL_RenderDrawLine(renderer, cx, 0, cx, window_size_ - 1);
}

void TurtleViewSdl::DrawTrail() {
    auto* renderer = static_cast<SDL_Renderer*>(renderer_);
    if (trail_.size() < 2) {
        return;
    }

    SetColor(renderer, 80, 200, 255, 220);
    for (std::size_t i = 1; i < trail_.size(); ++i) {
        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
        WorldToScreen(trail_[i - 1].first, trail_[i - 1].second, &x0, &y0);
        WorldToScreen(trail_[i].first, trail_[i].second, &x1, &y1);
        SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
    }
}

void TurtleViewSdl::DrawTurtle(double x, double y, double theta) {
    auto* renderer = static_cast<SDL_Renderer*>(renderer_);
    int sx = 0;
    int sy = 0;
    WorldToScreen(x, y, &sx, &sy);

    const double nose = 0.7;
    const double wing = 0.45;
    const double c = std::cos(theta);
    const double s = std::sin(theta);

    const double nx = x + nose * c;
    const double ny = y + nose * s;
    const double lx = x - wing * c + wing * s;
    const double ly = y - wing * s - wing * c;
    const double rx = x - wing * c - wing * s;
    const double ry = y - wing * s + wing * c;

    int px[3] = {0, 0, 0};
    int py[3] = {0, 0, 0};
    WorldToScreen(nx, ny, &px[0], &py[0]);
    WorldToScreen(lx, ly, &px[1], &py[1]);
    WorldToScreen(rx, ry, &px[2], &py[2]);

    SetColor(renderer, 255, 170, 40);
    SDL_RenderDrawLine(renderer, px[0], py[0], px[1], py[1]);
    SDL_RenderDrawLine(renderer, px[1], py[1], px[2], py[2]);
    SDL_RenderDrawLine(renderer, px[2], py[2], px[0], py[0]);

    // Fill by fan lines from the nose for a solid-looking turtle.
    for (int i = 0; i <= 12; ++i) {
        const double t = static_cast<double>(i) / 12.0;
        const int mx = static_cast<int>(std::lround(px[1] + t * (px[2] - px[1])));
        const int my = static_cast<int>(std::lround(py[1] + t * (py[2] - py[1])));
        SDL_RenderDrawLine(renderer, px[0], py[0], mx, my);
    }

    SetColor(renderer, 255, 220, 120);
    DrawFilledCircle(renderer, sx, sy, 3);
}

void TurtleViewSdl::Draw(double x, double y, double theta) {
    if (!ok_) {
        return;
    }

    if (trail_.empty() ||
        std::hypot(trail_.back().first - x, trail_.back().second - y) > 0.05) {
        trail_.emplace_back(x, y);
        while (trail_.size() > kMaxTrailPoints) {
            trail_.pop_front();
        }
    }

    auto* renderer = static_cast<SDL_Renderer*>(renderer_);
    SetColor(renderer, 24, 28, 36);
    SDL_RenderClear(renderer);
    DrawGrid();
    DrawTrail();
    DrawTurtle(x, y, theta);
    SDL_RenderPresent(renderer);
}
