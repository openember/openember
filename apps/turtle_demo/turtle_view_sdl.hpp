/*
 * Copyright (c) 2022-2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <deque>
#include <utility>

// Minimal Linux SDL2 view for turtle_sim. Returns false from Pump() when the
// window is closed. No font dependency (libsdl2 only).

class TurtleViewSdl {
public:
    TurtleViewSdl(double world_half_extent, int window_size_px);
    ~TurtleViewSdl();

    TurtleViewSdl(const TurtleViewSdl&) = delete;
    TurtleViewSdl& operator=(const TurtleViewSdl&) = delete;

    bool Ok() const {
        return ok_;
    }

    // Process window events. Returns false when the user closes the window.
    bool Pump();

    void Draw(double x, double y, double theta);

private:
    void WorldToScreen(double x, double y, int* sx, int* sy) const;
    void DrawGrid();
    void DrawTrail();
    void DrawTurtle(double x, double y, double theta);

    double world_half_extent_ = 10.0;
    int window_size_ = 640;
    bool ok_ = false;
    void* window_ = nullptr;    // SDL_Window*
    void* renderer_ = nullptr;  // SDL_Renderer*
    std::deque<std::pair<double, double>> trail_;
};
