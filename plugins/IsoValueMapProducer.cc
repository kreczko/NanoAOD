// -*- C++ -*-
//
// Package:    PhysicsTools/NanoAOD
// Class:      IsoValueMapProducer
// 
/**\class IsoValueMapProducer IsoValueMapProducer.cc PhysicsTools/NanoAOD/plugins/IsoValueMapProducer.cc

 Description: [one line class summary]

 Implementation:
     [Notes on implementation]
*/
//
// Original Author:  Marco Peruzzi
//         Created:  Mon, 04 Sep 2017 22:43:53 GMT
//
//


// system include files
#include <memory>

// user include files
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/stream/EDProducer.h"

#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"

#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Utilities/interface/StreamID.h"

#include "RecoEgamma/EgammaTools/interface/EffectiveAreas.h"

#include "DataFormats/PatCandidates/interface/Muon.h"
#include "DataFormats/PatCandidates/interface/Electron.h"
#include "DataFormats/PatCandidates/interface/Photon.h"

//
// class declaration
//

template <typename T>
class IsoValueMapProducer : public edm::stream::EDProducer<> {
   public:
  explicit IsoValueMapProducer(const edm::ParameterSet &iConfig):
    src_(consumes<edm::View<T>>(iConfig.getParameter<edm::InputTag>("src"))),
    rho_(consumes<double>(iConfig.getParameter<edm::InputTag>("rho")))
  {
    if ((typeid(T) == typeid(pat::Muon)) || (typeid(T) == typeid(pat::Electron))) {
      produces<edm::ValueMap<float>>("miniIsoChg");
      produces<edm::ValueMap<float>>("miniIsoAll");
      ea_miniiso_.reset(new EffectiveAreas((iConfig.getParameter<edm::FileInPath>("EAFile_MiniIso")).fullPath()));
    }
    if ((typeid(T) == typeid(pat::Electron))) {
      produces<edm::ValueMap<float>>("PFIsoChg");
      produces<edm::ValueMap<float>>("PFIsoAll");
      ea_pfiso_.reset(new EffectiveAreas((iConfig.getParameter<edm::FileInPath>("EAFile_PFIso")).fullPath()));
    }
    if ((typeid(T) == typeid(pat::Photon))) {
      produces<edm::ValueMap<float>>("PFIsoChg");
      produces<edm::ValueMap<float>>("PFIsoAll");
      mapIsoChg_ = consumes<edm::ValueMap<float> >(iConfig.getParameter<edm::InputTag>("mapIsoChg"));
      mapIsoNeu_ = consumes<edm::ValueMap<float> >(iConfig.getParameter<edm::InputTag>("mapIsoNeu"));
      mapIsoPho_ = consumes<edm::ValueMap<float> >(iConfig.getParameter<edm::InputTag>("mapIsoPho"));
      ea_pfiso_chg_.reset(new EffectiveAreas((iConfig.getParameter<edm::FileInPath>("EAFile_PFIso_Chg")).fullPath()));
      ea_pfiso_neu_.reset(new EffectiveAreas((iConfig.getParameter<edm::FileInPath>("EAFile_PFIso_Neu")).fullPath()));
      ea_pfiso_pho_.reset(new EffectiveAreas((iConfig.getParameter<edm::FileInPath>("EAFile_PFIso_Pho")).fullPath()));
    }
  }
  ~IsoValueMapProducer() {}
  
      static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);

   private:
  virtual void beginStream(edm::StreamID) override {};
  virtual void produce(edm::Event&, const edm::EventSetup&) override;
  virtual void endStream() override {};

      //virtual void beginRun(edm::Run const&, edm::EventSetup const&) override;
      //virtual void endRun(edm::Run const&, edm::EventSetup const&) override;
      //virtual void beginLuminosityBlock(edm::LuminosityBlock const&, edm::EventSetup const&) override;
      //virtual void endLuminosityBlock(edm::LuminosityBlock const&, edm::EventSetup const&) override;

      // ----------member data ---------------------------

  edm::EDGetTokenT<edm::View<T>> src_;
  edm::EDGetTokenT<double> rho_;
  edm::EDGetTokenT<edm::ValueMap<float>> mapIsoChg_;
  edm::EDGetTokenT<edm::ValueMap<float>> mapIsoNeu_;
  edm::EDGetTokenT<edm::ValueMap<float>> mapIsoPho_;
  std::unique_ptr<EffectiveAreas> ea_miniiso_;
  std::unique_ptr<EffectiveAreas> ea_pfiso_;
  std::unique_ptr<EffectiveAreas> ea_pfiso_chg_;
  std::unique_ptr<EffectiveAreas> ea_pfiso_neu_;
  std::unique_ptr<EffectiveAreas> ea_pfiso_pho_;
  float getEtaForEA(const T*) const;
  void doMiniIso(edm::Event&);
  void doPFIsoEle(edm::Event&);
  void doPFIsoPho(edm::Event&);

};

