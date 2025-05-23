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


#include "StdAfx.h"

#include <fstream>
#include <cassert>
#include <typeinfo>

#include "MeosSQL.h"

#include "oRunner.h"
#include "oEvent.h"
#include "meos_util.h"
#include "RunnerDB.h"
#include "progress.h"
#include "metalist.h"
#include "machinecontainer.h"
#include "MeOSFeatures.h"
#include "meosexception.h"
#include "generalresult.h"
#include "mysqlwrapper.h"

extern oEvent* gEvent;

wstring fromUTF(const char *w) {
  const int buff_pre_alloc = 1024 * 8;
  static wchar_t buff[buff_pre_alloc];
  int len = strlen(w);
  len = min(len + 1, buff_pre_alloc - 10);
  int wlen = MultiByteToWideChar(CP_UTF8, 0, w, len, buff, buff_pre_alloc);
  buff[wlen - 1] = 0;
  return buff;
}

wstring fromUTF(const string w) {
  const int buff_pre_alloc = 1024*8;
  static wchar_t buff[buff_pre_alloc];
  int len = w.length();
  len = min(len+1, buff_pre_alloc-10);
  int wlen = MultiByteToWideChar(CP_UTF8, 0, w.c_str(), len, buff, buff_pre_alloc);
  buff[wlen-1] = 0;
  return buff;
}

string toString(const wstring &w) {
  string &output = StringCache::getInstance().get();
  size_t alloc = w.length()*4+4;
  output.resize(alloc);
  WideCharToMultiByte(CP_UTF8, 0, w.c_str(), w.length()+1, (char *)output.c_str(), alloc, 0, 0);
  output.resize(strlen(output.c_str()));
  return output;
}

MeosSQL::MeosSQL()
{
  monitorId=0;
  warnedOldVersion=false;
  buildVersion=getMeosBuild();
  con = make_shared<ConnectionWrapper>();
}

MeosSQL::~MeosSQL(void)
{
}

void MeosSQL::alert(const string &s)
{
  errorMessage=s;
}

string C_INT(string name)
{
  return " "+name+" INT NOT NULL DEFAULT 0, ";
}

string C_INT64(string name)
{
  return " "+name+" BIGINT NOT NULL DEFAULT 0, ";
}

string C_UINT64(string name)
{
  return " " + name + " BIGINT UNSIGNED NOT NULL DEFAULT 0, ";
}

string C_STRING(string name, int len=64)
{
  char bf[16];
  sprintf_s(bf, "%d", len);
  return " "+name+" VARCHAR("+ bf +") NOT NULL DEFAULT '', ";
}

string C_TEXT(string name)
{
  return " "+name+" TEXT NOT NULL, ";
}

string C_MTEXT(string name)
{
  return " "+name+" MEDIUMTEXT NOT NULL, ";
}


string C_UINT(string name)
{
  return " "+name+" INT UNSIGNED NOT NULL DEFAULT 0, ";
}

string C_START(string name)
{
  return "CREATE TABLE IF NOT EXISTS "+name+" (" +
      " Id INT AUTO_INCREMENT NOT NULL, PRIMARY KEY (Id), ";
}

string C_START_noid(string name)
{
  return "CREATE TABLE IF NOT EXISTS "+name+" (";
}

string engine() {
  string eng = gEvent->getPropertyString("DatabaseEngine", "MyISAM");
  if (eng.empty())
    return eng;
  return " ENGINE = " + makeValidFileName(eng, true);
}

string C_END()
{
  return " Modified TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, "
    "Counter INT UNSIGNED NOT NULL DEFAULT 0, "
    "INDEX(Counter), INDEX(Modified), Removed BOOL NOT NULL DEFAULT 0)" + engine() + 
    " CHARACTER SET utf8 COLLATE utf8_general_ci";
}

string C_END_noindex()
{
  return " Modified TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP)" + 
    engine() + " CHARACTER SET utf8 COLLATE utf8_general_ci";
}

string limitLength(const string &in, size_t max) {
  if (in.length() < max)
    return in;
  else
    return in.substr(0, max);
}

string limitLength(const wstring &in, size_t max) {
  if (in.length() < max)
    return toString(in);
  else
    return toString(in.substr(0, max));
}

bool MeosSQL::listCompetitions(oEvent *oe, bool keepConnection) {
  errorMessage.clear();
  CmpDataBase="";
  if (oe->isClient())
    throw std::exception("Runtime error.");

  oe->serverName.clear();

  if (!keepConnection) {
    try {
      con->connect("", oe->MySQLServer, oe->MySQLUser,
                  oe->MySQLPassword, oe->MySQLPort);
    }
    catch (const Exception& er) {
      alert(string(er.what()) + " MySQL Error");
      return false;
    }
  }

  if (!con->connected()) {
    errorMessage = "Internal error connecting to MySQL";
    return false;
  }

  string serverInfo = con->server_info();

  if (serverInfo < "5.0.3") {
    errorMessage = "Minst MySQL X krävs. Du använder version Y.#5.0.3#" + serverInfo;
    return false;
  }

  serverName=oe->MySQLServer;
  serverUser=oe->MySQLUser;
  serverPassword=oe->MySQLPassword;
  serverPort=oe->MySQLPort;

  //Store verified server name
  oe->serverName=serverName;

  bool ok = false;
  try {
    con->select_db("MeOSMain");
    ok = true;
  }
  catch (const Exception &) {
  }

  if (!ok) {
    try {
      con->create_db("MeOSMain");
      con->select_db("MeOSMain");
    }
    catch (const Exception &er) {
      alert(string(er.what()) + " MySQL Error");      
      return false;
    }
  }

  auto query = con->query();

  try{
   auto queryset = con->query();
    queryset << "SET NAMES UTF8";
    queryset.execute();
  }
  catch (const Exception& ){

  }

  query.reset();

  try{
    query << C_START("oEvent")
      << C_STRING("Name", 128)
      << C_STRING("Annotation", 128)
      << C_STRING("Date", 32)
      << C_UINT("ZeroTime")
      << C_STRING("NameId", 64)
      << " Version INT UNSIGNED DEFAULT 1, " << C_END();

    query.execute();

    query.reset();
    auto res = query.store("DESCRIBE oEvent");
    int nr = (int)res.num_rows();
    if (nr == 9) {
      query.execute("ALTER TABLE oEvent ADD COLUMN "
                    "Annotation VARCHAR(128) NOT NULL DEFAULT '' AFTER Name");
    }
  }
  catch (const Exception& er) {
    alert(string(er.what())+ " MySQL Error");
    // Try a repair operation
    try {
      query.execute("REPAIR TABLE oEvent EXTENDED");
    }
    catch (const Exception&) {
    }

    return false;
  }

  query.reset();

  try {
    query << "SELECT * FROM oEvent";

    auto res = query.store();

    if (res) {
      for (int i=0; i<res.num_rows(); i++) {
        auto row=res.at(i);

        if (int(row["Version"]) <= oe->dbVersion) {
          CompetitionInfo ci;
          ci.Name = fromUTF((string)row["Name"]);
          ci.Annotation = fromUTF((string)row["Annotation"]);
          ci.Id = row["Id"];
          ci.Date = fromUTF((string)row["Date"]);
          ci.FullPath = fromUTF((string)row["NameId"]);
          ci.NameId = fromUTF((string)row["NameId"]);
          ci.Server = oe->MySQLServer;
          ci.ServerPassword = oe->MySQLPassword;
          ci.ServerUser = oe->MySQLUser;
          ci.ServerPort = oe->MySQLPort;

          oe->cinfo.push_front(ci);
        }
        else {
          CompetitionInfo ci;
          ci.Name = fromUTF(string(row["Name"]));
          ci.Date = fromUTF(string(row["Date"]));
          ci.Annotation = fromUTF(string(row["Annotation"]));
          ci.Id=0;
          ci.Server="bad";
          ci.FullPath=fromUTF(string(row["NameId"]));
          oe->cinfo.push_front(ci);
        }
      }
    }
  }
  catch (const Exception& er) {
    // Try a repair operation
    try {
      query.execute("REPAIR TABLE oEvent EXTENDED");
    }
    catch (const Exception&) {
    }

    setDefaultDB();
    alert(string(er.what()) + " MySQL Error");
    return false;
  }

  return true;
}

string MeosSQL::serverVersion() const {
  string serverInfo = con->server_info();
  return "MySQL " + serverInfo;
}

bool MeosSQL::repairTables(const string &db, vector<string> &output) {
  // Update list database;
  con->select_db(db);
  output.clear();

  if (!con->connected()) {
    errorMessage = "Internal error connecting to MySQL";
    return false;
  }

  auto q = con->query();
  auto res = q.store("SHOW TABLES");
  int numtab = (int)res.num_rows();
  vector<string> tb;
  for (int k = 0; k < numtab; k++)
    tb.push_back(res.at(k).at(0).c_str());

  for (int k = 0; k < numtab; k++) {
    string sql = "REPAIR TABLE " + tb[k] + " EXTENDED";
    try {
      res = q.store(sql);
      string msg;
      auto row = res.at(0);
      for (int j = 0; j < row.size(); j++) {
        string t = row.at(j).get_string();
        if (!msg.empty())
          msg += ", ";
        msg += t;
      }
      output.push_back(msg);
    }
    catch (const Exception &ex) {
      string err1 = "FAILED: " + sql;
      output.push_back(err1);
      output.push_back(ex.what());
    }
  }
  return true;
}

bool MeosSQL::createRunnerDB(oEvent *oe, QueryWrapper &query)
{
  query.reset();

  query << C_START_noid("dbRunner")
  << C_STRING("Name", 64) << C_INT("CardNo")
  << C_INT("Club") << C_STRING("Nation", 3)
  << C_STRING("Sex", 1) << C_INT("BirthYear")
  << C_INT64("ExtId") << C_END_noindex();

  query.execute();

  query.reset();
  query << C_START_noid("dbClub")
    << " Id INT NOT NULL, "
    << C_STRING("Name", 64)
    << oe->oClubData->generateSQLDefinition() << C_END_noindex();
  query.execute();

  // Ugrade dbClub
  upgradeDB("dbClub", oe->oClubData);

  return true;
}

void MeosSQL::getColumns(const string &table, set<string> &output) {
  auto query = con->query();
  output.clear();
  auto res = query.store("DESCRIBE " + table);
  for (int k = 0; k < res.num_rows(); k++) {
    output.insert(res.at(k).at(0).c_str());
  }
}

void MeosSQL::upgradeDB(const string &db, oDataContainer const * dc) {
  set<string> eCol;
  getColumns(db, eCol);

  auto query = con->query();

  if (db == "oEvent") {
    if (!eCol.count("Annotation")) {
      query.execute("ALTER TABLE oEvent ADD COLUMN "
                    "Annotation VARCHAR(128) NOT NULL DEFAULT '' AFTER Name");
    }
    if (!eCol.count("Lists")) {
      string sql = "ALTER TABLE oEvent ADD COLUMN " + C_MTEXT("Lists");
      sql = sql.substr(0, sql.length() - 2);
      query.execute(sql);
    }
    if (!eCol.count("Machine")) {
      string sql = "ALTER TABLE oEvent ADD COLUMN " + C_MTEXT("Machine");
      sql = sql.substr(0, sql.length() - 2);
      query.execute(sql);
    }
  }
  else if (db == "oCourse") {
    if (!eCol.count("Legs")) {
      string sql = "ALTER TABLE oCourse ADD COLUMN " + C_STRING("Legs", 1024);
      sql = sql.substr(0, sql.length() - 2);
      query.execute(sql);
    }
  }
  else if (db == "oRunner" || db == "oTeam") {
    if (!eCol.count("InputTime")) {
      string sql = "ALTER TABLE " + db + " ";
      sql += "ADD COLUMN " + C_INT("InputTime");
      sql += "ADD COLUMN InputStatus INT NOT NULL DEFAULT 1, ";
      sql += "ADD COLUMN " + C_INT("InputPoints");
      sql += "ADD COLUMN " + C_INT("InputPlace");
      sql = sql.substr(0, sql.length() - 2);
      query.execute(sql);
    }
  }
  else if (db == "oCard") {
    if (!eCol.count("Voltage")) {
      string sql = "ALTER TABLE " + db + " ";
      sql += "ADD COLUMN " + C_UINT("Voltage");
      sql = sql.substr(0, sql.length() - 2);
      query.execute(sql);
    }
    if (!eCol.count("BDate")) {
      string sql = "ALTER TABLE " + db + " ";
      sql += "ADD COLUMN " + C_INT("BDate");
      sql = sql.substr(0, sql.length() - 2);
      query.execute(sql);
    }
  }
  else if (db == "oPunch") {
    if (!eCol.count("Unit")) {
      string sql = "ALTER TABLE " + db + " ";
      sql += "ADD COLUMN " + C_INT("Unit");
      sql = sql.substr(0, sql.length() - 2);
      query.execute(sql);
    }
    if (!eCol.count("Origin")) {
      string sql = "ALTER TABLE " + db + " ";
      sql += "ADD COLUMN " + C_INT("Origin");
      sql = sql.substr(0, sql.length() - 2);
      query.execute(sql);
    }
  }
  if (dc) {
    // Ugrade table
    string sqlAdd = dc->generateSQLDefinition(eCol);
    if (!sqlAdd.empty()) {
      query.execute("ALTER TABLE " + db + " " + sqlAdd);
    }
  }
}

