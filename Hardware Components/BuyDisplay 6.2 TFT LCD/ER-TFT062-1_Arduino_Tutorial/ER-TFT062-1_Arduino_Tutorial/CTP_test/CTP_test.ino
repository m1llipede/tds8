/***************************************************
//Web: http://www.buydisplay.com
EastRising Technology Co.,LTD
Examples for  ER-TFT062-1 Capacitive touch screen test
Display is Hardward SPI 4-Wire SPI Interface and 5V Power Supply,CTP is I2C interface.
Tested and worked with:
Works with Arduino 1.6.0 IDE  
Test ok: Arduino UNO,Arduino MEGA2560 ,Arduino DUE
If using the DUE board, please disconnect the metal pins of IO0 and IO1, 
and connect the adapter board's IO0 to IO11, IO1 to IO13,and modify the IO definition in the LCD_init.H file
****************************************************/
#include <SPI.h>
#include <Wire.h>
#include "LCD.h"
#include "LCD_init.h"
#include <stdint.h>
#include "Arduino.h"
#include "Print.h"

uint8_t addr  = 0x5d;  //CTP IIC ADDRESS

#define GT911_RST 6
#define GT911_INT   7  


struct TouchLocation
{
  uint16_t x;
  uint16_t y;
};
TouchLocation touchLocations[5];


void inttostr(uint16_t value,uint8_t *str);

void writeGT911TouchRegister( uint16_t regAddr,uint8_t *val, uint16_t cnt);
uint8_t readGT911TouchAddr( uint16_t regAddr, uint8_t * pBuf, uint8_t len );
uint8_t readGT911TouchLocation( TouchLocation * pLoc, uint8_t num );
uint32_t dist(const TouchLocation & loc);
uint32_t dist(const TouchLocation & loc1, const TouchLocation & loc2);
bool sameLoc( const TouchLocation & loc, const TouchLocation & loc2 );

uint8_t buf[30];




void writeGT911TouchRegister( uint16_t regAddr,uint8_t *val, uint16_t cnt)
{	uint16_t i=0;
  Wire.beginTransmission(addr);
   Wire.write( regAddr>>8 );  // register 0
  Wire.write( regAddr);  // register 0 
	for(i=0;i<cnt;i++,val++)//data
	{		
          Wire.write( *val );  // value
	}
  uint8_t retVal = Wire.endTransmission(); 
}



uint8_t readGT911TouchAddr( uint16_t regAddr, uint8_t * pBuf, uint8_t len )
{
  Wire.beginTransmission(addr);
  Wire.write( regAddr>>8 );  // register 0
  Wire.write( regAddr);  // register 0  
  uint8_t retVal = Wire.endTransmission();
  
  uint8_t returned = Wire.requestFrom(addr, len);    // request 1 bytes from slave device #2
  
  uint8_t i;
  for (i = 0; (i < len) && Wire.available(); i++)
  
  {
    pBuf[i] = Wire.read();
  }
  
  return i;
}

