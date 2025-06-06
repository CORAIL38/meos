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

#include "stdafx.h"
#include <set>
#include "oDataContainer.h"
#include "oEvent.h"

#include "gdioutput.h"
#include "xmlparser.h"
#include "Table.h"
#include "meos_util.h"
#include "Localizer.h"
#include "meosException.h"

oDataContainer::oDataContainer(int maxsize) {
  dataPointer = 0;
  dataMaxSize = maxsize;
  stringIndexPointer = 0;
  stringArrayIndexPointer = 2;
}

oDataContainer::~oDataContainer(void) {
}

CellType oDataDefiner::getCellType(int index) const {
  return CellType::cellEdit; 
}

oDataInfo::oDataInfo() {
  memset(Name, 0, sizeof(Name));
  Index = 0;
  Size = 0;
  Type = 0;
  SubType = 0;
  decimalSize = 0;
  decimalScale = 1;
  zeroSortPadding = 0;
  memset(Description, 0, sizeof(Description));
}

oDataInfo::~oDataInfo() {
}

oDataInfo &oDataContainer::addVariableInt(const char *name,
                                          oIntSize isize, const char *description,
                                          const shared_ptr<oDataDefiner> &dataDef) {
  oDataInfo odi;
  odi.dataDefiner = dataDef;
  odi.Index=dataPointer;
  strcpy_s(odi.Name, name);
  strcpy_s(odi.Description, description);

  if (isize == oIS64)
    odi.Size=sizeof(__int64);
  else
    odi.Size=sizeof(int);

  odi.Type=oDTInt;
  odi.SubType=isize;

  if (dataPointer+odi.Size<=dataMaxSize){
    dataPointer+=odi.Size;
    return addVariable(odi);
  }
  else
    throw std::exception("oDataContainer: Out of bounds.");
}

oDataInfo &oDataContainer::addVariableDecimal(const char *name, const char *descr, int fixedDeci) {
  oDataInfo &odi = addVariableInt(name, oISDecimal, descr);
  odi.decimalSize = fixedDeci;
  int &s = odi.decimalScale;
  s = 1;
  for (int k = 0; k < fixedDeci; k++)
    s*=10;

  return odi;
}

oDataInfo &oDataContainer::addVariableString(const char *name, const char *descr, const shared_ptr<oDataDefiner> &dataDef) {
  return addVariableString(name, -1, descr, dataDef);
}

oDataInfo &oDataContainer::addVariableString(const char *name, int maxChar,
                                       const char *descr, const shared_ptr<oDataDefiner> &dataDef)
{
  oDataInfo odi;
  odi.dataDefiner = dataDef;
  strcpy_s(odi.Name,name);
  strcpy_s(odi.Description,descr);
  if (maxChar > 0) {
    odi.Index = dataPointer;
    odi.Size = sizeof(wchar_t)*(maxChar+1);
    odi.Type = oDTString;
    odi.SubType = oSSString;

    if (dataPointer+odi.Size<=dataMaxSize){
      dataPointer+=odi.Size;
      return addVariable(odi);
    }
    else
      throw std::exception("oDataContainer: Out of bounds.");
  }
  else {
    odi.Index = stringIndexPointer++;
    odi.Size = 0;
    odi.Type = oDTStringDynamic;
    odi.SubType = oSSString;
    return addVariable(odi);
  }
}

oDataInfo &oDataContainer::addVariableEnum(const char *name, int maxChar, const char *descr,
                                           const vector< pair<wstring, wstring> > enumValues) {
  oDataInfo &odi = addVariableString(name, maxChar, descr);
  odi.SubType = oSSEnum;
  for (size_t k = 0; k<enumValues.size(); k++)
    odi.enumDescription.push_back(enumValues[k]);
  return odi;
}

oDataInfo &oDataContainer::addVariable(oDataInfo &odi) {
  if (findVariable(odi.Name))
    throw std::exception("oDataContainer: Variable already exist.");

  //index[odi.Name]=odi;
  index.insert(hash(odi.Name), ordered.size());
  ordered.push_back(odi);
  return ordered.back();
}

int oDataContainer::hash(const char *name) {
  int res = 0;
  while(*name != 0) {
    res = 31 * res + *name;
    name++;
  }
  return res;
}

oDataInfo *oDataContainer::findVariable(const char *name) {
/*  map<string, oDataInfo>::iterator it=index.find(Name);

  if (it == index.end())
    return 0;
  else return &(it->second);

  return 0;*/
  int res;
  if (index.lookup(hash(name), res)) {
    return &ordered[res];
  }
  return 0;

}



const oDataInfo *oDataContainer::findVariable(const char *name) const
{
  if (name == 0)
    return 0;
  int res;
  if (index.lookup(hash(name), res)) {
    return &ordered[res];
  }
  return 0;
}

void oDataContainer::initData(oBase *ob, int datasize) {
  if (datasize<dataPointer)
    throw std::exception("oDataContainer: Buffer too small.");

  void *data, *oldData;
  vector< vector<wstring> > *strptr;
  ob->getDataBuffers(data, oldData, strptr);
  memset(data, 0, dataPointer);
  memset(oldData, 0, dataPointer);

  if (stringIndexPointer > 0 || stringArrayIndexPointer>2) {
    vector<vector<wstring>> &str = *strptr;
    str.clear();
    str.resize(stringArrayIndexPointer);
    str[0].resize(stringIndexPointer);
    str[1].resize(stringIndexPointer);
  }
}

bool oDataContainer::isInt(const char *name) const {
  const oDataInfo *odi = findVariable(name);

  if (!odi)
    throw std::exception("oDataContainer: Variable not found.");

  return odi->Type == oDTInt;
}

bool oDataContainer::isString(const char *name) const {
  const oDataInfo *odi = findVariable(name);

  if (!odi)
    throw std::exception("oDataContainer: Variable not found.");

  return odi->Type == oDTString || odi->Type == oDTStringDynamic;
}

bool oDataContainer::setInt(oBase* ob, void *data, const char *Name, int V)
{
  oDataInfo *odi=findVariable(Name);

  if (!odi)
    throw std::exception("oDataContainer: Variable not found.");

  if (odi->Type!=oDTInt)
    throw std::exception("oDataContainer: Variable of wrong type.");

  if (odi->SubType == oIS64)
    throw std::exception("oDataContainer: Variable to large.");

  LPBYTE vd=LPBYTE(data)+odi->Index;
  int oldValue = *((int*)vd);
  
  if (oldValue != V) {
    *((int*)vd) = V;
    if (odi->dataNotifier)
      odi->dataNotifier->notify(ob, oldValue, V);
    return true;
  }
  else return false;//Not modified
}