bool MeosSQL::openDB(oEvent *oe)
{
  clearReadTimes();
  errorMessage.clear();
  if (!con->connected() && !listCompetitions(oe, false))
    return false;

  try {
    con->select_db("MeOSMain");
  }
  catch (const Exception& er) {
    setDefaultDB();
    alert(string(er.what()) + " MySQL Error. Select MeosMain");
    return 0;
  }
  monitorId = 0;
  string dbname(oe->currentNameId.begin(), oe->currentNameId.end());
  bool tookLock = false;

  try {
    auto query = con->query();
    query << "SELECT * FROM oEvent WHERE NameId=" << quote << dbname;
    auto res = query.store();

    if (res && res.num_rows() >= 1) {
      auto row = res.at(0);

      int version = row["Version"];
      if (version < oEvent::dbVersion) {
        if (version <= 88) {
          query.exec("LOCK TABLE MeOSMain.oEvent WRITE");
          tookLock = true;

          query.reset();
          query << "SELECT Version FROM oEvent WHERE NameId=" << quote << dbname;
          auto resV = query.store();
          if (res && res.num_rows() >= 1) {
            version = res.at(0)["Version"];
          }
        }

        query.reset();
        query << "UPDATE oEvent SET Version=" << oEvent::dbVersion << " WHERE Id=" << int(row["Id"]);
        query.execute();
        if (version <= 88) {
          upgradeTimeFormat(dbname);
        }

        if (version <= 94) {
          auto query = con->query();
          string v = "ALTER TABLE oRunner MODIFY COLUMN Rank INT NOT NULL DEFAULT 0";
          try {
            query.execute(v);
          }
          catch (const Exception&) {
          }
        }

        if (tookLock) {
          con->query().exec("UNLOCK TABLES");
        }
      }
      else if (version > oEvent::dbVersion) {
        alert("A newer version av MeOS is required.");
        return false;
      }
      /*
      if (version != oe->dbVersion) {
        // Wrong version. Drop and reset.
        query.reset();
        query << "DELETE FROM oEvent WHERE Id=" << row["Id"];
        query.execute();
        con->drop_db(dbname);
        return openDB(oe);
      }*/

      oe->Id = row["Id"]; //Don't synchronize more here...
    }
    else {
      query.reset();
      query << "INSERT INTO oEvent SET Name='-', Date='', NameId=" << quote << dbname
        << ", Version=" << oe->dbVersion;

      ResNSel res = query.execute();

      if (res) {
        oe->Id = static_cast<int>(res.insert_id);
      }
    }
  }
  catch (const Exception& er) {
    if (tookLock && con) {
      con->query().exec("UNLOCK TABLES");
    }

    setDefaultDB();
    alert(string(er.what()) + " MySQL Error. Select DB.");
    return 0;
  }

  bool ok = false;
  try {
    con->select_db(dbname);
    ok = true;
  }
  catch (const Exception &) {
  }

  if (!ok) {
    try {
      con->create_db(dbname);
      con->select_db(dbname);
    }
    catch (const Exception& er) {
      setDefaultDB();
      alert(string(er.what()) + " MySQL Error. Select DB.");
      return 0;
    }
  }

  CmpDataBase=dbname;

  auto query = con->query();
  try {
    //Real version of oEvent db
    query << C_START("oEvent")
    << C_STRING("Name", 128)
    << C_STRING("Annotation", 128)
    << C_STRING("Date", 32)
    << C_UINT("ZeroTime")
    << C_STRING("NameId", 64)
    << C_UINT("BuildVersion")
    << oe->getDI().generateSQLDefinition()
    << C_MTEXT("Lists")
    << C_MTEXT("Machine") << C_END();
    query.execute();

    // Upgrade oEvent
    upgradeDB("oEvent", oe->oEventData);

    query.reset();
    query << C_START("oRunner")
    << C_STRING("Name") << C_INT("CardNo")
    << C_INT("Club") << C_INT("Class") << C_INT("Course") << C_INT("StartNo")
    << C_INT("StartTime") << C_INT("FinishTime")
    << C_INT("Status") << C_INT("Card") << C_STRING("MultiR", 200)
    << C_INT("InputTime") << C_INT("InputStatus") << C_INT("InputPoints") << C_INT("InputPlace")
    << oe->oRunnerData->generateSQLDefinition() << C_END();

    query.execute();

    // Ugrade oRunner
    upgradeDB("oRunner", oe->oRunnerData);

    query.reset();
    query << C_START("oCard")
      << C_INT("CardNo")
      << C_UINT("ReadId")
      << C_UINT("Voltage")
      << C_INT("BDate")
      << C_STRING("Punches", 16*190) << C_END();

    query.execute();

    // Ugrade oRunner
    upgradeDB("oCard", nullptr);


    query.reset();
    query << C_START("oClass")
      << C_STRING("Name", 128)
      << C_INT("Course")
      << C_MTEXT("MultiCourse")
      << C_STRING("LegMethod", 1024)
      << oe->oClassData->generateSQLDefinition() << C_END();
    query.execute();

    // Ugrade oClass
    upgradeDB("oClass", oe->oClassData);

    query.reset();
    query << C_START("oClub")
      << C_STRING("Name", 128)
      << oe->oClubData->generateSQLDefinition() << C_END();
    query.execute();

    // Ugrade oClub
    upgradeDB("oClub", oe->oClubData);

    query.reset();
    query << C_START("oControl")
      << C_STRING("Name", 128)
      << C_STRING("Numbers", 128)
      << C_UINT("Status")
      << oe->oControlData->generateSQLDefinition() << C_END();
    query.execute();

    // Ugrade oRunner
    upgradeDB("oControl", oe->oControlData);

    query.reset();
    query << C_START("oCourse")
      << C_STRING("Name")
      << C_STRING("Controls", 512)
      << C_UINT("Length")
      << C_STRING("Legs", 1024)
      << oe->oCourseData->generateSQLDefinition() << C_END();
    query.execute();

    // Ugrade oCourse
    upgradeDB("oCourse", oe->oCourseData);

    query.reset();
    query << C_START("oTeam")
      << C_STRING("Name")  << C_STRING("Runners", 256)
      << C_INT("Club") << C_INT("Class")
      << C_INT("StartTime") << C_INT("FinishTime")
      << C_INT("Status") << C_INT("StartNo")
      << C_INT("InputTime") << C_INT("InputStatus") << C_INT("InputPoints") << C_INT("InputPlace")
      << oe->oTeamData->generateSQLDefinition() << C_END();
    query.execute();

    // Ugrade oTeam
    upgradeDB("oTeam", oe->oTeamData);

    query.reset();
    query << C_START("oPunch")
      << C_INT("CardNo")
      << C_INT("Time")
      << C_INT("Type")
      << C_INT("Unit") 
      << C_INT("Origin") << C_END();
    query.execute();

    upgradeDB("oPunch", nullptr);

    query.reset();
    query << C_START("oMonitor")
      << C_STRING("Client")
      << C_UINT("Count")
      << C_END();
    query.execute();

    query.reset();
    query << "CREATE TABLE IF NOT EXISTS oCounter ("
      << "CounterId INT NOT NULL, "
      << C_UINT("oControl")
      << C_UINT("oCourse")
      << C_UINT("oClass")
      << C_UINT("oCard")
      << C_UINT("oClub")
      << C_UINT("oPunch")
      << C_UINT("oRunner")
      << C_UINT("oTeam")
      << C_UINT("oEvent")
      << " Modified TIMESTAMP)" << engine();
    query.execute();

    auto res = query.store("SELECT CounterId FROM oCounter");
    if (res.num_rows()==0) {
      query.reset();
      query << "INSERT INTO oCounter SET CounterId=1, oPunch=1, oTeam=1, oRunner=1";
      query.execute();
    }

    // Create runner/club DB
    createRunnerDB(oe, query);

    query.reset();
    query << C_START_noid("oImage")
      << C_UINT64("Id")
      << C_TEXT("Filename")
      << " Image LONGBLOB)" << engine();
    query.execute();
  }
  catch (const Exception& er){
    alert(string(er.what()) + " MySQL Error.");
    return 0;
  }

  return true;
}

bool MeosSQL::getErrorMessage(string &err)
{
  err = errorMessage;
  return !errorMessage.empty();
}

bool MeosSQL::closeDB()
{
  CmpDataBase="";
  errorMessage.clear();

  try {
    con->close();
  }
  catch (const Exception&) {
  }

  return true;
}

//CAN BE RUN IN A SEPARTE THREAD. Access nothing without thinking...
//No other MySQL-call will take place in parallell.
bool MeosSQL::reConnect()
{
  errorMessage.clear();

  if (CmpDataBase.empty()) {
    errorMessage="No database selected.";
    return false;
  }

  try {
    con->close();
  }
  catch (const Exception&) {
  }

  try {
    con->connect("", serverName, serverUser,
          serverPassword, serverPort);
  }
  catch (const Exception& er) {
    errorMessage=er.what();
    return false;
  }

  try {
    con->select_db(CmpDataBase);
  }
  catch (const Exception& er) {
    errorMessage=er.what();
    return false;
  }
 
  try{
    auto queryset = con->query();
    queryset << "SET NAMES UTF8";
    queryset.execute();
  }
  catch (const Exception& ){
  }

  return true;
}

OpFailStatus MeosSQL::SyncUpdate(oEvent *oe)
{
  OpFailStatus retValue = opStatusOK;
  errorMessage.clear();
  if (CmpDataBase.empty())
    return opStatusFail;


  try {
    con->query().exec("SET sql_log_bin=off");
  }
  catch (const Exception &ex) {
    oe->gdiBase().addInfoBox("binlog", L"warn:mysqlbinlog#" + oe->gdiBase().widen(ex.what()), L"", BoxStyle::Header, 10000);
  }

  try{
    con->select_db("MeOSMain");

    auto queryset = con->query();
    queryset << "UPDATE oEvent SET Name=" << quote << limitLength(oe->Name, 128) << ", "
        << " Annotation="  << quote << limitLength(oe->Annotation, 128) << ", "
        << " Date="  << quote << toString(oe->Date) << ", "
        << " NameId="  << quote << toString(oe->currentNameId) << ", "
        << " ZeroTime=" << unsigned(oe->ZeroTime)
        << " WHERE Id=" << oe->Id;

    queryset.execute();
  }
  catch (const Exception& er){
    setDefaultDB();
    alert(string(er.what()) + " [UPDATING oEvent]");
    return opStatusFail;
  }

  try{
    con->select_db(CmpDataBase);
  }
  catch (const Exception& er){
    alert(string(er.what())+" [OPENING] "+CmpDataBase);
    return opStatusFail;
  }

  {

    string listEnc;
    try {
      encodeLists(oe, listEnc);
    }
    catch (std::exception &ex) {
      retValue = opStatusWarning;
      alert(ex.what());
    }

    auto queryset = con->query();
    queryset << " Name=" << quote << limitLength(oe->Name, 128) << ", "
        << " Annotation="  << quote << limitLength(oe->Annotation, 128) << ", "
        << " Date="  << quote << toString(oe->Date) << ", "
        << " NameId="  << quote << toString(oe->currentNameId) << ", "
        << " ZeroTime=" << unsigned(oe->ZeroTime) << ", "
        << " BuildVersion=" << buildVersion << ", "
        << " Lists=" << quote << listEnc << ", "
        << " Machine=" <<quote << oe->getMachineContainer().save()
        <<  oe->getDI().generateSQLSet(true);

    if (syncUpdate(queryset, "oEvent", oe) == opStatusFail)
      return opStatusFail;
  }
  writeTime = true;
  try {
    con->query().exec("DELETE FROM oCard");
    {
      list<oCard>::iterator it = oe->Cards.begin();
      while (it != oe->Cards.end()) {
        if (!it->isRemoved() && syncUpdate(&*it, true) == opStatusFail)
          return opStatusFail;
        ++it;
      }
    }

    con->query().exec("DELETE FROM oClub");
    {
      list<oClub>::iterator it = oe->Clubs.begin();
      while (it != oe->Clubs.end()) {
        if (!it->isRemoved() && syncUpdate(&*it, true) == opStatusFail)
          return opStatusFail;
        ++it;
      }
    }
    con->query().exec("DELETE FROM oControl");
    {
      list<oControl>::iterator it = oe->Controls.begin();
      while (it != oe->Controls.end()) {
        if (!it->isRemoved() && syncUpdate(&*it, true) == opStatusFail)
          return opStatusFail;
        ++it;
      }
    }
    con->query().exec("DELETE FROM oCourse");
    {
      list<oCourse>::iterator it = oe->Courses.begin();
      while (it != oe->Courses.end()) {
        if (!it->isRemoved() && syncUpdate(&*it, true) == opStatusFail)
          return opStatusFail;
        ++it;
      }
    }
    con->query().exec("DELETE FROM oClass");
    {
      list<oClass>::iterator it = oe->Classes.begin();
      while (it != oe->Classes.end()) {
        if (!it->isRemoved() && syncUpdate(&*it, true) == opStatusFail)
          return opStatusFail;
        ++it;
      }
    }
    con->query().exec("DELETE FROM oRunner");
    {
      list<oRunner>::iterator it = oe->Runners.begin();
      while (it != oe->Runners.end()) {
        if (!it->isRemoved() && syncUpdate(&*it, true) == opStatusFail)
          return opStatusFail;
        ++it;
      }
    }

    con->query().exec("DELETE FROM oTeam");
    {
      list<oTeam>::iterator it = oe->Teams.begin();
      while (it != oe->Teams.end()) {
        if (!it->isRemoved() && syncUpdate(&*it, true) == opStatusFail)
          return opStatusFail;
        ++it;
      }
    }

    con->query().exec("DELETE FROM oPunch");
    {
      list<oFreePunch>::iterator it = oe->punches.begin();
      while (it != oe->punches.end()) {
        if (!it->isRemoved() && syncUpdate(&*it, true) == opStatusFail)
          return opStatusFail;
        ++it;
      }
    }
  }
  catch (...) {
    writeTime = false;
    throw;
  }
  writeTime = false;
  return retValue;
}

OpFailStatus MeosSQL::uploadRunnerDB(oEvent *oe)
{
  errorMessage.clear();
  if (CmpDataBase.empty())
    return opStatusFail;
  int errorCount = 0;
  int totErrorCount = 0;
  ProgressWindow pw(oe->gdiBase().getHWNDTarget(), oe->gdiBase().getScale());
  try {
    const vector<oDBClubEntry> &cdb = oe->runnerDB->getClubDB(true);
    size_t size = cdb.size();

    const vector<RunnerDBEntry> &rdb = oe->runnerDB->getRunnerDBN();
    const vector<RunnerWDBEntry> &rwdb = oe->runnerDB->getRunnerDB();

    if (cdb.size() + rdb.size() > 2000)
      pw.init();

    size_t tz = cdb.size() + rdb.size();
    int s1 = tz > 0 ? (1000 * cdb.size())/tz : 0;
    int s2 = tz > 0 ? (1000 * rdb.size())/tz : 0;

    // Reset databases
    con->query().exec("DELETE FROM dbClub");
    con->query().exec("DELETE FROM dbRunner");

    for (size_t k = 0; k<size; k++) {
      if (cdb[k].isRemoved())
        continue;

      auto query = con->query();
      string setId = "Id=" + itos(cdb[k].Id) + ", ";
      query << "INSERT INTO dbClub SET " << setId << "Name=" << quote << toString(cdb[k].name)
        <<  cdb[k].getDCI().generateSQLSet(true);

      try {
        query.execute();
        errorCount = 0;
      }
      catch (const Exception& ex) {
        errorMessage = ex.what();
        totErrorCount++;
        if (++errorCount > 5)
          throw;
      }

      if (k%200 == 150)
        pw.setProgress((k*s1)/size);
    }

    size = rdb.size();
    for (size_t k = 0; k<size; k++) {
      if (rdb[k].isRemoved())
        continue;
      if (!rdb[k].isUTF()) {
        rwdb[k].recode(rdb[k]);
      }

      auto query = con->query();
      query << "INSERT INTO dbRunner SET " <<
        "Name=" << quote << rdb[k].name <<
        ", ExtId=" << rdb[k].getExtId() << ", Club=" << rdb[k].clubNo <<
        ", CardNo=" << rdb[k].cardNo << ", Sex=" << quote << rdb[k].getSex() <<
        ", Nation=" << quote << rdb[k].getNationality() << ", BirthYear=" << rdb[k].getBirthDateInt();

      try {
        query.execute();
        errorCount = 0;
      }
      catch (const Exception& ex) {
        totErrorCount++;
        errorMessage = ex.what();
        if (++errorCount > 5)
          throw;
      }

      if (k%200 == 150)
        pw.setProgress(s1 + (k*s2)/size);
    }

    auto cnt = con->query().store("SELECT DATE_FORMAT(NOW(),'%Y-%m-%d %H:%i:%s')");
    string dateTime = cnt.at(0).at(0).get_string();
    oe->runnerDB->setDataDate(dateTime);
  }
  catch (const Exception& er) {
    errorMessage = er.what();
    return opStatusFail;
  }
  if (errorCount > 0)
    return opStatusWarning;

  return opStatusOK;
}

bool MeosSQL::storeData(oDataInterface odi, const RowWrapper &row, unsigned long &revision) {
  //errorMessage.clear();
  list<oVariableInt> varint;
  list<oVariableString> varstring;
  bool success=true;
  bool updated = false;
  try{
    odi.getVariableInt(varint);
    list<oVariableInt>::iterator it_int;
    for(it_int=varint.begin(); it_int!=varint.end(); it_int++) {
      if (it_int->data32) {
        int val = int(row[(const char *)it_int->name]);

        if (val != *(it_int->data32)) {
          *(it_int->data32) = val;
          updated = true;
        }
      }
      else {
        __int64 val = row[(const char*)it_int->name].longlong();
        __int64 oldVal = *(it_int->data64);
        if (val != oldVal) {
          memcpy(it_int->data64, &val, 8);
          updated = true;
        }

      }
    }
  }
  catch (const BadFieldName&) {
    success=false;
  }

  try {
    odi.getVariableString(varstring);
    list<oVariableString>::iterator it_string;
    for(it_string=varstring.begin(); it_string!=varstring.end(); it_string++) {
      wstring w(fromUTF(row[(const char*)it_string->name].c_str()));
      if (it_string->store(w.c_str()))
        updated = true;
    }
  }
  catch(const BadFieldName&){
    success=false;
  }

  // Mark all data as stored in memory
  odi.allDataStored();

  if (updated)
    revision++;

  return success;
}

