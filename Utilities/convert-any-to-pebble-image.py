#!/usr/bin/env python3

# convert-any-to-pebble-image.py
# routines for converting an image (bmp/jpg/etc) to Pebble/RebbleOS native format
# RebbleOS
#
# Author: Barry Carter <barry.carter@gmail.com>
# Author: Chris Multhaupt <chris.multhaupt@gmail.com>
#
# Public Domain. See RebbleOS LICENSE file

'''

Convert an image to Pebble/RebbleOS
single 8 bit byte per pixel 2 bit colour

'''

import sys
import io
from PIL import Image


def to_6bit(value):
    if(value < 43): return 0
    if(value < 129): return 85
    if(value < 213): return 170
    else: return 255


if len(sys.argv) < 3:
    print("USAGE: %s tintin|snowy|chalk inputfile.jpg|bmp|etc outputfile.raw" % sys.argv[0])
    sys.exit("No Parameters Given")

platform = sys.argv[1]
inputfile = sys.argv[2]
outputfile = sys.argv[3]

if(platform not in ["tintin", "snowy", "chalk"]):
    print("USAGE: %s tintin|snowy|chalk inputfile.jpg|bmp|etc outputfile.raw" % sys.argv[0])
    sys.exit("Invalid platform")

im = Image.open(inputfile)
im.convert('RGBA')
data = im.getdata()

if ((len(data) > (144*168)) and (platform == "tintin" or platform == "snowy") or ((len(data) > (180*180)) and (platform == "chalk"))):
    print("Image is %s. TOO BIG big. I could convert this, but a better tool for the job will give you better results" % len(data))
    im.close()
    sys.exit("Image was too massive")

if (platform == "tintin"):
    print("Tintin not implemented yet")
    sys.exit("Tintin not implemented")

newfile = ""
for pixel in data:
    bitR = pixel[0]
    bitG = pixel[1]
    bitB = pixel[2]
    if(len(pixel)>3):
        bitA = pixel[3]
    else:
        bitA = 0xFF
    #print ("R %x G %x B %x" % (ord(bitR), ord(bitG), ord(bitB)))
    
    newR = to_6bit(bitR)
    newG = to_6bit(bitG)
    newB = to_6bit(bitB)
    newA = to_6bit(bitA)
    #print ("R %x G %x B %x" % (newR, newG, newB))
    newbit = 0
    newbit |= (newR & 0x30)
    newbit |= (newG & 0xC)
    newbit |= (newB & 0x3)
    newbit |= (newA & 0xC0)
    
    #print newbit
    newfile += chr(newbit)
    
# write the new file out    
wfile = open(outputfile,"wb") 
b = bytearray()
b.extend(map(ord,newfile))
wfile.write(b)
wfile.close()
im.close()

wfile = open("rbl_img.raw.h","w") 

cfile = "static const uint8_t splash[] = {" 
count = 0
for bit in newfile:
    if (count % 144) == 0:
        cfile += "\n     "
    cfile += ("0x%02x" % ord(bit))
    # don't print last comma
    if (len(newfile) != count + 1):
        cfile += ", "
    count +=1
    
cfile += "};\n"
wfile.write(cfile)
wfile.close()