/********************************************************************************
  Function Name : main
  Description : Driver for LCD HY28A-LCDB 
  using: ILI9320 for LCD & ADS7843 for Touch Panel
  Input : None
  Output : None
  Return : None
  Compile/link : gcc -o spi -lrt main.c -lbcm2835 -lm* gcc -o spi -lrt main.c -lbcm2835 -lm -mfloat-abi=hard -Wall
  Execute : sudo ./spi

  Author Github: https://github.com/topogigio
  Updated By : mrmwogli / https://github.com/mrmowgli/HY28A-LCDB-Drivers

*******************************************************************************/


/* Includes */
#include <bcm2835.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "fonts.h"

/* Defines */
#define MAX_X 240
#define MAX_Y 320
#define CHY 0x90 /* channel Y+ selection command */
#define CHX 0xd0 /* channel X+ selection command */

#define SPI_START (0x70) /* Start byte for SPI transfer */
#define SPI_RD (0x01) /* WR bit 1 within start */
#define SPI_WR (0x00) /* WR bit 0 within start */
#define SPI_DATA (0x02) /* RS bit 1 within start byte */
#define SPI_INDEX (0x00) /* RS bit 0 within start byte */
#define DIVIDER_CS0 BCM2835_SPI_CLOCK_DIVIDER_8
#define DIVIDER_CS1 BCM2835_SPI_CLOCK_DIVIDER_64

/* 
  There are 2 arrow on lcd pcb one left one right of the glass; 
  arrow means up start from landscape & rotate 90 degree clockwise 0 landscape xy origin 
  lower left corner 1 & 2 not yet implemented 
  3 portrait xy origin upper left corner
*/
#define LANDSCAPE 0
#define PORTRAIT 3 

/* LCD colors */
#define White 0xFFFF
#define Black 0x0000
#define Grey 0xF7DE
#define Blue 0x001F
#define Blue2 0x051F
#define Red 0xF800
#define Magenta 0xF81F
#define Green 0x07E0
#define Cyan 0x7FFF
#define Yellow 0xFFE0/* GPIOs */

#define RESET RPI_GPIO_P1_22 // GPIO25
#define BACKLIGHT RPI_GPIO_P1_12 // GPIO18
#define IRQ RPI_V2_GPIO_P1_18 // GPIO24
#define RGB565CONVERT(red, green, blue)\(unsigned short)( (( red >> 3 ) << 11 ) | \(( green >> 2 ) << 5 ) | \( blue >> 3 ))

#define THRESHOLD 2 /* threshold */

/* Types */
typedef struct {int rows; int cols; unsigned char* data;} sImage;
typedef enum { DISABLE = 0, ENABLE = !DISABLE } FunctionalState;
typedef struct POINT{ unsigned short x; unsigned short y;} Coordinate;
typedef struct Matrix{long double An, Bn, Cn, Dn, En, Fn, Divider ;} Matrix;

/* Function declarations */
void TP_Init(void);
void IRQ_Clear(void);
unsigned char IRQ_Test(void);
Coordinate *Read_Ads7846(void);
void TP_Cal(void);
void DrawCross(unsigned short Xpos, unsigned short Ypos);
void TP_DrawPoint(unsigned short Xpos, unsigned short Ypos);
FunctionalState setCalibrationMatrix( Coordinate * displayPtr,Coordinate * screenPtr,Matrix * matrixPtr);
FunctionalState getDisplayPoint(Coordinate * displayPtr,Coordinate * screenPtr,Matrix * matrixPtr );
unsigned short Read_X(void);
unsigned short Read_Y(void);
long getImageInfo(FILE*, long, int);
int LCD_PutImage(unsigned short, unsigned short, char*);
void LCD_Reset(void);
void LCD_Init(unsigned char);
void LCD_WriteReg(unsigned short , unsigned short);
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

/* Public declarations */
static unsigned char Orient;
static Matrix matrix;
static Coordinate display;
static Coordinate ScreenSample[3];
static Coordinate DisplaySample[3] = { {45, 45}, {45, 270}, {190, 190} };
static Coordinate Screen;

int main(void){ 
  if (!bcm2835_init()) return 1;
    LCD_Reset();
    // TP_Init must be called before LCD_Init TP_Init();
    // LANDSCAPE xy origin lower left corner 
    // PORTRAIT xy origin upper left corner LCD_Init(PORTRAIT);
    TP_Cal();
    LCD_Text(50, 50, "Testing touch!", Magenta, Yellow);
    LCD_DrawLine(0, 0, 240, 320, White);
    LCD_DrawBox(10, 5, 30, 20, White, Blue);
    LCD_DrawCircleFill(100, 100, 31, Blue, White);
    getchar();
    LCD_DisplayOff();
    getchar();
  LCD_DisplayOn();
  getchar();
  // Image is composed from bottom to up due to BMP file format 
  // Orientation affect drawing, bottom to up 
  LCD_PutImage(50, 200, "test2.bmp");
  // Orientation affect clearing, up to bottom LCD_Clear(Black);
  while(1) { 
    getDisplayPoint(&display, Read_Ads7846(), &matrix);
    //printf("x: %d - y: %d", display.x, display.y);
    TP_DrawPoint(display.x, display.y);
  } 
  IRQ_Clear();
  bcm2835_spi_end();
  bcm2835_close();
  return 0;
 }

 
/********************************************************************************
  Function Name : IRQ_Clear 
  Description : Test if LCD is touched 
  Input : None 
  Output : None 
  Return : None 
  Attention : None
*******************************************************************************/
void IRQ_Clear(){ 
  bcm2835_gpio_clr_ren(IRQ); 
  bcm2835_gpio_clr_fen(IRQ); 
  bcm2835_gpio_clr_hen(IRQ); 
  bcm2835_gpio_clr_len(IRQ); 
  bcm2835_gpio_clr_aren(IRQ); 
  bcm2835_gpio_clr_afen(IRQ);
}