OpFailStatus MeosSQL::SyncRead(oEvent *oe) {
  OpFailStatus retValue = opStatusOK;
  errorMessage.clear();

  if (CmpDataBase.empty())
    return opStatusFail;

  if (!oe || !con->connected())
    return opStatusFail;

  if (oe->hasDBConnection()) {
    //We already have established connectation, and just want to sync data.
    return SyncEvent(oe);
  }
  warnedOldVersion=false;

  if (!oe->Id) return SyncUpdate(oe);

  ProgressWindow pw(oe->gdiBase().getHWNDTarget(), oe->gdiBase().getScale());

  try {
    con->select_db("MeOSMain");

    auto query = con->query();
    query << "SELECT * FROM oEvent WHERE Id=" << oe->Id;
    auto res = query.store();

    RowWrapper row;
    if (row=res.at(0)){
      oe->Name = fromUTF(string(row["Name"]));
      oe->Annotation = fromUTF(string(row["Annotation"]));
      oe->Date = fromUTF(string(row["Date"]));
      oe->ZeroTime = row["ZeroTime"];
      oe->currentNameId = fromUTF(string(row["NameId"]));
    }

    con->select_db(CmpDataBase);
  }
  catch (const Exception& er){
    setDefaultDB();
    alert(string(er.what())+" [SYNCREAD oEvent]");
    return opStatusFail;
  }

  auto query = con->query();

  int nRunner = 0;
  int nCard = 0;
  int nTeam = 0;
  int nClubDB = 0;
  int nRunnerDB = 0;

  int nSum = 1;

  try {
    auto cnt = query.store("SELECT COUNT(*) FROM dbClub");
    nClubDB = cnt.at(0).at(0);

    cnt = query.store("SELECT COUNT(*) FROM dbRunner");
    nRunnerDB = cnt.at(0).at(0);

    string time = oe->runnerDB->getDataDate();

    cnt = query.store("SELECT COUNT(*) FROM dbClub WHERE Modified>'" + time + "'");
    int modclub = cnt.at(0).at(0);
    cnt = query.store("SELECT COUNT(*) FROM dbRunner WHERE Modified>'" + time + "'");
    int modrunner = cnt.at(0).at(0);

    bool skipDB = modclub==0 && modrunner==0 && nClubDB == oe->runnerDB->getClubDB(false).size() &&
                                      nRunnerDB == oe->runnerDB->getRunnerDB().size();

    if (skipDB) {
      nClubDB = 0;
      nRunnerDB = 0;
    }

    cnt = query.store("SELECT COUNT(*) FROM oRunner");
    nRunner = cnt.at(0).at(0);

    cnt = query.store("SELECT COUNT(*) FROM oCard");
    nCard = cnt.at(0).at(0);

    cnt = query.store("SELECT COUNT(*) FROM oTeam");
    nTeam = cnt.at(0).at(0);

    nSum = nClubDB + nRunnerDB + nRunner + nTeam + nCard + 50;

    if (nSum > 400)
      pw.init();
  }
  catch (const Exception& er){
    alert(string(er.what())+" [SYNCREAD INFO]");
    return opStatusFail;
  }

  int pStart = 0, pPart = 50;

  try {
    //Update oEvent
    query << "SELECT * FROM oEvent";
    auto res = query.store();

    RowWrapper row;
    if (row=res.at(0)) {
      oe->Name = fromUTF(string(row["Name"]));
      oe->Annotation = fromUTF(string(row["Annotation"]));
      oe->Date = fromUTF(string(row["Date"]));
      oe->ZeroTime = row["ZeroTime"];
      oe->currentNameId = fromUTF(string(row["NameId"]));
      oe->sqlUpdated = row["Modified"];
      oe->counter = row["Counter"];

      if (checkOldVersion(oe, row)) {
        warnOldDB();
        retValue = opStatusWarning;
      }

      const string &lRaw = row.raw_string(res.field_num("Lists"));
      try {
        importLists(oe, lRaw.c_str());
      }
      catch (std::exception &ex) {
        alert(ex.what());
        retValue = opStatusWarning;
      }

      const string &mRaw = row.raw_string(res.field_num("Machine"));
      oe->getMachineContainer().load(mRaw);

      oDataInterface odi=oe->getDI();
      storeData(odi, row, oe->dataRevision);
      oe->changed = false;
      oe->setCurrency(-1, L"", L"", false); // Set currency tmp data
      oe->getMeOSFeatures().deserialize(oe->getDCI().getString("Features"), *oe);
    }
  }
  catch (const EndOfResults& ) {
    errorMessage = "Unexpected error, oEvent table was empty";
    return opStatusFail;
  }
  catch (const Exception& er){
    alert(string(er.what())+" [SYNCREAD oEvent/Club]");
    return  opStatusFail;
  }

  pw.setProgress(20);
  oe->sqlClubs.reset();
  try {
    auto res = query.use("SELECT * FROM oClub WHERE Removed=0");

    // Retreive result rows one by one.
    if (res){
      // Get each row in result set.
      RowWrapper row;

      while (row = res.fetch_row()) {
        oClub c(oe, row["Id"]);
        storeClub(row, c);
        oe->addClub(c);
        c.update(oe->sqlClubs);
      }
    }
  }
  catch (const EndOfResults&) {
  }
  catch (const Exception& er){
    alert(string(er.what())+" [SYNCREAD oEvent/Club]");
    return opStatusFail;
  }

  pw.setProgress(30);

  oe->sqlControls.reset();
  try {
    auto res = query.use("SELECT * FROM oControl WHERE Removed=0");

    if (res) {
      // Get each row in result set.
      RowWrapper row;

      while (row = res.fetch_row()) {
        oControl c(oe, row["Id"]);
        storeControl(row, c);
        oe->addControl(c);
        c.update(oe->sqlControls);
      }
    }
  }
  catch (const EndOfResults& ) {
  }
  catch (const Exception& er){
    alert(string(er.what())+" [SYNCREAD oEvent/Control]");
    return  opStatusFail;
  }

  oe->sqlCourses.reset();
  pw.setProgress(40);

  try{
    auto res = query.use("SELECT * FROM oCourse WHERE Removed=0");

    if (res){
      // Get each row in result set.
      RowWrapper row;
      set<int> tmp;
      while (row = res.fetch_row()) {
        oCourse c(oe, row["Id"]);
        storeCourse(row, c, tmp, false);
        oe->addCourse(c);
        c.update(oe->sqlCourses);
      }
    }
  }
  catch (const EndOfResults& ) {
  }
  catch (const Exception& er){
    alert(string(er.what())+" [SYNCREAD oEvent/Course]");
    return opStatusFail;
  }

  pw.setProgress(50);

  oe->sqlClasses.reset();
  try{
    auto res = query.use("SELECT * FROM oClass WHERE Removed=0");

    if (res) {
      RowWrapper row;
      while (row = res.fetch_row()) {
        oClass c(oe, row["Id"]);
        storeClass(row, c, false, false);
        c.changed = false;
        oe->addClass(c);
        c.update(oe->sqlClasses);
      }
    }
  }
  catch (const EndOfResults& ) {
  }
  catch (const Exception& er){
    alert(string(er.what())+" [SYNCREAD oEvent/Class]");
    return opStatusFail;
  }

  oe->sqlCards.reset();

  try{
    auto res = query.use("SELECT * FROM oCard WHERE Removed=0");
    int counter = 0;
    pStart += pPart;
    pPart = (1000 * nCard) / nSum;

    if (res){
      RowWrapper row;
      while (row = res.fetch_row()) {
        oCard c(oe, row["Id"]);
        storeCard(row, c);
        oe->addCard(c);
        assert(!c.changed);

        c.update(oe->sqlCards);
        if (++counter % 100 == 50)
          pw.setProgress(pStart + (counter * pPart) / nCard);
      }
    }

  }
  catch (const EndOfResults& ) {
  }
  catch (const Exception& er){
    alert(string(er.what())+" [SYNCREAD oEvent/Card]");
    return opStatusFail;
  }

  oe->sqlRunners.reset();
  try{
    auto res = query.use("SELECT * FROM oRunner WHERE Removed=0");
    int counter = 0;
    pStart += pPart;
    pPart = (1000 * nRunner) / nSum;

    if (res){
      RowWrapper row;
      while (row = res.fetch_row()) {
        oRunner r(oe, row["Id"]);
        storeRunner(row, r, false, false, false, false);
        assert(!r.changed);
        oe->addRunner(r, false);
        r.update(oe->sqlRunners);

        if (++counter % 100 == 50)
          pw.setProgress(pStart + (counter * pPart) / nRunner);
      }
    }
  }
  catch (const EndOfResults& ) {
  }
  catch (const Exception& er){
    alert(string(er.what())+" [SYNCREAD oEvent/Runner]");
    return opStatusFail;
  }

  oe->sqlTeams.reset();

  try{
    auto res = query.use("SELECT * FROM oTeam WHERE Removed=0");
    int counter = 0;
    pStart += pPart;
    pPart = (1000 * nTeam) / nSum;

    if (res){
      RowWrapper row;
      while (row = res.fetch_row()) {
        oTeam t(oe, row["Id"]);

        storeTeam(row, t, false, false);

        pTeam at = oe->addTeam(t, false);

        if (at) {
          at->apply(oBase::ChangeType::Quiet, nullptr);
          at->changed = false;
          for (size_t k = 0; k<at->Runners.size(); k++) {
            if (at->Runners[k]) {
              assert(!at->Runners[k]->changed);
              at->Runners[k]->changed = false;
            }
          }
        }
        t.update(oe->sqlTeams);
        
        if (++counter % 100 == 50)
          pw.setProgress(pStart + (counter * pPart) / nTeam);
      }
    }
  }
  catch (const EndOfResults& ) {
  }
  catch (const Exception& er){
    alert(string(er.what())+" [SYNCREAD oEvent/Team]");
    return opStatusFail;
  }

  string dateTime;
  try {
    if (nClubDB == 0 && nRunnerDB == 0)
      return retValue; // Not  modified

    ResultWrapper cnt;
    // Note dbRunner is stored after dbClub
    if (nRunnerDB>0)
      cnt = query.store("SELECT DATE_FORMAT(MAX(Modified),'%Y-%m-%d %H:%i:%s') FROM dbRunner");
    else
      cnt = query.store("SELECT DATE_FORMAT(NOW(),'%Y-%m-%d %H:%i:%s')");

    dateTime = cnt.at(0).at(0);

    oe->runnerDB->prepareLoadFromServer(nRunnerDB, nClubDB);

    auto res = query.use("SELECT * FROM dbClub");
    int counter = 0;
    pStart += pPart;
    pPart = (1000 * nClubDB) / nSum;

    if (res) {
      RowWrapper row;
      while (row = res.fetch_row()) {
        oClub t(oe, row["Id"]);

        string n = row["Name"];
        storeData(t.getDI(), row, oe->dataRevision);
        t.internalSetName(fromUTF(n));

        oe->runnerDB->addClub(t, false);

        if (++counter % 100 == 50)
          pw.setProgress(pStart + (counter * pPart) / nClubDB);
      }
    }
  }
  catch (const EndOfResults& ) {
  }
  catch (const Exception& er){
    alert(string(er.what())+" [SYNCREAD dbClub]");
    return opStatusFail;
  }

  try {
    auto res = query.use("SELECT * FROM dbRunner");
    int counter = 0;
    pStart += pPart;
    pPart = (1000 * nRunnerDB) / nSum;

    if (res) {
      RowWrapper row;
      while (row = res.fetch_row()) {
        string name = (string)row["Name"];
        string ext = row["ExtId"];
        string club = row["Club"];
        string card = row["CardNo"];
        string sex = row["Sex"];
        string nat = row["Nation"];
        string birth = row["BirthYear"];
        RunnerWDBEntry *db = oe->runnerDB->addRunner(name.c_str(), _atoi64(ext.c_str()),
                               atoi(club.c_str()), atoi(card.c_str()));
        if (db) {
          RunnerDBEntry &dbn = db->dbe();
          if (sex.length()==1)
            dbn.sex = sex[0];
          dbn.setBirthDate(atoi(birth.c_str()));

          if (nat.length()==3) {
            dbn.national[0] = nat[0];
            dbn.national[1] = nat[1];
            dbn.national[2] = nat[2];
          }
        }

        if (++counter % 100 == 50)
          pw.setProgress(pStart + (counter * pPart) / nRunnerDB);

      }
    }
  }
  catch (const EndOfResults& ) {
  }
  catch (const Exception& er) {
    alert(string(er.what())+" [SYNCREAD dbRunner]");
    return opStatusFail;
  }

  oe->runnerDB->setDataDate(dateTime);
  processMissingObjects();

  return retValue;
}

void MeosSQL::storeClub(const RowWrapper &row, oClub &c)
{
  string n = row["Name"];
  
  c.sqlUpdated = row["Modified"];
  c.counter = row["Counter"];
  c.Removed = row["Removed"];

  c.changedObject();

  synchronized(c);
  storeData(c.getDI(), row, c.oe->dataRevision);
  c.internalSetName(fromUTF(n));
}

void MeosSQL::storeControl(const RowWrapper &row, oControl &c)
{
  c.Name = fromUTF((string)row["Name"]);
  c.setNumbers(fromUTF((string)row["Numbers"]));
  oControl::ControlStatus oldStat = c.Status;
  c.Status = oControl::ControlStatus(int(row["Status"]));

  c.sqlUpdated = row["Modified"];
  c.counter = row["Counter"];
  c.Removed = row["Removed"];

  if (c.changed || oldStat != c.Status) {
    c.oe->dataRevision++;
    c.changed = false;
  }

  c.changedObject();

  synchronized(c);
  storeData(c.getDI(), row, c.oe->dataRevision);
}

void MeosSQL::storeCard(const RowWrapper &row, oCard &c)
{
  c.cardNo = row["CardNo"];
  c.readId = row["ReadId"];
  c.miliVolt = row["Voltage"];
  c.batteryDate = row["BDate"];
  c.importPunches(string(row["Punches"]));

  c.sqlUpdated = row["Modified"];
  c.counter = row["Counter"];
  c.Removed = row["Removed"];

  c.changedObject();
  synchronized(c);
}

void MeosSQL::storePunch(const RowWrapper &row, oFreePunch &p, bool rehash)
{
  if (rehash) {
    p.setCardNo(row["CardNo"], true);
    p.setTimeInt(row["Time"], true);
    p.setType(fromUTF(string(row["Type"])), true);
  }
  else {
    p.CardNo = row["CardNo"];
    p.punchTime = row["Time"];
    p.type = row["Type"];
  }
  p.punchUnit = row["Unit"];
  p.origin = row["Origin"];

  p.sqlUpdated = row["Modified"];
  p.counter = row["Counter"];
  p.Removed = row["Removed"];

  p.changedObject();
  synchronized(p);
}

OpFailStatus MeosSQL::storeClass(const RowWrapper &row, oClass &c,
                                 bool readCourses, bool allowSubRead)
{
  OpFailStatus success = opStatusOK;

  c.Name=fromUTF(string(row["Name"]));
  string multi = row["MultiCourse"];

  string lm(row["LegMethod"]);
  c.importLegMethod(lm);

  set<int> cid;
  vector<vector<int>> multip;
  oClass::parseCourses(multi, multip, cid);
  
  int classCourse =  row["Course"];
  if (classCourse != 0)
    cid.insert(classCourse);
  
  if (!readCourses) {
    for (set<int>::iterator clsIt = cid.begin(); clsIt != cid.end(); ++clsIt) {
      if (!c.oe->getCourse(*clsIt))
        readCourses = true; // There are missing courses. Force read.
    }
  }

  if (readCourses) {
    if (allowSubRead) {
      success = min(success, syncReadClassCourses(&c, cid, readCourses));
    }
    else {
      // Cannot read from database here. Add implicitly added courses
      for (int x : cid) {
        if (c.oe->getCourse(x) == nullptr) {
          oCourse oc(c.oe, x);
          oc.setImplicitlyCreated();
          addedFromDatabase(c.oe->addCourse(oc));
        }
      }
    }
  }

  if (classCourse != 0)
    c.Course = c.oe->getCourse(classCourse);
  else
    c.Course = nullptr;

  c.importCourses(multip);

  c.sqlUpdated = row["Modified"];
  c.counter = row["Counter"];
  c.Removed = row["Removed"];

  storeData(c.getDI(), row, c.oe->dataRevision);

  c.changed = false;
  c.changedObject();
  synchronized(c);
  return success;
}

OpFailStatus MeosSQL::storeCourse(const RowWrapper &row, oCourse &c,
                                  set<int> &readControls,
                                  bool allowSubRead) {
  OpFailStatus success = opStatusOK;

  c.Name = fromUTF((string)row["Name"]);
  c.importControls(string(row["Controls"]), false, false);
  c.Length = row["Length"];
  c.importLegLengths(string(row["Legs"]), false);

  for (int i=0;i<c.nControls; i++) {
    if (c.Controls[i]) {
      // Might have been created during last call.
      // Then just read to update
      if (!c.Controls[i]->existInDB()) {
        c.Controls[i]->setImplicitlyCreated();
        if (allowSubRead) {
          c.Controls[i]->changed = false;
          success = min(success, syncRead(true, c.Controls[i]));
        }
        addedFromDatabase(c.Controls[i]);
      }
      else {
        readControls.insert(c.Controls[i]->getId());
      }
    }
  }

  c.sqlUpdated = row["Modified"];
  c.counter = row["Counter"];
  c.Removed = row["Removed"];

  storeData(c.getDI(), row, c.oe->dataRevision);
  c.oe->dataRevision++;
  c.changed = false;
  c.changedObject();
  synchronized(c);
  return success;
}