bool oDataContainer::setInt64(void *data, const char *Name, __int64 V)
{
  oDataInfo *odi=findVariable(Name);

  if (!odi)
    throw std::exception("oDataContainer: Variable not found.");

  if (odi->Type!=oDTInt)
    throw std::exception("oDataContainer: Variable of wrong type.");

  if (odi->SubType != oIS64)
    throw std::exception("oDataContainer: Variable to large.");

  LPBYTE vd=LPBYTE(data)+odi->Index;
  if (*((__int64 *)vd)!=V){
    *((__int64 *)vd)=V;
    return true;
  }
  else return false;//Not modified
}

int oDataContainer::getInt(const void *data, const char *Name) const
{
  const oDataInfo *odi=findVariable(Name);

  if (!odi)
    throw std::exception("oDataContainer: Variable not found.");

  if (odi->Type!=oDTInt)
    throw std::exception("oDataContainer: Variable of wrong type.");

  if (odi->SubType == oIS64)
    throw std::exception("oDataContainer: Variable to large.");

  LPBYTE vd=LPBYTE(data)+odi->Index;
  return *((int *)vd);
}

__int64 oDataContainer::getInt64(const void *data, const char *Name) const
{
  const oDataInfo *odi=findVariable(Name);

  if (!odi)
    throw std::exception("oDataContainer: Variable not found.");

  if (odi->Type!=oDTInt)
    throw std::exception("oDataContainer: Variable of wrong type.");

  LPBYTE vd=LPBYTE(data)+odi->Index;

  if (odi->SubType == oIS64)
    return *((__int64 *)vd);
  else {
    int tmp = *((int *)vd);
    return tmp;
  }
}

bool oDataContainer::setString(oBase *ob, const char *name, const wstring &v) {
  oDataInfo *odi=findVariable(name);

  if (!odi)
    throw std::exception("oDataContainer: Variable not found.");

  void *data, *oldData;
  vector< vector<wstring> > *strptr;
  ob->getDataBuffers(data, oldData, strptr);

  if (odi->Type == oDTString) {
    LPBYTE vd=LPBYTE(data)+odi->Index;

    if (wcscmp((wchar_t *)vd, v.c_str())!=0){
      wcsncpy_s((wchar_t *)vd, odi->Size/sizeof(wchar_t), v.c_str(), (odi->Size-1)/sizeof(wchar_t));
      if (odi->dataNotifier)
        odi->dataNotifier->notify(ob, v);
      return true;
    }
    else return false;//Not modified
  }
  else if (odi->Type == oDTStringDynamic) {
    wstring &str = (*strptr)[0][odi->Index];
    if (str == v)
      return false; // Same string

    str = v;
    if (odi->dataNotifier)
      odi->dataNotifier->notify(ob, v);
    return true;
  }
  else
    throw std::exception("oDataContainer: Variable of wrong type.");

}

const wstring &oDataContainer::formatString(const oBase *ob, const char *Name) const {
  const oDataInfo *odi = findVariable(Name);
  if (odi->dataDefiner) {
    return odi->dataDefiner->formatData(ob, 0);
  }
  else if (odi->Type == oDTString) {
    return getString(ob, Name);
  }
  else if (odi->Type == oDTInt) {
    return itow(getInt(ob, Name));
  }
  throw std::exception("oDataContainer: Formatting failed.");
}

const wstring &oDataContainer::getString(const oBase *ob, const char *Name) const {
  const oDataInfo *odi=findVariable(Name);

  void *data, *oldData;
  vector< vector<wstring> > *strptr;
  ob->getDataBuffers(data, oldData, strptr);

  if (!odi)
    throw std::exception("oDataContainer: Variable not found.");

  if (odi->Type == oDTString) {
    LPBYTE vd=LPBYTE(data)+odi->Index;
    wstring &res = StringCache::getInstance().wget();
    res = (wchar_t *) vd;
    return res;
  }
  else if (odi->Type == oDTStringDynamic) {
    wstring &str = (*strptr)[0][odi->Index];
    return str;
  }
  else
    throw std::exception("oDataContainer: Variable of wrong type.");
}


bool oDataContainer::setDate(void *data, const char *Name, const wstring &V)
{
  oDataInfo *odi=findVariable(Name);

  if (!odi)
    throw std::exception("oDataContainer: Variable not found.");

  if (odi->Type!=oDTInt)
    throw std::exception("oDataContainer: Variable of wrong type.");

  int C = convertDateYMD(V, true);
  if (C <= 0) {
    C = _wtoi(V.c_str());
    if (V.length() >= 2 && (C > 0 && C < 100) || (C == 0 && V[0] == '0' && V[1] == '0'))
      C = extendYear(C);

    if (C < 1900 || C > 9999)
      C = 0;
  }

  LPBYTE vd=LPBYTE(data)+odi->Index;
  if (*((int *)vd)!=C){
    *((int *)vd)=C;
    return true;
  }
  else return false;//Not modified
}

const wstring& oDataContainer::getDate(const void* data, const char* name) const {
  const oDataInfo* odi = findVariable(name);

  if (!odi)
    throw std::exception("oDataContainer: Variable not found.");

  if (odi->Type != oDTInt)
    throw std::exception("oDataContainer: Variable of wrong type.");

  LPBYTE vd = LPBYTE(data) + odi->Index;
  int C = *((int*)vd);

  wchar_t bf[24];
  if (odi->SubType == oISDateOrYear) {
    if (C > 9999 && C % 10000 != 0)
      swprintf_s(bf, L"%04d-%02d-%02d", C / 10000, (C / 100) % 100, C % 100);
    else if (C > 9999)
      swprintf_s(bf, L"%04d", C / 10000);
    else if (C > 1900)
      swprintf_s(bf, L"%04d", C );
    else
      swprintf_s(bf, L"");
  }
  else {
    if (C % 10000 != 0 || C == 0)
      swprintf_s(bf, L"%04d-%02d-%02d", C / 10000, (C / 100) % 100, C % 100);
    else
      swprintf_s(bf, L"%04d", C / 10000);
  }
  wstring& res = StringCache::getInstance().wget();
  res = bf;
  return res;
}

int oDataContainer::getYear(const void* data, const char *name) const {
  const oDataInfo* odi = findVariable(name);

  if (!odi)
    throw std::exception("oDataContainer: Variable not found.");

  if (odi->Type != oDTInt)
    throw std::exception("oDataContainer: Variable of wrong type.");

  LPBYTE vd = LPBYTE(data) + odi->Index;
  int C = *((int*)vd);

  if (odi->SubType == oISDateOrYear) {
    if (C > 9999)
      return C / 10000;
    else if (C > 1900)
      return C;
    else
      return 0;
  }
  else {
    return C / 10000;
  }
}

