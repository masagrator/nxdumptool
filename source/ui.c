#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <math.h>

#include <switch.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <turbojpeg.h>

#include "dumper.h"
#include "fs_ext.h"
#include "ui.h"
#include "util.h"

/* Extern variables */

extern FsDeviceOperator fsOperatorInstance;

extern AppletType programAppletType;

extern bool gameCardInserted;

extern char gameCardSizeStr[32], trimmedCardSizeStr[32];

extern u8 *hfs0_header;
extern u64 hfs0_offset, hfs0_size;
extern u32 hfs0_partition_cnt;

extern u8 *partitionHfs0Header;
extern u64 partitionHfs0HeaderOffset, partitionHfs0HeaderSize;
extern u32 partitionHfs0FileCount, partitionHfs0StrTableSize;

extern u32 gameCardAppCount;
extern u64 *gameCardTitleID;
extern u32 *gameCardVersion;

extern u32 gameCardPatchCount;
extern u64 *gameCardPatchTitleID;
extern u32 *gameCardPatchVersion;

extern u32 gameCardAddOnCount;
extern u64 *gameCardAddOnTitleID;
extern u32 *gameCardAddOnVersion;

extern char **gameCardName;
extern char **gameCardAuthor;
extern char **gameCardVersionStr;
extern u8 **gameCardIcon;

extern char gameCardUpdateVersionStr[128];

extern char *filenameBuffer;
extern char *filenames[FILENAME_MAX_CNT];
extern int filenamesCount;

extern char curRomFsPath[NAME_BUF_LEN];
extern romfs_browser_entry *romFsBrowserEntries;

extern char strbuf[NAME_BUF_LEN * 4];

/* Statically allocated variables */

static PlFontData standardFont;
static PlFontData nintendoExtFont;
static FT_Library library;
static FT_Face standardFontFace;
static FT_Face nintendoExtFontFace;
static Framebuffer fb;

static u32 *framebuf = NULL;
static u32 framebuf_width = 0;

int cursor = 0;
int scroll = 0;
int breaks = 0;
int font_height = 0;

static u32 selectedAppInfoIndex = 0;
static u32 selectedAppIndex;
static u32 selectedPatchIndex;
static u32 selectedAddOnIndex;
static u32 selectedPartitionIndex;
static u32 selectedFileIndex;

static nspDumpType selectedNspDumpType;

static bool highlight = false;

static bool isFat32 = false, dumpCert = false, trimDump = false, calcCrc = true, setXciArchiveBit = false;

static char statusMessage[2048] = {'\0'};
static int statusMessageFadeout = 0;

u64 freeSpace = 0;
static char freeSpaceStr[64] = {'\0'};

static UIState uiState;

static const char *dirNormalIconPath = "romfs:/dir_normal.jpg";
static u8 *dirNormalIconBuf = NULL;

static const char *dirHighlightIconPath = "romfs:/dir_highlight.jpg";
static u8 *dirHighlightIconBuf = NULL;

static const char *fileNormalIconPath = "romfs:/file_normal.jpg";
static u8 *fileNormalIconBuf = NULL;

static const char *fileHighlightIconPath = "romfs:/file_highlight.jpg";
static u8 *fileHighlightIconBuf = NULL;

static const char *appHeadline = "Nintendo Switch Game Card Dump Tool v" APP_VERSION ".\nOriginal codebase by MCMrARM.\nUpdated and maintained by DarkMatterCore.\n\n";
static const char *appControlsNoGameCard = "[ " NINTENDO_FONT_PLUS " ] Exit";
static const char *appControlsSingleApp = "[ " NINTENDO_FONT_DPAD " / " NINTENDO_FONT_LSTICK " / " NINTENDO_FONT_RSTICK " ] Move | [ " NINTENDO_FONT_A " ] Select | [ " NINTENDO_FONT_B " ] Back | [ " NINTENDO_FONT_PLUS " ] Exit";
static const char *appControlsMultiApp = "[ " NINTENDO_FONT_DPAD " / " NINTENDO_FONT_LSTICK " / " NINTENDO_FONT_RSTICK " ] Move | [ " NINTENDO_FONT_A " ] Select | [ " NINTENDO_FONT_B " ] Back | [ " NINTENDO_FONT_L " / " NINTENDO_FONT_R " / " NINTENDO_FONT_ZL " / " NINTENDO_FONT_ZR " ] Change displayed base application info | [ " NINTENDO_FONT_PLUS " ] Exit";

static const char *mainMenuItems[] = { "Cartridge Image (XCI) dump", "Nintendo Submission Package (NSP) dump", "HFS0 options", "RomFS options", "Dump game card certificate", "Update options" };
static const char *xciDumpMenuItems[] = { "Start XCI dump process", "Split output dump (FAT32 support): ", "Create directory with archive bit set: ", "Dump certificate: ", "Trim output dump: ", "CRC32 checksum calculation + dump verification: " };
static const char *nspDumpMenuItems[] = { "Dump base application NSP", "Dump bundled update NSP", "Dump bundled DLC NSP" };
static const char *nspAppDumpMenuItems[] = { "Start NSP dump process", "Split output dump (FAT32 support): ", "CRC32 checksum calculation: ", "Bundled application to dump: " };
static const char *nspPatchDumpMenuItems[] = { "Start NSP dump process", "Split output dump (FAT32 support): ", "CRC32 checksum calculation: ", "Bundled update to dump: " };
static const char *nspAddOnDumpMenuItems[] = { "Start NSP dump process", "Split output dump (FAT32 support): ", "CRC32 checksum calculation: ", "Bundled DLC to dump: " };
static const char *hfs0MenuItems[] = { "Raw HFS0 partition dump", "HFS0 partition data dump", "Browse HFS0 partitions" };
static const char *hfs0PartitionDumpType1MenuItems[] = { "Dump HFS0 partition 0 (Update)", "Dump HFS0 partition 1 (Normal)", "Dump HFS0 partition 2 (Secure)" };
static const char *hfs0PartitionDumpType2MenuItems[] = { "Dump HFS0 partition 0 (Update)", "Dump HFS0 partition 1 (Logo)", "Dump HFS0 partition 2 (Normal)", "Dump HFS0 partition 3 (Secure)" };
static const char *hfs0BrowserType1MenuItems[] = { "Browse HFS0 partition 0 (Update)", "Browse HFS0 partition 1 (Normal)", "Browse HFS0 partition 2 (Secure)" };
static const char *hfs0BrowserType2MenuItems[] = { "Browse HFS0 partition 0 (Update)", "Browse HFS0 partition 1 (Logo)", "Browse HFS0 partition 2 (Normal)", "Browse HFS0 partition 3 (Secure)" };
static const char *romFsMenuItems[] = { "RomFS section data dump", "Browse RomFS section" };
static const char *romFsSectionDumpMenuItems[] = { "Start RomFS data dump process", "Bundled application to dump: " };
static const char *romFsSectionBrowserMenuItems[] = { "Browse RomFS section", "Bundled application to browse: " };
static const char *updateMenuItems[] = { "Update NSWDB.COM XML database", "Update application" };

void uiFill(int x, int y, int width, int height, u8 r, u8 g, u8 b)
{
    /* Perform validity checks */
	if ((x + width) < 0 || (y + height) < 0 || x >= FB_WIDTH || y >= FB_HEIGHT) return;
    
	if (x < 0)
	{
		width += x;
		x = 0;
	}
	
	if (y < 0)
	{
		height += y;
		y = 0;
	}
    
	if ((x + width) >= FB_WIDTH) width = (FB_WIDTH - x);
    
	if ((y + height) >= FB_HEIGHT) height = (FB_HEIGHT - y);
    
    if (framebuf == NULL)
    {
        /* Begin new frame */
        u32 stride;
        framebuf = (u32*)framebufferBegin(&fb, &stride);
        framebuf_width = (stride / sizeof(u32));
    }
    
    u32 lx, ly;
    u32 framex, framey;
    
    for (ly = 0; ly < height; ly++)
    {
        for (lx = 0; lx < width; lx++)
        {
            framex = (x + lx);
            framey = (y + ly);
            
            framebuf[(framey * framebuf_width) + framex] = RGBA8_MAXALPHA(r, g, b);
        }
    }
}

void uiDrawIcon(const u8 *icon, int width, int height, int x, int y)
{
    /* Perform validity checks */
    if (!icon || !width || !height || (x + width) < 0 || (y + height) < 0 || x >= FB_WIDTH || y >= FB_HEIGHT) return;
    
	if (x < 0)
	{
		width += x;
		x = 0;
	}
	
	if (y < 0)
	{
		height += y;
		y = 0;
	}
    
	if ((x + width) >= FB_WIDTH) width = (FB_WIDTH - x);
    
	if ((y + height) >= FB_HEIGHT) height = (FB_HEIGHT - y);
    
    if (framebuf == NULL)
    {
        /* Begin new frame */
        u32 stride;
        framebuf = (u32*)framebufferBegin(&fb, &stride);
        framebuf_width = (stride / sizeof(u32));
    }
    
    u32 lx, ly;
    u32 framex, framey;
    u32 pos = 0;
    
    for (ly = 0; ly < height; ly++)
    {
        for (lx = 0; lx < width; lx++)
        {
            framex = (x + lx);
            framey = (y + ly);
            
            pos = (((ly * width) + lx) * 3);
            
            framebuf[(framey * framebuf_width) + framex] = RGBA8_MAXALPHA(icon[pos], icon[pos + 1], icon[pos + 2]);
        }
    }
}

