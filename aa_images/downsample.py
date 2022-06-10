import cv2
fnameIn  = "z_out_16K.png"
fnameOut = "z_out_1K_ds.png"

for folder in ["scene1_input"]:
  img   = cv2.imread(folder + "/" + fnameIn)
  small = cv2.resize(img, (1024, 1024), interpolation = cv2.INTER_AREA)
  cv2.imwrite(folder + "/" + fnameOut, small) 
