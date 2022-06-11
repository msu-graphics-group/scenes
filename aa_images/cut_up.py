import cv2, os

sr2 = cv2.dnn_superres.DnnSuperResImpl_create()
sr8 = cv2.dnn_superres.DnnSuperResImpl_create()

sr2.readModel("TF-LapSRN-master/export/LapSRN_x2.pb")
sr8.readModel("TF-LapSRN-master/export/LapSRN_x8.pb")

sr2.setModel("lapsrn",2)
sr8.setModel("lapsrn",8)


fnameIn  = "z_out3.png"
fnameOut = "z_out_16K.png"

def process_folder(path, crop, out_folder):
  os.mkdir(out_folder)
  imgOrigin  = cv2.imread(path + "/" + fnameIn)
  imgUpsamp  = cv2.imread(path + "/" + fnameOut)
  
  imgOriginCrop = imgOrigin[crop[1]: crop[1]+50, crop[0]:crop[0]+50]
  imgUpsampCrop = imgUpsamp[16*crop[1]: 16*(crop[1]+50), 16*crop[0]:16*(crop[0]+50)]

  result1 = sr8.upsample(imgOriginCrop)  
  result2 = sr2.upsample(result1)  

  cv2.imwrite(out_folder + "/cut_origin_" + str(crop[0]) + "_" + str(crop[1]) + ".png", result2)
  cv2.imwrite(out_folder + "/cut_ref_" + str(crop[0]) + "_" + str(crop[1]) + ".png", imgUpsampCrop)


#process_folder("scene1_input", [400,143-13],   "scene1_output1")
#process_folder("scene1_input", [400,843-13],   "scene1_output2")
#process_folder("scene1_input", [520,915-25+1], "scene1_output3")
#process_folder("scene1_input", [590,600-25+1], "scene1_output4")
#process_folder("scene1_input", [540,455-25+1], "scene1_output5")

process_folder("scene2_input", [175,117-50+1],    "scene2_output1")
process_folder("scene2_input", [415,305+25-50],   "scene2_output2")
process_folder("scene2_input", [510,720+50-50+1], "scene2_output3")
process_folder("scene2_input", [225,80+50-50+1],  "scene2_output4")
process_folder("scene2_input", [760,225+40-50+1], "scene2_output5")




