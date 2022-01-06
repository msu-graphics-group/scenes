#ifndef HYDRAXML_H
#define HYDRAXML_H

#include "pugixml.hpp"
#include "LiteMath.h"
using namespace LiteMath;

#include <vector>
#include <set>
#include <unordered_map>
#include <iostream>

#if defined(__ANDROID__)
#include <android/asset_manager.h>
#include <android/log.h>
#endif

namespace hydra_xml
{
  std::wstring s2ws(const std::string& str);
  std::string  ws2s(const std::wstring& wstr);
  LiteMath::float4x4 float4x4FromString(const std::wstring &matrix_str);
  LiteMath::float3   read3f(pugi::xml_attribute a_attr);
  LiteMath::float3   read3f(pugi::xml_node a_node);
  LiteMath::float3   readval3f(pugi::xml_node a_node);

  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  
  struct Instance
  {
    uint32_t           geomId = uint32_t(-1); ///< geom id
    uint32_t           rmapId = uint32_t(-1); ///< remap list id, todo: add function to get real remap list by id
    LiteMath::float4x4 matrix;                ///< transform matrix
  };

  struct LightInstance
  {
    uint32_t           instId  = uint32_t(-1);
    uint32_t           lightId = uint32_t(-1);
    pugi::xml_node     instNode;
    pugi::xml_node     lightNode;
    LiteMath::float4x4 matrix;
  };

  struct Camera
  {
    float pos[3];
    float lookAt[3];
    float up[3];
    float fov;
    float nearPlane;
    float farPlane;
  };

  struct PbrMetallicRoughness // 8 * 32 bits = 32 bytes
  {
    float baseColor[4];
    float metallic;
    float roughness;
    int32_t baseColorTexId;
    int32_t metallicRoughnessTexId; // glTF 2.0: metalness - B channel, roughness - G channel
  };

  // NOTE: double-sided flag is ignored for now
  // it probably should be stored somewhere else, because we won't need it on GPU and it will just break the alignment
  struct gltfMaterialData // 16 * 32 bits = 64 bytes
  {
    PbrMetallicRoughness metRoughnessData;
    int32_t normalTexId;

    // glTF 2.0: only R channel,
    // Higher values indicate areas that receive full indirect lighting and lower values indicate no indirect lighting
    int32_t occlusionTexId;

    float emissionColor[3]; // linear multiplier of emission texture
    int32_t emissionTexId;

    float alphaCutoff;
    // alphaMode specifies baseColor alpha interpretation:
    // 0 - opaque, alpha ignored
    // 1 - mask, fully opaque or fully transparent, depending on alpha and alphaCutoff
    // 2 - blend, alpha used to composite with OVER operator
    int32_t alphaMode;
  };

  std::ostream& operator<<(std::ostream& os, gltfMaterialData gltfMat);

  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	class LocIterator //
	{
	  friend class pugi::xml_node;
    friend class pugi::xml_node_iterator;
  
	public:
  
		// Default constructor
		LocIterator() = default;
		LocIterator(const pugi::xml_node_iterator& a_iter, const std::string& a_str) : m_iter(a_iter), m_libraryRootDir(a_str) {}
  
		// Iterator operators
		bool operator==(const LocIterator& rhs) const { return m_iter == rhs.m_iter;}
		bool operator!=(const LocIterator& rhs) const { return (m_iter != rhs.m_iter); }
  
    std::string operator*() const 
    { 
      auto attr    = m_iter->attribute(L"loc");
      auto meshLoc = ws2s(std::wstring(attr.as_string()));
      return m_libraryRootDir + "/" + meshLoc;
    }
  
		const LocIterator& operator++() { ++m_iter; return *this; }
		LocIterator operator++(int)     { m_iter++; return *this; }
  
		const LocIterator& operator--() { --m_iter; return *this; }
		LocIterator operator--(int)     { m_iter--; return *this; }
  
  private:
    pugi::xml_node_iterator m_iter;
    std::string m_libraryRootDir;
	};

  class InstIterator //
	{
	  friend class pugi::xml_node;
    friend class pugi::xml_node_iterator;
  
	public:
  
		// Default constructor
		InstIterator() = default;
		InstIterator(const pugi::xml_node_iterator& a_iter, const pugi::xml_node_iterator& a_end) : m_iter(a_iter), m_end(a_end) {}
  
		// Iterator operators
		bool operator==(const InstIterator& rhs) const { return m_iter == rhs.m_iter;}
		bool operator!=(const InstIterator& rhs) const { return (m_iter != rhs.m_iter); }
  
    Instance operator*() const 
    { 
      Instance inst;
      inst.geomId = m_iter->attribute(L"mesh_id").as_uint();
      inst.rmapId = m_iter->attribute(L"rmap_id").as_uint();
      inst.matrix = float4x4FromString(m_iter->attribute(L"matrix").as_string());
      return inst;
    }
  
		const InstIterator& operator++() { do ++m_iter; while(m_iter != m_end && std::wstring(m_iter->name()) != L"instance"); return *this; }
		InstIterator operator++(int)     { do m_iter++; while(m_iter != m_end && std::wstring(m_iter->name()) != L"instance"); return *this; }
  
