/**
 *  @file   larpandoracontent/LArThreeDReco/LArCosmicRay/TwoViewDeltaRayMergeTool.h
 *
 *  @brief  Header file for the two view delta ray merge tool class
 *
 *  $Log: $
 */
#ifndef TWO_VIEW_DELTA_RAY_MERGE_TOOL_H
#define TWO_VIEW_DELTA_RAY_MERGE_TOOL_H 1

#include "larpandoracontent/LArThreeDReco/LArCosmicRay/TwoViewDeltaRayMatchingAlgorithm.h"

namespace lar_content
{

/**
 *  @brief  DeltaRayMergeTool class
 */
class TwoViewDeltaRayMergeTool : public DeltaRayMatrixTool
{
public:
    typedef std::vector<pandora::HitType> HitTypeVector;
    /**
     *  @brief  Default constructor
     */
    TwoViewDeltaRayMergeTool();

private:
    bool Run(TwoViewDeltaRayMatchingAlgorithm *const pAlgorithm, MatrixType &overlapMatrix);    
    pandora::StatusCode ReadSettings(const pandora::TiXmlHandle xmlHandle);

    void MakeMerges(TwoViewDeltaRayMatchingAlgorithm *const pAlgorithm, MatrixType &overlapMatrix, bool &mergesMade) const;
    
    bool PickOutGoodMatches(TwoViewDeltaRayMatchingAlgorithm *const pAlgorithm, const MatrixType::ElementList &elementList) const;

};

} // namespace lar_content

#endif // #ifndef TWO_VIEW_DELTA_RAY_MERGE_TOOL_H