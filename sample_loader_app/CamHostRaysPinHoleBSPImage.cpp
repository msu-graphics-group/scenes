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

#include "SubPixelImageBSP.h"
#include "SubPixelImageNaive.h"

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
  uint32_t hits     = 0;
};

static inline bool close_tex_data(SubPixelElement a, SubPixelElement b)
{
  return a.objId == b.objId;
}

//using BSPImage4f = SubPixelImageBSP<SubPixelElement>;
using BSPImage4f = SubPixelImageNaive<SubPixelElement>;

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
    sample.objId = tracer->CastSingleRay(x * float(width), y * float(height)).objId;
    return sample;
  }

  SubPixelElement fetch(uint32_t x, uint32_t y) const
  {
    SubPixelElement sample;
    sample.objId = tracer->CastSingleRay(float(x)+0.5f, float(y)+0.5f).objId; 
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
  
  std::string outImageFolder;

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

    m_pFrameBuffer = std::make_unique<BSPImage4f>(a_width, a_height, 0.5f);

    //std::string scenePath = "/home/frol/PROG/msu-graphics-group/scenes/01_simple_scenes/instanced_objects.xml"; //#TODO: pass scene path here!
    std::string scenePath = "/home/frol/PROG/msu-graphics-group/scenes/01_simple_scenes/bunny_cornell.xml";       //#TODO: pass scene path here!
    outImageFolder        = "/home/frol/PROG/msu-graphics-group/scenes/sample_loader_app";
    
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
  //float4*                     m_hydraFB = nullptr;

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
LiteMath::float3 EyeRayDirNormalized(float x, float y, LiteMath::float4x4 a_mViewProjInv);

void PinHoleBSPImageAccum::MakeRaysBlock(RayPart1* out_rayPosAndNear, RayPart2* out_rayDirAndFar, size_t in_blockSize, int passId)
{
  if(m_pipeline[0].size() == 0)
  {
    for(int i=0;i<HOST_RAYS_PIPELINE_LENGTH;i++)
    m_pipeline[i].resize(in_blockSize);
  }

  const int putID = passId % HOST_RAYS_PIPELINE_LENGTH;

  #pragma omp parallel for default(none) shared (m_pipeline,m_projInv,table,m_globalCounter,m_fwidth,m_fheight,putID,in_blockSize,out_rayPosAndNear,out_rayDirAndFar)
  for(int i=0;i<int(in_blockSize);i++)
  {
    const float rndX = hr_qmc::rndFloat(m_globalCounter+i, 0, table[0]);
    const float rndY = hr_qmc::rndFloat(m_globalCounter+i, 1, table[0]);

    float3 ray_pos = float3(0,0,0);
    float3 ray_dir = EyeRayDirNormalized(rndX, rndY, m_projInv);

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
    p1.xyPosPacked = myPackXY1616(std::max<int>(int(m_fwidth*rndX  - 0.5f), 0), 
                                  std::max<int>(int(m_fheight*rndY - 0.5f), 0));
   
    RayPart2 p2;
    p2.direction[0] = ray_dir.x;
    p2.direction[1] = ray_dir.y;
    p2.direction[2] = ray_dir.z;
    p2.dummy        = 0.0f;

    PipeThrough pipeData;
    pipeData.x = rndX; 
    pipeData.y = rndY; 
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
    subPixel.hits++;

    //assert(passData.packedIndex == packedIndex);  ///<! check that we actually took data from 'm_pipeline' for right ray
    //int x = (packedIndex & 0x0000FFFF);           ///<! extract x position from color.w
    //int y = (packedIndex & 0xFFFF0000) >> 16;     ///<! extract y position from color.w
    
    int x = int(m_fwidth*passData.x  - 0.0f);
    int y = int(m_fheight*passData.y - 0.0f);

    x = std::max<int>(0, std::min<int>(x, m_width -1));
    y = std::max<int>(0, std::min<int>(y, m_height-1));

    const int offset = (a_height-y-1)*a_width + x;
    //const int offset = y*a_width + x;

    if (x >= 0 && y >= 0 && x < int(a_width) && y < int(a_height))
    {
      out_color[offset].x += color.x;
      out_color[offset].y += color.y;
      out_color[offset].z += color.z;
    }
  }

  m_spp += float(in_blockSize) / float(a_width*a_height);
  //m_hydraFB = out_color;
}

void PinHoleBSPImageAccum::FinishRendering()
{ 
  std::cout << "[PinHoleBSPImageAccum]::FinishRendering is called" << std::endl; 

  std::vector<uint32_t> imageLDR(m_width*m_height);

  const float invSPP = 1.0f/m_spp;
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////
  const int refSubSamples = 16;
  std::vector<float> hammSamples(refSubSamples * 2);
  PlaneHammersley(hammSamples.data(), refSubSamples);
  const float aaSamplesTotalf = float(refSubSamples);
  const float aaSamplesTotalInv = 1.0f/aaSamplesTotalf;
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////

  for (uint32_t y1 = 0; y1 < m_height; ++y1)
  {
    for (uint32_t x1 = 0; x1 < m_width; ++x1)
    {
      float r = 0, g = 0, b = 0;

      for (int k = 0; k < refSubSamples; ++k)
      {
        SubPixelElement sample = m_pFrameBuffer->sample( (float(x1) + hammSamples[k * 2 + 0]) / m_fwidth, 
                                                         (float(y1) + hammSamples[k * 2 + 1]) / m_fheight);

        if(sample.hits > 0)
        {
          // perform tone mapping for subixel
          //
          const float hitsAccount = aaSamplesTotalf/float(sample.hits);
          r += std::min(sample.color[0]*invSPP*hitsAccount, aaSamplesTotalInv);
          g += std::min(sample.color[1]*invSPP*hitsAccount, aaSamplesTotalInv);
          b += std::min(sample.color[2]*invSPP*hitsAccount, aaSamplesTotalInv);
        }
      }
      
      const float r1 = std::pow(r, 1.0f/2.2f);
      const float g1 = std::pow(g, 1.0f/2.2f);
      const float b1 = std::pow(b, 1.0f/2.2f);

      imageLDR[y1*m_width+x1] = 0xFF000000 | (uint32_t(r1*255.0f) << 0) | (uint32_t(g1*255.0f) << 8) | (uint32_t(b1*255.0f) << 16);
    }
  }

  std::string out1 = outImageFolder + "/z_out1_bsp.png";
  saveImageLDR(out1.c_str(), imageLDR, m_width, m_height, 4);
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
