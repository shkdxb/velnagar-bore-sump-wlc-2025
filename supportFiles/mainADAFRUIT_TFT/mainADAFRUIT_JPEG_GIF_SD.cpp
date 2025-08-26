#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
// #include <XPT2046_Touchscreen.h>
#include <SD.h>
#include <AnimatedGIF.h>
#include <vector>
#include <JPEGDecoder.h>

#define SPI_MOSI 23
#define SPI_MISO 19
#define SPI_SCLK 18

#define SD_CS 22

// #define TOUCH_CS  5

#define TFT_CS 5
#define TFT_DC 15

#define BLACK 0x0000
#define BLUE 0x001F
#define RED 0xF800
#define GREEN 0x07E0
#define CYAN 0x07FF
#define MAGENTA 0xF81F
#define YELLOW 0xFFE0
#define WHITE 0xFFFF
#define GREY 0xCE79
#define LIGHTGREY 0xDEDB
#define NAVY 0x000F
#define DARKGREEN 0x03E0
#define DARKCYAN 0x03EF
#define MAROON 0x7800
#define PURPLE 0x780F
#define OLIVE 0x7BE0
#define DARKGREY 0x7BEF
#define ORANGE 0xFDA0
#define GREENYELLOW 0xB7E0
#define PINK 0xFE19
#define BROWN 0x9A60
#define GOLD 0xFEA0
#define SILVER 0xC618
#define SKYBLUE 0x867D
#define VIOLET 0x915C

#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 240

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
// XPT2046_Touchscreen ts(TOUCH_CS);
AnimatedGIF gif;
File f;
std::vector<String> mediaFiles; // ✅ merged GIF + JPEG list
int currentFile = 0;
void scanDir(fs::FS &fs, const char *dirname);
void setup();
void loop();
void *GIFOpenFile(const char *fname, int32_t *pSize);
void GIFCloseFile(void *pHandle);
int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen);
int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition);
void GIFDraw(GIFDRAW *pDraw);
void play_gif(const char *filename);
void drawFSJpeg(const char *filename, int xpos, int ypos);
void jpegRender(int xpos, int ypos);
void jpegInfo();
void createArray(const char *filename);

// #include "gif.h"
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

void setup()
{
    Serial.begin(115200);
    tft.begin();
    tft.setRotation(1);
    tft.fillScreen(ILI9341_BLACK);
    // ts.begin();
    if (!SD.begin(SD_CS))
    {
        Serial.println("[setup] SD Card mount failed!");
        while (1)
            ;
    }
    Serial.println("[setup] SD mounted!");

    //play_gif("/gif/simpsonPumping.gif");
    scanDir(SD, "/");

    Serial.printf("[setup] Total media found: %d\n", mediaFiles.size());
    for (size_t i = 0; i < mediaFiles.size(); i++)
    {
        Serial.printf("[setup] [%d] %s\n", i, mediaFiles[i].c_str());
    }

    if (mediaFiles.empty())
    {
        tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
        tft.setTextSize(2);
        tft.setCursor(10, 100);
        tft.print("No media found!");
    }
    gif.begin(LITTLE_ENDIAN_PIXELS);
}

void loop()
{
    if (mediaFiles.empty())
        return;

    String fname = mediaFiles[currentFile];
    currentFile = (currentFile + 1) % mediaFiles.size();
    if (fname.endsWith(".gif") || fname.endsWith(".GIF"))
    {

        tft.fillScreen(ILI9341_BLACK);
        play_gif(fname.c_str());
    }
    else
    {
        // show_jpeg(fname.c_str());
        drawFSJpeg(fname.c_str(), 0, 0);
        delay(3000); // show JPEG for 3s
    }
}

/*****************************************************************************************************************************************************
 *                                                                  G I F S                                                                          *
 *****************************************************************************************************************************************************/

void *GIFOpenFile(const char *fname, int32_t *pSize)
{
    f = SD.open(fname);
    if (f)
    {
        *pSize = f.size();
        return (void *)&f;
    }
    return NULL;
} /* GIFOpenFile() */

