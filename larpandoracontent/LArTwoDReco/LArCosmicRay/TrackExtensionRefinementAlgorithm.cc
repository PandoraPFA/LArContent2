/**
 *  @file   larpandoracontent/LArTwoDReco/LArCosmicRay/TrackExtensionRefinementAlgorithm.cc
 *
 *  @brief  Implementation of the track refinement class.
 *
 *  $Log: $
 */
#include "Pandora/AlgorithmHeaders.h"

#include "larpandoracontent/LArTwoDReco/LArCosmicRay/TrackExtensionRefinementAlgorithm.h"

#include "larpandoracontent/LArControlFlow/MultiPandoraApi.h"
#include "larpandoracontent/LArHelpers/LArStitchingHelper.h"

#include "larpandoracontent/LArHelpers/LArGeometryHelper.h"
#include "larpandoracontent/LArHelpers/LArClusterHelper.h"

using namespace pandora;

namespace lar_content
{

TrackExtensionRefinementAlgorithm::TrackExtensionRefinementAlgorithm() :
    m_growingFitInitialLength(20.f),
    m_growingFitSegmentLength(5.0f),
    m_furthestDistanceToLine(10.f),
    m_closestDistanceToLine(0.5f)        
{
}

//------------------------------------------------------------------------------------------------------------------------------------------    
    
StatusCode TrackExtensionRefinementAlgorithm::Run()
{
    PandoraMonitoringApi::SetEveDisplayParameters(this->GetPandora(), true, DETECTOR_VIEW_DEFAULT, -1.f, 1.f, 1.f);
    //std::cout << "IF TRACK IN EM SHOWER REMEMBER YOU CHANGED THE DIRECTION!" << std::endl;
    
    const ClusterList *pClusterList(nullptr);
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::GetCurrentList(*this, pClusterList));

    const CaloHitList *pCaloHitList(nullptr);
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::GetCurrentList(*this, pCaloHitList));

    ClusterVector clusterVector;
    TwoDSlidingFitResultMap microSlidingFitResultMap, macroSlidingFitResultMap;
    SlidingFitResultMapPair slidingFitResultMapPair({&microSlidingFitResultMap, &macroSlidingFitResultMap});

    this->InitialiseGeometry();
    
    this->InitialiseContainers(pClusterList, clusterVector, slidingFitResultMapPair);

    for (unsigned int isHigherXBoundary = 0; isHigherXBoundary < 2; ++isHigherXBoundary)
    {
        const float nearestTPCBoundaryX(isHigherXBoundary ? m_tpcMaxXEdge : m_tpcMinXEdge);
        if ((std::fabs(nearestTPCBoundaryX - m_detectorMinXEdge) < std::numeric_limits<float>::epsilon()) ||
            (std::fabs(nearestTPCBoundaryX - m_detectorMaxXEdge) < std::numeric_limits<float>::epsilon()))
        {
            continue;
        }
        
        unsigned int loopIterations(0);
        ClusterList consideredClusters;
        while(loopIterations < 10) 
        {
            ++loopIterations;

            std::sort(clusterVector.begin(), clusterVector.end(), SortByDistanceToTPCBoundary(isHigherXBoundary ? m_tpcMaxXEdge : m_tpcMinXEdge));

            std::cout << "\033[31m" << "isHigherXBoundary: " << isHigherXBoundary << "\033[0m"  << std::endl;

            ClusterEndpointAssociation clusterAssociation;
            if (!this->FindBestClusterAssociation(clusterVector, slidingFitResultMapPair, clusterAssociation, pClusterList, isHigherXBoundary))
                break;
            
            // ATTN: Considering clusterAssociation so remove from 'to consider' clusters i.e. the clusterVector
            this->ConsiderCluster(clusterAssociation, clusterVector);

            ClusterToCaloHitListMap clusterToCaloHitListMap;
            this->GetExtrapolatedCaloHits(clusterAssociation, pClusterList, clusterToCaloHitListMap);

            if(!this->AreExtrapolatedHitsGood(clusterAssociation, clusterToCaloHitListMap, isHigherXBoundary))
            {
                std::cout << "\033[31m" << "EXTRAPOLATED HITS ARE PANTS - ABORT" << "\033[0m"  << std::endl;
                continue;
            }

            this->CreateMainTrack(clusterAssociation, clusterToCaloHitListMap, pClusterList, clusterVector, slidingFitResultMapPair, consideredClusters);
        }
            
        if (isHigherXBoundary == 0)
            InitialiseContainers(&consideredClusters, clusterVector, slidingFitResultMapPair);
    }


    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------  

