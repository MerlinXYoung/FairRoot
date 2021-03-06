/********************************************************************************
 *    Copyright (C) 2014 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *         GNU Lesser General Public Licence version 3 (LGPL) version 3,        *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/
//
//  PixelDigiSource.h
// 
//
//  Created by Radoslaw Karabowicz on 19.02.2016Mohammad Al-Turany on 08/02/14.
//
//

#ifndef PIXELDIGISOURCE_H_
#define PIXELDIGISOURCE_H_

#include "FairSource.h"

#include "TString.h"

#include <list>  
#include <fstream>
  
class TClonesArray;

class FairLogger;
class FairRuntimeDb;
class PixelEventHeader;

class PixelDigiSource : public FairSource
{
public:
  PixelDigiSource(TString inputFileName="test.dat");
  virtual ~PixelDigiSource();
  
  Bool_t              Init();

  Int_t               ReadEvent(UInt_t i=0);
  void                Close();
  void                Reset();
  
  virtual Source_Type GetSourceType() { return kFILE; }

  virtual void SetParUnpackers() {}

  virtual Bool_t InitUnpackers() { return kTRUE; }

  virtual Bool_t ReInitUnpackers() { return kTRUE; }

  /**Check the maximum event number we can run to*/
  virtual Int_t  CheckMaxEventNo(Int_t EvtEnd=0);

  virtual void FillEventHeader(FairEventHeader* feh);
    
  void SetInputFileName(TString tstr) {fInputFileName = tstr;};

  virtual Bool_t  ActivateObject(TObject** obj, const char* BrName);

private:
  PixelEventHeader* fEventHeader;
  TClonesArray*     fDigis;        /** Output array of PixelDigi **/
  Int_t             fNDigis;
  
  Int_t             fTNofEvents;
  Int_t             fTNofDigis;

  TString  fInputFileName;
  std::ifstream fInputFile;

  Int_t    fCurrentEntryNo;

  Int_t    fRunId;
  Int_t    fMCEntryNo;
  Int_t    fPartNo;

  PixelDigiSource(const PixelDigiSource&);
  PixelDigiSource& operator=(const PixelDigiSource&);

  ClassDef(PixelDigiSource, 1)
};


#endif /* defined(PIXELDIGISOURCE_H_) */
