import sys
try:
    from PIL import Image
    print("PIL OK", Image.__name__)
    im = Image.open(sys.argv[1])
    print(im.size, im.mode)
except Exception as e:
    print("NO PIL:", e)
