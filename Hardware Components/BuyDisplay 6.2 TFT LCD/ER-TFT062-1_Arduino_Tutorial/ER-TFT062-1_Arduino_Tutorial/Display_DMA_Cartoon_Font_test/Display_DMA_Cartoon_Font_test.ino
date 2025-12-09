/***************************************************
//Web: http://www.buydisplay.com
EastRising Technology Co.,LTD
Examples for ER-TFT062-1  Display test
Display is Hardward SPI 4-Wire SPI Interface and 5V Power Supply
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

 #define Picture_1_Addr	  0
 #define Cartoon_Addr     0x002A3000
 
#define FLASH_ADDR_16	  0x00458B98
#define FLASH_ADDR_24	  0x00499F98
#define FLASH_ADDR_32	  0x0052CC98

#define SIZE_16_NUM	267264
#define SIZE_24_NUM     601344
#define SIZE_32_NUM     1069056

#define MEMORY_ADDR_16   layer8_start_addr
#define MEMORY_ADDR_24	(MEMORY_ADDR_16+SIZE_16_NUM)
#define MEMORY_ADDR_32	(MEMORY_ADDR_24+SIZE_24_NUM)

unsigned char f []={0xD0,0xF1,0xC8,0xD5,0xB6,0xAB,0xB7,0xBD,0xBF,0xC6,0xBC,0xBC,0x00};  //GBK 
void setup() { 
   delay(100);
  ER5517.Parallel_Init();
  ER5517.HW_Reset();
  ER5517.System_Check_Temp();
  delay(100);
  while(ER5517.LCD_StatusRead()&0x02);
  ER5517.initial();
  GC9503_Initial(); 
  ER5517.Display_ON();

}



void LCD_BTE_Memory_Copy
(
 unsigned long S0_Addr     
,unsigned short S0_W       
,unsigned short XS0        
,unsigned short YS0      
,unsigned long S1_Addr    
,unsigned short S1_W       
,unsigned short XS1        
,unsigned short YS1        
,unsigned long Des_Addr    
,unsigned short Des_W     
,unsigned short XDes      
,unsigned short YDes    
,unsigned int ROP_Code    
/*ROP_Code :
   0000b		0(Blackness)
   0001b		~S0!E~S1 or ~(S0+S1)
   0010b		~S0!ES1
   0011b		~S0
   0100b		S0!E~S1
   0101b		~S1
   0110b		S0^S1
   0111b		~S0 + ~S1 or ~(S0 + S1)
   1000b		S0!ES1
   1001b		~(S0^S1)
   1010b		S1
   1011b		~S0+S1
   1100b		S0
   1101b		S0+~S1
   1110b		S0+S1
   1111b		1(whiteness)*/
,unsigned short X_W      
,unsigned short Y_H       
)
{
  ER5517.BTE_S0_Color_16bpp();
  ER5517.BTE_S0_Memory_Start_Address(S0_Addr);
  ER5517.BTE_S0_Image_Width(S0_W);
  ER5517.BTE_S0_Window_Start_XY(XS0,YS0);

  ER5517.BTE_S1_Color_16bpp();
  ER5517.BTE_S1_Memory_Start_Address(S1_Addr);
  ER5517.BTE_S1_Image_Width(S1_W); 
  ER5517.BTE_S1_Window_Start_XY(XS1,YS1);

  ER5517.BTE_Destination_Color_16bpp();
  ER5517.BTE_Destination_Memory_Start_Address(Des_Addr);
  ER5517.BTE_Destination_Image_Width(Des_W);
  ER5517.BTE_Destination_Window_Start_XY(XDes,YDes);	
   
  ER5517.BTE_ROP_Code(ROP_Code);	
  ER5517.BTE_Operation_Code(0x02); //BTE Operation: Memory copy (move) with ROP.
  ER5517.BTE_Window_Size(X_W,Y_H); 
  ER5517.BTE_Enable();
  ER5517.Check_BTE_Busy();
}