/********************************************************************************
  Function Name : IRQ_Test
  Description : Test if LCD is touched
  Input : None
  Output : None
  Return : 1 if you touch display 0 normal condition
  Attention : None
*******************************************************************************/
 unsigned char IRQ_Test(){ 
  unsigned char value, eds; 
  value = bcm2835_gpio_lev(IRQ); 
  //printf("pin value during loop: %d\r", value); 
  eds = bcm2835_gpio_eds(IRQ); 
  //printf("Event Detect Status: %d\n", eds); 
  if (0 != eds) { 
    // Now clear the eds flag by setting it to 1 
    bcm2835_gpio_set_eds(IRQ); 
    // event detected for pin 
    return (1); 
  } 
  return value;
}

/********************************************************************************
  Function Name : getImageInfo
  Description : sub for LCD_PutImage
  Input : None
  Output : None
  Return : None
  Attention : None
*******************************************************************************/
long getImageInfo(FILE* inputFile, long offset, int numberOfChars){ 
  unsigned char *ptrC; 
  long value=0L; 
  int i; 
  unsigned char dummy; 
  dummy = '0'; 
  ptrC = &dummy; 
  
  fseek(inputFile, offset, SEEK_SET); 
  for(i=1; i<=numberOfChars; i++) { 
    fread(ptrC, sizeof(char), 1, inputFile); 
    // calculate value based on adding bytes 
    value = (long)(value + (*ptrC) * (pow(256, (i-1)))); 
  } 
  return(value);
} 

/********************************************************************************
  Function Name : LCD_PutImage
  Description : Show BMP
  Input : 
    x : upper left corner image start
    y : upper left corner image start
    file:  filename full qualified path
  Output : None
  Return : None
  Attention : The image must be 24 bits RGB (sub will convert to 16 bits)
*******************************************************************************/
 int LCD_PutImage(unsigned short x, unsigned short y, char* file){ 
  FILE *bmpInput; 
  sImage originalImage; 
  unsigned char someChar; 
  unsigned char *pChar; 
  long fileSize; 
  int nColors; 
  int r, c; 
  unsigned short redValue, greenValue, blueValue; 

  /*--------INITIALIZE POINTER----------*/
    someChar = '0'; 
    pChar = &someChar; 
    printf("Reading file %s\n", file); 
  
    /*----DECLARE INPUT AND OUTPUT FILES----*/
    bmpInput = fopen(file, "rb"); 
    fseek(bmpInput, 0L, SEEK_END);

    /*-----GET BMP INFO-----*/
    originalImage.cols = (int)getImageInfo(bmpInput, 18, 4); 
    originalImage.rows = (int)getImageInfo(bmpInput, 22, 4); 
    fileSize = getImageInfo(bmpInput, 2, 4); 
    nColors = getImageInfo(bmpInput, 46, 4); 

    /*----PRINT BMP INFO TO SCREEN-----*/
    printf("Width: %d\n", originalImage.cols); 
    printf("Height: %d\n", originalImage.rows); 
    printf("File size: %ld\n", fileSize); 
    printf("Bits/pixel: %lu\n", getImageInfo(bmpInput, 28, 4)); 
    printf("No. colors: %d\n", nColors); 

    /*----FOR 24-BIT BMP, THERE IS NO COLOR TABLE-----*/
    fseek(bmpInput, 54, SEEK_SET); 
    if ( (Orient==1) || (Orient==3) ) { 
      /*-----------READ RASTER DATA-----------*/
      for(c=originalImage.cols-1; c>=0; c--) { 
        for(r=0; r<=originalImage.rows-1; r++) { 
          /*----READ FIRST BYTE TO GET BLUE VALUE-----*/
          fread(pChar, sizeof(char), 1, bmpInput); blueValue = *pChar; 
          /*-----READ NEXT BYTE TO GET GREEN VALUE-----*/
          fread(pChar, sizeof(char), 1, bmpInput); greenValue = *pChar; 
          /*-----READ NEXT BYTE TO GET RED VALUE-----*/
          fread(pChar, sizeof(char), 1, bmpInput); redValue = *pChar; 
          /*---------PRINT PIXEL TO LCD---------*/
          LCD_SetPoint(r+x, c+y, RGB565CONVERT(redValue, greenValue, blueValue)); 
        } 
      } 
    } else { 
      for(r=0; r<=originalImage.rows-1; r++) { 
        for(c=0; c<=originalImage.cols-1; c++) { 
          /*----READ FIRST BYTE TO GET BLUE VALUE-----*/
          fread(pChar, sizeof(char), 1, bmpInput); blueValue = *pChar; /*-----READ NEXT BYTE TO GET GREEN VALUE-----*/
          fread(pChar, sizeof(char), 1, bmpInput); greenValue = *pChar; /*-----READ NEXT BYTE TO GET RED VALUE-----*/
          fread(pChar, sizeof(char), 1, bmpInput); redValue = *pChar; /*---------PRINT PIXEL TO LCD---------*/
          LCD_SetPoint(r+x, c+y, RGB565CONVERT(redValue, greenValue, blueValue)); 
        } 
      } 
    } 
    fclose(bmpInput); 
    return 0;
  }

/********************************************************************************
  Function Name : LCD_Reset
  Description : LCD TFT Controller.
  Input : None
  Output : None
  Return : None
  Attention : None
*******************************************************************************/
void LCD_Reset(){ 
  // Set the RESET pin to be an output 
  bcm2835_gpio_fsel(RESET, BCM2835_GPIO_FSEL_OUTP); 
  bcm2835_gpio_write(RESET, HIGH); //reset is low active 
  DelayMicrosecondsNoSleep (5000); //almost 1ms = 1000us 
  bcm2835_gpio_write(RESET, LOW); //reset is low active 
  DelayMicrosecondsNoSleep (15000); //almost 10ms = 10000us 
  bcm2835_gpio_write(RESET, HIGH); //reset is low active 
  DelayMicrosecondsNoSleep (15000); //almost 10ms = 10000us
}

