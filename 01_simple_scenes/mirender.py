import mitsuba as mi
import sys
import time

#print(mi.variants()) # ['scalar_rgb', 'scalar_spectral', 'cuda_ad_rgb', 'llvm_ad_rgb']
mi.set_variant("scalar_rgb")

scene = mi.load_file(sys.argv[1])
image = mi.render(scene, spp=1024)

time.sleep(0.5)
mi.util.write_bitmap("y08_sphere_mitsuba3_ggx_metal.png", image, write_async = False)
mi.util.write_bitmap("y08_sphere_mitsuba3_ggx_metal.exr", image, write_async = False)
time.sleep(0.5)