OpFailStatus MeosSQL::storeRunner(const RowWrapper &row, oRunner &r,
                                  bool readCourseCard,
                                  bool readClassClub,
                                  bool readRunners,
                                  bool allowSubRead) {
  OpFailStatus success = opStatusOK;
  oEvent *oe=r.oe;

  // Mark old class as changed
  if (r.Class)
    r.markClassChanged(-1);

  int oldSno = r.StartNo;
  const wstring &oldBib = r.getBib();

  r.sName = fromUTF((string)row["Name"]);
  r.getRealName(r.sName, r.tRealName);
  
  if (!r.cardWasSet)
    r.setCardNo(row["CardNo"], false, true);
  r.StartNo = row["StartNo"];
  r.tStartTime = r.startTime = row["StartTime"];
  if (!r.finishTimeWasSet)
    r.FinishTime = row["FinishTime"];
  r.tStatus = r.status = RunnerStatus(int(row["Status"]));

  r.inputTime = row["InputTime"];
  r.inputPoints = row["InputPoints"];
  r.inputStatus = RunnerStatus(int(row["InputStatus"]));
  r.inputPlace = row["InputPlace"];

  r.Removed = row["Removed"];
  r.sqlUpdated = row["Modified"];
  r.counter = row["Counter"];
  int oldHeat = r.getDCI().getInt("Heat");
  storeData(r.getDI(), row, oe->dataRevision);

  if (oldSno != r.StartNo || oldBib != r.getBib())
    oe->bibStartNoToRunnerTeam.clear(); // Clear quick map (lazy setup)

  if (int(row["Course"])!=0) {
    r.Course = oe->getCourse(int(row["Course"]));
    set<int> controlIds;
    if (!r.Course) {
      oCourse oc(oe,  row["Course"]);
      oc.setImplicitlyCreated();
      if (allowSubRead)
        success = min(success, syncReadCourse(true, &oc, controlIds));
      if (!oc.isRemoved()) {
        r.Course = oe->addCourse(oc);
        addedFromDatabase(r.Course);
      }
    }
    else if (readCourseCard && allowSubRead)
      success = min(success, syncReadCourse(false, r.Course, controlIds));

    if (readCourseCard)
      success = min(success, syncReadControls(oe, controlIds));
  }
  else r.Course=0;

  pClass oldClass = r.Class;

  if (int(row["Class"])!=0) {
    r.Class=oe->getClass(int(row["Class"]));

    if (!r.Class) {
      oClass oc(oe, row["Class"]);
      oc.setImplicitlyCreated();
      if (allowSubRead)
        success = min(success, syncRead(true, &oc, readClassClub));
      if (!oc.isRemoved()) {
        r.Class = oe->addClass(oc);
        addedFromDatabase(r.Class);
      }
    }
    else if (readClassClub && allowSubRead)
      success = min(success, syncRead(false, r.Class, true));

    if (r.tInTeam && r.tInTeam->Class!=r.Class)
      r.tInTeam = 0; //Temporaraly disable belonging. Restored on next apply.
  }
  else r.Class=0;

  if (oldClass != r.Class || oldHeat != r.getDCI().getInt("Heat"))
    oe->classIdToRunnerHash.reset();

  if (int(row["Club"])!=0){
    r.Club = oe->getClub(int(row["Club"]));

    if (!r.Club) {
      oClub oc(oe, row["Club"]);
      oc.setImplicitlyCreated();
      if (allowSubRead)
        success = min(success, syncRead(true, &oc));
      if (!oc.isRemoved()) {
        r.Club = oe->addClub(oc);
        addedFromDatabase(r.Club);
      }
    }
    else if (readClassClub && allowSubRead)
      success = min(success, syncRead(false, r.Club));
  }
  else r.Club=0;

  if (!r.cardWasSet) {
    pCard oldCard = r.Card;

    if (int(row["Card"]) != 0) {
      r.Card = oe->getCard(int(row["Card"]));

      if (!r.Card) {
        oCard oc(oe, row["Card"]);
        oc.setImplicitlyCreated();
        if (allowSubRead)
          success = min(success, syncRead(true, &oc));
        if (!oc.isRemoved()) {
          r.Card = oe->addCard(oc);
          r.Card->changed = false;
        }
        else {
          addedFromDatabase(r.Card);
        }
      }
      else if (readCourseCard && allowSubRead)
        success = min(success, syncRead(false, r.Card));
    }
    else r.Card = nullptr;

    // Update card ownership
    if (oldCard && oldCard != r.Card && oldCard->tOwner == &r)
      oldCard->tOwner = nullptr;
  }

  // This is updated by addRunner if this is a temporary copy.
  if (r.Card)
    r.Card->tOwner = &r;

  // This only loads indexes
  r.decodeMultiR(string(row["MultiR"]));

  // We now load/reload required other runners.
  if (readRunners) {
    for (size_t i=0;i<r.multiRunnerId.size();i++) {
      int rid = r.multiRunnerId[i];
      if (rid>0) {
        pRunner pr = oe->getRunner(rid, 0);
        if (pr==0) {
          oRunner oR(oe, rid);
          oR.setImplicitlyCreated();
          if (allowSubRead)
            success = min(success, syncRead(true, &oR, false, readCourseCard));
          if (!oR.isRemoved()) {
            pr = oe->addRunner(oR , false);
            addedFromDatabase(pr);
          }
          else {
            r.multiRunnerId[i] = 0;
          }
        }
        else if (allowSubRead)
          success = min(success, syncRead(false, pr, false, readCourseCard));
      }
    }
  }

  // Mark new class as changed
  r.changedObject();

  synchronized(r);
  return success;
}

OpFailStatus MeosSQL::storeTeam(const RowWrapper &row, oTeam &t,
                                bool readRecursive, bool allowSubRead)
{
  oEvent *oe=t.oe;
  OpFailStatus success = opStatusOK;

  // Mark old class as changed
  if (t.Class)
    t.Class->markSQLChanged(-1,-1);

  int oldSno = t.StartNo;
  const wstring &oldBib = t.getBib();

  t.sName=fromUTF((string)row["Name"]);
  t.StartNo=row["StartNo"];
  t.tStartTime  =  t.startTime = row["StartTime"];
  t.FinishTime = row["FinishTime"];
  t.tStatus = t.status = RunnerStatus(int(row["Status"]));
  
  t.inputTime = row["InputTime"];
  t.inputPoints = row["InputPoints"];
  t.inputStatus = RunnerStatus(int(row["InputStatus"]));
  t.inputPlace = row["InputPlace"];

  storeData(t.getDI(), row, oe->dataRevision);

  if (oldSno != t.StartNo || oldBib != t.getBib())
    oe->bibStartNoToRunnerTeam.clear(); // Clear quick map (lazy setup)

  t.Removed = row["Removed"];
  if (t.Removed)
    t.prepareRemove();

  t.sqlUpdated = row["Modified"];
  t.counter = row["Counter"];

  if (!t.Removed) {
    int classId = row["Class"];
    if (classId!=0) {
      t.Class = oe->getClass(classId);

      if (!t.Class) {
        oClass oc(oe, classId);
        oc.setImplicitlyCreated();
        if (allowSubRead)
          success = min(success, syncRead(true, &oc, readRecursive));
        if (!oc.isRemoved()) {
          t.Class = oe->addClass(oc);
          addedFromDatabase(t.Class);
        }
      }
      else if (readRecursive && allowSubRead)
        success = min(success, syncRead(false, t.Class, readRecursive));
    }
    else t.Class=0;

    int clubId = row["Club"];
    if (clubId!=0) {
      t.Club=oe->getClub(clubId);

      if (!t.Club) {
        oClub oc(oe, clubId);
        oc.setImplicitlyCreated();
        if (allowSubRead)
          success = min(success, syncRead(true, &oc));
        if (!oc.isRemoved()) {
          t.Club = oe->addClub(oc);
          addedFromDatabase(t.Club);
        }
      }
      else if (readRecursive && allowSubRead)
        success = min(success, syncRead(false, t.Club));
    }
    else t.Club = 0;

    vector<int> rns;
    vector<pRunner> pRns;
    t.decodeRunners(static_cast<string>(row["Runners"]), rns);

    pRns.resize(rns.size());
    for (size_t k=0;k<rns.size(); k++) {
      if (rns[k]>0) {
        pRns[k] = oe->getRunner(rns[k], 0);
        if (!pRns[k]) {
          oRunner oR(oe, rns[k]);
          oR.setImplicitlyCreated();
          if (allowSubRead)
            success = min(success, syncRead(true, &oR, readRecursive, readRecursive));

          if (oR.sName.empty()) {
            oR.sName = L"@AutoCorrection";
            oR.getRealName(oR.sName, oR.tRealName);
          }

          if (!oR.isRemoved()) {
            pRns[k] = oe->addRunner(oR , false);
            addedFromDatabase(pRns[k]);
            assert(pRns[k] && !pRns[k]->changed);
          }
        }
        else if (readRecursive && allowSubRead)
          success = min(success, syncRead(false, pRns[k]));
      }
    }
    t.importRunners(pRns);
  }

  t.changedObject(); // Also marks new class changed
  synchronized(t);
  return success;
}

bool MeosSQL::remove(oBase *ob)
{
  errorMessage.clear();

  if (CmpDataBase.empty())
    return false;

  if (!ob || !con->connected())
    return false;

  if (!ob->Id)
    return true; //Not in DB.

  auto query = con->query();

  string oTable;

  if (typeid(*ob)==typeid(oRunner)){
    oTable="oRunner";
  }
  else if (typeid(*ob)==typeid(oClass)){
    oTable="oClass";
  }
  else if (typeid(*ob)==typeid(oCourse)){
    oTable="oCourse";
  }
  else if (typeid(*ob)==typeid(oControl)){
    oTable="oControl";
  }
  else if (typeid(*ob)==typeid(oClub)){
    oTable="oClub";
  }
  else if (typeid(*ob)==typeid(oCard)){
    oTable="oCard";
  }
  else if (typeid(*ob)==typeid(oFreePunch)){
    oTable="oPunch";
  }
  else if (typeid(*ob)==typeid(oTeam)){
    oTable="oTeam";
  }
  else if (typeid(*ob)==typeid(oEvent)){
    oTable="oEvent";
    //Must change db!
    return 0;
  }

  query << "Removed=1";
  try{
    ResNSel res = updateCounter(oTable.c_str(), ob->Id, &query);
    ob->Removed = true;
    ob->changed = false;
  }
  catch (const Exception& er){
    alert(string(er.what())+" [REMOVE " +oTable +"]");
    return false;
  }

  return true;
}

OpFailStatus MeosSQL::syncUpdate(oRunner *r, bool forceWriteAll) {
  errorMessage.clear();

  if (CmpDataBase.empty())
    return opStatusFail;

  if (!r || !con->connected())
    return opStatusFail;

  if (r->Card && !r->Card->existInDB())
    syncUpdate(r->Card, forceWriteAll);

  if (r->Class && !r->Class->existInDB())
    syncUpdate(r->Class, forceWriteAll);

  if (r->Course && !r->Course->existInDB())
    syncUpdate(r->Course, forceWriteAll);

  if (r->Club && !r->Club->existInDB())
    syncUpdate(r->Club, forceWriteAll);

  auto queryset = con->query();
  queryset << " Name=" << quote << toString(r->sName) << ", "
      << " CardNo=" << r->cardNumber << ", "
      << " StartNo=" << r->StartNo << ", "
      << " StartTime=" << r->startTime << ", "
      << " FinishTime=" << r->FinishTime << ", "
      << " Course=" << r->getCourseId() << ", "
      << " Class=" << r->getClassId(false) << ", "
      << " Club=" << r->getClubId() << ", "
      << " Card=" << r->getCardId() << ", "
      << " Status=" << r->status << ", "
      << " InputTime=" << r->inputTime << ", "
      << " InputStatus=" << r->inputStatus << ", "
      << " InputPoints=" << r->inputPoints << ", "
      << " InputPlace=" << r->inputPlace << ", "
      << " MultiR=" << quote << r->codeMultiR()
      << r->getDI().generateSQLSet(forceWriteAll);

  /*
  wstring str = L"write runner " + r->sName + L", st = " + itow(r->startTime) + L"\n";
  OutputDebugString(str.c_str());
  */
  OpFailStatus res = syncUpdate(queryset, "oRunner", r);
  if (res != OpFailStatus::opStatusFail) {
    r->cardWasSet = false; // Clear flag that this data was set here
    r->finishTimeWasSet = false;
  }
  return res;
}

bool MeosSQL::isOld(int counter, const string &time, oBase *ob)
{
  return counter != ob->counter || time != ob->sqlUpdated;
}

OpFailStatus MeosSQL::syncRead(bool forceRead, oRunner *r)
{
  return syncRead(forceRead, r, true, true);
}

string MeosSQL::andWhereOld(oBase *ob) {
  if (ob->sqlUpdated.empty()) {
    if (ob->counter != 0)
      return " AND Counter!=" + itos(ob->counter);
    else
      return "";
  }
  else
    return " AND (Counter!=" + itos(ob->counter) + " OR Modified!='" + ob->sqlUpdated + "')";
}

OpFailStatus MeosSQL::syncRead(bool forceRead, oRunner *r, bool readClassClub, bool readCourseCard)
{
  errorMessage.clear();
  if (CmpDataBase.empty())
    return opStatusFail;

  if (!r || !con->connected())
    return opStatusFail;

  if (!forceRead) {
    if (!r->existInDB())
      return syncUpdate(r, true);

    if (!r->changed && skipSynchronize(*r))
      return opStatusOKSkipped;
  }

  try {
    auto query = con->query();
    query << "SELECT * FROM oRunner WHERE Id=" << r->Id << andWhereOld(r);
    auto res = query.store();

    RowWrapper row;
    if (!res.empty()) {
      row=res.at(0);
      // Remotly changed update!
      OpFailStatus success=opStatusOK;
      if (r->changed)
        success=opStatusWarning;

      success = min (success, storeRunner(row, *r, readCourseCard, readClassClub, true, true));

      r->oe->dataRevision++;
      r->Modified.update();
      r->changed = false;

      vector<int> mp;
      r->evaluateCard(true, mp, 0, oBase::ChangeType::Quiet);

      //Forget evaluated changes. Not our buisness to update.
      if (!r->cardWasSet && !r->finishTimeWasSet) {
        r->changed = false;
        return success;
      }
      else {
        // Preserve card/finish time set on this client even in case of data collision
        r->changed = true;
      }
    }

    if (r->Card && readCourseCard)
      syncRead(false, r->Card);
    if (r->Class && readClassClub)
      syncRead(false, r->Class, readClassClub);
    if (r->Course && readCourseCard) {
      set<int> controlIds;
      syncReadCourse(false, r->Course, controlIds);
      if (readClassClub)
        syncReadControls(r->oe, controlIds);
    }
    if (r->Club && readClassClub)
      syncRead(false, r->Club);
 
    if (r->changed)
      return syncUpdate(r, false);

    vector<int> mp;
    r->evaluateCard(true, mp, 0, oBase::ChangeType::Quiet);
    r->changed = false;
  
    return  opStatusOK;
  }
  catch (const Exception& er){
    alert(string(er.what())+" [SYNCREAD oRunner]");
    return opStatusFail;
  }

  return opStatusFail;
}

OpFailStatus MeosSQL::syncUpdate(oCard *c, bool forceWriteAll)
{
  errorMessage.clear();
  if (CmpDataBase.empty())
    return opStatusFail;

  if (!c || !con->connected())
    return opStatusFail;

  auto queryset = con->query();
  queryset << " CardNo=" << c->cardNo 
      << ", ReadId=" << c->readId << ", Voltage=" << max(0, c->miliVolt)
      << ", BDate=" << c->batteryDate 
      << ", Punches=" << quote << c->getPunchString();

  return syncUpdate(queryset, "oCard", c);
}

OpFailStatus MeosSQL::syncRead(bool forceRead, oCard *c)
{
  errorMessage.clear();
  if (CmpDataBase.empty())
    return opStatusFail;

  if (!c || !con->connected())
    return opStatusFail;

  if (!forceRead) {
    if (!c->existInDB())
      return syncUpdate(c, true);

    if (!c->changed && skipSynchronize(*c))
      return opStatusOKSkipped;
  }

  try{
    auto query = con->query();
    query << "SELECT * FROM oCard WHERE Id=" << c->Id;
    auto res = query.store();

    RowWrapper row;
    if (!res.empty()){
      row=res.at(0);
      if (!c->changed || isOld(row["Counter"], string(row["Modified"]), c)){

        OpFailStatus success=opStatusOK;
        if (c->changed)
          success=opStatusWarning;

        storeCard(row, *c);
        c->oe->dataRevision++;
        c->Modified.update();
        c->changed=false;
        return success;
      }
      else if (c->changed){
        return syncUpdate(c, false);
      }
    }
    else{
      //Something is wrong!? Deleted?
      return syncUpdate(c, true);
    }

    return  opStatusOK;
  }
  catch (const Exception& er){
    alert(string(er.what())+" [SYNCREAD oCard]");
    return opStatusFail;
  }

  return opStatusFail;
}