/********************************************************************************
  Function Name : LCD_Init
  Description : Initialize TFT Controller.
  Input : 
    ori : 0=landscape 3=portrait clockwise 1 & 2 not yet implemented
  Output : None
  Return : None
  Attention : None
*******************************************************************************/
void LCD_Init(unsigned char ori){ 
  unsigned short DeviceCode; 
  bcm2835_gpio_fsel(BACKLIGHT, BCM2835_GPIO_FSEL_OUTP); 
  bcm2835_gpio_write(BACKLIGHT, HIGH); //HIGH=on, LOW=off; 
  bcm2835_spi_begin(); 
  bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST); // MSB The default 
  bcm2835_spi_setDataMode(BCM2835_SPI_MODE3); // MODE3 
  bcm2835_spi_setClockDivider(DIVIDER_CS0); // 16 The default 4096 
  bcm2835_spi_chipSelect(BCM2835_SPI_CS0); // The default 
  bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW); // the default 

  /*  
    Send a some bytes to the slave and simultaneously read some bytes back 
    from the slave most SPI devices expect one or 2 bytes of command, after 
    which they will send back some data. In such a case you will have the 
    command bytes first in the buffer, followed by as many 0 bytes as you 
    expect returned data bytes. after the transfer, you can the read the 
    reply bytes from the buffer. If you tie MISO to MOSI, you should read 
    back what was sent. 
  */
  DeviceCode = LCD_ReadReg(0x0000); /* Read ID */
  
  /* Different driver IC initialization */
  if( DeviceCode == 0x9320 || DeviceCode == 0x9300 ) {  
    printf("DeviceCode: %hu\n", DeviceCode); 
  } else {  
    printf("other Code: %hu\n", DeviceCode); 
  } 
  LCD_WriteReg(0x00,0x0000); 
  LCD_WriteReg(0x01,0x0100); /* Driver Output Contral */
  LCD_WriteReg(0x02,0x0700); /* LCD Driver Waveform Contral */
  switch (ori) { 
    case 0: 
      LCD_WriteReg(0x03,0x1008); /* 1008 Set the scan mode landscape */
      break; 
    case 3: LCD_WriteReg(0x03,0x1030); /* 1030 Set the scan mode portrait */
    break; 
  } 
  
  Orient = ori; 

  LCD_WriteReg(0x04,0x0000); /* Scalling Contral */
  LCD_WriteReg(0x08,0x0202); /* Display Contral 2 */
  LCD_WriteReg(0x09,0x0000); /* Display Contral 3 */
  LCD_WriteReg(0x0a,0x0000); /* Frame Cycle Contal */
  LCD_WriteReg(0x0c,(1<<0)); /* Extern Display Interface Contral 1 */
  LCD_WriteReg(0x0d,0x0000); /* Frame Maker Position */
  LCD_WriteReg(0x0f,0x0000); /* Extern Display Interface Contral 2 */
  delay(50); LCD_WriteReg(0x07,0x0101); /* Display Contral */
  delay(50); LCD_WriteReg(0x10,(1<<12)|(0<<8)|(1<<7)|(1<<6)|(0<<4)); /* Power Control 1 */
  LCD_WriteReg(0x11,0x0007); /* Power Control 2 */
  LCD_WriteReg(0x12,(1<<8)|(1<<4)|(0<<0)); /* Power Control 3 */
  LCD_WriteReg(0x13,0x0b00); /* Power Control 4 */
  LCD_WriteReg(0x29,0x0000); /* Power Control 7 */
  LCD_WriteReg(0x2b,(1<<14)|(1<<4)); LCD_WriteReg(0x50,0); /* Set X Start */
  LCD_WriteReg(0x51,239); /* Set X End */
  LCD_WriteReg(0x52,0); /* Set Y Start */
  LCD_WriteReg(0x53,319); /* Set Y End */
  delay(50); LCD_WriteReg(0x60,0x2700); /* Driver Output Control */
  LCD_WriteReg(0x61,0x0001); /* Driver Output Control */
  LCD_WriteReg(0x6a,0x0000); /* Vertical Scroll Control */
  LCD_WriteReg(0x80,0x0000); /* Display Position? Partial Display 1 */
  LCD_WriteReg(0x81,0x0000); /* RAM Address Start? Partial Display 1 */
  LCD_WriteReg(0x82,0x0000); /* RAM Address End-Partial Display 1 */
  LCD_WriteReg(0x83,0x0000); /* Display Position? Partial Display 2 */
  LCD_WriteReg(0x84,0x0000); /* RAM Address Start? Partial Display 2 */
  LCD_WriteReg(0x85,0x0000); /* RAM Address End? Partial Display 2 */
  LCD_WriteReg(0x90,(0<<7)|(16<<0)); /* Frame Cycle Contral */
  LCD_WriteReg(0x92,0x0000); /* Panel Interface Contral 2 */
  LCD_WriteReg(0x93,0x0001); /* Panel Interface Contral 3 */
  LCD_WriteReg(0x95,0x0110); /* Frame Cycle Contral */
  LCD_WriteReg(0x97,(0<<8)); LCD_WriteReg(0x98,0x0000); /* Frame Cycle Contral */
  LCD_WriteReg(0x07,0x0133); delay(100); /* delay 50 ms */
}

/********************************************************************************
  Function Name : LCD_WriteReg
  Description : Writes to the selected LCD register.
  Input : 
    LCD_Reg: address of the selected register.
    LCD_RegValue: value to write to the selected register.
  Output : None
  Return : None
  Attention : None
*******************************************************************************/
void LCD_WriteReg( unsigned short LCD_Reg, unsigned short LCD_RegValue){ 
  /* Write 16-bit Index, then Write Reg */
  LCD_WriteIndex(LCD_Reg); /* Write 16-bit Reg */
  LCD_WriteData(LCD_RegValue);
}

/********************************************************************************
  Function Name : LCD_WriteIndex
  Description : LCD write register address
  Input : 
    index: register address
  Output : None
  Return : None
  Attention : None
*******************************************************************************/
void LCD_WriteIndex(unsigned char index){ 
  char buf[] = { SPI_START | SPI_WR | SPI_INDEX, 0, index}; 
  bcm2835_spi_transfern(buf, sizeof(buf)); 
  //uncomment for debug 
  //printf("SPI: WriteIndex: %02X %02X %02X \n", buf[0], buf[1], buf[2]);
}

