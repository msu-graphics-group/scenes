#include "CamHostPluginAPI.h"
#include <iostream>
#include <iomanip> 
#include <thread> // just for test big delay
#include <chrono> // std::chrono::seconds

#include <cstdint>
#include <cstddef>
#include <cassert>
#include <memory>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>

#include "LiteMath.h"
using LiteMath::float4x4;
using LiteMath::float4;
using LiteMath::float3;
using LiteMath::float2;

#include "HydraRngUtils.h"

#include "svm.h"
#include "RT_sampler.h"
#include "BSP_based_sampler.h"
#include <set>
#include <cassert>
#include <fstream>
#include <sstream>
#include <random>

#include "render/image_save.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct SubPixelElement // can process as float4 in some cases
{
  float    color[3] = {};
  uint32_t objId    = uint32_t(0xFFFFFFFF);
};

static inline bool close_tex_data(SubPixelElement a, SubPixelElement b)
{
  return a.objId == b.objId;
}

using BSPImage4f = BSPBasedSampler<SubPixelElement>;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class ZeroColorRTSampler
{
  std::shared_ptr<RayTracer> tracer;
  uint32_t width, height;
public:
  ZeroColorRTSampler(std::shared_ptr<RayTracer> tr, uint32_t w, uint32_t h) : tracer(tr), width(w), height(h) {}

  SubPixelElement sample(float x, float y) const
  {
    SubPixelElement sample;
    const uint32_t x_texel = uint32_t(x) * width;
    const uint32_t y_texel = uint32_t(y) * height;
    tracer->CastSingleRayForSurfaceId(x_texel, y_texel, x * width - x_texel, y * width - y_texel, &sample.objId);
    return sample;
  }

  SubPixelElement fetch(uint32_t x, uint32_t y) const
  {
    SubPixelElement sample;
    tracer->CastSingleRayForSurfaceId(x, y, 0, 0, &sample.objId);
    return sample;
  }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct PipeThrough
{
  float x = 0.0f;
  float y = 0.0f;
  uint32_t packedIndex = 0;
};

class PinHoleBSPImageAccum : public IHostRaysAPI
{
public:
  PinHoleBSPImageAccum() { hr_qmc::init(table); m_globalCounter = 0; }
  
  std::string outImagePath = "";

  void SetParameters(int a_width, int a_height, const float a_projInvMatrix[16], const wchar_t* a_camNodeText) override
  {
    m_fwidth  = float(a_width);
    m_fheight = float(a_height);
    m_width   = a_width;
    m_height  = a_height;
    for(int i=0;i<4;i++)
      for(int j=0;j<4;j++)
        m_projInv(i,j) = a_projInvMatrix[j*4+i];             // assume column major !
    //memcpy(&m_projInv, a_projInvMatrix, sizeof(float4x4)); // actually same but, not safe if our matrices and Hydra matrices will have different layout

    //std::cout << "[PinHoleBSPImageAccum] m_projInv: " << std::endl;
    //for(int i=0;i<4;i++) {
    //  for(int j=0;j<4;j++)
    //    cout << m_projInv(i,j) << " ";
    //  cout << std::endl;
    //}

    BSPImage4f::Config config;
    config.width          = a_width;
    config.height         = a_height;
    config.windowHalfSize = 1;
    config.radius         = 0.5f;
    config.additionalSamplesCnt = 32;
    m_pFrameBuffer = std::make_unique<BSPImage4f>(config);

    std::string scenePath = "/home/frol/PROG/msu-graphics-group/scenes/01_simple_scenes/instanced_objects.xml"; //#TODO: pass scene path here!
    outImagePath          = "/home/frol/PROG/msu-graphics-group/scenes/sample_loader_app/z_out_bsp.png";
    
    std::cout << "[PinHoleBSP]: loading scene from " << scenePath.c_str() << std::endl;
    auto pRayTracerCPU = std::make_shared<RayTracer>(a_width, a_height);
    auto loaded = pRayTracerCPU->LoadScene(scenePath.c_str()); 
    if(!loaded)
      std::cout << "[PinHoleBSP]: can't load scene from " << scenePath.c_str() << std::endl;

    std::cout << "[PinHoleBSP]: constructing BSPImage ... " << std::endl;
    m_pFrameBuffer->configure(ZeroColorRTSampler(pRayTracerCPU, a_width, a_height));
  }

  void MakeRaysBlock(RayPart1* out_rayPosAndNear, RayPart2* out_rayDirAndFar, size_t in_blockSize, int passId) override;
  void AddSamplesContribution(float* out_color4f, const float* colors4f, size_t in_blockSize, uint32_t a_width, uint32_t a_height, int passId) override;
  void FinishRendering() override;
  
  std::unique_ptr<BSPImage4f> m_pFrameBuffer = nullptr;
  std::vector<PipeThrough>    m_pipeline[HOST_RAYS_PIPELINE_LENGTH];

  unsigned int table[hr_qmc::QRNG_DIMENSIONS][hr_qmc::QRNG_RESOLUTION];
  unsigned int m_globalCounter = 0;

  float m_fwidth  = 1024.0f;
  float m_fheight = 1024.0f;
  float4x4 m_projInv;

  uint32_t m_width  = 1024;
  uint32_t m_height = 1024;
  float   m_spp    = 0;

  float FOCAL_PLANE_DIST = 10.0f;
  float DOF_LENS_RADIUS  = 0.0f;
  bool  DOF_IS_ENABLED = false;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


static inline int myPackXY1616(int x, int y) { return (y << 16) | (x & 0x0000FFFF); }
static inline int myAsInt(float x) 
{ 
  int res;
  memcpy(&res, &x, sizeof(int));
  return res;
}
LiteMath::float3 EyeRayDir(float x, float y, float w, float h, LiteMath::float4x4 a_mViewProjInv);

void PinHoleBSPImageAccum::MakeRaysBlock(RayPart1* out_rayPosAndNear, RayPart2* out_rayDirAndFar, size_t in_blockSize, int passId)
{
  if(m_pipeline[0].size() == 0)
  {
    for(int i=0;i<HOST_RAYS_PIPELINE_LENGTH;i++)
    m_pipeline[i].resize(in_blockSize);
  }

  const int putID = passId % HOST_RAYS_PIPELINE_LENGTH;

  const float pixelSizeX = 1.0f/m_fwidth;
  const float pixelSizeY = 1.0f/m_fheight;

  #pragma omp parallel for
  for(int i=0;i<int(in_blockSize);i++)
  {
    const float rndX = hr_qmc::rndFloat(m_globalCounter+i, 0, table[0]);
    const float rndY = hr_qmc::rndFloat(m_globalCounter+i, 1, table[0]);
    
    const float x    = m_fwidth*rndX; 
    const float y    = m_fheight*rndY;

    float3 ray_pos = float3(0,0,0);
    float3 ray_dir = EyeRayDir(x, y, m_fwidth, m_fheight, m_projInv);

    //if (DOF_IS_ENABLED) // dof is enabled
    //{
    //  const float lenzX = hr_qmc::rndFloat(m_globalCounter+i, 2, table[0]);
    //  const float lenzY = hr_qmc::rndFloat(m_globalCounter+i, 3, table[0]);
    //
    //  const float tFocus         = FOCAL_PLANE_DIST / (-ray_dir.z);
    //  const float3 focusPosition = ray_pos + ray_dir*tFocus;
    //  const float2 xy            = DOF_LENS_RADIUS*2.0f*MapSamplesToDisc(float2(lenzX - 0.5f, lenzY - 0.5f));
    //  ray_pos.x += xy.x;
    //  ray_pos.y += xy.y;
    //
    //  ray_dir = normalize(focusPosition - ray_pos);
    //}

    RayPart1 p1;
    p1.origin[0]   = ray_pos.x;
    p1.origin[1]   = ray_pos.y;
    p1.origin[2]   = ray_pos.z;
    p1.xyPosPacked = myPackXY1616(int(x), int(y));
   
    RayPart2 p2;
    p2.direction[0] = ray_dir.x;
    p2.direction[1] = ray_dir.y;
    p2.direction[2] = ray_dir.z;
    p2.dummy        = 0.0f;

    PipeThrough pipeData;
    pipeData.x = rndX; // + 0.5f*pixelSizeX;
    pipeData.y = rndY; // + 0.5f*pixelSizeY;
    pipeData.packedIndex = p1.xyPosPacked; 
    
    out_rayPosAndNear[i] = p1;
    out_rayDirAndFar [i] = p2;
    m_pipeline[putID][i] = pipeData;
  }

  //std::this_thread::sleep_for(std::chrono::milliseconds(50)); // test big delay

  m_globalCounter += unsigned(in_blockSize);
} 

void PinHoleBSPImageAccum::AddSamplesContribution(float* out_color4f, const float* colors4f, size_t in_blockSize, uint32_t a_width, uint32_t a_height, int passId)
{
  const int takeID = (passId + HOST_RAYS_PIPELINE_LENGTH - 2) % HOST_RAYS_PIPELINE_LENGTH;

  float4*       out_color = (float4*)out_color4f;
  const float4* colors    = (const float4*)colors4f;
  
  for (int i = 0; i < int(in_blockSize); i++)
  {
    const auto color = colors[i];
    const uint32_t packedIndex = myAsInt(color.w);

    const PipeThrough& passData = m_pipeline[takeID][i];
    if(passData.packedIndex != packedIndex)
    {
      std::cout << "warning, bad packed index: " << i << " : " << passData.packedIndex << " != " << packedIndex << std::endl; 
      std::cout.flush();
      continue;
      //assert(passData.packedIndex == packedIndex);
    }

    auto& subPixel = m_pFrameBuffer->access(passData.x, passData.y);
    subPixel.color[0] += color[0];
    subPixel.color[1] += color[1];
    subPixel.color[2] += color[2];

    //assert(passData.packedIndex == packedIndex);           ///<! check that we actually took data from 'm_pipeline' for right ray

    const int x      = (packedIndex & 0x0000FFFF);         ///<! extract x position from color.w
    const int y      = (packedIndex & 0xFFFF0000) >> 16;   ///<! extract y position from color.w
    const int offset = (a_height-y-1)*a_width + x;

    if (x >= 0 && y >= 0 && x < int(a_width) && y < int(a_height))
    {
      out_color[offset].x += color.x;
      out_color[offset].y += color.y;
      out_color[offset].z += color.z;
    }
  }

  m_spp += float(in_blockSize) / float(a_width*a_height);
}

void PinHoleBSPImageAccum::FinishRendering()
{ 
  std::cout << "[PinHoleBSPImageAccum]::FinishRendering is called" << std::endl; 

  std::vector<uint32_t> imageLDR(m_width*m_height);

  const float invSPP = 1.0f/m_spp;

  const uint32_t aaSamples = 4;
  for (uint32_t j = 0; j < m_width; ++j)
  {
    for (uint32_t i = 0; i < m_height; ++i)
    {
      float r = 0, g = 0, b = 0;
      for (uint32_t y = 0; y < aaSamples; ++y)
      {
        for (uint32_t x = 0; x < aaSamples; ++x)
        {
          float x_coord    = (float)(x + j * aaSamples) / (aaSamples * m_width);
          float y_coord    = (float)(y + i * aaSamples) / (aaSamples * m_height);
          //x_coord = LiteMath::clamp(x_coord, 0.0f, 0.995f);
          //y_coord = LiteMath::clamp(y_coord, 0.0f, 0.995f);
          SubPixelElement sample = m_pFrameBuffer->sample(x_coord, y_coord);
          
          // perform tone mapping for subixel
          //
          sample.color[0] = std::min(sample.color[0]*invSPP, 1.0f);
          sample.color[1] = std::min(sample.color[1]*invSPP, 1.0f);
          sample.color[2] = std::min(sample.color[2]*invSPP, 1.0f);
          
          r += sample.color[0];
          g += sample.color[1];
          b += sample.color[2];
        }
      }
      r /= (aaSamples * aaSamples);
      g /= (aaSamples * aaSamples);
      b /= (aaSamples * aaSamples);
      
      const float r1 = std::pow(r, 1.0f/2.2f);
      const float g1 = std::pow(g, 1.0f/2.2f);
      const float b1 = std::pow(b, 1.0f/2.2f);

      imageLDR[i*m_width+j] = 0xFF000000 | (uint32_t(r1*255.0f) << 0) | (uint32_t(g1*255.0f) << 8) | (uint32_t(b1*255.0f) << 16);
    }
  }

  saveImageLDR(outImagePath.c_str(), imageLDR, m_width, m_height, 4);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IHostRaysAPI* CreateTableLens();

IHostRaysAPI* MakeHostRaysEmitter(int a_pluginId) ///<! you replace this function or make your own ... the example will be provided
{
  if(a_pluginId == 0)
    return nullptr;

  return new PinHoleBSPImageAccum();
}

void DeleteRaysEmitter(IHostRaysAPI* pObject) { delete pObject; }
