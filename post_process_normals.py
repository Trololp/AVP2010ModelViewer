from PIL import Image
import math
import sys


im = Image.open(sys.argv[1])
im = im.convert('RGBA')

sizex, sizey = im.size[0], im.size[1]

print(sizex, sizey)

#test_px = im.getpixel((19, 426))
#print(test_px)
#print([test_px[3], test_px[1], int(math.sqrt(255*255 - test_px[3]*test_px[3] - test_px[1]*test_px[1])), 255])




#this is bad way to do this but it work. It so slow...
for x in range(sizex):
	for y in range(sizey):
		pixel = im.getpixel((x, y))
		new_pixel = (pixel[3], pixel[1], int(math.sqrt(abs(255*255 - pixel[3]*pixel[3] - pixel[1]*pixel[1]))), 255)
		im.putpixel((x, y), new_pixel)

im.show()

#assume that it is .tga or .dds

im.save(sys.argv[1][:-4] + '_processed.tga', 'TGA')
