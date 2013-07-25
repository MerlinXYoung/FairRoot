#include "FairDbSqlValPacket.h"
#include "FairDbLogService.h"
#include "FairDb.h"                     // for MakeDateTimeString, etc
#include "FairDbConfigSet.h"            // for FairDbConfigSet
#include "FairDbMultConnector.h"        // for string, FairDbMultConnector
#include "FairDbOutRowStream.h"         // for FairDbOutRowStream
#include "FairDbProxy.h"                // for FairDbProxy
#include "FairDbResult.h"               // for cout, FairDbResultSet
#include "FairDbStatement.h"            // for FairDbStatement
#include "FairDbString.h"               // for StringTok
#include "FairDbTableMetaData.h"        // for FairDbTableMetaData
#include "FairDbTableProxy.h"           // for FairDbTableProxy
#include "FairDbTableProxyRegistry.h"   // for FairDbTableProxyRegistry
#include "FairDbTableRow.h"             // for FairDbTableRow
#include "FairDbValidityRec.h"          // for FairDbValidityRec

#include "TString.h"                    // for TString

#include <algorithm>                    // for sort
#include <iostream>                     // for cout
#include <memory>                       // for auto_ptr, etc
#include <vector>                       // for vector

class ValRange;

using std::cout;
using std::endl;
using std::string;
using std::list;
using std::vector;
using std::ostringstream;
using std::istringstream;
using std::auto_ptr;

ClassImp(FairDbSqlValPacket)



Bool_t FairDbSqlValPacket::fgLastMetaHadRowCounter = false;



static bool compStringPtrs(const string* str1, const string* str2 )
{
  return *str1 < *str2;
}


FairDbSqlValPacket::FairDbSqlValPacket() :
  fNumErrors(0),
  fSeqNo(0),
  fSqlMySqlMetaMain(),
  fSqlMySqlMetaVal(),
  fSqlStmts(),
  fNumStmts(0),
  fTableName(),
  fCreationDate()
{
}



FairDbSqlValPacket::FairDbSqlValPacket(std::ifstream& is) :
  fNumErrors(0),
  fSeqNo(0),
  fSqlMySqlMetaMain(),
  fSqlMySqlMetaVal(),
  fSqlStmts(),
  fNumStmts(0),
  fTableName(),
  fCreationDate()
{
  Fill(is);
}


FairDbSqlValPacket::FairDbSqlValPacket(const FairDbValidityRec& vrec) :
  fNumErrors(0),
  fSeqNo(vrec.GetSeqNo()),
  fSqlMySqlMetaMain(),
  fSqlMySqlMetaVal(),
  fSqlStmts(),
  fNumStmts(0),
  fTableName(vrec.GetTableProxy()->GetTableName()),
  fCreationDate(vrec.GetCreationDate())
{

  const FairDbTableProxy& tableProxy = *vrec.GetTableProxy();
  Int_t seqNo  = vrec.GetSeqNo();
  UInt_t dbNo  = vrec.GetDbNo();

  this->AddRow(tableProxy,0,vrec);

  const FairDbProxy& dbProxy = tableProxy.GetDBProxy();
  FairDbResultSet* rset = dbProxy.QuerySeqNo(seqNo,dbNo);

  for(; ! rset->IsExhausted(); rset->FetchRow()) {
    string str;
    rset->RowAsCsv(str);
    this->AddRow(str);
  }
  delete rset;
  rset = 0;
}



FairDbSqlValPacket::~FairDbSqlValPacket()
{
  Clear();
}



Bool_t FairDbSqlValPacket::AddDataRow(const FairDbTableProxy& tblProxy,
                                      const FairDbValidityRec* vrec,
                                      const FairDbTableRow& row)
{

  if ( this->GetNumSqlStmts() == 0 ) {

    MAXDBLOG("FairDb",FairDbLog::kError,20)
        << "Cannot add Data Row - packet does not have a corresponding VALIDITY Row !"  << endl;
    ++fNumErrors;
    return kFALSE;
  }

  return this->AddRow(tblProxy,vrec,row);

}


