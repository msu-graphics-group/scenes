#include <cfloat>
#include "raytracing.h"
#include "../loader/hydraxml.h"
#include "../loader/cmesh.h"

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////

static inline float3 mul3x3(float4x4 m, float3 v)
{ 
  return to_float3(m*to_float4(v, 0.0f));
}

static inline float3 mul4x3(float4x4 m, float3 v)
{
  return to_float3(m*to_float4(v, 1.0f));
}

static inline void transform_ray3f(float4x4 a_mWorldViewInv, float3* ray_pos, float3* ray_dir) 
{
  float3 pos  = mul4x3(a_mWorldViewInv, (*ray_pos));
  float3 pos2 = mul4x3(a_mWorldViewInv, ((*ray_pos) + 100.0f*(*ray_dir)));

  float3 diff = pos2 - pos;

  (*ray_pos)  = pos;
  (*ray_dir)  = normalize(diff);
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////

SurfaceInfo RayTracer::CastSingleRay(float x, float y)
{
  SurfaceInfo outSam;
  LiteMath::float4 rayPosAndNear, rayDirAndFar;
  kernel_InitEyeRay(x, y, &rayPosAndNear, &rayDirAndFar);
  kernel_RayTrace  (0, 0, &rayPosAndNear, &rayDirAndFar, &outSam);
  return outSam;
}


void RayTracer::kernel_InitEyeRay(float tidX, float tidY, LiteMath::float4* rayPosAndNear, LiteMath::float4* rayDirAndFar)
{
  LiteMath::float3 rayDir = EyeRayDirNormalized(tidX/m_fwidth, tidY/m_fheight, m_invProjView); 
  LiteMath::float3 rayPos = float3(0,0,0);

  transform_ray3f(m_worldViewInv, &rayPos, &rayDir);

  *rayPosAndNear = to_float4(rayPos, 0.0f);
  *rayDirAndFar  = to_float4(rayDir, FLT_MAX);
}

void RayTracer::kernel_RayTrace(uint32_t tidX, uint32_t tidY, const LiteMath::float4* rayPosAndNear,
                                const LiteMath::float4* rayDirAndFar, SurfaceInfo* out_sample)
{
  const LiteMath::float4 rayPos = *rayPosAndNear;
  const LiteMath::float4 rayDir = *rayDirAndFar ;

  CRT_Hit hit = m_pAccelStruct->RayQuery_NearestHit(rayPos, rayDir);
  
  SurfaceInfo res;
  res.instId = hit.instId; 
  res.geomId = hit.geomId;
  res.primId = hit.primId;
  res.color  = m_palette[hit.instId % palette_size];
  out_sample[tidY * m_width + tidX] = res;
}

std::vector<SurfaceInfo> RemoveDuplicateLabels(const SurfaceInfo* a_samples, size_t a_samplesNum)
{
  std::vector<SurfaceInfo> triIds;
  triIds.reserve(a_samplesNum);
  
  for(size_t samId=0; samId < a_samplesNum; samId++) 
  {
    bool found = false;
    for(size_t j=0; j<triIds.size(); j++) 
    {
      if(triIds[j] == a_samples[samId])
      {
        found = true;
        break;
      }
    }  
    if(!found)
      triIds.push_back(a_samples[samId]);
  }

  return triIds;
}


std::vector<LiteMath::float2> RayTracer::GetAllTriangleVerts2D(const SurfaceInfo* a_samples, size_t a_samplesNum)
{
  const auto compressed = RemoveDuplicateLabels(a_samples, a_samplesNum);

  std::vector<LiteMath::float2> res(compressed.size()*3);
  for(size_t sampleId = 0; sampleId < compressed.size(); sampleId++)
  {
    SurfaceInfo hit = compressed[sampleId];

    // (1) read vertices of target mesh and triangle
    //
    const uint32_t triOffset  = m_matIdOffsets[hit.geomId];
    const uint32_t vertOffset = m_vertOffset  [hit.geomId];
  
    const uint32_t A   = m_triIndices[(triOffset + hit.primId)*3 + 0];
    const uint32_t B   = m_triIndices[(triOffset + hit.primId)*3 + 1];
    const uint32_t C   = m_triIndices[(triOffset + hit.primId)*3 + 2];

    const float4 A_pos = m_vPos4f[A + vertOffset];
    const float4 B_pos = m_vPos4f[B + vertOffset];
    const float4 C_pos = m_vPos4f[C + vertOffset];

    // (2) apply worldView and projection matrices to them
    //
    float4 A_transformed = m_WVP*(m_instMatrices[hit.instId]*A_pos); A_transformed /= A_transformed.w;
    float4 B_transformed = m_WVP*(m_instMatrices[hit.instId]*B_pos); B_transformed /= B_transformed.w;
    float4 C_transformed = m_WVP*(m_instMatrices[hit.instId]*C_pos); C_transformed /= C_transformed.w;

    // (3) write their coordinates to resulting vector 
    //
    res[sampleId*3+0].x = A_transformed.x;
    res[sampleId*3+0].y = A_transformed.y;

    res[sampleId*3+1].x = B_transformed.x;
    res[sampleId*3+1].y = B_transformed.y;

    res[sampleId*3+2].x = C_transformed.x;
    res[sampleId*3+2].y = C_transformed.y;
  }

  return res;
}


bool RayTracer::LoadScene(const std::string& path)
{
  m_pAccelStruct = std::shared_ptr<ISceneObject>(CreateSceneRT(""));
  m_pAccelStruct->ClearGeom();

  hydra_xml::HydraScene scene;
  if(scene.LoadState(path) < 0)
    return false;

  for(auto cam : scene.Cameras())
  {
    float aspect   = float(m_width) / float(m_height);
    auto proj      = perspectiveMatrix(cam.fov, aspect, cam.nearPlane, cam.farPlane);
    auto worldView = lookAt(float3(cam.pos), float3(cam.lookAt), float3(cam.up));
    m_invProjView  = LiteMath::inverse4x4(proj); // LiteMath::inverse4x4(proj * transpose(inverse4x4(worldView)));
    m_worldViewInv = inverse4x4(worldView);
    m_WVP          = proj*worldView;
    break; // take first cam
  }
  
  m_matIdOffsets.reserve(1024);
  m_vertOffset.reserve(1024);
  m_matIdByPrimId.reserve(128000);
  m_triIndices.reserve(128000*3);
  m_vPos4f.reserve(128000);

  m_pAccelStruct->ClearGeom();
  for(auto meshPath : scene.MeshFiles())
  {
    std::cout << "[LoadScene]: mesh = " << meshPath.c_str() << std::endl;
    auto currMesh = cmesh::LoadMeshFromVSGF(meshPath.c_str());
    auto geomId   = m_pAccelStruct->AddGeom_Triangles4f(currMesh.vPos4f.data(), currMesh.vPos4f.size(),
                                                        currMesh.indices.data(), currMesh.indices.size());
    (void)geomId; // silence "unused variable" compiler warnings

    m_matIdOffsets.push_back(m_matIdByPrimId.size());
    m_vertOffset.push_back(m_vPos4f.size());

    m_matIdByPrimId.insert(m_matIdByPrimId.end(), currMesh.matIndices.begin(), currMesh.matIndices.end() );
    m_triIndices.insert(m_triIndices.end(), currMesh.indices.begin(), currMesh.indices.end());
    m_vPos4f.insert(m_vPos4f.end(), currMesh.vPos4f.begin(), currMesh.vPos4f.end()); 
  }
  
  m_instMatrices.reserve(1024);
  m_instMatrices.clear();
  
  m_pAccelStruct->ClearScene();
  for(auto inst : scene.InstancesGeom())
  {
    m_pAccelStruct->AddInstance(inst.geomId, inst.matrix);
    m_instMatrices.push_back(inst.matrix);
  }
  m_pAccelStruct->CommitScene();

  return true;
}

LiteMath::float3 EyeRayDirNormalized(float x, float y, LiteMath::float4x4 a_mViewProjInv)
{
  LiteMath::float4 pos = LiteMath::make_float4(2.0f*x - 1.0f,
                                               2.0f*y - 1.0f,
                                               0.0f,
                                               1.0f );

  pos = a_mViewProjInv * pos;
  pos /= pos.w;
  //pos.y *= (-1.0f);
  return normalize(to_float3(pos));
}

LiteMath::float4x4 perspectiveMatrix(float fovy, float aspect, float zNear, float zFar)
{
  const float ymax = zNear * tanf(fovy * 3.14159265358979323846f / 360.0f);
  const float xmax = ymax * aspect;
  const float left = -xmax;
  const float right = +xmax;
  const float bottom = -ymax;
  const float top = +ymax;
  const float temp = 2.0f * zNear;
  const float temp2 = right - left;
  const float temp3 = top - bottom;
  const float temp4 = zFar - zNear;
  LiteMath::float4x4 res;
  res.m_col[0] = LiteMath::float4{ temp / temp2, 0.0f, 0.0f, 0.0f };
  res.m_col[1] = LiteMath::float4{ 0.0f, temp / temp3, 0.0f, 0.0f };
  res.m_col[2] = LiteMath::float4{ (right + left) / temp2,  (top + bottom) / temp3, (-zFar - zNear) / temp4, -1.0 };
  res.m_col[3] = LiteMath::float4{ 0.0f, 0.0f, (-temp * zFar) / temp4, 0.0f };
  return res;
}