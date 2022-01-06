# Scenes
Set of scenes for educational and research purpose in computer graphics applications.

# Table of Contents  <a name="top"/>
- [01_simple_scenes](#01_simple_scenes)
- [02_casual_effects](#02_casual_effects)
- [03_classic_scenes](#03_classic_scenes)
- [04_orca](#04_orca)
- [05_interiors](#05_interiors)
- [06_exteriors](#06_exteriors)
- [07_single_mesh](#07_single_mesh)
- [08_cars_animation](#08_cars_animation)
- [09_orb](#09_orb)

Scenes in this repository are in the in Hydra Renderer XML format ([HydraAPI](https://github.com/Ray-Tracing-Systems/HydraAPI),
[Hydra renderer](http://www.raytracing.ru/)), check sample application which loads scene and renders it with simple
ray tracing renderer using [embree](https://github.com/embree/embree) in [sample_loader_app](sample_loader_app) 
as well as [loader guide](sample_loader_app/README.md).

## Showcase
### [01_simple_scenes](01_simple_scenes) [[back to contents]](#top) <a name="01_simple_scenes"/>
| Model                                                             | Screenshot                                                          |
|-------------------------------------------------------------------|:-------------------------------------------------------------------:|
| [Bunny cornell](01_simple_scenes/bunny_cornell.xml)               | ![](01_simple_scenes/screenshots/bunny_cornell.jpeg)                |
| [Bunny plane](01_simple_scenes/bunny_plane.xml)                   | ![](01_simple_scenes/screenshots/bunny_plane.jpeg)                  |
| [Instanced objects](01_simple_scenes/instanced_objects.xml)       | ![](01_simple_scenes/screenshots/instanced_objects.jpeg)            |


### [02_casual_effects](https://disk.yandex.ru/d/M_qbkejoOYun1Q) [[back to contents]](#top) <a name="02_casual_effects"/>
Converted models from Morgan McGuire's [Computer Graphics Archive](https://casual-effects.com/data).

Actual data is stored in the cloud (Yandex disk) since it is quite large.

| Model                                                                                | Screenshot                                                     |
|--------------------------------------------------------------------------------------|:--------------------------------------------------------------:|
| [BMW](https://disk.yandex.ru/d/M_qbkejoOYun1Q/bmw)                                   | ![](02_casual_effects/screenshots/bmw.jpeg)                    |
| [Breakfast room](https://disk.yandex.ru/d/M_qbkejoOYun1Q/breakfast_room)             | ![](02_casual_effects/screenshots/breakfast_room.jpeg)         |
| [Buddha](https://disk.yandex.ru/d/M_qbkejoOYun1Q/buddha)                             | ![](02_casual_effects/screenshots/buddha.jpeg)                 |
| [Chestnut](https://disk.yandex.ru/d/M_qbkejoOYun1Q/chestnut)                         | ![](02_casual_effects/screenshots/chestnut.jpeg)               |
| [Clouds](https://disk.yandex.ru/d/M_qbkejoOYun1Q/clouds)                             | ![](02_casual_effects/screenshots/clouds.jpeg)                 |
| [Conference](https://disk.yandex.ru/d/M_qbkejoOYun1Q/conference)                     | ![](02_casual_effects/screenshots/conference.jpeg)             |
| [Cornell box sphere](https://disk.yandex.ru/d/M_qbkejoOYun1Q/cornell_box_sphere)     | ![](02_casual_effects/screenshots/cornellbox_spheres.jpeg)     |
| [Cornell box water](https://disk.yandex.ru/d/M_qbkejoOYun1Q/cornell_box_water)       | ![](02_casual_effects/screenshots/cornellbox_water.jpeg)       |
| [Crytek Sponza](https://disk.yandex.ru/d/M_qbkejoOYun1Q/crytek_sponza)               | ![](02_casual_effects/screenshots/crytek_sponza.jpeg)          |
| [Dragon](https://disk.yandex.ru/d/M_qbkejoOYun1Q/dragon)                             | ![](02_casual_effects/screenshots/dragon.jpeg)                 |
| [Erato](https://disk.yandex.ru/d/M_qbkejoOYun1Q/erato)                               | ![](02_casual_effects/screenshots/erato.jpeg)                  |
| [Fireplace room](https://disk.yandex.ru/d/M_qbkejoOYun1Q/fireplace_room)             | ![](02_casual_effects/screenshots/fireplace_room.jpeg)         |
| [Holodeck](https://disk.yandex.ru/d/M_qbkejoOYun1Q/holodeck)                         | ![](02_casual_effects/screenshots/holodeck.jpeg)               |
| [Indonesian statue](https://disk.yandex.ru/d/M_qbkejoOYun1Q/indonesian_statue)       | ![](02_casual_effects/screenshots/indonesian.jpeg)             |
| [Living room](https://disk.yandex.ru/d/M_qbkejoOYun1Q/living_room)                   | ![](02_casual_effects/screenshots/living_room.jpeg)            |
| [Lost empire](https://disk.yandex.ru/d/M_qbkejoOYun1Q/lost_empire)                   | ![](02_casual_effects/screenshots/lost_empire.jpeg)            |
| [Lpshead](https://disk.yandex.ru/d/M_qbkejoOYun1Q/lpshead)                           | ![](02_casual_effects/screenshots/lpshead.jpeg)                |
| [Mitsuba knob](https://disk.yandex.ru/d/M_qbkejoOYun1Q/mitsuba_knob)                 | ![](02_casual_effects/screenshots/mitsuba_knob.jpeg)           |
| [Mori knob](https://disk.yandex.ru/d/M_qbkejoOYun1Q/mori_knob)                       | ![](02_casual_effects/screenshots/mori_knob.jpeg)              |
| [Pine](https://disk.yandex.ru/d/M_qbkejoOYun1Q/pine)                                 | ![](02_casual_effects/screenshots/pine.jpeg)                   |
| [Powerplant](https://disk.yandex.ru/d/M_qbkejoOYun1Q/powerplant)                     | ![](02_casual_effects/screenshots/powerplant.jpeg)             |
| [Roadbike](https://disk.yandex.ru/d/M_qbkejoOYun1Q/roadbike)                         | ![](02_casual_effects/screenshots/roadbike.jpeg)               |
| [Rungholt](https://disk.yandex.ru/d/M_qbkejoOYun1Q/rungholt)                         | ![](02_casual_effects/screenshots/rungholt.jpeg)               |
| [Salle de bain](https://disk.yandex.ru/d/M_qbkejoOYun1Q/salle_de_bain)               | ![](02_casual_effects/screenshots/salle_de_bain.jpeg)          |
| [Serapis](https://disk.yandex.ru/d/M_qbkejoOYun1Q/serapis)                           | ![](02_casual_effects/screenshots/serapis.jpeg)                |
| [Sibenik](https://disk.yandex.ru/d/M_qbkejoOYun1Q/sibenik)                           | ![](02_casual_effects/screenshots/sibenik.jpeg)                |
| [Sports car](https://disk.yandex.ru/d/M_qbkejoOYun1Q/sportscar)                      | ![](02_casual_effects/screenshots/sportscar.jpeg)              |
| [Vokselia spawn](https://disk.yandex.ru/d/M_qbkejoOYun1Q/vokselia_spawn)             | ![](02_casual_effects/screenshots/vokselia_spawn.jpeg)         |
| [White oak](https://disk.yandex.ru/d/M_qbkejoOYun1Q/white_oak)                       | ![](02_casual_effects/screenshots/white_oak.jpeg)              |

### [03_classic_scenes](https://disk.yandex.ru/d/dDAqgrNeV92_kw) [[back to contents]](#top) <a name="03_classic_scenes"/>
Classic 3d scenes used in computer graphics research.

Actual data is stored in the cloud (Yandex disk) since it is quite large.

| Model                                                                     | Screenshot                                                  |
|---------------------------------------------------------------------------|:-----------------------------------------------------------:|
| [Sponza](https://disk.yandex.ru/d/dDAqgrNeV92_kw/01_sponza)               | ![](03_classic_scenes/screenshots/sponza.jpeg)              |
| [Crytek Sponza](https://disk.yandex.ru/d/dDAqgrNeV92_kw/02_cry_sponza)    | ![](03_classic_scenes/screenshots/crysponza.jpeg)           |
| [San Miguel](https://disk.yandex.ru/d/dDAqgrNeV92_kw/03_san_miguel)       | ![](03_classic_scenes/screenshots/san_miguel.jpeg)          |
| [Hairballs](https://disk.yandex.ru/d/dDAqgrNeV92_kw/04_hair_balls)        | ![](03_classic_scenes/screenshots/hairballs.jpeg)           |

### [04_orca](https://disk.yandex.ru/d/ri_J0wuoZiP34w) [[back to contents]](#top) <a name="04_orca"/>
Scenes from [Open Research Content Archive](https://developer.nvidia.com/orca)

Actual data is stored in the cloud (Yandex disk) since it is quite large.

| Model                                                                                         | Screenshot                                        |
|-----------------------------------------------------------------------------------------------|:-------------------------------------------------:|
| [Amazon Lumberyard Bistro Exterior](https://disk.yandex.ru/d/ri_J0wuoZiP34w/bistro_exterior)  | ![](04_orca/screenshots/bistro_exterior.jpeg)     |
| [Amazon Lumberyard Bistro Interior](https://disk.yandex.ru/d/ri_J0wuoZiP34w/bistro_interior)  | ![](04_orca/screenshots/bistro_interior.jpeg)     |
| [UE4 Sun Temple](https://disk.yandex.ru/d/ri_J0wuoZiP34w/suntemple)                           | ![](04_orca/screenshots/suntemple.jpeg)           |

### [05_interiors](https://disk.yandex.ru/d/TqcjyFsrSLelbw) [[back to contents]](#top) <a name="05_interiors"/>
Various interior scenes made for [Hydra render](https://github.com/Ray-Tracing-Systems/HydraCore) 

Actual data is stored in the cloud (Yandex disk) since it is quite large.

| Model                                                                     | Screenshot                                        |
|---------------------------------------------------------------------------|:-------------------------------------------------:|
| [scene_08](https://disk.yandex.ru/d/TqcjyFsrSLelbw/scene_08)              | ![](05_interiors/screenshots/scene_08.jpeg)       |
| [scene_10](https://disk.yandex.ru/d/TqcjyFsrSLelbw/scene_10)              | ![](05_interiors/screenshots/scene_10.jpeg)       |
| [scene_21](https://disk.yandex.ru/d/TqcjyFsrSLelbw/scene_21)              | ![](05_interiors/screenshots/scene_21.jpeg)       |

### [06_exteriors](https://disk.yandex.ru/d/MetYef2wCEdiag) [[back to contents]](#top) <a name="06_exteriors"/>
Various exterior scenes made for [Hydra render](https://github.com/Ray-Tracing-Systems/HydraCore)

Actual data is stored in the cloud (Yandex disk) since it is quite large.

| Model                                                                     | Screenshot                                        |
|---------------------------------------------------------------------------|:-------------------------------------------------:|
| [Road scene](https://disk.yandex.ru/d/MetYef2wCEdiag/RoadScenelib)        | ![](06_exteriors/screenshots/road_scene.jpeg)     |


### [07_single_mesh](https://disk.yandex.ru/d/RhDT-ty4hi7b_Q) [[back to contents]](#top) <a name="07_single_mesh"/>
Collection of various single mesh scene

Actual data is stored in the cloud (Yandex disk) since it is quite large.

| Model                                                                     | Screenshot                                        |
|---------------------------------------------------------------------------|:-------------------------------------------------:|
| [asteroid_01](https://disk.yandex.ru/d/RhDT-ty4hi7b_Q)                    | ![](07_single_mesh/screenshots/asteroid01.jpeg)   |
| [asteroid_02](https://disk.yandex.ru/d/RhDT-ty4hi7b_Q)                    | ![](07_single_mesh/screenshots/asteroid02.jpeg)   |
| [asteroid_03](https://disk.yandex.ru/d/RhDT-ty4hi7b_Q)                    | ![](07_single_mesh/screenshots/asteroid03.jpeg)   |
| [asteroid_04](https://disk.yandex.ru/d/RhDT-ty4hi7b_Q)                    | ![](07_single_mesh/screenshots/asteroid04.jpeg)   |
| [asteroid_05](https://disk.yandex.ru/d/RhDT-ty4hi7b_Q)                    | ![](07_single_mesh/screenshots/asteroid05.jpeg)   |
| [starship_01](https://disk.yandex.ru/d/RhDT-ty4hi7b_Q)                    | ![](07_single_mesh/screenshots/starship01.jpeg)   |
| [starship_02](https://disk.yandex.ru/d/RhDT-ty4hi7b_Q)                    | ![](07_single_mesh/screenshots/starship02.jpeg)   |



### [08_cars_animation](https://disk.yandex.ru/d/pV3eVRMZeCXpxg) [[back to contents]](#top) <a name="08_cars_animation"/>
Animation sequences of traffic modeling.
Actual data is stored in the cloud (Yandex disk) since it is quite large.

### [09_orb](https://disk.yandex.ru/d/n8SzslkmaYHFow) [[back to contents]](#top) <a name="09_orb"/>
Collection of scenes used in [Open Render Benchmark (ORB) paper](https://lppm3.ru/files/journal/XLV/MathMontXLV-Frolov.pdf) (in russian)
Some of the scenes are duplicates from other sections but generally have better tuned materials/light for Hydra renderer.

Actual data is stored in the cloud (Yandex disk) since it is quite large.

| Model                                                                                                       | Screenshot                                                  |
|-------------------------------------------------------------------------------------------------------------|:-----------------------------------------------------------:|
| [L1.1_cornell_box_hydra](https://disk.yandex.ru/d/n8SzslkmaYHFow/L1.1_cornell_box_hydra)                    | ![](09_orb/screenshots/L1.1_cornell_box_hydra.jpg)          |
| [L1.2_cornell_box_hydra](https://disk.yandex.ru/d/n8SzslkmaYHFow/L1.2_cornell_box_hydra)                    | ![](09_orb/screenshots/L1.2_cornell_box_hydra.jpg)          |
| [L1.3_cornell_box_hydra](https://disk.yandex.ru/d/n8SzslkmaYHFow/L1.3_cornell_box_hydra)                    | ![](09_orb/screenshots/L1.3_cornell_box_hydra.jpg)          |
| [L1.4_Box_Veach_hydra](https://disk.yandex.ru/d/n8SzslkmaYHFow/L1.4_Box_Veach_hydra)                        | ![](09_orb/screenshots/L1.4_Box_Veach_hydra.jpg)            |
| [L1.5_caustics_hydra](https://disk.yandex.ru/d/n8SzslkmaYHFow/L1.5_caustics_hydra)                          | ![](09_orb/screenshots/L1.5_caustics_hydra.jpg)             |
| [L1.6_glass_box_with_torus_hydra](https://disk.yandex.ru/d/n8SzslkmaYHFow/L1.6_glass_box_with_torus_hydra)  | ![](09_orb/screenshots/L1.6_glass_box_with_torus_hydra.jpg) |
| [L2.2_Cry_Sponza_hydra](https://disk.yandex.ru/d/n8SzslkmaYHFow/L2.2_Cry_Sponza_hydra)                      | ![](09_orb/screenshots/L2.2_Cry_Sponza_hydra.jpg)           |
| [L2.3_San_Miguel_hydra](https://disk.yandex.ru/d/n8SzslkmaYHFow/L2.3_San_Miguel_hydra)                      | ![](09_orb/screenshots/L2.3_San_Miguel_hydra.jpg)           |
| [L3.1_Hair_ball_hydra](https://disk.yandex.ru/d/n8SzslkmaYHFow/L3.1_Hair_ball_hydra)                        | ![](09_orb/screenshots/L3.1_Hair_ball_hydra.jpg)            |
| [L3.2_exterior_hydra](https://disk.yandex.ru/d/n8SzslkmaYHFow/L3.2_exterior_hydra)                          | ![](09_orb/screenshots/L3.2_exterior_hydra.jpg)             |
| [L4.1_statues_hydra](https://disk.yandex.ru/d/n8SzslkmaYHFow/L4.1_statues_hydra)                            | ![](09_orb/screenshots/L4.1_statues_hydra.jpg)              |
| [L5.1_windows_lights](https://disk.yandex.ru/d/n8SzslkmaYHFow/L5.1_windows_lights)                          | ![](09_orb/screenshots/L5.1_windows_lights.jpg)             |
| [L6.2_fireplace_hydra](https://disk.yandex.ru/d/n8SzslkmaYHFow/L6.2_fireplace_hydra)                        | ![](09_orb/screenshots/L6.2_fireplace_hydra.jpg)            |
| [L10.1_Room_Veach_hydra](https://disk.yandex.ru/d/n8SzslkmaYHFow/L10.1_Room_Veach_hydra)                    | ![](09_orb/screenshots/L10.1_Room_Veach_hydra.jpg)          |
| [L10.2_glossy_room_hydra](https://disk.yandex.ru/d/n8SzslkmaYHFow/L10.2_glossy_room_hydra)                  | ![](09_orb/screenshots/L10.2_glossy_room_hydra.jpg)         |
| [L10.4_glossy_kitchen_hydra](https://disk.yandex.ru/d/n8SzslkmaYHFow/L10.4_glossy_kitchen_hydra)            | ![](09_orb/screenshots/L10.4_glossy_kitchen_hydra.jpg)      |
| [L11_selfillum_hydra](https://disk.yandex.ru/d/n8SzslkmaYHFow/L11_selfillum_hydra)                          | ![](09_orb/screenshots/L11_selfillum_hydra.jpg)             |