void FairDbSqlValPacket::AddRow(const string& row)
{
  // Problem with PostGres
  //cout << " FairDbSqlValPacket AddRow " << row << endl;

  string sql("INSERT INTO ");
  sql += this->GetTableName();
  if ( this->GetNumSqlStmts() == 0 ) { sql += "VAL"; }
  sql += " VALUES (" + row + ");";
  ostringstream seqno;
  seqno << this->GetSeqNo();
  this->SetSeqNoOnRow(sql,seqno.str());
  fSqlStmts.push_back(sql);
  ++fNumStmts;

  //cout << "-I- FairDbSqlValPacket::AddRow() " << sql << endl;
}



Bool_t FairDbSqlValPacket::AddRow(const FairDbTableProxy& tblProxy,
                                  const FairDbValidityRec* vrec,
                                  const FairDbTableRow& row)
{

  bool isVal = this->GetNumSqlStmts() == 0;
  const FairDbTableMetaData& meta = isVal ? tblProxy.GetMetaValid() : tblProxy.GetMetaData();
  FairDbOutRowStream outRow(&meta);

// Store a dummy SEQNO, and, if necessary ROW_COUNTER, for data rows.
  if ( ! isVal ) {
    // define a dummy SEQNO
    outRow << 0;
    if ( outRow.HasRowCounter() ) { outRow <<  fNumStmts; }
  }

  row.Store(outRow,vrec);
  if ( ! outRow.HasGoodData() ) {
    if ( ! outRow.IsComplete() ) {
      MAXDBLOG("FairDb",FairDbLog::kError,20)
          << "Incomplete data supplied for row " << this->GetNumSqlStmts()-1
          << " of table "
          << tblProxy.GetTableName() << endl;
    }
    ++fNumErrors;
    return kFALSE;
  }


  this->AddRow(outRow.GetCSV());
  return kTRUE;
}
//.....................................................................

FairDbSqlValPacket::CompResult_t FairDbSqlValPacket::Compare(
  const FairDbSqlValPacket& that,
  Bool_t log,
  const Char_t* thisName,
  const Char_t* thatName ) const
{

  if ( this->IsEqual(that,log,thisName,thatName ) ) { return kIdentical; }

  if (    fSeqNo           != that.fSeqNo
          || fTableName       != that.fTableName     ) { return kConflict; }

  std::vector<std::string> valuesThis = this->GetStmtValues(0);
  std::vector<std::string> valuesThat = that.GetStmtValues(0);

  // Assume CreationDate is the 7th element
  // (counting begin from 0).
  int comp = valuesThis[7].compare(valuesThat[7]);

  if ( comp < 0 ) {
    if ( log ) {
      DBLOG("FairDb",FairDbLog::kInfo) << "Difference classified as Update" << endl;
    };
    return kUpdate;
  } else if ( comp > 0 ) {
    if ( log ) {
      DBLOG("FairDb",FairDbLog::kInfo) << "Difference classified as OutOfDate" << endl;
    }
    return kOutOfDate;
  }
  return kConflict;

}


Bool_t FairDbSqlValPacket::CreateTable(UInt_t dbNo) const
{
  if ( ! CanBeStored() ) { return kFALSE; }


  auto_ptr<FairDbStatement> stmtDb(FairDbTableProxyRegistry::Instance()
                                   .GetMultConnector()
                                   .CreateStatement(dbNo));
  if ( ! stmtDb.get() ) {

    DBLOG("FairDb",FairDbLog::kWarning)
        << "Attempting to write to non-existant cascade entry " << dbNo
        << endl;

    return kFALSE;
  }
  if ( fSqlMySqlMetaMain == "" || fSqlMySqlMetaVal  == "" ) {

    DBLOG("FairDb",FairDbLog::kWarning) << "No SQL available to create table " << fTableName
                                        << " in cascade entry: " << dbNo << endl;

    return kFALSE;
  }

  stmtDb->ExecuteUpdate(fSqlMySqlMetaVal.c_str());
  if ( stmtDb->PrintExceptions() ) { return kFALSE; }
  stmtDb->ExecuteUpdate(fSqlMySqlMetaMain.c_str());
  if ( stmtDb->PrintExceptions() ) { return kFALSE; }

  FairDbTableProxyRegistry::Instance().RefreshMetaData(this->GetTableName());

  return kTRUE;

}