OpFailStatus MeosSQL::syncUpdate(oTeam *t, bool forceWriteAll) {
  errorMessage.clear();

  if (CmpDataBase.empty())
    return opStatusFail;

  if (!t || !con->connected())
    return opStatusFail;

  if (t->Class && !t->Class->existInDB())
    syncUpdate(t->Class, forceWriteAll);

  if (t->Club && !t->Club->existInDB())
    syncUpdate(t->Club, forceWriteAll);

  for (int i = 0; i < t->Runners.size(); i++) {
    if (t->Runners[i] && !t->Runners[i]->existInDB()) {
      syncUpdate(t->Runners[i], forceWriteAll);
    }
  }

  auto queryset = con->query();

  queryset << " Name=" << quote << toString(t->sName) << ", "
      << " Runners=" << quote << t->getRunners() << ", "
      << " StartTime=" << t->startTime << ", "
      << " FinishTime=" << t->FinishTime << ", "
      << " Class=" << t->getClassId(false) << ", "
      << " Club=" << t->getClubId() << ", "
      << " StartNo=" << t->getStartNo() << ", "
      << " Status=" << t->status << ", "
      << " InputTime=" << t->inputTime << ", "
      << " InputStatus=" << t->inputStatus << ", "
      << " InputPoints=" << t->inputPoints << ", "
      << " InputPlace=" << t->inputPlace  
      << t->getDI().generateSQLSet(forceWriteAll);

  //wstring str = L"write team " + t->sName + L"\n";
  //OutputDebugString(str.c_str());
  return syncUpdate(queryset, "oTeam", t);
}

OpFailStatus MeosSQL::syncRead(bool forceRead, oTeam *t)
{
  return syncRead(forceRead, t, true);
}

void MeosSQL::upgradeTimeFormat(const string & dbname) {
  bool ok = false;
  try {
    con->select_db(dbname);
    ok = true;
  }
  catch (const Exception &) {
  }

  if (ok) {
    auto query = con->query();
    query.exec("LOCK TABLES oEvent WRITE, oClass WRITE, oControl WRITE, "
               "oCourse WRITE, oPunch WRITE, oRunner WRITE, oTeam WRITE");
  }
  else
    return;

  auto upgradeCol = [this](const string &db, const string &col) {
    auto query = con->query();
    string v = "UPDATE " + db + " SET " + col + "=" + col + "*10 WHERE " + col + "<>-1";
    try {
      query.execute(v);
    }
    catch (const Exception &) {
      return false;
    }
    return true;
  };

  auto alter = [this](const string &db, const string &col) {
    auto query = con->query();
    string v = "ALTER TABLE " + db + " MODIFY COLUMN " + col + " INT NOT NULL DEFAULT 0";
    try {
      query.execute(v);
    }
    catch (const Exception &) {
      return false;
    }
    return true;

  };
  upgradeCol("oEvent", "ZeroTime");
  upgradeCol("oEvent", "MaxTime");
  upgradeCol("oEvent", "DiffTime");

  upgradeCol("oClass", "FirstStart");
  upgradeCol("oClass", "StartInterval");
  upgradeCol("oClass", "MaxTime");

  upgradeCol("oControl", "TimeAdjust");
  upgradeCol("oControl", "MinTime");

  upgradeCol("oCourse", "RTimeLimit");

  upgradeCol("oPunch", "Time");

  upgradeCol("oRunner", "StartTime");
  upgradeCol("oRunner", "FinishTime");
  upgradeCol("oRunner", "InputTime");
  alter("oRunner", "TimeAdjust");
  upgradeCol("oRunner", "TimeAdjust");
  upgradeCol("oRunner", "EntryTime");

  upgradeCol("oTeam", "StartTime");
  upgradeCol("oTeam", "FinishTime");
  upgradeCol("oTeam", "InputTime");
  alter("oTeam", "TimeAdjust");
  upgradeCol("oTeam", "TimeAdjust");
  upgradeCol("oTeam", "EntryTime");
}

OpFailStatus MeosSQL::syncRead(bool forceRead, oTeam *t, bool readRecursive)
{
  errorMessage.clear();

  if (CmpDataBase.empty())
    return opStatusFail;

  if (!t || !con->connected())
    return opStatusFail;

  if (!forceRead) {
    if (!t->existInDB())
      return syncUpdate(t, true);

    if (!t->changed && skipSynchronize(*t))
      return opStatusOK;
  }

  try {
    auto query = con->query();
    query << "SELECT * FROM oTeam WHERE Id=" << t->Id << andWhereOld(t);
    auto res = query.store();

    RowWrapper row;
    if (!res.empty()) {
      row=res.at(0);

      OpFailStatus success=opStatusOK;
      if (t->changed)
        success=opStatusWarning;

      storeTeam(row, *t, readRecursive, true);
      t->oe->dataRevision++;
      t->Modified.update();
      t->changed = false;
      return success;
    }
    else {
      OpFailStatus success = opStatusOK;

      if (readRecursive) {
        if (t->Class)
          success = min(success, syncRead(false, t->Class, readRecursive));
        if (t->Club)
          success = min(success, syncRead(false, t->Club));
        for (size_t k = 0; k<t->Runners.size(); k++) {
          if (t->Runners[k])
            success = min(success, syncRead(false, t->Runners[k], false, readRecursive));
        }
      }
      if (t->changed)
        return min(success, syncUpdate(t, false));
      else
        return success;
    }

    return opStatusOK;
  }
  catch (const Exception& er){
    alert(string(er.what())+" [SYNCREAD oTeam]");
    return opStatusFail;
  }

  return opStatusFail;
}

OpFailStatus MeosSQL::syncUpdate(oClass *c, bool forceWriteAll)
{
  errorMessage.clear();

  if (CmpDataBase.empty())
    return opStatusFail;

  if (!c || !con->connected())
    return opStatusFail;
  auto queryset = con->query();
  OpFailStatus ret = opStatusOK;
  
  // Ensure all courses are written to db
  if (c->Course && !c->Course->existInDB()) {
    ret = syncUpdate(c->Course, forceWriteAll);
  }
  for (auto &mc : c->MultiCourse) {
    for (pCourse cc : mc) {
      if (cc && !cc->existInDB()) {
        ret = min(ret, syncUpdate(cc, forceWriteAll));
      }
    }
  }

  queryset << " Name=" << quote << toString(c->Name) << ","
    << " Course=" << c->getCourseId() << ","
    << " MultiCourse=" << quote << c->codeMultiCourse() << ","
    << " LegMethod=" << quote << c->codeLegMethod()
    << c->getDI().generateSQLSet(forceWriteAll);

  return syncUpdate(queryset, "oClass", c);
}

OpFailStatus MeosSQL::syncRead(bool forceRead, oClass *c)
{
  return syncRead(forceRead, c, true);
}

OpFailStatus MeosSQL::syncRead(bool forceRead, oClass *c, bool readCourses)
{
  errorMessage.clear();
  if (CmpDataBase.empty())
    return opStatusFail;

  if (!c || !con->connected())
    return opStatusFail;

  if (!forceRead && !c->existInDB())
    return syncUpdate(c, true);

  if (!c->changed && skipSynchronize(*c))
    return opStatusOK;

  try {
    auto query = con->query();
    query << "SELECT * FROM oClass WHERE Id=" << c->Id << andWhereOld(c);
    auto res = query.store();

    RowWrapper row;
    if (!res.empty()){
      row=res.at(0);
      OpFailStatus success = opStatusOK;

      if (c->changed)
        success=opStatusWarning;

      storeClass(row, *c, readCourses, true);
      c->oe->dataRevision++;
      c->Modified.update();
      c->changed = false;
      return opStatusOK;
    }
    else {
      OpFailStatus success = opStatusOK;
      if (readCourses) {
        set<int> d;
        c->getMCourseIdSet(d);
        if (c->getCourseId() != 0)
          d.insert(c->getCourseId());
        success = syncReadClassCourses(c, d, true);
      }

      if (c->changed && !forceRead)
        success = min(success, syncUpdate(c, false));

      return success;
    }

    return opStatusOK;
  }
  catch (const Exception& er){
    alert(string(er.what())+" [SYNCREAD oClass]");
    return opStatusFail;
  }

  return opStatusFail;
}

OpFailStatus MeosSQL::syncReadClassCourses(oClass *c, const set<int> &courses,
                                           bool readRecursive) {
  OpFailStatus success = opStatusOK;
  if (courses.empty())
    return success;
  oEvent *oe = c->oe;
  try {
    auto query = con->query();
    string in;
    for(set<int>::const_iterator it=courses.begin(); it!=courses.end(); ++it) {
      if (!in.empty())
        in += ",";
      in += itos(*it);
    }
    query << "SELECT Id, Counter, Modified FROM oCourse WHERE Id IN (" << in << ")";
    auto res = query.store();
    set<int> processedCourses(courses);
    set<int> controlIds;
    for (int k = 0; k < res.num_rows(); k++) {
      auto row = res.at(k);
      int id = row["Id"];
      int counter = row["Counter"];
      string modified = row["Modified"];

      pCourse pc = oe->getCourse(id);
      if (!pc) {
        oCourse oc(oe, id);
        oc.setImplicitlyCreated();
        success = min(success, syncReadCourse(true, &oc, controlIds));
        if (!oc.isRemoved())
          addedFromDatabase(oe->addCourse(oc));
      }
      else if (pc->changed || isOld(counter, modified, pc)) {
        success = min(success, syncReadCourse(false, pc, controlIds));
      }
      else {
        for (int m = 0; m <pc->nControls; m++)
          if (pc->Controls[m])
            controlIds.insert(pc->Controls[m]->getId());
      }
      processedCourses.erase(id);
    }

    // processedCourses should now be empty. The only change it is not empty is that
    // there are locally added courses that are not on the server (which is an error).
    for(set<int>::iterator it = processedCourses.begin(); it != processedCourses.end(); ++it) {
      assert(false);
      pCourse pc = oe->getCourse(*it);
      if (pc) {
        success = min(success, syncUpdate(pc, true));
      }
    }

    if (readRecursive)
      success = min(success, syncReadControls(oe, controlIds));

    return success;
  }
  catch (const Exception& er){
    alert(string(er.what())+" [SYNCREAD oClassCourse]");
    return opStatusFail;
  }
}

OpFailStatus MeosSQL::syncReadControls(oEvent *oe, const set<int> &controls) {
  OpFailStatus success = opStatusOK;
    if (controls.empty())
    return success;
   try {
     auto query = con->query();
     string in;
     for(set<int>::const_iterator it=controls.begin(); it!=controls.end(); ++it) {
       if (!in.empty())
         in += ",";
       in += itos(*it);
     }
     query << "SELECT Id, Counter, Modified FROM oControl WHERE Id IN (" << in << ")";
     auto res = query.store();
     set<int> processedControls(controls);
     for (int k = 0; k < res.num_rows(); k++) {
       RowWrapper row = res.at(k);
       int id = row["Id"];
       int counter = row["Counter"];
       string modified = row["Modified"];

       pControl pc = oe->getControl(id, false, false);
       if (!pc) {
         oControl oc(oe, id);
         success = min(success, syncRead(true, &oc));
         oe->addControl(oc);
       }
       else if (pc->changed || isOld(counter, modified, pc)) {
         success = min(success, syncRead(false, pc));
       }
       processedControls.erase(id);
     }

     // processedCourses should now be empty, unless there are local controls not yet added.
     for(set<int>::iterator it = processedControls.begin(); it != processedControls.end(); ++it) {
        pControl pc = oe->getControl(*it, false, false);
        if (pc) {
          success = min(success, syncUpdate(pc, true));
        }
     }

     return success;
   }
   catch (const Exception& er){
     alert(string(er.what())+" [SYNCREAD oClass]");
     return opStatusFail;
   }
}


OpFailStatus MeosSQL::syncUpdate(oClub *c, bool forceWriteAll)
{
  errorMessage.clear();

  if (CmpDataBase.empty())
    return opStatusFail;

  if (!c || !con->connected())
    return opStatusFail;
  auto queryset = con->query();
  queryset << " Name=" << quote << toString(c->name)
    << c->getDI().generateSQLSet(forceWriteAll);

  return syncUpdate(queryset, "oClub", c);
}

OpFailStatus MeosSQL::syncRead(bool forceRead, oClub *c)
{
  errorMessage.clear();

  if (CmpDataBase.empty())
    return opStatusFail;

  if (!c || !con->connected())
    return opStatusFail;

  if (!forceRead) {
    if (!c->existInDB())
      return syncUpdate(c, true);

    if (!c->changed && skipSynchronize(*c))
      return opStatusOKSkipped;
  }
  try {
    auto query = con->query();
    query << "SELECT * FROM oClub WHERE Id=" << c->Id << andWhereOld(c);
    auto res = query.store();

    RowWrapper row;
    if (!res.empty()) {
      row = res.at(0);
      if (!c->changed || isOld(row["Counter"], string(row["Modified"]), c)) {

        OpFailStatus success = opStatusOK;
        if (c->changed)
          success = opStatusWarning;

        storeClub(row, *c);

        c->Modified.update();
        c->changed = false;
        return success;
      }
      else if (c->changed) {
        return syncUpdate(c, false);
      }
    }
    else if (c->changed) {      
      return syncUpdate(c, true);
    }
    return opStatusOK;
  }
  catch (const Exception& er) {
    alert(string(er.what()) + " [SYNCREAD oClub]");
    return opStatusFail;
  }

  return opStatusFail;
}

OpFailStatus MeosSQL::syncUpdate(oControl *c, bool forceWriteAll) {
  errorMessage.clear();

  if (CmpDataBase.empty())
    return opStatusFail;

  if (!c || !con->connected())
    return opStatusFail;

  auto queryset = con->query();
  queryset << " Name=" << quote << toString(c->Name) << ", "
    << " Numbers=" << quote << toString(c->codeNumbers()) << ","
    << " Status=" << int(c->Status)
    << c->getDI().generateSQLSet(forceWriteAll);

  return syncUpdate(queryset, "oControl", c);
}

OpFailStatus MeosSQL::syncRead(bool forceRead, oControl *c)
{
  errorMessage.clear();

  if (CmpDataBase.empty())
    return opStatusFail;

  if (!c || !con->connected())
    return opStatusFail;

  if (!forceRead) {
    if (!c->existInDB())
      return syncUpdate(c, true);

    if (!c->changed && skipSynchronize(*c))
      return opStatusOKSkipped;
  }

  try{
    auto query = con->query();
    query << "SELECT * FROM oControl WHERE Id=" << c->Id << andWhereOld(c);
    auto res = query.store();

    RowWrapper row;
    if (!res.empty()){
      row=res.at(0);

      OpFailStatus success=opStatusOK;
      if (c->changed)
        success=opStatusWarning;

      storeControl(row, *c);
      c->oe->dataRevision++;
      c->Modified.update();
      c->changed=false;
      return success;
    }
    else if (c->changed) {
      return syncUpdate(c, false);
    }

    return opStatusOK;
  }
  catch (const Exception& er){
    alert(string(er.what())+" [SYNCREAD oControl]");
    return opStatusFail;
  }
  return opStatusFail;
}

OpFailStatus MeosSQL::syncUpdate(oCourse *c, bool forceWriteAll)
{
  errorMessage.clear();

  if (CmpDataBase.empty())
    return opStatusFail;

  bool isTMP = c->sqlUpdated == "TMP";
  assert(!isTMP);
  if (isTMP)
    return opStatusFail;

  if (!c || !con->connected())
    return opStatusFail;

  OpFailStatus ret = OpFailStatus::opStatusOK;
  // Check that controls are written
  for (int i = 0; i < c->nControls; i++) {
    if (c->Controls[i] && !c->Controls[i]->existInDB()) {
      ret = min(ret, syncUpdate(c->Controls[i], forceWriteAll));
    }
  }

  auto queryset = con->query();
  queryset << " Name=" << quote << toString(c->Name) << ", "
    << " Length=" << unsigned(c->Length) << ", "
    << " Controls=" << quote << c->getControls() << ", "
    << " Legs=" << quote << c->getLegLengths()
    << c->getDI().generateSQLSet(true);

  return min(ret, syncUpdate(queryset, "oCourse", c));
}

OpFailStatus MeosSQL::syncRead(bool forceRead, oCourse *c)
{
  set<int> controls;
  OpFailStatus res = syncReadCourse(forceRead, c, controls);
  res = min(res, syncReadControls(c->oe, controls));
  return res;
}

OpFailStatus MeosSQL::syncReadCourse(bool forceRead, oCourse *c, set<int> &readControls) {
  errorMessage.clear();

  if (CmpDataBase.empty())
    return opStatusFail;

  if (!c || !con->connected())
    return opStatusFail;

  bool isTMP = c->sqlUpdated == "TMP";
  assert(!isTMP);
  if (isTMP)
    return opStatusFail;

  if (!forceRead) {
    if (!c->existInDB())
      return syncUpdate(c, true);

    if (!c->changed && skipSynchronize(*c))
      return opStatusOKSkipped; // Skipped readout
  }

  try{
    auto query = con->query();
    query << "SELECT * FROM oCourse WHERE Id=" << c->Id << andWhereOld(c);
    ResultWrapper res = query.store();

    RowWrapper row;
    if (!res.empty()) {
      row=res.at(0);

      OpFailStatus success = opStatusOK;
      if (c->changed)
        success = opStatusWarning;

      storeCourse(row, *c, readControls, true);
      c->oe->dataRevision++;
      c->Modified.update();
      c->changed=false;
      return success;
    }
    else {
      OpFailStatus success = opStatusOK;

      // Plain read controls
      for (int i=0;i<c->nControls; i++) {
        if (c->Controls[i])
          readControls.insert(c->Controls[i]->getId());
          //success = min(success, syncRead(false, c->Controls[i]));
      }

      if (c->changed && !forceRead)
        return min(success, syncUpdate(c, false));
      else
        return success;
    }

    return opStatusOK;
  }
  catch (const Exception& er){
    alert(string(er.what())+" [SYNCREAD oCourse]");
    return opStatusFail;
  }

  return opStatusFail;
}

