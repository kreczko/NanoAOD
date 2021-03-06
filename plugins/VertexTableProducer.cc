// -*- C++ -*-
//
// Package:    PhysicsTools/NanoAOD
// Class:      VertexTableProducer
// 
/**\class VertexTableProducer VertexTableProducer.cc PhysicsTools/VertexTableProducer/plugins/VertexTableProducer.cc

 Description: [one line class summary]

 Implementation:
     [Notes on implementation]
*/
//
// Original Author:  Andrea Rizzi
//         Created:  Mon, 28 Aug 2017 09:26:39 GMT
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

#include "DataFormats/VertexReco/interface/Vertex.h"
#include "DataFormats/Candidate/interface/VertexCompositePtrCandidate.h"

#include "CommonTools/Utils/interface/StringCutObjectSelector.h"

#include "PhysicsTools/NanoAOD/interface/FlatTable.h"
#include "RecoVertex/VertexTools/interface/VertexDistance3D.h"
#include "RecoVertex/VertexPrimitives/interface/ConvertToFromReco.h"
#include "RecoVertex/VertexPrimitives/interface/VertexState.h"
#include "DataFormats/Common/interface/ValueMap.h"

//
// class declaration
//

class VertexTableProducer : public edm::stream::EDProducer<> {
   public:
      explicit VertexTableProducer(const edm::ParameterSet&);
      ~VertexTableProducer();

      static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);

   private:
      virtual void beginStream(edm::StreamID) override;
      virtual void produce(edm::Event&, const edm::EventSetup&) override;
      virtual void endStream() override;

      //virtual void beginRun(edm::Run const&, edm::EventSetup const&) override;
      //virtual void endRun(edm::Run const&, edm::EventSetup const&) override;
      //virtual void beginLuminosityBlock(edm::LuminosityBlock const&, edm::EventSetup const&) override;
      //virtual void endLuminosityBlock(edm::LuminosityBlock const&, edm::EventSetup const&) override;

      // ----------member data ---------------------------

      const edm::EDGetTokenT<std::vector<reco::Vertex>> pvs_;
      const edm::EDGetTokenT<edm::ValueMap<float>> pvsScore_;
      const edm::EDGetTokenT<edm::View<reco::VertexCompositePtrCandidate> > svs_;
      const StringCutObjectSelector<reco::Candidate> svCut_;
      const std::string  pvName_;
      const std::string  svName_;
      const std::string svDoc_;
      const double dlenMin_,dlenSigMin_;

};



//
// constructors and destructor
//
VertexTableProducer::VertexTableProducer(const edm::ParameterSet& params):
    pvs_(consumes<std::vector<reco::Vertex>>( params.getParameter<edm::InputTag>("pvSrc") )),
    pvsScore_(consumes<edm::ValueMap<float>>( params.getParameter<edm::InputTag>("pvSrc") )),
    svs_(consumes<edm::View<reco::VertexCompositePtrCandidate> >( params.getParameter<edm::InputTag>("svSrc") )),
    svCut_(params.getParameter<std::string>("svCut") , true),
    pvName_(params.getParameter<std::string>("pvName") ),
    svName_(params.getParameter<std::string>("svName") ),
    svDoc_(params.getParameter<std::string>("svDoc") ),
    dlenMin_(params.getParameter<double>("dlenMin") ),
    dlenSigMin_(params.getParameter<double>("dlenSigMin") )
   
{
   produces<FlatTable>("pv");
   produces<FlatTable>("otherPVs");
   produces<FlatTable>("svs");
   produces<edm::PtrVector<reco::Candidate> >();
 
}


VertexTableProducer::~VertexTableProducer()
{
 
   // do anything here that needs to be done at destruction time
   // (e.g. close files, deallocate resources etc.)

}


//
// member functions
//

// ------------ method called to produce the data  ------------


