/**
 *  @file   LArContent/src/LArVertex/VertexSelectionAlgorithm.cc
 * 
 *  @brief  Implementation of the vertex selection algorithm class.
 * 
 *  $Log: $
 */
#include "Pandora/AlgorithmHeaders.h"

#include "LArHelpers/LArClusterHelper.h"
#include "LArHelpers/LArGeometryHelper.h"

#include "LArUtility/KDTreeLinkerAlgoT.h"

#include "LArVertex/VertexSelectionAlgorithm.h"
#include "LArHelpers/LArMCParticleHelper.h"

using namespace pandora;

namespace lar_content
{

VertexSelectionAlgorithm::VertexSelectionAlgorithm() :
    m_replaceCurrentVertexList(true),
    m_nDecayLengthsInZSpan(2.f),
    m_kappa(0.42f),
    m_selectSingleVertex(true),
    m_maxTopScoreSelections(3),
    m_minCandidateDisplacement(2.f),
    m_minCandidateScoreFraction(0.5f),
    m_useDetectorGaps(true),
    m_gapTolerance(0.f),
    m_isEmptyViewAcceptable(true),
    m_enableClustering(false),
    m_directionFilter(false),
    m_beamWeightFilter(false),
    m_nVerticesToSelect(5)
{
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode VertexSelectionAlgorithm::Run()
{
    const VertexList *pTopologyVertexList(NULL);
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::GetList(*this, m_topologyVertexListName, pTopologyVertexList));
    
    if (!pTopologyVertexList || pTopologyVertexList->empty())
    {
        if (PandoraContentApi::GetSettings(*this)->ShouldDisplayAlgorithmInfo())
            std::cout << "VertexSelectionAlgorithm: unable to find current vertex list " << std::endl;

        return STATUS_CODE_SUCCESS;
    }
    
    std::vector<const VertexList*> vertexListVector = m_pVertexClusteringTool->ClusterVertices(pTopologyVertexList);
    
    int nVertices(0);
    
    for (const VertexList* pVertexList : vertexListVector)
    {
        nVertices += pVertexList->size();
        
        //for (const Vertex *const pVertex : (*pVertexList))
        //{
        //    CartesianVector vertexPosition(pVertex->GetPosition());
        //    
        //    const CartesianVector vertexProjectionU(lar_content::LArGeometryHelper::ProjectPosition(this->GetPandora(), vertexPosition, TPC_VIEW_U));
        //    const CartesianVector vertexProjectionV(lar_content::LArGeometryHelper::ProjectPosition(this->GetPandora(), vertexPosition, TPC_VIEW_V));
        //    const CartesianVector vertexProjectionW(lar_content::LArGeometryHelper::ProjectPosition(this->GetPandora(), vertexPosition, TPC_VIEW_W));
        //    
        //    PANDORA_MONITORING_API(AddMarkerToVisualization(this->GetPandora(), &vertexProjectionU, "Top N Vertex U", RED, 1));
        //    PANDORA_MONITORING_API(AddMarkerToVisualization(this->GetPandora(), &vertexProjectionV, "Top N Vertex V", RED, 1));
        //    PANDORA_MONITORING_API(AddMarkerToVisualization(this->GetPandora(), &vertexProjectionW, "Top N Vertex W", RED, 1));
        //}        
        //PANDORA_MONITORING_API(ViewEvent(this->GetPandora()));
    }
    
    std::vector<VertexScoringTool::VertexScoreList> scoredClusterCollection;
    m_pVertexScoringTool->ScoreVertices(this, pTopologyVertexList, vertexListVector, scoredClusterCollection);
    
    VertexList selectedVertexList;
    this->SelectTopScoreVertices(scoredClusterCollection, selectedVertexList);
    
    if (!selectedVertexList.empty())
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::SaveList(*this, m_outputVertexListName, selectedVertexList));
    
    //---------------------------------------------------------------------------------------------------------------------------------------
    
    const VertexList *pEnergyVertexList(NULL);
    VertexScoringTool::VertexScoreList energyVertexScoreList;
    
