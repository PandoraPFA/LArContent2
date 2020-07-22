/**
 *  @file   larpandoracontent/LArTwoDReco/LArCosmicRay/CosmicRayTrackRefinementBaseAlgorithm.h
 *
 *  @brief  Header file for the cosmic ray track refinement base class.
 *
 *  $Log: $
 */
#ifndef LAR_COSMIC_RAY_TRACK_REFINEMENT_BASE_ALGORITHM_H
#define LAR_COSMIC_RAY_TRACK_REFINEMENT_BASE_ALGORITHM_ALGORITHM_H 1

#include "Pandora/Algorithm.h"

#include "larpandoracontent/LArObjects/LArTwoDSlidingFitResult.h"

namespace lar_content
{

/**
 *  @brief CosmicRayTrackRefinementBaseAlgorithm class
 */
class CosmicRayTrackRefinementBaseAlgorithm : public pandora::Algorithm
{
public:
    
    /**
     *  @brief  Default constructor
     */
    CosmicRayTrackRefinementBaseAlgorithm();

protected:
    /**
     *  @brief  ClusterAssociation class
     */
    class ClusterAssociation
    {
    public:
        /**
         *  @brief  Default constructor
         */
        ClusterAssociation();

        /**
         *  @brief  Constructor
         *
         *  @param  pUpstreamCluster the upstream cluster of the two associated clusters
         *  @param  pDownstreamCluster the downstream cluster of the two associated clusters
         *  @param  upstreamMergePoint the upstream cluster point to be used in the merging process
         *  @param  upstreamMergeDirection the upstream cluster direction at the upstream merge point
         *  @param  downstreamMergePoint the downstream cluster point to be used in the merging process
         *  @param  downstreamMergeDirection the downstream cluster direction at the downstream merge point
         */
        ClusterAssociation(const pandora::Cluster *const pUpstreamCluster, const pandora::Cluster *const pDownstreamCluster, const pandora::CartesianVector &upstreamMergePoint,
            const pandora::CartesianVector &upstreamMergeDirection, const pandora::CartesianVector &downstreamMergePoint, const pandora::CartesianVector &downstreamMergeDirection);

        /**
         *  @brief  Returns the upstream cluster address
         *
         *  @return  Cluster the address of the upstream cluster
         */
        const pandora::Cluster *GetUpstreamCluster() const;

        /**
         *  @brief  Returns the downstream cluster address
         *
         *  @return  Cluster the address of the downstream cluster
         */
        const pandora::Cluster *GetDownstreamCluster() const;

        /**
         *  @brief  Returns the upstream cluster merge point
         *
         *  @return  CartesianVector the merge point of the upstream cluster
         */
        const pandora::CartesianVector GetUpstreamMergePoint() const;

        /**
         *  @brief  Returns the upstream cluster direction at the upstream merge point
         *
         *  @return  CartesianVector the direction at the merge point of the upstream cluster
         */        
        const pandora::CartesianVector GetUpstreamMergeDirection() const;

        /**
         *  @brief  Returns the downstream cluster merge point
         *
         *  @return  CartesianVector the merge point of the downstream cluster
         */
        const pandora::CartesianVector GetDownstreamMergePoint() const;

        /**
         *  @brief  Returns the downstream cluster direction at the downstream merge point
         *
         *  @return  CartesianVector the direction at the merge point of the downstream cluster
         */        
        const pandora::CartesianVector GetDownstreamMergeDirection() const;

        /**
         *  @brief  Returns the unit vector of the line connecting the upstream and downstream merge points (upstream -> downstream)
         *
         *  @return  CartesianVector the unit displacement vector from the upstream merge point to the downstream merge point
         */           
        const pandora::CartesianVector GetConnectingLineDirection() const;
        
    private:
        const pandora::Cluster     *m_pUpstreamCluster;            ///< The upstream cluster of the two associated clusters         
        const pandora::Cluster     *m_pDownstreamCluster;          ///< The downstream cluster of the two associated clusters
        pandora::CartesianVector    m_upstreamMergePoint;          ///< The upstream cluster point to be used in the merging process
        pandora::CartesianVector    m_upstreamMergeDirection;      ///< The upstream cluster direction at the upstream merge point (points in the direction of the downstream cluster)
        pandora::CartesianVector    m_downstreamMergePoint;        ///< The downstream cluster point to be used in the merging process
        pandora::CartesianVector    m_downstreamMergeDirection;    ///< The downstream cluster direction at the downstream merge point (points in the direction of the upstream cluster)
        pandora::CartesianVector    m_connectingLineDirection;     ///< The unit vector of the line connecting the upstream and downstream merge points (upstream -> downstream)
    };