/********************************************************************************
  Function Name : LCD_WriteData
  Description : LCD write register data
  Input : 
    data: register data
  Output : None
  Return : None
  Attention : None
*******************************************************************************/
void LCD_WriteData(unsigned short data){ 
  char buf[] = { SPI_START | SPI_WR | SPI_DATA, (data >> 8), (data & 0xFF)}; 
  bcm2835_spi_transfern(buf, sizeof(buf)); 
  //uncomment for debug //printf("SPI: WriteData: %02X %02X %02X \n", buf[0], buf[1], buf[2]);
}

/******************************************************************************* 
  Function Name : LCD_SetPoint
  Description : Drawn at a specified point coordinates
  Input : 
    Xpos: Row Coordinate
    Ypos: Line Coordinate
  Output : None
  Return : None
  Attention : None
*******************************************************************************/
void LCD_SetPoint(unsigned short Xpos, unsigned short Ypos, unsigned short point){ 
  if( Xpos >= MAX_X || Ypos >= MAX_Y ) { return; } 
  LCD_SetCursor(Xpos,Ypos); 
  LCD_WriteReg(0x0022,point); // (REG, VALUE)
}

/********************************************************************************
  Function Name : LCD_ReadReg
  Description : Reads the selected LCD Register.
  Input : None
  Output : None
  Return : LCD Register Value.
  Attention : None
*******************************************************************************/
unsigned short LCD_ReadReg(unsigned short LCD_Reg){ 
  unsigned short LCD_RAM; /* Write 16-bit Index (then Read Reg) */
  LCD_WriteIndex(LCD_Reg); /* Read 16-bit Reg */
  LCD_RAM = LCD_ReadData(); 
  return LCD_RAM;
}

/********************************************************************************
  Function Name : LCD_ReadData
  Description : LCD read data
  Input : None
  Output : None
  Return : return data
  Attention : None
*******************************************************************************/
unsigned short LCD_ReadData(void){
  unsigned short value; 
  char buf[] = { SPI_START | SPI_RD | SPI_DATA, 0, 0,0}; // Data to send 
  bcm2835_spi_transfern(buf, sizeof(buf)); 
  value = (short int)buf[3] + ((short int)buf[2]<<8); 
  return value;
}

/********************************************************************************
  Function Name : LCD_SetCursor
  Description : Sets the cursor position.
  Input : 
    Xpos: specifies the X position.
    Ypos: specifies the Y position.
  Output : None
  Return : None
  Attention : None
*******************************************************************************/
static void LCD_SetCursor(unsigned short Xpos, unsigned short Ypos ){
  /* 0x9320 */
  LCD_WriteReg(0x0020, Xpos ); 
  LCD_WriteReg(0x0021, Ypos );
}

/********************************************************************************
  Function Name : DelayMicrosecondsNoSleep
  Description : Delay n microseconds
  Input : 
    delay_us: specifies the n microseconds
  Output : None
  Return : None
  Attention : None
*******************************************************************************/
void DelayMicrosecondsNoSleep (int delay_us){ 
  long int start_time; 
  long int time_difference; 
  struct timespec gettime_now; 
  clock_gettime(CLOCK_REALTIME, &gettime_now); 
  start_time = gettime_now.tv_nsec; //Get nS value 
  while (1) { 
    clock_gettime(CLOCK_REALTIME, &gettime_now); 
    time_difference = gettime_now.tv_nsec - start_time; 
    if (time_difference < 0)  
      time_difference += 1000000000; //(Rolls over every 1 second)  
    if (time_difference > (delay_us * 1000)) //Delay for # nS 
      break; 
  }
}

/********************************************************************************
  Function Name : GetASCIICode
  Description : get ASCII code data
  Input : 
    ASCII: Input ASCII code
  Output : 
    *pBuffer: Store data pointer
  Return : None
  Attention : None
*******************************************************************************/
void GetASCIICode(unsigned char* pBuffer,unsigned char ASCII){ 
  memcpy(pBuffer,AsciiLib[(ASCII - 32)] ,16);
}

/********************************************************************************
  Function Name : LCD_Clear
  Description : Fill the screen with the specified color
  Input : 
    Color: Screen Color
  Output : None
  Return : None
  Attention : None
*******************************************************************************/
void LCD_Clear(unsigned short Color){ 
  unsigned int y=0; 
  LCD_SetPoint(0,0,Color); 
  for (y=1; y<MAX_Y*MAX_X; y++ ) { 
    LCD_WriteReg(0x0022,Color); 
  }
}

/******************************************************************************* 
  Function Name : LCD_BGR2RGB
  Description : RRRRRGGGGGGBBBBB To BBBBBGGGGGGRRRRR
  Input :
    color: BRG Color value
  Output : None
  Return : RGB Color value
  Attention : None
*******************************************************************************/
static unsigned short LCD_BGR2RGB(unsigned short color){ 
  unsigned short r, g, b, rgb; 
  b = ( color>>0 ) & 0x1f; 
  g = ( color>>5 ) & 0x3f; 
  r = ( color>>11 ) & 0x1f; 
  rgb = (b<<11) + (g<<5) + (r<<0); 
  return( rgb );
}

/******************************************************************************* 
  Function Name : LCD_GetPoint
  Description : Get color value for the specified coordinates
  Input :
    Xpos: Row Coordinate
    Xpos: Line Coordinate
  Output : None
  Return : Screen Color
  Attention : None
*******************************************************************************/
unsigned short LCD_GetPoint(unsigned short Xpos, unsigned short Ypos){ 
  unsigned short dummy; 
  LCD_SetCursor(Xpos,Ypos); 
  LCD_WriteIndex(0x0022); 
  dummy = LCD_ReadData(); /* An empty read */
  dummy = LCD_ReadData(); 
  return LCD_BGR2RGB(dummy);
}

