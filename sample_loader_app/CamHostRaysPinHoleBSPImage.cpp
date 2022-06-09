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
#include "pugixml.hpp" // for XML

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
#include "../loader/hydraxml.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct SubPixelElement // can process as float4 in some cases
{
  float    color[3] = {};
  uint32_t instId   = uint32_t(0xFFFFFFFF);
  uint32_t geomId   = uint32_t(0xFFFFFFFF);
  uint32_t primId   = uint32_t(0xFFFFFFFF);
  uint32_t hits     = 0;

  inline bool operator==(const SubPixelElement& rhs) const { return instId == rhs.instId; }
  inline bool operator!=(const SubPixelElement& rhs) const { return instId != rhs.instId; }
};

#define NAIVE_IMPL

#ifdef NAIVE_IMPL
using BSPImage4f = SubPixelImageNaive<SubPixelElement>;
#else
using BSPImage4f = SubPixelImageBSP<SubPixelElement>;
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class ZeroColorRTSampler
{
  std::shared_ptr<RayTracer> tracer;
  uint32_t width, height;
public:
  ZeroColorRTSampler(std::shared_ptr<RayTracer> tr, uint32_t w, uint32_t h) : tracer(tr), width(w), height(h) {}

  SubPixelElement sample(float x, float y) const
  {
    auto internal = tracer->CastSingleRay(x * float(width), y * float(height));
    SubPixelElement sample;
    sample.instId  = internal.instId;
    sample.geomId = internal.geomId;
    sample.primId = internal.primId;
    return sample;
  }

  SubPixelElement fetch(uint32_t x, uint32_t y) const
  {
    auto internal = tracer->CastSingleRay(float(x)+0.5f, float(y)+0.5f);
    SubPixelElement sample;
    sample.instId  = internal.instId; 
    sample.geomId = internal.geomId;
    sample.primId = internal.primId;
    return sample;
  }

  std::vector<LiteMath::float2> GetAllTriangleVerts2D(const SubPixelElement* a_compressed, size_t a_compressedNum) const 
  { 
    std::vector<SurfaceInfo> compressed2(a_compressedNum);
    for(size_t i=0;i<a_compressedNum;i++)
    {
      compressed2[i].instId = a_compressed[i].instId;
      compressed2[i].geomId = a_compressed[i].geomId;
      compressed2[i].primId = a_compressed[i].primId;
    }
    return tracer->GetAllTriangleVerts2D(compressed2.data(), compressed2.size());
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
  std::string m_scenefile;

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

    m_doc.load_string(a_camNodeText);
    //pugi::xml_node a_camNode      = m_doc.child(L"camera"); //
    pugi::xml_node a_settingsNode = m_doc.child(L"render_settings"); //
    //ReadParamsFromNode(a_camNode);
    ReadParamsFromSettingsNode(a_settingsNode);

    m_pFrameBuffer = std::make_unique<BSPImage4f>(a_width, a_height, 0.5f);

    std::string scenePath = m_scenefile;
    outImageFolder        = "."; //"/home/frol/PROG/msu-graphics-group/scenes/sample_loader_app";
    
    std::cout << "[PinHoleBSP]: loading scene from " << scenePath.c_str() << std::endl;
    auto pRayTracerCPU = std::make_shared<RayTracer>(a_width, a_height);
    auto loaded = pRayTracerCPU->LoadScene(scenePath.c_str()); 
    if(!loaded)
      std::cout << "[PinHoleBSP]: can't load scene from " << scenePath.c_str() << std::endl;
    
    for(int i=0;i<HOST_RAYS_PIPELINE_LENGTH;i++)
      m_pipeline[i].resize(0);
  
    std::cout << "[PinHoleBSP]: constructing BSPImage ... " << std::endl;
    m_pFrameBuffer->configure(ZeroColorRTSampler(pRayTracerCPU, a_width, a_height));
  }

  void ReadParamsFromSettingsNode(pugi::xml_node a_node);

  void MakeRaysBlock(RayPart1* out_rayPosAndNear, RayPart2* out_rayDirAndFar, size_t in_blockSize, int passId) override;
  void AddSamplesContribution(float* out_color4f, const float* colors4f, size_t in_blockSize, uint32_t a_width, uint32_t a_height, int passId) override;
  void FinishRendering() override;
  
  void SaveUpsampledRegion(int regionStartX, int regionStartY, int regionSize, int upSamplePower); 

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
  pugi::xml_document m_doc;

  int32_t* m_pInstRemapTable = nullptr;
  int32_t  m_pInstRemapSize = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void PinHoleBSPImageAccum::ReadParamsFromSettingsNode(pugi::xml_node a_node)
{
  const uint64_t address = a_node.child(L"remapInstAddress").text().as_ullong();
  const uint64_t size    = a_node.child(L"remapInstSize").text().as_ullong();

  m_pInstRemapTable = reinterpret_cast<int32_t*>(address);
  m_pInstRemapSize  = int32_t(size);

  const std::wstring folder = a_node.child(L"xmlfilepath").text().as_string();
  m_scenefile = hydra_xml::ws2s(folder); // "/" + hydra_xml::ws2s(statef);
  std::cout << "[ReadParamsFromSettingsNode]: m_scenefile = " << m_scenefile.c_str() << std::endl;
}

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
    p1.xyPosPacked = myPackXY1616(std::max<int>(int(m_fwidth*rndX), 0), 
                                  std::max<int>(int(m_fheight*rndY), 0));
   
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
  if(m_pipeline[0].size() == 0)
    return;

  const int takeID = (passId + HOST_RAYS_PIPELINE_LENGTH - 2) % HOST_RAYS_PIPELINE_LENGTH;

  float4*       out_color = (float4*)out_color4f;
  const float4* colors    = (const float4*)colors4f;
  
  for (int i = 0; i < int(in_blockSize); i++)
  {
    const auto color = colors[i];
    auto packedIndex = myAsInt(color.w);
    if(packedIndex < 0)
      continue;
    if(packedIndex < m_pInstRemapSize)
      packedIndex = m_pInstRemapTable[packedIndex];

    const PipeThrough& passData = m_pipeline[takeID][i];
    if(passData.x == 1.0f || passData.y == 1.0f)
      continue;
    
    auto& subPixel = m_pFrameBuffer->access(passData.x, passData.y); // process sample only if we hit same surface

    #ifdef NAIVE_IMPL
    if(true) 
    #else
    if(int(subPixel.instId) == packedIndex) 
    #endif
    {
      subPixel.color[0] += color[0];
      subPixel.color[1] += color[1];
      subPixel.color[2] += color[2];
      subPixel.hits++;
    }

    //assert(passData.packedIndex == packedIndex);  ///<! check that we actually took data from 'm_pipeline' for right ray
    //int x = (packedIndex & 0x0000FFFF);           ///<! extract x position from color.w
    //int y = (packedIndex & 0xFFFF0000) >> 16;     ///<! extract y position from color.w
    
    int x = int(m_fwidth*passData.x);
    int y = int(m_fheight*passData.y);

    x = std::min<int>(x, m_width -1);
    y = std::min<int>(y, m_height-1);

    const int offset = y*a_width + x;

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

  //const float invSPP = 1.0f/m_spp;
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////
  const int refSubSamples = 64;
  std::vector<float> hammSamples(refSubSamples * 2);
  PlaneHammersley(hammSamples.data(), refSubSamples);
  const float aaSamplesTotalf = float(refSubSamples);
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////
  
  //#pragma omp parallel for
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
          r += std::min(sample.color[0]/float(sample.hits), 1.0f);
          g += std::min(sample.color[1]/float(sample.hits), 1.0f);
          b += std::min(sample.color[2]/float(sample.hits), 1.0f);
        }
      }
      
      const float r1 = std::pow(r/aaSamplesTotalf, 1.0f/2.2f);
      const float g1 = std::pow(g/aaSamplesTotalf, 1.0f/2.2f);
      const float b1 = std::pow(b/aaSamplesTotalf, 1.0f/2.2f);

      imageLDR[y1*m_width+x1] = 0xFF000000 | (uint32_t(r1*255.0f) << 0) | (uint32_t(g1*255.0f) << 8) | (uint32_t(b1*255.0f) << 16);
    }
  }

  std::string out1 = outImageFolder + "/z_out1_bsp.png";
  saveImageLDR(out1.c_str(), imageLDR, m_width, m_height, 4);
  
  //bunny scene:
  SaveUpsampledRegion(400,m_height-130-50-1,50,16);
  SaveUpsampledRegion(400,143,              50,16);
  SaveUpsampledRegion(520,m_height-915-25-1,50,16);
  SaveUpsampledRegion(590,m_height-600-25-1,50,16);
  SaveUpsampledRegion(540,m_height-455-25-1,50,16);

  //inst scene:
  //SaveUpsampledRegion(110,m_height-725-50-1,50,16);
  //SaveUpsampledRegion(260,m_height-325-50-1,50,16);
  //SaveUpsampledRegion(175,m_height-320-40-1,50,16);
  //SaveUpsampledRegion(520,m_height-726-40-1,50,16);
  //SaveUpsampledRegion(450,m_height-338-40-1,50,16);

  //self-illum:
  //SaveUpsampledRegion(175,m_height-117-1,50,16);
  //SaveUpsampledRegion(415,m_height-305-25-1,50,16);
  //SaveUpsampledRegion(510,m_height-720-50-1,50,16);
  //SaveUpsampledRegion(225,m_height-80-50-1, 50,16);
  //SaveUpsampledRegion(760,m_height-225-40-1,50,16);

  //hairballs:
  //SaveUpsampledRegion(175,m_height-320-40-1,50,16);
  //SaveUpsampledRegion(450,m_height-350-40-1,50,16);
  //SaveUpsampledRegion(140,m_height-360-40-1,50,16);
  //SaveUpsampledRegion(322,m_height-400-40-1,50,16);
  //SaveUpsampledRegion(620,m_height-765-40-1,50,16);
  
}