Bool_t FairDbSqlValPacket::Fill(std::ifstream& is)
{

  enum { kLOOKING_FOR_HEADER,
         kLOOKING_FOR_TRAILER
       } state = kLOOKING_FOR_HEADER;

  enum { kMAXTABLENAMELEN        = FairDb::kMAXTABLENAMELEN,
         kHEADER_TRAILER_MAX_LEN = kMAXTABLENAMELEN + 20
       };

  string nameHead;
  string nameTrail;
  UInt_t seqNoHead  = 0;
  UInt_t seqNoTrail = 0;

  string line;
  string msg;
  string sql;
  int lineNum = 0;

  this->Reset();
  //  Loop until EOF reading lines.
  while ( ! is.eof() ) {
    getline(is,line);
    ++lineNum;
    // Skip null lines.
    if (line.size() == 0 ) { continue; }
    // Look for header line
    if ( state == kLOOKING_FOR_HEADER ) {
      if ( line.substr(0,5) == ">>>>>" ) {
        if ( line.size() >= kHEADER_TRAILER_MAX_LEN ) {
          Report("Bad header",lineNum,line);
          continue;
        }
        // Look for optional metadata.
        if ( line.find("Metadata") != string::npos ) {
          getline(is,fSqlMySqlMetaVal);
          ++lineNum;
          getline(is,fSqlMySqlMetaMain);
          // Record whether or not table has a ROW_COUNTER column.
          fgLastMetaHadRowCounter =
            fSqlMySqlMetaMain.find("ROW_COUNTER",fSqlMySqlMetaMain.find("(")) != string::npos;
          ++lineNum;
          getline(is,line);
          ++lineNum;
          if (   line.substr(0,5) != "<<<<<"
                 || line.find("Metadata") == string::npos ) {
            Report("Bad metadata",lineNum,line);
            continue;
          }
          getline(is,line);
          ++lineNum;
          if ( line.size() >= kHEADER_TRAILER_MAX_LEN ) {
            Report("Bad header",lineNum,line);
            continue;
          }
        }
        //  Collect table name and SeqNo.
        istringstream istr(line.substr(5));
        istr.width(kMAXTABLENAMELEN);
        istr >> nameHead >> seqNoHead;
        if ( ! istr.eof() ) {
          Report("Input error",lineNum,line);
          continue;
        }
        //if  Header OK,  start to collect SQL.
        state = kLOOKING_FOR_TRAILER;
        sql = "";
      } else {
        Report("Not header",lineNum,line);
      }
    }

    //Look for trailer line.
    else {
      if ( line.substr(0,5) == "<<<<<" ) {
        if ( line.size() >= kHEADER_TRAILER_MAX_LEN
           ) { msg = "Bad trailer"; }

        else {
          //  Collect table name and SeqNo.
          istringstream istr(line.substr(5));
          istr.width(kMAXTABLENAMELEN);
          istr >> nameTrail >> seqNoTrail;
          if ( ! istr.eof() ) { msg = "Input error"; }

          else if (    nameTrail != nameHead
                       || seqNoHead != seqNoTrail ) {
            msg = "Header/Trailer mismatch: Header: ";
            msg += istr.str();
          } else if ( GetNumSqlStmts() == 0
                    ) { msg = "No SQL statements between Header/Trailer"; }
          else {

            // Trailer looks good return with object filled.
            fSeqNo     = seqNoHead;
            fTableName = nameHead;
            //Dig out the creation date from the first record.
            string date = this->GetStmtValues(0)[7];
            //Remove the quotes.
            date.erase(0,1);
            date.erase(date.size()-1,1);
            fCreationDate = FairDb::MakeTimeStamp(date);
            // Temporary hack: set for Agg No = -1 for FAIRDBSUBRUNSUMMARY
            if ( fTableName  == "FAIRDBSUBRUNSUMMARY" ) {
              std::vector<std::string> ls;
              FairUtilString::StringTok(ls,*fSqlStmts.begin(),",");
              // Agg. no is element 6.
              if (ls.size() >= 7 && ls[6] != "-1") {
                static bool warnOnce = true;
                if ( warnOnce ) {

                  DBLOG("FairDb",FairDbLog::kWarning)  << "Setting aggregate number for FAIRDBSUBRUNSUMMARY = -1\n"
                                                       << "  Expect this message once.  FAIRDBSUBRUNSUMMARY needs to be "
                                                       << " fixed!" << endl;
                  warnOnce = false;
                }
                ls[6] = "-1";
                sql = ls[0];
                for (unsigned i=1; i<ls.size(); ++i ) { sql += "," + ls[i]; }
                fSqlStmts.pop_front();
                fSqlStmts.push_front(sql);
              }
            }
            return kTRUE;

          }
        }

        // Trailer bad, start again!
        Report(msg.c_str(),lineNum,line);
        state = kLOOKING_FOR_HEADER;
      }

      // Not a trailer line, must be SQL, collect and append lines
      // until a trailing ; found.
      else {
        sql += line;
        if ( sql[sql.size()-1] == ';') {
          fSqlStmts.push_back(sql);
          ++fNumStmts;
          sql = "";
        }
      }
    }

  }

  if ( state != kLOOKING_FOR_HEADER
     ) { Report("Unexpected EOF",lineNum,"EOF"); }
  this->Reset();
  return kFALSE;

}