//
// constants, enums and typedefs
//


//
// static data member definitions
//

template<typename T> float IsoValueMapProducer<T>::getEtaForEA(const T *obj) const{
  return obj->eta();
}
template<> float IsoValueMapProducer<pat::Electron>::getEtaForEA(const pat::Electron *el) const{
  return el->superCluster()->eta();
}
template<> float IsoValueMapProducer<pat::Photon>::getEtaForEA(const pat::Photon *ph) const{
  return ph->superCluster()->eta();
}

template <typename T>
void
IsoValueMapProducer<T>::produce(edm::Event& iEvent, const edm::EventSetup& iSetup)
{

  if ((typeid(T) == typeid(pat::Muon)) || (typeid(T) == typeid(pat::Electron))) { doMiniIso(iEvent); };
  if ((typeid(T) == typeid(pat::Electron))) { doPFIsoEle(iEvent); }
  if ((typeid(T) == typeid(pat::Photon))) { doPFIsoPho(iEvent); }

}

template<typename T>
void
IsoValueMapProducer<T>::doMiniIso(edm::Event& iEvent){

  edm::Handle<edm::View<T>> src;
  iEvent.getByToken(src_, src);
  edm::Handle<double> rho;
  iEvent.getByToken(rho_,rho);
  
  std::vector<float> miniIsoChg, miniIsoAll;
  miniIsoChg.reserve(src->size());
  miniIsoAll.reserve(src->size());
  
  for (uint i=0; i<src->size(); i++){
    auto obj = const_cast<T*>(src->ptrAt(i).get()); // temporarily needed
    auto iso = obj->miniPFIsolation();
    auto chg = iso.chargedHadronIso();
    auto neu = iso.neutralHadronIso();
    auto pho = iso.photonIso();
    auto ea = ea_miniiso_->getEffectiveArea(fabs(getEtaForEA(obj)));
    float R = 10.0/std::min(std::max(obj->pt(), 50.0),200.0);
    ea *= std::pow(R/0.3,2);
    miniIsoChg.push_back(chg);
    miniIsoAll.push_back(chg+std::max(0.0,neu+pho-(*rho)*ea));
  }
  
  std::unique_ptr<edm::ValueMap<float>> miniIsoChgV(new edm::ValueMap<float>());
  edm::ValueMap<float>::Filler fillerChg(*miniIsoChgV);
  fillerChg.insert(src,miniIsoChg.begin(),miniIsoChg.end());
  fillerChg.fill();
  std::unique_ptr<edm::ValueMap<float>> miniIsoAllV(new edm::ValueMap<float>());
  edm::ValueMap<float>::Filler fillerAll(*miniIsoAllV);
  fillerAll.insert(src,miniIsoAll.begin(),miniIsoAll.end());
  fillerAll.fill();

  iEvent.put(std::move(miniIsoChgV),"miniIsoChg");
  iEvent.put(std::move(miniIsoAllV),"miniIsoAll");
}

template<>
void
IsoValueMapProducer<pat::Photon>::doMiniIso(edm::Event& iEvent) {}


template<typename T>
void
IsoValueMapProducer<T>::doPFIsoEle(edm::Event& iEvent) {}

template<>
void
IsoValueMapProducer<pat::Electron>::doPFIsoEle(edm::Event& iEvent){

  edm::Handle<edm::View<pat::Electron>> src;
  iEvent.getByToken(src_, src);
  edm::Handle<double> rho;
  iEvent.getByToken(rho_,rho);
  
  std::vector<float> PFIsoChg, PFIsoAll;
  PFIsoChg.reserve(src->size());
  PFIsoAll.reserve(src->size());
  
  for (uint i=0; i<src->size(); i++){
    auto obj = src->ptrAt(i).get();
    auto iso = obj->pfIsolationVariables();
    auto chg = iso.sumChargedHadronPt;
    auto neu = iso.sumNeutralHadronEt;
    auto pho = iso.sumPhotonEt;
    auto ea = ea_pfiso_->getEffectiveArea(fabs(getEtaForEA(obj)));
    PFIsoChg.push_back(chg);
    PFIsoAll.push_back(chg+std::max(0.0,neu+pho-(*rho)*ea));
  }
  
  std::unique_ptr<edm::ValueMap<float>> PFIsoChgV(new edm::ValueMap<float>());
  edm::ValueMap<float>::Filler fillerChg(*PFIsoChgV);
  fillerChg.insert(src,PFIsoChg.begin(),PFIsoChg.end());
  fillerChg.fill();
  std::unique_ptr<edm::ValueMap<float>> PFIsoAllV(new edm::ValueMap<float>());
  edm::ValueMap<float>::Filler fillerAll(*PFIsoAllV);
  fillerAll.insert(src,PFIsoAll.begin(),PFIsoAll.end());
  fillerAll.fill();

  iEvent.put(std::move(PFIsoChgV),"PFIsoChg");
  iEvent.put(std::move(PFIsoAllV),"PFIsoAll");

}

