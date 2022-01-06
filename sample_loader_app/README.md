# Scene loader 
This sample demonstrates loading scene from HydraXML format and rendering it with simple ray tracing renderer. 
The renderer traces only primary rays and computes color by sampling predefined color palette with mesh/instance id. 
The renderer uses [embree](https://github.com/embree/embree), which is precompiled and included in this repository 
for your convenience. 

## Building
All dependencies are included in this repo. Binary will be built in the same directory as this README file.

Use CMake as usual to build the sample application.
For example, on Linux from this directory execute:
```
mkdir build
cd build
cmake ..
make
```
On Windows you would usually just open this directory as a CMake project in Visual studio or other IDE supporting CMake. 

## Loader description and usage

### HydraXML format
Overall, scene in HydraXML format consists of XML-description file and binary geometry/texture files, usually located 
in "data" directory alongside the XML file. It is similar to glTF format, however, currently there is no predefined
structure to various scene objects, so you need to interpret XML attributes and values by yourself.  
XML file contains several so-called libraries:
* textures_lib - texture description (resolution, etc.) often referencing to an image file;
* materials_lib - material model description (diffuse component, reflection component, etc.), possibly with references 
to textures_lib;
* geometry_lib - mesh description (byte size, vertices number, primitives number, vertex attributes etc.) referencing 
a geometry file in internal .vsgf format (loader for these files is [located here](loader/cmesh.h));
* lights_lib - light model description (shape, size, distribution, etc.), possibly with reference
  to IES file;
* cam_lib - camera model description (fov, clip planes, position, etc.)
* render_lib - render setting description (resolution, algorithm, etc.), usually for a specific renderer 
(most often [Hydra renderer](http://www.raytracing.ru/))
* scenes - scene description (mesh instances referencing geometry_lib, light instances referencing lights_lib, etc.)  

Details on HydraXML format can be found [here](https://github.com/Ray-Tracing-Systems/HydraAPI/tree/master/doc).

### Loader
Loader uses [pugixml](https://pugixml.org/) to parse HydraXML format.  Loader interface is located in
[hydraxml.h](loader/hydraxml.h).

Simple scene loading example is provided by this sample application - check *RayTracer::LoadScene* 
function [here](render/raytracing.cpp).

More complete example can be found in [vk_graphics_rt repo](https://github.com/msu-graphics-group/vk_graphics_rt/blob/master/src/render/scene_mgr_loaders.cpp).

Now, let's take a look at scene loading process step by step.

Basic usage is to create HydraScene object, load XML scene description file and iterate over objects in each library
(described above - geometry_lib, materials_lib, etc.) you need:
```c++
hydra_xml::HydraScene scene;
scene.LoadState("../01_simple_scenes/instanced_objects.xml");
```

*MeshFiles()* and *TextureFiles()* let you iterate over file paths with range based for:

```c++
for(auto meshPath : scene.MeshFiles())
{
  auto currMesh = cmesh::LoadMeshFromVSGF(meshPath.c_str());
  // store/use loaded mesh data
  // ... 
}
```

*TextureNodes()*, *MaterialNodes()*, *GeomNodes()*, *LightNodes()* and *CameraNodes()* let you iterate over XML nodes with range 
based for. You will need to get desired XML attributes and values by their name (or iterate over them using pugixml 
methods)

```c++
for(auto materialNode : scene.MaterialNodes())
{
  auto node = materialNode.child(L"diffuse").child(L"color");
  if(node != nullptr)
    color = LiteMath::to_float4(hydra_xml::readval3f(node), 0.0f);

  // load other components if needed
  // check for texture references, etc.
  // ...
}
```

*Cameras()* and lets you iterate over *hydra_xml::Camera* structs with range based for (camera description is loaded from 
XML into *hydra_xml::Camera* struct in *hydra_xml::CamIterator* class). You can also load camera model from XML by yourself 
by using *CameraNodes()* method.

*MaterialsGLTF()* analogously lets you iterate over *hydra_xml::gltfMaterialData* structs by converting Hydra Renderer 
material into *approximate* glTF PBRMetallicRoughness material.

```c++
for(auto cam : scene.Cameras())
{
  float aspect   = 1.0f;
  auto worldView = LiteMath::lookAt(LiteMath::float3(cam.pos), LiteMath::float3(cam.lookAt), LiteMath::float3(cam.up));
}
```

Finally, after loading all desired assets from libraries, you will usually iterate over instances in the scene using 
*InstancesGeom()* method to get *hydra_xml::Instance* structs, containing mesh id, matrix and remap list id
(remap lists allow you to change materials assigned to mesh on per instance basis, check 
[HydraXML docs](https://github.com/Ray-Tracing-Systems/HydraAPI/tree/master/doc) for more information).

```c++
for(auto inst : scene.InstancesGeom())
{
  m_pAccelStruct->AddInstance(inst.geomId, inst.matrix);
}
```