string FairDbSqlValPacket::GetStmt(UInt_t stmtNo) const
{
  if ( stmtNo >= this->GetNumSqlStmts() ) { return ""; }
  std::list<std::string>::const_iterator itr = fSqlStmts.begin();
  while ( stmtNo ) { ++itr; --stmtNo; }
  return *itr;
}

std::vector<std::string> FairDbSqlValPacket::GetStmtValues(UInt_t stmtNo) const
{
  std::vector<std::string> vec;
  if ( stmtNo >= this->GetNumSqlStmts() ) { return vec; }

  std::string str = this->GetStmt(stmtNo);
  std::string::size_type pos = str.find('(');
  ++pos;
  std::string::size_type n = str.find(')') - pos;
  FairUtilString::StringTok(vec,str.substr(pos,n),",");

  return vec;
}



Bool_t FairDbSqlValPacket::IsEqual(const FairDbSqlValPacket& that,
                                   Bool_t log,
                                   const Char_t* thisName,
                                   const Char_t* thatName ) const
{

  if (    fSeqNo           != that.fSeqNo
          || fTableName       != that.fTableName
          || fNumStmts != that.fNumStmts ) {
    if ( log ) {
      DBLOG("FairDb",FairDbLog::kInfo) << "Conflict found:"
                                       << " for : " << thisName << " , " << thatName << " :-\n"
                                       << "  SeqNo " << fSeqNo << "," << that.fSeqNo
                                       << "\n  TableName " << fTableName << "," << that.fTableName
                                       << "\n  Size " << fNumStmts << ","
                                       << that.fNumStmts << endl;
    }
    return kFALSE;
  }

  list<string>::const_iterator itrThisBegin = fSqlStmts.begin();
  list<string>::const_iterator itrThisEnd   = fSqlStmts.end();
  list<string>::const_iterator itrThatBegin = that.fSqlStmts.begin();
  list<string>::const_iterator itrThatEnd   = that.fSqlStmts.end();

  list<string>::const_iterator itrThis = itrThisBegin;
  list<string>::const_iterator itrThat = itrThatBegin;

  Bool_t isEqual = kTRUE;

  // Strip off InsertDate from first statement (assume its
  // the last parameter in list).

  string strThis = (*itrThis).substr(0,(*itrThis).rfind(','));
  string strThat = (*itrThat).substr(0,(*itrThat).rfind(','));
  if ( strThis != strThat ) {
    if ( ! log ) { return kFALSE; }
    isEqual = kFALSE;

    DBLOG("FairDb",FairDbLog::kInfo)    << "Difference on VAL record " << ":-\n"
                                        << "  " << thisName << ": " << strThis  << endl
                                        << "  " << thatName << ": " << strThat  << endl;
  }

  // Rows can come in any order (after the first) so we have
  // to sort before comparing.  However, if we are lucky, they
  // may be in the same order or inverse order so do a quick
  // test to see if there are no conflicts assuming these
  // relative orderings.

  ++itrThis;
  ++itrThat;
  while ( itrThis != itrThisEnd && (*itrThis) == (*itrThat) ) {
//     cout << "Debug: trying forward compare ..." << *itrThis
//   << "::" << *itrThat << endl;
    ++itrThis;
    ++itrThat;
  }
  if ( itrThis == itrThisEnd ) { return isEqual; }

  itrThis = itrThisBegin;
  itrThat = itrThatEnd;
  ++itrThis;
  --itrThat;

  while ( itrThis != itrThisEnd &&  (*itrThis) == (*itrThat) ) {
//   cout << "Debug: trying reverse compare ..." << *itrThis
//   << "::" << *itrThat << endl;
    ++itrThis;
    --itrThat;
  }
  if ( itrThis == itrThisEnd ) { return isEqual; }

  // O.K., we are out of luck so set up pointers to both sets
  // and sort these.

  typedef vector<const string*>      shadow_list_t;
  typedef shadow_list_t::iterator  shadow_list_itr_t;

  shadow_list_t shadowThis;
  shadow_list_t shadowThat;

  itrThis = itrThisBegin;
  itrThat = itrThatBegin;
  ++itrThis;
  ++itrThat;

  while ( itrThis != itrThisEnd ) {
    shadowThis.push_back(&(*itrThis));
    shadowThat.push_back(&(*itrThat));
    ++itrThis;
    ++itrThat;
  }

  shadow_list_itr_t shadowThisItr    = shadowThis.begin();
  shadow_list_itr_t shadowThisItrEnd = shadowThis.end();
  shadow_list_itr_t shadowThatItr    = shadowThat.begin();
  shadow_list_itr_t shadowThatItrEnd = shadowThat.end();

  sort(shadowThisItr,shadowThisItrEnd,compStringPtrs);
  sort(shadowThatItr,shadowThatItrEnd,compStringPtrs);

  while ( shadowThisItr != shadowThisItrEnd ) {
    if ( (**shadowThisItr) != (**shadowThatItr) ) {
      if ( ! log ) { return kFALSE; }
      isEqual = kFALSE;
      DBLOG("FairDb",FairDbLog::kInfo) << "Difference on data record "
                                       << ":-\n"
                                       << "  " << thisName << ": " << **shadowThisItr  << endl
                                       << "  " << thatName << ": " << **shadowThatItr  << endl;
    }
    ++shadowThisItr;
    ++shadowThatItr;
  }

  return isEqual;

}