/******************************************************************************* 
  Function Name : PutChar
  Description : Lcd screen displays a character
  Input : - 
    Xpos: Horizontal coordinate
    Ypos: Vertical coordinate
    ASCI: Displayed character
    charColor: Character color
    bkColor: Background color
  Output : None
  Return : None
  Attention : None
*******************************************************************************/
void PutChar(unsigned short Xpos, unsigned short Ypos, unsigned char ASCI, unsigned short charColor, unsigned short bkColor ){ 
  unsigned short i, j; 
  unsigned char buffer[16], tmp_char; 
  GetASCIICode(buffer,ASCI); /* get font data */
  if (Orient == 3) { 
    for( i=0; i<16; i++ ) { 
      tmp_char = buffer[i]; 
      for( j=0; j<8; j++ ) { 
        if(((tmp_char >> (7 - j)) & 0x01) == 0x01 ) { 
          LCD_SetPoint( Xpos + j, Ypos + i, charColor ); /* Character color */
        } else { 
          LCD_SetPoint( Xpos + j, Ypos + i, bkColor ); /* Background color */
        } 
      } 
    } 
  } else { 
    for( i=0; i<16; i++ ) { 
      tmp_char = buffer[i]; 
      for( j=0; j<8; j++ ) { 
        if(((tmp_char >> (7 - j)) & 0x01) == 0x01 ) { 
          LCD_SetPoint( Ypos - i, Xpos + j, charColor ); /* Character color */
        } else { 
          LCD_SetPoint( Ypos - i, Xpos + j, bkColor ); /* Background color */
        } 
      } 
    } 
  }
}

/******************************************************************************* 
  Function Name : LCD_Text
  Description : Displays the string
  Input : 
    Xpos: Horizontal coordinate 
    Ypos: Vertical coordinate 
    str: Displayed string
    charColor: Character color
    bkColor: Background color
  Output : None
  Return : None
  Attention : None
*******************************************************************************/
void LCD_Text(unsigned short Xpos, unsigned short Ypos, char *str, unsigned short Color, unsigned short bkColor){ 
  unsigned short TempChar; 
  do { 
    TempChar = *str++; 
    PutChar(Xpos, Ypos, TempChar, Color, bkColor ); 
    if( Xpos < MAX_X - 8 ) { 
      Xpos += 8; 
    } else if (Ypos < MAX_Y - 16 ) { 
      Xpos = 0; Ypos += 16; 
    } else { 
      Xpos = 0; Ypos = 0; 
    } 
  } while ( *str != 0 );
}

/******************************************************************************* 
  Function Name : sgn
  Description : return the sign of number
  Input : 
    nu: the number
  Output : None
  Return : 1 if > 0; -1 if < 0; 0 of = 0
  Attention : None
*******************************************************************************/
int sgn(int nu){ 
  if (nu > 0) return 1; 
  if (nu < 0) return -1; 
  if (nu == 0) return 0; 
  return 0;
}

/******************************************************************************* 
  Function Name : LCD_DrawLine
  Description : Bresenham's line algorithm
  Input : 
    x1: A point line coordinates
    y1: A point column coordinates
    x2: B point line coordinates
    y2: B point column coordinates
    col: Line color
  Output : None
  Return : None
  Attention : None
*******************************************************************************/
void LCD_DrawLine(unsigned short x1, unsigned short y1, unsigned short x2, unsigned short y2, unsigned short col){ 
  unsigned short n, deltax, deltay, sgndeltax, sgndeltay, deltaxabs, deltayabs, x, y, drawx, drawy;
  
  deltax = x2 - x1; 
  deltay = y2 - y1; 
  deltaxabs = abs(deltax); 
  deltayabs = abs(deltay); 
  sgndeltax = sgn(deltax); 
  sgndeltay = sgn(deltay); 
  x = deltayabs >> 1; 
  y = deltaxabs >> 1; 
  drawx = x1; 
  drawy = y1; 
  LCD_SetPoint(drawx, drawy, col); 
  if (deltaxabs >= deltayabs){ 
    for (n = 0; n < deltaxabs; n++){ 
      y += deltayabs; 
      if (y >= deltaxabs){ 
        y -= deltaxabs; 
        drawy += sgndeltay; 
      } 
      drawx += sgndeltax; 
      LCD_SetPoint(drawx, drawy, col); 
    } 
  } else { 
    for (n = 0; n < deltayabs; n++){ 
      x += deltaxabs; 
      if (x >= deltayabs){ 
        x -= deltayabs; 
        drawx += sgndeltax; 
      } 
      drawy += sgndeltay; 
      LCD_SetPoint(drawx, drawy, col); 
    } 
  }
}

/******************************************************************************* 
  Function Name : LCD_DrawBox
  Description : Multiple line makes box
  Input : 
    x1: A point line coordinates upper left corner
    y1: A point column coordinates
    x2: B point line coordinates lower right corner
    y2: B point column coordinates
    col: Line color
  Output : None
  Return : None
******************************************************************************/
void LCD_DrawBox(unsigned short x0, unsigned short y0, unsigned short x1, unsigned short y1 , unsigned short col, int fcol ){ 
  unsigned short i, xx0, xx1, yy0; 
  LCD_DrawLine(x0, y0, x1, y0, col); 
  LCD_DrawLine(x1, y0, x1, y1, col); 
  LCD_DrawLine(x0, y0, x0, y1, col); 
  LCD_DrawLine(x0, y1, x1, y1, col); 
  if (fcol!=-1) { 
    for (i=0; i<y1-y0-1; i++) { 
      xx0=x0+1; 
      yy0=y0+1+i; 
      xx1=x1-1; 
      LCD_DrawLine(xx0, yy0, xx1, yy0, (unsigned short)fcol); 
    } 
  }
}

/******************************************************************************* 
  Function Name : drawCircle
  Description : Sub for LCD_DrawCircle
  Input :  
    xc: 
    yc: 
    x: 
    y: 
    col: Line color
  Output : None
  Return : None
******************************************************************************/
void drawCircle(unsigned short xc, unsigned short yc, unsigned short x, unsigned short y, unsigned short col){ 
  LCD_SetPoint(xc+x, yc+y, col); 
  LCD_SetPoint(xc-x, yc+y, col); 
  LCD_SetPoint(xc+x, yc-y, col); 
  LCD_SetPoint(xc-x, yc-y, col); 
  LCD_SetPoint(xc+y, yc+x, col); 
  LCD_SetPoint(xc-y, yc+x, col); 
  LCD_SetPoint(xc+y, yc-x, col); 
  LCD_SetPoint(xc-y, yc-x, col);
}