    try
    {
        PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::GetList(*this, m_energyVertexListName, pEnergyVertexList));
        m_pVertexScoringTool->ScoreEnergyVertices(this, pEnergyVertexList, energyVertexScoreList);
    }
    catch (StatusCodeException &statusCodeException)
    {
        std::cout << "There are no energy vertices present." << std::endl;
    }

    //---------------------------------------------------------------------------------------------------------------------------------------
    
    this->StoreTopAllInformation(pTopologyVertexList, selectedVertexList, pEnergyVertexList);
    
    VertexScoringTool::VertexScoreList topNVertexScoreList;
    this->FindTopNVertices(scoredClusterCollection, energyVertexScoreList, topNVertexScoreList);
    
    this->StoreTopNInformation(topNVertexScoreList);
    
    //--------------------------------------------------------------------------------------------------------------------------------------
    
    if (!selectedVertexList.empty())
    {
        if (m_replaceCurrentVertexList)
            PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::ReplaceCurrentList<Vertex>(*this, m_outputVertexListName));
    }
    
    return STATUS_CODE_SUCCESS;
}
//------------------------------------------------------------------------------------------------------------------------------------------

void VertexSelectionAlgorithm::SelectTopScoreVertices(std::vector<VertexScoringTool::VertexScoreList> &scoredClusterCollection, VertexList &selectedVertexList) const
{
    float bestScore(0.f);
    
    VertexScoringTool::VertexScoreList vertexScoreList;
    for (VertexScoringTool::VertexScoreList &tempVertexScoreList : scoredClusterCollection)
        vertexScoreList.insert(vertexScoreList.begin(), tempVertexScoreList.begin(), tempVertexScoreList.end());
    
    std::sort(vertexScoreList.begin(), vertexScoreList.end());
    
    for (const VertexScoringTool::VertexScore &vertexScore : vertexScoreList)
    {
        if (selectedVertexList.size() >= m_maxTopScoreSelections)
            break;

        if (!selectedVertexList.empty() && !this->AcceptVertexLocation(vertexScore.GetVertex(), selectedVertexList))
            continue;

        if (!selectedVertexList.empty() && (vertexScore.GetScore() < m_minCandidateScoreFraction * bestScore))
            continue;

        selectedVertexList.insert(vertexScore.GetVertex());

        if (m_selectSingleVertex)
            return;

        if (vertexScore.GetScore() > bestScore)
            bestScore = vertexScore.GetScore();
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

bool VertexSelectionAlgorithm::AcceptVertexLocation(const Vertex *const pVertex, const VertexList &selectedVertexList) const
{
    const CartesianVector &position(pVertex->GetPosition());
    const float minCandidateDisplacementSquared(m_minCandidateDisplacement * m_minCandidateDisplacement);

    for (const Vertex *const pSelectedVertex : selectedVertexList)
    {
        if (pVertex == pSelectedVertex)
            return false;

        if ((position - pSelectedVertex->GetPosition()).GetMagnitudeSquared() < minCandidateDisplacementSquared)
            return false;
    }

    return true;
}

//------------------------------------------------------------------------------------------------------------------------------------------

void VertexSelectionAlgorithm::FindTopNVertices(std::vector<VertexScoringTool::VertexScoreList> &scoredClusterCollection, VertexScoringTool::VertexScoreList &energyVertexScoreList, VertexScoringTool::VertexScoreList &topNVertexScoreList)
{
    std::sort(energyVertexScoreList.begin(), energyVertexScoreList.end());

    unsigned int clusterCounter(0);
    
    if (m_enableClustering)
    {
        if (!energyVertexScoreList.empty())
        {
            topNVertexScoreList.push_back(*energyVertexScoreList.begin());
            clusterCounter++;
        }
        
        for (VertexScoringTool::VertexScoreList &vertexScoreList : scoredClusterCollection)
        {
            if (clusterCounter == m_nVerticesToSelect)
                break;
                
            if (vertexScoreList.size() == 0)
                continue;
            
            std::sort(vertexScoreList.begin(), vertexScoreList.end());
            
            topNVertexScoreList.push_back(*vertexScoreList.begin());
            clusterCounter++;
        }
    }
    else
    {
        VertexScoringTool::VertexScoreList temporaryVertexScoreList;
        
        for (VertexScoringTool::VertexScoreList &vertexScoreList : scoredClusterCollection)
        {
            if (vertexScoreList.size() == 0)
                continue;
            
            temporaryVertexScoreList.insert(temporaryVertexScoreList.begin(), vertexScoreList.begin(), vertexScoreList.end());
        }
        
        std::sort(temporaryVertexScoreList.begin(), temporaryVertexScoreList.end());
        
        if (!energyVertexScoreList.empty())
        {
            topNVertexScoreList.push_back(*energyVertexScoreList.begin());
            clusterCounter++;
        }
        
        for (VertexScoringTool::VertexScore &vertexScore : temporaryVertexScoreList)
        {
            if (clusterCounter == m_nVerticesToSelect)
                break;
            
            topNVertexScoreList.push_back(vertexScore);
            clusterCounter++;
        }
        
    }
    
    std::sort(topNVertexScoreList.begin(), topNVertexScoreList.end());
}

//------------------------------------------------------------------------------------------------------------------------------------------

void VertexSelectionAlgorithm::StoreTopNInformation(VertexScoringTool::VertexScoreList &topNVertexScoreList)
{
    for (VertexScoringTool::VertexScore &vertexScore : topNVertexScoreList)
    {
        CartesianVector vertexPosition(vertexScore.GetVertex()->GetPosition());
        
        const CartesianVector vertexProjectionU(lar_content::LArGeometryHelper::ProjectPosition(this->GetPandora(), vertexPosition, TPC_VIEW_U));
        const CartesianVector vertexProjectionV(lar_content::LArGeometryHelper::ProjectPosition(this->GetPandora(), vertexPosition, TPC_VIEW_V));
        const CartesianVector vertexProjectionW(lar_content::LArGeometryHelper::ProjectPosition(this->GetPandora(), vertexPosition, TPC_VIEW_W));
        
        PANDORA_MONITORING_API(AddMarkerToVisualization(this->GetPandora(), &vertexProjectionU, "Top N Vertex U", BLUE, 1));
        PANDORA_MONITORING_API(AddMarkerToVisualization(this->GetPandora(), &vertexProjectionV, "Top N Vertex V", BLUE, 1));
        PANDORA_MONITORING_API(AddMarkerToVisualization(this->GetPandora(), &vertexProjectionW, "Top N Vertex W", BLUE, 1));
    }
    
    PANDORA_MONITORING_API(ViewEvent(this->GetPandora()));
    
    const VertexList *pTopNTemporaryList(NULL);
    std::string topNTemporaryListName;
    PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::CreateTemporaryListAndSetCurrent(*this, pTopNTemporaryList, topNTemporaryListName));
    
    VertexList topNVerticesList;
    
    for (const VertexScoringTool::VertexScore &vertexScore: topNVertexScoreList)
    {
        PandoraContentApi::Vertex::Parameters parameters;
        parameters.m_position = vertexScore.GetVertex()->GetPosition();
        parameters.m_vertexLabel = vertexScore.GetVertex()->GetVertexLabel();
        parameters.m_vertexType = vertexScore.GetVertex()->GetVertexType();

        const Vertex *pVertexClone(NULL);
        PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::Vertex::Create(*this, parameters, pVertexClone));
        topNVerticesList.insert(pVertexClone);
    }
    
    if (!topNVerticesList.empty())
        PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::SaveList(*this, m_topNVertexListName, topNVerticesList));
}