//.....................................................................

void FairDbSqlValPacket::Print(Option_t* /* option */) const
{


  DBLOG("FairDb",FairDbLog::kInfo) << "FairDbSQLValPacket:"
                                   << " table \"" << fTableName << "\" "
                                   << " SeqNo " << fSeqNo
                                   << " NumErrors " << fNumErrors
                                   << endl
                                   << "   CreationDate " << fCreationDate
                                   << endl;


  DBLOG("FairDb",FairDbLog::kInfo) << "   MySQL Main table creation: \"" << fSqlMySqlMetaMain << "\"" << endl;


  DBLOG("FairDb",FairDbLog::kInfo)  << "   MySQL VAL table creation: \"" << fSqlMySqlMetaVal << "\"" << endl;

  if ( GetNumSqlStmts() > 0 ) {
    std::list<string>::const_iterator itr    = fSqlStmts.begin();
    std::list<string>::const_iterator itrEnd = fSqlStmts.end();
    for (; itr != itrEnd; ++itr)

    {
      DBLOG("FairDb",FairDbLog::kInfo)  << "   SqlStmt \"" << *itr << "\"" << endl;
    }
  } else {
    DBLOG("FairDb",FairDbLog::kInfo) << "   No SqlStmts." << endl;
  }

}


//.....................................................................

void FairDbSqlValPacket::Recreate(const string& tableName,
                                  const ValRange& vr,
                                  Int_t aggNo,
                                  FairDb::Version task,             /*  Default: 0 */
                                  ValTimeStamp creationDate   /*  Default: now */
                                 )
{


  this->Clear();

  FairDbTableProxyRegistry& tablePR = FairDbTableProxyRegistry::Instance();
  if ( ! tablePR.GetMultConnector().TableExists(tableName) ) {
    MAXDBLOG("FairDb",FairDbLog::kError,20)
        << "Cannot create packet - table " << tableName
        << " does not exist." << endl;
    fNumErrors = 1;
    return;
  }

  fTableName = tableName;


  // Create a FairDbValidityRec from the supplied data.
  FairDbValidityRec vrec(vr,task,aggNo,0);

  //  Create a FairDbOutRowStream that can serialise this validity record
  FairDbConfigSet dummy;     // For validity row any FairDbTableRow will do.
  const FairDbTableMetaData&  metaValid = tablePR.GetTableProxy(tableName,&dummy)
                                          .GetMetaValid();
  FairDbOutRowStream buff(&metaValid);

  vrec.Store(buff,0);
  this->AddRow(buff.GetCSV());
  this->SetCreationDate(creationDate);

}


