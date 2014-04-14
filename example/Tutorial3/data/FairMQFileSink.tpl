/* 
 * File:   FairMQFileSink.tpl
 * Author: winckler, A. Rybalchenko
 *
 * Created on March 11, 2014, 12:12 PM
 */




template <typename TIn, typename TPayloadIn>
FairMQFileSink<TIn,TPayloadIn>::FairMQFileSink()
{
    
    fHasBoostSerialization=true;
    #if __cplusplus >= 201103L
    fHasBoostSerialization=false;
    if(   std::is_same<TPayloadIn,boost::archive::binary_iarchive>::value || std::is_same<TPayloadIn,boost::archive::text_iarchive>::value)
    {
        if(has_BoostSerialization<TIn, void(TPayloadIn &, const unsigned int)>::value ==1) 
                fHasBoostSerialization=true;
    }
    #endif
    
}


template <typename TIn, typename TPayloadIn>
FairMQFileSink<TIn,TPayloadIn>::~FairMQFileSink()
{
    fTree->Write();
    fOutFile->Close();
    if(fHitVector.size()>0) fHitVector.clear();
}


template <typename TIn, typename TPayloadIn>
void FairMQFileSink<TIn,TPayloadIn>::InitOutputFile(TString defaultId)
{
    fOutput = new TClonesArray("FairTestDetectorHit");
    char out[256];
    sprintf(out, "filesink%s.root", defaultId.Data());

    fOutFile = new TFile(out,"recreate");
    fTree = new TTree("MQOut", "Test output");
    fTree->Branch("Output","TClonesArray", &fOutput, 64000, 99);
}



template <typename TIn, typename TPayloadIn>
void FairMQFileSink<TIn,TPayloadIn>::Run()
{
  
    if(fHasBoostSerialization)
    {
        LOG(INFO) << ">>>>>>> Run <<<<<<<";

        boost::thread rateLogger(boost::bind(&FairMQDevice::LogSocketRates, this));
        int receivedMsgs = 0;
        bool received = false;


        while ( fState == RUNNING ) 
        {
            FairMQMessage* msg = fTransportFactory->CreateMessage();
            received = fPayloadInputs->at(0)->Receive(msg);

            if (received) 
            {
                receivedMsgs++;
                std::string msgStr( static_cast<char*>(msg->GetData()), msg->GetSize() );
                std::istringstream ibuffer(msgStr);
                TPayloadIn InputArchive(ibuffer);

                try
                {
                    InputArchive >> fHitVector;
                }
                catch (boost::archive::archive_exception e)
                {
                    LOG(ERROR) << e.what();
                }

                int numInput=fHitVector.size();
                fOutput->Delete();

                for (Int_t i = 0; i < numInput; ++i) 
                {
                  new ((*fOutput)[i]) TIn(fHitVector.at(i));
                }

                if (!fOutput) 
                {
                  cout << "-W- FairMQFileSink::Run: " << "No Output array!" << endl;
                }
                
                fTree->Fill();
                received = false;
            }
            delete msg;
            if(fHitVector.size()>0) 
                fHitVector.clear();
        }
        
        cout << "I've received " << receivedMsgs << " messages!" << endl;
        boost::this_thread::sleep(boost::posix_time::milliseconds(5000));
        rateLogger.interrupt();
        rateLogger.join();
    }
    else
    {
        LOG(ERROR) <<" Boost Serialization not ok";
    }
}


template <>
void FairMQFileSink<FairTestDetectorHit, TestDetectorPayload::TestDetectorHit>::Run()
{
  LOG(INFO) << ">>>>>>> Run <<<<<<<";

  boost::thread rateLogger(boost::bind(&FairMQDevice::LogSocketRates, this));

  int receivedMsgs = 0;
  bool received = false;

  while ( fState == RUNNING ) {
    FairMQMessage* msg = fTransportFactory->CreateMessage();

    received = fPayloadInputs->at(0)->Receive(msg);

    if (received) {
      Int_t inputSize = msg->GetSize();
      Int_t numInput = inputSize / sizeof(TestDetectorPayload::TestDetectorHit);
      TestDetectorPayload::TestDetectorHit* input = static_cast<TestDetectorPayload::TestDetectorHit*>(msg->GetData());

      fOutput->Delete();

      for (Int_t i = 0; i < numInput; ++i) {
        TVector3 pos(input[i].posX,input[i].posY,input[i].posZ);
        TVector3 dpos(input[i].dposX,input[i].dposY,input[i].dposZ);
        new ((*fOutput)[i]) FairTestDetectorHit(input[i].detID, input[i].mcindex, pos, dpos);
      }

      if (!fOutput) {
        cout << "-W- FairMQFileSink::Run: " << "No Output array!" << endl;
      }

      fTree->Fill();
      received = false;
    }

    delete msg;
  }

  cout << "I've received " << receivedMsgs << " messages!" << endl;

  boost::this_thread::sleep(boost::posix_time::milliseconds(5000));

  rateLogger.interrupt();
  rateLogger.join();
}


#ifdef PROTOBUF

#include "FairTestDetectorPayload.pb.h"

template <>
void FairMQFileSink<FairTestDetectorHit,TestDetectorProto::HitPayload>::Run()
{
  LOG(INFO) << ">>>>>>> Run <<<<<<<";

  boost::thread rateLogger(boost::bind(&FairMQDevice::LogSocketRates, this));

  int receivedMsgs = 0;
  bool received = false;

  while ( fState == RUNNING ) {
    FairMQMessage* msg = fTransportFactory->CreateMessage();

    received = fPayloadInputs->at(0)->Receive(msg);

    if (received) {

      fOutput->Delete();

      TestDetectorProto::HitPayload hp;
      hp.ParseFromArray(msg->GetData(), msg->GetSize());

      int numEntries = hp.hit_size();

      for (int i = 0; i < numEntries; ++i) {
        const TestDetectorProto::Hit& hit = hp.hit(i);
        TVector3 pos(hit.posx(), hit.posy(), hit.posz());
        TVector3 dpos(hit.dposx(), hit.dposy(), hit.dposz());
        new ((*fOutput)[i]) FairTestDetectorHit(hit.detid(), hit.mcindex(), pos, dpos);
      }

      if (!fOutput) {
        cout << "-W- FairMQFileSink::Run: " << "No Output array!" << endl;
      }

      fTree->Fill();
      received = false;
    }

    delete msg;
  }

  cout << "I've received " << receivedMsgs << " messages!" << endl;

  boost::this_thread::sleep(boost::posix_time::milliseconds(5000));

  rateLogger.interrupt();
  rateLogger.join();
}

#endif /* PROTOBUF */