		const InstIterator& operator--() { do --m_iter; while(m_iter != m_end && std::wstring(m_iter->name()) != L"instance"); return *this; }
		InstIterator operator--(int)     { do m_iter--; while(m_iter != m_end && std::wstring(m_iter->name()) != L"instance"); return *this; }
  
  private:
    pugi::xml_node_iterator m_iter;
    pugi::xml_node_iterator m_end;
	};

  class CamIterator //
	{
	  friend class pugi::xml_node;
    friend class pugi::xml_node_iterator;
  
	public:
  
		// Default constructor
		CamIterator() = default;
		CamIterator(const pugi::xml_node_iterator& a_iter) : m_iter(a_iter) {}
  
		// Iterator operators
		bool operator==(const CamIterator& rhs) const { return m_iter == rhs.m_iter;}
		bool operator!=(const CamIterator& rhs) const { return (m_iter != rhs.m_iter); }
  
    Camera operator*() const 
    { 
      Camera cam = {};
      cam.fov       = m_iter->child(L"fov").text().as_float(); 
      cam.nearPlane = m_iter->child(L"nearClipPlane").text().as_float();
      cam.farPlane  = m_iter->child(L"farClipPlane").text().as_float();  
      
      LiteMath::float3 pos    = hydra_xml::read3f(m_iter->child(L"position"));
      LiteMath::float3 lookAt = hydra_xml::read3f(m_iter->child(L"look_at"));
      LiteMath::float3 up     = hydra_xml::read3f(m_iter->child(L"up"));
      for(int i=0;i<3;i++)
      {
        cam.pos   [i] = pos[i];
        cam.lookAt[i] = lookAt[i];
        cam.up    [i] = up[i];
      }
      return cam;
    }
  
		const CamIterator& operator++() { ++m_iter; return *this; }
		CamIterator operator++(int)     { m_iter++; return *this; }
  
		const CamIterator& operator--() { --m_iter; return *this; }
		CamIterator operator--(int)     { m_iter--; return *this; }
  
  private:
    pugi::xml_node_iterator m_iter;
	};

  class MaterialIteratorGLTF //
  {
    friend class pugi::xml_node;
    friend class pugi::xml_node_iterator;

  public:

    // Default constructor
    MaterialIteratorGLTF() = default;
    MaterialIteratorGLTF(const pugi::xml_node_iterator& a_iter) : m_iter(a_iter) {}

    // Iterator operators
    bool operator==(const MaterialIteratorGLTF& rhs) const { return m_iter == rhs.m_iter;}
    bool operator!=(const MaterialIteratorGLTF& rhs) const { return (m_iter != rhs.m_iter); }

    gltfMaterialData operator*() const
    {
      gltfMaterialData materialData = {};
      materialData.alphaCutoff    = 0.5f;
      materialData.alphaMode      = 0;
      materialData.emissionTexId  = -1;
      materialData.normalTexId    = -1;
      materialData.occlusionTexId = -1;
      materialData.metRoughnessData.baseColorTexId = -1;
      materialData.metRoughnessData.metallicRoughnessTexId = -1;

      if(m_iter->child(L"opacity").child(L"texture"))
      {
        materialData.alphaMode = 1;
      }

      LiteMath::float3 emission = hydra_xml::read3f(m_iter->child(L"emission").child(L"color").attribute(L"val"));
      LiteMath::float3 diffuse  = hydra_xml::read3f(m_iter->child(L"diffuse").child(L"color").attribute(L"val"));
      LiteMath::float3 reflect  = hydra_xml::read3f(m_iter->child(L"reflectivity").child(L"color").attribute(L"val"));

      // determine where to take the base color
      bool diffColor = true;
      materialData.metRoughnessData.baseColor[0] = diffuse.x;
      materialData.metRoughnessData.baseColor[1] = diffuse.y;
      materialData.metRoughnessData.baseColor[2] = diffuse.z;
      if(diffuse.x < LiteMath::EPSILON && diffuse.y < LiteMath::EPSILON && diffuse.z < LiteMath::EPSILON)
      {
        diffColor = false;
        materialData.metRoughnessData.baseColor[0] = reflect.x;
        materialData.metRoughnessData.baseColor[1] = reflect.y;
        materialData.metRoughnessData.baseColor[2] = reflect.z;
      }

      materialData.metRoughnessData.roughness = 1.0f - m_iter->child(L"reflectivity").child(L"glossiness").attribute(L"val").as_float();

      float ior = m_iter->child(L"reflectivity").child(L"fresnel_ior").attribute(L"val").as_float();
      materialData.metRoughnessData.metallic = ior < LiteMath::EPSILON ? 0 : powf((1 - ior) / (1 + ior), 2);

      materialData.emissionColor[0] = emission.x;
      materialData.emissionColor[1] = emission.y;
      materialData.emissionColor[2] = emission.z;

      // texture IDs
      //
      auto emissionColorTex = m_iter->child(L"emission").child(L"color").child(L"texture");
      if(emissionColorTex)
      {
        materialData.emissionTexId = emissionColorTex.attribute(L"id").as_int();
      }

      auto baseColorTex = m_iter->child(L"diffuse").child(L"color").child(L"texture");
      if(!diffColor)
      {
        baseColorTex = m_iter->child(L"reflectivity").child(L"color").child(L"texture");
      }
      if(baseColorTex)
      {
        materialData.metRoughnessData.baseColorTexId = baseColorTex.attribute(L"id").as_int();
      }

      auto roughnessTex = m_iter->child(L"reflectivity").child(L"glossiness").child(L"texture");
      if(roughnessTex)
      {
        materialData.metRoughnessData.metallicRoughnessTexId = roughnessTex.attribute(L"id").as_int();
      }

      auto displaceType = std::wstring(m_iter->child(L"displacement").attribute(L"type").as_string());
//      if(displaceType == L"height_bump")
      {
        auto normalTex = m_iter->child(L"displacement").child(L"height_map").child(L"texture");
        if(normalTex)
        {
          materialData.normalTexId = normalTex.attribute(L"id").as_int();
        }
      }
        return materialData;
    }