void FairDbSqlValPacket::Report(const char* msg,
                                UInt_t lineNum,
                                const string& line)
{
  MAXDBLOG("FairDb",FairDbLog::kError,20)  << msg << " on line " << lineNum
      <<":- \n  " << line << endl;
  this->Reset();
  ++fNumErrors;
}


void FairDbSqlValPacket::Reset()
{
  fSeqNo       = 0;
  fSqlMySqlMetaMain = "";
  fSqlMySqlMetaVal  = "";
  fSqlStmts.clear();
  fNumStmts    = 0;
  fTableName   = "";

}

void FairDbSqlValPacket::SetCreationDate(ValTimeStamp ts)
{

  fCreationDate = ts;
  if ( this->GetNumSqlStmts() == 0 ) { return; }
  string& vldRow = *fSqlStmts.begin();
  string::size_type locEnd = vldRow.rfind(',');
  if ( locEnd == string::npos ) { return; }
  locEnd -=2;
  string::size_type locStart = vldRow.rfind(',',locEnd);
  if ( locStart == string::npos ) { return; }
  locStart+=2;
  vldRow.replace(locStart,locEnd-locStart+1,ts.AsString("s"));
}

void FairDbSqlValPacket::SetMetaData() const
{
  FairDbTableProxyRegistry& tbprxreg = FairDbTableProxyRegistry::Instance();
  //  Locate the table in the connected list.
  FairDbMultConnector& cas = tbprxreg.GetMultConnector();
  Int_t dbNo = cas.GetTableDbNo(this->GetTableName());
  if ( dbNo < 0 ) { return; }
  FairDbConfigSet dummy;
  const FairDbTableMetaData& metaVal =  tbprxreg.GetTableProxy(this->GetTableName(),&dummy)
                                        .GetMetaValid();
  const FairDbTableMetaData& metaMain = tbprxreg.GetTableProxy(this->GetTableName(),&dummy)
                                        .GetMetaData();
  fSqlMySqlMetaVal   = metaVal.Sql();
  fSqlMySqlMetaMain  = metaMain.Sql();
}


void FairDbSqlValPacket::SetSeqNo(UInt_t seqno)
{
  fSeqNo = seqno;
  if ( this->GetNumSqlStmts() == 0 ) { return; }

  ostringstream tmp;
  tmp << seqno;
  const string seqnoStr = tmp.str();

  std::list<string>::iterator itr    = fSqlStmts.begin();
  std::list<string>::iterator itrEnd = fSqlStmts.end();
  for (; itr != itrEnd; ++itr) { SetSeqNoOnRow(*itr,seqnoStr); }

}


void FairDbSqlValPacket::SetSeqNoOnRow(string& row,const string& seqno)
{

  string::size_type locStart = row.find('(');
  if ( locStart == string::npos ) { return; }
  ++locStart;
  string::size_type locEnd = row.find(',',locStart);
  if ( locEnd == string::npos ) { return; }
  row.replace(locStart,locEnd-locStart,seqno);

}


