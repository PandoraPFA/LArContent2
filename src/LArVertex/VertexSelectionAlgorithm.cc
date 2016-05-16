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
    m_minVertexAcceptableViews(3),
    m_writeToTree(false)
{
}
//------------------------------------------------------------------------------------------------------------------------------------------

VertexSelectionAlgorithm::~VertexSelectionAlgorithm()
{
    if (m_writeToTree)
    {
        PANDORA_MONITORING_API(SaveTree(this->GetPandora(), m_treeName.c_str(), m_fileName.c_str(), "UPDATE"));
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode VertexSelectionAlgorithm::Run()
{
    const VertexList *pInputVertexList(NULL);
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::GetCurrentList(*this, pInputVertexList));

    if (!pInputVertexList || pInputVertexList->empty())
    {
        if (PandoraContentApi::GetSettings(*this)->ShouldDisplayAlgorithmInfo())
            std::cout << "VertexSelectionAlgorithm: unable to find current vertex list " << std::endl;

        return STATUS_CODE_SUCCESS;
    }

    //Test clustering tool
     std::vector<const VertexList*> vertexListVector = m_pVertexClusteringTool->ClusterVertices(pInputVertexList);
        
//    
//    for (const VertexList &vertexList : vertexListVector)
//    {
//        for (const Vertex *const pVertex : vertexList)
//        {
//            const CartesianVector vertexProjectionU(lar_content::LArGeometryHelper::ProjectPosition(this->GetPandora(), pVertex->GetPosition(), TPC_VIEW_U));
//            const CartesianVector vertexProjectionV(lar_content::LArGeometryHelper::ProjectPosition(this->GetPandora(), pVertex->GetPosition(), TPC_VIEW_V));
//            const CartesianVector vertexProjectionW(lar_content::LArGeometryHelper::ProjectPosition(this->GetPandora(), pVertex->GetPosition(), TPC_VIEW_W));
//            
//            PANDORA_MONITORING_API(AddMarkerToVisualization(this->GetPandora(), &vertexProjectionU, "Target Vertex", RED, 1));
//            PANDORA_MONITORING_API(AddMarkerToVisualization(this->GetPandora(), &vertexProjectionV, "Target Vertex", RED, 1));
//            PANDORA_MONITORING_API(AddMarkerToVisualization(this->GetPandora(), &vertexProjectionW, "Target Vertex", RED, 1));
//        }
//        
//        PANDORA_MONITORING_API(ViewEvent(this->GetPandora()));
//    }
    
    VertexScoringTool::VertexScoreList intermediateVertexScoreList;
    m_pVertexScoringTool->ScoreVertices(this, vertexListVector, intermediateVertexScoreList);
    
    //for (VertexScoringTool::VertexScore &vertexScore : intermediateVertexScoreList)
    //{
    //    const Vertex *const pVertex(vertexScore.GetVertex());
    //    
    //    const CartesianVector vertexProjectionU(lar_content::LArGeometryHelper::ProjectPosition(this->GetPandora(), pVertex->GetPosition(), TPC_VIEW_U));
    //    const CartesianVector vertexProjectionV(lar_content::LArGeometryHelper::ProjectPosition(this->GetPandora(), pVertex->GetPosition(), TPC_VIEW_V));
    //    const CartesianVector vertexProjectionW(lar_content::LArGeometryHelper::ProjectPosition(this->GetPandora(), pVertex->GetPosition(), TPC_VIEW_W));
    //    
    //    PANDORA_MONITORING_API(AddMarkerToVisualization(this->GetPandora(), &vertexProjectionU, "Target Vertex", GREEN, 1));
    //    PANDORA_MONITORING_API(AddMarkerToVisualization(this->GetPandora(), &vertexProjectionV, "Target Vertex", GREEN, 1));
    //    PANDORA_MONITORING_API(AddMarkerToVisualization(this->GetPandora(), &vertexProjectionW, "Target Vertex", GREEN, 1));
    //    
    //    PANDORA_MONITORING_API(ViewEvent(this->GetPandora()));
    //}
    
    //-------------------------TREE--------------------------------------------------------------------
    const MCParticleList *pMCParticleList = NULL;
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::GetList(*this, "MCParticleList3D", pMCParticleList));
    MCParticleVector mcNeutrinoList;                                // true neutrinos
    LArMCParticleHelper::GetNeutrinoMCParticleList(pMCParticleList, mcNeutrinoList);
    
    CartesianVector mcNeutrinoVertexPosition((*mcNeutrinoList.begin())->GetVertex());
    std::vector<float> allVerticesDR;
    
    std::sort(intermediateVertexScoreList.begin(), intermediateVertexScoreList.end());
    
    for (VertexScoringTool::VertexScore &vertexScore : intermediateVertexScoreList)
    {
        const Vertex *const pVertex(vertexScore.GetVertex());
        float vertexDR((pVertex->GetPosition() - mcNeutrinoVertexPosition).GetMagnitude());
        allVerticesDR.push_back(vertexDR);
    }
    
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "allVerticesDR", &allVerticesDR));
    PANDORA_MONITORING_API(FillTree(this->GetPandora(), m_treeName.c_str()));
    //-------------------------TREE--------------------------------------------------------------------
    
    VertexList selectedVertexList;
    this->SelectTopScoreVertices(intermediateVertexScoreList, selectedVertexList);
    
    std::cout << "# output: " << selectedVertexList.size() << std::endl;
    
    if (!selectedVertexList.empty())
    {
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::SaveList(*this, m_outputVertexListName, selectedVertexList));
    
        if (m_replaceCurrentVertexList)
            PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::ReplaceCurrentList<Vertex>(*this, m_outputVertexListName));
    }

    return STATUS_CODE_SUCCESS;
}
//------------------------------------------------------------------------------------------------------------------------------------------

void VertexSelectionAlgorithm::SelectTopScoreVertices(VertexScoringTool::VertexScoreList &vertexScoreList, VertexList &selectedVertexList) const
{
    float bestScore(0.f);
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
    
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "WriteToTree", m_writeToTree));
    
    if (m_writeToTree)
    {
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, XmlHelper::ReadValue(xmlHandle, "OutputTree", m_treeName));
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, XmlHelper::ReadValue(xmlHandle, "OutputFile", m_fileName));
    }

    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, XmlHelper::ReadValue(xmlHandle, "OutputVertexListName", m_outputVertexListName));

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
        "MinVertexAcceptableViews", m_minVertexAcceptableViews));

    return STATUS_CODE_SUCCESS;
}

} // namespace lar_content
