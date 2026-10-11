from pathlib import Path
from PIL import Image, ImageDraw
import math
size=1024
mask=Image.new('L',(size,size)); d=ImageDraw.Draw(mask)
d.rounded_rectangle((48,48,976,976),radius=216,fill=255)
img=Image.new('RGBA',(size,size))
pixels=img.load()
for y in range(size):
    t=y/size
    color=tuple(round(a*(1-t)+b*t) for a,b in zip((67,98,198),(32,42,97)))
    for x in range(size): pixels[x,y]=(*color,mask.getpixel((x,y)))
layer=Image.new('RGBA',(size,size)); paint=ImageDraw.Draw(layer)
paint.ellipse((532,212,772,452),fill='#ffe0a0')
points=[(x,round(675-120*math.sin((x-48)/928*math.pi*1.75))) for x in range(48,977)]
paint.polygon(points+[(976,1024),(48,1024)],fill='#7fa1dd')
points=[(x,round(806-125*math.sin((x-48)/928*math.pi*1.5))) for x in range(48,977)]
paint.polygon(points+[(976,1024),(48,1024)],fill='#c2d6f3')
layer.putalpha(Image.composite(layer.getchannel('A'),Image.new('L',(size,size)),mask))
img.alpha_composite(layer)
img=img.resize((256,256),Image.Resampling.LANCZOS)
img.save('assets/app.png')
img.save('assets/app.ico',sizes=[(16,16),(24,24),(32,32),(48,48),(64,64),(128,128),(256,256)])
