﻿#pragma once

/************************************************************************
MeOS - Orienteering Software
Copyright (C) 2009-2025 Melin Software HB

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

Melin Software HB - software@melin.nu - www.melin.nu
Eksoppsvägen 16, SE-75646 UPPSALA, Sweden

************************************************************************/

#include "gdioutput.h"
#include <thread>
#include <atomic>

class AnimationData : public GuiHandler {
  vector<RenderedPage> pages;
  int width;
  int height;
  int nCol;
  int margin;
  bool animate;

  int page;
  uint64_t lastTime;
  uint64_t nextTime;
  DWORD timeOut;
  bool doAnimation;
  std::atomic_bool errorState;
  gdioutput *gdiRef;

  void renderSubPage(HDC hDC, gdioutput &gdi, RenderedPage &page, int x, int y, int animationDelay);

  void doRender(HDC hDC, gdioutput &gdi, size_t sp, int delay);

  shared_ptr<std::thread> animationThread;
  shared_ptr<AnimationData> delayedTakeOver;
  void takeOverInternal(const shared_ptr<AnimationData> &other);

  void threadRender(gdioutput *gdi, size_t sp, int delay);
public:

  AnimationData(gdioutput &gdi, int timePerPage, int nCol, 
                int marginPercent, bool animate, bool respectPageBreak);
  ~AnimationData();

  void handle(gdioutput &gdi, BaseInfo &info, GuiEventType type);
  bool takeOver(const shared_ptr<AnimationData> &other);

  void renderPage(HDC hDC, gdioutput &gdi, uint64_t time);
};
