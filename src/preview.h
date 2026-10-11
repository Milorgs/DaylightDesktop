#pragma once
#include <wincodec.h>
inline HBITMAP themePreview(const WCHAR *path,int width,int height,int radius=8)
{
    IWICImagingFactory *factory=nullptr; IWICBitmapDecoder *decoder=nullptr; IWICBitmapFrameDecode *frame=nullptr;
    IWICBitmapScaler *scaler=nullptr; IWICBitmapClipper *clipper=nullptr; IWICFormatConverter *converter=nullptr; HBITMAP bitmap=nullptr;
    HRESULT result=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_IWICImagingFactory,(void**)&factory);
    if (SUCCEEDED(result)) result=factory->CreateDecoderFromFilename(path,nullptr,GENERIC_READ,WICDecodeMetadataCacheOnDemand,&decoder);
    if (SUCCEEDED(result)) result=decoder->GetFrame(0,&frame);
    UINT sourceWidth=0,sourceHeight=0;
    if (SUCCEEDED(result)) result=frame->GetSize(&sourceWidth,&sourceHeight);
    if (SUCCEEDED(result)&&(sourceWidth==0||sourceHeight==0||sourceWidth>65535||sourceHeight>65535)) result=E_INVALIDARG;
    // Resize the decoder source first, so JPEG decoders can use native downsampling.
    // Crop only the small scaled image; never create a full-resolution intermediate.
    UINT scaledWidth=width,scaledHeight=height;
    if (SUCCEEDED(result)) {
        if ((unsigned long long)sourceWidth*height>(unsigned long long)sourceHeight*width)
            scaledWidth=(UINT)(((unsigned long long)sourceWidth*height+sourceHeight-1)/sourceHeight);
        else scaledHeight=(UINT)(((unsigned long long)sourceHeight*width+sourceWidth-1)/sourceWidth);
        if (scaledWidth>65535||scaledHeight>65535) result=E_INVALIDARG;
    }
    if (SUCCEEDED(result)) result=factory->CreateBitmapScaler(&scaler);
    if (SUCCEEDED(result)) result=scaler->Initialize(frame,scaledWidth,scaledHeight,WICBitmapInterpolationModeFant);
    if (SUCCEEDED(result)) result=factory->CreateBitmapClipper(&clipper);
    if (SUCCEEDED(result)) {
        WICRect crop={((int)scaledWidth-width)/2,((int)scaledHeight-height)/2,width,height};
        result=clipper->Initialize(scaler,&crop);
    }
    if (SUCCEEDED(result)) result=factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(result)) result=converter->Initialize(clipper,GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom);
    if (SUCCEEDED(result)) {
        BITMAPINFO info={}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth=width; info.bmiHeader.biHeight=-height;
        info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; void *bits;
        bitmap=CreateDIBSection(nullptr,&info,DIB_RGB_COLORS,&bits,nullptr,0);
        if (bitmap) {
            result=converter->CopyPixels(nullptr,width*4,width*height*4,(BYTE*)bits);
            if (FAILED(result)) { DeleteObject(bitmap); bitmap=nullptr; }
            else {
                DWORD *pixels=(DWORD*)bits;
                for (int y=0;y<height;++y) for (int x=0;x<width;++x) {
                    if (x>=radius&&x<width-radius) continue;
                    if (y>=radius&&y<height-radius) continue;
                    double cx=x<radius?radius:width-radius,cy=y<radius?radius:height-radius;
                    unsigned int hits=0;
                    for (int sy=0;sy<8;++sy) for (int sx=0;sx<8;++sx) {
                        double dx=x+(sx+0.5)/8-cx,dy=y+(sy+0.5)/8-cy;
                        if (dx*dx+dy*dy<=radius*radius) ++hits;
                    }
                    DWORD original=pixels[y*width+x],next=0;
                    for (int channel=0;channel<4;++channel) next|=((((original>>(channel*8))&255)*hits+32)/64)<<(channel*8);
                    pixels[y*width+x]=next;
                }
            }
        }
    }
    if (converter) converter->Release();
    if (scaler) scaler->Release();
    if (clipper) clipper->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    if (factory) factory->Release();
    return bitmap;
}
