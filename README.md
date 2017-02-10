# HY28A-LCDB Drivers  For Raspberry Pi2b / Raspbian Wheezy

Drivers for LCD HY28A-LCDB with these hardware: 
 - ILI9320 for LCD 
 - DS7843 for Touch Panel

** NOTE: **
This isn't an actual driver, and will not load as a module.  It DOES allow basic
communication with the screen through SPI.

# Pre-requisite:

**Hardware:**
 - Raspberry Pi Type B 512Mb Ver2
 - 2.8" inch 320x240 Touch TFT LCD Display Module, SPI Interface, ILI9320, HY28A-LCDB http://www.ebay.it/itm/181008290930

**Wiring Diagram:**
 - See files Wiring.txt

**Linux Distro:**
 - Wheezy image: 2013-02-09-wheezy-raspbian.img on SD 4/8Gb

**Compiler:**
 - gcc version 4.6.3 (Debian 4.6.3-14+rpi1)

**Libraries:**
 - BCM2835 Library Download from: http://www.airspayce.com/mikem/bcm2835/

**Compile:**
 - gcc -o spi -lrt main.c -lbcm2835 -lm -mfloat-abi=hard -Wall

**Execute:**
 - sudo ./spi

## Reference Manual
_Touch Panel Functions_
void TP_Cal(void);
void DrawCross(unsigned short Xpos, unsigned short Ypos);
void TP_DrawPoint(unsigned short Xpos, unsigned short Ypos);
FunctionalState setCalibrationMatrix( Coordinate * displayPtr,Coordinate * screenPtr,Matrix * matrixPtr);
FunctionalState getDisplayPoint(Coordinate * displayPtr,Coordinate * screenPtr,Matrix * matrixPtr );
unsigned short Read_X(void);
unsigned short Read_Y(void);

_LCD Functions:_
long getImageInfo(FILE*, long, int);

int LCD_PutImage(unsigned short, unsigned short, char*);

void LCD_Reset(void);

void LCD_Init(unsigned char);

void LCD_WriteReg(unsigned short, unsigned short);

void LCD_WriteIndex(unsigned char);

void LCD_WriteData(unsigned short);

void LCD_SetPoint(unsigned short, unsigned short, unsigned short);

unsigned short LCD_ReadReg(unsigned short);

unsigned short LCD_ReadData(void);

void LCD_Clear(unsigned short);

void LCD_Text(unsigned short, unsigned short, char *, unsigned short, unsigned short);

void PutChar(unsigned short, unsigned short, unsigned char, unsigned short, unsigned short);

int sgn(int);

void LCD_DrawLine(unsigned short, unsigned short, unsigned short, unsigned short, unsigned short);

void LCD_DrawBox(unsigned short, unsigned short, unsigned short, unsigned short, unsigned short, int);

void LCD_DrawCircle(unsigned short, unsigned short, unsigned short, unsigned short);

void LCD_DrawCircleFill(unsigned short, unsigned short, unsigned short, unsigned short, unsigned short);

void LCD_SetPoint(unsigned short, unsigned short, unsigned short);

unsigned short LCD_GetPoint(unsigned short, unsigned short);

static unsigned short LCD_BGR2RGB(unsigned short);

static void LCD_SetCursor(unsigned short, unsigned short);

void DelayMicrosecondsNoSleep(int delay_us);

void LCD_DisplayOn(void);

void LCD_DisplayOff(void);

_Details in file main.c_