//------------------------------------------------------------------------------------------------------------------------------------------

void VertexSelectionAlgorithm::StoreTopAllInformation(const VertexList* pTopologyVertexList, VertexList selectedVertexList, const VertexList* pEnergyVertexList)
{
    const VertexList *pAllVerticesTemporaryList(NULL);
    std::string allVerticesTemporaryList;
    PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::CreateTemporaryListAndSetCurrent(*this, pAllVerticesTemporaryList, allVerticesTemporaryList));
    
    VertexList allVerticesList;
    
    if (pTopologyVertexList != NULL)
    {
        for (const Vertex *const pVertex: (*pTopologyVertexList))
        {
            PandoraContentApi::Vertex::Parameters parameters;
            parameters.m_position = pVertex->GetPosition();
            parameters.m_vertexLabel = pVertex->GetVertexLabel();
            parameters.m_vertexType = pVertex->GetVertexType();
        
            const Vertex *pVertexClone(NULL);
            PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::Vertex::Create(*this, parameters, pVertexClone));
            
            allVerticesList.insert(pVertexClone);
        }
    }
        
    if (!selectedVertexList.empty())
    {
        for (const Vertex *const pVertex : selectedVertexList)
        {
            PandoraContentApi::Vertex::Parameters parameters;
            parameters.m_position = pVertex->GetPosition();
            parameters.m_vertexLabel = pVertex->GetVertexLabel();
            parameters.m_vertexType = pVertex->GetVertexType();
    
            const Vertex *pVertexClone(NULL);
            PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::Vertex::Create(*this, parameters, pVertexClone));
            
            allVerticesList.insert(pVertexClone);
        }
    }
    
    if (pEnergyVertexList != NULL)
    {
        for (const Vertex *const pVertex: (*pEnergyVertexList))
        {
            PandoraContentApi::Vertex::Parameters parameters;
            parameters.m_position = pVertex->GetPosition();
            parameters.m_vertexLabel = pVertex->GetVertexLabel();
            parameters.m_vertexType = pVertex->GetVertexType();
        
            const Vertex *pVertexClone(NULL);
            PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::Vertex::Create(*this, parameters, pVertexClone));
            
            allVerticesList.insert(pVertexClone);
        }
    }
    
    if (!allVerticesList.empty())
        PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::SaveList(*this, m_allOtherVertexListName, allVerticesList));
}