/******************************************************************************* 
  Function Name : LCD_DrawCircle
  Description : Draw a circle
  Input : 
    xc: A point line coordinates center
    yc: A point column coordinates center
    r: radius of circle
    col: Line color
  Output : None
  Return : None
******************************************************************************/
void LCD_DrawCircle(unsigned short xc, unsigned short yc, unsigned short r, unsigned short col){ 
  int x = 0, y = r; 
  int p = 1 - r; 
  while (x < y) { 
    drawCircle(xc, yc, x, y, col); 
    x++; 
    if (p < 0) {
      p = p + 2 * x + 1;
    } else { 
      y--; 
      p = p + 2 * (x-y) + 1; 
    } 
    drawCircle(xc, yc, x, y, col); 
  }
}

/******************************************************************************* 
  Function Name : LCD_DrawCircleFill
  Description : Draw a circle filled
  Input : 
    xc: A point line coordinates center
    yc: A point column coordinates center
    r: radius of circle
    bcol: border color
    col: fill color
  Output : None
  Return : None
******************************************************************************/
void LCD_DrawCircleFill(unsigned short x, unsigned short y, unsigned short r, unsigned short bcol, unsigned short col) { 
  int xc, yc; 
  double testRadius; 
  double rsqMin = (double)(r-1)*(r-1); 
  double rsqMax = (double)r*r; 
  int fillFlag = 1; /* Ensure radius is positive */
  if (r < 0) { r = -r; } 
  for (yc = -r; yc < r; yc++) { 
    for (xc = -r; xc < r; xc++) { 
      testRadius = (double)(xc*xc + yc*yc); 
      if (((rsqMin < testRadius)&&(testRadius <= rsqMax)) || ((fillFlag)&&(testRadius <= rsqMax))) { 
        LCD_SetPoint(x + xc, y + yc, col); 
      } 
    } 
  } 
  if (col != bcol) LCD_DrawCircle(x, y, r, bcol);
}

/********************************************************************************
  Function Name : TP_Init
  Description : ADS7843 SPI Initialization
  Input : None
  Output : None
  Return : None
  Attention : None
*******************************************************************************/
void TP_Init(void){ 
  // Set polarity of CS1 
  bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS1, LOW); 
  // Set pin to be an input 
  bcm2835_gpio_fsel(IRQ, BCM2835_GPIO_FSEL_INPT); 
  // With a pullup 
  bcm2835_gpio_set_pud(IRQ, BCM2835_GPIO_PUD_UP); 
  // Disable ALL Detect Enables for pin 
  bcm2835_gpio_clr_ren(IRQ); 
  bcm2835_gpio_clr_fen(IRQ); 
  bcm2835_gpio_clr_hen(IRQ); 
  bcm2835_gpio_clr_len(IRQ); 
  bcm2835_gpio_clr_aren(IRQ); 
  bcm2835_gpio_clr_afen(IRQ); 
  bcm2835_gpio_afen(IRQ);
}

/********************************************************************************
  Function Name : LCD_DisplayOn
  Description : Switch display on via software
  Input : None
  Output : None
  Return : None
  Attention : None
*******************************************************************************/
void LCD_DisplayOn(void){ 
  LCD_WriteReg(0x07, 0x0173);
}

/********************************************************************************
  Function Name : LCD_DisplayOff
  Description : Switch display of via software
  Input : None
  Output : None
  Return : None
  Attention : None
*******************************************************************************/
void LCD_DisplayOff(void){ 
  LCD_WriteReg(0x07, 0x0000);
}

/******************************************************************************* 
  Function Name : Read_X
  Description : Read display X position of touch panel
  Input : None
  Output : None
  Return : None
  Attention : None
*******************************************************************************/
unsigned short Read_X(void){ 
  unsigned short x = 0; 
  char buf[3]; 
  bcm2835_spi_setClockDivider(DIVIDER_CS1); 
  bcm2835_spi_chipSelect(BCM2835_SPI_CS1); 
  buf[0] = CHX; 
  buf[1] = 0; 
  buf[2] = 0; 
  bcm2835_spi_transfern(buf, 3); 
  x = buf[1]; 
  x <<= 8; 
  x += buf[2]; 
  x >>= 4; 
  x &= 0x0fff; 
  bcm2835_spi_setClockDivider(DIVIDER_CS0); 
  bcm2835_spi_chipSelect(BCM2835_SPI_CS0); 
  return x;
}

/********************************************************************************
  Function Name : Read_Y
  Description : Read display Y position of touch panel
  Input : None
  Output : None
  Return : None
  Attention : None
*******************************************************************************/
unsigned short Read_Y(void){ 
  unsigned short y = 0; 
  char buf[3]; 
  bcm2835_spi_setClockDivider(DIVIDER_CS1); 
  bcm2835_spi_chipSelect(BCM2835_SPI_CS1); 
  buf[0] = CHY; 
  buf[1] = 0; 
  buf[2] = 0; 
  bcm2835_spi_transfern(buf, 3); 
  y = buf[1]; 
  y <<= 8; 
  y += buf[2]; 
  y >>= 4; 
  y &= 0x0fff; 
  bcm2835_spi_setClockDivider(DIVIDER_CS0); 
  bcm2835_spi_chipSelect(BCM2835_SPI_CS0); 
  return y;
}

/********************************************************************************
  Function Name : TP_GetAdXY
  Description : Read ADS7843 ADC value of X + Y + channel
  Input : None
  Output : None
  Return : return X + Y + channel ADC value
  Attention : None
*******************************************************************************/
void TP_GetAdXY(int *x,int *y){ 
  int adx,ady; 
  adx = Read_X(); 
  ady = Read_Y(); 
  *x = adx; *y = ady;
}

