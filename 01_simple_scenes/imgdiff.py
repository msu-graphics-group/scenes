import sys
import os
os.environ["OPENCV_IO_ENABLE_OPENEXR"]="1"
import cv2

# then just type in following

image1 = cv2.imread(sys.argv[1], cv2.IMREAD_ANYCOLOR | cv2.IMREAD_ANYDEPTH)
image2 = cv2.imread(sys.argv[2], cv2.IMREAD_ANYCOLOR | cv2.IMREAD_ANYDEPTH)

# compute difference
difference = cv2.absdiff(image1, image2)*10

cv2.imshow('diff*10', difference)
cv2.waitKey(0)
cv2.destroyAllWindows()

