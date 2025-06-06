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

#pragma once
#include <vector>
#include <map>

class StringCache {
private:
  vector<std::string> cache;
  size_t ix;

  vector<std::wstring> wcache;
  size_t wix;
public:
  static StringCache &getInstance();

  void init() {cache.resize(256); wcache.resize(256);}
  void clear() {cache.clear(); wcache.clear();}

  std::string &get() {
    if ( (++ix) >= cache.size() )
      ix = 0;
    size_t lx = ix;
    return cache[lx];
  }

  std::wstring &wget() {
    if ( (++wix) >= wcache.size() )
      wix = 0;
    size_t lx = wix;
    return wcache[lx];
  }
};

string convertSystemTimeN(const SYSTEMTIME &st);
string getLocalTimeN();

bool checkValidDate(const wstring &date);

wstring convertSystemTime(const SYSTEMTIME &st);
wstring convertSystemTimeOnly(const SYSTEMTIME &st);
wstring convertSystemDate(const SYSTEMTIME &st);
wstring getLocalTime();
wstring getLocalDate();
wstring getLocalTimeOnly();
// Returns time in seconds after midnight
int getLocalAbsTime();

// Get a day number after a fixed day some time ago...
int getRelativeDay();

/// Get time and date in a format that forms a part of a filename
wstring getLocalTimeFileName();

enum class SubSecond {
  Off,
  On,
  Auto
};

int parseRelativeTime(const char *data);
int parseRelativeTime(const wchar_t *data);

const wstring &codeRelativeTimeW(int rt);
const string &codeRelativeTime(int rt);

// Format time MM:SS.t (force2digit=true) or M:SS.t (force2digit=false)
const wstring &formatTimeMS(int m, bool force2digit, SubSecond mode = SubSecond::Auto);
const wstring &formatTime(int rt, SubSecond mode = SubSecond::Auto);
const wstring &formatTimeHMS(int rt, SubSecond mode = SubSecond::Auto);

wstring formatTimeIOF(int rt, int zeroTime);

int convertDateYMD(const string &m, bool checkValid);
int convertDateYMD(const string &m, SYSTEMTIME &st, bool checkValid);

int convertDateYMD(const wstring &m, bool checkValid);
int convertDateYMD(const wstring &m, SYSTEMTIME &st, bool checkValid);

// Convert a "general" time string to a MeOS compatible time string
void processGeneralTime(const wstring &generalTime, wstring &meosTime, wstring &meosDate);

// Format number date 20160421 -> 2016-04-21 (if iso) or according to a custom format otherwise
//string formatDate(int m, bool useIsoFormat);
wstring formatDate(int m, bool useIsoFormat);

__int64 SystemTimeToInt64TenthSecond(const SYSTEMTIME &st);
SYSTEMTIME Int64TenthSecondToSystemTime(__int64 time);

#define NOTIME 0x7FFFFFFF

//Returns a time converted from +/-MM:SS or NOTIME, in MeOS time unit
int convertAbsoluteTimeMS(const wstring &m);
int convertAbsoluteTimeMS(const string& m);

// Parses a time on format HH:MM:SS+01:00Z or HHMMSS+0100Z (but ignores time zone)
int convertAbsoluteTimeISO(const wstring &m);

/** Returns a time converted from HH:MM:SS or -1, in MeOS time unit 
   @param m time to convert
   @param daysZeroTime -1 do not support days syntax, positive interpret days w.r.t the specified zero time.
*/
int convertAbsoluteTimeHMS(const string &m, int daysZeroTime);

/** Returns a time converted from HH:MM:SS or -1, in MeOS time unit
   @param m time to convert
   @param daysZeroTime -1 do not support days syntax, positive interpret days w.r.t the specified zero time.
*/
int convertAbsoluteTimeHMS(const wstring &m, int daysZeroTime);

// Add or subtract a number of days from a date in Y-M-D format
wstring addOrSubtractDays(const wstring& m, int days);

const vector<string> &split(const string &line, const string &separators, vector<string> &split_vector);

const vector<wstring> &split(const wstring &line, const wstring &separators, vector<wstring> &split_vector);
const wstring &unsplit(const vector<wstring> &split_vector, const wstring &separators, wstring &line);

// Compare two strings, ignore case. 0 = equal, != zero compares as the integers.
int compareStringIgnoreCase(const wstring &a, const wstring &b);
const wstring &limitText(const wstring& tIn, size_t numChar);
wstring ensureEndingColon(const wstring &text);

const wstring &makeDash(const wstring &t);
const wstring &makeDash(const wchar_t *t);

wstring formatRank(int rank);
const string &itos(int i);
string itos(unsigned int i);
string itos(unsigned long i);
string itos(int64_t i);
string itos(uint64_t i);

const wstring &itow(int i);
wstring itow(unsigned int i);
wstring itow(unsigned long i);
wstring itow(int64_t i);
wstring itow(uint64_t i);


