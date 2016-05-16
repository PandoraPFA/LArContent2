/**
 *  @file   ExampleContent/src/VertexClusteringTools/VertexClusteringTool.cc
 * 
 *  @brief  Implementation of the example algorithm tool class.
 * 
 *  $Log: $
 */

#include "Pandora/AlgorithmHeaders.h"

#include "LArVertex/VertexClusteringTool.h"

using namespace pandora;

namespace lar_content
{

VertexClusteringTool::VertexClusteringTool() :
    m_maxVertexToCentroidDistance(5.f),
    m_removeSmallClusters(false)
{
}

//------------------------------------------------------------------------------------------------------------------------------------------

bool VertexClusteringTool::SortVerticesByZ(const pandora::Vertex *const pLhs, const pandora::Vertex *const pRhs)
{
    const CartesianVector deltaPosition(pRhs->GetPosition() - pLhs->GetPosition());

    if (std::fabs(deltaPosition.GetZ()) > std::numeric_limits<float>::epsilon())
        return (deltaPosition.GetZ() > std::numeric_limits<float>::epsilon());

    if (std::fabs(deltaPosition.GetX()) > std::numeric_limits<float>::epsilon())
        return (deltaPosition.GetX() > std::numeric_limits<float>::epsilon());

    // ATTN No way to distinguish between vertices if still have a tie in y coordinate
    return (deltaPosition.GetY() > std::numeric_limits<float>::epsilon());
}

//------------------------------------------------------------------------------------------------------------------------------------------

std::vector<const VertexList*> VertexClusteringTool::ClusterVertices(const VertexList &vertexList)
{
    if (this->GetPandora().GetSettings()->ShouldDisplayAlgorithmInfo())
       std::cout << "----> Running Algorithm Tool: " << this << ", " << this->GetType() << std::endl;

    //-------------------------------------------------------------------------------------------------

    std::vector<const Vertex*> sortedVertexVector;
    for (const Vertex *const pVertex : vertexList)
        sortedVertexVector.push_back(pVertex);

    std::sort(sortedVertexVector.begin(), sortedVertexVector.end(), SortVerticesByZ);

    VertexClusterList vertexClusterList;
    VertexList usedVertices;
    
    VertexCluster *const pVertexClusterSeed = new VertexCluster();
    
    for (const Vertex *const pVertex : sortedVertexVector)
    {
        if (usedVertices.count(pVertex) == 1)
            continue;
        
        CartesianVector currentClusterCentroid(0., 0., 0.);

        if (pVertexClusterSeed->GetVertexList().size() == 0)
            currentClusterCentroid = pVertex->GetPosition();
        else
            currentClusterCentroid = pVertexClusterSeed->GetCentroidPosition();
        
        if ((currentClusterCentroid - (pVertex->GetPosition())).GetMagnitude() < m_maxVertexToCentroidDistance)
        {
            pVertexClusterSeed->AddVertex(pVertex);
            usedVertices.insert(pVertex);
        }
        else
        {
            VertexCluster *const pNewVertexCluster = new VertexCluster(*pVertexClusterSeed);
            vertexClusterList.push_back(pNewVertexCluster);
            pVertexClusterSeed->ClearVertexCluster();
        }
        
    }

    std::vector<const VertexList*> outputVertexListVector;
    
    if (m_removeSmallClusters == true)
        this->RemoveSmallClusters(vertexClusterList);
    
    for (VertexCluster* pVertexCluster : vertexClusterList)
        outputVertexListVector.push_back(&(pVertexCluster->GetVertexList()));

    return outputVertexListVector;  

}

//------------------------------------------------------------------------------------------------------------------------------------------

pandora::CartesianVector VertexClusteringTool::VertexCluster::GetCentroidPosition() const
{
    if (this->GetVertexList().empty())
        throw pandora::StatusCodeException(pandora::STATUS_CODE_NOT_INITIALIZED);

    pandora::CartesianVector centroid(0.f, 0.f, 0.f);

    for (const pandora::Vertex *const pVertex : this->GetVertexList())
        centroid += pVertex->GetPosition();

    centroid *= static_cast<float>(1.f / this->GetVertexList().size());
    return centroid;
}

//------------------------------------------------------------------------------------------------------------------------------------------

void VertexClusteringTool::RemoveSmallClusters(VertexClusterList &vertexClusterList)
{
    for (VertexClusterList::const_iterator iter = vertexClusterList.begin(); iter != vertexClusterList.end(); )
    {
        const VertexCluster* pVertexCluster(*iter);
        
        if (pVertexCluster->GetVertexList().size() < 3)
            iter = vertexClusterList.erase(iter); //calls destructor
        else
            ++iter;
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode VertexClusteringTool::ReadSettings(const TiXmlHandle xmlHandle)
{
    // Read settings from xml file here

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MaxVertexToCentroidDistance", m_maxVertexToCentroidDistance));
        
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "RemoveSmallClusters", m_removeSmallClusters));

    return STATUS_CODE_SUCCESS;
}

} // namespace lar_content