bool oDataContainer::write(const oBase *ob, xmlparser &xml) const {
  void *data, *oldData;
  vector< vector<wstring> > *strptr;
  ob->getDataBuffers(data, oldData, strptr);
  xml.startTag("oData");

  for (size_t kk = 0; kk < ordered.size(); kk++) {
    const oDataInfo &di=ordered[kk];
    if (di.Type==oDTInt){
      LPBYTE vd=LPBYTE(data)+di.Index;
      if (di.SubType == oISTime || di.SubType == oISTimeAdjust) {
        int nr;
        memcpy(&nr, vd, sizeof(int));
        xml.writeTime(di.Name, nr);
      }
      else if (di.SubType == oISDateOrYear) {
        int nr;
        memcpy(&nr, vd, sizeof(int));
        if (nr < 9999)
          xml.write(di.Name, nr);
        else {
          char date[20];
          sprintf_s(date, "%d-%02d-%02d", nr / 10000, (nr / 100) % 100, nr % 100);
          xml.write(di.Name, date);
        }
      }
      else if (di.SubType != oIS64) {
        int nr;
        memcpy(&nr, vd, sizeof(int));
        xml.write(di.Name, nr);
      }
      else {
        __int64 nr;
        memcpy(&nr, vd, sizeof(__int64));
        xml.write64(di.Name, nr);
      }
    }
    else if (di.Type == oDTString){
      LPBYTE vd=LPBYTE(data)+di.Index;
      wstring out = (wchar_t*)vd;
      xml.write(di.Name, out);
    }
    else if (di.Type == oDTStringDynamic) {
      const wstring &str = (*strptr)[0][di.Index];
      xml.write(di.Name, str);
    }
  }

  xml.endTag();

  return true;
}

void oDataContainer::set(oBase *ob, const xmlobject &xo) {
  void *data, *oldData;
  vector< vector<wstring> > *strptr;
  ob->getDataBuffers(data, oldData, strptr);

  xmlList xl;
  xo.getObjects(xl);

  xmlList::const_iterator it;

  for(it=xl.begin(); it != xl.end(); ++it){

    oDataInfo *odi=findVariable(it->getName());

    if (odi) {
      if (odi->Type == oDTInt){
        LPBYTE vd=LPBYTE(data)+odi->Index;
        if (odi->SubType == oISTime || odi->SubType == oISTimeAdjust)
          *((int *)vd) = it->getRelativeTime();
        else if (odi->SubType == oISDateOrYear) {
          int val = convertDateYMD(it->getStr(), true);
          if (val > 0)
            *((int*)vd) = val;
          else
            *((int*)vd) = it->getInt();
        }
        else if (odi->SubType != oIS64)
          *((int *)vd) = it->getInt();
        else
          *((__int64 *)vd) = it->getInt64();
      }
      else if (odi->Type == oDTString) {
        LPBYTE vd=LPBYTE(data)+odi->Index;
        const wchar_t* ptr = it->getWPtr();
        if (ptr)
          wcsncpy_s((wchar_t *)vd, odi->Size/sizeof(wchar_t), ptr, (odi->Size-1)/sizeof(wchar_t));
      }
      else if (odi->Type == oDTStringDynamic) {
        wstring &str = (*strptr)[0][odi->Index];
        str = it->getWStr();
      }
    }
  }

  allDataStored(ob);
}

vector<InputInfo *> oDataContainer::buildDataFields(gdioutput &gdi, int maxFieldSize) const
{
  vector<string> fields;
  for (size_t k = 0; k < ordered.size(); k++)
    fields.push_back(ordered[k].Name);

  return buildDataFields(gdi, fields, maxFieldSize);
}

vector<InputInfo *> oDataContainer::buildDataFields(gdioutput &gdi, const vector<string> &fields, int maxFieldSize) const
{
  vector<InputInfo *> out;
  for (size_t k=0;k<fields.size();k++) {
    //map<string, oDataInfo>::const_iterator it=index.find(fields[k]);
    const oDataInfo *odi = findVariable(fields[k].c_str());

    //if (it==index.end())
    if (odi == 0)
      throw std::exception( ("Bad key: " + fields[k]).c_str());

    const oDataInfo &di=*odi;
    string Id=di.Name+string("_odc");

    if (di.Type==oDTInt){
      if (di.SubType == oISDate || di.SubType == oISTime || di.SubType == oISTimeAdjust || di.SubType == oISDateOrYear)
        out.push_back(&gdi.addInput(Id, L"", 10, 0, gdi.widen(di.Description) + L":"));
      else
        out.push_back(&gdi.addInput(Id, L"", 6, 0, gdi.widen(di.Description) + L":"));
    }
    else if (di.Type==oDTString){
      out.push_back(&gdi.addInput(Id, L"", min(di.Size+2, maxFieldSize), 0, gdi.widen(di.Description) + L":"));
    }
    else if (di.Type==oDTStringDynamic){
      out.push_back(&gdi.addInput(Id, L"", maxFieldSize, 0, gdi.widen(di.Description) + L":"));
    }
  }
  return out;
}

int oDataContainer::getDataAmountMeasure(const void *data) const
{
  int amount = 0;
  for (size_t kk = 0; kk < ordered.size(); kk++) {
    const oDataInfo &di=ordered[kk];
    if (di.Type==oDTInt) {
      LPBYTE vd=LPBYTE(data)+di.Index;
      int nr;
      memcpy(&nr, vd, sizeof(int));
      if (nr != 0)
        amount++;
    }
    else if (di.Type==oDTString) {
      LPBYTE vd=LPBYTE(data)+di.Index;
      amount += strlen((char *)vd);
    }

  }
  return amount;
}

void oDataContainer::fillDataFields(const oBase *ob, gdioutput &gdi) const
{
  void *data, *oldData;
  vector< vector<wstring> > *strptr;
  ob->getDataBuffers(data, oldData, strptr);

  for (size_t kk = 0; kk < ordered.size(); kk++) {
    const oDataInfo &di=ordered[kk];

    string Id=di.Name+string("_odc");
    if (di.Type==oDTInt){
      LPBYTE vd=LPBYTE(data)+di.Index;

      if (di.SubType != oIS64) {
        int nr;
        memcpy(&nr, vd, sizeof(int));
        if (di.SubType == oISCurrency) {
          if (strcmp(di.Name, "CardFee") == 0 && nr == -1)
            nr = 0; //XXX CardFee hack. CardFee = 0 is coded as -1
          gdi.setText(Id.c_str(), ob->getEvent()->formatCurrency(nr));
        }
        else {
          wchar_t bf[64];
          formatNumber(nr, di, bf);
          gdi.setText(Id.c_str(), bf);
        }
      }
      else {
        __int64 nr;
        memcpy(&nr, vd, sizeof(__int64));
        wchar_t bf[16];
        oBase::converExtIdentifierString(nr, bf);
        gdi.setText(Id.c_str(), bf);
      }
    }
    else if (di.Type==oDTString){
      LPBYTE vd=LPBYTE(data)+di.Index;
      gdi.setText(Id.c_str(), (wchar_t *)vd);
    }
    else if (di.Type==oDTStringDynamic){
      const wstring &str = (*strptr)[0][di.Index];
      gdi.setText(Id.c_str(), str);
    }
  }
}

