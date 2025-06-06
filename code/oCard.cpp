﻿/************************************************************************
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

// oCard.cpp: implementation of the oCard class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "oCard.h"
#include "oEvent.h"
#include "gdioutput.h"
#include "table.h"
#include "Localizer.h"
#include "meos_util.h"

#include <algorithm>
#include <cassert>

#include "SportIdent.h"
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

oCard::oCard(oEvent *poe): oBase(poe)
{
  Id=oe->getFreeCardId();
  cardNo=0;
  readId=0;
  tOwner=0;
}

oCard::oCard(oEvent *poe, int id): oBase(poe)
{
  Id=id;
  cardNo=0;
  readId=0;
  tOwner=0;
  oe->qFreeCardId = max(id, oe->qFreeCardId);
}

oCard::~oCard()
{

}

bool oCard::Write(xmlparser &xml)
{
  if (Removed) return true;
  xml.startTag("Card");
  xml.write("CardNo", cardNo);
  xml.write("Punches", getPunchString());
  xml.write("ReadId", int(readId));
  xml.write("Voltage", miliVolt);
  xml.write("BatteryDate", batteryDate);
  xml.write("Id", Id);
  xml.write("Updated", getStamp());
  xml.endTag();

  return true;
}

void oCard::Set(const xmlobject &xo)
{
  xmlList xl;
  xo.getObjects(xl);
  xmlList::const_iterator it;

  for(it=xl.begin(); it != xl.end(); ++it){
    if (it->is("CardNo")){
      cardNo = it->getInt();
    }
    else if (it->is("Voltage")) {
      miliVolt = it->getInt();
    }
    else if (it->is("BatteryDate")) {
      batteryDate = it->getInt();
    }
    else if (it->is("Punches")){
      importPunches(it->getRawStr());
    }
    else if (it->is("ReadId")){
      readId = it->getInt(); // Coded as signed int
    }
    else if (it->is("Id")){
      Id = it->getInt();
    }
    else if (it->is("Updated")){
      Modified.setStamp(it->getRawStr());
    }
  }
}

pair<int, int> oCard::getCardHash() const {
  int a = cardNo;
  int b = readId;

  for (auto &p : punches) {
    a = a * 31 + p.punchTime * 997 + p.getTypeCode();
    b = b * 41 + p.punchTime * 97 + p.getTypeCode();
  }
  return make_pair(a, b);
}

void oCard::setCardNo(int c)
{
  if (cardNo!=c)
    updateChanged();

  cardNo=c;
}

const wstring &oCard::getCardNoString() const {
  return itow(cardNo);
}

void oCard::addPunch(int type, int time, int matchControlId, int unit, PunchOrigin origin) {
  oPunch p(oe);
  p.punchTime = time;
  p.type = type;
  p.tMatchControlId = matchControlId;
  p.isUsed =  matchControlId!=0;
  p.punchUnit = unit;
  if (origin == PunchOrigin::Original)
    p.origin = p.computeOrigin(oe->getZeroTimeNum() + time, type);
  else if (origin == PunchOrigin::Manual)
    p.origin = -1;

  if (punches.empty())
    punches.push_back(p);
  else {
    oPunch oldBack = punches.back();
    if (oldBack.isFinish()) { //Make sure finish is last.
      punches.pop_back();
      punches.push_back(p);
      punches.push_back(oldBack);
    }
    else
      punches.push_back(p);
  }
  updateChanged();
}

const string &oCard::getPunchString() const {
  punchString.clear();
  punchString.reserve(punches.size() * 16);
  for(auto &p : punches) {
    p.appendCodeString(punchString);
  }
  return punchString;
}

void oCard::importPunches(const string &s) {
  int startpos = 0;
  int endpos;

  endpos = s.find_first_of(';', startpos);
  punches.clear();

  while (endpos != string::npos) {
    oPunch p(oe);
    p.decodeString(s.c_str() + startpos);
    punches.push_back(p);
    startpos = endpos + 1;
    endpos = s.find_first_of(';', startpos);
  }
  return;
}

bool oCard::fillPunches(gdioutput &gdi, const string &name, oCourse *crs) {
  oPunchList::iterator it;
  synchronize(true);
  int ix = 0;
  for (it=punches.begin(); it != punches.end(); ++it) {
    it->tCardIndex = ix++;
  }

  gdi.clearList(name);

  bool showStart = crs ? !crs->useFirstAsStart() : true;
  bool showFinish = crs ? !crs->useLastAsFinish() : true;

  bool hasStart=false;
  bool hasFinish=false;
  bool extra=false;
  int k=0;
  int currentTimeAdjust = 0;
  pControl ctrl=0;

  int matchPunch=0;
  int punchRemain=1;
  bool hasRogaining = false;
  if (crs) {
    ctrl=crs->getControl(matchPunch);
    hasRogaining = crs->hasRogaining();
  }
  if (ctrl)
    punchRemain=ctrl->getNumMulti();

  map<int, pair<int, pPunch > > rogainingIndex;
  
  if (crs) {
    for (it=punches.begin(); it != punches.end(); ++it) {
      if (it->tRogainingIndex >= 0) {
        rogainingIndex[it->tRogainingIndex] = make_pair(it->tCardIndex, &*it);
      }
      ix++;
    }
  }

  for (it=punches.begin(); it != punches.end(); ++it){
    if (!hasStart && !it->isStart()){
      if (it->isUsed){
        if (showStart)
          gdi.addItem(name, lang.tl("Start")+L"\t\u2013", -1);
        hasStart=true;
      }
    }

    if (crs && it->tRogainingIndex != -1)
      continue;

    {
      if (it->isStart())
        hasStart=true;
      else if (crs && it->isUsed && !it->isFinish() &&  !it->isCheck()) {
        while(ctrl && it->tMatchControlId!=ctrl->getId()) {
          if (ctrl->isRogaining(hasRogaining)) {
            if (rogainingIndex.count(matchPunch) == 1)
              gdi.addItem(name, rogainingIndex[matchPunch].second->getString(),
                                rogainingIndex[matchPunch].first);
            else
              gdi.addItem(name, L"\u2013\t\u2013", -1);
          }
          else {
            while(0<punchRemain--) {
              gdi.addItem(name, L"\u2013\t\u2013", -1);
            }
          }
          // Next control
          ctrl=crs ? crs->getControl(++matchPunch):0;
          punchRemain=ctrl ? ctrl->getNumMulti() : 1;
        }
      }


      if ((!crs || it->isUsed) || (showFinish && it->isFinish()) || (showStart && it->isStart())) {
        if (it->isFinish() && hasRogaining && crs) {
          while (ctrl) {
            if (ctrl->isRogaining(hasRogaining)) {
              // Check if we have reach finihs without adding rogaining punches
              while (ctrl && ctrl->isRogaining(hasRogaining)) {
                if (rogainingIndex.count(matchPunch) == 1)
                  gdi.addItem(name, rogainingIndex[matchPunch].second->getString(),
                                    rogainingIndex[matchPunch].first);
                else
                  gdi.addItem(name, L"\u2013\t\u2013", -1);
                ctrl = crs->getControl(++matchPunch);
              }
              punchRemain = ctrl ? ctrl->getNumMulti() : 1;
            }
            else {
              gdi.addItem(name, L"\u2013\t\u2013", -1);
              ctrl = crs->getControl(++matchPunch);
            }
          }
        }

        if (it->isFinish() && crs) { //Add missing punches before the finish
          while(ctrl) {
            gdi.addItem(name, L"\u2013\t\u2013", -1);
            ctrl = crs->getControl(++matchPunch);
          }
        }

        wstring tadj;
        if (currentTimeAdjust != it->tTimeAdjust.second) {
          int adj = it->tTimeAdjust.second - currentTimeAdjust;
          currentTimeAdjust = it->tTimeAdjust.second;
          if (adj > 0)
            tadj = L" +" + formatTime(adj);
          else
            tadj = L" -" + formatTime(-adj);
        }
        gdi.addItem(name, it->getString() + tadj, it->tCardIndex);

        if (!(it->isFinish() || it->isStart())) {
          punchRemain--;
          if (punchRemain<=0) {
            // Next contol
            ctrl = crs ? crs->getControl(++matchPunch):0;

            // Match rogaining here
            while (ctrl && ctrl->isRogaining(hasRogaining)) {
              if (rogainingIndex.count(matchPunch) == 1)
                gdi.addItem(name, rogainingIndex[matchPunch].second->getString(),
                                  rogainingIndex[matchPunch].first);
              else
                gdi.addItem(name, L"\u2013\t\u2013", -1);
              ctrl = crs->getControl(++matchPunch);
            }
            punchRemain = ctrl ? ctrl->getNumMulti() : 1;
          }
        }
      }
      else
        extra=true;

      k++;

      if (it->isFinish() && showFinish)
        hasFinish=true;
    }
  }

  if (!hasStart && showStart)
    gdi.addItem(name, lang.tl("Start")+L"\t\u2013", -1);

  if (!hasFinish && showFinish) {

    while (ctrl) {
      if (ctrl->isRogaining(hasRogaining)) {
        // Check if we have reach finihs without adding rogaining punches
        while (ctrl && ctrl->isRogaining(hasRogaining)) {
          if (rogainingIndex.count(matchPunch) == 1)
            gdi.addItem(name, rogainingIndex[matchPunch].second->getString(),
                              rogainingIndex[matchPunch].first);
          else
            gdi.addItem(name, L"\u2013\t\u2013", -1);
          ctrl = crs->getControl(++matchPunch);
        }
        punchRemain = ctrl ? ctrl->getNumMulti() : 1;
      }
      else {
        gdi.addItem(name, L"-\t-", -1);
        ctrl = crs->getControl(++matchPunch);
      }
    }

    gdi.addItem(name, lang.tl("Mål")+L"\t\u2013", -1);
  }

  if (extra) {
    //Show punches that are not used.
    k=0;
    gdi.addItem(name, L"", -1);
    gdi.addItem(name, lang.tl("Extra stämplingar"), -1);
    for (it=punches.begin(); it != punches.end(); ++it) {
      if (!it->isUsed && !(it->isFinish() && showFinish) && !(it->isStart() && showStart))
        gdi.addItem(name, it->getString(), it->tCardIndex);
    }
  }
  return true;
}


void oCard::insertPunchAfter(int pos, int type, int time)
{
  if (pos==1023)
    return;

  oPunchList::iterator it;

  oPunch punch(oe);
  punch.punchTime=time;
  punch.type=type;

  int k=-1;
  for (it=punches.begin(); it != punches.end(); ++it) {
    if (k==pos) {
      updateChanged();
      punches.insert(it, punch);
      return;
    }
    k++;
  }

  updateChanged();
  //Insert last
  punches.push_back(punch);
}

void oCard::deletePunch(pPunch pp)
{
  if (pp == 0)
    throw std::exception("Punch not found");
  int k=0;
  oPunchList::iterator it;

  for (it=punches.begin(); it != punches.end(); ++it) {
    if (&*it == pp) {
      punches.erase(it);
      updateChanged();
      return;
    }
    k++;
  }
}

wstring oCard::getInfo() const
{
  wchar_t bf[128];
  swprintf_s(bf, lang.tl("Löparbricka %d").c_str(), cardNo);
  return bf;
}

oPunch *oCard::getPunch(const pPunch punch)
{
  int k=0;
  oPunchList::iterator it;

  for (it=punches.begin(); it != punches.end(); ++it) {
    if (&*it == punch) return &*it;
    k++;
  }
  return 0;
}


oPunch *oCard::getPunchByType(int Type) const
{
  oPunchList::const_iterator it;

  for (it=punches.begin(); it != punches.end(); ++it)
    if (it->type==Type)
      return pPunch(&*it);

  return 0;
}

oPunch *oCard::getPunchById(int courseControlId) const
{
  pair<int, int> idix = oControl::getIdIndexFromCourseControlId(courseControlId);
  oPunchList::const_iterator it;
  pPunch res = 0;
  for (it=punches.begin(); it != punches.end(); ++it) {
    if (it->tMatchControlId==idix.first) {
      res = pPunch(&*it);
      if (idix.second == 0)
        return res;
      --idix.second; // Not this match, decrease count
    }
  }
  return 0; // Punch not found
}


oPunch *oCard::getPunchByIndex(int ix) const
{
  oPunchList::const_iterator it;
  for (it=punches.begin(); it != punches.end(); ++it) {
    if (0 == ix--)
      return pPunch(&*it);
  }
  return 0;
}


void oCard::setReadId(const SICard &card)
{
  updateChanged();
  readId=card.calculateHash();
}

bool oCard::isCardRead(const SICard &card) const
{
  if (readId==card.calculateHash())
    return true;
  else return false;
}

void oCard::getSICard(SICard &card) const {
  card.clear(0);
  card.CardNumber = cardNo;
  card.convertedTime = ConvertedTimeStatus::Done;
  oPunchList::const_iterator it;
  for (it = punches.begin(); it != punches.end(); ++it) {
    if (it->type>30)
      card.Punch[card.nPunch++].Code = it->type;
  }
}


pRunner oCard::getOwner() const {
  return tOwner && !tOwner->isRemoved() ? tOwner : 0;
}

bool oCard::setPunchTime(const pPunch punch, const wstring& time)
{
  oPunch* op = getPunch(punch);
  if (!op) return false;

  DWORD ot = op->punchTime;
  op->setTime(time);

  if (ot != op->punchTime)
    updateChanged();

  return true;
}


pCard oEvent::getCard(int Id) const
{  // Do allow removed cards
  if (Id < int(Cards.size() / 2)) {
    for (oCardList::const_iterator it = Cards.begin(); it != Cards.end(); ++it){
      if (it->Id==Id)
        return const_cast<pCard>(&*it);
    }
  }
  else {
    for (oCardList::const_reverse_iterator it = Cards.rbegin(); it != Cards.rend(); ++it){
      if (it->Id==Id)
        return const_cast<pCard>(&*it);
    }
  }
  return 0;
}

void oEvent::getCards(vector<pCard>& cards, bool synchronize, bool onlyUnpaired) {
  if (synchronize)
    synchronizeList(oListId::oLCardId);

  cards.clear();
  cards.reserve(Cards.size());

  for (auto &c : Cards) {
    if (!c.isRemoved()) {
      if (onlyUnpaired && c.getOwner() != nullptr)
        continue;

      cards.push_back(&c);
    }
  }
}

pCard oEvent::addCard(const oCard &oc)
{
  if (oc.Id<=0)
    return 0;

  Cards.push_back(oc);
  Cards.back().tOwner = nullptr;
  Cards.back().addToEvent(this, &oc);
  qFreeCardId = max(oc.Id, qFreeCardId);
  return &Cards.back();
}

pCard oEvent::getCardByNumber(int cno) const
{
  oCardList::const_reverse_iterator it;
  pCard second = 0;
  for (it=Cards.rbegin(); it != Cards.rend(); ++it){
    if (!it->isRemoved() && it->cardNo==cno) {
      if (it->getOwner() == nullptr)
        return const_cast<pCard>(&*it);
      else if (second == 0)
        second = const_cast<pCard>(&*it);
    }
  }
  return second;
}

bool oEvent::isCardRead(const SICard &card) const
{
  oCardList::const_iterator it;

  for(it=Cards.begin(); it!=Cards.end(); ++it) {
    if (it->isRemoved())
      continue;

    if (it->cardNo==card.CardNumber && it->isCardRead(card))
      return true;
  }

  return false;
}

const shared_ptr<Table> &oCard::getTable(oEvent *oe) {
  if (!oe->hasTable("cards")) {
    auto table = make_shared<Table>(oe, 20, L"Brickor", "cards");

    table->addColumn("Id", 70, true, true);
    table->addColumn("Ändrad", 70, false);

    table->addColumn("Bricka", 120, true);
    table->addColumn("Deltagare", 200, false);
    table->addColumn("Spänning", 70, false);
    table->addColumn("Batteridatum", 100, false);

    table->addColumn("Starttid", 70, false);
    table->addColumn("Måltid", 70, false);
    table->addColumn("Stämplingar", 70, true);

    table->setTableProp(Table::CAN_DELETE);
    oe->setTable("cards", table);
  }
  
  return oe->getTable("cards");
}

void oEvent::generateCardTableData(Table &table, oCard *addCard)
{
  if (addCard) {
    addCard->addTableRow(table);
    return;
  }

  oCardList::iterator it;
  synchronizeList({ oListId::oLCardId, oListId::oLRunnerId });

  for (it=Cards.begin(); it!=Cards.end(); ++it) {
    if (!it->isRemoved()) {
      it->addTableRow(table);
    }
  }
}

void oCard::addTableRow(Table &table) const {

  wstring runner(lang.tl("Oparad"));
  if (getOwner())
    runner = tOwner->getNameAndRace(true);

  oCard &it = *pCard(this);
  table.addRow(getId(), &it);

  int row = 0;
  table.set(row++, it, TID_ID, itow(getId()), false);
  table.set(row++, it, TID_MODIFIED, getTimeStamp(), false);

  table.set(row++, it, TID_CARD, getCardNoString(), true, cellAction);

  table.set(row++, it, TID_RUNNER, runner, true, cellAction);

  table.set(row++, it, TID_VOLTAGE, getCardVoltage(), false, cellAction);

  table.set(row++, it, TID_BATTERYDATE, getBatteryDate(), false, cellAction);


  oPunch *p=getPunchByType(oPunch::PunchStart);
  wstring time;
  if (p)
    time = p->getTime(false, SubSecond::Auto);
  else
    time = makeDash(L"-");
  table.set(row++, it, TID_START, time, false, cellEdit);

  p = getPunchByType(oPunch::PunchFinish);
  if (p)
    time = p->getTime(false, SubSecond::Auto);
  else
    time = makeDash(L"-");
  
  table.set(row++, it, TID_FINISH, time, false, cellEdit);

  table.set(row++, it, TID_COURSE, itow(getNumPunches()), false, cellEdit);
}

oDataContainer &oCard::getDataBuffers(pvoid &data, pvoid &olddata, pvectorstr &strData) const {
  throw std::exception("Unsupported");
}

int oCard::getSplitTime(int startTime, const pPunch punch) const {

  for (oPunchList::const_iterator it = punches.begin(); it != punches.end(); ++it) {
    if (&*it == punch) {
      int t = it->getAdjustedTime();
      if (t<=0)
        return -1;

      if (startTime > 0)
        return t - startTime;
      else
        return -1;
    }
    else if (it->isUsed)
      startTime = it->getAdjustedTime();
  }
  return -1;
}


wstring oCard::getRogainingSplit(int ix, int startTime) const
{
  oPunchList::const_iterator it;
  for (it = punches.begin(); it != punches.end(); ++it) {
    int t = it->getAdjustedTime();
    if (0 == ix--) {
      if (t > 0 && t > startTime)
        return formatTime(t - startTime);
    }
    if (it->isUsed)
      startTime = t;
  }
 return makeDash(L"-");
}

void oCard::remove() {
  if (oe)
    oe->removeCard(Id);
}

bool oCard::canRemove() const {
  return getOwner() == 0;
}

pair<int, int> oCard::getTimeRange() const {
  pair<int, int> t(24*timeConstHour, 0);
  bool finishLock = false;

  for(auto it = punches.begin(); it != punches.end(); ++it) {
    if (it->hasTime()) {
      int pt = it->getTimeInt();
      t.first = min(t.first, pt);
      if (it->isFinish()) {
        t.second = it->getTimeInt(); // Do not allow times after finish
        finishLock = true;
      }
      if (!finishLock && !it->isCheck() && !it->isStart())
        t.second = max(t.second, pt);
    }
  }
  return t;
}

void oCard::getPunches(vector<pPunch> &punchesOut) const {
  punchesOut.clear();
  punchesOut.reserve(punches.size());
  for(oPunchList::const_iterator it = punches.begin(); it != punches.end(); ++it) {
    punchesOut.push_back(pPunch(&*it));
  }
}

void oCard::setupFromRadioPunches(oRunner &r) {
  oe->synchronizeList(oListId::oLPunchId);
  vector<pFreePunch> p;
  oe->getPunchesForRunner(r.getId(), true, p);

  for (size_t k = 0; k < p.size(); k++)
    addPunch(p[k]->type, p[k]->punchTime, 0, p[k]->punchUnit, p[k]->isOriginal() ? oCard::PunchOrigin::Original : oCard::PunchOrigin::Manual);
  
  cardNo = r.getCardNo();
  readId = ConstructedFromPunches; //Indicates
}

void oCard::changedObject() {
  if (tOwner)
    tOwner->changedObject();

  oe->sqlCards.changed = true;
}

int oCard::getNumControlPunches(int startPunchType, int finishPunchType) const {
  int count = 0;
  for(oPunchList::const_iterator it = punches.begin(); it != punches.end(); ++it) {
    if (it->isFinish(finishPunchType) || it->isCheck() || it->isStart(startPunchType)) {
      continue;
    }
    count++;
  }
  return count;
}

void oCard::adaptTimes(int startTime) {
  int st = -1;
  oPunchList::iterator it = punches.begin();
  while (it != punches.end()) {
    if (it->hasTime()) {
      st = it->getTimeInt();
      break;
    }
    ++it;
  }
  
  if (st == -1)
    return;

  const int h24 = 24 * timeConstHour;
  int offset = st / h24;
  if (offset > 0) {
    for (it = punches.begin(); it != punches.end(); ++it) {
      if (it->hasTime() && it->getTimeInt() < offset * h24)
        return; // Inconsistent, do nothing
    }
         
    for (it = punches.begin(); it != punches.end(); ++it) {
      if (it->hasTime())
        it->punchTime -= offset * h24;
    }
    updateChanged();
  }

  if (startTime >= h24) {
    offset = startTime / h24;
    for (it = punches.begin(); it != punches.end(); ++it) {
      if (it->hasTime())
        it->punchTime += offset * h24;
    }
    updateChanged();
  }
}

wstring oCard::getCardVoltage() const {
  return getCardVoltage(miliVolt);
}

wstring oCard::getCardVoltage(int miliVolt) {
  if (miliVolt <= 10)
    return L"";
  int vi = miliVolt / 1000;
  int vd = (miliVolt % 1000) / 10;

  wchar_t bf[64];
  swprintf_s(bf, L"%d.%02d V", vi, vd);
  return bf;
}

oCard::BatteryStatus oCard::isCriticalCardVoltage() const {
  return isCriticalCardVoltage(miliVolt);
}

oCard::BatteryStatus oCard::isCriticalCardVoltage(int miliVolt)  {
  if (miliVolt > 10 && miliVolt < 2445)
    return BatteryStatus::Bad;
  else if (miliVolt > 10 && miliVolt <= 2710)
    return BatteryStatus::Warning;

  return BatteryStatus::OK;
}

wstring oCard::getBatteryDate() const {
  if (batteryDate > 0)
    return formatDate(batteryDate, false);
  return _EmptyWString;
}

oCard::PunchOrigin oCard::isOriginalCard() const {
  bool isUnknown = false;
  for (auto& p : punches) {
    if (p.origin == 0) {
      isUnknown = true;
    }
    else {
      if (!p.isOriginal())
        return PunchOrigin::Manual;
    }
  }
  if (isUnknown)
    return PunchOrigin::Unknown;
  else
    return PunchOrigin::Original;
}

int oCard::getStartPunchCode() const {
  auto it = punches.begin();
  if (it != punches.end()) {
    if (it->getTypeCode() == oPunch::SpecialPunch::PunchStart)
      return it->getPunchUnit();
    ++it;
    if (it != punches.end()) {
      if (it->getTypeCode() == oPunch::SpecialPunch::PunchStart)
        return it->getPunchUnit();
    }
  }
  return 0;
}

int oCard::getFinishPunchCode() const {
  auto it = punches.rbegin();
  if (it != punches.rend()) {
    if (it->getTypeCode() == oPunch::SpecialPunch::PunchFinish)
      return it->getPunchUnit();
  }
  return 0;
}
