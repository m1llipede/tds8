#ifndef LCD_init_h
#define LCD_init_h
#include "Arduino.h"
//#include <avr/pgmspace.h>
#include <SPI.h>

#define  SPI_CS     2
#define  SPI_CLK    1  // DUE   13
#define  SPI_DI     0 //  DUE   11 


#define  SPI_CS_RES   digitalWrite(SPI_CS, LOW)
#define  SPI_CS_SET   digitalWrite(SPI_CS, HIGH)
#define  SPI_CLK_RES   digitalWrite(SPI_CLK, LOW)
#define  SPI_CLK_SET   digitalWrite(SPI_CLK, HIGH)
#define  SPI_DI_RES   digitalWrite(SPI_DI, LOW)
#define  SPI_DI_SET   digitalWrite(SPI_DI, HIGH)



void HW_SPI_Send(unsigned char i)
{ 
  SPI.transfer( i);
}




//RGB+9b_SPI(rise)
 void SW_SPI_Send(unsigned char i)
{  
   unsigned char n;
   
   for(n=0; n<8; n++)			
   {       
			SPI_CLK_RES;
			if(i&0x80)SPI_DI_SET ;
                        else SPI_DI_RES ;

			SPI_CLK_SET;

			i<<=1;
	  
   }
}


void SPI_WriteComm(unsigned char i)
{
    SPI_CS_RES;
    SPI_DI_RES;//DI=0 COMMAND
 
    SPI_CLK_RES;
    SPI_CLK_SET;
  //  HW_SPI_Send(i);
    SW_SPI_Send(i);	
    SPI_CS_SET;	
}



void SPI_WriteData(unsigned char i)
{ 
    SPI_CS_RES;
    SPI_DI_SET;//DI=1 DATA
    SPI_CLK_RES;
    SPI_CLK_SET;
  //  HW_SPI_Send(i);   
    SW_SPI_Send(i);
    SPI_CS_SET;  		
} 