bool uiLoadJpgFromMem(u8 *rawJpg, size_t rawJpgSize, int expectedWidth, int expectedHeight, int desiredWidth, int desiredHeight, u8 **outBuf)
{
    if (!rawJpg || !rawJpgSize || !expectedWidth || !expectedHeight || !desiredWidth || !desiredHeight || !outBuf)
    {
        snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "uiLoadJpgFromMem: invalid parameters to process JPG image buffer.");
        return false;
    }
    
    int ret, w, h, samp;
    tjhandle _jpegDecompressor = NULL;
    bool success = false;
    
    bool foundScalingFactor = false;
    int i, numScalingFactors = 0, pitch;
    tjscalingfactor *scalingFactors = NULL;
    
    u8 *jpgScaledBuf = NULL;
    
    _jpegDecompressor = tjInitDecompress();
    if (_jpegDecompressor)
    {
        ret = tjDecompressHeader2(_jpegDecompressor, rawJpg, rawJpgSize, &w, &h, &samp);
        if (ret != -1)
        {
            if (w == expectedWidth && h == expectedHeight)
            {
                scalingFactors = tjGetScalingFactors(&numScalingFactors);
                if (scalingFactors)
                {
                    for(i = 0; i < numScalingFactors; i++)
                    {
                        if (TJSCALED(expectedWidth, scalingFactors[i]) == desiredWidth && TJSCALED(expectedHeight, scalingFactors[i]) == desiredHeight)
                        {
                            foundScalingFactor = true;
                            break;
                        }
                    }
                    
                    if (foundScalingFactor)
                    {
                        pitch = TJPAD(desiredWidth * tjPixelSize[TJPF_RGB]);
                        
                        jpgScaledBuf = malloc(pitch * desiredHeight);
                        if (jpgScaledBuf)
                        {
                            ret = tjDecompress2(_jpegDecompressor, rawJpg, rawJpgSize, jpgScaledBuf, desiredWidth, 0, desiredHeight, TJPF_RGB, TJFLAG_ACCURATEDCT);
                            if (ret != -1)
                            {
                                *outBuf = jpgScaledBuf;
                                success = true;
                            } else {
                                free(jpgScaledBuf);
                                snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "uiLoadJpgFromMem: tjDecompress2 failed (%d).", ret);
                            }
                        } else {
                            snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "uiLoadJpgFromMem: unable to allocated memory for the scaled RGB image output.");
                        }
                    } else {
                        snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "uiLoadJpgFromMem: unable to find a valid scaling factor.");
                    }
                } else {
                    snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "uiLoadJpgFromMem: error retrieving scaling factors.");
                }
            } else {
                snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "uiLoadJpgFromMem: invalid image width/height.");
            }
        } else {
            snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "uiLoadJpgFromMem: tjDecompressHeader2 failed (%d).", ret);
        }
        
        tjDestroy(_jpegDecompressor);
    } else {
        snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "uiLoadJpgFromMem: tjInitDecompress failed.");
    }
    
    return success;
}

bool uiLoadJpgFromFile(const char *filename, int expectedWidth, int expectedHeight, int desiredWidth, int desiredHeight, u8 **outBuf)
{
    if (!filename || !desiredWidth || !desiredHeight || !outBuf)
    {
        snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "uiLoadJpgFromFile: invalid parameters to process JPG image file.\n");
        return false;
    }
    
    u8 *buf = NULL;
    FILE *fp = NULL;
    size_t filesize = 0, read = 0;
    
    fp = fopen(filename, "rb");
    if (!fp)
    {
        snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "uiLoadJpgFromFile: failed to open file \"%s\".\n", filename);
        return false;
    }
    
    fseek(fp, 0, SEEK_END);
    filesize = ftell(fp);
    rewind(fp);
    
    if (!filesize)
    {
        snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "uiLoadJpgFromFile: file \"%s\" is empty.\n", filename);
        fclose(fp);
        return false;
    }
    
    buf = malloc(filesize);
    if (!buf)
    {
        snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "uiLoadJpgFromFile: error allocating memory for image \"%s\".\n", filename);
        fclose(fp);
        return false;
    }
    
    read = fread(buf, 1, filesize, fp);
    
    fclose(fp);
    
    if (read != filesize)
    {
        snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "uiLoadJpgFromFile: error reading image \"%s\".\n", filename);
        free(buf);
        return false;
    }
    
    bool ret = uiLoadJpgFromMem(buf, filesize, expectedWidth, expectedHeight, desiredWidth, desiredHeight, outBuf);
    
    free(buf);
    
    if (!ret) strcat(strbuf, "\n");
    
    return ret;
}

void uiDrawChar(FT_Bitmap *bitmap, int x, int y, u8 r, u8 g, u8 b)
{
    if (framebuf == NULL) return;
    
    u32 framex, framey;
    u32 tmpx, tmpy;
    u8 *imageptr = bitmap->buffer;
    
    u8 src_val;
    float opacity;
    
    u8 fontR;
    u8 fontG;
    u8 fontB;
    
    if (bitmap->pixel_mode != FT_PIXEL_MODE_GRAY) return;
    
    for(tmpy = 0; tmpy < bitmap->rows; tmpy++)
    {
        for (tmpx = 0; tmpx < bitmap->width; tmpx++)
        {
            framex = (x + tmpx);
            framey = (y + tmpy);
            
            if (framex >= FB_WIDTH || framey >= FB_HEIGHT) continue;
            
            src_val = imageptr[tmpx];
            if (!src_val)
            {
                /* Render background color */
                if (highlight)
                {
                    framebuf[(framey * framebuf_width) + framex] = RGBA8_MAXALPHA(HIGHLIGHT_BG_COLOR_R, HIGHLIGHT_BG_COLOR_G, HIGHLIGHT_BG_COLOR_B);
                } else {
                    framebuf[(framey * framebuf_width) + framex] = RGBA8_MAXALPHA(BG_COLOR_RGB, BG_COLOR_RGB, BG_COLOR_RGB);
                }
            } else {
                /* Calculate alpha (opacity) */
                opacity = (src_val / 255.0);
                
                if (highlight)
                {
                    fontR = (r * opacity + (1 - opacity) * HIGHLIGHT_BG_COLOR_R);
                    fontG = (g * opacity + (1 - opacity) * HIGHLIGHT_BG_COLOR_G);
                    fontB = (b * opacity + (1 - opacity) * HIGHLIGHT_BG_COLOR_B);
                } else {
                    fontR = (r * opacity + (1 - opacity) * BG_COLOR_RGB);
                    fontG = (g * opacity + (1 - opacity) * BG_COLOR_RGB);
                    fontB = (b * opacity + (1 - opacity) * BG_COLOR_RGB);
                }
                
                framebuf[(framey * framebuf_width) + framex] = RGBA8_MAXALPHA(fontR, fontG, fontB);
            }
        }
        
        imageptr += bitmap->pitch;
    }
}

void uiScroll()
{
    if (framebuf == NULL)
    {
        /* Begin new frame */
        u32 stride;
        framebuf = (u32*)framebufferBegin(&fb, &stride);
        framebuf_width = (stride / sizeof(u32));
    }
    
    u32 lx, ly;
    
    for (ly = 0; ly < (FB_HEIGHT - font_height - 8); ly++)
    {
        for (lx = 0; lx < FB_WIDTH; lx++)
        {
            framebuf[(ly * framebuf_width) + lx] = framebuf[((ly + font_height) * framebuf_width) + lx];
        }
    }
    
    uiFill(0, FB_HEIGHT - (8 + (font_height + (font_height / 4))), FB_WIDTH, (8 + (font_height + (font_height / 4))), BG_COLOR_RGB, BG_COLOR_RGB, BG_COLOR_RGB);
    
    breaks = (FB_HEIGHT - (8 + (font_height + (font_height / 4))) + (font_height / 8));
}

void uiDrawString(const char *string, int x, int y, u8 r, u8 g, u8 b)
{
    u32 tmpx = (x <= 8 ? 8 : (x + 8));
    u32 tmpy = (font_height + (y <= 8 ? 8 : (y + 8)));
    FT_Error ret = 0;
    FT_UInt glyph_index;
    FT_GlyphSlot standardFontSlot = standardFontFace->glyph;
    FT_GlyphSlot nintendoExtFontSlot = nintendoExtFontFace->glyph;
    
    u32 i;
    u32 str_size = strlen(string);
    u32 tmpchar;
    ssize_t unitcount = 0;
    
    if (framebuf == NULL)
    {
        /* Begin new frame */
        u32 stride;
        framebuf = (u32*)framebufferBegin(&fb, &stride);
        framebuf_width = (stride / sizeof(u32));
    }
    
    if (tmpy >= FB_HEIGHT)
    {
        tmpy = (FB_HEIGHT - (8 + (font_height + (font_height / 4))) + (font_height / 8));
        uiScroll();
    }
    
    for(i = 0; i < str_size;)
    {
        bool useNintendoExt = (string[i] == 0xE0);
        
        if (useNintendoExt)
        {
            tmpchar = (((string[i] << 8) & 0xFF00) | string[i + 1]);
            i += 2;
        } else {
            unitcount = decode_utf8(&tmpchar, (const u8*)&string[i]);
            if (unitcount <= 0) break;
            i += unitcount;
            
            if (tmpchar == '\n')
            {
                tmpx = 8;
                tmpy += ((font_height + (font_height / 4)) + (font_height / 8));
                breaks++;
                continue;
            } else
            if (tmpchar == '\t')
            {
                tmpx += (font_height * TAB_WIDTH);
                continue;
            } else
            if (tmpchar == '\r')
            {
                continue;
            }
        }
        
        if (useNintendoExt)
        {
            glyph_index = FT_Get_Char_Index(nintendoExtFontFace, tmpchar);
            
            ret = FT_Load_Glyph(nintendoExtFontFace, glyph_index, FT_LOAD_DEFAULT);
            if (ret == 0) ret = FT_Render_Glyph(nintendoExtFontFace->glyph, FT_RENDER_MODE_NORMAL);
            
            if (ret) break;
            
            if ((tmpx + (nintendoExtFontSlot->advance.x >> 6)) >= FB_WIDTH)
            {
                tmpx = 8;
                tmpy += ((font_height + (font_height / 4)) + (font_height / 8));
                breaks++;
            }
            
            uiDrawChar(&nintendoExtFontSlot->bitmap, tmpx + nintendoExtFontSlot->bitmap_left, tmpy - nintendoExtFontSlot->bitmap_top, r, g, b);
            
            tmpx += (nintendoExtFontSlot->advance.x >> 6);
            tmpy += (nintendoExtFontSlot->advance.y >> 6);
        } else {
            glyph_index = FT_Get_Char_Index(standardFontFace, tmpchar);
            
            ret = FT_Load_Glyph(standardFontFace, glyph_index, FT_LOAD_DEFAULT);
            if (ret == 0) ret = FT_Render_Glyph(standardFontFace->glyph, FT_RENDER_MODE_NORMAL);
            
            if (ret) break;
            
            if ((tmpx + (standardFontSlot->advance.x >> 6)) >= FB_WIDTH)
            {
                tmpx = 8;
                tmpy += ((font_height + (font_height / 4)) + (font_height / 8));
                breaks++;
            }
            
            uiDrawChar(&standardFontSlot->bitmap, tmpx + standardFontSlot->bitmap_left, tmpy - standardFontSlot->bitmap_top, r, g, b);
            
            tmpx += (standardFontSlot->advance.x >> 6);
            tmpy += (standardFontSlot->advance.y >> 6);
        }
    }
}

void uiRefreshDisplay()
{
    if (framebuf != NULL)
    {
        framebufferEnd(&fb);
        framebuf = NULL;
        framebuf_width = 0;
    }
}

void uiStatusMsg(const char *fmt, ...)
{
	statusMessageFadeout = 1000;
	
	va_list args;
	va_start(args, fmt);
	vsnprintf(statusMessage, sizeof(statusMessage) / sizeof(statusMessage[0]), fmt, args);
	va_end(args);
}