void LCD_Select_Outside_Font_Init
(
 unsigned char SCS           // 选择外挂的SPI   : SCS：0       SCS：1
,unsigned char Clk           // SPI时钟分频参数 : SPI Clock = System Clock /{(Clk+1)*2}
,unsigned long FlashAddr     // 源地址(Flash)
,unsigned long MemoryAddr    // 目的地址(SDRAM)
,unsigned long Num           // 字库的数据量大小
,unsigned char Size          // 设置字体大小  16：16*16     24:24*24    32:32*32
,unsigned char XxN           // 字体的宽度放大倍数：1~4
,unsigned char YxN           // 字体的高度放大倍数：1~4
,unsigned char ChromaKey     // 0：字体背景色透明    1：可以设置字体的背景色
,unsigned char Alignment     // 0：不字体不对齐      1：字体对齐
)
{
	if(Size==16)	ER5517.Font_Select_8x16_16x16();
	if(Size==24)	ER5517.Font_Select_12x24_24x24();
	if(Size==32)	ER5517.Font_Select_16x32_32x32();

	//(*)
	if(XxN==1)	ER5517.Font_Width_X1();
	if(XxN==2)	ER5517.Font_Width_X2();
	if(XxN==3)	ER5517.Font_Width_X3();
	if(XxN==4)	ER5517.Font_Width_X4();

	//(*)	
	if(YxN==1)	ER5517.Font_Height_X1();
	if(YxN==2)	ER5517.Font_Height_X2();
	if(YxN==3)	ER5517.Font_Height_X3();
	if(YxN==4)	ER5517.Font_Height_X4();

	//(*)
	if(ChromaKey==0)	ER5517.Font_Background_select_Color();	
	if(ChromaKey==1)	ER5517.Font_Background_select_Transparency();	

	//(*)
	if(Alignment==0)	ER5517.Disable_Font_Alignment();
	if(Alignment==1)	ER5517.Enable_Font_Alignment();	
	
	ER5517.DMA_24bit_Linear(SCS,Clk,FlashAddr,MemoryAddr,Num);
	ER5517.CGRAM_Start_address(MemoryAddr);        
}



void LCD_Print_Outside_Font_String
(
 unsigned short x               
,unsigned short y               
,unsigned long FontColor       
,unsigned long BackGroundColor 
,unsigned char *c               
)
{
	unsigned short temp_H = 0;
	unsigned short temp_L = 0;
	unsigned short temp = 0;
	unsigned int i = 0;
	
  ER5517.Text_Mode();
  ER5517.Font_Select_UserDefine_Mode();
  ER5517.Foreground_color_65k(FontColor);
  ER5517.Background_color_65k(BackGroundColor);
  ER5517.Goto_Text_XY(x,y);
	
	while(c[i] != '\0')
  { 
		if(c[i] < 0xa1)
		{
		  ER5517.CGROM_Select_Internal_CGROM();   // 内部CGROM为字符来源
		  ER5517.LCD_CmdWrite(0x04);
		  ER5517.LCD_DataWrite(c[i]);
		  ER5517.Check_Mem_WR_FIFO_not_Full();  
			i += 1;
		}
		else
		{
		ER5517.Font_Select_UserDefine_Mode();   // 自定义字库
		ER5517.LCD_CmdWrite(0x04);
			temp_H = ((c[i] - 0xa1) & 0x00ff) * 94;
			temp_L = c[i+1] - 0xa1;
			temp = temp_H + temp_L + 0x8000;
		ER5517.LCD_DataWrite((temp>>8)&0xff);
		ER5517.Check_Mem_WR_FIFO_not_Full();
		ER5517.LCD_DataWrite(temp&0xff);
		ER5517.Check_Mem_WR_FIFO_not_Full();
			i += 2;		
		}
	}
	
ER5517.Check_2D_Busy();

ER5517.Graphic_Mode(); //back to graphic mode;图形模式
}





