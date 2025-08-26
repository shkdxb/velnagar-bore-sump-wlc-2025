
int currentFile = 0;
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);
void *GIFOpenFile(const char *fname, int32_t *pSize);
void GIFCloseFile(void *pHandle);
int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen);
int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition);
void GIFDraw(GIFDRAW *pDraw);
void play_gif(const char *filename);
void show_jpeg(const char *filename);
void scanDir(fs::FS &fs, const char *dirname);
void gifJpegInitialize();

// #include "demofonts.h"
// #include "Free_Fonts.h" // Include the header file attached to this sketch
// ==== JPEG callback ====
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
    if (y >= tft.height())
        return 0;
    tft.pushImage(x, y, w, h, bitmap);
    return 1;
}

// ==== AnimatedGIF callbacks (unchanged, except Adafruit→TFT_eSPI calls) ====
void *GIFOpenFile(const char *fname, int32_t *pSize)
{
    Serial.printf("[GIFOpenFile] Trying to open: %s\n", fname);
    f = SD.open(fname);
    if (f)
    {
        *pSize = f.size();
        Serial.printf("[GIFOpenFile] Success, size=%d\n", *pSize);
        return (void *)&f;
    }
    Serial.println("[GIFOpenFile] FAILED!");
    return NULL;
}

void GIFCloseFile(void *pHandle)
{
    File *f = static_cast<File *>(pHandle);
    if (f)
        f->close();
}

int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen)
{
    File *f = static_cast<File *>(pFile->fHandle);
    if (!f)
        return 0;
    int32_t n = f->read(pBuf, iLen);
    pFile->iPos = f->position();
    return n;
}

int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition)
{
    File *f = static_cast<File *>(pFile->fHandle);
    f->seek(iPosition);
    pFile->iPos = f->position();
    return pFile->iPos;
}

void GIFDraw(GIFDRAW *pDraw)
{
    uint8_t *s;
    uint16_t *d, *usPalette, usTemp[320];
    int x, y, iWidth;

    iWidth = pDraw->iWidth;
    if (iWidth + pDraw->iX > DISPLAY_WIDTH)
        iWidth = DISPLAY_WIDTH - pDraw->iX;
    usPalette = pDraw->pPalette;
    y = pDraw->iY + pDraw->y; // current line
    // if (y >= DISPLAY_HEIGHT || pDraw->iX >= DISPLAY_WIDTH || iWidth < 1)
    // return;
    int gifMaxY = DISPLAY_HEIGHT - tickerHeight;
    if (y >= gifMaxY || pDraw->iX >= DISPLAY_WIDTH || iWidth < 1)
    {
        updateTicker(); // ~25 FPS
        return;
    }

    s = pDraw->pPixels;
    if (pDraw->ucDisposalMethod == 2) // restore to background color
    {
        for (x = 0; x < iWidth; x++)
        {
            if (s[x] == pDraw->ucTransparent)
                s[x] = pDraw->ucBackground;
        }
        pDraw->ucHasTransparency = 0;
    }

    if (pDraw->ucHasTransparency)
    {
        uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
        int x, iCount;
        pEnd = s + iWidth;
        x = 0;
        iCount = 0;
        while (x < iWidth)
        {
            c = ucTransparent - 1;
            d = usTemp;
            while (c != ucTransparent && s < pEnd)
            {
                c = *s++;
                if (c == ucTransparent)
                {
                    s--;
                }
                else
                {
                    *d++ = usPalette[c];
                    iCount++;
                }
            }
            if (iCount)
            {
                tft.startWrite();
                tft.setAddrWindow(pDraw->iX + x, y, iCount, 1);
                tft.pushPixels(usTemp, iCount);
                tft.endWrite();
                x += iCount;
                iCount = 0;
            }
            c = ucTransparent;
            while (c == ucTransparent && s < pEnd)
            {
                c = *s++;
                if (c == ucTransparent)
                    iCount++;
                else
                    s--;
            }
            if (iCount)
            {
                x += iCount;
                iCount = 0;
            }
        }
    }
    else
    {
        s = pDraw->pPixels;
        for (x = 0; x < iWidth; x++)
            usTemp[x] = usPalette[*s++];
        tft.startWrite();
        tft.setAddrWindow(pDraw->iX, y, iWidth, 1);
        tft.pushPixels(usTemp, iWidth);
        tft.endWrite();
    }
    // // After finishing the last line of the GIF frame:
    // if (pDraw->iY + pDraw->y >= DISPLAY_HEIGHT - 1)
    // {
    //     int remainingOnTime = boreSettings.onTime - boreLastOnTime / (1000 * 60);
    //     // Example: Remaining ON time
    //     tft.setTextSize(2);
    //     tft.setTextColor(TFT_YELLOW, TFT_BLACK); // yellow on black background
    //     tft.setCursor(10, 10);                   // position inside GIF area
    //     tft.print("Remaining ON: ");
    //     tft.print(remainingOnTime); // your variable
    //     tft.print(" min");
    // }
}

