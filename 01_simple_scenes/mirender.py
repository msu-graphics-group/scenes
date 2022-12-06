import mitsuba as mi
import sys
import time

#print(mi.variants()) # ['scalar_rgb', 'scalar_spectral', 'cuda_ad_rgb', 'llvm_ad_rgb']
mi.set_variant("scalar_rgb")

scene = mi.load_file(sys.argv[1])
image = mi.render(scene, spp=1024)

time.sleep(0.5)
#ldrImage = image.convert(mi.Bitmap.PixelFormat.RGB, mi.Struct.Type.UInt8, True)
mi.util.write_bitmap("y02_sphere_mitsuba_lambert.png", image, write_async = False)
mi.util.write_bitmap("y02_sphere_mitsuba_lambert.exr", image, write_async = False)
time.sleep(0.5)