bool oDataContainer::saveDataFields(oBase *ob, gdioutput &gdi, std::set<string> &modified) {
  void *data, *oldData;
  vector<vector<wstring>> *strptr;
  ob->getDataBuffers(data, oldData, strptr);

  for (size_t kk = 0; kk < ordered.size(); kk++) {
    const oDataInfo &di=ordered[kk];

    string Id=di.Name+string("_odc");

    if (!gdi.hasWidget(Id)) {
      continue;
    }
    if (di.Type==oDTInt){
      int no = 0;
      if (di.SubType == oISCurrency) {
        no = ob->getEvent()->interpretCurrency(gdi.getText(Id.c_str()));
      }
      else if (di.SubType == oISDate) {
        no = convertDateYMD(gdi.getText(Id.c_str()), true);
        if (no <= 0)
          no = 0;
      }
      else if (di.SubType == oISDateOrYear) {
        no = convertDateYMD(gdi.getText(Id.c_str()), true);
        if (no <= 0) {
          no = gdi.getTextNo(Id.c_str());
          if (no < 1900 || no > 9999)
            no = 0;
        }
      }
      else if (di.SubType == oISTime) {
        no = convertAbsoluteTimeHMS(gdi.getText(Id.c_str()), -1);
      }
      else if (di.SubType == oISTimeAdjust) {
        no = convertAbsoluteTimeMS(gdi.getText(Id.c_str()));
      }
      else if (di.SubType == oISDecimal) {
        wstring str = gdi.getText(Id.c_str());
        for (size_t k = 0; k < str.length(); k++) {
          if (str[k] == ',') {
            str[k] = '.';
            break;
          }
        }
        double val = _wtof(str.c_str());
        no = int(di.decimalScale * val);
      }
      else {
        no = gdi.getTextNo(Id.c_str());
      }

      LPBYTE vd=LPBYTE(data)+di.Index;
      int oldNo = *((int *)vd);
      if (oldNo != no) {
        *((int *)vd)=no;
        ob->updateChanged();
        modified.insert(di.Name);
      }
    }
    else if (di.Type == oDTString) {
      LPBYTE vd=LPBYTE(data)+di.Index;
      wstring oldS = (wchar_t *)vd;
      wstring newS = gdi.getText(Id.c_str());
      if (oldS != newS) {
        wcsncpy_s((wchar_t *)vd, di.Size/sizeof(wchar_t), newS.c_str(), (di.Size-1)/sizeof(wchar_t));
        ob->updateChanged();
        modified.insert(di.Name);
      }
    }
    else if (di.Type == oDTStringDynamic) {
      wstring &oldS = (*strptr)[0][di.Index];
      wstring newS = gdi.getText(Id.c_str());
      if (oldS != newS) {
        oldS = newS;
        modified.insert(di.Name);
        ob->updateChanged();
      }
    }
  }

  return true;
}

string oDataContainer::C_INT64(const string &name)
{
  return " `"+name+"` BIGINT NOT NULL DEFAULT 0, ";
}

string oDataContainer::C_INT(const string &name)
{
  return " `"+name+"` INT NOT NULL DEFAULT 0, ";
}

string oDataContainer::C_SMALLINT(const string &name)
{
  return " `"+name+"` SMALLINT NOT NULL DEFAULT 0, ";
}

string oDataContainer::C_TINYINT(const string &name)
{
  return " `"+name+"` TINYINT NOT NULL DEFAULT 0, ";
}

string oDataContainer::C_SMALLINTU(const string &name)
{
  return " `"+name+"` SMALLINT UNSIGNED NOT NULL DEFAULT 0, ";
}

string oDataContainer::C_TINYINTU(const string &name)
{
  return " `"+name+"` TINYINT UNSIGNED NOT NULL DEFAULT 0, ";
}

string oDataContainer::C_STRING(const string &name, int len)
{
  if (len>0) {
    char bf[16];
    sprintf_s(bf, "%d", len);
    return " `"+name+"` VARCHAR("+ bf +") NOT NULL DEFAULT '', ";
  }
  else {
    return " `"+name+"` MEDIUMTEXT NOT NULL, ";
  }
}

string oDataContainer::SQL_quote(const wchar_t *in)
{
  int len = wcslen(in);
  size_t alloc = len*4+4;
  if (alloc < 500) {
    char output[512];
    int len8 = WideCharToMultiByte(CP_UTF8, 0, in, len+1, output, alloc, 0, 0);
    output[len8] = 0;
    const char *inutf = output;
    char out[1024];
    int o=0;
    while(*inutf && o<1023){
      if (*inutf=='\'')
        out[o++]='\'';
      if (*inutf=='\\')
        out[o++]='\\';
      out[o++]=*inutf;
      inutf++;
    }
    out[o]=0;
    return out;
  }
  else {
    vector<char> output;
    output.resize(alloc);
    int len8 = WideCharToMultiByte(CP_UTF8, 0, in, len+1, &output.front(), alloc, 0, 0);
    output[len8] = 0;
    const char *inutf = &output.front();
  
    vector<char> out;
    out.reserve(alloc);
    while(*inutf){
      if (*inutf=='\'')
        out.push_back('\'');
      if (*inutf=='\\')
        out.push_back('\\');
      out.push_back(*inutf);
      inutf++;
    }
    out.push_back(0);
    string outs(&out[0]);
    return outs;
  }
}

