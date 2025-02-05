#include <coreinit/cache.h>
#include <coreinit/screen.h>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <tga_reader.h>
#include <utils/DrawUtils.h>


// buffer width
#define TV_WIDTH  0x500
#define DRC_WIDTH 0x380

bool DrawUtils::isBackBuffer;

uint8_t *DrawUtils::tvBuffer = nullptr;
uint32_t DrawUtils::tvSize = 0;
uint8_t *DrawUtils::drcBuffer = nullptr;
uint32_t DrawUtils::drcSize = 0;
static SFT pFont = {};

static Color font_col(0xFFFFFFFF);

bool DrawUtils::redraw = true;

template<typename T>
inline typename std::unique_ptr<T> make_unique_nothrow(size_t num) noexcept {
    return std::unique_ptr<T>(new (std::nothrow) std::remove_extent_t<T>[num]());
}

void DrawUtils::setRedraw(bool value) {
    redraw = value;
}

void DrawUtils::initBuffers(void *tvBuffer_, uint32_t tvSize_, void *drcBuffer_, uint32_t drcSize_) {
    DrawUtils::tvBuffer = (uint8_t *) tvBuffer_;
    DrawUtils::tvSize = tvSize_;
    DrawUtils::drcBuffer = (uint8_t *) drcBuffer_;
    DrawUtils::drcSize = drcSize_;
}

void DrawUtils::beginDraw() {
    uint32_t pixel = *(uint32_t *) tvBuffer;

    // check which buffer is currently used
    OSScreenPutPixelEx(SCREEN_TV, 0, 0, 0xABCDEF90);
    if (*(uint32_t *) tvBuffer == 0xABCDEF90) {
        isBackBuffer = false;
    } else {
        isBackBuffer = true;
    }

    // restore the pixel we used for checking
    *(uint32_t *) tvBuffer = pixel;
}

void DrawUtils::endDraw() {
    // OSScreenFlipBuffersEx already flushes the cache?
    // DCFlushRange(tvBuffer, tvSize);
    // DCFlushRange(drcBuffer, drcSize);

    OSScreenFlipBuffersEx(SCREEN_DRC);
    OSScreenFlipBuffersEx(SCREEN_TV);
}

void DrawUtils::clear(Color col) {
    OSScreenClearBufferEx(SCREEN_TV, col.color);
    OSScreenClearBufferEx(SCREEN_DRC, col.color);
}

void DrawUtils::drawPixel(uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    float opacity = a / 255.0f;

    // put pixel in the drc buffer
    uint32_t i = (x + y * DRC_WIDTH) * 4;
    if (i + 3 < drcSize / 2) {
        if (isBackBuffer) {
            i += drcSize / 2;
        }
        if (a == 0xFF) {
            drcBuffer[i] = r;
            drcBuffer[i + 1] = g;
            drcBuffer[i + 2] = b;
        } else {
            drcBuffer[i] = r * opacity + drcBuffer[i] * (1 - opacity);
            drcBuffer[i + 1] = g * opacity + drcBuffer[i + 1] * (1 - opacity);
            drcBuffer[i + 2] = b * opacity + drcBuffer[i + 2] * (1 - opacity);
        }
    }

    uint32_t USED_TV_WIDTH = TV_WIDTH;
    float scale = 1.5f;
    if (DrawUtils::tvSize == 0x00FD2000) {
        USED_TV_WIDTH = 1920;
        scale = 2.25f;
    }

    // scale and put pixel in the tv buffer
    for (uint32_t yy = (y * scale); yy < ((y * scale) + (uint32_t) scale); yy++) {
        for (uint32_t xx = (x * scale); xx < ((x * scale) + (uint32_t) scale); xx++) {
            uint32_t i = (xx + yy * USED_TV_WIDTH) * 4;
            if (i + 3 < tvSize / 2) {
                if (isBackBuffer) {
                    i += tvSize / 2;
                }
                if (a == 0xFF) {
                    tvBuffer[i] = r;
                    tvBuffer[i + 1] = g;
                    tvBuffer[i + 2] = b;
                } else {
                    tvBuffer[i] = r * opacity + tvBuffer[i] * (1 - opacity);
                    tvBuffer[i + 1] = g * opacity + tvBuffer[i + 1] * (1 - opacity);
                    tvBuffer[i + 2] = b * opacity + tvBuffer[i + 2] * (1 - opacity);
                }
            }
        }
    }
}

void DrawUtils::drawRect(int x1, int y1, int x2, int y2, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    drawLine(x1, y1, x2, y1, r, g, b, a);
    drawLine(x2, y1, x2, y2, r, g, b, a);
    drawLine(x1, y2, x2, y2, r, g, b, a);
    drawLine(x1, y1, x1, y2, r, g, b, a);
}

