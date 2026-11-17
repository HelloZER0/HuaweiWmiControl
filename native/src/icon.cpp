/* ============================================================================
 * icon.cpp — 运行时绘制 16x16 托盘图标
 * ----------------------------------------------------------------------------
 * 造型：电池轮廓（绿色 #22C55E 描边 + 白色内部 + 底部绿色电量块）。
 * 实现要点：
 *   1. CreateDIBSection 建 32bpp 自上而下（biHeight = -16）位图，直接写 RGBA。
 *   2. 另建 1bpp AND 掩码：透明像素 bit = 1，不透明像素 bit = 0。
 *      没有正确掩码时 32bpp 图标会带黑底（README 风险 R5）。
 *   3. 16 像素宽 = 掩码每行 2 字节，正好是 WORD，无需手动补齐。
 *   4. CreateIconIndirect 会复制位图，因此创建成功后必须释放 hbmColor/hbmMask。
 *   5. 失败一律返回 NULL，由调用方回退到系统图标。
 * ========================================================================== */
#include <windows.h>
#include <string.h>

#include "icon.h"

namespace {

const int ICON_SIZE   = 16;
const int ICON_STRIDE = ICON_SIZE * 4;        /* 每行字节数（32bpp） */
const int MASK_STRIDE = ICON_SIZE / 8;        /* 每行字节数（1bpp，16/8 = 2） */

/* 32bpp 位图内存顺序是 BGRA。
 * 名字刻意避开 Win32 的 PutPixel/FillRect（user32 有同名 FillRect）。 */
void PutPixelRGBA(BYTE* bits, int x, int y, BYTE r, BYTE g, BYTE b, BYTE a)
{
    BYTE* p = bits + (size_t)y * ICON_STRIDE + (size_t)x * 4;
    p[0] = b;
    p[1] = g;
    p[2] = r;
    p[3] = a;
}

/* 闭区间填充矩形，越界自动裁剪。 */
void FillBoxRGBA(BYTE* bits, int x0, int y0, int x1, int y1,
                 BYTE r, BYTE g, BYTE b, BYTE a)
{
    int x, y;
    for (y = y0; y <= y1; ++y) {
        if (y < 0 || y >= ICON_SIZE) continue;
        for (x = x0; x <= x1; ++x) {
            if (x < 0 || x >= ICON_SIZE) continue;
            PutPixelRGBA(bits, x, y, r, g, b, a);
        }
    }
}

/* 由 alpha 通道生成 AND 掩码：alpha == 0 -> 1（与屏幕 AND，即透明）。 */
void BuildMaskBits(const BYTE* bits, BYTE* maskBits)
{
    int x, y;
    memset(maskBits, 0xFF, (size_t)MASK_STRIDE * ICON_SIZE);
    for (y = 0; y < ICON_SIZE; ++y) {
        for (x = 0; x < ICON_SIZE; ++x) {
            BYTE a = bits[(size_t)y * ICON_STRIDE + (size_t)x * 4 + 3];
            if (a != 0) {
                /* 不透明 -> 清掉对应位（最高位是左侧第一个像素）。 */
                maskBits[(size_t)y * MASK_STRIDE + (x >> 3)] &=
                    (BYTE)~(0x80u >> (x & 7));
            }
        }
    }
}

} /* namespace */

HICON CreateTrayIcon(void)
{
    const BYTE kGreenR = 0x22;
    const BYTE kGreenG = 0xC5;
    const BYTE kGreenB = 0x5E;

    HDC     hdcScreen = NULL;
    HDC     hdcMem    = NULL;
    HBITMAP hColor    = NULL;
    HBITMAP hMask     = NULL;
    HICON   hIcon     = NULL;
    void*   rawBits   = NULL;
    BITMAPINFO bmi;
    ICONINFO   ii;
    BYTE    maskBits[MASK_STRIDE * ICON_SIZE];
    BYTE*   bits;

    /* 建一个兼容 DC 只为让 CreateDIBSection 有个参照设备。 */
    hdcScreen = GetDC(NULL);
    if (hdcScreen == NULL) return NULL;
    hdcMem = CreateCompatibleDC(hdcScreen);
    if (hdcMem == NULL) {
        ReleaseDC(NULL, hdcScreen);
        return NULL;
    }

    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = ICON_SIZE;
    bmi.bmiHeader.biHeight      = -ICON_SIZE;   /* 负值 = 自上而下 */
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    hColor = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &rawBits, NULL, 0);

    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
    hdcMem = NULL;
    hdcScreen = NULL;

    if (hColor == NULL || rawBits == NULL) {
        if (hColor != NULL) DeleteObject(hColor);
        return NULL;
    }

    bits = (BYTE*)rawBits;
    ZeroMemory(bits, (size_t)ICON_STRIDE * ICON_SIZE);   /* 先全透明 */

    /* --- 电池外壳（绿色）与内部（白色） ---
     * 外壳：(1,3)-(12,12)；内部：(2,4)-(11,11) 共 10x8 像素。 */
    FillBoxRGBA(bits, 1, 3, 12, 12, kGreenR, kGreenG, kGreenB, 0xFF);
    FillBoxRGBA(bits, 2, 4, 11, 11, 0xFF, 0xFF, 0xFF, 0xFF);

    /* --- 正极触点 --- */
    FillBoxRGBA(bits, 13, 6, 14, 9, kGreenR, kGreenG, kGreenB, 0xFF);

    /* --- 电量块：内部下半部（约 50%）。
     * 左右各留 1px 白边，否则填充与描边同色会糊成一个整绿块，
     * 16x16 下就完全看不出「电量」了（已用位图预览验证）。 */
    FillBoxRGBA(bits, 3, 8, 10, 11, kGreenR, kGreenG, kGreenB, 0xFF);

    /* --- 掩码 --- */
    BuildMaskBits(bits, maskBits);
    hMask = CreateBitmap(ICON_SIZE, ICON_SIZE, 1, 1, maskBits);
    if (hMask == NULL) {
        DeleteObject(hColor);
        return NULL;
    }

    ZeroMemory(&ii, sizeof(ii));
    ii.fIcon    = TRUE;
    ii.hbmColor = hColor;
    ii.hbmMask  = hMask;
    hIcon = CreateIconIndirect(&ii);

    /* CreateIconIndirect 已复制位图内容，这里必须释放。 */
    DeleteObject(hColor);
    DeleteObject(hMask);

    return hIcon;
}
