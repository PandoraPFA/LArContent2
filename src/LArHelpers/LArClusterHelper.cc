/**
 *  @file   LArContent/src/LArHelpers/LArClusterHelper.cc
 *
 *  @brief  Implementation of the cluster helper class.
 *
 *  $Log: $
 */

#include "Helpers/ClusterHelper.h"
#include "Helpers/XmlHelper.h"

#include "Pandora/PandoraSettings.h"

#include "LArHelpers/LArClusterHelper.h"

#include <algorithm>
#include <cmath>
#include <limits>

using namespace pandora;

namespace lar
{

HitType LArClusterHelper::GetClusterHitType(const Cluster *const pCluster)
{
    if (0 == pCluster->GetNCaloHits())
        throw StatusCodeException(STATUS_CODE_NOT_INITIALIZED);

    if (PandoraSettings::SingleHitTypeClusteringMode())
        return (*(pCluster->GetOrderedCaloHitList().begin()->second->begin()))->GetHitType();

    HitType hitType(CUSTOM);

    if (pCluster->ContainsHitType(TPC_VIEW_U))
    {
        if (CUSTOM == hitType) hitType = TPC_VIEW_U;
        else throw StatusCodeException(STATUS_CODE_FAILURE);
    }

    if (pCluster->ContainsHitType(TPC_VIEW_V))
    {
        if (CUSTOM == hitType) hitType = TPC_VIEW_V;
        else throw StatusCodeException(STATUS_CODE_FAILURE);
    }

    if (pCluster->ContainsHitType(TPC_VIEW_W))
    {
        if (CUSTOM == hitType) hitType = TPC_VIEW_W;
        else throw StatusCodeException(STATUS_CODE_FAILURE);
    }

    if (CUSTOM == hitType)
        throw StatusCodeException(STATUS_CODE_FAILURE);

    return hitType;
}

//------------------------------------------------------------------------------------------------------------------------------------------

float LArClusterHelper::GetLengthSquared(const Cluster *const pCluster)
{
    const OrderedCaloHitList &orderedCaloHitList(pCluster->GetOrderedCaloHitList());

    if (orderedCaloHitList.empty())
        throw StatusCodeException(STATUS_CODE_NOT_INITIALIZED);

    // ATTN: in 2D case, we will actually calculate the quadrature sum of deltaX and deltaU/V/W
    float minX(std::numeric_limits<float>::max()), maxX(-std::numeric_limits<float>::max());
    float minY(std::numeric_limits<float>::max()), maxY(-std::numeric_limits<float>::max());
    float minZ(std::numeric_limits<float>::max()), maxZ(-std::numeric_limits<float>::max());

    for (OrderedCaloHitList::const_iterator iter = orderedCaloHitList.begin(), iterEnd = orderedCaloHitList.end(); iter != iterEnd; ++iter)
    {
        for (CaloHitList::const_iterator hitIter = iter->second->begin(), hitIterEnd = iter->second->end(); hitIter != hitIterEnd; ++hitIter)
        {
            const CartesianVector &hitPosition((*hitIter)->GetPositionVector());
            minX = std::min(hitPosition.GetX(), minX);
            maxX = std::max(hitPosition.GetX(), maxX);
            minY = std::min(hitPosition.GetY(), minY);
            maxY = std::max(hitPosition.GetY(), maxY);
            minZ = std::min(hitPosition.GetZ(), minZ);
            maxZ = std::max(hitPosition.GetZ(), maxZ);
        }
    }

    const float deltaX(maxX - minX), deltaY(maxY - minY), deltaZ(maxZ - minZ);
    return (deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);
}

//------------------------------------------------------------------------------------------------------------------------------------------

float LArClusterHelper::GetLength(const Cluster *const pCluster)
{
    return std::sqrt(LArClusterHelper::GetLengthSquared(pCluster));
}

//------------------------------------------------------------------------------------------------------------------------------------------

float LArClusterHelper::GetEnergyFromLength(const Cluster *const pCluster)
{
    static const float dEdX(0.002f); // approximately 2 MeV/cm

    return (dEdX * LArClusterHelper::GetLength(pCluster));
}

//------------------------------------------------------------------------------------------------------------------------------------------

unsigned int LArClusterHelper::GetLayerSpan(const Cluster *const pCluster)
{
    return (1 + pCluster->GetOuterPseudoLayer() - pCluster->GetInnerPseudoLayer());
}

//------------------------------------------------------------------------------------------------------------------------------------------

float LArClusterHelper::GetLayerOccupancy(const Cluster *const pCluster)
{
    const unsigned int nOccupiedLayers(pCluster->GetOrderedCaloHitList().size());
    const unsigned int nLayers(1 + pCluster->GetOuterPseudoLayer() - pCluster->GetInnerPseudoLayer());

    if (nLayers > 0)
        return (static_cast<float>(nOccupiedLayers) / static_cast<float>(nLayers));

    return 0.f;
}

//------------------------------------------------------------------------------------------------------------------------------------------

float LArClusterHelper::GetLayerOccupancy(const Cluster *const pCluster1, const Cluster *const pCluster2)
{
    const unsigned int nOccupiedLayers(pCluster1->GetOrderedCaloHitList().size() + pCluster2->GetOrderedCaloHitList().size());
    const unsigned int nLayers(1 + std::max(pCluster1->GetOuterPseudoLayer(), pCluster2->GetOuterPseudoLayer()) -
        std::min(pCluster1->GetInnerPseudoLayer(), pCluster2->GetInnerPseudoLayer()));

    if (nLayers > 0)
        return (static_cast<float>(nOccupiedLayers) / static_cast<float>(nLayers));

    return 0.f;
}

//------------------------------------------------------------------------------------------------------------------------------------------

float LArClusterHelper::GetClosestDistance(const ClusterList &clusterList1, const ClusterList &clusterList2)
{
    if (clusterList1.empty() || clusterList2.empty())
        throw StatusCodeException(STATUS_CODE_NOT_FOUND);

    float closestDistance(std::numeric_limits<float>::max());

    for (ClusterList::const_iterator iter1 = clusterList1.begin(), iterEnd1 = clusterList1.end(); iter1 != iterEnd1; ++iter1)
    {
        const Cluster *pCluster1 = *iter1;
        const float thisDistance(LArClusterHelper::GetClosestDistance(pCluster1, clusterList2));
 
        if (thisDistance < closestDistance)
            closestDistance = thisDistance; 
    }

    return closestDistance;
}

//------------------------------------------------------------------------------------------------------------------------------------------

float LArClusterHelper::GetClosestDistance(const Cluster *const pCluster, const ClusterList &clusterList)
{
    if (clusterList.empty())
        throw StatusCodeException(STATUS_CODE_NOT_FOUND);

    float closestDistance(std::numeric_limits<float>::max());

    for (ClusterList::const_iterator iter = clusterList.begin(), iterEnd = clusterList.end(); iter != iterEnd; ++iter)
    {
        const Cluster *pTestCluster = *iter;
        const float thisDistance(LArClusterHelper::GetClosestDistance(pCluster, pTestCluster));

        if (thisDistance < closestDistance)
            closestDistance = thisDistance; 
    }

    return closestDistance;
}

//------------------------------------------------------------------------------------------------------------------------------------------

float LArClusterHelper::GetClosestDistance(const Cluster *const pCluster1, const Cluster *const pCluster2)
{
    return ClusterHelper::GetDistanceToClosestHit(pCluster1, pCluster2);
}

//------------------------------------------------------------------------------------------------------------------------------------------

float LArClusterHelper::GetClosestDistance(const CartesianVector &position, const Cluster *const pCluster)
{
    return (position - LArClusterHelper::GetClosestPosition(position, pCluster)).GetMagnitude();
}

//------------------------------------------------------------------------------------------------------------------------------------------

CartesianVector LArClusterHelper::GetClosestPosition(const CartesianVector &position, const Cluster *const pCluster)
{
    const OrderedCaloHitList &orderedCaloHitList(pCluster->GetOrderedCaloHitList());

    CaloHit *pClosestCaloHit(NULL);
    float closestDistanceSquared(std::numeric_limits<float>::max());

    for (OrderedCaloHitList::const_iterator iter = orderedCaloHitList.begin(), iterEnd = orderedCaloHitList.end(); iter != iterEnd; ++iter)
    {
        for (CaloHitList::const_iterator hitIter = iter->second->begin(), hitIterEnd = iter->second->end(); hitIter != hitIterEnd; ++hitIter)
        {
            CaloHit* pCaloHit = *hitIter;
            const float distanceSquared((pCaloHit->GetPositionVector() - position).GetMagnitudeSquared());

            if (distanceSquared < closestDistanceSquared)
            {
                closestDistanceSquared = distanceSquared;
                pClosestCaloHit = pCaloHit;
            }
        }
    }

    if (pClosestCaloHit)
        return pClosestCaloHit->GetPositionVector();

    throw StatusCodeException(STATUS_CODE_NOT_FOUND);
}

//------------------------------------------------------------------------------------------------------------------------------------------

void LArClusterHelper::GetClusterSpanXZ(const Cluster *const pCluster, CartesianVector &minimumCoordinate, CartesianVector &maximumCoordinate)
{
    const OrderedCaloHitList &orderedCaloHitList(pCluster->GetOrderedCaloHitList());

    float xmin(std::numeric_limits<float>::max());
    float ymin(std::numeric_limits<float>::max());
    float zmin(std::numeric_limits<float>::max());
    float xmax(-std::numeric_limits<float>::max());
    float ymax(-std::numeric_limits<float>::max());
    float zmax(-std::numeric_limits<float>::max());

    for (OrderedCaloHitList::const_iterator ochIter = orderedCaloHitList.begin(), ochIterEnd = orderedCaloHitList.end(); ochIter != ochIterEnd; ++ochIter)
    {
        for (CaloHitList::const_iterator hIter = ochIter->second->begin(), hIterEnd = ochIter->second->end(); hIter != hIterEnd; ++hIter)
        {
            const CaloHit *pCaloHit = *hIter;
            const CartesianVector &hit(pCaloHit->GetPositionVector());
            xmin = std::min(hit.GetX(), xmin);
            xmax = std::max(hit.GetX(), xmax);
            ymin = std::min(hit.GetY(), ymin);
            ymax = std::max(hit.GetY(), ymax);
            zmin = std::min(hit.GetZ(), zmin);
            zmax = std::max(hit.GetZ(), zmax);
        }
    }

    minimumCoordinate.SetValues(xmin, ymin, zmin);
    maximumCoordinate.SetValues(xmax, ymax, zmax);
}

//------------------------------------------------------------------------------------------------------------------------------------------

void LArClusterHelper::GetClusterSpanX(const Cluster *const pCluster, float &xmin, float &xmax)
{
    const OrderedCaloHitList &orderedCaloHitList(pCluster->GetOrderedCaloHitList());
    xmin = std::numeric_limits<float>::max();
    xmax = -std::numeric_limits<float>::max();

    for (OrderedCaloHitList::const_iterator ochIter = orderedCaloHitList.begin(), ochIterEnd = orderedCaloHitList.end(); ochIter != ochIterEnd; ++ochIter)
    {
        for (CaloHitList::const_iterator hIter = ochIter->second->begin(), hIterEnd = ochIter->second->end(); hIter != hIterEnd; ++hIter)
        {
            const CaloHit *pCaloHit = *hIter;
            const CartesianVector &hit(pCaloHit->GetPositionVector());
            xmin = std::min(hit.GetX(), xmin);
            xmax = std::max(hit.GetX(), xmax);
        }
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

void LArClusterHelper::GetClusterSpanZ(const Cluster *const pCluster, const float xmin, const float xmax, 
    float &zmin, float &zmax)
{
    if (xmin > xmax)
        throw StatusCodeException(STATUS_CODE_INVALID_PARAMETER);

    const OrderedCaloHitList &orderedCaloHitList(pCluster->GetOrderedCaloHitList());

    zmin = std::numeric_limits<float>::max();
    zmax = -std::numeric_limits<float>::max();

    bool foundHits(false);
    
    for (OrderedCaloHitList::const_iterator ochIter = orderedCaloHitList.begin(), ochIterEnd = orderedCaloHitList.end(); ochIter != ochIterEnd; ++ochIter)
    {
        for (CaloHitList::const_iterator hIter = ochIter->second->begin(), hIterEnd = ochIter->second->end(); hIter != hIterEnd; ++hIter)
        {
            const CaloHit *pCaloHit = *hIter;
            const CartesianVector &hit(pCaloHit->GetPositionVector());

            if (hit.GetX() < xmin || hit.GetX() > xmax)
                continue;

            zmin = std::min(hit.GetZ(), zmin);
            zmax = std::max(hit.GetZ(), zmax);
            foundHits = true;
        }
    }

    if (!foundHits)
        throw StatusCodeException(STATUS_CODE_NOT_FOUND);
}

//------------------------------------------------------------------------------------------------------------------------------------------

float LArClusterHelper::GetAverageZ(const Cluster *const pCluster, const float xmin, const float xmax)
{
    if (xmin > xmax)
        throw StatusCodeException(STATUS_CODE_INVALID_PARAMETER);

    const OrderedCaloHitList &orderedCaloHitList(pCluster->GetOrderedCaloHitList());

    float zsum(0.f);
    int count(0);
    
    for (OrderedCaloHitList::const_iterator ochIter = orderedCaloHitList.begin(), ochIterEnd = orderedCaloHitList.end(); ochIter != ochIterEnd; ++ochIter)
    {
        for (CaloHitList::const_iterator hIter = ochIter->second->begin(), hIterEnd = ochIter->second->end(); hIter != hIterEnd; ++hIter)
        {
            const CaloHit *pCaloHit = *hIter;
            const CartesianVector &hit(pCaloHit->GetPositionVector());

            if (hit.GetX() < xmin || hit.GetX() > xmax)
                continue;

            zsum += hit.GetZ();
            ++count;
        }
    }

    if (count == 0)
        throw StatusCodeException(STATUS_CODE_NOT_FOUND);

    return zsum / static_cast<float>(count);
}

//------------------------------------------------------------------------------------------------------------------------------------------

void LArClusterHelper::GetExtremalCoordinatesXZ(const Cluster *const pCluster, CartesianVector &innerCoordinate, CartesianVector &outerCoordinate)
{
    CaloHitList candidateList;
    const OrderedCaloHitList &orderedCaloHitList(pCluster->GetOrderedCaloHitList());

    // Transfer all inner layer hits
    OrderedCaloHitList::const_iterator iterInner = orderedCaloHitList.begin();
    for (CaloHitList::const_iterator hitIter = iterInner->second->begin(), hitIterEnd = iterInner->second->end(); hitIter != hitIterEnd; ++hitIter)
        candidateList.insert(*hitIter);

    // Transfer all outer layer hits
    OrderedCaloHitList::const_reverse_iterator iterOuter = orderedCaloHitList.rbegin();
    for (CaloHitList::const_iterator hitIter = iterOuter->second->begin(), hitIterEnd = iterOuter->second->end(); hitIter != hitIterEnd; ++hitIter)
        candidateList.insert(*hitIter);

    // Transfer the extremal hits in X (assume there are no ties)
    CaloHit *pFirstCaloHit(NULL);
    CaloHit *pSecondCaloHit(NULL);

    for (OrderedCaloHitList::const_iterator iter = orderedCaloHitList.begin(), iterEnd = orderedCaloHitList.end(); iter != iterEnd; ++iter)
    {
        for (CaloHitList::const_iterator hitIter = iter->second->begin(), hitIterEnd = iter->second->end(); hitIter != hitIterEnd; ++hitIter)
        {
            CaloHit *pCaloHit = *hitIter;

            if (NULL == pFirstCaloHit || pCaloHit->GetPositionVector().GetX() < pFirstCaloHit->GetPositionVector().GetX())
                pFirstCaloHit = pCaloHit;

            if (NULL == pSecondCaloHit || pCaloHit->GetPositionVector().GetX() > pSecondCaloHit->GetPositionVector().GetX())
                pSecondCaloHit = pCaloHit;
        }
    }

    if (NULL == pFirstCaloHit || NULL == pSecondCaloHit)
        throw StatusCodeException(STATUS_CODE_NOT_FOUND);

    candidateList.insert(pFirstCaloHit);
    candidateList.insert(pSecondCaloHit);

    // Find the two most separated hits
    float maxDistanceSquared(0.f);

    for (CaloHitList::const_iterator iterI = candidateList.begin(), iterEndI = candidateList.end(); iterI != iterEndI; ++iterI )
    {
        CaloHit *pCaloHitI = *iterI;

        for (CaloHitList::const_iterator iterJ = iterI, iterEndJ = candidateList.end(); iterJ != iterEndJ; ++iterJ )
        {
            CaloHit *pCaloHitJ = *iterJ;

            const float distanceSquared((pCaloHitI->GetPositionVector() - pCaloHitJ->GetPositionVector()).GetMagnitudeSquared());

            if (distanceSquared > maxDistanceSquared)
            {
                maxDistanceSquared = distanceSquared;
                pFirstCaloHit = pCaloHitI;
                pSecondCaloHit = pCaloHitJ;
            }
        }
    }

    // Set the inner and outer coordinates (Check Z first, then X in the event of a tie)
    const float deltaZ(pSecondCaloHit->GetPositionVector().GetZ() - pFirstCaloHit->GetPositionVector().GetZ());
    const float deltaX(pSecondCaloHit->GetPositionVector().GetX() - pFirstCaloHit->GetPositionVector().GetX());

    if ((deltaZ > 0.f) || ((std::fabs(deltaZ) < std::numeric_limits<float>::epsilon()) && (deltaX > 0.f)))
    {
        innerCoordinate = pFirstCaloHit->GetPositionVector();
        outerCoordinate = pSecondCaloHit->GetPositionVector();
    }
    else
    {
        innerCoordinate = pSecondCaloHit->GetPositionVector();
        outerCoordinate = pFirstCaloHit->GetPositionVector();
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

bool LArClusterHelper::SortByInnerLayer(const Cluster *const pLhs, const Cluster *const pRhs)
{
    const unsigned int innerLayerLhs(pLhs->GetInnerPseudoLayer());
    const unsigned int innerLayerRhs(pRhs->GetInnerPseudoLayer());

    if( innerLayerLhs != innerLayerRhs )
      return (innerLayerLhs < innerLayerRhs);

    // Use SortByNOccupiedLayers method to resolve ties
    return SortByNOccupiedLayers(pLhs,pRhs);
}

//------------------------------------------------------------------------------------------------------------------------------------------

bool LArClusterHelper::SortByNOccupiedLayers(const Cluster *const pLhs, const Cluster *const pRhs)
{
    const unsigned int nOccupiedLayersLhs(pLhs->GetOrderedCaloHitList().size());
    const unsigned int nOccupiedLayersRhs(pRhs->GetOrderedCaloHitList().size());

    if (nOccupiedLayersLhs != nOccupiedLayersRhs)
    return (nOccupiedLayersLhs > nOccupiedLayersRhs);

    const unsigned int layerSpanLhs(pLhs->GetOuterPseudoLayer() - pLhs->GetInnerPseudoLayer());
    const unsigned int layerSpanRhs(pRhs->GetOuterPseudoLayer() - pRhs->GetInnerPseudoLayer());

    if (layerSpanLhs != layerSpanRhs)
        return (layerSpanLhs > layerSpanRhs);

    return (pLhs->GetHadronicEnergy() > pRhs->GetHadronicEnergy());
}

//------------------------------------------------------------------------------------------------------------------------------------------

bool LArClusterHelper::SortByNHits(const Cluster *const pLhs, const Cluster *const pRhs)
{
    const unsigned int nHitsLhs(pLhs->GetNCaloHits());
    const unsigned int nHitsRhs(pRhs->GetNCaloHits());

    if (nHitsLhs != nHitsRhs)
        return (nHitsLhs > nHitsRhs);

    const unsigned int layerSpanLhs(pLhs->GetOuterPseudoLayer() - pLhs->GetInnerPseudoLayer());
    const unsigned int layerSpanRhs(pRhs->GetOuterPseudoLayer() - pRhs->GetInnerPseudoLayer());

    if (layerSpanLhs != layerSpanRhs)
        return (layerSpanLhs > layerSpanRhs);

    return (pLhs->GetHadronicEnergy() > pRhs->GetHadronicEnergy());
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode LArClusterHelper::ReadSettings(const TiXmlHandle /*xmlHandle*/)
{
    return STATUS_CODE_SUCCESS;
}

} // namespace lar