uint8_t readGT911TouchLocation( TouchLocation * pLoc, uint8_t num )
{
  uint8_t retVal;
  uint8_t i;
  uint8_t k;
  uint8_t  ss[1];
  do
  {  
    
    if (!pLoc) break; // must have a buffer
    if (!num)  break; // must be able to take at least one
     ss[0]=0;
      readGT911TouchAddr( 0x814e, ss, 1);
      uint8_t status=ss[0];

    if ((status & 0x0f) == 0) break; // no points detected
    uint8_t hitPoints = status & 0x0f;
    
    Serial.print("number of hit points = ");
    Serial.println( hitPoints );
    
     uint8_t tbuf[32]; uint8_t tbuf1[32];uint8_t tbuf2[16];  
    readGT911TouchAddr( 0x8150, tbuf, 32);
    readGT911TouchAddr( 0x8150+32, tbuf1, 32);
    
      if(hitPoints<=4)
            {   
              for (k=0,i = 0; (i <  4*8)&&(k < num); k++, i += 8)
              {
                pLoc[k].x = tbuf[i+1] << 8 | tbuf[i+0];
                pLoc[k].y = tbuf[i+3] << 8 | tbuf[i+2];
              }   
            }
        if(hitPoints>4)   
            {  
               for (k=0,i = 0; (i <  4*8)&&(k < num); k++, i += 8)
              {
                pLoc[k].x = tbuf[i+1] << 8 | tbuf[i+0];
                pLoc[k].y = tbuf[i+3] << 8 | tbuf[i+2];
              }               
              
              for (k=4,i = 0; (i <  4*8)&&(k < num); k++, i += 8)
              {
                pLoc[k].x = tbuf1[i+1] << 8 | tbuf1[i+0];
                pLoc[k].y = tbuf1[i+3] << 8 | tbuf1[i+2];
              }   
            } 
            
                
            
    
    retVal = hitPoints;
    
  } while (0);
  
    ss[0]=0;
    writeGT911TouchRegister( 0x814e,ss,1); 
  
  return retVal;
}

void inttostr(uint16_t value,uint8_t *str)
{
	str[0]=value/1000+0x30;
	str[1]=value%1000/100+0x30;
	str[2]=value%1000%100/10+0x30;
	str[3]=value%1000%100%10+0x30;

}



void setup() {uint8_t ss[4];
//  Serial.begin(9600);
 Wire.setClock(100000);
  Wire.begin();        // join i2c bus (address optional for master)   


   delay(100);
  ER5517.Parallel_Init();
  ER5517.HW_Reset();
  ER5517.System_Check_Temp();
  delay(100);
  while(ER5517.LCD_StatusRead()&0x02);
  ER5517.initial();
  GC9503_Initial(); 
  ER5517.Display_ON();

  ER5517.Select_Main_Window_16bpp();
  ER5517.Main_Image_Start_Address(layer1_start_addr);        
  ER5517.Main_Image_Width(LCD_XSIZE_TFT);
  ER5517.Main_Window_Start_XY(0,0);
  ER5517.Canvas_Image_Start_address(0);
  ER5517.Canvas_image_width(LCD_XSIZE_TFT);
  ER5517.Active_Window_XY(0,0);
  ER5517.Active_Window_WH(LCD_XSIZE_TFT,LCD_YSIZE_TFT); 
  
  ER5517.Foreground_color_65k(Black);
  ER5517.Line_Start_XY(0,0);
  ER5517.Line_End_XY(LCD_XSIZE_TFT-1,LCD_YSIZE_TFT-1);
  ER5517.Start_Square_Fill(); 
  
  ER5517.Foreground_color_65k(Red);
  ER5517.Line_Start_XY(0+xoffset,0);
  ER5517.Line_End_XY(LCD_XSIZE_TFT-1-xoffset,LCD_YSIZE_TFT-1);
  ER5517.Start_Square();
  
   ER5517.Foreground_color_65k(White);
  ER5517.Background_color_65k(Black);
  ER5517.CGROM_Select_Internal_CGROM();  
  ER5517.Font_Select_8x16_16x16();
  ER5517.Goto_Text_XY(40+xoffset,200);
  ER5517.Show_String( "www.buydisplay.com");   
  ER5517.Goto_Text_XY(40+xoffset,240); 
  ER5517.Show_String( "Capacitive touch screen tese"); 
 		ER5517.Goto_Text_XY(LCD_XSIZE_TFT-48-xoffset,LCD_YSIZE_TFT-20);
		ER5517.Show_String("Clear");   
		ER5517.Goto_Text_XY(15+xoffset,LCD_YSIZE_TFT-20);
		ER5517.Show_String("Exit");   

 
    pinMode(GT911_RST, OUTPUT); 
    pinMode     (GT911_INT, OUTPUT);
    digitalWrite(GT911_RST, LOW);
    digitalWrite(GT911_INT, LOW);
    delay(20);
     digitalWrite(GT911_RST, HIGH);
     delay(50);  
    pinMode     (GT911_INT, INPUT);
     delay(100);    

}
  uint8_t flag = 1;