OpFailStatus MeosSQL::syncUpdate(oFreePunch *c, bool forceWriteAll)
{
  errorMessage.clear();

  if (CmpDataBase.empty()) {
    errorMessage = "Not connected";
    return opStatusFail;
  }

  if (!c || !con->connected()) {
    errorMessage = "Not connected";
    return opStatusFail;
  }
  auto queryset = con->query();
  queryset << " CardNo=" <<  c->CardNo << ", "
    << " Type=" << c->type << ","
    << " Time=" << c->punchTime << ","
    << " Origin=" << c->origin << ","
    << " Unit=" << c->punchUnit;

  return syncUpdate(queryset, "oPunch", c);
}

OpFailStatus MeosSQL::syncRead(bool forceRead, oFreePunch *c, bool rehash)
{
  errorMessage.clear();

  if (CmpDataBase.empty())
    return opStatusFail;

  if (!c || !con->connected())
    return opStatusFail;

  if (!forceRead) {
    if (!c->existInDB())
      return syncUpdate(c, true);

    if (!c->changed && skipSynchronize(*c))
      return opStatusOKSkipped;
  }

  try{
    auto query = con->query();
    query << "SELECT * FROM oPunch WHERE Id=" << c->Id << andWhereOld(c);
    auto res = query.store();

    RowWrapper row;
    if (!res.empty()) {
      row=res.at(0);
      OpFailStatus success = opStatusOK;
      if (c->changed)
        success = opStatusWarning;

      storePunch(row, *c, rehash);
      c->oe->dataRevision++;
      c->Modified.update();
      c->changed=false;
      return success;
    }
    else if (c->changed) {
      return syncUpdate(c, false);
    }

    return opStatusOK;
  }
  catch (const Exception& er){
    alert(string(er.what())+" [SYNCREAD oPunch]");
    return opStatusFail;
  }
  return opStatusFail;
}

OpFailStatus MeosSQL::updateTime(const char *oTable, oBase *ob)
{
  errorMessage.clear();

  auto query = con->query();

  query << "SELECT Modified, Counter FROM " << oTable << " WHERE Id=" << ob->Id;

  auto res = query.store();

  if (!res.empty()) {
    ob->sqlUpdated=res.at(0)["Modified"];
    ob->counter = res.at(0)["Counter"];
    ob->changed=false; //Mark as saved.
    // Mark all data as stored in memory
    if (ob->getDISize() >= 0)
      ob->getDI().allDataStored();
    return opStatusOK;
  }
  else {
    alert("Update time failed for " + string(oTable));
    return opStatusFail;
  }
}

static int nUpdate = 0;

ResNSel MeosSQL::updateCounter(const char *oTable, int id, QueryWrapper *updateqry) {
  auto query = con->query();

  try {
    query.exec(string("LOCK TABLES ") + oTable + string(" WRITE"));
    query << "SELECT MAX(Counter) FROM " << oTable;
    int counter;
    {
      const auto c = query.store().at(0).at(0);
      bool null = c.is_null();
      counter = null ? 1 : int(c) + 1;
    }
    query.reset();
    query << "UPDATE " << oTable << " SET Counter=" << counter;

    if (writeTime)
      query << ", Modified=Modified";

    if (updateqry != 0)
      query << "," << updateqry->str();

    query << " WHERE Id=" << id;
        
    ResNSel res = query.execute();

    query.exec("UNLOCK TABLES");

    query.reset();
    query << "UPDATE oCounter SET " << oTable << "=GREATEST(" << counter << "," << oTable << ")";
    query.execute();
    return res;
  }
  catch(...) {
    query.exec("UNLOCK TABLES");
    throw;
  }
}


OpFailStatus MeosSQL::syncUpdate(QueryWrapper &updateqry,
                                 const char *oTable, oBase *ob)
{
  nUpdate++;
  if (nUpdate % 100 == 99)
    OutputDebugStringA((itos(nUpdate) +" updates\n").c_str());

  assert(ob->getEvent());
  if (!ob->getEvent())
    return opStatusFail;

  if (ob->getEvent()->isReadOnly())
    return opStatusOK;

  errorMessage.clear();

  if (!con->connected()) {
    errorMessage = "Not connected";
    return opStatusFail;
  }

  auto query = con->query();
  try{
    if (!ob->existInDB()) {
      if (ob->isRemoved())
        return opStatusOK;
      bool setId = false;
      bool update = false;
      if (ob->Id > 0) {
        query << "SELECT Removed FROM " << oTable << " WHERE Id=" << ob->Id;
        auto res=query.store();
        if (res.empty())
          setId = true;
        else if (ob->isImplicitlyCreated()) {
          return opStatusWarning;//XXX Should we read this object?
        }
        else {
          int removed = res.at(0).at(0);
          if (removed) {
            update = true;
          }
        }
      }
      else {
        assert(!ob->isImplicitlyCreated());
      }

      query.reset();
      if (update) {
        query << "UPDATE " << oTable << " SET Removed = 0, " << updateqry.str();
      }
      else {
        query << "INSERT INTO " << oTable << " SET " << updateqry.str();

        if (setId)
          query << ", Id=" << ob->Id;
      }

      if (writeTime) {
        query << ", Modified='" << ob->getTimeStampN() << "'";
      }

      if (update) {
        query << " WHERE Id=" << ob->Id;
      }

      ResNSel res=query.execute();
      if (res) {
        if (!update) {
          if (ob->Id > 0 && ob->Id != (int)res.insert_id) {
            ob->correctionNeeded = true;
          }

          if (ob->Id != res.insert_id)
            ob->changeId((int)res.insert_id);
        }

        updateCounter(oTable, ob->Id, 0);
        ob->oe->updateFreeId(ob);

        return updateTime(oTable, ob);
      }
      else {
        errorMessage = "Unexpected error: update failed";
        return opStatusFail;
      }
    }
    else {

      ResNSel res = updateCounter(oTable, ob->Id, &updateqry);

      if (res){
        if (res.rows==0){
          query.reset();

          query << "SELECT Id FROM " << oTable << " WHERE Id=" << ob->Id;
          auto store_res = query.store();

          if (store_res.num_rows()==0) {
            if (ob->isRemoved())
              return opStatusOK;
            query.reset();
            query << "INSERT INTO " << oTable << " SET " <<
                updateqry.str() << ", Id=" << ob->Id;

            ResNSel resx=query.execute();
            if (!resx) {
              errorMessage = "Unexpected error: insert failed";
              return opStatusFail;
            }

            updateCounter(oTable, ob->Id, 0);
          }
        }
      }

      return updateTime(oTable, ob);
    }
  }
  catch (const Exception& er){
    alert(string(er.what())+" [" + oTable + " \n\n(" + query.str() + ")]");
    return opStatusFail;
  }

  return opStatusFail;
}

bool MeosSQL::checkOldVersion(oEvent *oe, RowWrapper &row) {
  int dbv=int(row["BuildVersion"]);
  if (dbv < buildVersion) {
    string bv = "UPDATE oEvent SET BuildVersion=if (BuildVersion<" +
           itos(buildVersion) + "," + itos(buildVersion) + ",BuildVersion) WHERE Id = " + itos(oe->Id);

    con->query().exec(bv);
  }
  else if (dbv>buildVersion)
    return true;

  return false;
}

OpFailStatus MeosSQL::SyncEvent(oEvent *oe) {
  errorMessage.clear();
  OpFailStatus retValue = opStatusOK;
  if (!con->connected())
    return opStatusOK;

  bool oldVersion=false;
  try{
    auto query = con->query();

    query << "SELECT * FROM oEvent";
    query << " WHERE Counter>" << oe->counter;

    auto res = query.store();

    if (res && res.num_rows()>0) {
      auto row=res.at(0);
      string Modified=row["Modified"];
      int counter = row["Counter"];

      oldVersion = checkOldVersion(oe, row);
  /*    int dbv=int(row["BuildVersion"]);
      if ( dbv<buildVersion )
        oe->updateChanged();
      else if (dbv>buildVersion)
        oldVersion=true;
*/
      if (isOld(counter, Modified, oe)) {
        oe->Name=fromUTF(string(row["Name"]));
        oe->Annotation = fromUTF(string(row["Annotation"]));
        oe->Date=fromUTF(string(row["Date"]));
        oe->ZeroTime=row["ZeroTime"];
        oe->sqlUpdated=Modified;
        const string &lRaw = row.raw_string(res.field_num("Lists"));
        try {
          importLists(oe, lRaw.c_str());
        }
        catch (std::exception &ex) {
          alert(ex.what());
        }
        const string &mRaw = row.raw_string(res.field_num("Machine"));
        oe->getMachineContainer().load(mRaw);

        oe->counter = counter;
        oDataInterface odi=oe->getDI();
        storeData(odi, row, oe->dataRevision);
        oe->setCurrency(-1, L"", L"", false);//Init temp data from stored data
        oe->getMeOSFeatures().deserialize(oe->getDCI().getString("Features"), *oe);
        oe->changed=false;
        oe->changedObject();
      }
      else if (oe->isChanged()) {

        string listEnc;
        try {
          encodeLists(oe, listEnc);
        }
        catch (std::exception &ex) {
          retValue = opStatusWarning;
          alert(ex.what());
        }

        auto queryset = con->query();
        queryset << " Name=" << quote << limitLength(oe->Name, 128) << ", "
                 << " Annotation="  << quote << limitLength(oe->Annotation, 128) << ", "
                 << " Date="  << quote << toString(oe->Date) << ", "
                 << " NameId="  << quote << toString(oe->currentNameId) << ", "
                 << " ZeroTime=" << unsigned(oe->ZeroTime) << ", "
                 << " BuildVersion=if (BuildVersion<" <<
                      buildVersion << "," << buildVersion << ",BuildVersion), "
                 << " Lists=" << quote << listEnc << ", "
                 << " Machine=" << quote << oe->getMachineContainer().save()
                 <<  oe->getDI().generateSQLSet(false);

        syncUpdate(queryset, "oEvent", oe);

        // Update list database;
        con->select_db("MeOSMain");
        queryset.reset();
        queryset << "UPDATE oEvent SET Name=" << quote << limitLength(oe->Name, 128) << ", "
                 << " Annotation="  << quote << limitLength(oe->Annotation, 128) << ", "
                 << " Date="  << quote << toString(oe->Date) << ", "
                 << " NameId="  << quote << toString(oe->currentNameId) << ", "
                 << " ZeroTime=" << unsigned(oe->ZeroTime)
                 << " WHERE Id=" << oe->Id;

        queryset.execute();
        //syncUpdate(queryset, "oEvent", oe, true);
        con->select_db(CmpDataBase);
      }
    }
    else if ( oe->isChanged() ){
      string listEnc;
      encodeLists(oe, listEnc);

      auto queryset = con->query();
      queryset << " Name=" << quote << limitLength(oe->Name, 128) << ", "
               << " Annotation="  << quote << limitLength(oe->Annotation, 128) << ", "
               << " Date="  << quote << toString(oe->Date) << ","
               << " NameId="  << quote << toString(oe->currentNameId) << ","
               << " ZeroTime=" << unsigned(oe->ZeroTime) << ","
               << " BuildVersion=if (BuildVersion<" <<
                    buildVersion << "," << buildVersion << ",BuildVersion),"
               << " Lists=" << quote << listEnc << ", "
               << " Machine=" << quote << oe->getMachineContainer().save()
               <<  oe->getDI().generateSQLSet(false);

      syncUpdate(queryset, "oEvent", oe);

      // Update list database;
      con->select_db("MeOSMain");
      queryset.reset();
      queryset << "UPDATE oEvent SET Name=" << quote << limitLength(oe->Name, 128) << ", "
               << " Annotation="  << quote << limitLength(oe->Annotation, 128) << ", "
               << " Date="  << quote << toString(oe->Date) << ", "
               << " NameId="  << quote << toString(oe->currentNameId) << ", "
               << " ZeroTime=" << unsigned(oe->ZeroTime)
               << " WHERE Id=" << oe->Id;

      queryset.execute();
      //syncUpdate(queryset, "oEvent", oe, true);
      con->select_db(CmpDataBase);
    }
  }
  catch (const Exception& er){
    setDefaultDB();
    alert(string(er.what())+" [SYNCLIST oEvent]");
    return opStatusFail;
  }

  if (oldVersion) {
    warnOldDB();
    return opStatusWarning;
  }

  return retValue;
}

void MeosSQL::warnOldDB() {
  if (!warnedOldVersion) {
    warnedOldVersion=true;
    alert("warn:olddbversion");
  }
}

bool MeosSQL::syncListRunner(oEvent *oe)
{
  errorMessage.clear();

  if (!con->connected())
    return false;
  int maxCounterRunner = -1;
  try{
    auto query = con->query();

    auto res = query.store(selectUpdated("oRunner", oe->sqlRunners));

    if (res) {
      const auto nr = res.num_rows();
      for (int i = 0; i < nr; i++) {
        OpFailStatus st = OpFailStatus::opUnreachable;

        auto row=res.at(i);
        int Id = row["Id"];
        int counter = row["Counter"];
        string modified = row["Modified"];

        if (int(row["Removed"])==1){
          st = OpFailStatus::opStatusOK;
          oRunner *r=oe->getRunner(Id, 0);

          if (r && !r->Removed) {
            r->Removed=true;
            if (r->tInTeam)
              r->tInTeam->correctRemove(r);

            if (r->tParentRunner) {
              r->tParentRunner->correctRemove(r);
            }

            r->changedObject();
            r->changed = false;
            oe->dataRevision++;
          }
        }
        else{
          oRunner *r=oe->getRunner(Id, 0);

          if (r) {
            if (isOld(counter, modified, r))
              st = syncRead(false, r);
            else
              st = opStatusOK;
          }
          else {
            oRunner oR(oe, Id);
            oR.setImplicitlyCreated();
            st = syncRead(true, &oR, false, false);
            r = oe->addRunner(oR, false);
          }
        }
        updateCounters(st, counter, modified, oe->sqlRunners, maxCounterRunner);
      }
    }
  }
  catch (const Exception& er){
    alert(string(er.what())+" [SYNCLIST oRunner]");
    return false;
  }

  if (maxCounterRunner > 0) 
    oe->sqlRunners.counter = maxCounterRunner - 1;

  return true;
}

bool MeosSQL::syncListClass(oEvent *oe) {
  errorMessage.clear();

  if (!con->connected())
    return false;

  int maxCounter = -1;
  try {
    auto query = con->query();
    auto res = query.store(selectUpdated("oClass", oe->sqlClasses));

    if (res) {
      auto nr = res.num_rows();
      for (int i = 0; i < nr; i++) {
        OpFailStatus st = OpFailStatus::opUnreachable;
        auto row = res.at(i);
        int counter = row["Counter"];
        string modified = row["Modified"];

        int Id = row["Id"];

        if (int(row["Removed"])) {
          st = OpFailStatus::opStatusOK;
          oClass *c = oe->getClass(Id);
          if (c && !c->Removed) {
            c->changedObject();
            c->Removed = true;
            c->changed = false;
            oe->dataRevision++;
          }
        }
        else {
          oClass *c = oe->getClass(Id);

          if (!c) {
            oClass oc(oe, Id);
            oc.setImplicitlyCreated();
            st = syncRead(true, &oc, false);
            c = oe->addClass(oc);
            if (c != 0) {
              c->changed = false;
            }
          }
          else if (isOld(counter, modified, c))
            st = syncRead(false, c, false);
          else
            st = opStatusOK;
        }

        updateCounters(st, counter, modified, oe->sqlClasses, maxCounter);
      }
    }
  }
  catch (const Exception& er){
    alert(string(er.what())+" [SYNCLIST oClass]");
    return false;
  }

  if (maxCounter > 0) 
    oe->sqlClasses.counter = maxCounter - 1;

  return true;
}

