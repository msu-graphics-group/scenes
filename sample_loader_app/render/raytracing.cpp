#include <cfloat>
#include "raytracing.h"
#include "../loader/hydraxml.h"
#include "../loader/cmesh.h"


void RayTracer::CastSingleRay(uint32_t tidX, uint32_t tidY, uint32_t* out_color)
{
  LiteMath::float4 rayPosAndNear, rayDirAndFar;
  kernel_InitEyeRay(tidX, tidY, &rayPosAndNear, &rayDirAndFar);

  kernel_RayTrace(tidX, tidY, &rayPosAndNear, &rayDirAndFar, out_color);
}

void RayTracer::kernel_InitEyeRay(uint32_t tidX, uint32_t tidY, LiteMath::float4* rayPosAndNear, LiteMath::float4* rayDirAndFar)
{
  *rayPosAndNear = to_float4(m_camPos, 1.0f);
  
  const LiteMath::float3 rayDir  = EyeRayDir(float(tidX), float(tidY), float(m_width), float(m_height), m_invProjView);
  *rayDirAndFar  = to_float4(rayDir, FLT_MAX);
}


void RayTracer::kernel_RayTrace(uint32_t tidX, uint32_t tidY, const LiteMath::float4* rayPosAndNear,
                                const LiteMath::float4* rayDirAndFar, uint32_t* out_color)
{
  const LiteMath::float4 rayPos = *rayPosAndNear;
  const LiteMath::float4 rayDir = *rayDirAndFar ;

  CRT_Hit hit = m_pAccelStruct->RayQuery_NearestHit(rayPos, rayDir);

  out_color[tidY * m_width + tidX] = m_palette[hit.instId % palette_size];
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
    m_invProjView = LiteMath::inverse4x4(proj * transpose(inverse4x4(worldView)));
    m_camPos = float3(cam.pos);

    break; // take first cam
  }

  m_pAccelStruct->ClearGeom();
  for(auto meshPath : scene.MeshFiles())
  {
    std::cout << "[LoadScene]: mesh = " << meshPath.c_str() << std::endl;
    auto currMesh = cmesh::LoadMeshFromVSGF(meshPath.c_str());
    auto geomId   = m_pAccelStruct->AddGeom_Triangles3f((const float*)currMesh.vPos4f.data(), currMesh.vPos4f.size(),
                                                        currMesh.indices.data(), currMesh.indices.size(), BUILD_HIGH, sizeof(float)*4);
    (void)geomId; // silence "unused variable" compiler warnings
  }

  m_pAccelStruct->ClearScene();
  for(auto inst : scene.InstancesGeom())
  {
    m_pAccelStruct->AddInstance(inst.geomId, inst.matrix);
  }
  m_pAccelStruct->CommitScene();

  return true;
}

LiteMath::float3 EyeRayDir(float x, float y, float w, float h, LiteMath::float4x4 a_mViewProjInv)
{
  LiteMath::float4 pos = LiteMath::make_float4(2.0f * (x + 0.5f) / w - 1.0f,
                                               2.0f * (y + 0.5f) / h - 1.0f,
                                               0.0f,
                                               1.0f );

  pos = a_mViewProjInv * pos;
  pos /= pos.w;
  pos.y *= (-1.0f);

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