///Lower case match (filt_lc must be lc and stripped of accents)
bool filterMatchString(const wstring &c, const wchar_t *filt_lc, int &score);

/** To lower case and strip accants */
void prepareMatchString(wchar_t* data_c, int size);


bool matchNumber(int number, const wchar_t *key);

int getMeosBuild();
wstring getMeosDate();
wstring getMeosFullVersion();
wstring getMajorVersion();
wstring getMeosCompectVersion();

void getSupporters(vector<wstring> &supp, vector<wstring> &developSupp);

int countWords(const wchar_t *p);

wstring trim(const wstring &s);
string trim(const string &s);

bool fileExists(const wstring &file);

bool stringMatch(const wstring &a, const wstring &b);

const char *decodeXML(const char *in);
const string &decodeXML(const string &in);
const string &encodeXML(const string &in);
const wstring &encodeXML(const wstring &in);
const wstring &encodeHTML(const wstring &in);

/** Extend a year from 03 -> 2003, 97 -> 1997 etc */
int extendYear(int year);

/** Get current year, e.g., 2010 */
int getThisYear();

/** Translate a char to lower/stripped of accents etc.*/
int toLowerStripped(wchar_t c);

/** Canonize a person/club name */
//const char *canonizeName(const char *name);
const wchar_t *canonizeName(const wchar_t *name);

/** String distance between 0 and 1. 0 is equal*/
double stringDistance(const wchar_t *a, const wchar_t *b);

/** Return how close sample is to target. 1.0 means equal*/
double stringDistanceAssymetric(const wstring &target, const wstring &sample);

/** Get a number suffix, Start 1 -> 1. Zero for none*/
int getNumberSuffix(const string &str);
int getNumberSuffix(const wstring &str);

/// Extract any number from a string and return the number, prefix and suffix
int extractAnyNumber(const wstring &str, wstring &prefix, wstring &suffix);


/** Compare classnames, match H21 Elit with H21E and H21 E */
bool compareClassName(const wstring &a, const wstring &b);

/** Get WinAPI error from code */
wstring getErrorMessage(int code);

class HLS {
private:
  WORD HueToRGB(WORD n1, WORD n2, WORD hue) const;
public:

  HLS(WORD H, WORD L, WORD S) : hue(H), lightness(L), saturation(S) {}
  HLS() : hue(0), lightness(0), saturation(1) {}
  short hue;
  short lightness;
  short saturation;
  void lighten(double f);
  void saturate(double s);
  void colorDegree(double d);
  HLS &RGBtoHLS(DWORD lRGBColor);
  DWORD HLStoRGB() const;
};

void unzip(const wchar_t *zipfilename, const char *password, vector<wstring> &extractedFiles);
int zip(const wchar_t *zipfilename, const char *password, const vector<wstring> &files);

bool isAscii(const wstring &s);
bool isNumber(const wstring &s);

bool isAscii(const string &s);
bool isNumber(const string &s);
int convertDynamicBase(const wstring &s, long long &out);
void convertDynamicBase(long long val, int base, wchar_t out[16]);

/// Find all files in dir matching given file pattern
bool expandDirectory(const wchar_t *dir, const wchar_t *pattern, vector<wstring> &res);

enum PersonSex {sFemale = 1, sMale, sBoth, sUnknown};

PersonSex interpretSex(const wstring &sex);

wstring encodeSex(PersonSex sex);

wstring makeValidFileName(const wstring &input, bool strict);
string makeValidFileName(const string& input, bool strict);

/** Initial capital letter. */
void capitalize(wstring &str);

/** Initial capital letter for each word. */
void capitalizeWords(wstring &str);

wstring getTimeZoneString(const wstring &date);

/** Return bias in seconds. UTC = local time + bias. */
int getTimeZoneInfo(const wstring &date);

/** Compare bib numbers (which may contain non-digits, e.g. A-203, or 301a, 301b)*/
bool compareBib(const wstring &b1, const wstring &b2);

/** Split a name into Given, Family, and return Given.*/
wstring getGivenName(const wstring &name);

/** Split a name into Given, Family, and return Family.*/
wstring getFamilyName(const wstring &name);

/** Simple file locking class to prevent opening in different MeOS session. */
class MeOSFileLock {
  HANDLE lockedFile;
  // Not supported
  MeOSFileLock(const MeOSFileLock &);
  const MeOSFileLock &operator=(const MeOSFileLock &);

public:
  MeOSFileLock() {lockedFile = INVALID_HANDLE_VALUE;}
  ~MeOSFileLock() {unlockFile();}

  void unlockFile();
  void lockFile(const wstring &file);
};

namespace MeOSUtil {
  extern int useHourFormat;
}

void string2Wide(const string &in, wstring &out);
void wide2String(const wstring &in, string &out);

void checkWriteAccess(const wstring &file);

void moveFile(const wstring& src, const wstring& dst);
