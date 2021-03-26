/**
 *  @file   larpandoracontent/LArThreeDReco/LArTransverseTrackMatching/LongTracksTool.cc
 *
 *  @brief  Implementation of the long tracks tool class.
 *
 *  $Log: $
 */

#include "Pandora/AlgorithmHeaders.h"

#include "larpandoracontent/LArThreeDReco/LArTransverseTrackMatching/LongTracksTool.h"

using namespace pandora;

namespace lar_content
{

LongTracksTool::LongTracksTool() :
    m_minMatchedFraction(0.9f),
    m_minMatchedSamplingPoints(20),
    m_minXOverlapFraction(0.9f),
    m_minMatchedSamplingPointRatio(2)
{
}

//------------------------------------------------------------------------------------------------------------------------------------------

bool LongTracksTool::HasLongDirectConnections(IteratorList::const_iterator iIter, const IteratorList &iteratorList)
{
    for (IteratorList::const_iterator iIter2 = iteratorList.begin(), iIter2End = iteratorList.end(); iIter2 != iIter2End; ++iIter2)
    {
        if (iIter == iIter2)
            continue;

        if (((*iIter)->GetClusterU() == (*iIter2)->GetClusterU()) || ((*iIter)->GetClusterV() == (*iIter2)->GetClusterV()) ||
            ((*iIter)->GetClusterW() == (*iIter2)->GetClusterW()))
            return true;
    }

    return false;
}

//------------------------------------------------------------------------------------------------------------------------------------------

bool LongTracksTool::IsLongerThanDirectConnections(IteratorList::const_iterator iIter, const TensorType::ElementList &elementList,
    const unsigned int minMatchedSamplingPointRatio, const pandora::ClusterSet &usedClusters)
{
    const unsigned int nMatchedSamplingPoints((*iIter)->GetOverlapResult().GetNMatchedSamplingPoints());

    for (TensorType::ElementList::const_iterator eIter = elementList.begin(); eIter != elementList.end(); ++eIter)
    {
        if ((*iIter) == eIter)
            continue;

        if (usedClusters.count(eIter->GetClusterU()) || usedClusters.count(eIter->GetClusterV()) || usedClusters.count(eIter->GetClusterW()))
            continue;

        if (((*iIter)->GetClusterU() != eIter->GetClusterU()) && ((*iIter)->GetClusterV() != eIter->GetClusterV()) &&
            ((*iIter)->GetClusterW() != eIter->GetClusterW()))
            continue;

        if (nMatchedSamplingPoints < minMatchedSamplingPointRatio * eIter->GetOverlapResult().GetNMatchedSamplingPoints())
            return false;
    }

    return true;
}

//------------------------------------------------------------------------------------------------------------------------------------------

bool LongTracksTool::Run(ThreeViewTransverseTracksAlgorithm *const pAlgorithm, TensorType &overlapTensor)
{
    if (PandoraContentApi::GetSettings(*pAlgorithm)->ShouldDisplayAlgorithmInfo())
        std::cout << "----> Running Algorithm Tool: " << this->GetInstanceName() << ", " << this->GetType() << std::endl;

    ProtoParticleVector protoParticleVector;
    this->FindLongTracks(overlapTensor, protoParticleVector);

    const bool particlesMade(pAlgorithm->CreateThreeDParticles(protoParticleVector));
    return particlesMade;
}

//------------------------------------------------------------------------------------------------------------------------------------------

void LongTracksTool::FindLongTracks(const TensorType &overlapTensor, ProtoParticleVector &protoParticleVector) const
{
    ClusterSet usedClusters;
    ClusterVector sortedKeyClusters;
    overlapTensor.GetSortedKeyClusters(sortedKeyClusters);

    for (const Cluster *const pKeyCluster : sortedKeyClusters)
    {
        if (!pKeyCluster->IsAvailable())
            continue;

        unsigned int nU(0), nV(0), nW(0);
        TensorType::ElementList elementList;
        overlapTensor.GetConnectedElements(pKeyCluster, true, elementList, nU, nV, nW);

        IteratorList iteratorList;
        this->SelectLongElements(elementList, usedClusters, iteratorList);

        // Check that elements are not directly connected and are significantly longer than any other directly connected elements
        for (IteratorList::const_iterator iIter = iteratorList.begin(), iIterEnd = iteratorList.end(); iIter != iIterEnd; ++iIter)
        {
            if (LongTracksTool::HasLongDirectConnections(iIter, iteratorList))
                continue;

            if (!LongTracksTool::IsLongerThanDirectConnections(iIter, elementList, m_minMatchedSamplingPointRatio, usedClusters))
                continue;

            ProtoParticle protoParticle;
            protoParticle.m_clusterList.push_back((*iIter)->GetClusterU());
            protoParticle.m_clusterList.push_back((*iIter)->GetClusterV());
            protoParticle.m_clusterList.push_back((*iIter)->GetClusterW());
            protoParticleVector.push_back(protoParticle);

            usedClusters.insert((*iIter)->GetClusterU());
            usedClusters.insert((*iIter)->GetClusterV());
            usedClusters.insert((*iIter)->GetClusterW());
        }
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

void LongTracksTool::SelectLongElements(const TensorType::ElementList &elementList, const pandora::ClusterSet &usedClusters, IteratorList &iteratorList) const
{
    for (TensorType::ElementList::const_iterator eIter = elementList.begin(); eIter != elementList.end(); ++eIter)
    {
        if (usedClusters.count(eIter->GetClusterU()) || usedClusters.count(eIter->GetClusterV()) || usedClusters.count(eIter->GetClusterW()))
            continue;

        if (eIter->GetOverlapResult().GetMatchedFraction() < m_minMatchedFraction)
            continue;

        if (eIter->GetOverlapResult().GetNMatchedSamplingPoints() < m_minMatchedSamplingPoints)
            continue;

        const XOverlap &xOverlap(eIter->GetOverlapResult().GetXOverlap());

        if ((xOverlap.GetXSpanU() > std::numeric_limits<float>::epsilon()) && (xOverlap.GetXOverlapSpan() / xOverlap.GetXSpanU() > m_minXOverlapFraction) &&
            (xOverlap.GetXSpanV() > std::numeric_limits<float>::epsilon()) && (xOverlap.GetXOverlapSpan() / xOverlap.GetXSpanV() > m_minXOverlapFraction) &&
            (xOverlap.GetXSpanW() > std::numeric_limits<float>::epsilon()) && (xOverlap.GetXOverlapSpan() / xOverlap.GetXSpanW() > m_minXOverlapFraction))
        {
            iteratorList.push_back(eIter);
        }
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode LongTracksTool::ReadSettings(const TiXmlHandle xmlHandle)
{
    PANDORA_RETURN_RESULT_IF_AND_IF(
        STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle, "MinMatchedFraction", m_minMatchedFraction));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=,
        XmlHelper::ReadValue(xmlHandle, "MinMatchedSamplingPoints", m_minMatchedSamplingPoints));

    PANDORA_RETURN_RESULT_IF_AND_IF(
        STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle, "MinXOverlapFraction", m_minXOverlapFraction));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=,
        XmlHelper::ReadValue(xmlHandle, "MinMatchedSamplingPointRatio", m_minMatchedSamplingPointRatio));

    return STATUS_CODE_SUCCESS;
}

} // namespace lar_content
