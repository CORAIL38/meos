#pragma once

/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2026 Melin Software HB

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
class oRunner;
#include "gdifonts.h"
#include "gdioutput.h"

struct SpeakerString {
  wstring str;
  int format;
  bool hasTimer;
  int timer; // Timer time (in normal time units)
  int timeout; // Timeout time (in seconds)
  string moveKey;
  GDICOLOR color;
  SpeakerString() : format(0), hasTimer(false), timer(0), timeout(NOTIMEOUT), color(colorDefault) {}
  SpeakerString(int formatIn, const wstring &in) : str(in), format(formatIn), hasTimer(false),
                                           timer(0), timeout(NOTIMEOUT),  color(colorDefault) {}
  SpeakerString(int formatIn, int timerIn, int timeoutIn = NOTIMEOUT) : format(formatIn),
                                           hasTimer(true), timer(timerIn),
                                           timeout(timeoutIn), color(colorDefault) {}
};

class oSpeakerObject {
public:
  struct RunningTime {
    void reset() { time = 0; preliminary = 0; }
    int time = 0;
    int preliminary = 0;
    RunningTime() = default;
  };

  struct ResultAtControl {
    RunningTime runningTime;
    RunningTime runningTimeLeg;
    RunningTime runningTimeSinceLast;

    int place = 0;
    int parallelScore = 0;
    RunnerStatus status = StatusUnknown;

    bool hasResult() const { return status == StatusOK || status == StatusUnknown && runningTime.time > 0; }
    bool isIncomming() const { return status == StatusUnknown && runningTime.time <= 0; }

    // For parallel legs
    int runnersFinishedLeg = 0;
    bool restarted = false;
  };

  void reset() {
    owner = 0;
    bib.clear();
    names.clear();
    outgoingnames.clear();
    resultRemark.clear();
    club.clear();
    startTimeS.clear();
    finishStatus = StatusUnknown;
    useSinceLast = 0;
    result.clear();
  }

  size_t size() const {
    return result.size();
  }

  const ResultAtControl& operator[](size_t ix) const {
    return result[ix];
  }

  oRunner *owner = nullptr;
  wstring bib;
  vector<wstring> names;
  vector<wstring> outgoingnames;
  string resultRemark;
  wstring club;
  wstring startTimeS;
  vector<ResultAtControl> result;

  int compareResultIndex = -1;
  int nextPreliminaryTime = -1;

  // For parallel legs
  int runnersTotalLeg = 0;

  RunnerStatus finishStatus = StatusUnknown;

  bool hasResult(int where) const { return where < result.size() && result[where].hasResult(); }
  bool isIncomming(int where) const { return where < result.size() && result[where].isIncomming(); }
  
  // In seconds. Negative if undefined.
  int timeSinceChange = -1;

  int priority = 0;
  bool missingStartTime = false;
  bool isRendered = false;
  bool useSinceLast = false;

  bool highlight(int timeToHighlightSecond) const {
    return timeSinceChange < timeToHighlightSecond * timeConstSecond && timeSinceChange >= 0;
  }

  oSpeakerObject() = default;
};