template<typename T>
void
IsoValueMapProducer<T>::doPFIsoPho(edm::Event& iEvent) {}

template<>
void
IsoValueMapProducer<pat::Photon>::doPFIsoPho(edm::Event& iEvent){

  edm::Handle<edm::View<pat::Photon>> src;
  iEvent.getByToken(src_, src);
  edm::Handle<double> rho;
  iEvent.getByToken(rho_,rho);
  edm::Handle<edm::ValueMap<float> > mapIsoChg;
  iEvent.getByToken(mapIsoChg_, mapIsoChg);
  edm::Handle<edm::ValueMap<float> > mapIsoNeu;
  iEvent.getByToken(mapIsoNeu_, mapIsoNeu);
  edm::Handle<edm::ValueMap<float> > mapIsoPho;
  iEvent.getByToken(mapIsoPho_, mapIsoPho);
  
  std::vector<float> PFIsoChg, PFIsoAll;
  PFIsoChg.reserve(src->size());
  PFIsoAll.reserve(src->size());
  
  for (uint i=0; i<src->size(); i++){
    auto obj = src->ptrAt(i);
    auto chg = (*mapIsoChg)[obj];
    auto neu = (*mapIsoNeu)[obj];
    auto pho = (*mapIsoPho)[obj];
    auto ea_chg = ea_pfiso_chg_->getEffectiveArea(fabs(getEtaForEA(obj.get())));
    auto ea_neu = ea_pfiso_neu_->getEffectiveArea(fabs(getEtaForEA(obj.get())));
    auto ea_pho = ea_pfiso_pho_->getEffectiveArea(fabs(getEtaForEA(obj.get())));
    PFIsoChg.push_back(std::max(0.0,chg-(*rho)*ea_chg));
    PFIsoAll.push_back(PFIsoChg.back()+std::max(0.0,neu-(*rho)*ea_neu)+std::max(0.0,pho-(*rho)*ea_pho));
  }
  
  std::unique_ptr<edm::ValueMap<float>> PFIsoChgV(new edm::ValueMap<float>());
  edm::ValueMap<float>::Filler fillerChg(*PFIsoChgV);
  fillerChg.insert(src,PFIsoChg.begin(),PFIsoChg.end());
  fillerChg.fill();
  std::unique_ptr<edm::ValueMap<float>> PFIsoAllV(new edm::ValueMap<float>());
  edm::ValueMap<float>::Filler fillerAll(*PFIsoAllV);
  fillerAll.insert(src,PFIsoAll.begin(),PFIsoAll.end());
  fillerAll.fill();

  iEvent.put(std::move(PFIsoChgV),"PFIsoChg");
  iEvent.put(std::move(PFIsoAllV),"PFIsoAll");

}



// ------------ method fills 'descriptions' with the allowed parameters for the module  ------------
template <typename T>
void
IsoValueMapProducer<T>::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  //The following says we do not know what parameters are allowed so do no validation
  // Please change this to state exactly what you do use, even if it is no parameters
  edm::ParameterSetDescription desc;
  desc.setUnknown();
  descriptions.addDefault(desc);
}


typedef IsoValueMapProducer<pat::Muon> MuonIsoValueMapProducer;
typedef IsoValueMapProducer<pat::Electron> EleIsoValueMapProducer;
typedef IsoValueMapProducer<pat::Photon> PhoIsoValueMapProducer;

//define this as a plug-in
DEFINE_FWK_MODULE(MuonIsoValueMapProducer);
DEFINE_FWK_MODULE(EleIsoValueMapProducer);
DEFINE_FWK_MODULE(PhoIsoValueMapProducer);