void uiUpdateStatusMsg()
{
	if (!strlen(statusMessage) || !statusMessageFadeout) return;
	
    if ((statusMessageFadeout - 4) > BG_COLOR_RGB)
    {
        int fadeout = (statusMessageFadeout > 255 ? 255 : statusMessageFadeout);
        uiFill(0, FB_HEIGHT - (8 + (font_height + (font_height / 4))), FB_WIDTH, (8 + (font_height + (font_height / 4))), BG_COLOR_RGB, BG_COLOR_RGB, BG_COLOR_RGB);
        uiDrawString(statusMessage, 8, (FB_HEIGHT - (16 + (font_height + (font_height / 4))) + (font_height / 8)), fadeout, fadeout, fadeout);
        statusMessageFadeout -= 4;
    } else {
        statusMessageFadeout = 0;
    }
}

void uiPleaseWait(u8 wait)
{
    uiDrawString("Please wait...", 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
    uiRefreshDisplay();
    if (wait) delay(wait);
}

void uiUpdateFreeSpace()
{
    getSdCardFreeSpace(&freeSpace);
    
    char tmp[32] = {'\0'};
    convertSize(freeSpace, tmp, sizeof(tmp) / sizeof(tmp[0]));
    
    snprintf(freeSpaceStr, sizeof(freeSpaceStr) / sizeof(freeSpaceStr[0]), "Free SD card space: %s.", tmp);
}

void uiClearScreen()
{
    uiFill(0, 0, FB_WIDTH, FB_HEIGHT, BG_COLOR_RGB, BG_COLOR_RGB, BG_COLOR_RGB);
}

void uiPrintHeadline()
{
    breaks = 0;
    uiClearScreen();
    uiDrawString(appHeadline, 8, 8, 255, 255, 255);
}

void error_screen(const char *fmt, ...)
{
    consoleInit(NULL);
    
    va_list va;
    va_start(va, fmt);
    vprintf(fmt, va);
    va_end(va);
    
    printf("Press any button to exit.\n");
    
    while(appletMainLoop())
    {
        hidScanInput();
        
        u32 keysDown = hidKeysDown(CONTROLLER_P1_AUTO);
        
        if (keysDown && !((keysDown & KEY_TOUCH) || (keysDown & KEY_LSTICK_LEFT) || (keysDown & KEY_LSTICK_RIGHT) || (keysDown & KEY_LSTICK_UP) || (keysDown & KEY_LSTICK_DOWN) || \
            (keysDown & KEY_RSTICK_LEFT) || (keysDown & KEY_RSTICK_RIGHT) || (keysDown & KEY_RSTICK_UP) || (keysDown & KEY_RSTICK_DOWN))) break;
        
        consoleUpdate(NULL);
    }
    
    consoleExit(NULL);
}

int uiInit()
{
    Result rc = 0;
    FT_Error ret = 0;
    
    int status = 0;
    bool pl_init = false, romfs_init = false, ft_lib_init = false, ft_std_face_init = false, ft_nintendo_face_init = false;
    
    /* Set initial UI state */
    uiState = stateMainMenu;
    cursor = 0;
    scroll = 0;
    
    /* Initialize pl service */
    rc = plInitialize();
    if (R_FAILED(rc))
    {
        error_screen("plInitialize() failed (0x%08X).\n", rc);
        goto out;
    }
    
    pl_init = true;
    
    /* Retrieve standard shared font */
    rc = plGetSharedFontByType(&standardFont, PlSharedFontType_Standard);
    if (R_FAILED(rc))
    {
        error_screen("plGetSharedFontByType() failed to retrieve standard shared font (0x%08X).\n", rc);
        goto out;
    }
    
    /* Retrieve Nintendo shared font */
    rc = plGetSharedFontByType(&nintendoExtFont, PlSharedFontType_NintendoExt);
    if (R_FAILED(rc))
    {
        error_screen("plGetSharedFontByType() failed to retrieve Nintendo shared font (0x%08X).\n", rc);
        goto out;
    }
    
    /* Initialize FreeType */
    ret = FT_Init_FreeType(&library);
    if (ret)
    {
        error_screen("FT_Init_FreeType() failed (%d).\n", ret);
        goto out;
    }
    
    ft_lib_init = true;
    
    /* Create memory face for the standard shared font */
    ret = FT_New_Memory_Face(library, standardFont.address, standardFont.size, 0, &standardFontFace);
    if (ret)
    {
        error_screen("FT_New_Memory_Face() failed to create memory face for the standard shared font (%d).\n", ret);
        goto out;
    }
    
    ft_std_face_init = true;
    
    /* Create memory face for the Nintendo shared font */
    ret = FT_New_Memory_Face(library, nintendoExtFont.address, nintendoExtFont.size, 0, &nintendoExtFontFace);
    if (ret)
    {
        error_screen("FT_New_Memory_Face() failed to create memory face for the Nintendo shared font (%d).\n", ret);
        goto out;
    }
    
    ft_nintendo_face_init = true;
    
    /* Set standard shared font character size */
    ret = FT_Set_Char_Size(standardFontFace, 0, CHAR_PT_SIZE * 64, SCREEN_DPI_CNT, SCREEN_DPI_CNT);
    if (ret)
    {
        error_screen("FT_Set_Char_Size() failed to set character size for the standard shared font (%d).\n", ret);
        goto out;
    }
    
    /* Set Nintendo shared font character size */
    ret = FT_Set_Char_Size(nintendoExtFontFace, 0, CHAR_PT_SIZE * 64, SCREEN_DPI_CNT, SCREEN_DPI_CNT);
    if (ret)
    {
        error_screen("FT_Set_Char_Size() failed to set character size for the Nintendo shared font (%d).\n", ret);
        goto out;
    }
    
    /* Store font height and max width */
    font_height = (standardFontFace->size->metrics.height / 64);
    
    /* Prepare additional data needed by the UI functions */
    filenameBuffer = calloc(FILENAME_BUFFER_SIZE, sizeof(char));
    if (!filenameBuffer)
    {
        error_screen("Failed to allocate memory for the filename buffer.\n");
        goto out;
    }
    
    /* Mount Application's RomFS */
    rc = romfsInit();
    if (R_FAILED(rc))
    {
        error_screen("romfsInit() failed (0x%08X).\n", rc);
        goto out;
    }
    
    romfs_init = true;
    
    if (!uiLoadJpgFromFile(dirNormalIconPath, BROWSER_ICON_DIMENSION, BROWSER_ICON_DIMENSION, BROWSER_ICON_DIMENSION, BROWSER_ICON_DIMENSION, &dirNormalIconBuf))
    {
        strcat(strbuf, "Failed to load directory icon (normal).\n");
        error_screen(strbuf);
        goto out;
    }
    
    if (!uiLoadJpgFromFile(dirHighlightIconPath, BROWSER_ICON_DIMENSION, BROWSER_ICON_DIMENSION, BROWSER_ICON_DIMENSION, BROWSER_ICON_DIMENSION, &dirHighlightIconBuf))
    {
        strcat(strbuf, "Failed to load directory icon (highlighted).\n");
        error_screen(strbuf);
        goto out;
    }
    
    if (!uiLoadJpgFromFile(fileNormalIconPath, BROWSER_ICON_DIMENSION, BROWSER_ICON_DIMENSION, BROWSER_ICON_DIMENSION, BROWSER_ICON_DIMENSION, &fileNormalIconBuf))
    {
        strcat(strbuf, "Failed to load file icon (normal).\n");
        error_screen(strbuf);
        goto out;
    }
    
    if (!uiLoadJpgFromFile(fileHighlightIconPath, BROWSER_ICON_DIMENSION, BROWSER_ICON_DIMENSION, BROWSER_ICON_DIMENSION, BROWSER_ICON_DIMENSION, &fileHighlightIconBuf))
    {
        strcat(strbuf, "Failed to load file icon (highlighted).\n");
        error_screen(strbuf);
        goto out;
    }
    
    /* Unmount Application's RomFS */
    romfsExit();
    romfs_init = false;
    
    /* Reinitialize FS stuff */
    /* Fixes a problem where the file descriptor for the application NRO isn't properly closed */
    /* We'll need to have write access to the NRO if the user runs the update procedure */
    if (!envIsNso())
    {
        fsdevUnmountAll();
        fsExit();
        
        rc = fsInitialize();
        if (R_FAILED(rc))
        {
            error_screen("fsInitialize() failed (0x%08X).\n", rc);
            goto out;
        }
        
        rc = fsdevMountSdmc();
        if (R_FAILED(rc))
        {
            error_screen("fsdevMountSdmc() failed (0x%08X).\n", rc);
            goto out;
        }
    }
    
    /* Create framebuffer */
    framebufferCreate(&fb, nwindowGetDefault(), FB_WIDTH, FB_HEIGHT, PIXEL_FORMAT_RGBA_8888, 2);
    framebufferMakeLinear(&fb);
    
    /* Disable screen dimming and auto sleep */
    appletSetMediaPlaybackState(true);
    
    /* Get applet type */
    programAppletType = appletGetAppletType();
    
    /* Block HOME menu button presses if we're running as a regular application or a system application */
    if (programAppletType == AppletType_Application || programAppletType == AppletType_SystemApplication) appletBeginBlockingHomeButton(0);
    
    /* Clear screen */
    uiClearScreen();
    
    /* Update free space */
    uiUpdateFreeSpace();
    
    /* Set output status */
    status = 1;
    
out:
    if (!status)
    {
        if (fileHighlightIconBuf) free(fileHighlightIconBuf);
        if (fileNormalIconBuf) free(fileNormalIconBuf);
        if (dirHighlightIconBuf) free(dirHighlightIconBuf);
        if (dirNormalIconBuf) free(dirNormalIconBuf);
        
        if (romfs_init) romfsExit();
        
        if (filenameBuffer) free(filenameBuffer);
        
        if (ft_nintendo_face_init) FT_Done_Face(nintendoExtFontFace);
        if (ft_std_face_init) FT_Done_Face(standardFontFace);
        if (ft_lib_init) FT_Done_FreeType(library);
        
        if (pl_init) plExit();
    }
    
    return status;
}

void uiDeinit()
{
    /* Unblock HOME menu button presses if we're running as a regular application or a system application */
    if (programAppletType == AppletType_Application || programAppletType == AppletType_SystemApplication) appletEndBlockingHomeButton();
    
    /* Enable screen dimming and auto sleep */
    appletSetMediaPlaybackState(false);
    
    /* Free framebuffer object */
    framebufferClose(&fb);
    
    /* Free directory/file icons */
    free(fileHighlightIconBuf);
    free(fileNormalIconBuf);
    free(dirHighlightIconBuf);
    free(dirNormalIconBuf);
    
    /* Free filename buffer */
    free(filenameBuffer);
    
    /* Free FreeType resources */
    FT_Done_Face(nintendoExtFontFace);
    FT_Done_Face(standardFontFace);
    FT_Done_FreeType(library);
    
    /* Deinitialize pl service */
    plExit();
}

void uiSetState(UIState state)
{
    uiState = state;
    cursor = 0;
    scroll = 0;
}

UIState uiGetState()
{
    return uiState;
}

UIResult uiProcess()
{
    UIResult res = resultNone;
    
    int i, j;
    
    const char **menu = NULL;
    int menuItemsCount = 0;
    
    u32 keysDown;
    u32 keysHeld;
    
    int scrollAmount = 0;
    
    u32 patch, addon, xpos, ypos, startYPos;
    
    char versionStr[128] = {'\0'};
    
    uiPrintHeadline();
    loadGameCardInfo();
    
    if (uiState == stateMainMenu || uiState == stateXciDumpMenu || uiState == stateNspDumpMenu || uiState == stateNspAppDumpMenu || uiState == stateNspPatchDumpMenu || uiState == stateNspAddOnDumpMenu || uiState == stateHfs0Menu || uiState == stateRawHfs0PartitionDumpMenu || uiState == stateHfs0PartitionDataDumpMenu || uiState == stateHfs0BrowserMenu || uiState == stateHfs0Browser || uiState == stateRomFsMenu || uiState == stateRomFsSectionDataDumpMenu || uiState == stateRomFsSectionBrowserMenu || uiState == stateRomFsSectionBrowser || uiState == stateUpdateMenu)
    {
        uiDrawString((!gameCardInserted ? appControlsNoGameCard : (gameCardAppCount > 1 ? appControlsMultiApp : appControlsSingleApp)), 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 255, 255, 255);
        breaks += 2;
        
        uiDrawString(freeSpaceStr, 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 255, 255, 255);
        breaks += 2;
    }
    
    if (!gameCardInserted || hfs0_header == NULL || (hfs0_partition_cnt != GAMECARD_TYPE1_PARTITION_CNT && hfs0_partition_cnt != GAMECARD_TYPE2_PARTITION_CNT) || !gameCardAppCount || gameCardTitleID == NULL)
    {
        if (gameCardInserted)
        {
            if (hfs0_header != NULL)
            {
                if (hfs0_partition_cnt == GAMECARD_TYPE1_PARTITION_CNT || hfs0_partition_cnt == GAMECARD_TYPE2_PARTITION_CNT)
                {
                    if (gameCardAppCount > 0)
                    {
                        uiDrawString("Error: unable to retrieve the game card Title ID!", 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 255, 0, 0);
                        
                        if (strlen(gameCardUpdateVersionStr))
                        {
                            breaks++;
                            snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Bundled FW Update: %s", gameCardUpdateVersionStr);
                            uiDrawString(strbuf, 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 0, 255, 0);
                            breaks++;
                            
                            uiDrawString("In order to be able to dump data from this cartridge, make sure your console is at least on this FW version.", 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 255, 255, 255);
                        }
                    } else {
                        uiDrawString("Error: gamecard application count is zero!", 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 255, 0, 0);
                    }
                } else {
                    snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Error: unknown root HFS0 header partition count! (%u)", hfs0_partition_cnt);
                    uiDrawString(strbuf, 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 255, 0, 0);
                }
            } else {
                uiDrawString("Error: unable to get root HFS0 header data!", 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 255, 0, 0);
            }
        } else {
            uiDrawString("Game card is not inserted!", 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 255, 0, 0);
        }
        
        uiUpdateStatusMsg();
        uiRefreshDisplay();
        
        res = resultShowMainMenu;
        
        hidScanInput();
        keysDown = hidKeysDown(CONTROLLER_P1_AUTO);
        
        // Exit
        if (keysDown & KEY_PLUS) res = resultExit;
        
        return res;
    }
    
    if (uiState == stateMainMenu || uiState == stateXciDumpMenu || uiState == stateNspDumpMenu || uiState == stateNspAppDumpMenu || uiState == stateNspPatchDumpMenu || uiState == stateNspAddOnDumpMenu || uiState == stateHfs0Menu || uiState == stateRawHfs0PartitionDumpMenu || uiState == stateHfs0PartitionDataDumpMenu || uiState == stateHfs0BrowserMenu || uiState == stateHfs0Browser || uiState == stateRomFsMenu || uiState == stateRomFsSectionDataDumpMenu || uiState == stateRomFsSectionBrowserMenu || uiState == stateRomFsSectionBrowser || uiState == stateUpdateMenu)
    {
        if (uiState != stateHfs0Browser && uiState != stateRomFsSectionBrowser)
        {
            uiDrawString("Game card is inserted!", 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 0, 255, 0);
            breaks += 2;
            
            /*snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Root HFS0 header offset: 0x%016lX", hfs0_offset);
            uiDrawString(strbuf, 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 0, 255, 0);
            breaks++;
            
            snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Root HFS0 header size: 0x%016lX", hfs0_size);
            uiDrawString(strbuf, 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 0, 255, 0);
            breaks++;*/
            
            /* Print application info */
            xpos = 8;
            ypos = ((breaks * (font_height + (font_height / 4))) + (font_height / 8));
            startYPos = ypos;
            
            /* Draw icon */
            if (gameCardIcon != NULL && gameCardIcon[selectedAppInfoIndex] != NULL)
            {
                uiDrawIcon(gameCardIcon[selectedAppInfoIndex], NACP_ICON_DOWNSCALED, NACP_ICON_DOWNSCALED, xpos, ypos + 8);
                xpos += (NACP_ICON_DOWNSCALED + 8);
                ypos += 8;
            }
            
            if (gameCardName != NULL && gameCardName[selectedAppInfoIndex] != NULL && strlen(gameCardName[selectedAppInfoIndex]))
            {
                snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Name: %s", gameCardName[selectedAppInfoIndex]);
                uiDrawString(strbuf, xpos, ypos, 0, 255, 0);
                ypos += (font_height + (font_height / 4));
            }
            
            if (gameCardAuthor != NULL && gameCardAuthor[selectedAppInfoIndex] != NULL && strlen(gameCardAuthor[selectedAppInfoIndex]))
            {
                snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Developer: %s", gameCardAuthor[selectedAppInfoIndex]);
                uiDrawString(strbuf, xpos, ypos, 0, 255, 0);
                ypos += (font_height + (font_height / 4));
            }
            
            snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Title ID: %016lX", gameCardTitleID[selectedAppInfoIndex]);
            uiDrawString(strbuf, xpos, ypos, 0, 255, 0);
            
            if (gameCardPatchCount > 0)
            {
                u32 patchCnt = 0;
                
                snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Bundled update(s): v");
                
                for(patch = 0; patch < gameCardPatchCount; patch++)
                {
                    if ((gameCardTitleID[selectedAppInfoIndex] | APPLICATION_PATCH_BITMASK) == gameCardPatchTitleID[patch])
                    {
                        if (patchCnt > 0) strcat(strbuf, ", v");
                        
                        convertTitleVersionToDecimal(gameCardPatchVersion[patch], versionStr, sizeof(versionStr));
                        strcat(strbuf, versionStr);
                        
                        patchCnt++;
                    }
                }
                
                if (patchCnt > 0) uiDrawString(strbuf, (FB_WIDTH / 2) - (FB_WIDTH / 8), ypos, 0, 255, 0);
            }
            
            ypos += (font_height + (font_height / 4));
            
            if (gameCardVersionStr != NULL && gameCardVersionStr[selectedAppInfoIndex] != NULL && strlen(gameCardVersionStr[selectedAppInfoIndex]))
            {
                snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Version: %s", gameCardVersionStr[selectedAppInfoIndex]);
                uiDrawString(strbuf, xpos, ypos, 0, 255, 0);
                if (!gameCardAddOnCount) ypos += (font_height + (font_height / 4));
            }
            
            if (gameCardAddOnCount > 0)
            {
                u32 addOnCnt = 0;
                
                for(addon = 0; addon < gameCardAddOnCount; addon++)
                {
                    if ((gameCardTitleID[selectedAppInfoIndex] & APPLICATION_ADDON_BITMASK) == (gameCardAddOnTitleID[addon] & APPLICATION_ADDON_BITMASK)) addOnCnt++;
                }
                
                if (addOnCnt > 0)
                {
                    snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Bundled DLC(s): %u", addOnCnt);
                    uiDrawString(strbuf, (FB_WIDTH / 2) - (FB_WIDTH / 8), ypos, 0, 255, 0);
                    ypos += (font_height + (font_height / 4));
                }
            }
            
            ypos += 8;
            if (xpos > 8 && (ypos - NACP_ICON_DOWNSCALED) < startYPos) ypos += (NACP_ICON_DOWNSCALED - (ypos - startYPos));
            ypos += (font_height + (font_height / 4));
            
            breaks += (int)round((double)(ypos - startYPos) / (double)(font_height + (font_height / 4)));
            
            snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Size: %s | Used space: %s", gameCardSizeStr, trimmedCardSizeStr);
            uiDrawString(strbuf, 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 0, 255, 0);
            
            if (gameCardAppCount > 1)
            {
                breaks++;
                snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Base application count: %u | Base application currently displayed: %u", gameCardAppCount, selectedAppInfoIndex + 1);
                uiDrawString(strbuf, 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 0, 255, 0);
            }
            
            breaks++;
            
            snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Partition count: %u (%s)", hfs0_partition_cnt, GAMECARD_TYPE(hfs0_partition_cnt));
            uiDrawString(strbuf, 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 0, 255, 0);
            
            if (strlen(gameCardUpdateVersionStr))
            {
                breaks++;
                snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Bundled FW update: %s", gameCardUpdateVersionStr);
                uiDrawString(strbuf, 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 0, 255, 0);
            }
            
            if (gameCardPatchCount > 0)
            {
                breaks++;
                snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Total bundled update(s): %u", gameCardPatchCount);
                uiDrawString(strbuf, 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 0, 255, 0);
            }
            
            if (gameCardAddOnCount > 0)
            {
                breaks++;
                snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Total bundled DLC(s): %u", gameCardAddOnCount);
                uiDrawString(strbuf, 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 0, 255, 0);
            }
            
            breaks += 2;
        }
        
        switch(uiState)
        {
            case stateMainMenu:
                menu = mainMenuItems;
                menuItemsCount = sizeof(mainMenuItems) / sizeof(mainMenuItems[0]);
                break;
            case stateXciDumpMenu:
                menu = xciDumpMenuItems;
                menuItemsCount = sizeof(xciDumpMenuItems) / sizeof(xciDumpMenuItems[0]);
                
                uiDrawString(mainMenuItems[0], 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
                
                break;
            case stateNspDumpMenu:
                menu = nspDumpMenuItems;
                menuItemsCount = sizeof(nspDumpMenuItems) / sizeof(nspDumpMenuItems[0]);
                
                uiDrawString(mainMenuItems[1], 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
                
                break;
            case stateNspAppDumpMenu:
                menu = nspAppDumpMenuItems;
                menuItemsCount = sizeof(nspAppDumpMenuItems) / sizeof(nspAppDumpMenuItems[0]);
                
                uiDrawString(nspDumpMenuItems[0], 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
                
                break;
            case stateNspPatchDumpMenu:
                menu = nspPatchDumpMenuItems;
                menuItemsCount = sizeof(nspPatchDumpMenuItems) / sizeof(nspPatchDumpMenuItems[0]);
                
                uiDrawString(nspDumpMenuItems[1], 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
                
                break;
            case stateNspAddOnDumpMenu:
                menu = nspAddOnDumpMenuItems;
                menuItemsCount = sizeof(nspAddOnDumpMenuItems) / sizeof(nspAddOnDumpMenuItems[0]);
                
                uiDrawString(nspDumpMenuItems[2], 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
                
                break;
            case stateHfs0Menu:
                menu = hfs0MenuItems;
                menuItemsCount = sizeof(hfs0MenuItems) / sizeof(hfs0MenuItems[0]);
                
                uiDrawString(mainMenuItems[2], 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
                
                break;
            case stateRawHfs0PartitionDumpMenu:
            case stateHfs0PartitionDataDumpMenu:
                menu = (hfs0_partition_cnt == GAMECARD_TYPE1_PARTITION_CNT ? hfs0PartitionDumpType1MenuItems : hfs0PartitionDumpType2MenuItems);
                menuItemsCount = (hfs0_partition_cnt == GAMECARD_TYPE1_PARTITION_CNT ? (sizeof(hfs0PartitionDumpType1MenuItems) / sizeof(hfs0PartitionDumpType1MenuItems[0])) : (sizeof(hfs0PartitionDumpType2MenuItems) / sizeof(hfs0PartitionDumpType2MenuItems[0])));
                
                uiDrawString((uiState == stateRawHfs0PartitionDumpMenu ? hfs0MenuItems[0] : hfs0MenuItems[1]), 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
                
                break;
            case stateHfs0BrowserMenu:
                menu = (hfs0_partition_cnt == GAMECARD_TYPE1_PARTITION_CNT ? hfs0BrowserType1MenuItems : hfs0BrowserType2MenuItems);
                menuItemsCount = (hfs0_partition_cnt == GAMECARD_TYPE1_PARTITION_CNT ? (sizeof(hfs0BrowserType1MenuItems) / sizeof(hfs0BrowserType1MenuItems[0])) : (sizeof(hfs0BrowserType2MenuItems) / sizeof(hfs0BrowserType2MenuItems[0])));
                
                uiDrawString(hfs0MenuItems[2], 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
                
                break;
            case stateHfs0Browser:
                menu = (const char**)filenames;
                menuItemsCount = filenamesCount;
                
                uiDrawString((hfs0_partition_cnt == GAMECARD_TYPE1_PARTITION_CNT ? hfs0BrowserType1MenuItems[selectedPartitionIndex] : hfs0BrowserType2MenuItems[selectedPartitionIndex]), 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
                breaks += 2;
                
                snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "File count: %d | Current file: %d", menuItemsCount, cursor + 1);
                uiDrawString(strbuf, 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 255, 255, 255);
                
                break;
            case stateRomFsMenu:
                menu = romFsMenuItems;
                menuItemsCount = sizeof(romFsMenuItems) / sizeof(romFsMenuItems[0]);
                
                uiDrawString(mainMenuItems[3], 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
                
                break;
            case stateRomFsSectionDataDumpMenu:
                menu = romFsSectionDumpMenuItems;
                menuItemsCount = sizeof(romFsSectionDumpMenuItems) / sizeof(romFsSectionDumpMenuItems[0]);
                
                uiDrawString(romFsMenuItems[0], 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
                
                break;
            case stateRomFsSectionBrowserMenu:
                menu = romFsSectionBrowserMenuItems;
                menuItemsCount = sizeof(romFsSectionBrowserMenuItems) / sizeof(romFsSectionBrowserMenuItems[0]);
                
                uiDrawString(romFsMenuItems[1], 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
                
                break;
            case stateRomFsSectionBrowser:
                menu = (const char**)filenames;
                menuItemsCount = filenamesCount;
                
                uiDrawString(romFsMenuItems[1], 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
                breaks++;
                
                convertTitleVersionToDecimal(gameCardVersion[selectedAppIndex], versionStr, sizeof(versionStr));
                snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "%s%s v%s", romFsSectionBrowserMenuItems[1], gameCardName[selectedAppIndex], versionStr);
                uiDrawString(strbuf, 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
                breaks += 2;
                
                snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Path: romfs:%s", curRomFsPath);
                uiDrawString(strbuf, 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
                breaks += 2;
                
                if (cursor > 0)
                {
                    snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Entry count: %d | Current entry: %d", menuItemsCount - 1, cursor);
                } else {
                    snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Entry count: %d", menuItemsCount - 1);
                }
                
                uiDrawString(strbuf, 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 255, 255, 255);
                
                break;
            case stateUpdateMenu:
                menu = updateMenuItems;
                menuItemsCount = sizeof(updateMenuItems) / sizeof(updateMenuItems[0]);
                
                uiDrawString(mainMenuItems[5], 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
                
                break;
            default:
                break;
        }
        
        if (menu && menuItemsCount)
        {
            if (uiState != stateMainMenu) breaks += 2;
            
            j = 0;
            highlight = false;
            
            for(i = scroll; i < menuItemsCount; i++, j++)
            {
                if (((uiState != stateHfs0Browser && uiState != stateRomFsSectionBrowser) && j >= COMMON_MAX_ELEMENTS) || (uiState == stateHfs0Browser && j >= HFS0_MAX_ELEMENTS) || (uiState == stateRomFsSectionBrowser && j >= ROMFS_MAX_ELEMENTS)) break;
                
                // Avoid printing the "Create directory with archive bit set" option if "Split output dump" is disabled
                if (uiState == stateXciDumpMenu && i == 2 && !isFat32)
                {
                    j--;
                    continue;
                }
                
                // Avoid printing the "Dump bundled update NSP" option in the NSP dump menu if our current gamecard doesn't include any bundled updates
                // Also avoid printing the "Dump bundled DLC NSP" option in the NSP dump menu if our current gamecard doesn't include any bundled DLCs
                if (uiState == stateNspDumpMenu)
                {
                    if (i == 1)
                    {
                        if (!gameCardPatchCount) continue;
                    } else
                    if (i == 2)
                    {
                        if (!gameCardAddOnCount)
                        {
                            continue;
                        } else {
                            if (!gameCardPatchCount) j--;
                        }
                    }
                }
                
                // Avoid printing the parent directory entry ("..") in the RomFS browser if we're currently at the root directory
                if (uiState == stateRomFsSectionBrowser && i == 0 && strlen(curRomFsPath) <= 1)
                {
                    j--;
                    continue;
                }
                
                xpos = 8;
                int font_r = 255, font_g = 255, font_b = 255;
                
                if (i == cursor)
                {
                    highlight = true;
                    
                    uiFill(0, 8 + (breaks * (font_height + (font_height / 4))) + (j * (font_height + 12)), FB_WIDTH, font_height + 12, HIGHLIGHT_BG_COLOR_R, HIGHLIGHT_BG_COLOR_G, HIGHLIGHT_BG_COLOR_B);
                    
                    font_r = HIGHLIGHT_FONT_COLOR_R;
                    font_g = HIGHLIGHT_FONT_COLOR_G;
                    font_b = HIGHLIGHT_FONT_COLOR_B;
                }
                
                if (uiState == stateHfs0Browser || uiState == stateRomFsSectionBrowser)
                {
                    u8 *icon = (highlight ? (uiState == stateRomFsSectionBrowser ? (romFsBrowserEntries[i].type == ROMFS_ENTRY_DIR ? dirHighlightIconBuf : fileHighlightIconBuf) : fileHighlightIconBuf) : (uiState == stateRomFsSectionBrowser ? (romFsBrowserEntries[i].type == ROMFS_ENTRY_DIR ? dirNormalIconBuf : fileNormalIconBuf) : fileNormalIconBuf));
                    
                    uiDrawIcon(icon, BROWSER_ICON_DIMENSION, BROWSER_ICON_DIMENSION, xpos, (breaks * (font_height + (font_height / 4))) + (j * (font_height + 12)) + 14);
                    
                    xpos += (BROWSER_ICON_DIMENSION + 8);
                }
                
                uiDrawString(menu[i], xpos, (breaks * (font_height + (font_height / 4))) + (j * (font_height + 12)) + 6, font_r, font_g, font_b);
                
                // Print XCI dump menu settings values
                if (uiState == stateXciDumpMenu && i > 0)
                {
                    switch(i)
                    {
                        case 1: // Split output dump (FAT32 support)
                            uiDrawString((isFat32 ? "Yes" : "No"), OPTIONS_X_POS, (breaks * (font_height + (font_height / 4))) + (j * (font_height + 12)) + 6, (isFat32 ? 0 : 255), (isFat32 ? 255 : 0), 0);
                            break;
                        case 2: // Create directory with archive bit set
                            uiDrawString((setXciArchiveBit ? "Yes" : "No"), OPTIONS_X_POS, (breaks * (font_height + (font_height / 4))) + (j * (font_height + 12)) + 6, (setXciArchiveBit ? 0 : 255), (setXciArchiveBit ? 255 : 0), 0);
                            break;
                        case 3: // Dump certificate
                            uiDrawString((dumpCert ? "Yes" : "No"), OPTIONS_X_POS, (breaks * (font_height + (font_height / 4))) + (j * (font_height + 12)) + 6, (dumpCert ? 0 : 255), (dumpCert ? 255 : 0), 0);
                            break;
                        case 4: // Trim output dump
                            uiDrawString((trimDump ? "Yes" : "No"), OPTIONS_X_POS, (breaks * (font_height + (font_height / 4))) + (j * (font_height + 12)) + 6, (trimDump ? 0 : 255), (trimDump ? 255 : 0), 0);
                            break;
                        case 5: // CRC32 checksum calculation + dump verification
                            uiDrawString((calcCrc ? "Yes" : "No"), OPTIONS_X_POS, (breaks * (font_height + (font_height / 4))) + (j * (font_height + 12)) + 6, (calcCrc ? 0 : 255), (calcCrc ? 255 : 0), 0);
                            break;
                        default:
                            break;
                    }
                }
                
                // Print NSP dump menus settings values
                if ((uiState == stateNspAppDumpMenu || uiState == stateNspPatchDumpMenu || uiState == stateNspAddOnDumpMenu) && i > 0)
                {
                    switch(i)
                    {
                        case 1: // Split output dump (FAT32 support)
                            uiDrawString((isFat32 ? "Yes" : "No"), OPTIONS_X_POS, (breaks * (font_height + (font_height / 4))) + (j * (font_height + 12)) + 6, (isFat32 ? 0 : 255), (isFat32 ? 255 : 0), 0);
                            break;
                        case 2: // CRC32 checksum calculation
                            uiDrawString((calcCrc ? "Yes" : "No"), OPTIONS_X_POS, (breaks * (font_height + (font_height / 4))) + (j * (font_height + 12)) + 6, (calcCrc ? 0 : 255), (calcCrc ? 255 : 0), 0);
                            break;
                        case 3: // Bundled application/update/DLC to dump
                            if (uiState == stateNspAppDumpMenu)
                            {
                                // Print application name
                                convertTitleVersionToDecimal(gameCardVersion[selectedAppIndex], versionStr, sizeof(versionStr));
                                snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "%s v%s", gameCardName[selectedAppIndex], versionStr);
                            } else
                            if (uiState == stateNspPatchDumpMenu)
                            {
                                // Find a matching application to print its name
                                // Otherwise, just print the Title ID
                                retrieveDescriptionForPatchOrAddOn(gameCardPatchTitleID[selectedPatchIndex], gameCardPatchVersion[selectedPatchIndex], false, NULL);
                            } else
                            if (uiState == stateNspAddOnDumpMenu)
                            {
                                // Find a matching application to print its name and Title ID
                                // Otherwise, just print the Title ID
                                retrieveDescriptionForPatchOrAddOn(gameCardAddOnTitleID[selectedAddOnIndex], gameCardAddOnVersion[selectedAddOnIndex], true, NULL);
                            }
                            
                            uiDrawString(strbuf, OPTIONS_X_POS, (breaks * (font_height + (font_height / 4))) + (j * (font_height + 12)) + 6, 255, 255, 255);
                            
                            break;
                        default:
                            break;
                    }
                    
                    if (i == 2)
                    {
                        if (calcCrc)
                        {
                            uiDrawString("This takes extra time after the NSP dump has been completed!", FB_WIDTH / 2, (breaks * (font_height + (font_height / 4))) + (j * (font_height + 12)) + 6, 255, 255, 255);
                        } else {
                            uiFill(FB_WIDTH / 2, 8 + (breaks * (font_height + (font_height / 4))) + (j * (font_height + 12)), FB_WIDTH / 2, font_height + 12, (highlight ? HIGHLIGHT_BG_COLOR_R : BG_COLOR_RGB), (highlight ? HIGHLIGHT_BG_COLOR_G : BG_COLOR_RGB), (highlight ? HIGHLIGHT_BG_COLOR_B : BG_COLOR_RGB));
                        }
                    }
                }
                
                // Print RomFS menus settings values
                if ((uiState == stateRomFsSectionDataDumpMenu || uiState == stateRomFsSectionBrowserMenu) && i > 0)
                {
                    switch(i)
                    {
                        case 1: // Bundled application to dump/browse
                            // Print application name
                            convertTitleVersionToDecimal(gameCardVersion[selectedAppIndex], versionStr, sizeof(versionStr));
                            snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "%s v%s", gameCardName[selectedAppIndex], versionStr);
                            uiDrawString(strbuf, OPTIONS_X_POS,  (breaks * (font_height + (font_height / 4))) + (j * (font_height + 12)) + 6, 255, 255, 255);
                            break;
                        default:
                            break;
                    }
                }
                
                if (i == cursor) highlight = false;
            }
        }
        
        uiUpdateStatusMsg();
        uiRefreshDisplay();
        
        hidScanInput();
        keysDown = hidKeysDown(CONTROLLER_P1_AUTO);
        keysHeld = hidKeysHeld(CONTROLLER_P1_AUTO);
        
        // Exit
        if (keysDown & KEY_PLUS) res = resultExit;
        
        // Process key inputs only if the UI state hasn't been changed
        if (res == resultNone)
        {
            // Process base application info change
            if (gameCardAppCount > 1 && uiState != stateHfs0Browser && uiState != stateRomFsSectionBrowser)
            {
                if ((keysDown & KEY_L) || (keysDown & KEY_ZL))
                {
                    if (selectedAppInfoIndex > 0)
                    {
                        selectedAppInfoIndex--;
                    } else {
                        selectedAppInfoIndex = 0;
                    }
                }
                
                if ((keysDown & KEY_R) || (keysDown & KEY_ZR))
                {
                    selectedAppInfoIndex++;
                    if (selectedAppInfoIndex >= gameCardAppCount) selectedAppInfoIndex = (gameCardAppCount - 1);
                }
            }
            
            if (uiState == stateXciDumpMenu)
            {
                // Select
                if ((keysDown & KEY_A) && cursor == 0) res = resultDumpXci;
                
                // Back
                if (keysDown & KEY_B) res = resultShowMainMenu;
                
                // Change option to false
                if (keysDown & KEY_LEFT)
                {
                    switch(cursor)
                    {
                        case 1: // Split output dump (FAT32 support)
                            isFat32 = setXciArchiveBit = false;
                            break;
                        case 2: // Create directory with archive bit set
                            setXciArchiveBit = false;
                            break;
                        case 3: // Dump certificate
                            dumpCert = false;
                            break;
                        case 4: // Trim output dump
                            trimDump = false;
                            break;
                        case 5: // CRC32 checksum calculation + dump verification
                            calcCrc = false;
                            break;
                        default:
                            break;
                    }
                }
                
                // Change option to true
                if (keysDown & KEY_RIGHT)
                {
                    switch(cursor)
                    {
                        case 1: // Split output dump (FAT32 support)
                            isFat32 = true;
                            break;
                        case 2: // Create directory with archive bit set
                            setXciArchiveBit = true;
                            break;
                        case 3: // Dump certificate
                            dumpCert = true;
                            break;
                        case 4: // Trim output dump
                            trimDump = true;
                            break;
                        case 5: // CRC32 checksum calculation + dump verification
                            calcCrc = true;
                            break;
                        default:
                            break;
                    }
                }
                
                // Go up
                if ((keysDown & KEY_DUP) || (keysDown & KEY_LSTICK_UP) || (keysHeld & KEY_RSTICK_UP)) scrollAmount = -1;
                
                // Go down
                if ((keysDown & KEY_DDOWN) || (keysDown & KEY_LSTICK_DOWN) || (keysHeld & KEY_RSTICK_DOWN)) scrollAmount = 1;
            } else
            if (uiState == stateNspAppDumpMenu || uiState == stateNspPatchDumpMenu || uiState == stateNspAddOnDumpMenu)
            {
                // Select
                if ((keysDown & KEY_A) && cursor == 0)
                {
                    selectedNspDumpType = (uiState == stateNspAppDumpMenu ? DUMP_APP_NSP : (uiState == stateNspPatchDumpMenu ? DUMP_PATCH_NSP : DUMP_ADDON_NSP));
                    res = resultDumpNsp;
                }
                
                // Back
                if (keysDown & KEY_B)
                {
                    if (uiState == stateNspAppDumpMenu && !gameCardPatchCount && !gameCardAddOnCount)
                    {
                        res = resultShowMainMenu;
                    } else {
                        res = resultShowNspDumpMenu;
                    }
                }
                
                // Change option to false
                if (keysDown & KEY_LEFT)
                {
                    switch(cursor)
                    {
                        case 1: // Split output dump (FAT32 support)
                            isFat32 = false;
                            break;
                        case 2: // CRC32 checksum calculation
                            calcCrc = false;
                            break;
                        case 3: // Bundled application/update/DLC to dump
                            if (uiState == stateNspAppDumpMenu)
                            {
                                if (selectedAppIndex > 0)
                                {
                                    selectedAppIndex--;
                                } else {
                                    selectedAppIndex = 0;
                                }
                            } else
                            if (uiState == stateNspPatchDumpMenu)
                            {
                                if (selectedPatchIndex > 0)
                                {
                                    selectedPatchIndex--;
                                } else {
                                    selectedPatchIndex = 0;
                                }
                            } else
                            if (uiState == stateNspAddOnDumpMenu)
                            {
                                if (selectedAddOnIndex > 0)
                                {
                                    selectedAddOnIndex--;
                                } else {
                                    selectedAddOnIndex = 0;
                                }
                            }
                            break;
                        default:
                            break;
                    }
                }
                
                // Change option to true
                if (keysDown & KEY_RIGHT)
                {
                    switch(cursor)
                    {
                        case 1: // Split output dump (FAT32 support)
                            isFat32 = true;
                            break;
                        case 2: // CRC32 checksum calculation
                            calcCrc = true;
                            break;
                        case 3: // Bundled application/update/DLC to dump
                            if (uiState == stateNspAppDumpMenu)
                            {
                                if (gameCardAppCount > 1)
                                {
                                    selectedAppIndex++;
                                    if (selectedAppIndex >= gameCardAppCount) selectedAppIndex = (gameCardAppCount - 1);
                                }
                            } else
                            if (uiState == stateNspPatchDumpMenu)
                            {
                                if (gameCardPatchCount > 1)
                                {
                                    selectedPatchIndex++;
                                    if (selectedPatchIndex >= gameCardPatchCount) selectedPatchIndex = (gameCardPatchCount - 1);
                                }
                            } else
                            if (uiState == stateNspAddOnDumpMenu)
                            {
                                if (gameCardAddOnCount > 1)
                                {
                                    selectedAddOnIndex++;
                                    if (selectedAddOnIndex >= gameCardAddOnCount) selectedAddOnIndex = (gameCardAddOnCount - 1);
                                }
                            }
                            break;
                        default:
                            break;
                    }
                }
                
                // Go up
                if ((keysDown & KEY_DUP) || (keysDown & KEY_LSTICK_UP) || (keysHeld & KEY_RSTICK_UP)) scrollAmount = -1;
                
                // Go down
                if ((keysDown & KEY_DDOWN) || (keysDown & KEY_LSTICK_DOWN) || (keysHeld & KEY_RSTICK_DOWN)) scrollAmount = 1;
            } else
            if (uiState == stateRomFsSectionDataDumpMenu || uiState == stateRomFsSectionBrowserMenu)
            {
                // Select
                if ((keysDown & KEY_A) && cursor == 0) res = (uiState == stateRomFsSectionDataDumpMenu ? resultDumpRomFsSectionData : resultRomFsSectionBrowserGetEntries);
                
                // Back
                if (keysDown & KEY_B) res = resultShowMainMenu;
                
                // Change option to false
                if (keysDown & KEY_LEFT)
                {
                    switch(cursor)
                    {
                        case 1: // Bundled application to dump/browse
                            if (selectedAppIndex > 0)
                            {
                                selectedAppIndex--;
                            } else {
                                selectedAppIndex = 0;
                            }
                            break;
                        default:
                            break;
                    }
                }
                
                // Change option to true
                if (keysDown & KEY_RIGHT)
                {
                    switch(cursor)
                    {
                        case 1: // Bundled application to dump/browse
                            if (gameCardAppCount > 1)
                            {
                                selectedAppIndex++;
                                if (selectedAppIndex >= gameCardAppCount) selectedAppIndex = (gameCardAppCount - 1);
                            }
                            break;
                        default:
                            break;
                    }
                }
                
                // Go up
                if ((keysDown & KEY_DUP) || (keysDown & KEY_LSTICK_UP) || (keysHeld & KEY_RSTICK_UP)) scrollAmount = -1;
                
                // Go down
                if ((keysDown & KEY_DDOWN) || (keysDown & KEY_LSTICK_DOWN) || (keysHeld & KEY_RSTICK_DOWN)) scrollAmount = 1;
            } else {
                // Select
                if (keysDown & KEY_A)
                {
                    if (uiState == stateMainMenu)
                    {
                        switch(cursor)
                        {
                            case 0:
                                res = resultShowXciDumpMenu;
                                
                                // Reset options to their default values
                                isFat32 = false;
                                dumpCert = false;
                                trimDump = false;
                                calcCrc = true;
                                break;
                            case 1:
                                if (!gameCardPatchCount && !gameCardAddOnCount)
                                {
                                    res = resultShowNspAppDumpMenu;
                                    
                                    // Reset options to their default values
                                    isFat32 = false;
                                    calcCrc = false;
                                    selectedAppIndex = 0;
                                } else {
                                    res = resultShowNspDumpMenu;
                                }
                                break;
                            case 2:
                                res = resultShowHfs0Menu;
                                break;
                            case 3:
                                res = resultShowRomFsMenu;
                                break;
                            case 4:
                                res = resultDumpGameCardCertificate;
                                break;
                            case 5:
                                res = resultShowUpdateMenu;
                                break;
                            default:
                                break;
                        }
                    } else
                    if (uiState == stateNspDumpMenu)
                    {
                        switch(cursor)
                        {
                            case 0:
                                res = resultShowNspAppDumpMenu;
                                break;
                            case 1:
                                res = (gameCardPatchCount > 0 ? resultShowNspPatchDumpMenu : resultShowNspAddOnDumpMenu);
                                break;
                            case 2:
                                res = resultShowNspAddOnDumpMenu;
                                break;
                            default:
                                break;
                        }
                        
                        // Reset options to their default values
                        isFat32 = false;
                        calcCrc = false;
                        selectedAppIndex = 0;
                        selectedPatchIndex = 0;
                        selectedAddOnIndex = 0;
                    } else
                    if (uiState == stateHfs0Menu)
                    {
                        switch(cursor)
                        {
                            case 0:
                                res = resultShowRawHfs0PartitionDumpMenu;
                                break;
                            case 1:
                                res = resultShowHfs0PartitionDataDumpMenu;
                                break;
                            case 2:
                                res = resultShowHfs0BrowserMenu;
                                break;
                            default:
                                break;
                        }
                    } else
                    if (uiState == stateRawHfs0PartitionDumpMenu)
                    {
                        // Save selected partition index
                        selectedPartitionIndex = (u32)cursor;
                        res = resultDumpRawHfs0Partition;
                    } else
                    if (uiState == stateHfs0PartitionDataDumpMenu)
                    {
                        // Save selected partition index
                        selectedPartitionIndex = (u32)cursor;
                        res = resultDumpHfs0PartitionData;
                    } else
                    if (uiState == stateHfs0BrowserMenu)
                    {
                        // Save selected partition index
                        selectedPartitionIndex = (u32)cursor;
                        res = resultHfs0BrowserGetList;
                    } else
                    if (uiState == stateHfs0Browser)
                    {
                        // Save selected file index
                        selectedFileIndex = (u32)cursor;
                        res = resultHfs0BrowserCopyFile;
                    } else
                    if (uiState == stateRomFsMenu)
                    {
                        switch(cursor)
                        {
                            case 0:
                                res = (gameCardAppCount > 1 ? resultShowRomFsSectionDataDumpMenu : resultDumpRomFsSectionData);
                                
                                // Reset options to their default values
                                selectedAppIndex = 0;
                                
                                break;
                            case 1:
                                res = (gameCardAppCount > 1 ? resultShowRomFsSectionBrowserMenu : resultRomFsSectionBrowserGetEntries);
                                
                                // Reset options to their default values
                                selectedAppIndex = 0;
                                
                                break;
                            default:
                                break;
                        }
                    } else
                    if (uiState == stateRomFsSectionBrowser)
                    {
                        if (menu && menuItemsCount)
                        {
                            // Save selected file index
                            selectedFileIndex = (u32)cursor;
                            res = (romFsBrowserEntries[cursor].type == ROMFS_ENTRY_DIR ? resultRomFsSectionBrowserChangeDir : resultRomFsSectionBrowserCopyFile);
                        }
                    } else
                    if (uiState == stateUpdateMenu)
                    {
                        switch(cursor)
                        {
                            case 0:
                                res = resultUpdateNSWDBXml;
                                break;
                            case 1:
                                res = resultUpdateApplication;
                                break;
                            default:
                                break;
                        }
                    }
                }
                
                // Back
                if (keysDown & KEY_B)
                {
                    if (uiState == stateNspDumpMenu || uiState == stateHfs0Menu || uiState == stateRomFsMenu || uiState == stateUpdateMenu)
                    {
                        res = resultShowMainMenu;
                    } else
                    if (uiState == stateRawHfs0PartitionDumpMenu || uiState == stateHfs0PartitionDataDumpMenu || uiState == stateHfs0BrowserMenu)
                    {
                        res = resultShowHfs0Menu;
                    } else
                    if (uiState == stateHfs0Browser)
                    {
                        free(partitionHfs0Header);
                        partitionHfs0Header = NULL;
                        partitionHfs0HeaderOffset = 0;
                        partitionHfs0HeaderSize = 0;
                        partitionHfs0FileCount = 0;
                        partitionHfs0StrTableSize = 0;
                        
                        res = resultShowHfs0BrowserMenu;
                    } else
                    if (uiState == stateRomFsSectionDataDumpMenu || uiState == stateRomFsSectionBrowserMenu)
                    {
                        res = resultShowRomFsMenu;
                    } else
                    if (uiState == stateRomFsSectionBrowser)
                    {
                        if (strlen(curRomFsPath) > 1)
                        {
                            // Point to the parent directory entry ("..")
                            selectedFileIndex = 0;
                            res = resultRomFsSectionBrowserChangeDir;
                        } else {
                            if (romFsBrowserEntries != NULL)
                            {
                                free(romFsBrowserEntries);
                                romFsBrowserEntries = NULL;
                            }
                            
                            freeRomFsContext();
                            
                            res = (gameCardAppCount > 1 ? resultShowRomFsSectionBrowserMenu : resultShowRomFsMenu);
                        }
                    }
                }
                
                if (menu && menuItemsCount)
                {
                    // Go up
                    if ((keysDown & KEY_DUP) || (keysDown & KEY_LSTICK_UP) || (keysHeld & KEY_RSTICK_UP)) scrollAmount = -1;
                    if ((keysDown & KEY_DLEFT) || (keysDown & KEY_LSTICK_LEFT) || (keysHeld & KEY_RSTICK_LEFT)) scrollAmount = -5;
                    
                    // Go down
                    if ((keysDown & KEY_DDOWN) || (keysDown & KEY_LSTICK_DOWN) || (keysHeld & KEY_RSTICK_DOWN)) scrollAmount = 1;
                    if ((keysDown & KEY_DRIGHT) || (keysDown & KEY_LSTICK_RIGHT) || (keysHeld & KEY_RSTICK_RIGHT)) scrollAmount = 5;
                }
            }
            
            // Calculate scroll only if the UI state hasn't been changed
            if (res == resultNone)
            {
                if (scrollAmount > 0)
                {
                    for(i = 0; i < scrollAmount; i++)
                    {
                        if (cursor < menuItemsCount - 1)
                        {
                            cursor++;
                            if (((uiState != stateHfs0Browser && uiState != stateRomFsSectionBrowser) && (cursor - scroll) >= COMMON_MAX_ELEMENTS) || (uiState == stateHfs0Browser && (cursor - scroll) >= HFS0_MAX_ELEMENTS) || (uiState == stateRomFsSectionBrowser && (cursor - scroll) >= ROMFS_MAX_ELEMENTS)) scroll++;
                        }
                    }
                } else
                if (scrollAmount < 0)
                {
                    for(i = 0; i < -scrollAmount; i++)
                    {
                        if (cursor > 0)
                        {
                            cursor--;
                            if ((cursor - scroll) < 0) scroll--;
                        }
                    }
                }
                
                // Avoid placing the cursor on the "Create directory with archive bit set" option in the XCI dump menu if "Split output dump" is disabled
                if (uiState == stateXciDumpMenu && cursor == 2 && !isFat32)
                {
                    if (scrollAmount > 0)
                    {
                        cursor++;
                    } else
                    if (scrollAmount < 0)
                    {
                        cursor--;
                    }
                }
                
                // Avoid placing the cursor on the "Dump bundled update NSP" option in the NSP dump menu if our current gamecard doesn't include any bundled updates
                // Also avoid placing the cursor on the "Dump bundled DLC NSP" option in the NSP dump menu if our current gamecard doesn't include any bundled DLCs
                if (uiState == stateNspDumpMenu)
                {
                    if ((gameCardPatchCount && !gameCardAddOnCount) || (!gameCardPatchCount && gameCardAddOnCount))
                    {
                        if (cursor >= 2) cursor = 1;
                    } else
                    if (!gameCardPatchCount && !gameCardAddOnCount)
                    {
                        // Just in case
                        cursor = 0;
                    }
                }
                
                // Avoid placing the cursor on the parent directory entry ("..") in the RomFS browser if we're currently at the root directory
                if (uiState == stateRomFsSectionBrowser && cursor == 0 && strlen(curRomFsPath) <= 1) cursor = 1;
            }
        }
    } else
    if (uiState == stateDumpXci)
    {
        uiDrawString(mainMenuItems[0], 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
        breaks++;
        
        snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "%s%s", xciDumpMenuItems[1], (isFat32 ? "Yes" : "No"));
        uiDrawString(strbuf, 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
        breaks++;
        
        if (isFat32)
        {
            snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "%s%s", xciDumpMenuItems[2], (setXciArchiveBit ? "Yes" : "No"));
            uiDrawString(strbuf, 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
            breaks++;
        }
        
        snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "%s%s", xciDumpMenuItems[3], (dumpCert ? "Yes" : "No"));
        uiDrawString(strbuf, 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
        breaks++;
        
        snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "%s%s", xciDumpMenuItems[4], (trimDump ? "Yes" : "No"));
        uiDrawString(strbuf, 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
        breaks++;
        
        snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "%s%s", xciDumpMenuItems[5], (calcCrc ? "Yes" : "No"));
        uiDrawString(strbuf, 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
        breaks += 2;
        
        uiRefreshDisplay();
        
        dumpCartridgeImage(&fsOperatorInstance, isFat32, setXciArchiveBit, dumpCert, trimDump, calcCrc);
        
        waitForButtonPress();
        
        uiUpdateFreeSpace();
        res = resultShowXciDumpMenu;
    } else
    if (uiState == stateDumpNsp)
    {
        uiDrawString(nspDumpMenuItems[selectedNspDumpType], 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
        breaks++;
        
        menu = (selectedNspDumpType == DUMP_APP_NSP ? nspAppDumpMenuItems : (selectedNspDumpType == DUMP_PATCH_NSP ? nspPatchDumpMenuItems : nspAddOnDumpMenuItems));
        
        snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "%s%s", menu[1], (isFat32 ? "Yes" : "No"));
        uiDrawString(strbuf, 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
        breaks++;
        
        snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "%s%s", menu[2], (calcCrc ? "Yes" : "No"));
        uiDrawString(strbuf, 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
        breaks++;
        
        if (selectedNspDumpType == DUMP_APP_NSP)
        {
            convertTitleVersionToDecimal(gameCardVersion[selectedAppIndex], versionStr, sizeof(versionStr));
            snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "%s%s v%s", menu[3], gameCardName[selectedAppIndex], versionStr);
        } else
        if (selectedNspDumpType == DUMP_PATCH_NSP)
        {
            retrieveDescriptionForPatchOrAddOn(gameCardPatchTitleID[selectedPatchIndex], gameCardPatchVersion[selectedPatchIndex], false, menu[3]);
        } else
        if (selectedNspDumpType == DUMP_ADDON_NSP)
        {
            retrieveDescriptionForPatchOrAddOn(gameCardAddOnTitleID[selectedAddOnIndex], gameCardAddOnVersion[selectedAddOnIndex], true, menu[3]);
        }
        
        uiDrawString(strbuf, 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
        breaks += 2;
        
        uiRefreshDisplay();
        
        dumpNintendoSubmissionPackage(&fsOperatorInstance, selectedNspDumpType, (selectedNspDumpType == DUMP_APP_NSP ? selectedAppIndex : (selectedNspDumpType == DUMP_PATCH_NSP ? selectedPatchIndex : selectedAddOnIndex)), isFat32, calcCrc);
        
        waitForButtonPress();
        
        uiUpdateFreeSpace();
        
        res = (selectedNspDumpType == DUMP_APP_NSP ? resultShowNspAppDumpMenu : (selectedNspDumpType == DUMP_PATCH_NSP ? resultShowNspPatchDumpMenu : resultShowNspAddOnDumpMenu));
    } else
    if (uiState == stateDumpRawHfs0Partition)
    {
        snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Raw %s", (hfs0_partition_cnt == GAMECARD_TYPE1_PARTITION_CNT ? hfs0PartitionDumpType1MenuItems[selectedPartitionIndex] : hfs0PartitionDumpType2MenuItems[selectedPartitionIndex]));
        uiDrawString(strbuf, 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
        breaks += 2;
        
        uiRefreshDisplay();
        
        dumpRawHfs0Partition(&fsOperatorInstance, selectedPartitionIndex, true);
        
        waitForButtonPress();
        
        uiUpdateFreeSpace();
        res = resultShowRawHfs0PartitionDumpMenu;
    } else
    if (uiState == stateDumpHfs0PartitionData)
    {
        snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Data %s", (hfs0_partition_cnt == GAMECARD_TYPE1_PARTITION_CNT ? hfs0PartitionDumpType1MenuItems[selectedPartitionIndex] : hfs0PartitionDumpType2MenuItems[selectedPartitionIndex]));
        uiDrawString(strbuf, 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
        breaks += 2;
        
        uiRefreshDisplay();
        
        dumpHfs0PartitionData(&fsOperatorInstance, selectedPartitionIndex);
        
        waitForButtonPress();
        
        uiUpdateFreeSpace();
        res = resultShowHfs0PartitionDataDumpMenu;
    } else
    if (uiState == stateHfs0BrowserGetList)
    {
        uiDrawString((hfs0_partition_cnt == GAMECARD_TYPE1_PARTITION_CNT ? hfs0BrowserType1MenuItems[selectedPartitionIndex] : hfs0BrowserType2MenuItems[selectedPartitionIndex]), 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
        breaks += 2;
        
        uiPleaseWait(0);
        breaks += 2;
        
        if (getHfs0FileList(selectedPartitionIndex))
        {
            cursor = 0;
            scroll = 0;
            res = resultShowHfs0Browser;
        } else {
            waitForButtonPress();
            res = resultShowHfs0BrowserMenu;
        }
    } else
    if (uiState == stateHfs0BrowserCopyFile)
    {
        snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Manual File Dump: %s (HFS0 partition %u [%s])", filenames[selectedFileIndex], selectedPartitionIndex, GAMECARD_PARTITION_NAME(hfs0_partition_cnt, selectedPartitionIndex));
        uiDrawString(strbuf, 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
        breaks += 2;
        
        uiRefreshDisplay();
        
        dumpFileFromHfs0Partition(&fsOperatorInstance, selectedPartitionIndex, selectedFileIndex, filenames[selectedFileIndex]);
        
        waitForButtonPress();
        
        uiUpdateFreeSpace();
        res = resultShowHfs0Browser;
    } else
    if (uiState == stateDumpRomFsSectionData)
    {
        uiDrawString(romFsMenuItems[0], 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
        breaks++;
        
        convertTitleVersionToDecimal(gameCardVersion[selectedAppIndex], versionStr, sizeof(versionStr));
        snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "%s%s v%s", romFsSectionDumpMenuItems[1], gameCardName[selectedAppIndex], versionStr);
        uiDrawString(strbuf, 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
        breaks += 2;
        
        uiRefreshDisplay();
        
        dumpRomFsSectionData(&fsOperatorInstance, selectedAppIndex);
        
        waitForButtonPress();
        
        uiUpdateFreeSpace();
        res = (gameCardAppCount > 1 ? resultShowRomFsSectionDataDumpMenu : resultShowRomFsMenu);
    } else
    if (uiState == stateRomFsSectionBrowserGetEntries)
    {
        uiDrawString(romFsMenuItems[1], 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
        breaks++;
        
        convertTitleVersionToDecimal(gameCardVersion[selectedAppIndex], versionStr, sizeof(versionStr));
        snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "%s%s v%s", romFsSectionBrowserMenuItems[1], gameCardName[selectedAppIndex], versionStr);
        uiDrawString(strbuf, 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
        breaks += 2;
        
        uiPleaseWait(0);
        breaks += 2;
        
        bool romfs_fail = false;
        
        initRomFsContext();
        
        if (readProgramNcaRomFs(selectedAppIndex))
        {
            if (getRomFsFileList(0))
            {
                cursor = 0;
                scroll = 0;
                res = resultShowRomFsSectionBrowser;
            } else {
                freeRomFsContext();
                romfs_fail = true;
            }
        } else {
            romfs_fail = true;
        }
        
        if (romfs_fail)
        {
            breaks += 2;
            waitForButtonPress();
            res = (gameCardAppCount > 1 ? resultShowRomFsSectionBrowserMenu : resultShowRomFsMenu);
        }
    } else
    if (uiState == stateRomFsSectionBrowserChangeDir)
    {
        uiDrawString(romFsMenuItems[1], 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
        breaks++;
        
        convertTitleVersionToDecimal(gameCardVersion[selectedAppIndex], versionStr, sizeof(versionStr));
        snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "%s%s v%s", romFsSectionBrowserMenuItems[1], gameCardName[selectedAppIndex], versionStr);
        uiDrawString(strbuf, 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
        breaks += 2;
        
        bool romfs_fail = false;
        
        if (romFsBrowserEntries[selectedFileIndex].type == ROMFS_ENTRY_DIR)
        {
            if (getRomFsFileList(romFsBrowserEntries[selectedFileIndex].offset))
            {
                cursor = 0;
                scroll = 0;
                res = resultShowRomFsSectionBrowser;
            } else {
                romfs_fail = true;
            }
        } else {
            // Unexpected condition
            uiDrawString("Error: the selected entry is not a directory!", 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
            romfs_fail = true;
        }
        
        if (romfs_fail)
        {
            if (romFsBrowserEntries != NULL)
            {
                free(romFsBrowserEntries);
                romFsBrowserEntries = NULL;
            }
            
            freeRomFsContext();
            
            breaks += 2;
            waitForButtonPress();
            res = (gameCardAppCount > 1 ? resultShowRomFsSectionBrowserMenu : resultShowRomFsMenu);
        }
    } else
    if (uiState == stateRomFsSectionBrowserCopyFile)
    {
        snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Manual File Dump: %s (RomFS)", filenames[selectedFileIndex]);
        uiDrawString(strbuf, 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
        breaks++;
        
        convertTitleVersionToDecimal(gameCardVersion[selectedAppIndex], versionStr, sizeof(versionStr));
        snprintf(strbuf, sizeof(strbuf) / sizeof(strbuf[0]), "Base application: %s v%s", gameCardName[selectedAppIndex], versionStr);
        uiDrawString(strbuf, 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
        breaks += 2;
        
        uiRefreshDisplay();
        
        if (romFsBrowserEntries[selectedFileIndex].type == ROMFS_ENTRY_FILE)
        {
            dumpFileFromRomFsSection(selectedAppIndex, romFsBrowserEntries[selectedFileIndex].offset, true);
        } else {
            // Unexpected condition
            uiDrawString("Error: the selected entry is not a file!", 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
        }
        
        waitForButtonPress();
        
        uiUpdateFreeSpace();
        res = resultShowRomFsSectionBrowser;
    } else
    if (uiState == stateDumpGameCardCertificate)
    {
        uiDrawString(mainMenuItems[4], 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
        breaks += 2;
        
        dumpGameCertificate(&fsOperatorInstance);
        
        waitForButtonPress();
        
        uiUpdateFreeSpace();
        res = resultShowMainMenu;
    } else
    if (uiState == stateUpdateNSWDBXml)
    {
        uiDrawString(updateMenuItems[0], 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
        breaks += 2;
        
        updateNSWDBXml();
        
        waitForButtonPress();
        
        uiUpdateFreeSpace();
        res = resultShowUpdateMenu;
    } else
    if (uiState == stateUpdateApplication)
    {
        uiDrawString(updateMenuItems[1], 8, (breaks * (font_height + (font_height / 4))) + (font_height / 8), 115, 115, 255);
        breaks += 2;
        
        updateApplication();
        
        waitForButtonPress();
        
        uiUpdateFreeSpace();
        res = resultShowUpdateMenu;
    }
    
    return res;
}
