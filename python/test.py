from ctypes import *
import tkinter as tk
import sys

class Image(Structure):
    _fields_ = [
        ("width", c_uint32),
        ("height", c_uint32),
        ("decoded_data", POINTER(c_uint8)),
        ("size", c_uint32),
        ("components", c_uint8),
        ("error", c_int)
    ]

# NOTE: First compile the static library and make sure that it's in the same directory as this file
# Define the decode image function using ctypes
libidl = CDLL("./libidl.so")
decode_image = libidl.decode_image
decode_image.argtypes = [POINTER(c_uint8)]
decode_image.restype = Image

if len(sys.argv) < 1:
    print("Error: no arguments passed!")
    sys.exit(1)

# Decode the image file
file = (sys.argv[1]).encode(encoding="utf-8")
char_str_type = c_uint8 * len(file)
image = decode_image(char_str_type(*file))
image_data = bytes(cast(image.decoded_data, POINTER(c_uint8))[:image.size]) # Convert the image data to "bytes" object

# Check for errors
if (image.error):
    print(f"An error occured, terminated the program with the error code: {image.error}\n")
    sys.exit(image.error)

root = tk.Tk()
photo_image = tk.PhotoImage(width=image.width, height=image.height)

# Fill the PhotoImage object with the image data
for y in range(image.height):
    for x in range(image.width):
        pos = 3 * (y * image.width + x)
        color = f'#{image_data[pos]:02x}{image_data[pos + 1]:02x}{image_data[pos + 2]:02x}'
        photo_image.put(color, (x, y))

label = tk.Label(root, image=photo_image)
label.pack()
root.mainloop()