bool DrawUtils::initFont() {
    void *font = nullptr;
    uint32_t size = 0;
    OSGetSharedData(OS_SHAREDDATATYPE_FONT_STANDARD, 0, &font, &size);

    if (font && size) {
        pFont.xScale = 22;
        pFont.yScale = 22,
        pFont.flags = SFT_DOWNWARD_Y;
        pFont.font = sft_loadmem(font, size);
        if (!pFont.font) {
            return false;
        }
        OSMemoryBarrier();
        return true;
    }
    return false;
}

bool DrawUtils::setFont(OSSharedDataType fontType) {
    void *font = nullptr;
    uint32_t size = 0;
    OSGetSharedData(fontType, 0, &font, &size);

    if (font && size) {
        pFont.xScale = 22;
        pFont.yScale = 22,
        pFont.flags = SFT_DOWNWARD_Y;
        pFont.font = sft_loadmem(font, size);
        if (!pFont.font) {
            return false;
        }
        OSMemoryBarrier();
        return true;
    }
    return false;
}

void DrawUtils::deinitFont() {
    sft_freefont(pFont.font);
    pFont.font = nullptr;
    pFont = {};
}

void DrawUtils::setFontColor(Color col) {
    font_col = col;
}

static void draw_freetype_bitmap(SFT_Image *bmp, int32_t x, int32_t y) {
    int32_t i, j, p, q;

    int32_t x_max = x + bmp->width;
    int32_t y_max = y + bmp->height;

    auto *src = (uint8_t *) bmp->pixels;

    for (i = x, p = 0; i < x_max; i++, p++) {
        for (j = y, q = 0; j < y_max; j++, q++) {
            if (i < 0 || j < 0 || i >= SCREEN_WIDTH || j >= SCREEN_HEIGHT) {
                continue;
            }

            float opacity = src[q * bmp->width + p] / 255.0f;
            DrawUtils::drawPixel(i, j, font_col.r, font_col.g, font_col.b, font_col.a * opacity);
        }
    }
}

void DrawUtils::print(uint32_t x, uint32_t y, const char *string, bool alignRight) {
    auto *buffer = new wchar_t[strlen(string) + 1];

    size_t num = mbstowcs(buffer, string, strlen(string));
    if (num > 0) {
        buffer[num] = 0;
    } else {
        wchar_t *tmp = buffer;
        while ((*tmp++ = *string++))
            ;
    }

    print(x, y, buffer, alignRight);
    delete[] buffer;
}

void DrawUtils::print(uint32_t x, uint32_t y, const wchar_t *string, bool alignRight) {
    auto penX = (int32_t) x;
    auto penY = (int32_t) y;

    if (alignRight) {
        penX -= getTextWidth(string);
    }

    uint16_t textureWidth = 0, textureHeight = 0;
    for (; *string; string++) {
        SFT_Glyph gid; //  unsigned long gid;
        if (sft_lookup(&pFont, *string, &gid) >= 0) {
            SFT_GMetrics mtx;
            if (sft_gmetrics(&pFont, gid, &mtx) < 0) {
                return;
            }

            if (*string == '\n') {
                penY += mtx.minHeight;
                penX = x;
                continue;
            }

            textureWidth = (mtx.minWidth + 3) & ~3;
            textureHeight = mtx.minHeight;

            SFT_Image img = {
                    .pixels = nullptr,
                    .width = textureWidth,
                    .height = textureHeight,
            };

            if (textureWidth == 0) {
                textureWidth = 4;
            }
            if (textureHeight == 0) {
                textureHeight = 4;
            }

            auto buffer = make_unique_nothrow<uint8_t[]>((uint32_t) (img.width * img.height));
            if (!buffer) {
                return;
            }
            img.pixels = buffer.get();
            if (sft_render(&pFont, gid, img) < 0) {
                return;
            } else {
                draw_freetype_bitmap(&img, (int32_t) (penX + mtx.leftSideBearing), (int32_t) (penY + mtx.yOffset));
                penX += (int32_t) mtx.advanceWidth;
            }
        }
    }
}

uint32_t DrawUtils::getTextWidth(const char *string) {
    auto *buffer = new wchar_t[strlen(string) + 1];

    size_t num = mbstowcs(buffer, string, strlen(string));
    if (num > 0) {
        buffer[num] = 0;
    } else {
        wchar_t *tmp = buffer;
        while ((*tmp++ = *string++))
            ;
    }

    uint32_t width = getTextWidth(buffer);
    delete[] buffer;

    return width;
}

