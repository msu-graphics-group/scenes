import cv2

fnames = ["z_out1_bsp.png", "z_out1_naive.png", "z_out3.png"] # "z_out_16K.png"


def process_folder(path, crops, out_folder):
  for fname in fnames:
    for crop in crops:
      img      = cv2.imread(path + "/" + fname)
      crop_img = img[crop[1]: crop[1]+50, crop[0]:crop[0]+50]
      cv2.imwrite(out_folder + "/cut_" + fname + "_" + str(crop[0]) + "_" + str(crop[1]) + ".png", crop_img)


yOffset = 13
process_folder("scene1_input", [[400,143-yOffset], [400,843-yOffset], [540,543-yOffset], [590,398-yOffset], [520,83-yOffset]], "scene1_output")
