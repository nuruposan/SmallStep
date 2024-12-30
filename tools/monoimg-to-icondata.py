# monoimg-to-icondata.py
# This is a script for creating binary bitmap data used by AppUI class.
# When you run this from the CLI, a file dialog will appear, then select 
# an icon image file to use in your M5Stack app.
# The supported size of an inputimage is up to 256x256.
#
# To run on Ubuntu:
# $ sudo bash -c "apt update && apt install python3-tk python3-opencv -y"
# $ python3 monoimg-to-icondata.py
#
# To run in other platforms (untested):
# $ pip install opencv-python
# $ python3 monoimg-to-icondata.py

import tkinter.filedialog
import cv2

ftypes = [("monochrome icons", "*")]
imgname = tkinter.filedialog.askopenfilename(filetypes=ftypes)

if (imgname == ""): quit()

img = cv2.imread(imgname)
print("// %s" % (imgname))
print("const uint8_t ICON_NAME[] = {")
print("  %d, %d, 0, 0, // width, height, reserved, reserved" % (img.shape[1], img.shape[0]))
for y in range(img.shape[0]):
    print("  ", end="")
    for x in range(img.shape[1]):
        if (x % 8) == 0: print("0b", end="")
        px = 1 if (img[y,x][0] > 0) or (img[y,x][1] > 0) or (img[y,x][2] > 0) else 0
        print(px, end="")
        if (x % 8) == 7: print(", ", end="")
    print(" // %2d" % (y))
print("};")