void GIFCloseFile(void *pHandle)
{
    File *f = static_cast<File *>(pHandle);
    if (f != NULL)
        f->close();
} /* GIFCloseFile() */

int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen)
{
    int32_t iBytesRead;
    iBytesRead = iLen;
    File *f = static_cast<File *>(pFile->fHandle);
    // Note: If you read a file all the way to the last byte, seek() stops working
    if ((pFile->iSize - pFile->iPos) < iLen)
        iBytesRead = pFile->iSize - pFile->iPos - 1; // <-- ugly work-around
    if (iBytesRead <= 0)
        return 0;
    iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
    pFile->iPos = f->position();
    return iBytesRead;
} /* GIFReadFile() */

int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition)
{
    int i = micros();
    File *f = static_cast<File *>(pFile->fHandle);
    f->seek(iPosition);
    pFile->iPos = (int32_t)f->position();
    i = micros() - i;
    //  Serial.printf("Seek time = %d us\n", i);
    return pFile->iPos;
} /* GIFSeekFile() */

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

void play_gif(const char *filename)
{
    Serial.println("=====================================");
    Serial.print("Drawing file: ");
    Serial.println(filename);
    Serial.println("=====================================");
    if (gif.open(filename, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw))
    {
        while (gif.playFrame(true, NULL))
        {
        }
        gif.close();
    }
}

/*====================================================================================
  This sketch contains support functions to render the Jpeg images.

  Created by Bodmer 15th Jan 2017
  ==================================================================================*/

// Return the minimum of two values a and b
#define minimum(a, b) (((a) < (b)) ? (a) : (b))

//====================================================================================
//   This function opens the Filing System Jpeg image file and primes the decoder
//====================================================================================
void drawFSJpeg(const char *filename, int xpos, int ypos)
{

    Serial.println("=====================================");
    Serial.print("Drawing file: ");
    Serial.println(filename);
    Serial.println("=====================================");

    // Open the file (the Jpeg decoder library will close it)
    // fs::File jpgFile = SPIFFS.open(filename, "r"); // File handle reference for SPIFFS
     File jpgFile = SD.open( filename, FILE_READ);  // or, file handle reference for SD library

    if (!jpgFile)
    {
        Serial.print("ERROR: File \"");
        Serial.print(filename);
        Serial.println("\" not found!");
        return;
    }

    // To initialise the decoder and provide the file, we can use one of the three following methods:
    // boolean decoded = JpegDec.decodeFsFile(jpgFile); // We can pass the SPIFFS file handle to the decoder,
    boolean decoded = JpegDec.decodeSdFile(jpgFile); // or we can pass the SD file handle to the decoder,
    // boolean decoded = JpegDec.decodeFsFile(filename); // or we can pass the filename (leading / distinguishes SPIFFS files)
                                                      // The filename can be a String or character array
    if (decoded)
    {
        // print information about the image to the serial port
        jpegInfo();

        // render the image onto the screen at given coordinates
        jpegRender(xpos, ypos);
    }
    else
    {
        Serial.println("Jpeg file format not supported!");
    }
}