bool MeosSQL::syncListClub(oEvent *oe)
{
  errorMessage.clear();

  if (!con->connected())
    return false;
  int maxCounter = -1;

  try {
    auto query = con->query();

    auto res = query.store(selectUpdated("oClub", oe->sqlClubs));

    if (res) {
      const auto nr = res.num_rows();
      for (int i = 0; i < nr; i++) {
        OpFailStatus st = OpFailStatus::opUnreachable;
        auto row = res.at(i);

        int counter = row["Counter"];
        string modified = row["Modified"];
        int Id = row["Id"];

        if (int(row["Removed"])) {
          st = opStatusOK;
          oClub *c = oe->getClub(Id);

          if (c && !c->Removed) {
            c->Removed = true;
            c->changedObject();
            c->changed = false;
            oe->dataRevision++;
          }
        }
        else {
          oClub *c = oe->getClub(Id);

          if (c == 0) {
            oClub oc(oe, Id);
            oc.setImplicitlyCreated();
            st = syncRead(true, &oc);
            oe->addClub(oc);
          }
          else if (isOld(counter, modified, c))
            st = syncRead(false, c);
          else
            st = opStatusOK;
        }
        updateCounters(st, counter, modified, oe->sqlClubs, maxCounter);
      }
    }
  }
  catch (const Exception& er) {
    alert(string(er.what()) + " [SYNCLIST oClub]");
    return false;
  }
  if (maxCounter > 0)
    oe->sqlClubs.counter = maxCounter - 1;

  return true;
}

bool MeosSQL::syncListCourse(oEvent *oe) {
  errorMessage.clear();

  if (!con->connected())
    return false;
  int maxCounter = -1;
  try {
    auto query = con->query();
    auto res = query.store(selectUpdated("oCourse", oe->sqlCourses));

    if (res) {
      set<int> tmp;
      const auto nr = res.num_rows();
      for (int i = 0; i < nr; i++) {
        OpFailStatus st = OpFailStatus::opUnreachable;
        auto row = res.at(i);
        int counter = row["Counter"];
        string modified(row["Modified"]);

        int Id = row["Id"];

        if (int(row["Removed"])) {
          st = opStatusOK;
          oCourse *c = oe->getCourse(Id);

          if (c && !c->Removed) {
            c->Removed = true;
            c->changedObject();
            c->changed = false;
            oe->dataRevision++;
          }
        }
        else {
          oCourse *c = oe->getCourse(Id);

          if (c == 0) {
            oCourse oc(oe, Id);
            oc.setImplicitlyCreated();
            st = syncReadCourse(true, &oc, tmp);
            oe->addCourse(oc);
          }
          else if (isOld(counter, modified, c))
            st = syncReadCourse(false, c, tmp);
          else
            st = opStatusOK;
        }
        updateCounters(st, counter, modified, oe->sqlCourses, maxCounter);
      }
    }
  }
  catch (const Exception& er) {
    alert(string(er.what()) + " [SYNCLIST oCourse]");
    return false;
  }
  if (maxCounter > 0)
    oe->sqlCourses.counter = maxCounter - 1;
  return true;
}

bool MeosSQL::syncListCard(oEvent *oe)
{
  errorMessage.clear();

  if (!con->connected())
    return false;
  int maxCounter = -1;

  try {
    auto query = con->query();
    auto res = query.store(selectUpdated("oCard", oe->sqlCards));

    if (res) {
      const auto nr = res.num_rows();
      for (int i = 0; i < nr; i++) {
        OpFailStatus st = OpFailStatus::opUnreachable;
        auto row = res.at(i);
        int counter = row["Counter"];
        string modified(row["Modified"]);
        int Id = row["Id"];

        if (int(row["Removed"])) {
          st = opStatusOK;
          oCard *c = oe->getCard(Id);
          if (c && !c->Removed) {
            c->changedObject();
            c->Removed = true;
            c->changed = false;
            oe->dataRevision++;
          }
        }
        else {
          oCard *c = oe->getCard(Id);

          if (c) {
            if (isOld(counter, modified, c))
              st = syncRead(false, c);
            else
              st = opStatusOK;
          }
          else {
            oCard oc(oe, Id);
            oc.setImplicitlyCreated();
            c = oe->addCard(oc);
            if (c != 0)
              st = syncRead(true, c);
          }
        }
        updateCounters(st, counter, modified, oe->sqlCards, maxCounter);
      }
    }
  }
  catch (const Exception& er) {
    alert(string(er.what()) + " [SYNCLIST oCard]");
    return false;
  }
  if (maxCounter > 0)
    oe->sqlCards.counter = maxCounter - 1;

  return true;
}

bool MeosSQL::syncListControl(oEvent *oe) {
  errorMessage.clear();

  if (!con->connected())
    return false;
  int maxCounter = -1;

  try {
    auto query = con->query();
    auto res = query.store(selectUpdated("oControl", oe->sqlControls));

    if (res) {
      const auto nr = res.num_rows();
      for (int i = 0; i < nr; i++) {
        OpFailStatus st = OpFailStatus::opUnreachable;
        auto row = res.at(i);
        int counter = row["Counter"];
        string modified(row["Modified"]);
        int Id = row["Id"];

        if (int(row["Removed"])) {
          st = opStatusOK;
          oControl *c = oe->getControl(Id, false, false);

          if (c && !c->Removed) {
            c->Removed = true;
            c->changedObject();
            c->changed = false;
            oe->dataRevision++;
          }
        }
        else {
          oControl *c = oe->getControl(Id, false, false);
          if (c) {
            if (isOld(counter, modified, c))
              st = syncRead(false, c);
            else
              st = opStatusOK;
          }
          else {
            oControl oc(oe, Id);
            oc.setImplicitlyCreated();
            st = syncRead(true, &oc);
            c = oe->addControl(oc);
          }
        }
        updateCounters(st, counter, modified, oe->sqlControls, maxCounter);
      }
    }
  }
  catch (const Exception& er) {
    alert(string(er.what()) + " [SYNCLIST oControl]");
    return false;
  }
  if (maxCounter > 0)
    oe->sqlControls.counter = maxCounter - 1;

  return true;
}

bool MeosSQL::syncListPunch(oEvent *oe)
{
  errorMessage.clear();

  if (!con->connected())
    return false;
  int maxCounter = -1;
  try{
    auto query = con->query();

    auto res = query.store(selectUpdated("oPunch", oe->sqlPunches) + " ORDER BY Id");

    if (res) {
      auto nr = res.num_rows();
      for(int i=0; i<nr; i++){
        OpFailStatus st = opUnreachable;

        auto row=res.at(i);
        int counter = row["Counter"];
        string modified(row["Modified"]);
        int Id=row["Id"];

        if (int(row["Removed"])) {
          st = OpFailStatus::opStatusOK;
          oFreePunch *c=oe->getPunch(Id);
          if (c && !c->Removed) {
            c->Removed=true;
            int cid = c->getControlId();
            oFreePunch::rehashPunches(*oe, c->CardNo, 0);
            pRunner r = oe->getRunner(c->tRunnerId, 0);
            if (r)
              r->markClassChanged(cid);

            c->changed = false;
            oe->dataRevision++;
          }
        }
        else {
          oFreePunch *c=oe->getPunch(Id);

          if (c) {
            if (isOld(counter, modified, c))
              st = syncRead(false, c, true);
            else
              st = opStatusOK;
          }
          else {
            oFreePunch p(oe, Id);
            p.setImplicitlyCreated();
            st = syncRead(true, &p, false);
            oe->addFreePunch(p);
          }
        }

        updateCounters(st, counter, modified, oe->sqlPunches, maxCounter);
      }
    }
  }
  catch (const Exception& er){
    alert(string(er.what())+" [SYNCLIST oPunch]");
    return false;
  }

  if (maxCounter > 0) {
    oe->sqlPunches.counter = maxCounter - 1;
  }
  return true;
}

bool MeosSQL::syncListTeam(oEvent *oe) {
  errorMessage.clear();

  if (!con->connected())
    return false;
  int maxCounterTeam = -1;

  try {
    auto query = con->query();
    auto res = query.store(selectUpdated("oTeam", oe->sqlTeams));
    
    if (res) {
      auto nr = res.num_rows();
      for (int i = 0; i < nr; i++) {
        OpFailStatus st = OpFailStatus::opUnreachable;

        auto row = res.at(i);
        int counter = row["Counter"];
        string modified(row["Modified"]);
        int Id = row["Id"];

        if (int(row["Removed"])) {
          st = OpFailStatus::opStatusOK;
          oTeam *t = oe->getTeam(Id);
          if (t && !t->Removed) {
            t->changedObject();
            t->prepareRemove();
            t->Removed = true;
            t->changed = false;
            oe->dataRevision++;
          }
        }
        else {
          oTeam *t = oe->getTeam(Id);

          if (t) {
            if (isOld(counter, modified, t))
              st = syncRead(false, t, false);
            else
              st = opStatusOK;
          }
          else {
            oTeam ot(oe, Id);
            ot.setImplicitlyCreated();
            t = oe->addTeam(ot, false);
            if (t) {
              st = syncRead(true, t, false);
              t->apply(oBase::ChangeType::Quiet, nullptr);
              t->changed = false;
            }
          }
        }
        updateCounters(st, counter, modified, oe->sqlTeams, maxCounterTeam);
      }
    }
  }
  catch (const Exception& er){
    alert(string(er.what())+" [SYNCLIST oTeam]");
    return false;
  }

  if (maxCounterTeam > 0) {
    oe->sqlTeams.counter = maxCounterTeam - 1;
  }
  return true;
}

string MeosSQL::selectUpdated(const char *oTable, const SqlUpdated &updated) {
  string p1 = string("SELECT Id, Counter, Modified, Removed FROM ") + oTable;
  string cond1 = p1 + " WHERE Counter>" + itos(updated.counter);
  if (updated.updated.empty())
    return cond1;
  
  string q = "(" + cond1  + ") UNION ALL ("+
                   p1 + " WHERE Modified>'" + updated.updated + "' AND Counter<=" + itos(updated.counter) + ")";
  return q;
}

bool MeosSQL::checkConnection(oEvent *oe)
{
  if (!con->connected())
    return false;
  errorMessage.clear();

  if (!oe) {
    if (monitorId && con->connected()) {
      try {
        auto query = con->query();
        query << "Update oMonitor SET Removed=1 WHERE Id = " << monitorId;
        query.execute();
      }
      catch(...) {
        return false; //Not an important error.
      }
    }
    return true;
  }

  oe->connectedClients.clear();
  if (monitorId==0) {
    try {
      auto query = con->query();
      query << "INSERT INTO oMonitor SET Count=1, Client=" << quote << toString(oe->clientName);
      ResNSel res=query.execute();
      if (res)
        monitorId=static_cast<int>(res.insert_id);
    }
    catch (const Exception& er){
      oe->connectedClients.push_back(L"Error: " + fromUTF(er.what()));
      return false;
    }
  }
  else {
    try {
      auto query = con->query();
      query << "Update oMonitor SET Count=Count+1, Client=" << quote << toString(oe->clientName)
            << " WHERE Id = " << monitorId;
      query.execute();
    }
    catch (const Exception& er){
      oe->connectedClients.push_back(L"Error: " + fromUTF(er.what()));
      return false;
    }
  }
  bool callback=false;

  try {
    auto query = con->query();
    query << "SELECT Id, Client FROM oMonitor WHERE Modified>TIMESTAMPADD(SECOND, -30, NOW())"
             " AND Removed=0 ORDER BY Client";

    auto res = query.store();

    if (res) {
      for (int i=0; i<res.num_rows(); i++) {
        auto row=res.at(i);
        oe->connectedClients.push_back(fromUTF(string(row["Client"])));

        if (int(row["Id"])==monitorId)
          callback=true;
      }
    }
  }
  catch (const Exception& er){
    oe->connectedClients.push_back(L"Error: " + fromUTF(er.what()));
    return false;
  }
  return callback;
}

void MeosSQL::setDefaultDB()
{
  errorMessage.clear();

  if (CmpDataBase.empty())
    return;

  try {
    if (!con->connected())
      return;

    con->select_db(CmpDataBase);
  }
  catch(...) {
  }
}

bool MeosSQL::dropDatabase(oEvent *oe)
{
  // Check if other clients are connected.
  if ( !checkConnection(oe) ) {
    if (!oe->connectedClients.empty())
      throw meosException("Database is used and cannot be deleted.");
  }

  if (oe->connectedClients.size()!=1) {
    throw meosException("Database is used and cannot be deleted.");
    return false;
  }

  try {
    con->select_db("MeOSMain");
  }
  catch (const Exception& er) {
    alert(string(er.what()) + " MySQL Error. Select MeosMain");
    setDefaultDB();
    return 0;
  }

  string error;
  try {
    con->drop_db(CmpDataBase);
  }
  catch (const Exception& ex) {
    error = ex.what();
  }

  try {
    auto query = con->query();
    query << "DELETE FROM oEvent WHERE NameId=" << quote << CmpDataBase;
    query.execute();
  }
  catch (const Exception& ex) {
    if (!error.empty())
      error += ", ";
    error += ex.what();
    //Don't care if we fail.
  }

  CmpDataBase.clear();

  errorMessage.clear();

  try {
    con->close();
  }
  catch (const Exception&) {
  }

  if (!error.empty())
    throw meosException(error);

  return true;
}

void MeosSQL::importLists(oEvent *oe, const char *bf) {
  xmlparser xml;
  xml.readMemory(bf, 0);
  oe->listContainer->clearExternal();
  oe->listContainer->load(MetaListContainer::ExternalList, xml.getObject("Lists"), false);
}

void MeosSQL::encodeLists(const oEvent *oe, string &listEnc) const {
  xmlparser parser;
  parser.openMemoryOutput(true);
  parser.startTag("Lists");
  oe->listContainer->save(MetaListContainer::ExternalList, parser, oe);
  parser.endTag();
  parser.getMemoryOutput(listEnc);
}

void MeosSQL::clearReadTimes() {
  readTimes.clear();
}

int getTypeId(const oBase &ob)
{
  if (typeid(ob)==typeid(oRunner)){
    return 1;
  }
  else if (typeid(ob)==typeid(oClass)){
    return 2;
  }
  else if (typeid(ob)==typeid(oCourse)){
    return 3;
  }
  else if (typeid(ob)==typeid(oControl)){
    return 4;
  }
  else if (typeid(ob)==typeid(oClub)){
    return 5;
  }
  else if (typeid(ob)==typeid(oCard)){
    return 6;
  }
  else if (typeid(ob)==typeid(oFreePunch)){
    return 7;
  }
  else if (typeid(ob)==typeid(oTeam)){
    return 8;
  }
  else if (typeid(ob)==typeid(oEvent)){
    return 9;
  }
  return -1;
}
static int skipped = 0, notskipped = 0, readent = 0;

void MeosSQL::synchronized(oBase &entity) {
  entity.Modified.setStamp(entity.sqlUpdated);
  
  int id = getTypeId(entity);
  readTimes[make_pair(id, entity.getId())] = GetTickCount();
  readent++;
  if (readent % 100 == 99)
    OutputDebugStringA("Read 100 entities\n");
}

bool MeosSQL::skipSynchronize(const oBase &entity) const {
  int id = getTypeId(entity);
  map<pair<int, int>, DWORD>::const_iterator res = readTimes.find(make_pair(id, entity.getId()));

  if (res != readTimes.end()) {
    DWORD t = GetTickCount();
    if (t > res->second && (t - res->second) < 1000) {
      skipped++;
      return true;
    }
  }

  notskipped++;
  return false;
}
namespace {
  int encode(oListId id) {
    return int(id);
  }
}

int MeosSQL::getModifiedMask(oEvent &oe) {
  try {
    auto query = con->query();
    int res = 0;
    auto store_res = query.store("SELECT * FROM oCounter");
    if (store_res.num_rows()>0) {
      auto r = store_res.at(0);
      int ctrl = r["oControl"];
      int crs = r["oCourse"];
      int cls = r["oClass"];
      int card = r["oCard"];
      int club = r["oClub"];
      int punch = r["oPunch"];
      int runner = r["oRunner"];
      int t = r["oTeam"];
      int e = r["oEvent"];

      if (ctrl > oe.sqlControls.counter)
        res |= encode(oListId::oLControlId);
      if (crs > oe.sqlCourses.counter)
        res |= encode(oListId::oLCourseId);
      if (cls > oe.sqlClasses.counter)
        res |= encode(oListId::oLClassId);
      if (card > oe.sqlCards.counter)
        res |= encode(oListId::oLCardId);
      if (club > oe.sqlClubs.counter)
        res |= encode(oListId::oLClubId);
      if (punch > oe.sqlPunches.counter)
        res |= encode(oListId::oLPunchId);
      if (runner > oe.sqlRunners.counter)
        res |= encode(oListId::oLRunnerId);
      if (t > oe.sqlTeams.counter)
        res |= encode(oListId::oLTeamId);
      if (e > oe.counter)
        res |= encode(oListId::oLEventId);

      return res;
    }
  }
  catch(...) {
  }
  return -1;
}

void MeosSQL::addedFromDatabase(oBase *object) {
  assert(object);
  if (object && !object->existInDB()) {
    missingObjects.push_back(object);
  }
}

