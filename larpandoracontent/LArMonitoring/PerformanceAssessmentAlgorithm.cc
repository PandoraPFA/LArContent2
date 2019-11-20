/**
 *  @file   larpandoracontent/LArMonitoring/PerformanceAssessmentAlgorithm
 *
 *  @brief  Implementation of the performance assessment algorithm
 *
 *  $Log: $
 */

#include "Pandora/AlgorithmHeaders.h"

#include "larpandoracontent/LArHelpers/LArMCParticleHelper.h"
#include "larpandoracontent/LArHelpers/LArPfoHelper.h"
#include "larpandoracontent/LArHelpers/LArMonitoringHelper.h"

#include "larpandoracontent/LArMonitoring/PerformanceAssessmentAlgorithm.h"

using namespace pandora;

namespace lar_content {

  PerformanceAssessmentAlgorithm::PerformanceAssessmentAlgorithm() : 
    m_caloHitListName(), 
    m_pfoListName(),  
    m_writeToTree(false),
    m_printToScreen(true),
    m_eventTreeName(),
    m_targetMCParticleTreeName(),
    m_fileName(), 
    m_eventNumber(0)
  {
  }

  PerformanceAssessmentAlgorithm::~PerformanceAssessmentAlgorithm() 
  {
    if(m_writeToTree) {

    try {
      PANDORA_MONITORING_API(SaveTree(this->GetPandora(), m_eventTreeName, m_fileName, "UPDATE"));
    }
    catch (const StatusCodeException &) {
      std::cout << "UNABLE TO WRITE TREE" << std::endl; 
    }

    try {
      PANDORA_MONITORING_API(SaveTree(this->GetPandora(), m_targetMCParticleTreeName, m_fileName, "UPDATE"));
    }
    catch (const StatusCodeException &) {
      std::cout << "UNABLE TO WRITE TREE" << std::endl; 
    }

    }

  }


  StatusCode PerformanceAssessmentAlgorithm::Run() {
             
    const MCParticleList *pMCParticleList = nullptr;
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::GetCurrentList(*this, pMCParticleList));

    const CaloHitList *pCaloHitList = nullptr;
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::GetList(*this, m_caloHitListName, pCaloHitList));

    const PfoList *pPfoList = nullptr;
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::GetList(*this, m_pfoListName, pPfoList));


    
    // Construct target MC particle to reconstructable hits map
    LArMCParticleHelper::MCContributionMap nuMCParticlesToGoodHitsMap;
 
   if(m_parameters.m_foldToPrimaries){
      LArMCParticleHelper::SelectReconstructableMCParticles(pMCParticleList, pCaloHitList, m_parameters, LArMCParticleHelper::IsBeamNeutrinoFinalState, nuMCParticlesToGoodHitsMap);
    } else {
      LArMCParticleHelper::SelectReconstructableMCParticles(pMCParticleList, pCaloHitList, m_parameters, LArMCParticleHelper::IsDownstreamOfBeamNeutrino, nuMCParticlesToGoodHitsMap);
    }

    
    // Get pfo to reconstructable hits map
    LArMCParticleHelper::PfoContributionMap pfoToReconstructable2DHitsMap;

    if (m_parameters.m_foldToPrimaries) {
      // Get list of pfos to be matched with the target MC particles
      PfoList finalStatePfos;
      for (const ParticleFlowObject *const pPfo : *pPfoList) {
	if (LArPfoHelper::IsFinalState(pPfo))
	  finalStatePfos.push_back(pPfo);
      }

      LArMCParticleHelper::GetPfoToReconstructable2DHitsMap(finalStatePfos, nuMCParticlesToGoodHitsMap, pfoToReconstructable2DHitsMap);

    } else {
      LArMCParticleHelper::GetUnfoldedPfoToReconstructable2DHitsMap(*pPfoList, nuMCParticlesToGoodHitsMap, pfoToReconstructable2DHitsMap);
    }


    // Find hits that they share
    LArMCParticleHelper::PfoToMCParticleHitSharingMap pfoToMCParticleHitSharingMap;
    LArMCParticleHelper::MCParticleToPfoHitSharingMap mcParticleToPfoHitSharingMap;
    LArMCParticleHelper::GetPfoMCParticleHitSharingMaps(pfoToReconstructable2DHitsMap, {nuMCParticlesToGoodHitsMap}, pfoToMCParticleHitSharingMap, mcParticleToPfoHitSharingMap);



    // Calculate purity and completeness for MC->Pfo matches
    LArMCParticleHelper::MCParticleToPfoCompletenessPurityMap mcParticleToPfoCompletenessMap;
    LArMCParticleHelper::MCParticleToPfoCompletenessPurityMap mcParticleToPfoPurityMap; 
    LArMCParticleHelper::GetMCToPfoCompletenessPurityMaps(nuMCParticlesToGoodHitsMap, pfoToReconstructable2DHitsMap, mcParticleToPfoHitSharingMap, mcParticleToPfoCompletenessMap, mcParticleToPfoPurityMap);

