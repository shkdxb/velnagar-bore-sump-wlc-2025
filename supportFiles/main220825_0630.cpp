#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <SD.h>
#include <AnimatedGIF.h>
#include <vector>

#define SD_CS 22
#define TFT_CS 5
#define TFT_DC 15

#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 240

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
AnimatedGIF gif;
File f;

std::vector<String> gifFiles;
int currentFile = 0;

// ==== AnimatedGIF callbacks ====
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
    if (y >= DISPLAY_HEIGHT || pDraw->iX >= DISPLAY_WIDTH || iWidth < 1)
        return;
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

    // Apply the new pixels to the main image
    if (pDraw->ucHasTransparency) // if transparency used
    {
        uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
        int x, iCount;
        pEnd = s + iWidth;
        x = 0;
        iCount = 0; // count non-transparent pixels
        while (x < iWidth)
        {
            c = ucTransparent - 1;
            d = usTemp;
            while (c != ucTransparent && s < pEnd)
            {
                c = *s++;
                if (c == ucTransparent) // done, stop
                {
                    s--; // back up to treat it like transparent
                }
                else // opaque
                {
                    *d++ = usPalette[c];
                    iCount++;
                }
            } // while looking for opaque pixels
            if (iCount) // any opaque pixels?
            {
                tft.startWrite();
                tft.setAddrWindow(pDraw->iX + x, y, iCount, 1);
                tft.writePixels(usTemp, iCount, false, false);
                tft.endWrite();
                x += iCount;
                iCount = 0;
            }
            // no, look for a run of transparent pixels
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
                x += iCount; // skip these
                iCount = 0;
            }
        }
    }
    else
    {
        s = pDraw->pPixels;
        // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
        for (x = 0; x < iWidth; x++)
            usTemp[x] = usPalette[*s++];
        tft.startWrite();
        tft.setAddrWindow(pDraw->iX, y, iWidth, 1);
        tft.writePixels(usTemp, iWidth, false, false);
        tft.endWrite();
    }
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
            // keep playing
        }
        gif.close();
        Serial.printf("[play_gif] Done: %s\n", filename);
    }
    else
    {
        Serial.printf("[play_gif] ERROR opening %s (%d)\n", filename, gif.getLastError());
    }
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

            // Build full path for subdirectory
            String subdir = String(dirname);
            if (!subdir.endsWith("/"))
                subdir += "/";
            subdir += file.name();
            subdir.replace("//", "/"); // sanitize

            scanDir(fs, subdir.c_str()); // recurse with absolute path
        }
        else
        {
            String fname = file.name();
            Serial.printf("[scanDir] Found file: %s\n", fname.c_str());
            if (fname.endsWith(".gif") || fname.endsWith(".GIF"))
            {
                // Build full path
                String fullpath = String(dirname);
                if (!fullpath.endsWith("/"))
                    fullpath += "/";
                fullpath += fname;
                fullpath.replace("//", "/"); // sanitize

                gifFiles.push_back(fullpath);
                Serial.printf("[scanDir] --> Added GIF: %s\n", fullpath.c_str());
            }
        }
        file.close();
    }
}

// ==== Recursive scanner with debug ====
void scanDir1(fs::FS &fs, const char *dirname)
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
            scanDir(fs, file.name()); // recurse
        }
        else
        {
            String fname = file.name();
            Serial.printf("[scanDir] Found file: %s\n", fname.c_str());
            if (fname.endsWith(".gif") || fname.endsWith(".GIF"))
            {
                // Build full path
                String fullpath = String(dirname);
                if (!fullpath.endsWith("/"))
                    fullpath += "/";
                fullpath += fname;
                fullpath.replace("//", "/"); // sanitize

                gifFiles.push_back(fullpath);
                Serial.printf("[scanDir] --> Added GIF: %s\n", fullpath.c_str());
            }
        }
        file.close();
    }
}

void setup()
{
    Serial.begin(115200);
    tft.begin();
    tft.setRotation(1);
    tft.fillScreen(ILI9341_BLACK);

    if (!SD.begin(SD_CS))
    {
        Serial.println("[setup] SD Card mount failed!");
        while (1)
            ;
    }
    Serial.println("[setup] SD mounted!");

    gif.begin(LITTLE_ENDIAN_PIXELS);

    scanDir(SD, "/");

    Serial.printf("[setup] Total GIFs found: %d\n", gifFiles.size());
    for (size_t i = 0; i < gifFiles.size(); i++)
    {
        Serial.printf("[setup] [%d] %s\n", i, gifFiles[i].c_str());
    }

    if (gifFiles.empty())
    {
        tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
        tft.setTextSize(2);
        tft.setCursor(10, 100);
        tft.print("No GIFs found!");
    }
}

void loop()
{
    if (gifFiles.empty())
        return;

    String fname = gifFiles[currentFile];
    currentFile = (currentFile + 1) % gifFiles.size();
    tft.fillScreen(ILI9341_BLACK);
    play_gif(fname.c_str());
    delay(2000);
}
