import cv2, os

fnames = ["z_out1_bsp.png", "z_out1_naive.png", "z_out3.png", "z_out_1K_ds.png"]


def process_folder(path, crop, out_folder):
  os.mkdir(out_folder)
  for fname in fnames:
    img      = cv2.imread(path + "/" + fname)
    crop_img = img[crop[1]: crop[1]+50, crop[0]:crop[0]+50]
    cv2.imwrite(out_folder + "/cut_" + fname + "_" + str(crop[0]) + "_" + str(crop[1]) + ".png", crop_img)



process_folder("scene1_input", [400,143-13],   "scene1_output1")
process_folder("scene1_input", [400,843-13],   "scene1_output2")
process_folder("scene1_input", [520,915-25+1], "scene1_output3")
process_folder("scene1_input", [590,600-25+1], "scene1_output4")
process_folder("scene1_input", [540,455-25+1], "scene1_output5")

#yOffset = 13
#process_folder("scene1_input", [[400,143-yOffset]],     "scene1_output1")
#process_folder("scene1_input", [[400,843-yOffset]],     "scene1_output2")
#process_folder("scene1_input", [[540,543-100-yOffset]], "scene1_output3")
#process_folder("scene1_input", [[590,600-25+1]],        "scene1_output4")