void PinHoleBSPImageAccum::SaveUpsampledRegion(int regionStartX, int regionStartY, int regionSize, int upSamplePower)
{
  std::cout << "compute upsampled image ..." << std::endl;

  int totalSizeX = regionSize*upSamplePower;
  int totalSizeY = regionSize*upSamplePower;

  std::vector<uint32_t> imageLDR(totalSizeX*totalSizeY);
  
  #pragma omp parallel for
  for (int y1 = regionStartY; y1 < regionStartY + regionSize; ++y1) {
    for (int x1 = regionStartX; x1 < regionStartX + regionSize; ++x1) {

      for (int y2 = 0; y2 < upSamplePower; ++y2) {
        for (int x2 = 0; x2 < upSamplePower; ++x2) {
              
          float coordX = (float(x1) + float(x2)/float(upSamplePower) )/m_fwidth;
          float coordY = (float(y1) + float(y2)/float(upSamplePower) )/m_fheight;
         
          int coordXAbs = (x1 - regionStartX)*upSamplePower + x2;
          int coordYAbs = (y1 - regionStartY)*upSamplePower + y2;

          SubPixelElement sample = m_pFrameBuffer->sample(coordX, coordY);  
          float fHitsCount = float( std::max(sample.hits, 1u) );

          float r = std::min(sample.color[0]/fHitsCount, 1.0f);
          float g = std::min(sample.color[1]/fHitsCount, 1.0f);
          float b = std::min(sample.color[2]/fHitsCount, 1.0f);

          const float r1 = std::pow(r, 1.0f/2.2f);
          const float g1 = std::pow(g, 1.0f/2.2f);
          const float b1 = std::pow(b, 1.0f/2.2f);

          imageLDR[coordYAbs*totalSizeX + coordXAbs] = 0xFF000000 | (uint32_t(r1*255.0f) << 0) | (uint32_t(g1*255.0f) << 8) | (uint32_t(b1*255.0f) << 16);
        }
      }

    }
  }
  
  std::stringstream strOut;
  strOut << outImageFolder.c_str() << "/" << "z_upsampled_" << std::setfill('0') << std::setw(4) << regionStartX << "_" << std::setfill('0') << std::setw(4) << regionStartY << ".png";
  std::string out1 = strOut.str(); //outImageFolder + "/z_upsampled.png";
  saveImageLDR(out1.c_str(), imageLDR, totalSizeX, totalSizeY, 4);
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