    typedef std::unordered_map<const pandora::Cluster*, pandora::CaloHitList> ClusterToCaloHitListMap;
    typedef std::pair<TwoDSlidingFitResultMap*, TwoDSlidingFitResultMap*> SlidingFitResultMapPair;    
    
    virtual pandora::StatusCode Run() = 0;
    virtual pandora::StatusCode ReadSettings(const pandora::TiXmlHandle xmlHandle) = 0;

    /**
     *  @brief  Get the merging coordinate and direction for an input cluster with respect to an associated cluster
     *
     *  @param  currentMicroFitResult the local TwoDSlidingFitResult of the cluster
     *  @param  currentMacroFitResult the global TwoDSlidingFitResult of the cluster
     *  @param  associatedMacroFitReult the global TwoDSlidingFitResult of the associated cluster
     *  @param  isUpstream whether the cluster is the upstream cluster
     *  @param  currentMergePosition the merge position of the cluster
     *  @param  currentMergeDirection the merge direction of the cluster
     *
     *  @return bool whether it was possible to find a suitable merge position
     */
    bool GetClusterMergingCoordinates(const TwoDSlidingFitResult &currentMicroFitResult, const TwoDSlidingFitResult &currentMacroFitResult,
        const TwoDSlidingFitResult &associatedMacroFitResult, const bool isUpstream, pandora::CartesianVector &currentMergePosition,
        pandora::CartesianVector &currentMergeDirection) const;

    void GetExtrapolatedCaloHits(const ClusterAssociation &clusterAssociation, const pandora::ClusterList *const pClusterList,
        ClusterToCaloHitListMap &clusterToCaloHitListMap) const;

    void GetExtrapolatedCaloHits(const pandora::CartesianVector &upstreamPoint, const pandora::CartesianVector &downstreamPoint,
        const pandora::CartesianVector &connectingLineDirection, const pandora::ClusterList *const pClusterList, ClusterToCaloHitListMap &clusterToCaloHitListMap) const;

    /**
     *  @brief  Remove any hits in the upstream/downstream cluster that lie off of the main track axis (i.e. clustering errors)
     *
     *  @param  pCluster the input cluster
     *  @param  splitPosition the position after which hits are considered for removal
     *  @param  isUpstream whether the input cluster is the upstream cluster
     *  @param  clusterToCaloHitListMap the map [parent cluster -> list of hits which belong to the main track]
     *  @param  microSlidingFitResultMap the mapping [cluster -> TwoDSlidingFitResult] where fits correspond to local gradients
     *  @param  macroSlidingFitResultMap the mapping [cluster -> TwoDSlidingFitResult] where fits correspond to global cluster gradients
     *  @param  clusterVector the vector of 'relevant' clusters
     *
     *  @return Cluster the address of the (possibly) modified cluster
     */
    const pandora::Cluster *RemoveOffAxisHitsFromTrack(const pandora::Cluster *const pCluster, const pandora::CartesianVector &splitPosition, const bool isUpstream,
        const ClusterToCaloHitListMap &clusterToCaloHitListMap, pandora::ClusterList &remnantClusterList, TwoDSlidingFitResultMap &microSlidingFitResultMap,
        TwoDSlidingFitResultMap &macroSlidingFitResultMap) const;

    /**
     *  @brief  Remove the hits from a shower cluster that belong to the main track and add them into the main track cluster
     *
     *  @param  pShowerCluster the input shower cluster
     *  @param  pMainTrackCluster the main track cluster
     *  @param  caloHitsToMerge the list of calo hits to remove from the shower cluster
     *  @param  clusterAssociation the clusterAssociation
     *  @param  remnantClusterList the input list to store the remnant clusters
     */
    void AddHitsToMainTrack(const pandora::Cluster *const pShowerCluster, const pandora::Cluster *const pMainTrackCluster, const pandora::CaloHitList &caloHitsToMerge,
        const ClusterAssociation &clusterAssociation, pandora::ClusterList &remnantClusterList) const;

