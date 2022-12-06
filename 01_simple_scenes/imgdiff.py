import cv2
import sys

# load images
image1 = cv2.imread(sys.argv[1])
image2 = cv2.imread(sys.argv[2])

# compute difference
difference = cv2.subtract(image1, image2)*10

cv2.imshow('image', difference)
cv2.waitKey(0)
cv2.destroyAllWindows()