string oDataContainer::generateSQLDefinition(const std::set<string> &exclude) const {
  string sql;
  bool addSyntx = !exclude.empty();

  for (size_t k = 0; k < ordered.size(); k++) {
    if (exclude.count(ordered[k].Name) == 0) {
      if (addSyntx)
         sql += "ADD COLUMN ";

      const oDataInfo &di = ordered[k];
      string name = di.Name;
      if (di.Type==oDTInt){
        if (di.SubType==oIS32 || di.SubType==oISDate || di.SubType==oISCurrency ||
           di.SubType==oISTime  || di.SubType == oISTimeAdjust || di.SubType==oISDecimal || di.SubType == oISDateOrYear)
          sql+=C_INT(name);
        else if (di.SubType==oIS16)
          sql+=C_SMALLINT(name);
        else if (di.SubType==oIS8)
          sql+=C_TINYINT(name);
        else if (di.SubType==oIS64)
          sql+=C_INT64(name);
        else if (di.SubType==oIS16U)
          sql+=C_SMALLINTU(name);
        else if (di.SubType==oIS8U)
          sql+=C_TINYINTU(name);
      }
      else if (di.Type==oDTString){
        sql+=C_STRING(name, di.Size-1);
      }
      else if (di.Type==oDTStringDynamic || di.Type==oDTStringArray){
        sql+=C_STRING(name, -1);
      }
    }
  }

  if (addSyntx && !sql.empty())
    return sql.substr(0, sql.length() - 2); //Remove trailing comma-space
  else
    return sql;
}

bool oDataContainer::isModified(const oDataInfo &di,
                                const void *data,
                                const void *oldData,
                                vector< vector<wstring> > *strptr) const {

  if (di.Type == oDTInt) {
    LPBYTE vd=LPBYTE(data)+di.Index;
    LPBYTE vdOld=LPBYTE(oldData)+di.Index;
    if (di.SubType != oIS64) {
      return memcmp(vd, vdOld, 4) != 0;
    }
    else {
      return memcmp(vd, vdOld, 8) != 0;
    }
  }
  else if (di.Type == oDTString) {
    LPBYTE vdB = LPBYTE(data) + di.Index;
    LPBYTE vdOldB = LPBYTE(oldData) + di.Index;
    wchar_t * vd=(wchar_t *)vdB;
    wchar_t * vdOld=(wchar_t *)vdOldB;
    return wcscmp(vd, vdOld) != 0;
  }
  else if (di.Type == oDTStringDynamic){
    const wstring &newS = (*strptr)[0][di.Index];
    const wstring &oldS = (*strptr)[1][di.Index];
    return newS != oldS;
  }
  else if (di.Type == oDTStringArray){
    const vector<wstring> &newS = (*strptr)[di.Index];
    const vector<wstring> &oldS = (*strptr)[di.Index+1];
    return newS != oldS;
  }
  else
    return true;
}

void oDataContainer::allDataStored(const oBase *ob) {
  void *data, *oldData;
  vector< vector<wstring> > *strptr;
  ob->getDataBuffers(data, oldData, strptr);
  memcpy(oldData, data, ob->getDISize());
  if (stringIndexPointer > 0 || stringArrayIndexPointer > 2) {
    for (size_t k = 0; k < stringArrayIndexPointer; k+=2) {
      (*strptr)[k+1] = (*strptr)[k];
    }
  }
}

namespace {
  char *ensureCapacity(int size, vector<char> &bfData, int &alloc) {
    assert(alloc > 100);
    if (size >= alloc) {
      alloc += size;
      bfData.resize(alloc);
    }
    return &bfData[0];
  }
}

string oDataContainer::generateSQLSet(const oBase *ob, bool forceSetAll) const {
  void *data, *oldData;
  vector< vector<wstring> > *strptr;
  ob->getDataBuffers(data, oldData, strptr);

  string sql;
  int alloc = 256;
  vector<char> bfData(alloc);
  char *bf = &bfData[0];

  for (size_t kk = 0; kk < ordered.size(); kk++) {
    const oDataInfo &di=ordered[kk];

    if (!forceSetAll && !isModified(di, data, oldData, strptr)) {
      continue;
    }

    if (di.Type==oDTInt) {
      LPBYTE vd=LPBYTE(data)+di.Index;
      if (di.SubType == oIS8U) {
        sprintf_s(bf, alloc, ", `%s`=%u", di.Name, (*((int *)vd))&0xFF);
        sql+=bf;
      }
      else if (di.SubType == oIS16U) {
        sprintf_s(bf, alloc, ", `%s`=%u", di.Name, (*((int *)vd))&0xFFFF);
        sql+=bf;
      }
      else if (di.SubType == oIS8) {
        char r = (*((int *)vd))&0xFF;
        sprintf_s(bf, alloc, ", `%s`=%d", di.Name, (int)r);
        sql+=bf;
      }
      else if (di.SubType == oIS16) {
        short r = (*((int *)vd))&0xFFFF;
        sprintf_s(bf, alloc, ", `%s`=%d", di.Name, (int)r);
        sql+=bf;
      }
      else if (di.SubType != oIS64) {
        sprintf_s(bf, alloc, ", `%s`=%d", di.Name, *((int *)vd));
        sql+=bf;
      }
      else {
        char tmp[32];
        _i64toa_s(*((__int64 *)vd), tmp, 32, 10);
        sprintf_s(bf, alloc, ", `%s`=%s", di.Name, tmp);
        sql+=bf;
      }
    }
    else if (di.Type==oDTString) {
      LPBYTE vd=LPBYTE(data)+di.Index;
      sprintf_s(bf, alloc, ", `%s`='%s'", di.Name, SQL_quote((wchar_t *)vd).c_str());
      sql+=bf;
    }
    else if (di.Type==oDTStringDynamic) {
      const wstring &str = (*strptr)[0][di.Index];
      bf = ensureCapacity(2 * str.length() + 30, bfData, alloc);
      sprintf_s(bf, alloc, ", `%s`='%s'", di.Name, SQL_quote(str.c_str()).c_str());
      sql+=bf;
    }
    else if (di.Type==oDTStringArray) {
      const wstring str = encodeArray((*strptr)[di.Index]);
      bf = ensureCapacity(2 * str.length() + 30, bfData, alloc);
      sprintf_s(bf, alloc, ", `%s`='%s'", di.Name, SQL_quote(str.c_str()).c_str());
      sql+=bf;
    }
  }
  return sql;
}

