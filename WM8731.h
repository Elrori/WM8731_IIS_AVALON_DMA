/*
 * WM8731.h
 *
 *  Created on: 2018年1月25日
 *      Author: Helrori
 *      仅使用了DAC
 */

#ifndef WM8731_H_
#define WM8731_H_
#include <system.h>
#include <io.h>
#include <alt_types.h>
#include <stdio.h>

//#define USE_12288000HZ_MCLK
#define USE_12000000HZ_MCLK
//---------------------------------------------------------------------------
//								IIC	pin
//---------------------------------------------------------------------------
#define DEVICE_ADDR				(0x1A)
#define DEVICE_ADDR_WR			(DEVICE_ADDR*2)
#define DEVICE_ADDR_RD			((DEVICE_ADDR*2)+1)//useless for wm8731
#define SET_WM_SCLK 			(IOWR(WM8731_SCLK_BASE,0,0x01))
#define CLR_WM_SCLK 			(IOWR(WM8731_SCLK_BASE,0,0x00))
#define SET_WM_SDA				(IOWR(WM8731_SDA_BASE,0,0x01))
#define CLR_WM_SDA				(IOWR(WM8731_SDA_BASE,0,0x00))
#define USE_WM_SDA_AS_OUTPUT 	(IOWR(WM8731_SDA_BASE,1,0xFFFFFFFF))
#define USE_WM_SDA_AS_INPUT 	(IOWR(WM8731_SDA_BASE,1,0x00))
#define RD_WM_SDA				(IORD(WM8731_SDA_BASE,0))
//---------------------------------------------------------------------------
//									Reg
//---------------------------------------------------------------------------
//Default volume (0dB), disable mute, disable simultaneous loading
#define LEFT_LINE_IN 				  0x17		//addr (00h)
#define RIGHT_LINE_IN 				  0x17 		//addr (01h)

//Default volume (0dB), No zero cross detection, disable simultaneous loading
#define LEFT_HEAD_OUT 				  0x51		//addr (02h)
#define RIGHT_HEAD_OUT 				  0x51		//addr (03h)

// analog audio path control
// bit 0: micboost disabled
// bit 1: mute mic disabled
// bit 2: INSEL (1: Mic in 0: Line in) line in selected.
// bit 3: BYPASS disabled
// bit 4: DACSEL (1: select, 0: Dont select)
// bit 5: SIDETONE disabled
// bit [7:6] sidetone antenuation 00
#define ANALOGUE_AUDIO_PATH_CONTROL 	  0x10		//addr (04h)

// digital audio path control
// bit 0: ADC High Pass Filter Enable (1: disable 0: enable)
// bit[2:1]: De-emphasis Control
// 		11 = 48kHz
// 		10 = 44.1 kHz
// 		01 = 32kHz
// 		00 = Disable
// bit3: DAC soft mute (1: enable, 0: disable)
// bit4: Store dc offset when High pass Filter disabled (1: store, 0: clear offset)
#define DIGITAL_AUDIO_PATH_CONTROL 	  0x01		//addr (05h)

// all power saving features are turned off.
#define POWER_DOWN_CONTROL 			  0x0		//addr (06h)

// digital audio interface format
// bit[1:0] DSP mode 11
// bit[3:2] data length select
//		11 = 32 bits
//		10 = 24 bits
//		01 = 20 bits
//		00 = 16 bits
// bit [4] select DSP mode A/B
// 		1: MSB on 2nd BCLK rising edge after DACLRC rising edge
//		0: MSB on 1st "  "
// bit [5] Left Right Swap (1:enable 0: disable)
// bit [6] Master/Slave (1:master, 0:slave)
// bit [7] BCLK invert	(1: invert, 0: don't)
#define DIGITAL_AUDIO_INTERFACE		  0x53		//addr (07h)9'b001010011;

// Normal mode 256fs No clock dividing
// bit [0] 		1=USB;0=Normal
// bit [1]		BOSR
// bit [5:2]	SR[3:0]
#define SAMPLING_CONTROL 			  0x0		//addr (08h)

//bit [0]: activate interface (1: active, 0: inactive)
#define ACTIVE_CONTROL 				  0x01		//addr (09h)