void MeosSQL::processMissingObjects() {
  if (!missingObjects.empty()) {
    auto cpyMissing = missingObjects;
    missingObjects.clear();
    for (oBase *obj : cpyMissing) {
      obj->changed = true;
      syncRead(true, obj);
      obj->changed = false;
      assert(obj->existInDB());
      {
        oRunner *r = dynamic_cast<oRunner *>(obj);
        if (r && r->getName().empty()) {
          r->setName(L"@AutoCorrection", false);
          syncUpdate(r, false);
        }
      }
      {
        oTeam *t = dynamic_cast<oTeam *>(obj);
        if (t && t->getName().empty()) {
          t->setName(L"@AutoCorrection", false);
          syncUpdate(t, false);
        }
      }
      {
        oClass *cls = dynamic_cast<oClass *>(obj);
        if (cls && cls->getName().empty()) {
          cls->setName(L"@AutoCorrection", false);
          syncUpdate(cls, false);
        }
      }
      {
        oCourse *crs = dynamic_cast<oCourse *>(obj);
        if (crs && crs->getName().empty()) {
          crs->setName(L"@AutoCorrection");
          syncUpdate(crs, false);
        }
      }
      {
        oClub *clb = dynamic_cast<oClub *>(obj);
        if (clb && clb->getName().empty()) {
          clb->setName(L"@AutoCorrection");
          syncUpdate(clb, false);
        }
      }
    }
  }
  missingObjects.clear();
}

OpFailStatus MeosSQL::syncRead(bool forceRead, oBase *obj) {
  OpFailStatus ret = OpFailStatus::opStatusFail;

  if (typeid(*obj) == typeid(oRunner)) {
    ret = syncRead(forceRead, (oRunner *)obj);
  }
  else if (typeid(*obj) == typeid(oClass)) {
    ret = syncRead(forceRead, (oClass *)obj);
  }
  else if (typeid(*obj) == typeid(oCourse)) {
    ret = syncRead(forceRead, (oCourse *)obj);
  }
  else if (typeid(*obj) == typeid(oControl)) {
    ret = syncRead(forceRead, (oControl *)obj);
  }
  else if (typeid(*obj) == typeid(oClub)) {
    ret = syncRead(forceRead, (oClub *)obj);
  }
  else if (typeid(*obj) == typeid(oCard)) {
    ret = syncRead(forceRead, (oCard *)obj);
  }
  else if (typeid(*obj) == typeid(oFreePunch)) {
    ret = syncRead(forceRead, (oFreePunch *)obj, true);
  }
  else if (typeid(*obj) == typeid(oTeam)) {
    ret = syncRead(forceRead, (oTeam *)obj);
  }
  else if (typeid(*obj) == typeid(oEvent)) {
    ret = SyncRead((oEvent *)obj);
  }
  else 
    throw std::exception("Database error");

  processMissingObjects();

  return ret;
}

void MeosSQL::updateCounters(OpFailStatus st, 
                             int counter, 
                             const string &modified, 
                             SqlUpdated &update, 
                             int &maxCounter) {
  if (st == OpFailStatus::opStatusOK || st == OpFailStatus::opStatusWarning) {
    update.counter = max(counter, update.counter);
    update.updated = max(modified, update.updated);
  }
  else /*if (st == opStatusOKSkipped)*/ {
    if (maxCounter < 0)
      maxCounter = counter;
    else
      maxCounter = min(counter, maxCounter);
  }
}

void  MeosSQL::checkAgainstDB(const char *oTable, map<int, oBase *> &existing, vector<pair<int, oBase *>> &idsToUpdate) {
  idsToUpdate.clear();

  auto query = con->query();
  query << "SELECT Id, Modified, Counter, Removed FROM " << oTable;
  auto res = query.store();
  
  if (res) {
    for (int i = 0; i < res.num_rows(); i++) {
      auto row = res.at(i);
      int id = row["Id"];
      bool removed = row["Removed"];
      auto e = existing.find(id);
      if (e == existing.end()) {
        if (!removed)
          idsToUpdate.emplace_back(id, nullptr);
      }
      else {
        string modified = row["Modified"];
        int c = row["Counter"];
        if (modified != e->second->sqlUpdated || c != e->second->counter)
          idsToUpdate.emplace_back(id, e->second);

        existing.erase(e);
      }
    }

    for (auto &e : existing) {
      if (e.second->existInDB()) {
        e.second->Removed = true;
        pTeam t = dynamic_cast<oTeam *>(e.second);
        if (t)
          t->prepareRemove();
        e.second->changedObject();
        e.second->oe->dataRevision++;
        e.second->changed = false;
      }
    }
  }
}

template<typename T>
bool MeosSQL::checkTableCheckSum(const char *oTable, const list<T> &def, int p1, int p2, int p3) {
  int csCounter1 = 0;
  int csCounter2 = 0;
  int csCounter3 = 0;
  int csum = 0;
  bool validT = false;
  for (const T &ent : def) {
    if (ent.isRemoved() || !ent.existInDB())
      continue;
    int c = ent.counter;
    int t = 0;
    if (ent.sqlUpdated.length() > 8) {
      int len = ent.sqlUpdated.length();
      if (ent.sqlUpdated[len - 3] == ':' && ent.sqlUpdated[len - 6] == ':') {
        validT = true;
        constexpr int zero = '0';
        int s = ent.sqlUpdated[len - 1] + 10 * ent.sqlUpdated[len - 2];
        int m = 60 * ent.sqlUpdated[len - 4] + 600 * ent.sqlUpdated[len - 5];
        t = s + m - (zero + 10 * zero + 60 * zero + 600 * zero);
      }
    }
    csCounter1 += (ent.Id % p1) * (c % p2);
    csCounter2 += ((ent.Id + 55) % p2) * ((c + 13) % p3);
    csCounter3 += ((ent.Id + 11) % p3) * ((c + 17*t) % p1);
    csum += c;
  }

  auto query = con->query();
  query << "SELECT SUM((Id%" << p1 << ")*(Counter%" << p2 << ")), ";
  query << "SUM(((Id+55)%" << p2 << ")*((Counter+13)%" << p3 << ")), ";
  query << "SUM(((Id+11)%" << p3 << ")*((Counter+17*(second(Modified)+60*minute(Modified)))%" << p1 << "))  ";
  query << "FROM " << oTable << " WHERE Removed<>1";
  auto res = query.store();

  if (res) {
    auto row = res.at(0);
    int a = row[0];
    int b = row[1];
    int c = row[2];
    return csCounter1 == a && csCounter2 == b && (!validT || csCounter3 == c);
  }

  return true;
}

bool MeosSQL::checkConsistency(oEvent *oe, bool force) {
  try {
    bool doCheck = force;

    constexpr int numPrimes = 6;
    static const int primes[numPrimes] = { 919, 1201, 1033, 907, 1049, 997 };
    static int checkCounter = 0;
    int p1 = 0, p2 = 0, p3 = 0;
    if (!doCheck) {
      p1 = primes[checkCounter%numPrimes];
      p2 = primes[(checkCounter + 1) % numPrimes];
      p3 = primes[(checkCounter + 2) % numPrimes];
      checkCounter++;
    }

    if (!doCheck)
      doCheck = !checkTableCheckSum("oControl", oe->Controls, p1, p2, p3);

    if (!doCheck)
      doCheck = !checkTableCheckSum("oCourse", oe->Courses, p1, p2, p3);

    if (!doCheck)
      doCheck = !checkTableCheckSum("oClass", oe->Classes, p1, p2, p3);

    if (!doCheck)
      doCheck = !checkTableCheckSum("oClub", oe->Clubs, p1, p2, p3);

    if (!doCheck)
      doCheck = !checkTableCheckSum("oCard", oe->Cards, p1, p2, p3);

    if (!doCheck)
      doCheck = !checkTableCheckSum("oRunner", oe->Runners, p1, p2, p3);

    if (!doCheck)
      doCheck = !checkTableCheckSum("oTeam", oe->Teams, p1, p2, p3);

    if (!doCheck)
      doCheck = !checkTableCheckSum("oPunch", oe->punches, p1, p2, p3);

    if (!doCheck)
      return false;

    map<int, oBase *> bmap;
    vector<pair<int, oBase *>> needUpdate;

    auto setupMap = [&bmap](auto &list) {
      bmap.clear();
      for (auto &it : list) {
        if (!it.isRemoved()) {
          bmap.emplace(it.Id, &it);
        }
      }
    };

    setupMap(oe->Controls);
    checkAgainstDB("oControl", bmap, needUpdate);
    for (auto id : needUpdate) {
      if (!id.second) {
        oControl oR(oe, id.first);
        oR.setImplicitlyCreated();
        auto c = oe->addControl(oR);
        if (c != nullptr)
          syncRead(true, c);
      }
      else {
        id.second->counter = 0;
        syncRead(true, (oControl *)id.second);
      }
    }

    setupMap(oe->Courses);
    checkAgainstDB("oCourse", bmap, needUpdate);
    for (auto id : needUpdate) {
      if (!id.second) {
        oCourse oR(oe, id.first);
        oR.setImplicitlyCreated();
        auto c = oe->addCourse(oR);
        if (c != nullptr)
          syncRead(true, c);
      }
      else {
        id.second->counter = 0;
        syncRead(true, (oCourse *)id.second);
      }
    }

    setupMap(oe->Classes);
    checkAgainstDB("oClass", bmap, needUpdate);
    for (auto id : needUpdate) {
      if (!id.second) {
        oClass oR(oe, id.first);
        oR.setImplicitlyCreated();
        auto c = oe->addClass(oR);
        if (c != nullptr)
          syncRead(true, c);
      }
      else {
        id.second->counter = 0;
        syncRead(true, (oClass *)id.second);
      }
    }

    setupMap(oe->Clubs);
    checkAgainstDB("oClub", bmap, needUpdate);
    for (auto id : needUpdate) {
      if (!id.second) {
        oClub oR(oe, id.first);
        oR.setImplicitlyCreated();
        auto c = oe->addClub(oR);
        if (c != nullptr)
          syncRead(true, c);
      }
      else {
        id.second->counter = 0;
        syncRead(true, (oClub *)id.second);
      }
    }

    setupMap(oe->Cards);
    checkAgainstDB("oCard", bmap, needUpdate);
    for (auto id : needUpdate) {
      if (!id.second) {
        oCard oR(oe, id.first);
        oR.setImplicitlyCreated();
        auto c = oe->addCard(oR);
        if (c != nullptr)
          syncRead(true, c);
      }
      else {
        id.second->counter = 0;
        syncRead(true, (oCard *)id.second);
      }
    }

    setupMap(oe->Runners);
    checkAgainstDB("oRunner", bmap, needUpdate);
    for (auto id : needUpdate) {
      if (!id.second) {
        oRunner oR(oe, id.first);
        oR.setImplicitlyCreated();
        syncRead(true, &oR);
        oe->addRunner(oR, false);
      }
      else {
        id.second->counter = 0;
        syncRead(true, (oRunner *)id.second);
      }
    }

    setupMap(oe->Teams);
    checkAgainstDB("oTeam", bmap, needUpdate);
    for (auto id : needUpdate) {
      if (!id.second) {
        oTeam ot(oe, id.first);
        ot.setImplicitlyCreated();
        auto t = oe->addTeam(ot, false);
        if (t) {
          syncRead(true, t, false);
          t->apply(oBase::ChangeType::Quiet, nullptr);
          t->changed = false;
        }
      }
      else {
        id.second->counter = 0;
        auto t = (oTeam *)id.second;
        syncRead(true, t);
        t->apply(oBase::ChangeType::Quiet, nullptr);
        t->changed = false;
      }
    }

    setupMap(oe->punches);
    checkAgainstDB("oPunch", bmap, needUpdate);
    for (auto id : needUpdate) {
      if (!id.second) {
        oFreePunch p(oe, id.first);
        p.setImplicitlyCreated();
        syncRead(true, &p, false);
        oe->addFreePunch(p);
      }
      else {
        id.second->counter = 0;
        syncRead(true, (oFreePunch *)id.second);
      }
    }

    return true;
  }
  catch (const Exception &ex) {
    throw meosException(ex.what());
  }
}


bool MeosSQL::synchronizeList(oEvent *oe, oListId lid) {
  bool ret = false;
  switch (lid) {
  case oListId::oLRunnerId:
    ret = syncListRunner(oe);
    break;
  case oListId::oLClassId:
    ret = syncListClass(oe);
    break;
  case oListId::oLCourseId:
    ret = syncListCourse(oe);
    break;
  case oListId::oLControlId:
    ret = syncListControl(oe);
    break;
  case oListId::oLClubId:
    ret = syncListClub(oe);
    break;
  case oListId::oLCardId:
    ret = syncListCard(oe);
    break;
  case oListId::oLPunchId:
    ret = syncListPunch(oe);
    break;
  case oListId::oLTeamId:
    ret = syncListTeam(oe);
    break;
  }

  processMissingObjects();
  return ret;
}

OpFailStatus MeosSQL::synchronizeUpdate(oBase *obj) {
  if (typeid(*obj) == typeid(oRunner)) {
    return syncUpdate((oRunner *)obj, false);
  }
  else if (typeid(*obj) == typeid(oClass)) {
    return syncUpdate((oClass *)obj, false);
  }
  else if (typeid(*obj) == typeid(oCourse)) {
    return syncUpdate((oCourse *)obj, false);
  }
  else if (typeid(*obj) == typeid(oControl)) {
    return syncUpdate((oControl *)obj, false);
  }
  else if (typeid(*obj) == typeid(oClub)) {
    return syncUpdate((oClub *)obj, false);
  }
  else if (typeid(*obj) == typeid(oCard)) {
    return syncUpdate((oCard *)obj, false);
  }
  else if (typeid(*obj) == typeid(oFreePunch)) {
    return syncUpdate((oFreePunch *)obj, false);
  }
  else if (typeid(*obj) == typeid(oEvent)) {

    return SyncUpdate((oEvent *)obj);
  }
  else if (typeid(*obj) == typeid(oTeam)) {
    return syncUpdate((oTeam *)obj, false);
  }
  return OpFailStatus::opStatusFail;
}

OpFailStatus MeosSQL::enumerateImages(vector<pair<wstring, uint64_t>>& images) {
  try {
    auto query = con->query();
    images.clear();
    auto res = query.store("SELECT Id, Filename FROM oImage");
    if (res) {
      for (int i = 0; i < res.num_rows(); i++) {
        auto row = res.at(i);
        wstring fileName = fromUTF((string)row["Filename"]);
        uint64_t id = row["Id"].ulonglong();
        images.emplace_back(fileName, id);
      }
    }
  }
  catch (const Exception& ) {
    return OpFailStatus::opStatusFail;
  }

  return OpFailStatus::opStatusOK;
}

OpFailStatus MeosSQL::getImage(uint64_t id, wstring& fileName, vector<uint8_t>& data) {
  try {
    auto query = con->query();
    auto res = query.store("SELECT * FROM oImage WHERE id=" + itos(id));
    if (res && res.num_rows() > 0) {
      auto row = res.at(0);
      fileName = fromUTF((string)row["Filename"]);
      row["Image"].storeBlob(data);
      return OpFailStatus::opStatusOK;
    }
  }
  catch (const Exception&) {
  }

  return OpFailStatus::opStatusFail;
}

string hexEncode(const vector<uint8_t>& data) {
  string out;
  out.reserve(4 + data.size() * 2);
  out.append("X'");

  char table[17] = "0123456789ABCDEF";
  char block[33];
  block[32] = 0;

  for (int j = 0; j < data.size();) {
    if (j + 16 < data.size()) {
      for (int i = 0; i < 32; ) {
        uint8_t b = data[j];
        int bLow = b & 0xF;
        int bHigh = (b >> 4) & 0xF;
        block[i++] = table[bHigh];
        block[i++] = table[bLow];
        j++;
      }
    }
    else {
      int i = 0;
      while (j < data.size()) {
        uint8_t b = data[j];
        int bLow = b & 0xF;
        int bHigh = (b >> 4) & 0xF;
        block[i++] = table[bHigh];
        block[i++] = table[bLow];
        j++;
      }
      block[i] = 0;
    }

    out.append(block);
  }
  out.append("'");
  return out;
}

OpFailStatus MeosSQL::storeImage(uint64_t id, const wstring& fileName, const vector<uint8_t>& data) {
  try {
    auto query = con->query();
    auto res = query.store("SELECT Id FROM oImage WHERE Id=" + itos(id));
    if (res.empty()) {
      query << ("INSERT INTO oImage SET Id=" + itos(id) + ", Filename=")
        << quote << toString(fileName) << ", Image=" << hexEncode(data);
      query.execute();
    }
  }
  catch (Exception &) {
    return OpFailStatus::opStatusFail;
  }

  return OpFailStatus::opStatusOK;
}
