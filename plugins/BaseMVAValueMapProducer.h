#ifndef PhysicsTools_NanoAOD_BaseMVAValueMapProducer
#define PhysicsTools_NanoAOD_BaseMVAValueMapProducer

// -*- C++ -*-
//
// Package:    PhysicsTools/NanoAOD
// Class:      BaseMVAValueMapProducer
// 
/**\class BaseMVAValueMapProducer BaseMVAValueMapProducer.cc PhysicsTools/NanoAOD/plugins/BaseMVAValueMapProducer.cc

 Description: [one line class summary]

 Implementation:
     [Notes on implementation]
*/
//
// Original Author:  Andre Rizzi
//         Created:  Mon, 07 Sep 2017 09:18:03 GMT
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


#include "TMVA/Factory.h"
#include "TMVA/Reader.h"

#include "CommonTools/Utils/interface/StringObjectFunction.h"
#include "DataFormats/Common/interface/ValueMap.h"
#include <string>
//
// class declaration
//

template <typename T>
class BaseMVAValueMapProducer : public edm::stream::EDProducer<> {
   public:
  explicit BaseMVAValueMapProducer(const edm::ParameterSet &iConfig):
    src_(consumes<edm::View<T>>(iConfig.getParameter<edm::InputTag>("src"))),
    variablesOrder_(iConfig.getParameter<std::vector<std::string>>("variablesOrder")),
    name_(iConfig.getParameter<std::string>("name"))
  {
      edm::ParameterSet const & varsPSet = iConfig.getParameter<edm::ParameterSet>("variables");
      for (const std::string & vname : varsPSet.getParameterNamesForType<std::string>()) {
      	  std::cout << vname << std::endl;
	  funcs_.emplace_back(std::pair<std::string,StringObjectFunction<T>>(vname,varsPSet.getParameter<std::string>(vname)));
      }

      values_.resize(variablesOrder_.size());
      size_t i=0;
      for(const auto & v : variablesOrder_){
	positions_[v]=i;
	reader_.AddVariable(v,(&values_.front())+i);
	i++;
      }
      reader_.BookMVA(name_,iConfig.getParameter<edm::FileInPath>("weightFile").fullPath() );
      produces<edm::ValueMap<float>>();

  }
  ~BaseMVAValueMapProducer() {}

  void setValue(const std::string var,float val) {
	values_[positions_[var]]=val;
  }
  
  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);

   private:
  virtual void beginStream(edm::StreamID) override {};
  virtual void produce(edm::Event&, const edm::EventSetup&) override;
  virtual void endStream() override {};

  ///to be implemented in derived classes, filling values for additional variables
  virtual void readAdditionalCollections(edm::Event&, const edm::EventSetup&)  {}
  virtual void fillAdditionalVariables(const T&)  {}


  edm::EDGetTokenT<edm::View<T>> src_;
  std::map<std::string,size_t> positions_;
  std::vector<std::pair<std::string,StringObjectFunction<T>>> funcs_;
  std::vector<std::string> variablesOrder_;
  std::vector<float> values_;
  TMVA::Reader reader_;
  std::string name_;

};

template <typename T>
void
BaseMVAValueMapProducer<T>::produce(edm::Event& iEvent, const edm::EventSetup& iSetup)
{
  edm::Handle<edm::View<T>> src;
  iEvent.getByToken(src_, src);
  readAdditionalCollections(iEvent,iSetup);
  
  std::vector<float> mvaOut;
  mvaOut.reserve(src->size());
  for(auto const & o: *src) {
	for(auto const & p : funcs_ ){
		values_[positions_[p.first]]=p.second(o);
	}
        fillAdditionalVariables(o);
	mvaOut.push_back(reader_.EvaluateRegression(name_)[0]);
  }
  std::unique_ptr<edm::ValueMap<float>> mvaV(new edm::ValueMap<float>());
  edm::ValueMap<float>::Filler filler(*mvaV);
  filler.insert(src,mvaOut.begin(),mvaOut.end());
  filler.fill();
  iEvent.put(std::move(mvaV));

}

template <typename T>
void
BaseMVAValueMapProducer<T>::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  //The following says we do not know what parameters are allowed so do no validation
  // Please change this to state exactly what you do use, even if it is no parameters
  edm::ParameterSetDescription desc;
  desc.setUnknown();
  descriptions.addDefault(desc);
}



#endif