// ==== Play one GIF file ====
void play_gif(const char *filename)
{
    Serial.printf("[play_gif] Opening: %s\n", filename);
    if (gif.open(filename, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw))
    {
        Serial.printf("[play_gif] Playing %s...\n", filename);
        while (gif.playFrame(true, NULL))
        {
            blinkLED(boreMode, true);
            blinkLED(sumpMode, false);
        }
        gif.close();
        Serial.printf("[play_gif] Done: %s\n", filename);
    }
    else
    {
        Serial.printf("[play_gif] ERROR opening %s (%d)\n", filename, gif.getLastError());
    }
}

// ==== Show one JPEG file ====
void show_jpeg(const char *filename)
{
    Serial.printf("[show_jpeg] Showing: %s\n", filename);
    tft.fillScreen(TFT_BLACK);
    TJpgDec.drawFsJpg(0, 0, filename, SD); // direct from SD
    // ---- Now overlay text on top ----
    tft.setTextColor(TFT_YELLOW, TFT_BLACK); // text color + background color
    tft.setTextSize(2);
    tft.setCursor(10, 40); // X, Y position
    tft.println("Water level controller");

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(10, 220);
    tft.println(filename); // print filename at bottom
}

// ==== Recursive scanner with debug ====
void scanDir(fs::FS &fs, const char *dirname)
{
    Serial.printf("[scanDir] Entering directory: %s\n", dirname);
    File root = fs.open(dirname);
    if (!root || !root.isDirectory())
    {
        Serial.printf("[scanDir] ERROR: %s is not a directory!\n", dirname);
        return;
    }

    while (true)
    {
        File file = root.openNextFile();
        if (!file)
            break;

        if (file.isDirectory())
        {
            Serial.printf("[scanDir] DIR : %s\n", file.name());
            String subdir = String(dirname);
            if (!subdir.endsWith("/"))
                subdir += "/";
            subdir += file.name();
            scanDir(fs, subdir.c_str()); // recurse
        }
        else
        {
            String fname = file.name();
            Serial.printf("[scanDir] Found file: %s\n", fname.c_str());
            if (fname.endsWith(".gif") || fname.endsWith(".GIF") ||
                fname.endsWith(".jpg") || fname.endsWith(".JPG") ||
                fname.endsWith(".jpeg") || fname.endsWith(".JPEG"))
            {
                String fullpath = String(dirname);
                if (!fullpath.endsWith("/"))
                    fullpath += "/";
                fullpath += fname;
                fullpath.replace("//", "/");

                mediaFiles.push_back(fullpath);
                Serial.printf("[scanDir] --> Added media: %s\n", fullpath.c_str());
            }
        }
        file.close();
    }
}

void gifJpegInitialize()
{

    if (!SD.begin(SD_CS))
    {
        Serial.println("[setup] SD Card mount failed!");
        while (1)
            ;
    }
    Serial.println("[setup] SD mounted!");

    gif.begin(BIG_ENDIAN_PIXELS);
    TJpgDec.setCallback(tft_output); // ✅ JPEG init
    TJpgDec.setSwapBytes(true);

    scanDir(SD, "/");

    Serial.printf("[setup] Total media found: %d\n", mediaFiles.size());
    for (size_t i = 0; i < mediaFiles.size(); i++)
    {
        Serial.printf("[setup] [%d] %s\n", i, mediaFiles[i].c_str());
    }

    if (mediaFiles.empty())
    {
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextSize(2);
        tft.setCursor(10, 100);
        tft.print("No media found!");
    }
}