void loop() {
  ER5517.Select_Main_Window_16bpp();
  ER5517.Main_Image_Start_Address(layer1_start_addr);        
  ER5517.Main_Image_Width(LCD_XSIZE_TFT);
  ER5517.Main_Window_Start_XY(0,0);
  ER5517.Canvas_Image_Start_address(0);
  ER5517.Canvas_image_width(LCD_XSIZE_TFT);
  ER5517.Active_Window_XY(0,0);
  ER5517.Active_Window_WH(LCD_XSIZE_TFT,LCD_YSIZE_TFT); 
  
  ER5517.DrawSquare_Fill(0+xoffset,0,LCD_XSIZE_TFT-xoffset,LCD_YSIZE_TFT,Red);
  delay(1000);
  ER5517.DrawSquare_Fill(0+xoffset,0,LCD_XSIZE_TFT-xoffset,LCD_YSIZE_TFT,Green);
  delay(1000);
  ER5517.DrawSquare_Fill(0+xoffset,0,LCD_XSIZE_TFT-xoffset,LCD_YSIZE_TFT,Blue);
  delay(1000);

  ER5517.DrawSquare_Fill(0+xoffset,0,LCD_XSIZE_TFT-xoffset,LCD_YSIZE_TFT,Cyan);
  delay(1000);
  ER5517.DrawSquare_Fill(0+xoffset,0,LCD_XSIZE_TFT-xoffset,LCD_YSIZE_TFT,Yellow);
  delay(1000); 
  ER5517.DrawSquare_Fill(0+xoffset,0,LCD_XSIZE_TFT-xoffset,LCD_YSIZE_TFT,Purple);
  delay(1000);   
 
  ER5517.DrawSquare_Fill(0+xoffset,0,LCD_XSIZE_TFT-xoffset,LCD_YSIZE_TFT,Black);
  delay(1000); 
  ER5517.DrawSquare_Fill(0+xoffset,0,LCD_XSIZE_TFT-xoffset,LCD_YSIZE_TFT,White);
  delay(1000);


////////BackLight Brightness control test  whit ER's PWM0
  unsigned char  brightness=10;
  ER5517.Foreground_color_65k(White);
  ER5517.Background_color_65k(Red);
  ER5517.CGROM_Select_Internal_CGROM();  
  ER5517.Font_Select_12x24_24x24();
  ER5517.Goto_Text_XY(0,10); 
  ER5517.Show_String( "BackLight Brightness control");
  while(brightness<=100)
 {
  ER5517.Select_PWM1();
  ER5517.Set_PWM_Prescaler_1_to_256(20);
  ER5517.Select_PWM1_Clock_Divided_By_1();
  ER5517.Set_Timer1_Count_Buffer(100); 
  ER5517.Set_Timer1_Compare_Buffer(brightness); 
  ER5517.Start_PWM1(); 
  delay(50);
  brightness+=10;
  } 
   delay(1000); 


   
  ////////Drawing
  unsigned int i;
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

  
///////////// External Chinese Font
  ER5517.Select_Main_Window_16bpp();
  ER5517.Main_Image_Start_Address(layer1_start_addr);        
  ER5517.Main_Image_Width(LCD_XSIZE_TFT);
  ER5517.Main_Window_Start_XY(0,0);
  ER5517.Canvas_Image_Start_address(0);
  ER5517.Canvas_image_width(LCD_XSIZE_TFT);
  ER5517.Active_Window_XY(0,0);
  ER5517.Active_Window_WH(LCD_XSIZE_TFT,LCD_YSIZE_TFT); 
  
  ER5517.DrawSquare_Fill(0,0,LCD_XSIZE_TFT,LCD_YSIZE_TFT,Black);
  LCD_Select_Outside_Font_Init(1,0,FLASH_ADDR_16,MEMORY_ADDR_16,SIZE_16_NUM,16,1,1,0,0);
  LCD_Print_Outside_Font_String(0+xoffset,50,Red,White, f );
  
  ER5517.Font_Width_X2();
  ER5517.Font_Height_X2();
  LCD_Print_Outside_Font_String(0+xoffset,75,LIGHTRED,Black,f);
  	
  ER5517.Font_Width_X3();
  ER5517.Font_Height_X3();
  LCD_Print_Outside_Font_String(0+xoffset,120,Blue,White,f);
  	
  ER5517.Font_Width_X4();
  ER5517.Font_Height_X4();
  LCD_Print_Outside_Font_String(0+xoffset,178,Purple,Black,f);
  delay(3000);
  
  LCD_Select_Outside_Font_Init(1,0,FLASH_ADDR_24,MEMORY_ADDR_24,SIZE_24_NUM,24,1,1,0,0);
  
  ER5517.DrawSquare_Fill(0,0,LCD_XSIZE_TFT,LCD_YSIZE_TFT,Blue);     
  LCD_Print_Outside_Font_String(0+xoffset,20,Red,White,f);
  	
  ER5517.Font_Width_X2();
  ER5517.Font_Height_X2();
  LCD_Print_Outside_Font_String(0+xoffset,55,Green,Black,f);
  
  ER5517.Font_Width_X3();
  ER5517.Font_Height_X3();
  LCD_Print_Outside_Font_String(0+xoffset,115,Cyan,White,f);
  	
  ER5517.Font_Width_X4();
  ER5517.Font_Height_X4();
  LCD_Print_Outside_Font_String(0+xoffset,195,Yellow,Black,f);
  delay(3000);
  
  LCD_Select_Outside_Font_Init(1,0,FLASH_ADDR_32,MEMORY_ADDR_32,SIZE_32_NUM,32,1,1,0,0);
  		
  ER5517.DrawSquare_Fill(0,0,LCD_XSIZE_TFT,LCD_YSIZE_TFT,White);
  LCD_Print_Outside_Font_String(0+xoffset,55,Red,White,f);
  	
  ER5517.Font_Width_X2();
  ER5517.Font_Height_X2();
  LCD_Print_Outside_Font_String(0+xoffset,115,Green,Black,f);
  
  ER5517.Font_Width_X3();
  ER5517.Font_Height_X3();
  LCD_Print_Outside_Font_String(0+xoffset,200,Blue,White,f);
  	
  ER5517.Font_Width_X4();
  ER5517.Font_Height_X4();
  LCD_Print_Outside_Font_String(0+xoffset,310,Yellow,Black,f);
  delay(3000);



/////////////BTE   
  unsigned int temp;
  unsigned long im=1;

  ER5517.Select_Main_Window_16bpp();
  ER5517.Main_Image_Start_Address(0);				
  ER5517.Main_Image_Width(LCD_XSIZE_TFT);							
  ER5517.Main_Window_Start_XY(0,0);

  ER5517.Canvas_Image_Start_address(0);//Layer 1
  ER5517.Canvas_image_width(LCD_XSIZE_TFT);//
  ER5517.Active_Window_XY(0,0);
  ER5517.Active_Window_WH(LCD_XSIZE_TFT,LCD_YSIZE_TFT);

  ER5517.Foreground_color_65k(Black);
  ER5517.Line_Start_XY(0,0);
  ER5517.Line_End_XY(LCD_XSIZE_TFT-1,LCD_YSIZE_TFT-25);
  ER5517.Start_Square_Fill();

  ER5517.Foreground_color_65k(Blue);
  ER5517.Line_Start_XY(0,LCD_YSIZE_TFT-24);
  ER5517.Line_End_XY(LCD_XSIZE_TFT-1,LCD_YSIZE_TFT-1);
  ER5517.Start_Square_Fill();
  ER5517.Foreground_color_65k(White);
  ER5517.Background_color_65k(Blue);
  ER5517.CGROM_Select_Internal_CGROM();
  ER5517.Font_Width_X1(); 
  ER5517.Font_Height_X1();
  ER5517.Font_Select_12x24_24x24();
  ER5517.Goto_Text_XY(0+xoffset,LCD_YSIZE_TFT-24);
  ER5517.Show_String("  Demo BTE Compare");
  ER5517.Foreground_color_65k(Black);
  ER5517.Background_color_65k(White);
  ER5517.Font_Select_8x16_16x16();
  ER5517.Goto_Text_XY(0+xoffset,LCD_YSIZE_TFT-48);
  ER5517.Show_String("Execute Logic 'OR' 0xf000");
/*
  ER5517.Active_Window_XY(20+xoffset,40);
  ER5517.Active_Window_WH(80,80);
  ER5517.Goto_Pixel_XY(20+xoffset,40);
  ER5517. Show_picture(80*80,pic_80x80); 
  ER5517.Active_Window_XY(20+80+20+xoffset,40); 
  ER5517.Active_Window_WH(80,80);
  ER5517.Goto_Pixel_XY(120+xoffset,40);
  ER5517.Show_picture(80*80,pic_80x80);
  ER5517.Active_Window_XY(20+80+20+80+20+xoffset,40);
  ER5517.Active_Window_WH(80,80);
  ER5517.Goto_Pixel_XY(220+xoffset,40);
  ER5517.Show_picture(80*80,pic_80x80);	*/	 
  ER5517.Active_Window_XY(0,0);
  ER5517.Active_Window_WH(LCD_XSIZE_TFT,LCD_YSIZE_TFT);
  ER5517.Foreground_color_65k(Black);
  ER5517.Background_color_65k(White);
  ER5517.CGROM_Select_Internal_CGROM();
  ER5517.Font_Select_8x16_16x16();
  ER5517.Goto_Text_XY(20+xoffset,130 );
  ER5517.Show_String("Without BTE");
  ER5517.Goto_Text_XY(120+xoffset,130 );
  ER5517.Show_String("BTE Write");
  ER5517.Goto_Text_XY(120+xoffset,150 );
  ER5517.Show_String("ROP");
  ER5517.Goto_Text_XY(220+xoffset,130 );
  ER5517.Show_String("BTE Move");
  ER5517.Goto_Text_XY(220+xoffset,150 );
  ER5517.Show_String("ROP");
  delay(1000);
  ER5517.Active_Window_XY(20+xoffset,40);
  ER5517.Active_Window_WH(80,80); 
  ER5517. Goto_Pixel_XY(20+xoffset,40);
  ER5517.LCD_CmdWrite(0x04);
  temp =   ER5517.LCD_DataRead();
  ER5517.Check_Mem_RD_FIFO_not_Empty();  //dummy
  for(i=0; i<80*80;i++)
  {				
  temp =   ER5517.LCD_DataRead();		   
  temp=temp|(  ER5517.LCD_DataRead()<<8);
  ER5517.Check_Mem_RD_FIFO_not_Empty();
  temp |= 0xf000; 
  ER5517.LCD_DataWrite(temp);
  ER5517.LCD_DataWrite(temp>>8);
  ER5517.Check_Mem_WR_FIFO_not_Full();
  }
  ER5517.Active_Window_XY(0,0);
  ER5517.Active_Window_WH(LCD_XSIZE_TFT,LCD_YSIZE_TFT);
  delay(1000);
   //second block, MCU write with BTE ROP 
  ER5517.BTE_S0_Color_16bpp();
  ER5517.BTE_S1_Color_16bpp();
  ER5517.BTE_S1_Memory_Start_Address(0);
  ER5517.BTE_S1_Image_Width(LCD_XSIZE_TFT);
  ER5517.BTE_S1_Window_Start_XY(120+xoffset,40);

  ER5517.BTE_Destination_Color_16bpp();  
  ER5517.BTE_Destination_Memory_Start_Address(0);
  ER5517.BTE_Destination_Image_Width(LCD_XSIZE_TFT);
  ER5517.BTE_Destination_Window_Start_XY(120+xoffset,40);  
  ER5517.BTE_Window_Size(80,80);

  ER5517.BTE_ROP_Code(14);
  ER5517.BTE_Operation_Code(0); //BTE write
  ER5517.BTE_Enable();

  ER5517.LCD_CmdWrite(0x04);
  for(i=0; i<80*80;i++)
  {				
  ER5517.LCD_DataWrite(0xf000);
  ER5517.LCD_DataWrite(0xf000>>8);
  ER5517.Check_Mem_WR_FIFO_not_Full();
  }
  ER5517.Check_Mem_WR_FIFO_Empty();//糶Ч浪琩
  ER5517.Check_BTE_Busy();

  delay(1000);   
	  //third block, BTE MOVE with ROP
  ER5517.Canvas_Image_Start_address(layer2_start_addr);//
  ER5517.Canvas_image_width(LCD_XSIZE_TFT);//
  ER5517.Active_Window_XY(0,0);
  ER5517.Active_Window_WH(LCD_XSIZE_TFT,LCD_YSIZE_TFT);

 
  ER5517.Foreground_color_65k(0xf000);
  ER5517.Background_color_65k(Black);
  ER5517.Line_Start_XY(0+xoffset,40);
  ER5517.Line_End_XY(80+xoffset,120);
  ER5517.Start_Square_Fill();  
   
   
  ER5517.BTE_S0_Color_16bpp();
  ER5517.BTE_S0_Memory_Start_Address(layer2_start_addr);
  ER5517.BTE_S0_Image_Width(LCD_XSIZE_TFT);
  ER5517.BTE_S0_Window_Start_XY(0+xoffset,40);

  ER5517.BTE_S1_Color_16bpp();
  ER5517.BTE_S1_Memory_Start_Address(layer1_start_addr);
  ER5517.BTE_S1_Image_Width(LCD_XSIZE_TFT);
  ER5517.BTE_S1_Window_Start_XY(220+xoffset,40);

  ER5517.BTE_Destination_Color_16bpp();  
  ER5517.BTE_Destination_Memory_Start_Address(layer1_start_addr);
  ER5517.BTE_Destination_Image_Width(LCD_XSIZE_TFT);
  ER5517.BTE_Destination_Window_Start_XY(220+xoffset,40);  
  ER5517.BTE_Window_Size(80,80);

  ER5517.BTE_ROP_Code(14);
  ER5517.BTE_Operation_Code(2); //BTE write
  ER5517.BTE_Enable();
  ER5517.Check_BTE_Busy();
  delay(1000);  
  ER5517.BTE_Disable();

  
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
 
 
 
 ///////////////////////////Cartoon_Show
  	     										
 	ER5517.Select_Main_Window_16bpp();
	ER5517.Main_Image_Start_Address(layer2_start_addr);		  
	ER5517.Main_Image_Width(LCD_XSIZE_TFT);
	ER5517.Main_Window_Start_XY(0,0);				                   
	ER5517.Canvas_Image_Start_address(layer2_start_addr);	           
	ER5517.Canvas_image_width(LCD_XSIZE_TFT);				           
	ER5517.Active_Window_XY(0,0);
	ER5517.Active_Window_WH(LCD_XSIZE_TFT,LCD_YSIZE_TFT);	          
	ER5517.DrawSquare_Fill(0,0,LCD_XSIZE_TFT,LCD_YSIZE_TFT,Blue);
        ER5517.DMA_24bit_Block(1,0,0+xoffset,0,360,LCD_YSIZE_TFT,360,Picture_1_Addr);
       
	ER5517.Canvas_image_width(223);
	ER5517.Canvas_Image_Start_address(layer3_start_addr);
	ER5517.DMA_24bit_Block(1,0,0,0,223,134*30,223,Cartoon_Addr);
       												
	while(1)
	{
		for(i = 0 ; i < 30 ; i++)
		{
			LCD_BTE_Memory_Copy(layer3_start_addr,223,0,134*i,
			layer3_start_addr,223,0,134*i,
			layer2_start_addr,LCD_XSIZE_TFT,10+xoffset,53,0X0C,220,134);
				
			delay(40);	
		}
	}
 
    
}