//------------------------------------------------------------------------------------------------------------------------------------------

    MCParticleVector orderedTargetMCParticleVector;
    LArMonitoringHelper::GetOrderedMCParticleVector({nuMCParticlesToGoodHitsMap}, orderedTargetMCParticleVector);

    PfoVector orderedPfoVector;
    LArMonitoringHelper::GetOrderedPfoVector(pfoToReconstructable2DHitsMap, orderedPfoVector);


    if(m_writeToTree) {

    ++m_eventNumber;


    for(const MCParticle *const pMCParticle : orderedTargetMCParticleVector) {

      int uHits(0);
      int vHits(0);
      int wHits(0);

      for(const CaloHit *const caloHit : nuMCParticlesToGoodHitsMap.at(pMCParticle)) { 
	if(caloHit->GetHitType() == TPC_VIEW_U) {
	  ++uHits;
	} else if(caloHit->GetHitType() == TPC_VIEW_V) {
	  ++vHits;
	} else {
	  ++wHits;
	}
      }

      int matchesMade = mcParticleToPfoHitSharingMap.at(pMCParticle).size();

      std::vector<double> completenessVector;
      std::vector<double> purityVector;

      for(auto entry : mcParticleToPfoCompletenessMap.at(pMCParticle)) {
	completenessVector.push_back(entry.second);
      }

      for(auto entry : mcParticleToPfoPurityMap.at(pMCParticle)) {
	purityVector.push_back(entry.second);
      }

      PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_targetMCParticleTreeName, "EventNumber", m_eventNumber));
      PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_targetMCParticleTreeName, "MCParticleID", pMCParticle->GetParticleId()));
      
      PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_targetMCParticleTreeName, "Energy", pMCParticle->GetEnergy()));

      PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_targetMCParticleTreeName, "totHits", static_cast<int>(nuMCParticlesToGoodHitsMap.at(pMCParticle).size())));
      PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_targetMCParticleTreeName, "uHits", uHits));
      PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_targetMCParticleTreeName, "vHits", vHits));
      PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_targetMCParticleTreeName, "wHits", wHits));   

      PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_targetMCParticleTreeName, "MatchesMade", matchesMade));
      PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_targetMCParticleTreeName, "Completeness", &completenessVector));
      PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_targetMCParticleTreeName, "Purity", &purityVector));

      PANDORA_MONITORING_API(FillTree(this->GetPandora(), m_targetMCParticleTreeName));

    }

    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_eventTreeName, "EventNumber", m_eventNumber));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_eventTreeName, "MCParticleNumber", static_cast<int>(orderedTargetMCParticleVector.size())));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_eventTreeName, "PfoNumber", static_cast<int>(orderedPfoVector.size())));


    PANDORA_MONITORING_API(FillTree(this->GetPandora(), m_eventTreeName));

    }