void TrackExtensionRefinementAlgorithm::GetExtrapolatedCaloHits(ClusterEndpointAssociation &clusterAssociation, const ClusterList *const pClusterList,
    ClusterToCaloHitListMap &clusterToCaloHitListMap) const
{

    // Look for clusters in the region of interest
    ClusterToCaloHitListMap hitsInRegion;
    for (const Cluster *const pCluster : *pClusterList)
    {
        const OrderedCaloHitList &orderedCaloHitList(pCluster->GetOrderedCaloHitList());
        for (const OrderedCaloHitList::value_type &mapEntry : orderedCaloHitList)
        {
            for (const CaloHit *const pCaloHit : *mapEntry.second)
            {
                const CartesianVector &hitPosition(pCaloHit->GetPositionVector());
                
                if(!IsInLineSegment(clusterAssociation.GetUpstreamMergePoint(), clusterAssociation.GetDownstreamMergePoint(), hitPosition))
                    continue;
                
                hitsInRegion[pCluster].push_back(pCaloHit);
            }
        }
    }

    // ATTN: hitsInRegion is ordered (it isnt)
    ClusterVector clustersInRegion;
    for (const ClusterToCaloHitListMap::value_type &entry : hitsInRegion)
        clustersInRegion.push_back(entry.first);

    std::sort(clustersInRegion.begin(), clustersInRegion.end(), LArClusterHelper::SortByNHits);
    
    CartesianVector extrapolatedStartPosition(clusterAssociation.IsEndUpstream() ? clusterAssociation.GetDownstreamMergePoint() : clusterAssociation.GetUpstreamMergePoint());
    CartesianVector extrapolatedDirection(clusterAssociation.IsEndUpstream() ? clusterAssociation.GetDownstreamMergeDirection() : clusterAssociation.GetUpstreamMergeDirection());
    const CartesianVector clusterSubsetBoundary(extrapolatedStartPosition + (extrapolatedDirection * (-1.f) * m_growingFitInitialLength));

    const float minX(std::min(extrapolatedStartPosition.GetX(), clusterSubsetBoundary.GetX())), maxX(std::max(extrapolatedStartPosition.GetX(), clusterSubsetBoundary.GetX()));
    const float minZ(std::min(extrapolatedStartPosition.GetZ(), clusterSubsetBoundary.GetZ())), maxZ(std::max(extrapolatedStartPosition.GetZ(), clusterSubsetBoundary.GetZ()));

    CartesianPointVector hitPositionVector;
    const OrderedCaloHitList &orderedCaloHitList(clusterAssociation.GetMainTrackCluster()->GetOrderedCaloHitList());
    for (const OrderedCaloHitList::value_type &mapEntry : orderedCaloHitList)
    {
        for (const CaloHit *const pCaloHit : *mapEntry.second)
        {
            const CartesianVector &hitPosition(pCaloHit->GetPositionVector());

            if ((hitPosition.GetX() < minX) || (hitPosition.GetX() > maxX) || (hitPosition.GetZ() < minZ) || (hitPosition.GetZ() > maxZ))
                continue;

            hitPositionVector.push_back(hitPosition);    
        }
    }
    
    unsigned int count(0);
    //unsigned int count(1);
    unsigned int hitsCollected(std::numeric_limits<int>::max());
    CartesianVector extrapolatedEndPosition(0.f, 0.f, 0.f);
    while (hitsCollected > 0)
    {
        hitsCollected = 0;
        
        try
        {
            const float slidingFitPitch(LArGeometryHelper::GetWireZPitch(this->GetPandora()));
            const TwoDSlidingFitResult extrapolatedFit(&hitPositionVector, m_microSlidingFitWindow, slidingFitPitch);

            if (count > 0)
            {
                extrapolatedStartPosition = clusterAssociation.IsEndUpstream() ? extrapolatedFit.GetGlobalMinLayerPosition() : extrapolatedFit.GetGlobalMaxLayerPosition();
                extrapolatedDirection = clusterAssociation.IsEndUpstream() ? extrapolatedFit.GetGlobalMinLayerDirection() * (-1.f) : extrapolatedFit.GetGlobalMaxLayerDirection();
            }
            
            extrapolatedEndPosition = extrapolatedStartPosition + (extrapolatedDirection * m_growingFitSegmentLength);
            const float gradient((extrapolatedEndPosition.GetZ() - extrapolatedStartPosition.GetZ()) / (extrapolatedEndPosition.GetX() - extrapolatedStartPosition.GetX()));
                        
            ////////////////
            PandoraMonitoringApi::AddMarkerToVisualization(this->GetPandora(), &extrapolatedStartPosition, "start", RED, 2);
            PandoraMonitoringApi::AddMarkerToVisualization(this->GetPandora(), &extrapolatedEndPosition, "end", RED, 2);
            ////////////////

            for (const Cluster *const pCluster : clustersInRegion)
            {
                const CaloHitList &theCaloHits(hitsInRegion.at(pCluster));
                for (const CaloHit *const pCaloHit : theCaloHits)
                {
                    const CartesianVector &hitPosition(pCaloHit->GetPositionVector());

                    // ATTN: To avoid counting same hit twice
                    const ClusterToCaloHitListMap::iterator iter(clusterToCaloHitListMap.find(pCluster));
                    if (iter != clusterToCaloHitListMap.end())
                    {
                        if (std::find(iter->second.begin(), iter->second.end(), pCaloHit) != iter->second.end())
                            continue;
                    }

                    if (!IsInLineSegment(extrapolatedStartPosition, extrapolatedEndPosition, hitPosition))
                        continue;

                    const float &hitWidth(pCaloHit->GetCellSize1());
                    const CartesianVector hitHighEdge(hitPosition.GetX() + (hitWidth * 0.5f), 0, hitPosition.GetZ());
                    const CartesianVector hitLowEdge(hitPosition.GetX() - (hitWidth * 0.5f), 0, hitPosition.GetZ());
                    
                    const float highEdgeDistanceFromLine((extrapolatedEndPosition - extrapolatedStartPosition).GetCrossProduct(hitHighEdge - extrapolatedStartPosition).GetMagnitude());
                    const float lowEdgeDistanceFromLine((extrapolatedEndPosition - extrapolatedStartPosition).GetCrossProduct(hitLowEdge - extrapolatedStartPosition).GetMagnitude());

                    if ((highEdgeDistanceFromLine > m_furthestDistanceToLine) || (lowEdgeDistanceFromLine > m_furthestDistanceToLine))
                        continue;

                    float xOnLine(((hitPosition.GetZ() - extrapolatedStartPosition.GetZ()) / gradient) + extrapolatedStartPosition.GetX());

                    if (((hitHighEdge.GetX() > xOnLine) && (hitLowEdge.GetX() > xOnLine)) ||
                        ((hitHighEdge.GetX() < xOnLine) && (hitLowEdge.GetX() < xOnLine)))
                    {
                    
                        if (!((highEdgeDistanceFromLine < m_closestDistanceToLine) || (lowEdgeDistanceFromLine < m_closestDistanceToLine)))
                            continue;
                    }

                    ++hitsCollected;
                    hitPositionVector.push_back(hitPosition);
                    clusterToCaloHitListMap[pCluster].push_back(pCaloHit);
                }
            }
        }
        catch (const StatusCodeException &)
        {
            return;
        }

        ++count;
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

void TrackExtensionRefinementAlgorithm::CreateMainTrack(ClusterEndpointAssociation &clusterEndpointAssociation, const ClusterToCaloHitListMap &clusterToCaloHitListMap,
    const ClusterList *const pClusterList, ClusterVector &clusterVector, SlidingFitResultMapPair &slidingFitResultMapPair, ClusterList &consideredClusters) const
{
    const Cluster *pMainTrackCluster(clusterEndpointAssociation.GetMainTrackCluster());
    const CartesianVector &clusterMergePoint(clusterEndpointAssociation.IsEndUpstream() ?
        clusterEndpointAssociation.GetDownstreamMergePoint() : clusterEndpointAssociation.GetUpstreamMergePoint());

   
    ////////////////
    ClusterList originalTrack({pMainTrackCluster});
    PandoraMonitoringApi::VisualizeClusters(this->GetPandora(), &originalTrack, "ORIGINAL TRACK", BLACK);
    ///////////////

    // Determine the shower clusters which contain hits that belong to the main track
    ClusterVector showerClustersToFragment;
    for (auto &entry : clusterToCaloHitListMap)
    {
        if (entry.first != pMainTrackCluster)
            showerClustersToFragment.push_back(entry.first);
    }

    std::sort(showerClustersToFragment.begin(), showerClustersToFragment.end(), LArClusterHelper::SortByNHits);

    ////////////////
    /*
    for (auto &entry : showerClustersToFragment)
    {
        ClusterList jam({entry});
        PandoraMonitoringApi::VisualizeClusters(this->GetPandora(), &jam, "SHOWER CLUSTER", VIOLET);
    }
    */
    ////////////////

    ClusterList remnantClusterList;
    pMainTrackCluster = RemoveOffAxisHitsFromTrack(pMainTrackCluster, clusterMergePoint, clusterEndpointAssociation.IsEndUpstream(),
        clusterToCaloHitListMap, remnantClusterList, *slidingFitResultMapPair.first, *slidingFitResultMapPair.second);
   
    ////////////////
    /*
    PandoraMonitoringApi::ViewEvent(this->GetPandora());
    ClusterList refinedCluster({pMainTrackCluster});
    PandoraMonitoringApi::VisualizeClusters(this->GetPandora(), &refinedCluster, "REFINED MAIN TRACK", BLACK);
    PandoraMonitoringApi::VisualizeClusters(this->GetPandora(), &remnantClusterList, "REMNANT CLUSTERS", VIOLET);
    PandoraMonitoringApi::ViewEvent(this->GetPandora());
    */
    ////////////////
   
    for (const Cluster *const pShowerCluster : showerClustersToFragment)
    {
        const CaloHitList &caloHitsToMerge(clusterToCaloHitListMap.at(pShowerCluster));
        this->AddHitsToMainTrack(pMainTrackCluster, pShowerCluster, caloHitsToMerge, clusterEndpointAssociation, remnantClusterList);
    }

    ////////////////
    /*
    ClusterList extendedCluster({pMainTrackCluster});
    PandoraMonitoringApi::VisualizeClusters(this->GetPandora(), &extendedCluster, "REFINED MAIN TRACK", BLACK);
    PandoraMonitoringApi::VisualizeClusters(this->GetPandora(), &remnantClusterList, "REMNANT CLUSTERS", VIOLET);
    PandoraMonitoringApi::ViewEvent(this->GetPandora());    
    */
    ////////////////   

    ClusterList createdClusters;
    this->ProcessRemnantClusters(remnantClusterList, pMainTrackCluster, pClusterList, createdClusters);

    ////////////////
    
    PandoraMonitoringApi::VisualizeClusters(this->GetPandora(), &createdClusters, "CREATED CLUSTERS", RED);
    ClusterList extendedCluster({pMainTrackCluster});
    PandoraMonitoringApi::VisualizeClusters(this->GetPandora(), &extendedCluster, "REFINED MAIN TRACK", BLACK);
    PandoraMonitoringApi::ViewEvent(this->GetPandora());     

    ////////////////

    //ATTN: Update references to pMainTrackCluster
    //IT IS REALLY IMPORTANT THAT THIS IS DONE FIRST I.E. DELETE NULL POINTERS FROM MAP BEFORE ANY ARE ADDED IN AS THEY CAN BECOME THE SAME
    
    // ATTN: Cleanup
    ClusterList modifiedClusters(showerClustersToFragment.begin(), showerClustersToFragment.end());
    modifiedClusters.push_back(clusterEndpointAssociation.GetMainTrackCluster());
    this->UpdateContainers(createdClusters, modifiedClusters, clusterVector, slidingFitResultMapPair);
    consideredClusters.push_back(pMainTrackCluster);
    
}

//------------------------------------------------------------------------------------------------------------------------------------------
/*
void TrackExtensionRefinementAlgorithm::UpdateAfterMainTrackModification(const Cluster *const pMainTrackCluster, ClusterEndpointAssociation &clusterEndpointAssociation,
    SlidingFitResultMapPair &slidingFitResultMapPair) const
{
    const Cluster *const pDeletedTrackCluster(clusterEndpointAssociation.GetMainTrackCluster());
    
    // REPLACE IN MICRO FIT MAP
    const TwoDSlidingFitResultMap::const_iterator microFitToDeleteIter(slidingFitResultMapPair.first->find(pDeletedTrackCluster));
    if (microFitToDeleteIter == slidingFitResultMapPair.first->end())
    {
        std::cout << "ISOBEL THIS IS BAD" << std::endl;
        throw STATUS_CODE_FAILURE;
    }

    slidingFitResultMapPair.first->erase(microFitToDeleteIter);

    // REPLACE IN MACRO FIT MAP
    const TwoDSlidingFitResultMap::const_iterator macroFitToDeleteIter(slidingFitResultMapPair.second->find(pDeletedTrackCluster));
    if (macroFitToDeleteIter == slidingFitResultMapPair.second->end())
    {
        std::cout << "ISOBEL THIS IS BAD" << std::endl;
        throw STATUS_CODE_FAILURE;
    }

    slidingFitResultMapPair.second->erase(macroFitToDeleteIter);


}
*/
//------------------------------------------------------------------------------------------------------------------------------------------

void TrackExtensionRefinementAlgorithm::ConsiderCluster(const ClusterEndpointAssociation &clusterAssociation, ClusterVector &clusterVector) const
{
    const ClusterVector::const_iterator clusterToDeleteIter(std::find(clusterVector.begin(), clusterVector.end(), clusterAssociation.GetMainTrackCluster()));
    
    if (clusterToDeleteIter == clusterVector.end())
    {
        std::cout << "LArTrackExtensionRefinement - Considered cluster not in ClusterVector" << std::endl;
        throw STATUS_CODE_NOT_FOUND;
    }

    clusterVector.erase(clusterToDeleteIter);
}

//------------------------------------------------------------------------------------------------------------------------------------------

bool TrackExtensionRefinementAlgorithm::AreExtrapolatedHitsGood(ClusterEndpointAssociation &clusterAssociation, const ClusterToCaloHitListMap &clusterToCaloHitListMap, const bool isHigherXBoundary) const
{
    const float m_boundaryTolerance(2.f);

    CaloHitVector extrapolatedHitVector;
    for (const auto &entry : clusterToCaloHitListMap)
        extrapolatedHitVector.insert(extrapolatedHitVector.begin(), entry.second.begin(), entry.second.end());

    // ATTN: SORTED FROM UPSTREAM -> DOWNSTREAM POINT
    std::sort(extrapolatedHitVector.begin(), extrapolatedHitVector.end(), SortByDistanceAlongLine(clusterAssociation.GetUpstreamMergePoint(), clusterAssociation.GetConnectingLineDirection()));
    
    ////////////////////
    ClusterList theJam({clusterAssociation.GetMainTrackCluster()});
    PandoraMonitoringApi::VisualizeClusters(this->GetPandora(), &theJam, "MAIN TRACK", BLUE);
    for (const CaloHit *const pCaloHit : extrapolatedHitVector)
    {
        const CartesianVector &hitPosition(pCaloHit->GetPositionVector());
        PandoraMonitoringApi::AddMarkerToVisualization(this->GetPandora(), &hitPosition, "EXTRAP", GREEN, 2);
    }
    PandoraMonitoringApi::ViewEvent(this->GetPandora());
    ///////////////    
    
    if (!this->IsExtrapolatedEndpointNearBoundary(extrapolatedHitVector, isHigherXBoundary, m_boundaryTolerance, clusterAssociation))
        return false;

    if (clusterToCaloHitListMap.empty())
        return true;

    const float averageHitSeparation(this->GetAverageHitSeparation(extrapolatedHitVector));
    std::cout << "averageHitSeparation: " << averageHitSeparation << std::endl;
    
    if (!this->IsTrackContinuous(clusterAssociation, extrapolatedHitVector, m_maxTrackGaps, m_lineSegmentLength))
    {
        std::cout << "GAP IN HIT VECTOR" << std::endl;
        PandoraMonitoringApi::ViewEvent(this->GetPandora());
        //consideredClusters.push_back(clusterAssociation.GetMainTrackCluster());                 
        return false;
    }

    return true;

}

//------------------------------------------------------------------------------------------------------------------------------------------

bool TrackExtensionRefinementAlgorithm::IsExtrapolatedEndpointNearBoundary(const CaloHitVector &extrapolatedHitVector, const bool isHigherXBoundary, const float boundaryTolerance, 
    ClusterEndpointAssociation &clusterAssociation) const
{
    const CartesianVector clusterMergePoint(clusterAssociation.IsEndUpstream() ? clusterAssociation.GetDownstreamMergePoint() : clusterAssociation.GetUpstreamMergePoint());
    const float nearestTPCBoundaryX(isHigherXBoundary ? m_tpcMaxXEdge : m_tpcMinXEdge);

    ////////////////////    
    ClusterList theCluster({clusterAssociation.GetMainTrackCluster()});
    PandoraMonitoringApi::VisualizeClusters(this->GetPandora(), &theCluster, "THE CLUSTER", BLACK);
    PandoraMonitoringApi::AddMarkerToVisualization(this->GetPandora(), &clusterMergePoint, "MERGE POINT", BLACK, 2);
    ////////////////////  
    
    if (extrapolatedHitVector.empty())
    {
        const float distanceFromTPCBoundary(std::fabs(clusterMergePoint.GetX() - nearestTPCBoundaryX));
        
        if (distanceFromTPCBoundary > boundaryTolerance)
        {
            std::cout << "MERGE POINT TOO FAR AWAY FROM BOUNDARY & NO HITS" << std::endl;
            PandoraMonitoringApi::ViewEvent(this->GetPandora());
            return false;
        }
        else
        {
            std::cout << "MERGE POINT CLOSE TO BOUNDARY & NO HITS" << std::endl;
            PandoraMonitoringApi::ViewEvent(this->GetPandora());
            return true;
        }
    }

    const CartesianVector &closestPoint(clusterAssociation.IsEndUpstream() ? extrapolatedHitVector.back()->GetPositionVector() : extrapolatedHitVector.front()->GetPositionVector());
    const CartesianVector &furthestPoint(clusterAssociation.IsEndUpstream() ? extrapolatedHitVector.front()->GetPositionVector() : extrapolatedHitVector.back()->GetPositionVector());
    
    const float distanceFromTPCBoundary(std::fabs(furthestPoint.GetX() - nearestTPCBoundaryX));

    ////////////////////  
    PandoraMonitoringApi::AddMarkerToVisualization(this->GetPandora(), &closestPoint, "CLOSEST POINT", RED, 2);
    PandoraMonitoringApi::AddMarkerToVisualization(this->GetPandora(), &furthestPoint, "FURTHEST POINT", RED, 2);
    std::cout << "distanceFromTPCBoundary: " << distanceFromTPCBoundary << std::endl;
    std::cout << "distance between merge and closest" << (clusterMergePoint - closestPoint).GetMagnitude() << std::endl;
    ////////////////////    
    
    if ((distanceFromTPCBoundary > boundaryTolerance) || ((clusterMergePoint - closestPoint).GetMagnitude() > 2.f))
    {
        std::cout << "failed cuts" << std::endl;
        PandoraMonitoringApi::ViewEvent(this->GetPandora());
        return false;
    }

    PandoraMonitoringApi::ViewEvent(this->GetPandora());
    
    clusterAssociation.IsEndUpstream() ? clusterAssociation.SetUpstreamMergePoint(furthestPoint) : clusterAssociation.SetDownstreamMergePoint(furthestPoint);

    return true;
}

//------------------------------------------------------------------------------------------------------------------------------------------

float TrackExtensionRefinementAlgorithm::GetAverageHitSeparation(const CaloHitVector &orderedCaloHitVector) const
{
    const CaloHit *pPreviousCaloHit(orderedCaloHitVector.front());

    float separationSum(0.f);
    for (const CaloHit *const pCaloHit : orderedCaloHitVector)
    {
        if (pCaloHit == pPreviousCaloHit)
            continue;

        separationSum += std::sqrt(pCaloHit->GetPositionVector().GetDistanceSquared(pPreviousCaloHit->GetPositionVector()));
        pPreviousCaloHit = pCaloHit;
    }

    return (separationSum / orderedCaloHitVector.size());
}

//------------------------------------------------------------------------------------------------------------------------------------------

void TrackExtensionRefinementAlgorithm::InitialiseGeometry()
{
    const Pandora *pPrimaryPandoraInstance;
    try
    {
        pPrimaryPandoraInstance = MultiPandoraApi::GetPrimaryPandoraInstance(&this->GetPandora());
    }
    catch (const StatusCodeException &)
    {
        pPrimaryPandoraInstance = &this->GetPandora();
    }
           
    m_detectorMinXEdge = std::numeric_limits<float>::max();
    m_detectorMaxXEdge = -std::numeric_limits<float>::max();
    m_pLArTPC = &this->GetPandora().GetGeometry()->GetLArTPC();
    
    const LArTPCMap &larTPCMap(pPrimaryPandoraInstance->GetGeometry()->GetLArTPCMap());
    
    for (const LArTPCMap::value_type &mapEntry : larTPCMap)
    {
        const LArTPC *const pSubLArTPC(mapEntry.second);
        m_detectorMinXEdge = std::min(m_detectorMinXEdge, pSubLArTPC->GetCenterX() - 0.5f * pSubLArTPC->GetWidthX());
        m_detectorMaxXEdge = std::max(m_detectorMaxXEdge, pSubLArTPC->GetCenterX() + 0.5f * pSubLArTPC->GetWidthX());

        //ATTN: Child & parent pandora instance TPCs have different addresses
        if (std::fabs(pSubLArTPC->GetCenterX() - m_pLArTPC->GetCenterX()) < std::numeric_limits<float>::epsilon())
            m_pLArTPC = pSubLArTPC;
    }

    m_tpcMinXEdge = m_pLArTPC->GetCenterX() - (m_pLArTPC->GetWidthX() * 0.5f);
    m_tpcMaxXEdge = m_pLArTPC->GetCenterX() + (m_pLArTPC->GetWidthX() * 0.5f);

    float &cpaXBoundary((m_pLArTPC->IsDriftInPositiveX() ? m_tpcMinXEdge : m_tpcMaxXEdge));
    
    const LArTPC *const pNeighboughTPC(&LArStitchingHelper::FindClosestTPC(*pPrimaryPandoraInstance, *m_pLArTPC, !m_pLArTPC->IsDriftInPositiveX()));
    const float gapSizeX(std::fabs(pNeighboughTPC->GetCenterX() - m_pLArTPC->GetCenterX()) - (pNeighboughTPC->GetWidthX() * 0.5f) - (m_pLArTPC->GetWidthX() * 0.5f));

    cpaXBoundary += gapSizeX * (m_pLArTPC->IsDriftInPositiveX() ? -0.5f : 0.5f);

    const CartesianVector lowXPoint(m_tpcMinXEdge, m_pLArTPC->GetCenterY(), m_pLArTPC->GetCenterZ());
    const CartesianVector highXPoint(m_tpcMaxXEdge, m_pLArTPC->GetCenterY(), m_pLArTPC->GetCenterZ());

    PandoraMonitoringApi::AddMarkerToVisualization(this->GetPandora(), &lowXPoint, "lowXPoint", RED, 2);
    PandoraMonitoringApi::AddMarkerToVisualization(this->GetPandora(), &highXPoint, "highXPoint", BLUE, 2);
    PandoraMonitoringApi::ViewEvent(this->GetPandora());
    
}




//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode TrackExtensionRefinementAlgorithm::ReadSettings(const TiXmlHandle xmlHandle)
{
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "GrowingFitInitialLength", m_growingFitInitialLength));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "GrowingFitSegmentLength", m_growingFitSegmentLength));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "FurthestDistanceToLine", m_furthestDistanceToLine));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "ClosestDistanceToLine", m_closestDistanceToLine));    

    return TrackRefinementBaseAlgorithm::ReadSettings(xmlHandle);
}