/********************************************************************************
  Function Name : TP_DrawPoint
  Description : Draw point Must have a LCD driver
  Input : 
    Xpos: Row Coordinate
    Ypos: Line Coordinate
  Output : None
  Return : None
  Attention : None
*******************************************************************************/
void TP_DrawPoint(unsigned short Xpos,unsigned short Ypos){ 
  LCD_SetPoint(Xpos,Ypos,0xf800); /* Center point */
  LCD_SetPoint(Xpos+1,Ypos,0xf800); 
  LCD_SetPoint(Xpos,Ypos+1,0xf800); 
  LCD_SetPoint(Xpos+1,Ypos+1,0xf800);
}

/********************************************************************************
  Function Name : DrawCross
  Description : specified coordinates painting crosshairs
  Input :
    Xpos: Row Coordinate
    Ypos: Line Coordinate 
  Output : None
  Return : None
  Attention : None
*******************************************************************************/
void DrawCross(unsigned short Xpos,unsigned short Ypos){ 
  LCD_DrawLine(Xpos-15,Ypos,Xpos-2,Ypos,0xffff); 
  LCD_DrawLine(Xpos+2,Ypos,Xpos+15,Ypos,0xffff); 
  LCD_DrawLine(Xpos,Ypos-15,Xpos,Ypos-2,0xffff); 
  LCD_DrawLine(Xpos,Ypos+2,Xpos,Ypos+15,0xffff); 
  //LCD_DrawLine(Xpos-15,Ypos+15,Xpos-7,Ypos+15,RGB565CONVERT(184,158,131)); 
  //LCD_DrawLine(Xpos-15,Ypos+7,Xpos-15,Ypos+15,RGB565CONVERT(184,158,131)); 
  //LCD_DrawLine(Xpos-15,Ypos-15,Xpos-7,Ypos-15,RGB565CONVERT(184,158,131)); 
  //LCD_DrawLine(Xpos-15,Ypos-7,Xpos-15,Ypos-15,RGB565CONVERT(184,158,131)); 
  //LCD_DrawLine(Xpos+7,Ypos+15,Xpos+15,Ypos+15,RGB565CONVERT(184,158,131)); 
  //LCD_DrawLine(Xpos+15,Ypos+7,Xpos+15,Ypos+15,RGB565CONVERT(184,158,131)); 
  //LCD_DrawLine(Xpos+7,Ypos-15,Xpos+15,Ypos-15,RGB565CONVERT(184,158,131)); 
  //LCD_DrawLine(Xpos+15,Ypos-15,Xpos+15,Ypos-7,RGB565CONVERT(184,158,131));
}

/********************************************************************************
  Function Name : Read_Ads7846
  Description : X Y obtained after filtering
  Input : None
  Output : None
  Return : Coordinate Structure address
  Attention : None
*******************************************************************************/
Coordinate *Read_Ads7846(void) { 
  static Coordinate screen; 
  int m0,m1,m2,TP_X[1],TP_Y[1],temp[3]; 
  unsigned char count = 0; 
  int buffer[2][9] = {{0},{0}}; /* Multiple sampling coordinates X and Y */
  /* Loop sampling 9 times */
  do { 
    if (! IRQ_Test()) { 
      TP_GetAdXY(TP_X,TP_Y);  
      buffer[0][count] = TP_X[0]; 
      buffer[1][count] = TP_Y[0]; 
      count++;  
    } 
  } 
  /* when user clicks on the touch screen, IRQ_Test() touchscreen interrupt pin will be set to low */
  while( ! IRQ_Test() && count < 9 );

  /* Successful sampling 9, filtering */
  if( count == 9 ) { 
    /* In order to reduce the amount of computation, were divided into three groups averaged */
    temp[0] = ( buffer[0][0] + buffer[0][1] + buffer[0][2] ) / 3; 
    temp[1] = ( buffer[0][3] + buffer[0][4] + buffer[0][5] ) / 3; 
    temp[2] = ( buffer[0][6] + buffer[0][7] + buffer[0][8] ) / 3; 
    /* Calculate the three groups of data */
    m0 = temp[0] - temp[1]; 
    m1 = temp[1] - temp[2]; 
    m2 = temp[2] - temp[0]; 
    /* Absolute value of the above difference */
    m0 = m0 > 0 ? m0 : (-m0); 
    m1 = m1 > 0 ? m1 : (-m1); 
    m2 = m2 > 0 ? m2 : (-m2); 
    /* 
      Judge whether the absolute difference exceeds the difference between 
      the threshold, If these three absolute difference exceeds the threshold, 
      The sampling point is judged as outliers, Discard sampling points 
    */
    if( m0 > THRESHOLD && m1 > THRESHOLD && m2 > THRESHOLD ){ return 0; } /* Calculating their average value */
    if( m0 < m1 ){  
      if( m2 < m0 ) { 
        screen.x = ( temp[0] + temp[2] ) / 2; 
      } else { 
        screen.x = ( temp[0] + temp[1] ) / 2; 
      } 
    } else if(m2<m1) { 
      screen.x = ( temp[0] + temp[2] ) / 2; 
    } else { 
      screen.x = ( temp[1] + temp[2] ) / 2; 
    } 
    /* calculate the average value of Y */
    temp[0] = ( buffer[1][0] + buffer[1][1] + buffer[1][2] ) / 3; 
    temp[1] = ( buffer[1][3] + buffer[1][4] + buffer[1][5] ) / 3; 
    temp[2] = ( buffer[1][6] + buffer[1][7] + buffer[1][8] ) / 3; 
    m0 = temp[0] - temp[1]; 
    m1 = temp[1] - temp[2]; 
    m2 = temp[2] - temp[0]; 
    m0 = m0 > 0 ? m0 : (-m0); 
    m1 = m1 > 0 ? m1 : (-m1); 
    m2 = m2 > 0 ? m2 : (-m2); 
    if( m0 > THRESHOLD && m1 > THRESHOLD && m2 > THRESHOLD ) { return 0; }  
    if( m0 < m1 ) { 
      if( m2 < m0 ) { 
        screen.y = ( temp[0] + temp[2] ) / 2; 
      } else { 
        screen.y = ( temp[0] + temp[1] ) / 2; 
      } 
    } else if( m2 < m1 ) {  
      screen.y = ( temp[0] + temp[2] ) / 2; 
    } else { 
      screen.y = ( temp[1] + temp[2] ) / 2; 
    } 
    //printf("x: %4u - y: %4u\n", screen.x, screen.y); 
    Screen.x = screen.x; 
    Screen.y = screen.y; 
    return &screen; 
  } 
  return 0;
}