bool oDataContainer::merge(oBase &destination, const oBase &source, const oBase *base) const {
  bool modified = false;
  void *destdata, *oldDataDmy;
  vector< vector<wstring> > *deststrptr;
  destination.getDataBuffers(destdata, oldDataDmy, deststrptr);

  void *srcdata;
  vector< vector<wstring> > *srcstrptr;
  source.getDataBuffers(srcdata, oldDataDmy, srcstrptr);

  void *basedata = nullptr;
  vector< vector<wstring> > *basestrptr = nullptr;
  if (base)
    base->getDataBuffers(basedata, oldDataDmy, basestrptr);
 
  auto setData = [](void *d, void *s, void *b, int off, int size) {
    LPBYTE vd = LPBYTE(d) + off;
    LPBYTE vs = LPBYTE(s) + off;
    if (memcmp(vd, vs, size) != 0) {
      if (b == nullptr || memcmp(LPBYTE(b) + off, vs, size) != 0) {
        memcpy(vd, vs, size);
        return true;
      }
    }
    return false;
  };
  
  for (size_t kk = 0; kk < ordered.size(); kk++) {
    const oDataInfo &di = ordered[kk];
    if (di.Type == oDTInt) {
      if (di.SubType != oIS64) {
        if (setData(destdata, srcdata, basedata, di.Index, sizeof(int)))
          modified = true;
      }
      else {
        if (setData(destdata, srcdata, basedata, di.Index, sizeof(int64_t)))
          modified = true;
      }
    }
    else if (di.Type == oDTString) {
      if (setData(destdata, srcdata, basedata, di.Index, di.Size))
        modified = true;
    }
    else if (di.Type == oDTStringDynamic) {
      const wstring &s = (*srcstrptr)[0][di.Index];
      wstring &d = (*deststrptr)[0][di.Index];
      if (s != d) {
        if (basestrptr == nullptr || (*basestrptr)[0][di.Index] != s) {
          d = s;
          modified = true;
        }
      }
    }
    else if (di.Type == oDTStringArray) {
      const auto &s = (*srcstrptr)[di.Index];
      auto &d = (*deststrptr)[di.Index];
      if (s != d) {
        if (basestrptr == nullptr || (*basestrptr)[di.Index] != s) {
          d = s;
          modified = true;
        }
      }
    }
  }

  return modified;
}

void oDataContainer::getVariableInt(const void *data,
                                    list<oVariableInt> &var) const {
  var.clear();

  for (size_t kk = 0; kk < ordered.size(); kk++) {
    const oDataInfo &di=ordered[kk];

    if (di.Type == oDTInt) {
      LPBYTE vd=LPBYTE(data)+di.Index;

      oVariableInt vi;
      memcpy(vi.name, di.Name, sizeof(vi.name));
      if (di.SubType != oIS64)
        vi.data32 = (int *)vd;
      else
        vi.data64 = (__int64 *)vd;
      var.push_back(vi);
    }
  }
}


void oDataContainer::getVariableString(const oBase *ob,
                                       list<oVariableString> &var) const
{
  void *data, *oldData;
  vector< vector<wstring> > *strptr;
  ob->getDataBuffers(data, oldData, strptr);

  var.clear();

  for (size_t kk = 0; kk < ordered.size(); kk++) {
    const oDataInfo &di=ordered[kk];

    if (di.Type == oDTString){
      LPBYTE vd=(LPBYTE)(data)+di.Index;

      oVariableString vs((wchar_t *)vd, di.Size);
      memcpy(vs.name, di.Name, sizeof(vs.name));
      var.push_back(vs);
    }
    else if (di.Type == oDTStringDynamic) {
      oVariableString vs((*strptr)[0], di.Index);
      memcpy(vs.name, di.Name, sizeof(vs.name));
      var.push_back(vs);
    }
    else if (di.Type == oDTStringArray) {
      oVariableString vs((*strptr)[di.Index]);
      memcpy(vs.name, di.Name, sizeof(vs.name));
      var.push_back(vs);
    }
  }
}

oDataInterface oDataContainer::getInterface(void *data, int datasize, oBase *ob)
{
  if (datasize<dataPointer) throw std::exception("Out Of Bounds.");

  return oDataInterface(this, data, ob);
}

oDataConstInterface oDataContainer::getConstInterface(const void *data, int datasize,
                                                      const oBase *ob) const
{
  if (datasize<dataPointer) throw std::exception("Out Of Bounds.");

  return oDataConstInterface(this, data, ob);
}


oDataInterface::oDataInterface(oDataContainer *odc, void *data, oBase *ob)
{
  oB=ob;
  oDC=odc;
  Data=data;
}

oDataConstInterface::~oDataConstInterface(void)
{
}

oDataConstInterface::oDataConstInterface(const oDataContainer *odc, const void *data, const oBase *ob)
{
  oB=ob;
  oDC=odc;
  Data=data;
}

oDataInterface::~oDataInterface(void)
{
}


void oDataContainer::buildTableCol(Table *table)
{
  for (size_t kk = 0; kk < ordered.size(); kk++) {
    oDataInfo &di=ordered[kk];

    if (di.dataDefiner)  {
      table->addDataDefiner(di.Name, di.dataDefiner.get());
      int w = strlen(di.Description)*6;
      di.tableIndex = di.dataDefiner->addTableColumn(table, di.Description, w);
    }
    else if (di.Type == oDTInt) {
      bool right = di.SubType == oISCurrency;
      bool numeric = di.SubType != oISDate && di.SubType != oISTime && di.SubType != oISTimeAdjust && di.SubType != oISDateOrYear;
      int w;
      if (di.SubType == oISDecimal)
        w = max( (di.decimalSize+4)*10, 70);
      else
        w = 70;

      w = max(int(strlen(di.Description))*6, w);
      di.tableIndex = table->addColumn(di.Description, w, numeric, right);
    }
    else if (di.Type == oDTString) {
      int w = max(max(di.Size+1, int(strlen(di.Description)))*6, 70);

      for (size_t k = 0; k < di.enumDescription.size(); k++)
        w = max<int>(w, lang.tl(di.enumDescription[k].second).length() * 6);

      if (di.zeroSortPadding)
        di.tableIndex = TableColSpec(table->addColumnPaddedSort(di.Description, w, di.zeroSortPadding, true), 1);

      else
        di.tableIndex = table->addColumn(di.Description, w, false);

    }
    else if (di.Type == oDTStringDynamic) {
      int w = 64*6;
      di.tableIndex = table->addColumn(di.Description, w, false);
    }
  }
}