Bool_t FairDbSqlValPacket::Store(UInt_t dbNo, Bool_t replace) const
{
  //cout << "-I- FairDbSqlValPacket::Store() " << endl;

  if ( ! CanBeStored() ) { return kFALSE; }

  FairDbConfigSet pet;
  FairDbTableProxy& tp =  FairDbTableProxyRegistry::Instance()
                          .GetTableProxy(this->GetTableName(),&pet);
  if ( replace ) {
    const FairDbProxy& proxy = tp.GetDBProxy();
    if ( ! proxy.RemoveSeqNo(this->GetSeqNo(),dbNo) ) { return kFALSE; }
  }


  auto_ptr<FairDbStatement> stmtDb(FairDbTableProxyRegistry::Instance()
                                   .GetMultConnector()
                                   .CreateStatement(dbNo));
  if ( ! stmtDb.get() ) {

    DBLOG("FairDb",FairDbLog::kInfo)
        << "Attempting to write to non-existant cascade entry " << dbNo
        << endl;
    return kFALSE;
  }

  // Decide if ROW_COUNTER column has to be removed i.e. Master
  // DB exported with ROW_COUNTER but Slave still has old table form
  // without ROW_COUNTER.

  bool removeRowCounter =    fgLastMetaHadRowCounter
                             && ! tp.GetMetaData().HasRowCounter();

  // Loop processing all SQL statements
  Bool_t first = kTRUE;
  int combineInserts = 0;
  // Cannot group ORACLE inserts.
  int maxInserts = stmtDb->GetDBType() == FairDb::kOracle ? 1 : 20;
  string sqlInserts;

  for (list<string>::const_iterator itr = fSqlStmts.begin();
       itr != fSqlStmts.end();
       ++itr) {
    if ( first ) {
//    On first statement replace InsertDate by current datetime.
      string sql = *itr;
      list<string>::size_type locDate = sql.rfind(",\'");
      if ( locDate !=  string::npos ) {
        ValTimeStamp now;
        sql.replace(locDate+2,19,FairDb::MakeDateTimeString(now));
      }
      //cout << "-I- FairDbSqlValPacket::Store() exxecute SQL:" << sql <<  endl;
      stmtDb->ExecuteUpdate(sql.c_str());
      if ( stmtDb->PrintExceptions() ) { return kFALSE; }
      first = kFALSE;
      continue;
    }

    string sql = *itr;


    if (removeRowCounter) {
      list<string>::size_type locStart  = sql.find(',');
      ++locStart;
      list<string>::size_type locEnd    = sql.find(',',locStart);
      if ( locEnd !=  string::npos ) {
        list<string>::size_type numChars = locEnd - locStart + 1;
        sql.erase(locStart,numChars);
      }
    }

//  Reduce database I/O by combining groups of insert commands.

    string::size_type insertIndex = sql.find("VALUES (");
    if ( insertIndex == string::npos) {
      MAXDBLOG("FairDb",FairDbLog::kError,20)  << "Unexpected SQL : " << sql
          << "\n  should be of form INSERT INTO ... VALUES (...);" << endl;
      return kFALSE;
    }
    ++combineInserts;
    if ( combineInserts == 1 ) { sqlInserts = sql; }
    else {
      sqlInserts[sqlInserts.size()-1] = ',';
      sqlInserts += sql.substr(insertIndex+7);
    }
    if ( combineInserts >= maxInserts ) {
      stmtDb->ExecuteUpdate(sqlInserts.c_str());
      if ( stmtDb->PrintExceptions() ) { return kFALSE; }
      combineInserts = 0;
    }
  }

// Deal with last group of inserts.
  if ( combineInserts ) {
    stmtDb->ExecuteUpdate(sqlInserts.c_str());
    combineInserts = 0;
    if ( stmtDb->PrintExceptions() ) { return kFALSE; }
  }

  //cout << "-I- FairDbSqlValPacket::Store(): done ...  " << endl;
  return kTRUE;

}

Bool_t FairDbSqlValPacket::Write(std::ofstream& ios,
                                 Bool_t addMetadata) const
{


  if ( ! CanBeStored() ) { return kFALSE; }
  if ( addMetadata ) {
    if ( fSqlMySqlMetaMain.size() == 0 ) { this->SetMetaData(); }
    if ( fSqlMySqlMetaMain.size() == 0 ) {

      DBLOG("FairDb",FairDbLog::kWarning)<< "Cannot write metadata; no associated FairDbTableProxy "
                                         << endl;
    } else {
      ios << ">>>>>" << GetTableName() << " Metadata [MySQL]" << endl;
      ios << fSqlMySqlMetaVal   << endl;
      ios << fSqlMySqlMetaMain  << endl;
      ios << "<<<<<" << GetTableName() << " Metadata" << endl;
    }
  }

  ios << ">>>>>" << GetTableName() << "  " << GetSeqNo() << endl;

  for ( list<string>::const_iterator itr = fSqlStmts.begin();
        itr != fSqlStmts.end();
        ++itr) { ios << (*itr) << endl; }

  ios << "<<<<<" << GetTableName() << "  " << GetSeqNo() << endl;

  return kTRUE;

}

