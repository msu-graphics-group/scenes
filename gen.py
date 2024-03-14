import os

def find_xml_files(directory):
    xml_files = []
    for root, dirs, files in os.walk(directory):
        for file in files:
            if file.endswith(".xml"):
                xml_path = os.path.join(root, file)
                xml_files.append(xml_path)
    return xml_files

directory_path = "."

#######################################################################################################

scenes1 = [
  '01_simple_scenes/bunny_cornell.xml',
  '03_classic_scenes/02_cry_sponza/statex_00001.xml',
  '03_classic_scenes/04_hair_balls/statex_00001.xml',
]


PATH_TO_HYDRA2_FOLDER = "/home/frol/hydra"
PATH_TO_HYDRA3_FOLDER = "/home/frol/hydra3"

# Создаем текстовый файл
sppArray   = [1024] # 64,256,1024
cpuOrGPU   = "--gpu"
intName    = "mispt"
qmc        = "" #"--qmc"

with open("run_hydra3.sh", 'w') as file:
  file.write(f"SCNDIR=\"$PWD\" \n")  
  file.write(f"cd {PATH_TO_HYDRA3_FOLDER} \n")

  for spp in sppArray:

    for scene in scenes1:
      scene2    = "$SCNDIR/" + scene
      sceneFld  = scene.split("/")[0]
      sceneName = scene[:-4]
      opt_suffix = "_optics" if "_optics" in scene else ""
      file.write(f"./hydra {cpuOrGPU} -in {scene2} -integrator {intName} {qmc} -width 1024 -height 1024 -spp {str(spp)} -out $SCNDIR/{sceneFld}/Images/h3{opt_suffix}_{str(spp)}.png \n")

with open("run_hydra2.sh", 'w') as file:
  file.write(f"SCNDIR=\"$PWD\" \n")  
  file.write(f"cd {PATH_TO_HYDRA2_FOLDER} \n")

  for spp in sppArray:

    for scene in scenes1:
      scene2    = "$SCNDIR/" + scene
      sceneFld  = scene.split("/")[0]
      sceneName = scene[:-4]
      opt_suffix = "_optics" if "_optics" in scene else ""
      pathSplitted = scene.split("/")
      pathFolder = pathSplitted[0] + "/" + pathSplitted[1] if len(pathSplitted) > 2 else pathSplitted[0]
      stateF     = pathSplitted[-1]
      file.write(f"./hydra -inputlib $SCNDIR/{pathFolder} -statefile {stateF} -width 1024 -height 1024 -spp {str(spp)} -out $SCNDIR/{sceneFld}/Images/h2{opt_suffix}_{str(spp)}.png -nowindow 1 \n")
    
