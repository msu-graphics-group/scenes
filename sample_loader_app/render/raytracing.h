#ifndef LOADER_RAYTRACING_H
#define LOADER_RAYTRACING_H

#include <cstdint>
#include <memory>
#include <array>
#include <iostream>
#include "LiteMath.h"
#include "CrossRT.h"

class RayTracer
{
public:
  RayTracer(uint32_t a_width, uint32_t a_height) : m_width(a_width), m_height(a_height) {}

  bool LoadScene(const std::string& path);

  void CastSingleRay(uint32_t tidX, uint32_t tidY, uint32_t* out_color);
  void kernel_InitEyeRay(uint32_t tidX, uint32_t tidY, LiteMath::float4* rayPosAndNear, LiteMath::float4* rayDirAndFar);
  void kernel_RayTrace(uint32_t tidX, uint32_t tidY, const LiteMath::float4* rayPosAndNear, const LiteMath::float4* rayDirAndFar, uint32_t* out_color);

protected:
  uint32_t m_width;
  uint32_t m_height;

  LiteMath::float3   m_camPos;
  LiteMath::float4x4 m_invProjView;

  std::shared_ptr<ISceneObject> m_pAccelStruct;

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
LiteMath::float3 EyeRayDir(float x, float y, float w, float h, LiteMath::float4x4 a_mViewProjInv);

#endif// LOADER_RAYTRACING_H
