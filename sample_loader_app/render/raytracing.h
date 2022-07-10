#ifndef LOADER_RAYTRACING_H
#define LOADER_RAYTRACING_H

#include <cstdint>
#include <memory>
#include <array>
#include <iostream>
#include <vector>
#include "LiteMath.h"
#include "CrossRT.h"

struct SurfaceInfo
{
  uint32_t instId; 
  uint32_t geomId;
  uint32_t primId;
  uint32_t color;

  inline bool operator==(const SurfaceInfo& rhs) const { return instId == rhs.instId; }
  inline bool operator!=(const SurfaceInfo& rhs) const { return instId != rhs.instId; }
};

class RayTracer
{
public:
  RayTracer(uint32_t a_width, uint32_t a_height) : m_width(a_width), m_height(a_height), m_fwidth(a_width), m_fheight(a_height) {}
  bool LoadScene(const std::string& path);
  SurfaceInfo CastSingleRay(float x, float y);

  std::vector<LiteMath::float2> GetAllTriangleVerts2D(const SurfaceInfo* a_compressed, size_t a_compressedNum);

protected:

  void kernel_InitEyeRay(float tidX, float tidY, LiteMath::float4* rayPosAndNear, LiteMath::float4* rayDirAndFar);
  void kernel_RayTrace(uint32_t tidX, uint32_t tidY, const LiteMath::float4* rayPosAndNear, const LiteMath::float4* rayDirAndFar, SurfaceInfo* out_sample);

  uint32_t m_width;
  uint32_t m_height;
  float    m_fwidth;
  float    m_fheight;

  LiteMath::float4x4 m_invProjView;
  LiteMath::float4x4 m_worldViewInv;
  LiteMath::float4x4 m_WVP;

  std::shared_ptr<ISceneObject> m_pAccelStruct;

  std::vector<uint32_t> m_matIdOffsets;   ///< offset = m_matIdOffsets[geomId]
  std::vector<uint32_t> m_matIdByPrimId;  ///< matId  = m_matIdByPrimId[offset + primId]
  std::vector<uint32_t> m_triIndices;     ///< (A,B,C) = m_triIndices[(offset + primId)*3 + 0/1/2]
  std::vector<uint32_t> m_vertOffset;     ///< vertOffs = m_vertOffset[geomId]
  std::vector<LiteMath::float4>   m_vPos4f; ///< vertPos  = m_vPos4f[vertOffs + vertId]
  std::vector<LiteMath::float4x4> m_instMatrices;

  // color palette to select color for objects based on mesh/instance id
  static constexpr uint32_t palette_size = 20;
  static constexpr std::array<uint32_t, palette_size> m_palette = {
    0xffe6194b, 0xff3cb44b, 0xffffe119, 0xff0082c8,
    0xfff58231, 0xff911eb4, 0xff46f0f0, 0xfff032e6,
    0xffd2f53c, 0xfffabebe, 0xff008080, 0xffe6beff,
    0xffaa6e28, 0xfffffac8, 0xff800000, 0xffaaffc3,
    0xff808000, 0xffffd8b1, 0xff000080, 0xff808080
  };
};

LiteMath::float4x4 perspectiveMatrix(float fovy, float aspect, float zNear, float zFar);
LiteMath::float3   EyeRayDirNormalized(float x, float y, LiteMath::float4x4 a_mViewProjInv);

#endif// LOADER_RAYTRACING_H