uint32_t DrawUtils::getTextWidth(const wchar_t *string) {
    uint32_t width = 0;

    for (; *string; string++) {
        SFT_Glyph gid; //  unsigned long gid;
        if (sft_lookup(&pFont, *string, &gid) >= 0) {
            SFT_GMetrics mtx;
            sft_gmetrics(&pFont, gid, &mtx);
            width += (int32_t) mtx.advanceWidth;
        }
    }

    return (uint32_t) width;
}

void DrawUtils::drawLine(int x1, int y1, int x2, int y2, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    int x;
    int y;
    if (x1 == x2) {
        if (y1 < y2)
            for (y = y1; y <= y2; y++)
                DrawUtils::drawPixel(x1, y, r, g, b, a);
        else
            for (y = y2; y <= y1; y++)
                DrawUtils::drawPixel(x1, y, r, g, b, a);
    } else {
        if (x1 < x2)
            for (x = x1; x <= x2; x++)
                DrawUtils::drawPixel(x, y1, r, g, b, a);
        else
            for (x = x2; x <= x1; x++)
                DrawUtils::drawPixel(x, y1, r, g, b, a);
    }
}

void DrawUtils::drawPic(int x, int y, uint32_t w, uint32_t h, float scale, const uint32_t *pixels) {
    uint32_t nw = w, nh = h;
    Color color(0xFFFFFFFF);
    if (scale <= 0) scale = 1;
    nw = w * scale;
    nh = h * scale;


    for (uint32_t j = y; j < (y + nh); j++) {
        for (uint32_t i = x; i < (x + nw); i++) {
            color.color = pixels[((i - x) * w / nw) + ((j - y) * h / nh) * w];
            DrawUtils::drawPixel(i, j, color);
        }
    }
}

void DrawUtils::drawTGA(int x, int y, float scale, uint8_t *fileContent) {
    uint32_t w = tgaGetWidth(fileContent), h = tgaGetHeight(fileContent);
    uint32_t nw = w, nh = h;
    auto *out = (uint32_t *) tgaRead(fileContent, TGA_READER_RGBA);

    if (scale <= 0) scale = 1;
    nw = w * scale;
    nh = h * scale;

    drawPic(x, y, w, h, scale, out);
    if (scale > 0.5) {
        if ((x > 0) && (y > 0)) drawRect(x - 1, y - 1, x + nw, y + nh, 255, 255, 255, 128);
        if ((x > 1) && (y > 1)) drawRect(x - 2, y - 2, x + nw + 1, y + nh + 1, 255, 255, 255, 128);
        if ((x > 2) && (y > 2)) drawRect(x - 3, y - 3, x + nw + 2, y + nh + 2, 255, 255, 255, 128);
    }
    free(out);
}

void DrawUtils::drawRGB5A3(int x, int y, float scale, uint8_t *fileContent) {
    uint32_t w = 192;
    uint32_t h = 64;
    uint32_t nw = w;
    uint32_t nh = h;

    if (scale <= 0)
        scale = 1;
    nw = w * scale;
    nh = h * scale;

    uint32_t pos = 0;
    uint32_t npos = 0;
    Color color(0xFFFFFFFF);
    auto *pixels = (uint16_t *) fileContent;

    uint8_t sum = (4 * scale);
    for (uint32_t j = y; j < (y + nh); j += sum) {
        for (uint32_t i = x; i < (x + nw); i += sum) {
            for (uint32_t sj = j; sj < (j + sum); sj++, pos++) {
                for (uint32_t si = i; si < (i + sum); si++) {
                    npos = ((si - i) / scale) + ((pos / scale) * 4);
                    if ((pixels[npos] & 0x8000) != 0) {
                        color.color = ((pixels[npos] & 0x7C00) << 17) | ((pixels[npos] & 0x3E0) << 14) |
                                      ((pixels[npos] & 0x1F) << 11) | 0xFF;
                    } else {
                        color.color = (((pixels[npos] & 0xF00) * 0x11) << 16) | (((pixels[npos] & 0xF0) * 0x11) << 12) |
                                      (((pixels[npos] & 0xF) * 0x11) << 8) | (((pixels[npos] & 0x7000) >> 12) * 0x24);
                    }
                    DrawUtils::drawPixel(si, sj, color);
                }
            }
        }
    }
    if (scale > 0.5) {
        if ((x > 0) && (y > 0)) {
            drawRect(x - 1, y - 1, x + nw, y + nh, 255, 255, 255, 128);
        }
        if ((x > 1) && (y > 1)) {
            drawRect(x - 2, y - 2, x + nw + 1, y + nh + 1, 255, 255, 255, 128);
        }
        if ((x > 2) && (y > 2)) {
            drawRect(x - 3, y - 3, x + nw + 2, y + nh + 2, 255, 255, 255, 128);
        }
    }
}