//------------------------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------------------------

bool TrackExtensionRefinementAlgorithm::SortByDistanceToTPCBoundary::operator() (const Cluster *const pLhs, const Cluster *const pRhs)
{
    const unsigned int lhsInnerPseudoLayer(pLhs->GetInnerPseudoLayer()), lhsOuterPseudoLayer(pLhs->GetOuterPseudoLayer());
    const float lhsInnerX(pLhs->GetCentroid(lhsInnerPseudoLayer).GetX()), lhsOuterX(pLhs->GetCentroid(lhsOuterPseudoLayer).GetX());

    const unsigned int rhsInnerPseudoLayer(pRhs->GetInnerPseudoLayer()), rhsOuterPseudoLayer(pRhs->GetOuterPseudoLayer());
    const float rhsInnerX(pRhs->GetCentroid(rhsInnerPseudoLayer).GetX()), rhsOuterX(pRhs->GetCentroid(rhsOuterPseudoLayer).GetX());       

    const float lhsFurthestDistance(std::max(std::fabs(lhsInnerX - m_tpcXBoundary), std::fabs(lhsOuterX - m_tpcXBoundary)));
    const float rhsFurthestDistance(std::max(std::fabs(rhsInnerX - m_tpcXBoundary), std::fabs(rhsOuterX - m_tpcXBoundary)));

    // ATTN: Order from furthest away to closest
    return (lhsFurthestDistance > rhsFurthestDistance);
}


    
}// namespace lar_content



/*
            /////////////////////

            std::cout  << "\033[31m" << "After hits collected..." << "\033[0m" << std::endl;
            CaloHitVector extrapolatedCaloHitVector;
            for (const auto &entry : clusterToCaloHitListMap)
                extrapolatedCaloHitVector.insert(extrapolatedCaloHitVector.begin(), entry.second.begin(), entry.second.end());
            
            for (auto &entry : extrapolatedCaloHitVector)
            {
                const CartesianVector &hitPosition(entry->GetPositionVector());
                PandoraMonitoringApi::AddMarkerToVisualization(this->GetPandora(), &hitPosition, "EXTRAPOLATED HIT", GREEN, 2);
            }

            //////////////////////////////
*/
