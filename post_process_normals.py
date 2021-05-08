from PIL import Image
import math
import sys


im = Image.open(sys.argv[1])
im = im.convert('RGBA')

sizex, sizey = im.size[0], im.size[1]

print(sizex, sizey)

#this is bad way to do this but it work. It so slow...
for x in range(sizex):
	for y in range(sizey):
		pixel = im.getpixel((x, y))
		#if (255*255*4 - pixel[3]*pixel[3] - pixel[1]*pixel[1] >= 0):
		#	
		#else:
		#	new_pixel = (0,0,255,255)
		new_pixel = (pixel[3], pixel[1], int(math.sqrt(255*255*4 - pixel[3]*pixel[3] - pixel[1]*pixel[1])), 255)#maybe incorrect !!!
		im.putpixel((x, y), new_pixel)

im.show()

#assume that it is .tga or .dds

im.save(sys.argv[1][:-4] + '_processed.tga', 'TGA')