//----------------------------------------------------------------------------------------------------------------------------------

StatusCode VertexSelectionAlgorithm::ReadSettings(const TiXmlHandle xmlHandle)
{
    AlgorithmTool *pAlgorithmTool(nullptr);
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, XmlHelper::ProcessAlgorithmTool(*this, xmlHandle,
        "VertexClustering", pAlgorithmTool));

    m_pVertexClusteringTool = dynamic_cast<VertexClusteringTool *>(pAlgorithmTool);

    AlgorithmTool *pAnotherAlgorithmTool(nullptr);
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, XmlHelper::ProcessAlgorithmTool(*this, xmlHandle,
        "VertexScoring", pAnotherAlgorithmTool));

    m_pVertexScoringTool = dynamic_cast<VertexScoringTool *>(pAnotherAlgorithmTool);

    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, XmlHelper::ReadValue(xmlHandle, "TopologyVertexListName", m_topologyVertexListName));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, XmlHelper::ReadValue(xmlHandle, "EnergyVertexListName", m_energyVertexListName));

    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, XmlHelper::ReadValue(xmlHandle, "OutputVertexListName", m_outputVertexListName));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, XmlHelper::ReadValue(xmlHandle, "TopNVertexListName", m_topNVertexListName));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, XmlHelper::ReadValue(xmlHandle, "AllOtherVertexListName", m_allOtherVertexListName));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "ReplaceCurrentVertexList", m_replaceCurrentVertexList));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "BeamMode", m_beamMode));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "NDecayLengthsInZSpan", m_nDecayLengthsInZSpan));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "Kappa", m_kappa));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "SelectSingleVertex", m_selectSingleVertex));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MaxTopScoreSelections", m_maxTopScoreSelections));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MinCandidateDisplacement", m_minCandidateDisplacement));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MinCandidateScoreFraction", m_minCandidateScoreFraction));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "UseDetectorGaps", m_useDetectorGaps));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "GapTolerance", m_gapTolerance));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "IsEmptyViewAcceptable", m_isEmptyViewAcceptable));
        
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "EnableClustering", m_enableClustering));
        
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "DirectionFilter", m_directionFilter));
        
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "BeamWeightFilter", m_beamWeightFilter));
    
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "NVerticesToSelect", m_nVerticesToSelect));
        
    return STATUS_CODE_SUCCESS;
}

} // namespace lar_content
