import mitsuba as mi
import sys

#print(mi.variants()) # ['scalar_rgb', 'scalar_spectral', 'cuda_ad_rgb', 'llvm_ad_rgb']
mi.set_variant("scalar_rgb")

scene = mi.load_file(sys.argv[1])
image = mi.render(scene, spp=256)

mi.util.write_bitmap("my_first_render.png", image)
mi.util.write_bitmap("my_first_render.exr", image)