bool oDataContainer::formatNumber(int nr, const oDataInfo &di, wchar_t bf[64]) const {
  if (di.SubType == oISDate) {
    if (nr == 0)
      bf[0] = 0;
    else if ((nr < 99999999 && nr % 10000 != 0) || nr == 0)
      swprintf_s(bf, 64, L"%d-%02d-%02d", nr / (100 * 100), (nr / 100) % 100, nr % 100);
    else if (nr > 0  && nr < 9999)
      swprintf_s(bf, 64, L"%04d", nr / 10000);
    else {
      bf[0] = '-';
      bf[1] = 0;
    }
    return true;
  } 
  else if (di.SubType == oISDateOrYear) {
    if (nr == 0)
      bf[0] = 0;
    else if (nr > 9999 && nr % 10000 != 0)
      swprintf_s(bf, 64, L"%04d-%02d-%02d", nr / 10000, (nr / 100) % 100, nr % 100);
    else if (nr > 9999)
      swprintf_s(bf, 64, L"%04d", nr / 10000);
    else if (nr > 1900)
      swprintf_s(bf, 64, L"%04d", nr);
    else {
      bf[0] = '-';
      bf[1] = 0;
    }
    return true;
  }
  else if (di.SubType == oISTime) {
    if (nr>0 && nr<(30*24*timeConstHour)) {
      int cnt = 0;
      if (nr < 24*timeConstHour)
        cnt = swprintf_s(bf, 64, L"%02d:%02d:%02d", nr/timeConstHour, (nr/timeConstMinute)%60, (nr/timeConstSecond)%60);
      else {
        int days = nr / (24*timeConstHour);
        nr = nr % (24*timeConstHour);
        cnt = swprintf_s(bf, 64, L"%d+%02d:%02d:%02d", days, nr/timeConstHour, (nr/timeConstSecond)%60, (nr/timeConstSecond)%60);
      }
      if (timeConstSecond > 1) {
        if (nr % 10 != 0) {
          swprintf_s(bf + cnt, 64-cnt, L".%01d", nr % 10);
        }
      }
    }
    else {
      bf[0] = '-';
      bf[1] = 0;
    }
    return true;
  }
  else if (di.SubType == oISTimeAdjust) {
    if (nr != 0 && nr != NOTIME) {
      bool neg = false;
      int cnt = 0;
      if (nr < 0) {
        bf[0] = '-';
        cnt = 1;
        nr = -nr;
      }
      if (nr < 24 * timeConstHour)
        cnt += swprintf_s(bf + cnt, 64 - cnt, L"%02d:%02d:%02d", nr / timeConstHour, (nr / timeConstMinute) % 60, (nr / timeConstSecond) % 60);
      else {
        int days = nr / (24 * timeConstHour);
        nr = nr % (24 * timeConstHour);
        cnt += swprintf_s(bf + cnt, 64 - cnt, L"%d+%02d:%02d:%02d", days, nr / timeConstHour, (nr / timeConstSecond) % 60, (nr / timeConstSecond) % 60);
      }
      if (timeConstSecond > 1) {
        if (nr % 10 != 0) {
          swprintf_s(bf + cnt, 64 - cnt, L".%01d", nr % 10);
        }
      }
    }
    else {
      bf[0] = '-';
      bf[1] = 0;
    }
    return true;
  }
  else if (di.SubType == oISDecimal) {
    if (nr) {
      int whole = nr / di.decimalScale;
      int part = nr - whole * di.decimalScale;
      wstring deci = L",";
      wstring ptrn = L"%d" + deci + L"%0" + itow(di.decimalSize) + L"d";
      swprintf_s(bf, 64, ptrn.c_str(), whole, abs(part));
    }
    else
      bf[0] = 0;

    return true;
  }
  else {
    if (nr)
      swprintf_s(bf, 64, L"%d", nr);
    else
      bf[0] = 0;
    return true;
  }
}

int oDataContainer::fillTableCol(const oBase &owner, Table &table, bool canEdit) const {
  void *data, *oldData;
  vector< vector<wstring> > *strptr;
  owner.getDataBuffers(data, oldData, strptr);

  int nextIndex = 0;
  wchar_t bf[64];
  oBase &ob = *(oBase *)&owner;
  for (size_t kk = 0; kk < ordered.size(); kk++) {
    const oDataInfo &di=ordered[kk];
    if (di.dataDefiner != nullptr) {
      for (int i = 0; i < di.tableIndex.numColumns(); i++) {
        table.set(di.tableIndex[i], ob, 1000 + di.tableIndex[i],
                  di.dataDefiner->formatData(&ob, i), canEdit && di.dataDefiner->canEdit(i),
                  di.dataDefiner->getCellType(i));
      }
    }
    else if (di.Type==oDTInt) {
      LPBYTE vd=LPBYTE(data)+di.Index;
      if (di.SubType != oIS64) {
        int nr;
        memcpy(&nr, vd, sizeof(int));
        if (di.SubType == oISCurrency) {
          table.set(di.tableIndex[0], ob, 1000 + di.tableIndex[0], ob.getEvent()->formatCurrency(nr), canEdit);
        }
        else {
          formatNumber(nr, di, bf);
          table.set(di.tableIndex[0], ob, 1000 + di.tableIndex[0], bf, canEdit);
        }
      }
      else {
        __int64 nr;
        memcpy(&nr, vd, sizeof(__int64));
        wchar_t bf[16];
        oBase::converExtIdentifierString(nr, bf);
        table.set(di.tableIndex[0], ob, 1000 + di.tableIndex[0], bf, canEdit);
      }
    }
    else if (di.Type==oDTString) {
      LPBYTE vd=LPBYTE(data)+di.Index;
      if (di.SubType == oSSString || !canEdit) {
        table.set(di.tableIndex[0], *((oBase*)&owner), 1000 + di.tableIndex[0], (wchar_t*)vd, canEdit, cellEdit);
      }
      else {
        wstring str((wchar_t *)vd);
        for (size_t k = 0; k<di.enumDescription.size(); k++) {
          if ( str == di.enumDescription[k].first ) {
            str = lang.tl(di.enumDescription[k].second);
            break;
          }
        }
        table.set(di.tableIndex[0], *((oBase*)&owner), 1000 + di.tableIndex[0], str, true, cellSelection);
      }
    }
    else if (di.Type == oDTStringDynamic) {
      const wstring &str = (*strptr)[0][di.Index];
      table.set(di.tableIndex[0], *((oBase*)&owner), 1000 + di.tableIndex[0], str, canEdit, cellEdit);
    }
    if (di.tableIndex.numColumns() > 0)
      nextIndex = di.tableIndex.nextColumn();
  }
  return nextIndex;
}