void loop() {
   static uint16_t w = LCD_XSIZE_TFT;
  static uint16_t h = LCD_YSIZE_TFT; 
   unsigned int i;
   double float_data;  

   while(flag) 
  {    pinMode     (GT911_INT, INPUT);
       uint8_t st=digitalRead(GT911_INT);       
      if(!st)    //Hardware touch interrupt
    {  
      Serial.println("Touch: ");
      
      uint8_t count = readGT911TouchLocation( touchLocations, 5);
                
        for (i = 0; i < count; i++)
        {
            
   	   if ((touchLocations[0].x<85) &&(touchLocations[0].y>920)) flag=0;     
            if (((touchLocations[0].x)>250) &&((touchLocations[0].y)>920))
              {   ER5517.Foreground_color_65k(Black);
                  ER5517.Line_Start_XY(0,0);
                  ER5517.Line_End_XY(LCD_XSIZE_TFT-1,LCD_YSIZE_TFT-1);
                  ER5517.Start_Square_Fill(); 
                  
                  ER5517.Foreground_color_65k(Red);
                  ER5517.Line_Start_XY(0+xoffset,0);
                  ER5517.Line_End_XY(LCD_XSIZE_TFT-1-xoffset,LCD_YSIZE_TFT-1);
                  ER5517.Start_Square();
                  
                   ER5517.Foreground_color_65k(White);
                  ER5517.Background_color_65k(Black);
                  ER5517.CGROM_Select_Internal_CGROM();  
                  ER5517.Font_Select_8x16_16x16();
                  ER5517.Goto_Text_XY(40+xoffset,200);
                  ER5517.Show_String( "www.buydisplay.com");   
                  ER5517.Goto_Text_XY(40+xoffset,240); 
                  ER5517.Show_String( "Capacitive touch screen tese"); 
 		ER5517.Goto_Text_XY(LCD_XSIZE_TFT-48-xoffset,LCD_YSIZE_TFT-20);
		ER5517.Show_String("Clear");   
		ER5517.Goto_Text_XY(15+xoffset,LCD_YSIZE_TFT-20);
		ER5517.Show_String("Exit");   
   
              }  
              
          else{                                       
              snprintf((char*)buf,sizeof(buf),"(%3d,%3d)",touchLocations[i].x,touchLocations[i].y); 
            const  char *str=(const char *)buf;
             ER5517.Foreground_color_65k(Red);  
            ER5517.Text_Mode();
            ER5517.Goto_Text_XY(50+xoffset,80+16*i);
            ER5517.LCD_CmdWrite(0x04);
            while(*str != '\0')
            {
            ER5517.LCD_DataWrite(*str);
            ER5517.Check_Mem_WR_FIFO_not_Full();      
            ++str; 
            } 
            ER5517.Check_2D_Busy();
            ER5517.Graphic_Mode(); //back to graphic mode;
                   
           
         if(i==0)  ER5517.DrawCircle_Fill(touchLocations[i].x+xoffset,touchLocations[i].y, 3, Red);  
        else if(i==1)  ER5517.DrawCircle_Fill(touchLocations[i].x+xoffset,touchLocations[i].y, 3, Green); 
        else if(i==2)  ER5517.DrawCircle_Fill(touchLocations[i].x+xoffset,touchLocations[i].y, 3, Blue);        
        else if(i==3)  ER5517.DrawCircle_Fill(touchLocations[i].x+xoffset,touchLocations[i].y, 3, Cyan); 
        else if(i==4)  ER5517.DrawCircle_Fill(touchLocations[i].x+xoffset,touchLocations[i].y, 3, Yellow);   
          }
        }
     } 
    
  }
 
  ////////Drawing
 
  ER5517.Select_Main_Window_16bpp();
  ER5517.Main_Image_Start_Address(layer1_start_addr);				
  ER5517.Main_Image_Width(LCD_XSIZE_TFT);
  ER5517.Main_Window_Start_XY(0,0);
  ER5517.Canvas_Image_Start_address(layer1_start_addr);
  ER5517.Canvas_image_width(LCD_XSIZE_TFT);
  ER5517.Active_Window_XY(0,0);
  ER5517.Active_Window_WH(LCD_XSIZE_TFT,LCD_YSIZE_TFT);	

  ER5517.Foreground_color_65k(Black);
  ER5517.Line_Start_XY(0,0);
  ER5517.Line_End_XY(LCD_XSIZE_TFT-1,LCD_YSIZE_TFT-1);
  ER5517.Start_Square_Fill();

  for(i=0;i<=360/2-10;i+=8)
  {ER5517.Foreground_color_65k(Red);
  ER5517.Line_Start_XY(0+i+xoffset,0+i);
  ER5517.Line_End_XY(LCD_XSIZE_TFT-1-i-xoffset,LCD_YSIZE_TFT-1-i);
  ER5517.Start_Square();
  delay(10);
  }

  for(i=0;i<=360/2-10;i+=8)
  {ER5517.Foreground_color_65k(Black);
  ER5517.Line_Start_XY(0+i+xoffset,0+i);
  ER5517.Line_End_XY(LCD_XSIZE_TFT-1-i-xoffset,LCD_YSIZE_TFT-1-i);
  ER5517.Start_Square();
  delay(10);
  }
 delay(100);
///////////////////////////Square Of Circle
  ER5517.Foreground_color_65k(Black);
  ER5517.Line_Start_XY(0,0);
  ER5517.Line_End_XY(LCD_XSIZE_TFT-1-xoffset,LCD_YSIZE_TFT-1);
  ER5517.Start_Square_Fill();

  for(i=0;i<=360/2-10;i+=8)
  {ER5517.Foreground_color_65k(Green);
  ER5517.Line_Start_XY(0+i+xoffset,0+i);
  ER5517.Line_End_XY(LCD_XSIZE_TFT-1-i-xoffset,LCD_YSIZE_TFT-1-i);
  ER5517.Circle_Square_Radius_RxRy(10,10);
  ER5517.Start_Circle_Square();
  delay(10);
  }

  for(i=0;i<=360/2-10;i+=8)
  {ER5517.Foreground_color_65k(Black);
  ER5517.Line_Start_XY(0+i+xoffset,0+i);
  ER5517.Line_End_XY(LCD_XSIZE_TFT-1-i-xoffset,LCD_YSIZE_TFT-1-i);
  ER5517.Circle_Square_Radius_RxRy(10,10);
  ER5517.Start_Circle_Square();
  delay(10);
  }
  delay(100);

///////////////////////////Circle
  ER5517.Foreground_color_65k(Black);
  ER5517.Line_Start_XY(0,0);
  ER5517.Line_End_XY(LCD_XSIZE_TFT-1-xoffset,LCD_YSIZE_TFT-1);
  ER5517.Start_Square_Fill();

  for(i=0;i<=360/2-10;i+=8)
  {ER5517.Foreground_color_65k(Blue);
  ER5517.Circle_Center_XY(LCD_XSIZE_TFT/2,LCD_YSIZE_TFT/2);
  ER5517.Circle_Radius_R(i);
  ER5517.Start_Circle_or_Ellipse();
  delay(10);
  }

  for(i=0;i<=360/2-10;i+=8)
  {ER5517.Foreground_color_65k(Black);
  ER5517.Circle_Center_XY(LCD_XSIZE_TFT/2,LCD_YSIZE_TFT/2);
  ER5517.Circle_Radius_R(i);
  ER5517.Start_Circle_or_Ellipse();
  delay(10);
  }
  delay(100);

///////////////////////////Ellipse
  ER5517.Foreground_color_65k(Black);
  ER5517.Line_Start_XY(0,0);
  ER5517.Line_End_XY(LCD_XSIZE_TFT-1-xoffset,LCD_YSIZE_TFT-1);
  ER5517.Start_Square_Fill();

  for(i=0;i<=360/2-100;i+=8)
  {ER5517.Foreground_color_65k(White);
  ER5517.Circle_Center_XY(LCD_XSIZE_TFT/2,LCD_YSIZE_TFT/2);
  ER5517.Ellipse_Radius_RxRy(i,i+50);
  ER5517.Start_Circle_or_Ellipse();
  delay(10);
  }

  for(i=0;i<=360/2-100;i+=8)
  {ER5517.Foreground_color_65k(Black);
  ER5517.Circle_Center_XY(LCD_XSIZE_TFT/2,LCD_YSIZE_TFT/2);
  ER5517.Ellipse_Radius_RxRy(i,i+50);
  ER5517.Start_Circle_or_Ellipse();
  delay(10);
  }
  delay(100);

 ////////////////////////////Triangle
  ER5517.Foreground_color_65k(Black);
  ER5517.Line_Start_XY(0,0);
  ER5517.Line_End_XY(LCD_XSIZE_TFT-1-xoffset,LCD_YSIZE_TFT-1);
  ER5517.Start_Square_Fill();

  for(i=0;i<=360/2-10;i+=8)
  {ER5517.Foreground_color_65k(Yellow);
  ER5517.Triangle_Point1_XY(LCD_XSIZE_TFT/2,i);
  ER5517.Triangle_Point2_XY(i+xoffset,LCD_YSIZE_TFT-1-i);
  ER5517.Triangle_Point3_XY(LCD_XSIZE_TFT-1-i-xoffset,LCD_YSIZE_TFT-1-i);
  ER5517.Start_Triangle();
  delay(10);
  }

  for(i=0;i<=360/2-10;i+=8)
  {ER5517.Foreground_color_65k(Black);
  ER5517.Triangle_Point1_XY(LCD_XSIZE_TFT/2,i);
  ER5517.Triangle_Point2_XY(i+xoffset,LCD_YSIZE_TFT-1-i);
  ER5517.Triangle_Point3_XY(LCD_XSIZE_TFT-1-i-xoffset,LCD_YSIZE_TFT-1-i);
  ER5517.Start_Triangle();
  delay(10);
  }
  delay(100);


 ////////////////////////////line
  ER5517.Foreground_color_65k(Black);
  ER5517.Line_Start_XY(0,0);
  ER5517.Line_End_XY(LCD_XSIZE_TFT-1-xoffset,LCD_YSIZE_TFT-1);
  ER5517.Start_Square_Fill();

  for(i=0;i<=360;i+=8)
  {ER5517.Foreground_color_65k(Red);
  ER5517.Line_Start_XY(i+xoffset,0);
  ER5517.Line_End_XY(LCD_XSIZE_TFT-1-i-xoffset,LCD_YSIZE_TFT-1);
  ER5517.Start_Line();
  delay(10);
  }
  for(i=0;i<=LCD_YSIZE_TFT;i+=8)
  {ER5517.Foreground_color_65k(Red);
  ER5517.Line_Start_XY(0+xoffset,LCD_YSIZE_TFT-1-i);
  ER5517.Line_End_XY(LCD_XSIZE_TFT-1-xoffset,i);
  ER5517.Start_Line();
  delay(10);
  }


  for(i=0;i<=360;i+=8)
  {ER5517.Foreground_color_65k(Black);
  ER5517.Line_Start_XY(i+xoffset,0);
  ER5517.Line_End_XY(LCD_XSIZE_TFT-1-i-xoffset,LCD_YSIZE_TFT-1);
  ER5517.Start_Line();
  delay(10);
  }
  for(i=0;i<=LCD_YSIZE_TFT;i+=8)
  {ER5517.Foreground_color_65k(Black);
  ER5517.Line_Start_XY(0+xoffset,LCD_YSIZE_TFT-1-i);
  ER5517.Line_End_XY(LCD_XSIZE_TFT-1-xoffset,i);
  ER5517.Start_Line();
  delay(10);
  }


  delay(100);  

/////////////Internal characters 
  ER5517.Select_Main_Window_16bpp();
  ER5517.Main_Image_Start_Address(layer1_start_addr);				
  ER5517.Main_Image_Width(LCD_XSIZE_TFT);
  ER5517.Main_Window_Start_XY(0,0);
  ER5517.Canvas_Image_Start_address(layer1_start_addr);
  ER5517.Canvas_image_width(LCD_XSIZE_TFT);
  ER5517.Active_Window_XY(0+xoffset,0);
  ER5517.Active_Window_WH(LCD_XSIZE_TFT-xoffset,LCD_YSIZE_TFT);	
 
  
  ER5517.Foreground_color_65k(Black);
  ER5517.Line_Start_XY(0,0);
  ER5517.Line_End_XY(LCD_XSIZE_TFT-1,LCD_YSIZE_TFT-1);
  ER5517.Start_Square_Fill();



  ER5517.Background_color_65k(Black);
  ER5517.Foreground_color_65k(Red);  
  ER5517.CGROM_Select_Internal_CGROM();
  ER5517.Font_Select_8x16_16x16();
  ER5517.Goto_Text_XY(0+xoffset,10);
  ER5517.Show_String("buydisplay.com");
  
  ER5517.Foreground_color_65k(Green);
  ER5517.Font_Select_12x24_24x24();
  ER5517.Goto_Text_XY(0+xoffset,26);
  ER5517.Show_String("buydisplay.com");

  ER5517.Foreground_color_65k(Blue);
  ER5517.Font_Select_16x32_32x32();
  ER5517.Goto_Text_XY(0+xoffset,50);
  ER5517.Show_String("buydisplay.com");  
  
  ER5517.Foreground_color_65k(Yellow);
  ER5517.Font_Width_X4(); 
  ER5517.Font_Height_X4();
  ER5517.Goto_Text_XY(0+xoffset,90);  
  ER5517.Show_String("buydisplay.com");    
  delay(2000); 

   ER5517.Font_Width_X1(); 
  ER5517.Font_Height_X1();

 ///////////////////////// DMA
  ER5517.Select_Main_Window_16bpp();
  ER5517.Main_Image_Start_Address(layer1_start_addr);				
  ER5517.Main_Image_Width(LCD_XSIZE_TFT);
  ER5517.Main_Window_Start_XY(0,0);
  ER5517.Canvas_Image_Start_address(layer1_start_addr);
  ER5517.Canvas_image_width(LCD_XSIZE_TFT);
  ER5517.Active_Window_XY(0,0);
  ER5517.Active_Window_WH(LCD_XSIZE_TFT,LCD_YSIZE_TFT);	
 
  
  ER5517.Foreground_color_65k(Black);
  ER5517.Line_Start_XY(0+xoffset,0);
  ER5517.Line_End_XY(LCD_XSIZE_TFT-1-xoffset,LCD_YSIZE_TFT);
  ER5517.Start_Square_Fill();
 
 
        unsigned long im;
 	 for(im=0;im<4;im++)
	 {
        ER5517.DMA_24bit_Block(1,0,0+xoffset,0,360,LCD_YSIZE_TFT,360,im*360*LCD_YSIZE_TFT*2);
        // Select SPI : SCS��0       SCS��1
        // SPI Clock = System Clock /{(Clk+1)*2}
        // Transfer to SDRAM address:X1
         // Transfer to SDRAM address:Y1
        // DMA data width
        // DMA data height
        // Picture's width
        // Flash address	
	  delay(2000);
 	
	 }     
}