/********************************************************************************
  Function Name : setCalibrationMatrix
  Description : Calculated K A B C D E F
  Input : None
  Output : None
  Return : return 1 success , return 0 fail
  Attention : None
*******************************************************************************/
FunctionalState setCalibrationMatrix(Coordinate * displayPtr, Coordinate * screenPtr, Matrix * matrixPtr){ 
  FunctionalState retTHRESHOLD = ENABLE; 
  matrixPtr->Divider = ((screenPtr[0].x - screenPtr[2].x) * (screenPtr[1].y - screenPtr[2].y)) - ((screenPtr[1].x - screenPtr[2].x) * (screenPtr[0].y - screenPtr[2].y)); 
  if(matrixPtr->Divider == 0) { 
    retTHRESHOLD = DISABLE; 
  } else { 
    matrixPtr->An = ((displayPtr[0].x - displayPtr[2].x) * (screenPtr[1].y - screenPtr[2].y)) - ((displayPtr[1].x - displayPtr[2].x) * (screenPtr[0].y - screenPtr[2].y)); 
    matrixPtr->Bn = ((screenPtr[0].x - screenPtr[2].x) * (displayPtr[1].x - displayPtr[2].x)) - ((displayPtr[0].x - displayPtr[2].x) * (screenPtr[1].x - screenPtr[2].x)); 
    matrixPtr->Cn = (screenPtr[2].x * displayPtr[1].x - screenPtr[1].x * displayPtr[2].x) * screenPtr[0].y + (screenPtr[0].x * displayPtr[2].x - screenPtr[2].x * displayPtr[0].x) * screenPtr[1].y + (screenPtr[1].x * displayPtr[0].x - screenPtr[0].x * displayPtr[1].x) * screenPtr[2].y; 
    matrixPtr->Dn = ((displayPtr[0].y - displayPtr[2].y) * (screenPtr[1].y - screenPtr[2].y)) - ((displayPtr[1].y - displayPtr[2].y) * (screenPtr[0].y - screenPtr[2].y)); 
    matrixPtr->En = ((screenPtr[0].x - screenPtr[2].x) * (displayPtr[1].y - displayPtr[2].y)) - ((displayPtr[0].y - displayPtr[2].y) * (screenPtr[1].x - screenPtr[2].x)); 
    matrixPtr->Fn = (screenPtr[2].x * displayPtr[1].y - screenPtr[1].x * displayPtr[2].y) * screenPtr[0].y + (screenPtr[0].x * displayPtr[2].y - screenPtr[2].x * displayPtr[0].y) * screenPtr[1].y + (screenPtr[1].x * displayPtr[0].y - screenPtr[0].x * displayPtr[1].y) * screenPtr[2].y; 
  } 
  return( retTHRESHOLD ) ;
}

/********************************************************************************
  Function Name : getDisplayPoint
  Description : channel XY via K A B C D E F value converted to the LCD screen coordinates
  Input : None
  Output : None
  Return : return 1 success , return 0 fail
  Attention : None
*******************************************************************************/
FunctionalState getDisplayPoint(Coordinate * displayPtr, Coordinate * screenPtr, Matrix * matrixPtr){ 
  FunctionalState retTHRESHOLD = ENABLE ; 
  long double an, bn, cn, dn, en, fn, sx, sy, md; 
  /* 
  an = matrixPtr->An; 
  bn = matrixPtr->Bn; 
  cn = matrixPtr->Cn; 
  dn = matrixPtr->Dn; 
  en = matrixPtr->En; 
  fn = matrixPtr->Fn; 
  sx = screenPtr->x; 
  sy = screenPtr->y; 
  md = matrixPtr->Divider;
  */
  an = matrix.An; 
  bn = matrix.Bn; 
  cn = matrix.Cn; 
  dn = matrix.Dn; 
  en = matrix.En; 
  fn = matrix.Fn; 
  sx = Screen.x; 
  sy = Screen.y; 
  md = matrix.Divider; 
  if( matrixPtr->Divider != 0 ) { 
    /* XD = AX+BY+C */
    display.x = ( (an * sx) + (bn * sy) + cn) / md; 
    /* YD = DX+EY+F */
    display.y = ( (dn * sx) + (en * sy) + fn) / md; 
  } else { 
    retTHRESHOLD = DISABLE; 
  } 
  return(retTHRESHOLD);
}

/********************************************************************************
  Function Name : TP_Cal
  Description : calibrate touch screen
  Input : None
  Output : None
  Return : None
  Attention : None
*******************************************************************************/
void TP_Cal(void){ 
  unsigned char i; 
  Coordinate * Ptr; 
  for(i=0;i<3;i++) { 
    // LCD_Clear(Black); 
    LCD_Text(10,10,"Touch crosshair to calibrate", White,Black); 
    DrawCross(DisplaySample[i].x,DisplaySample[i].y); 
    do { 
      Ptr = Read_Ads7846(); 
    } while( Ptr == (void*)0 );

    ScreenSample[i].x = Ptr->x; 
    ScreenSample[i].y = Ptr->y; 
    printf("cal: %u x: %4u y: %4u\n", i, ScreenSample[i].x, ScreenSample[i].y); 
  } 
  // get calibration parameters 
  setCalibrationMatrix(&DisplaySample[0], &ScreenSample[0], &matrix); 
  Screen.x = -1; 
  Screen.y = -1; 
  LCD_Clear(Black);
}

/********************************************************************************
            *********** END FILE***********
*********************************************************************************/