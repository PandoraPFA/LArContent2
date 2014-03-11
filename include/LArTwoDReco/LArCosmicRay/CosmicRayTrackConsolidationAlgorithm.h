/**
 *  @file   LArContent/include/LArTwoDReco/LArCosmicRay/CosmicRayTrackConsolidationAlgorithm.h
 *
 *  @brief  Header file for the cosmic ray track consolidation algorithm class.
 *
 *  $Log: $
 */
#ifndef LAR_COSMIC_RAY_TRACK_CONSOLIDATION_ALGORITHM_H
#define LAR_COSMIC_RAY_TRACK_CONSOLIDATION_ALGORITHM_H 1

#include "Pandora/Algorithm.h"

namespace lar
{

/**
 *  @brief  CosmicRayTrackConsolidationAlgorithm class
 */
class CosmicRayTrackConsolidationAlgorithm : public pandora::Algorithm
{
public:
    /**
     *  @brief  Factory class for instantiating algorithm
     */
    class Factory : public pandora::AlgorithmFactory
    {
    public:
        pandora::Algorithm *CreateAlgorithm() const;
    };

private:
    pandora::StatusCode Run();
    pandora::StatusCode ReadSettings(const pandora::TiXmlHandle xmlHandle);


    float m_minTrackLength;

};

//------------------------------------------------------------------------------------------------------------------------------------------

inline pandora::Algorithm *CosmicRayTrackConsolidationAlgorithm::Factory::CreateAlgorithm() const
{
    return new CosmicRayTrackConsolidationAlgorithm();
}

} // namespace lar

#endif // #ifndef LAR_COSMIC_RAY_TRACK_CONSOLIDATION_ALGORITHM_H