//------------------------------------------------------------------------------------------------------------------------------------------

    if(m_printToScreen) {

    // Visualize the target MC particles
    PandoraMonitoringApi::Create(this->GetPandora());
    PandoraMonitoringApi::SetEveDisplayParameters(this->GetPandora(), true, DETECTOR_VIEW_DEFAULT, -1.f, 1.f, 1.f);
    srand(time(NULL));
    for(const MCParticle *const pMCParticle : orderedTargetMCParticleVector) {
      CaloHitList uHits;
      CaloHitList vHits;
      CaloHitList wHits;
      for(const CaloHit *const caloHit : nuMCParticlesToGoodHitsMap.at(pMCParticle)) { 
	if(caloHit->GetHitType() == TPC_VIEW_U) {
	  uHits.push_back(caloHit);
	} else if(caloHit->GetHitType() == TPC_VIEW_V) {
	  vHits.push_back(caloHit);
	} else {
	  wHits.push_back(caloHit);
	}
      }
      Color colour = static_cast<Color>((rand() % (Color::LIGHTYELLOW - 1)) + 1);
      std::string name = "PDG: " + std::to_string(pMCParticle->GetParticleId()) + " Hierarchy Tier: " + std::to_string(LArMCParticleHelper::GetHierarchyTier(pMCParticle));
      PandoraMonitoringApi::VisualizeCaloHits(this->GetPandora(), &uHits, name + " (" + std::to_string(uHits.size()) + " U HITS)", colour);
      PandoraMonitoringApi::VisualizeCaloHits(this->GetPandora(), &vHits, name + " (" + std::to_string(vHits.size()) + " V HITS)", colour);
      PandoraMonitoringApi::VisualizeCaloHits(this->GetPandora(), &wHits, name + " (" + std::to_string(wHits.size()) + " W HITS)", colour);

      PandoraMonitoringApi::Pause(this->GetPandora());
    }
    PandoraMonitoringApi::ViewEvent(this->GetPandora());





    typedef std::map<const ParticleFlowObject*, int> PfoToIdMap; 

    PfoToIdMap pfoToIdMap;
    for(unsigned int id(0); id < orderedPfoVector.size(); ++id) {
      pfoToIdMap[orderedPfoVector[id]] = (id + 1);
    }
    
    // Visualize Pfos
    PandoraMonitoringApi::Create(this->GetPandora());
    PandoraMonitoringApi::SetEveDisplayParameters(this->GetPandora(), true, DETECTOR_VIEW_DEFAULT, -1.f, 1.f, 1.f);
    srand(time(NULL));

    for(const ParticleFlowObject *const pPfo: orderedPfoVector) {
      CaloHitList uHits;
      CaloHitList vHits;
      CaloHitList wHits;
      for(const CaloHit *const caloHit : pfoToReconstructable2DHitsMap.at(pPfo)) { 
	if(caloHit->GetHitType() == TPC_VIEW_U) {
	  uHits.push_back(caloHit);
	} else if(caloHit->GetHitType() == TPC_VIEW_V) {
	  vHits.push_back(caloHit);
	} else {
	  wHits.push_back(caloHit);
	}
      }
      Color colour = static_cast<Color>((rand() % (Color::LIGHTYELLOW - 1)) + 1);
      std::string name = "Id: " + std::to_string(pfoToIdMap.at(pPfo)) + " Hierarchy Tier: " + std::to_string(LArPfoHelper::GetHierarchyTier(pPfo));
      PandoraMonitoringApi::VisualizeCaloHits(this->GetPandora(), &uHits, name + " (" + std::to_string(uHits.size()) + " U HITS)", colour);
      PandoraMonitoringApi::VisualizeCaloHits(this->GetPandora(), &vHits, name + " (" + std::to_string(vHits.size()) + " V HITS)", colour);
      PandoraMonitoringApi::VisualizeCaloHits(this->GetPandora(), &wHits, name + " (" + std::to_string(wHits.size()) + " W HITS)", colour);

      PandoraMonitoringApi::Pause(this->GetPandora());
    }
    PandoraMonitoringApi::ViewEvent(this->GetPandora());
    

//------------------------------------------------------------------------------------------------------------------------------------------

    unsigned int reconstructedMCParticles(0);

    for(const MCParticle *const pMCParticle : orderedTargetMCParticleVector) {

      LArMCParticleHelper::MCParticleToPfoCompletenessPurityMap::const_iterator completenessIter = mcParticleToPfoCompletenessMap.find(pMCParticle);
      LArMCParticleHelper::MCParticleToPfoCompletenessPurityMap::const_iterator purityIter = mcParticleToPfoPurityMap.find(pMCParticle);

      bool isReconstructed(false);

      if((completenessIter == mcParticleToPfoCompletenessMap.end()) && (purityIter != mcParticleToPfoPurityMap.end()))
	std::cout << "REALLY BIG ERROR" << std::endl;

      if((completenessIter == mcParticleToPfoCompletenessMap.end()) && (purityIter == mcParticleToPfoPurityMap.end())){
	std::cout << "I DON'T THINK THIS SHOULD HAPPEN" << std::endl;
	continue;
      }

      std::string mcTag = "MC Particle: (PDG: " + std::to_string(pMCParticle->GetParticleId()) + " Hierarchy Tier: " + std::to_string(LArMCParticleHelper::GetHierarchyTier(pMCParticle)) + ")";
      std::cout << mcTag << std::endl;

      std::cout << completenessIter->second.size() << " match(es) made: ";

      if(!completenessIter->second.size()) {
	std::cout << std::endl;
	std::cout << "NOT RECONSTRUCTED" << std::endl;
	continue;
      }
      
      std::cout << "(Pfo Id, Shared Hits, Completeness, Purity)" << std::endl;

      for(LArMCParticleHelper::PfoToSharedHitsVector::const_iterator iter(mcParticleToPfoHitSharingMap.at(pMCParticle).begin()); iter != mcParticleToPfoHitSharingMap.at(pMCParticle).end(); ++iter) {

	std::cout << "(" << pfoToIdMap.at(iter->first) << ", " << iter->second.size();


        LArMCParticleHelper::PfoCompletenessPurityPair matchedPfoCompletenessPair;
        LArMCParticleHelper::PfoCompletenessPurityPair matchedPfoPurityPair;

        for(LArMCParticleHelper::PfoToCompletenessPurityVector::const_iterator pfoCompletenessPairIter(completenessIter->second.begin()); pfoCompletenessPairIter != completenessIter->second.end(); ++pfoCompletenessPairIter) {
	  if(pfoToIdMap.at(pfoCompletenessPairIter->first) == pfoToIdMap.at(iter->first))   
	    matchedPfoCompletenessPair = *pfoCompletenessPairIter;
	}

        for(LArMCParticleHelper::PfoToCompletenessPurityVector::const_iterator pfoPurityPairIter(purityIter->second.begin()); pfoPurityPairIter != purityIter->second.end(); ++pfoPurityPairIter) {
	  if(pfoToIdMap.at(pfoPurityPairIter->first) == pfoToIdMap.at(iter->first)) 
	    matchedPfoPurityPair = *pfoPurityPairIter;
	}

	std::cout << ", " << matchedPfoCompletenessPair.second;
	std::cout << ", " << matchedPfoPurityPair.second << ")" << std::endl;

	if((matchedPfoCompletenessPair.second > 0.8) && (matchedPfoPurityPair.second > 0.8)) {
	  isReconstructed = true;
	  reconstructedMCParticles++;
	}
	
      }

      isReconstructed ? std::cout << "RECONSTRUCTED" << std::endl : std::cout << "NOT RECONSTRUCTED" << std::endl;
      
    }

    std::cout << "Reconstruction Efficiency: " << static_cast<double>(reconstructedMCParticles*100)/static_cast<double>(orderedTargetMCParticleVector.size()) << "%" << std::endl;


    }

    return STATUS_CODE_SUCCESS;

  }