pair<int, bool>  oDataContainer::inputData(oBase *ob, int id,
                                           const wstring &input, int inputId,
                                           wstring &output, bool noUpdate)
{
  void *data, *oldData;
  vector< vector<wstring> > *strptr;
  ob->getDataBuffers(data, oldData, strptr);

  for (size_t kk = 0; kk < ordered.size(); kk++) {
    const oDataInfo &di = ordered[kk];

    if (di.tableIndex.hasColumn(id - 1000)) {
      if (di.dataDefiner) {
        const int ix = di.tableIndex.getIndex(id - 1000);
        const wstring &src = di.dataDefiner->formatData(ob, ix);
        auto ret = di.dataDefiner->setData(ob, ix, 
                                           input, output, inputId);
        bool ch = output != src;
        if (ch && noUpdate == false)
          ob->synchronize(true);
        return ret;
      }
      else if (di.Type == oDTInt) {
        LPBYTE vd = LPBYTE(data) + di.Index;

        int no = 0;
        if (di.SubType == oISCurrency) {
          no = ob->getEvent()->interpretCurrency(input);
        }
        else if (di.SubType == oISDate) {
          no = convertDateYMD(input, true);
          if (no <= 0)
            no = 0;
        } 
        else if (di.SubType == oISDateOrYear) {
          no = convertDateYMD(input, true);
          if (no <= 0) {
            no = _wtoi(input.c_str());
            if (input.length() >= 2 && (no > 0 && no < 100) || (no == 0 && input[0] == '0' && input[1] == '0'))
              no = extendYear(no);

            if (no < 1900 || no > 9999)
              no = 0;
          }
        }
        else if (di.SubType == oISTime) {
          no = convertAbsoluteTimeHMS(input, -1);
        }
        else if (di.SubType == oISTimeAdjust) {
          no = convertAbsoluteTimeMS(input);
          if (no == NOTIME)
            no = 0;
        }
        else if (di.SubType == oISDecimal) {
          wstring str = input;
          for (size_t k = 0; k < str.length(); k++) {
            if (str[k] == ',') {
              str[k] = '.';
              break;
            }
          }
          double val = _wtof(str.c_str());
          no = int(di.decimalScale * val);
        }
        else if (di.SubType == oIS64) {
          oEvent* oe = ob->getEvent();
          __int64 k64;
          memcpy(&k64, vd, sizeof(__int64));

          __int64 no64 = oBase::converExtIdentifierString(input);

          wchar_t bf[16];
          oBase::converExtIdentifierString(no64, bf);

          if (compareStringIgnoreCase(bf, input)) {
            throw meosException(L"Cannot represent ID X#" + input);
          }

          if (no64 != k64 && !oe->hasWarnedModifiedId()) {
            if (oe->gdiBase().askOkCancel(L"warn:changeid") == gdioutput::AskAnswer::AnswerCancel)
              throw meosCancel();
            oe->hasWarnedModifiedId(true);
          }

          memcpy(vd, &no64, sizeof(__int64));
          __int64 out64 = no64;
          if (k64 != no64) {
            ob->updateChanged();
            if (noUpdate == false)
              ob->synchronize(true);

            memcpy(&out64, vd, sizeof(__int64));
          }
          //output = itos(out64);
          wchar_t outbf[16];
          oBase::converExtIdentifierString(out64, outbf);
          output = outbf;

          return make_pair(0, false);
        }
        else
          no = _wtoi(input.c_str());

        int k;
        memcpy(&k, vd, sizeof(int));
        memcpy(vd, &no, sizeof(int));
        wchar_t bf[128];
        int outN = no;

        if (k != no) {
          ob->updateChanged();
          if (noUpdate == false)
            ob->synchronize(true);

          memcpy(&outN, vd, sizeof(int));

          if (di.dataNotifier)
            di.dataNotifier->notify(ob, k, no);
        }

        formatNumber(outN, di, bf);
        output = bf;
      }
      else if (di.Type == oDTString) {
        LPBYTE vd = LPBYTE(data) + di.Index;
        const wchar_t *str = input.c_str();

        if (di.SubType == oSSEnum) {
          size_t ix = inputId - 1;
          if (ix < di.enumDescription.size()) {
            str = di.enumDescription[ix].first.c_str();
          }
        }

        if (wcscmp((wchar_t *)vd, str) != 0) {
          wcsncpy_s((wchar_t *)vd, di.Size / sizeof(wchar_t), str, (di.Size - 1) / sizeof(wchar_t));
          if (di.dataNotifier)
            di.dataNotifier->notify(ob, input);
          ob->updateChanged();
          if (noUpdate == false)
            ob->synchronize(true);

          if (di.SubType == oSSEnum) {
            size_t ix = inputId - 1;
            if (ix < di.enumDescription.size()) {
              output = lang.tl(di.enumDescription[ix].second);
              // This might be incorrect if data was changed on server,
              // but this issue is minor, I think: conflicts have no resolution
              // Anyway, the row will typically be reloaded
            }
            else
              output = (wchar_t *)vd;
          }
          else
            output = (wchar_t *)vd;
          return make_pair(0, false);
        }
        else
          output = input;

      }
      else if (di.Type == oDTStringDynamic) {
        wstring &vd = (*strptr)[0][di.Index];

        if (vd != input) {
          vd = input;
          ob->updateChanged();
          if (noUpdate == false)
            ob->synchronize(true);

          output = (*strptr)[0][di.Index];
        }
        else
          output = input;
      }
    }
  }
  return make_pair(0, false);
}

void oDataContainer::fillInput(const oBase *obj, int id, const char *name,
                               vector< pair<wstring, size_t> > &out, size_t &selected) const {

  void *data, *olddata;
  pvectorstr strData;
  obj->getDataBuffers(data, olddata, strData);

  const oDataInfo * info = findVariable(name);

  if (!info) {
    for (size_t kk = 0; kk < ordered.size(); kk++) {
      const oDataInfo &di=ordered[kk];
      if (di.tableIndex.hasColumn(id - 1000)) {
        info = &di;
        break;
      }
    }
  }

  if (info && info->Type == oDTString && info->SubType == oSSEnum) {
    wchar_t *vd = (wchar_t *)(LPBYTE(data)+info->Index);

    selected = -1;
    for (size_t k = 0; k < info->enumDescription.size(); ++k) {
      out.push_back(make_pair(lang.tl(info->enumDescription[k].second), k+1));
      if (info->enumDescription[k].first == wstring(vd))
        selected = k+1;
    }
  }
  else if (info && info->dataDefiner) {
    info->dataDefiner->fillInput(obj, info->tableIndex.getIndex(id-1000), out, selected);
  }
  else
    throw meosException("Invalid enum");
}

bool oDataContainer::setEnum(oBase *ob, const char *name, int selectedIndex) {
  const oDataInfo * info = findVariable(name);

  if (info  && info->Type == oDTString && info->SubType == oSSEnum) {
    if (size_t(selectedIndex - 1) < info->enumDescription.size()) {
      return setString(ob, name, info->enumDescription[selectedIndex-1].first);
    }
  }
  throw meosException("Invalid enum");
}

bool oVariableString::store(const wchar_t *in) {
  if (data) {
    if (wcscmp(in, data) != 0) {
      wcsncpy_s(data, maxSize/sizeof(wchar_t), in, (maxSize-1)/sizeof(wchar_t));
      return true;
    }
    return false;
  }
  else {
    vector<wstring> &str = *strData;
    if (strIndex>=0) {
      if (str[strIndex] != in) {
        str[strIndex] = in;
        return true;
      }
      else
        return false;
    }
  }

  return false;
}

wstring oDataContainer::encodeArray(const vector<wstring> &input) {
  return L"";//XXX
}

void oDataContainer::decodeArray(const string &input, vector<wstring> &output) {
}