    /**
     *  @brief  Process the remnant clusters separating those that stradle the main track
     *
     *  @param  remnantClusterList the list of remnant clusters
     *  @param  pMainTrackCluster the main track cluster
     *  @param  pClusterList the list of all clusters
     */
    void ProcessRemnantClusters(const pandora::ClusterList &remnantClusterList, const pandora::Cluster *const pMainTrackCluster, const pandora::ClusterList *const pClusterList, pandora::ClusterList &createdClusters) const;

    /**
     *  @brief  Add a cluster to the nearest cluster satisfying separation distance thresholds
     *
     *  @param  pClusterToMerge the cluster to merge
     *  @param  pMainTrackCluster the main track cluster
     *  @param  pClusterList the list of all clusters
     */
    bool AddToNearestCluster(const pandora::Cluster *const pClusterToMerge, const pandora::Cluster *const pClusterToEnlarge, const pandora::ClusterList *const pClusterList) const;

    /**
     *  @brief  Whether a remnant cluster is considered to be disconnected and therefore should undergo further fragmentation
     *
     *  @param  pRemnantCluster the input remnant cluster
     */
    bool IsClusterRemnantDisconnected(const pandora::Cluster *const pRemnantCluster) const;

    /**
     *  @brief  Fragment a cluster using simple hit separation logic
     *
     *  @param  pRemnantCluster the input remnant cluster to fragment
     *  @param  fragmentedClusterList the list of created clusters
     */
    void FragmentRemnantCluster(const pandora::Cluster *const pRemnantCluster, pandora::ClusterList &fragmentedClusterList) const;    


    void InitialiseContainers(const pandora::ClusterList *pClusterList, pandora::ClusterVector &clusterVector, SlidingFitResultMapPair &slidingFitResultMapPair) const;

    void UpdateContainers(const pandora::ClusterVector &clustersToDelete, const pandora::ClusterList &clustersToAdd, pandora::ClusterVector &clusterVector, SlidingFitResultMapPair &slidingFitResultMapPair) const;


//------------------------------------------------------------------------------------------------------------------------------------------    

    unsigned int m_microSlidingFitWindow;
    unsigned int m_macroSlidingFitWindow;
    float m_stableRegionClusterFraction;
    float m_mergePointMinCosAngleDeviation;
    float m_distanceFromLine;
    float          m_minHitFractionForHitRemoval;           ///< The threshold fraction of hits to be removed from the cluster for hit removal to proceed
    float m_maxDistanceFromMainTrack;
    float m_maxHitDistanceFromCluster;
    float m_maxHitSeparationForConnectedCluster;
    

    
};

//------------------------------------------------------------------------------------------------------------------------------------------

inline const pandora::Cluster *CosmicRayTrackRefinementBaseAlgorithm::ClusterAssociation::GetUpstreamCluster() const
{
    return m_pUpstreamCluster;
}
    
//------------------------------------------------------------------------------------------------------------------------------------------

inline const pandora::Cluster *CosmicRayTrackRefinementBaseAlgorithm::ClusterAssociation::GetDownstreamCluster() const
{
    return m_pDownstreamCluster;
}

//------------------------------------------------------------------------------------------------------------------------------------------

inline const pandora::CartesianVector CosmicRayTrackRefinementBaseAlgorithm::ClusterAssociation::GetUpstreamMergePoint() const
{
    return m_upstreamMergePoint;
}

//------------------------------------------------------------------------------------------------------------------------------------------

inline const pandora::CartesianVector CosmicRayTrackRefinementBaseAlgorithm::ClusterAssociation::GetUpstreamMergeDirection() const
{
    return m_upstreamMergeDirection;
}

//------------------------------------------------------------------------------------------------------------------------------------------

inline const pandora::CartesianVector CosmicRayTrackRefinementBaseAlgorithm::ClusterAssociation::GetDownstreamMergePoint() const
{
    return m_downstreamMergePoint;
}

//------------------------------------------------------------------------------------------------------------------------------------------

inline const pandora::CartesianVector CosmicRayTrackRefinementBaseAlgorithm::ClusterAssociation::GetDownstreamMergeDirection() const
{
    return m_downstreamMergeDirection;
}

//------------------------------------------------------------------------------------------------------------------------------------------

inline const pandora::CartesianVector CosmicRayTrackRefinementBaseAlgorithm::ClusterAssociation::GetConnectingLineDirection() const
{
    return m_connectingLineDirection;
} 
    
} // namespace lar_content

#endif // #ifndef LAR_COSMIC_RAY_TRACK_REFINEMENT_BASE_ALGORITHM_H