//====================================================================================
//   Decode and paint onto the TFT screen
//====================================================================================
void jpegRender(int xpos, int ypos)
{

    // retrieve infomration about the image
    uint16_t *pImg;
    uint16_t mcu_w = JpegDec.MCUWidth;
    uint16_t mcu_h = JpegDec.MCUHeight;
    uint32_t max_x = JpegDec.width;
    uint32_t max_y = JpegDec.height;

    // Jpeg images are draw as a set of image block (tiles) called Minimum Coding Units (MCUs)
    // Typically these MCUs are 16x16 pixel blocks
    // Determine the width and height of the right and bottom edge image blocks
    uint32_t min_w = minimum(mcu_w, max_x % mcu_w);
    uint32_t min_h = minimum(mcu_h, max_y % mcu_h);

    // save the current image block size
    uint32_t win_w = mcu_w;
    uint32_t win_h = mcu_h;

    // record the current time so we can measure how long it takes to draw an image
    uint32_t drawTime = millis();

    // save the coordinate of the right and bottom edges to assist image cropping
    // to the screen size
    max_x += xpos;
    max_y += ypos;

    // read each MCU block until there are no more
    while (JpegDec.read())
    {

        // save a pointer to the image block
        pImg = JpegDec.pImage;

        // calculate where the image block should be drawn on the screen
        int mcu_x = JpegDec.MCUx * mcu_w + xpos;
        int mcu_y = JpegDec.MCUy * mcu_h + ypos;

        // check if the image block size needs to be changed for the right edge
        if (mcu_x + mcu_w <= max_x)
            win_w = mcu_w;
        else
            win_w = min_w;

        // check if the image block size needs to be changed for the bottom edge
        if (mcu_y + mcu_h <= max_y)
            win_h = mcu_h;
        else
            win_h = min_h;

        // copy pixels into a contiguous block
        if (win_w != mcu_w)
        {
            for (int h = 1; h < win_h - 1; h++)
            {
                memcpy(pImg + h * win_w, pImg + (h + 1) * mcu_w, win_w << 1);
            }
        }

        // draw image MCU block only if it will fit on the screen
        if ((mcu_x + win_w) <= tft.width() && (mcu_y + win_h) <= tft.height())
        {
            tft.drawRGBBitmap(mcu_x, mcu_y, pImg, win_w, win_h);
        }

        // Stop drawing blocks if the bottom of the screen has been reached,
        // the abort function will close the file
        else if ((mcu_y + win_h) >= tft.height())
            JpegDec.abort();
    }

    // calculate how long it took to draw the image
    drawTime = millis() - drawTime;

    // print the results to the serial port
    Serial.print("Total render time was    : ");
    Serial.print(drawTime);
    Serial.println(" ms");
    Serial.println("=====================================");
}

//====================================================================================
//   Send time taken to Serial port
//====================================================================================
void jpegInfo()
{
    Serial.println(F("==============="));
    Serial.println(F("JPEG image info"));
    Serial.println(F("==============="));
    Serial.print(F("Width      :"));
    Serial.println(JpegDec.width);
    Serial.print(F("Height     :"));
    Serial.println(JpegDec.height);
    Serial.print(F("Components :"));
    Serial.println(JpegDec.comps);
    Serial.print(F("MCU / row  :"));
    Serial.println(JpegDec.MCUSPerRow);
    Serial.print(F("MCU / col  :"));
    Serial.println(JpegDec.MCUSPerCol);
    Serial.print(F("Scan type  :"));
    Serial.println(JpegDec.scanType);
    Serial.print(F("MCU width  :"));
    Serial.println(JpegDec.MCUWidth);
    Serial.print(F("MCU height :"));
    Serial.println(JpegDec.MCUHeight);
    Serial.println(F("==============="));
}

//====================================================================================
//   Open a Jpeg file and dump it to the Serial port as a C array
//====================================================================================
void createArray(const char *filename)
{

    fs::File jpgFile; // File handle reference for SPIFFS
    //  File jpgFile;  // File handle reference For SD library

    if (!(jpgFile = SPIFFS.open(filename, "r")))
    {
        Serial.println(F("JPEG file not found"));
        return;
    }

    uint8_t data;
    byte line_len = 0;
    Serial.println("// Generated by a JPEGDecoder library example sketch:");
    Serial.println("// https://github.com/Bodmer/JPEGDecoder");
    Serial.println("");
    Serial.println("#if defined(__AVR__)");
    Serial.println("  #include <avr/pgmspace.h>");
    Serial.println("#endif");
    Serial.println("");
    Serial.print("const uint8_t ");
    while (*filename != '.')
        Serial.print(*filename++);
    Serial.println("[] PROGMEM = {"); // PROGMEM added for AVR processors, it is ignored by Due

    while (jpgFile.available())
    {

        data = jpgFile.read();
        Serial.print("0x");
        if (abs(data) < 16)
            Serial.print("0");
        Serial.print(data, HEX);
        Serial.print(","); // Add value and comma
        line_len++;
        if (line_len >= 32)
        {
            line_len = 0;
            Serial.println();
        }
    }

    Serial.println("};\r\n");
    // jpgFile.seek( 0, SeekEnd);
    jpgFile.close();
}
//====================================================================================