    const MaterialIteratorGLTF& operator++() { ++m_iter; return *this; }
    MaterialIteratorGLTF operator++(int)     { m_iter++; return *this; }

    const MaterialIteratorGLTF& operator--() { --m_iter; return *this; }
    MaterialIteratorGLTF operator--(int)     { m_iter--; return *this; }

  private:
    pugi::xml_node_iterator m_iter;
  };

  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  struct HydraScene
  {
  public:
    HydraScene() = default;
    ~HydraScene() = default;  
    
    #if defined(__ANDROID__)
    int LoadState(AAssetManager* mgr, const std::string &path);
    #else
    int LoadState(const std::string &path);
    #endif  

    //// use this functions with C++11 range for 
    //
    pugi::xml_object_range<pugi::xml_node_iterator> TextureNodes()  { return m_texturesLib.children();  } 
    pugi::xml_object_range<pugi::xml_node_iterator> MaterialNodes() { return m_materialsLib.children(); }
    pugi::xml_object_range<pugi::xml_node_iterator> GeomNodes()     { return m_geometryLib.children();  }
    pugi::xml_object_range<pugi::xml_node_iterator> LightNodes()    { return m_lightsLib.children();    }
    pugi::xml_object_range<pugi::xml_node_iterator> CameraNodes()   { return m_cameraLib.children();    }
    
    //// please also use this functions with C++11 range for
    //
    pugi::xml_object_range<LocIterator> MeshFiles()    { return {LocIterator(m_geometryLib.begin(), m_libraryRootDir),
                                                                 LocIterator(m_geometryLib.end(), m_libraryRootDir)}; }

    pugi::xml_object_range<LocIterator> TextureFiles() { return {LocIterator(m_texturesLib.begin(), m_libraryRootDir),
                                                                 LocIterator(m_texturesLib.end(), m_libraryRootDir)}; }

    pugi::xml_object_range<InstIterator> InstancesGeom() { return {InstIterator(m_sceneNode.child(L"scene").child(L"instance"), m_sceneNode.child(L"scene").end()),
                                                                   InstIterator(m_sceneNode.child(L"scene").end(), m_sceneNode.child(L"scene").end())}; }
    
    std::vector<LightInstance> InstancesLights(uint32_t a_sceneId = 0);

    pugi::xml_object_range<CamIterator> Cameras() { return {CamIterator(m_cameraLib.begin()),
                                                            CamIterator(m_cameraLib.end())}; }

    pugi::xml_object_range<MaterialIteratorGLTF> MaterialsGLTF() { return {MaterialIteratorGLTF(m_materialsLib.begin()),
        MaterialIteratorGLTF(m_materialsLib.end())}; }

    std::vector<LiteMath::float4x4> GetAllInstancesOfMeshLoc(const std::string& a_loc) const 
    { 
      auto pFound = m_instancesPerMeshLoc.find(a_loc);
      if(pFound == m_instancesPerMeshLoc.end())
        return {};
      else
        return pFound->second; 
    }
    
  private:
    void parseInstancedMeshes(pugi::xml_node a_scenelib, pugi::xml_node a_geomlib);
    void LogError(const std::string &msg);  
    
    std::set<std::string> unique_meshes;
    std::string m_libraryRootDir;
    pugi::xml_node m_texturesLib ; 
    pugi::xml_node m_materialsLib; 
    pugi::xml_node m_geometryLib ; 
    pugi::xml_node m_lightsLib   ;
    pugi::xml_node m_cameraLib   ; 
    pugi::xml_node m_settingsNode; 
    pugi::xml_node m_sceneNode   ; 
    pugi::xml_document m_xmlDoc;

    std::unordered_map<std::string, std::vector<LiteMath::float4x4> > m_instancesPerMeshLoc;
  };

  
}

#endif //HYDRAXML_H