void GC9503_Initial(void)
{  
     pinMode(SPI_CS,   OUTPUT);
    digitalWrite(SPI_CS, HIGH);
    pinMode(SPI_CLK,   OUTPUT);
    digitalWrite(SPI_CLK, LOW);
    pinMode(SPI_DI,   OUTPUT);
    digitalWrite(SPI_DI, LOW);  
  

	SPI_WriteComm(0xF0);SPI_WriteData(0x55);SPI_WriteData(0xAA);SPI_WriteData(0x52);SPI_WriteData(0x08);SPI_WriteData(0x00);
	SPI_WriteComm(0xF6);SPI_WriteData(0x5A);SPI_WriteData(0x87);
	SPI_WriteComm(0xC1);SPI_WriteData(0x3F);
	SPI_WriteComm(0xCD);SPI_WriteData(0x25);
	SPI_WriteComm(0xC9);SPI_WriteData(0x10);
	SPI_WriteComm(0xF8);SPI_WriteData(0x8A);
	SPI_WriteComm(0xAC);SPI_WriteData(0x45);
	SPI_WriteComm(0xA7);SPI_WriteData(0x47);
	SPI_WriteComm(0xA0);SPI_WriteData(0xCC);
	SPI_WriteComm(0x86);SPI_WriteData(0x99);SPI_WriteData(0xA3);SPI_WriteData(0xA3);SPI_WriteData(0x31);
	SPI_WriteComm(0xFA);SPI_WriteData(0x08);SPI_WriteData(0x08);SPI_WriteData(0x00);SPI_WriteData(0x04);
	SPI_WriteComm(0xA3);SPI_WriteData(0x6E);
	SPI_WriteComm(0xFD);SPI_WriteData(0x28);SPI_WriteData(0x3C);SPI_WriteData(0x00);
	SPI_WriteComm(0x9A);SPI_WriteData(0x4a);
	SPI_WriteComm(0x9B);SPI_WriteData(0x22);
	SPI_WriteComm(0x82);SPI_WriteData(0x00);SPI_WriteData(0x00);
	SPI_WriteComm(0x80);SPI_WriteData(0x54);

  
	SPI_WriteComm(0xB0);SPI_WriteData(0x00);SPI_WriteData(0x25); SPI_WriteData(0x20);SPI_WriteData(0x15);SPI_WriteData(0x10); 
 	SPI_WriteComm(0xB1);SPI_WriteData(0x32); 
 	SPI_WriteComm(0x3A);SPI_WriteData(0x60);

	SPI_WriteComm(0x7A);SPI_WriteData(0x0F);SPI_WriteData(0x13);
	SPI_WriteComm(0x7B);SPI_WriteData(0x0F);SPI_WriteData(0x13);
	SPI_WriteComm(0x6D);SPI_WriteData(0x0c);SPI_WriteData(0x03);SPI_WriteData(0x1e);SPI_WriteData(0x02);SPI_WriteData(0x08);SPI_WriteData(0x1a);SPI_WriteData(0x19);SPI_WriteData(0x03);SPI_WriteData(0x0d);SPI_WriteData(0x0e);SPI_WriteData(0x0f);SPI_WriteData(0x10);SPI_WriteData(0x1E);SPI_WriteData(0x1E);SPI_WriteData(0x1E);SPI_WriteData(0x1E);SPI_WriteData(0x1E);SPI_WriteData(0x1E);SPI_WriteData(0x1E);SPI_WriteData(0x1E);SPI_WriteData(0x11);SPI_WriteData(0x12);SPI_WriteData(0x13);SPI_WriteData(0x14);SPI_WriteData(0x03);SPI_WriteData(0x19);SPI_WriteData(0x1a);SPI_WriteData(0x07);SPI_WriteData(0x01);SPI_WriteData(0x1E);SPI_WriteData(0x03);SPI_WriteData(0x0c);
	SPI_WriteComm(0x64);SPI_WriteData(0x38);SPI_WriteData(0x04);SPI_WriteData(0x03);SPI_WriteData(0xc4);SPI_WriteData(0x03);SPI_WriteData(0x03);SPI_WriteData(0x38);SPI_WriteData(0x02);SPI_WriteData(0x03);SPI_WriteData(0xc6);SPI_WriteData(0x03);SPI_WriteData(0x03);SPI_WriteData(0x2C);SPI_WriteData(0x7A);SPI_WriteData(0x2C);SPI_WriteData(0x7A);
	SPI_WriteComm(0x65);SPI_WriteData(0x38);SPI_WriteData(0x08);SPI_WriteData(0x03);SPI_WriteData(0xc0);SPI_WriteData(0x03);SPI_WriteData(0x03);SPI_WriteData(0x38);SPI_WriteData(0x06);SPI_WriteData(0x03);SPI_WriteData(0xc2);SPI_WriteData(0x03);SPI_WriteData(0x03);SPI_WriteData(0x2C);SPI_WriteData(0x7A);SPI_WriteData(0x2C);SPI_WriteData(0x7A);
	SPI_WriteComm(0x66);SPI_WriteData(0x83);SPI_WriteData(0xd0);SPI_WriteData(0x03);SPI_WriteData(0xc4);SPI_WriteData(0x03);SPI_WriteData(0x03);SPI_WriteData(0x83);SPI_WriteData(0xd0);SPI_WriteData(0x03);SPI_WriteData(0xc4);SPI_WriteData(0x03);SPI_WriteData(0x03);SPI_WriteData(0x2C);SPI_WriteData(0x7A);SPI_WriteData(0x2C);SPI_WriteData(0x7A);
	SPI_WriteComm(0x60);SPI_WriteData(0x38);SPI_WriteData(0x0c);SPI_WriteData(0x3c);SPI_WriteData(0x3c);SPI_WriteData(0x38);SPI_WriteData(0x0b);SPI_WriteData(0x3c);SPI_WriteData(0x3c);
	SPI_WriteComm(0x61);SPI_WriteData(0xb3);SPI_WriteData(0xc4);SPI_WriteData(0x3c);SPI_WriteData(0x3c);SPI_WriteData(0xb3);SPI_WriteData(0xc4);SPI_WriteData(0x3c);SPI_WriteData(0x3c);
	SPI_WriteComm(0x62);SPI_WriteData(0xb3);SPI_WriteData(0xc4);SPI_WriteData(0x3c);SPI_WriteData(0x3c);SPI_WriteData(0xb3);SPI_WriteData(0xc4);SPI_WriteData(0x3c);SPI_WriteData(0x3c);
	SPI_WriteComm(0x63);SPI_WriteData(0x38);SPI_WriteData(0x0a);SPI_WriteData(0x3c);SPI_WriteData(0x3c);SPI_WriteData(0x38);SPI_WriteData(0x09);SPI_WriteData(0x3c);SPI_WriteData(0x3c);
	SPI_WriteComm(0x68);SPI_WriteData(0x77);SPI_WriteData(0x08);SPI_WriteData(0x0a);SPI_WriteData(0x08);SPI_WriteData(0x09);SPI_WriteData(0x00);SPI_WriteData(0x00);SPI_WriteData(0x18);SPI_WriteData(0x0a);SPI_WriteData(0x08);SPI_WriteData(0x09);SPI_WriteData(0x00);SPI_WriteData(0x00);
	SPI_WriteComm(0x69);SPI_WriteData(0x14);SPI_WriteData(0x22);SPI_WriteData(0x14);SPI_WriteData(0x22);SPI_WriteData(0x44);SPI_WriteData(0x22);SPI_WriteData(0x08);
	SPI_WriteComm(0x6B);SPI_WriteData(0x07);
	SPI_WriteComm(0xD1);SPI_WriteData(0x00);SPI_WriteData(0x00);SPI_WriteData(0x00);SPI_WriteData(0x10);SPI_WriteData(0x00);SPI_WriteData(0x22);SPI_WriteData(0x00);SPI_WriteData(0x2c);SPI_WriteData(0x00);SPI_WriteData(0x2e);SPI_WriteData(0x00);SPI_WriteData(0x56);SPI_WriteData(0x00);SPI_WriteData(0x58);SPI_WriteData(0x00);SPI_WriteData(0x7c);SPI_WriteData(0x00);SPI_WriteData(0x9a);SPI_WriteData(0x00);SPI_WriteData(0xce);SPI_WriteData(0x00);SPI_WriteData(0xfa);SPI_WriteData(0x01);SPI_WriteData(0x4c);SPI_WriteData(0x01);SPI_WriteData(0x94);SPI_WriteData(0x01);SPI_WriteData(0x96);SPI_WriteData(0x01);SPI_WriteData(0xda);SPI_WriteData(0x02);SPI_WriteData(0x32);SPI_WriteData(0x02);SPI_WriteData(0x76);SPI_WriteData(0x02);SPI_WriteData(0xcc);SPI_WriteData(0x03);SPI_WriteData(0x18);SPI_WriteData(0x03);SPI_WriteData(0x55);SPI_WriteData(0x03);SPI_WriteData(0x6b);SPI_WriteData(0x03);SPI_WriteData(0x9b);SPI_WriteData(0x03);SPI_WriteData(0xac);SPI_WriteData(0x03);SPI_WriteData(0xb8);SPI_WriteData(0x03);SPI_WriteData(0xe0);SPI_WriteData(0x03);SPI_WriteData(0xFF);
	SPI_WriteComm(0xD2);SPI_WriteData(0x00);SPI_WriteData(0x00);SPI_WriteData(0x00);SPI_WriteData(0x10);SPI_WriteData(0x00);SPI_WriteData(0x22);SPI_WriteData(0x00);SPI_WriteData(0x2c);SPI_WriteData(0x00);SPI_WriteData(0x2e);SPI_WriteData(0x00);SPI_WriteData(0x56);SPI_WriteData(0x00);SPI_WriteData(0x58);SPI_WriteData(0x00);SPI_WriteData(0x7c);SPI_WriteData(0x00);SPI_WriteData(0x9a);SPI_WriteData(0x00);SPI_WriteData(0xce);SPI_WriteData(0x00);SPI_WriteData(0xfa);SPI_WriteData(0x01);SPI_WriteData(0x4c);SPI_WriteData(0x01);SPI_WriteData(0x94);SPI_WriteData(0x01);SPI_WriteData(0x96);SPI_WriteData(0x01);SPI_WriteData(0xda);SPI_WriteData(0x02);SPI_WriteData(0x32);SPI_WriteData(0x02);SPI_WriteData(0x76);SPI_WriteData(0x02);SPI_WriteData(0xcc);SPI_WriteData(0x03);SPI_WriteData(0x18);SPI_WriteData(0x03);SPI_WriteData(0x55);SPI_WriteData(0x03);SPI_WriteData(0x6b);SPI_WriteData(0x03);SPI_WriteData(0x9b);SPI_WriteData(0x03);SPI_WriteData(0xac);SPI_WriteData(0x03);SPI_WriteData(0xb8);SPI_WriteData(0x03);SPI_WriteData(0xe0);SPI_WriteData(0x03);SPI_WriteData(0xFF);
	SPI_WriteComm(0xD3);SPI_WriteData(0x00);SPI_WriteData(0x00);SPI_WriteData(0x00);SPI_WriteData(0x10);SPI_WriteData(0x00);SPI_WriteData(0x22);SPI_WriteData(0x00);SPI_WriteData(0x2c);SPI_WriteData(0x00);SPI_WriteData(0x2e);SPI_WriteData(0x00);SPI_WriteData(0x56);SPI_WriteData(0x00);SPI_WriteData(0x58);SPI_WriteData(0x00);SPI_WriteData(0x7c);SPI_WriteData(0x00);SPI_WriteData(0x9a);SPI_WriteData(0x00);SPI_WriteData(0xce);SPI_WriteData(0x00);SPI_WriteData(0xfa);SPI_WriteData(0x01);SPI_WriteData(0x4c);SPI_WriteData(0x01);SPI_WriteData(0x94);SPI_WriteData(0x01);SPI_WriteData(0x96);SPI_WriteData(0x01);SPI_WriteData(0xda);SPI_WriteData(0x02);SPI_WriteData(0x32);SPI_WriteData(0x02);SPI_WriteData(0x76);SPI_WriteData(0x02);SPI_WriteData(0xcc);SPI_WriteData(0x03);SPI_WriteData(0x18);SPI_WriteData(0x03);SPI_WriteData(0x55);SPI_WriteData(0x03);SPI_WriteData(0x6b);SPI_WriteData(0x03);SPI_WriteData(0x9b);SPI_WriteData(0x03);SPI_WriteData(0xac);SPI_WriteData(0x03);SPI_WriteData(0xb8);SPI_WriteData(0x03);SPI_WriteData(0xe0);SPI_WriteData(0x03);SPI_WriteData(0xFF);
	SPI_WriteComm(0xD4);SPI_WriteData(0x00);SPI_WriteData(0x00);SPI_WriteData(0x00);SPI_WriteData(0x10);SPI_WriteData(0x00);SPI_WriteData(0x22);SPI_WriteData(0x00);SPI_WriteData(0x2c);SPI_WriteData(0x00);SPI_WriteData(0x2e);SPI_WriteData(0x00);SPI_WriteData(0x56);SPI_WriteData(0x00);SPI_WriteData(0x58);SPI_WriteData(0x00);SPI_WriteData(0x7c);SPI_WriteData(0x00);SPI_WriteData(0x9a);SPI_WriteData(0x00);SPI_WriteData(0xce);SPI_WriteData(0x00);SPI_WriteData(0xfa);SPI_WriteData(0x01);SPI_WriteData(0x4c);SPI_WriteData(0x01);SPI_WriteData(0x94);SPI_WriteData(0x01);SPI_WriteData(0x96);SPI_WriteData(0x01);SPI_WriteData(0xda);SPI_WriteData(0x02);SPI_WriteData(0x32);SPI_WriteData(0x02);SPI_WriteData(0x76);SPI_WriteData(0x02);SPI_WriteData(0xcc);SPI_WriteData(0x03);SPI_WriteData(0x18);SPI_WriteData(0x03);SPI_WriteData(0x55);SPI_WriteData(0x03);SPI_WriteData(0x6b);SPI_WriteData(0x03);SPI_WriteData(0x9b);SPI_WriteData(0x03);SPI_WriteData(0xac);SPI_WriteData(0x03);SPI_WriteData(0xb8);SPI_WriteData(0x03);SPI_WriteData(0xe0);SPI_WriteData(0x03);SPI_WriteData(0xFF);
	SPI_WriteComm(0xD5);SPI_WriteData(0x00);SPI_WriteData(0x00);SPI_WriteData(0x00);SPI_WriteData(0x10);SPI_WriteData(0x00);SPI_WriteData(0x22);SPI_WriteData(0x00);SPI_WriteData(0x2c);SPI_WriteData(0x00);SPI_WriteData(0x2e);SPI_WriteData(0x00);SPI_WriteData(0x56);SPI_WriteData(0x00);SPI_WriteData(0x58);SPI_WriteData(0x00);SPI_WriteData(0x7c);SPI_WriteData(0x00);SPI_WriteData(0x9a);SPI_WriteData(0x00);SPI_WriteData(0xce);SPI_WriteData(0x00);SPI_WriteData(0xfa);SPI_WriteData(0x01);SPI_WriteData(0x4c);SPI_WriteData(0x01);SPI_WriteData(0x94);SPI_WriteData(0x01);SPI_WriteData(0x96);SPI_WriteData(0x01);SPI_WriteData(0xda);SPI_WriteData(0x02);SPI_WriteData(0x32);SPI_WriteData(0x02);SPI_WriteData(0x76);SPI_WriteData(0x02);SPI_WriteData(0xcc);SPI_WriteData(0x03);SPI_WriteData(0x18);SPI_WriteData(0x03);SPI_WriteData(0x55);SPI_WriteData(0x03);SPI_WriteData(0x6b);SPI_WriteData(0x03);SPI_WriteData(0x9b);SPI_WriteData(0x03);SPI_WriteData(0xac);SPI_WriteData(0x03);SPI_WriteData(0xb8);SPI_WriteData(0x03);SPI_WriteData(0xe0);SPI_WriteData(0x03);SPI_WriteData(0xFF);
	SPI_WriteComm(0xD6);SPI_WriteData(0x00);SPI_WriteData(0x00);SPI_WriteData(0x00);SPI_WriteData(0x10);SPI_WriteData(0x00);SPI_WriteData(0x22);SPI_WriteData(0x00);SPI_WriteData(0x2c);SPI_WriteData(0x00);SPI_WriteData(0x2e);SPI_WriteData(0x00);SPI_WriteData(0x56);SPI_WriteData(0x00);SPI_WriteData(0x58);SPI_WriteData(0x00);SPI_WriteData(0x7c);SPI_WriteData(0x00);SPI_WriteData(0x9a);SPI_WriteData(0x00);SPI_WriteData(0xce);SPI_WriteData(0x00);SPI_WriteData(0xfa);SPI_WriteData(0x01);SPI_WriteData(0x4c);SPI_WriteData(0x01);SPI_WriteData(0x94);SPI_WriteData(0x01);SPI_WriteData(0x96);SPI_WriteData(0x01);SPI_WriteData(0xda);SPI_WriteData(0x02);SPI_WriteData(0x32);SPI_WriteData(0x02);SPI_WriteData(0x76);SPI_WriteData(0x02);SPI_WriteData(0xcc);SPI_WriteData(0x03);SPI_WriteData(0x18);SPI_WriteData(0x03);SPI_WriteData(0x55);SPI_WriteData(0x03);SPI_WriteData(0x6b);SPI_WriteData(0x03);SPI_WriteData(0x9b);SPI_WriteData(0x03);SPI_WriteData(0xac);SPI_WriteData(0x03);SPI_WriteData(0xb8);SPI_WriteData(0x03);SPI_WriteData(0xe0);SPI_WriteData(0x03);SPI_WriteData(0xFF);
 
 
	
	SPI_WriteComm (0x11);     
	delay(120);                
	SPI_WriteComm (0x29); 
	delay(20);  
}

#endif