//------------------------------------------------------------------------------------------------------------------------------------------

  void PerformanceAssessmentAlgorithm::GetMatchedMCParticlePfoCompleteness(const LArMCParticleHelper::MCParticleToPfoCompletenessPurityMap &mcParticleToPfoCompletenessMap, const MCParticle *const pMCParticle, const ParticleFlowObject *const pPfo, double &completeness) {

    LArMCParticleHelper::MCParticleToPfoCompletenessPurityMap::const_iterator completenessIter = mcParticleToPfoCompletenessMap.find(pMCParticle);

    for(LArMCParticleHelper::PfoToCompletenessPurityVector::const_iterator pfoCompletenessPairIter(completenessIter->second.begin()); pfoCompletenessPairIter != completenessIter->second.end(); ++pfoCompletenessPairIter) {
      if(pfoCompletenessPairIter->first == pPfo)
	completeness = pfoCompletenessPairIter->second;
    }

  }

//------------------------------------------------------------------------------------------------------------------------------------------

  void PerformanceAssessmentAlgorithm::GetMatchedMCParticlePfoPurity(const LArMCParticleHelper::MCParticleToPfoCompletenessPurityMap &mcParticleToPfoPurityMap, const MCParticle *const pMCParticle, const ParticleFlowObject *const pPfo, double &purity) {

    LArMCParticleHelper::MCParticleToPfoCompletenessPurityMap::const_iterator purityIter = mcParticleToPfoPurityMap.find(pMCParticle);

    for(LArMCParticleHelper::PfoToCompletenessPurityVector::const_iterator pfoPurityPairIter(purityIter->second.begin()); pfoPurityPairIter != purityIter->second.end(); ++pfoPurityPairIter) {
      if(pfoPurityPairIter->first == pPfo)
        purity = pfoPurityPairIter->second;
    }

  }



//------------------------------------------------------------------------------------------------------------------------------------------



  StatusCode PerformanceAssessmentAlgorithm::ReadSettings(const TiXmlHandle xmlHandle) {

    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, XmlHelper::ReadValue(xmlHandle, "CaloHitListName", m_caloHitListName));

    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, XmlHelper::ReadValue(xmlHandle, "PfoListName", m_pfoListName));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MinPrimaryGoodHits", m_parameters.m_minPrimaryGoodHits));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MinHitsForGoodView", m_parameters.m_minHitsForGoodView));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MinPrimaryGoodViews", m_parameters.m_minPrimaryGoodViews));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "SelectInputHits", m_parameters.m_selectInputHits));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "FoldToPrimaries", m_parameters.m_foldToPrimaries));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MaxPhotonPropagation", m_parameters.m_maxPhotonPropagation));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MinHitSharingFraction", m_parameters.m_minHitSharingFraction));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "WriteToTree", m_writeToTree));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "PrintToScreen", m_printToScreen));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "EventTreeName", m_eventTreeName));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "TargetMCParticleTreeName", m_targetMCParticleTreeName));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "FileName", m_fileName));


  return STATUS_CODE_SUCCESS;

}


} //namespace lar_content