//writing all zeros resets the device.
#define RESET_ZEROS					  0x0		//addr (0Fh)
//---------------------------------------------------------------------------
//									State
//---------------------------------------------------------------------------
#define NONE					((unsigned int)(0))
#define OK						((unsigned int)(1))
#define SAMPLE_RATE_8K			8000
#define SAMPLE_RATE_32K			32000
#define SAMPLE_RATE_44K1		44100
#define SAMPLE_RATE_48K			48000
#define SAMPLE_RATE_88K2		88200
#define SAMPLE_RATE_96K			96000
//---------------------------------------------------------------------------
//									IIS
//---------------------------------------------------------------------------
/*
*                         AVALON SLAVE REGISTER MAP
*
*	Offset Name               Function
*	=================================================================================================================
*	                         | 							| 	     8      	  ..    	    0   || R/W || default
*	=================================================================================================================
*	 0x0 STATUS				 |    		 				|              DAC FIFO used            ||  R  || FFFFFE00 ||
*	-------------------------+------------------------------------------------------------------++-----++----------++
*	 0x1 READ ADDR START	 |                          RAM ADDRESS                             || R/W ||    0     ||
*	-------------------------+------------------------------------------------------------------++-----++----------++
*	 0x2 READ DATA LEN(BYTES)|                          READ DATA LENGTH(BYTES)                 || R/W ||    0     ||
*	-------------------------+------------------------------------------------------------------++-----++----------++
*	 0x3 FIFO THRESHOLD 	 |                          |FIFO too little threshold number(6-512)|| R/W || FFFFFE00 ||
*	=================================================================================================================
*	                  		 |         |        3       |      2    |     1     |    	 0		||     ||
*	=================================================================================================================
*	 0x4 CONTRAL REG  		 |         |  Global reset  |  IRQ flag |   IRQ en  |  Go/Stop_n    || R/W || FFFFFFF0 ||
*	-------------------------+------------------------------------------------------------------++-----++----------++
*	DAC FIFO used:	DAC FIFO 占用字数。
*	RAM ADDRESS:	传送起始地址。
*	READ DATA LENGTH(BYTES):传送字节长度。
*	FIFO too little threshold number(6-512):FIFO阈值,范围(6-512),应大于FIFO深度的一半。
*	Global reset:	置1异步复位DACFIFO，置0恢复DACFIFO。CONTRAL REG低4位置0用来给通用寄存器复位。
*	IRQ flag:		中断标志,一次传送结束自动置1。
*	IRQ en:			使能IRQ flag的中断。
*	Go/Stop_n:		置1开始传送,一次传送结束自动置0。
*
*	Operation process:		1.Set RAM ADDRESS
*							2.Clear IRQ flag
*							3.Set Go/Stop_n
*							4.Check IRQ flag
*
*	Operation function process:(not use interrput)
*							IIS_DMA_Init(&WM8731_DEVICE,buff_,AUDIOBUFFSIZE,500,0);
*						+-->IIS_DMA_Go(&WM8731_DEVICE);
*						|	check IIS_Get_IRQ_Flag(&WM8731_DEVICE)==1 or not.
*						|	IIS_Clear_IRQ_Flag(&WM8731_DEVICE);
*		  	  	  	  	+<--IIS_Set_DMA_Base(&WM8731_DEVICE,_buff);//set start address
*
*	*/
#define GLOBLE_RESET_MASK		(0x8)
#define IRQ_FLAG_MASK			(0x4)
#define IRQ_EN_MASK				(0x2)
#define GO_STOP_MASK			(0x1)
typedef struct
{
	alt_u32 WM_IIS_base;//IIS 硬件地址，一个IIS地址操作一个WM芯片
	alt_u8	left_line_in;
	alt_u8	right_line_in;
	alt_u8	left_head_out;
	alt_u8	right_head_out;
	alt_u8	analogue_audio_path_control;
	alt_u8	digital_audio_path_control;
	alt_u8	power_down_control;
	alt_u8	digital_audio_interface;
	alt_u8	sampling_control;
	alt_u8	active_control;
	alt_u8	reset_zero;
}WM8731_DEVICE_TYPE;

//	写FIFO操作
//#define DAC_FIFO_WR(x) (IOWR(MY_WM8731_AVS_IP_0_BASE,1,(unsigned int)(x)))
//	操作IIS硬件
extern alt_u32 		IIS_IP_Get_DAC_Used			(WM8731_DEVICE_TYPE *WM8731_DEVICE);
//extern alt_u32		IIS_IP_Get_Write_Allow_Flag	(WM8731_DEVICE_TYPE *WM8731_DEVICE);
extern void 		IIS_IP_Global_Reset			(WM8731_DEVICE_TYPE *WM8731_DEVICE);
extern alt_u32		IIS_Get_IRQ_Flag			(WM8731_DEVICE_TYPE *WM8731_DEVICE);
extern void			IIS_Clear_IRQ_Flag			(WM8731_DEVICE_TYPE *WM8731_DEVICE);
extern void			IIS_Set_IRQ_Flag			(WM8731_DEVICE_TYPE *WM8731_DEVICE);
extern void			IIS_IRQ_Enable				(WM8731_DEVICE_TYPE *WM8731_DEVICE);
extern void			IIS_IRQ_Disable				(WM8731_DEVICE_TYPE *WM8731_DEVICE);
extern void			IIS_DMA_Go					(WM8731_DEVICE_TYPE *WM8731_DEVICE);
extern void			IIS_DMA_Stop				(WM8731_DEVICE_TYPE *WM8731_DEVICE);

extern void 		IIS_DMA_Init				(WM8731_DEVICE_TYPE *WM8731_DEVICE,\
												void *base,\
												alt_u32 bytetoread,\
												alt_u32 threshold,/*6 ~ 512*/\
												alt_32 IRQ_en);
extern void 		IIS_IP_Print_All_Reg(WM8731_DEVICE_TYPE *WM8731_DEVICE);
extern void			IIS_Set_DMA_Base			(WM8731_DEVICE_TYPE *WM8731_DEVICE,void *base);//set start address
//	通过软件IIC操作WM芯片
extern void 		WM_DEInit				(WM8731_DEVICE_TYPE *WM8731_DEVICE,alt_u32 IIS_BASE);
extern void 		WM_Set_Volume			(WM8731_DEVICE_TYPE *WM8731_DEVICE,char vol);
extern alt_u32 		WM_Set_Sample_Rate		(WM8731_DEVICE_TYPE *WM8731_DEVICE,alt_u32 sample_rate);
extern void 		WM_MUTE					(WM8731_DEVICE_TYPE *WM8731_DEVICE,alt_u32 n);
#endif /* WM8731_H_ */