void
VertexTableProducer::produce(edm::Event& iEvent, const edm::EventSetup& iSetup)
{
    using namespace edm;
    edm::Handle<edm::ValueMap<float>> pvsScoreIn;
    edm::Handle<std::vector<reco::Vertex>> pvsIn;
    iEvent.getByToken(pvs_, pvsIn);
    iEvent.getByToken(pvsScore_, pvsScoreIn);
    auto pvTable = std::make_unique<FlatTable>(1,pvName_,true);
    pvTable->addColumnValue<float>("ndof",(*pvsIn)[0].ndof(),"main primary vertex number of degree of freedom",FlatTable::FloatColumn,8);
    pvTable->addColumnValue<float>("x",(*pvsIn)[0].position().x(),"main primary vertex position x coordinate",FlatTable::FloatColumn,10);
    pvTable->addColumnValue<float>("y",(*pvsIn)[0].position().y(),"main primary vertex position y coordinate",FlatTable::FloatColumn,10);
    pvTable->addColumnValue<float>("z",(*pvsIn)[0].position().z(),"main primary vertex position z coordinate",FlatTable::FloatColumn,16);
    pvTable->addColumnValue<float>("chi2",(*pvsIn)[0].normalizedChi2(),"main primary vertex reduced chi2",FlatTable::FloatColumn,8);
    pvTable->addColumnValue<int>("npvs",(*pvsIn).size(),"total number of reconstructed primary vertices",FlatTable::IntColumn);
    pvTable->addColumnValue<float>("score",(*pvsScoreIn).get(pvsIn.id(),0),"main primary vertex score, i.e. sum pt2 of clustered objects",FlatTable::FloatColumn,8);

    auto otherPVsTable = std::make_unique<FlatTable>((*pvsIn).size() >4?3:(*pvsIn).size()-1,"Other"+pvName_,false);
    std::vector<float> pvsz;
    for(size_t i=1;i < (*pvsIn).size() && i < 4; i++) pvsz.push_back((*pvsIn)[i-1].position().z());
    otherPVsTable->addColumn<float>("z",pvsz,"Z position of other primary vertices, excluding the main PV",FlatTable::FloatColumn,8);


    edm::Handle<edm::View<reco::VertexCompositePtrCandidate> > svsIn;
    iEvent.getByToken(svs_, svsIn);
    auto selCandSv = std::make_unique<PtrVector<reco::Candidate>>();
    std::vector<float> dlen,dlenSig;
    VertexDistance3D vdist;

    size_t i=0;
    for (const auto & sv : *svsIn) {
       if (svCut_(sv)) {
           Measurement1D dl= vdist.distance((*pvsIn)[0],VertexState(RecoVertex::convertPos(sv.position()),RecoVertex::convertError(sv.error())));
	   if(dl.value() > dlenMin_ and dl.significance() > dlenSigMin_){
                dlen.push_back(dl.value());	
                dlenSig.push_back(dl.significance());	
	 	edm::Ptr<reco::Candidate> c =  svsIn->ptrAt(i);
		selCandSv->push_back(c);
	   }
       }
       i++;
    }


    auto svsTable = std::make_unique<FlatTable>(selCandSv->size(),svName_,false);
    // For SV we fill from here only stuff that cannot be created with the SimpleFlatTableProducer 
    svsTable->addColumn<float>("dlen",dlen,"decay length in cm",FlatTable::FloatColumn,10);
    svsTable->addColumn<float>("dlenSig",dlenSig,"decay length significance",FlatTable::FloatColumn, 10);
 

    iEvent.put(std::move(pvTable),"pv");
    iEvent.put(std::move(otherPVsTable),"otherPVs");
    iEvent.put(std::move(svsTable),"svs");
    iEvent.put(std::move(selCandSv));
}

// ------------ method called once each stream before processing any runs, lumis or events  ------------
void
VertexTableProducer::beginStream(edm::StreamID)
{
}

// ------------ method called once each stream after processing all runs, lumis and events  ------------
void
VertexTableProducer::endStream() {
}

// ------------ method fills 'descriptions' with the allowed parameters for the module  ------------
void
VertexTableProducer::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  //The following says we do not know what parameters are allowed so do no validation
  // Please change this to state exactly what you do use, even if it is no parameters
  edm::ParameterSetDescription desc;
  desc.setUnknown();
  descriptions.addDefault(desc);
}

//define this as a plug-in
DEFINE_FWK_MODULE(VertexTableProducer);
