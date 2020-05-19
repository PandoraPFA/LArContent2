/**
 *  @file   larpandoracontent/LArControlFlow/NeutrinoSliceSelectionTool.cc
 *
 *  @brief  Implementation of the neutrino id tool class.
 *
 *  $Log: $
 */

#include "Pandora/AlgorithmHeaders.h"

#include "larpandoracontent/LArControlFlow/NeutrinoSliceSelectionTool.h"

#include "larpandoracontent/LArHelpers/LArMCParticleHelper.h"

using namespace pandora;

namespace lar_content
{

NeutrinoSliceSelectionTool::NeutrinoSliceSelectionTool()
{
}

//------------------------------------------------------------------------------------------------------------------------------------------

bool NeutrinoSliceSelectionTool::IsTarget(const MCParticle *const mcParticle) const
{
    return LArMCParticleHelper::IsNeutrino(mcParticle);
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode NeutrinoSliceSelectionTool::ReadSettings(const TiXmlHandle xmlHandle)
{
    return GenericSliceSelectionTool::ReadSettings(xmlHandle);
}

} // namespace lar_content