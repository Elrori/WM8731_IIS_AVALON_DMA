/*
 * WM8731.c
 *
 *  Created on: 2018年1月25日
 *      Author: Helrori
 */
#include "WM8731.h"
void IIC_WM_GPIO_Init()
{
	USE_WM_SDA_AS_OUTPUT;
	SET_WM_SCLK;
	SET_WM_SDA;
}
alt_u32 IIC_WM_ACK()
{
	alt_u8 state = 0;
	USE_WM_SDA_AS_INPUT;
	usleep(1);
	SET_WM_SCLK;
	usleep(1);
//	state = IORD(WM8731_SDA_BASE,0);ignore ACK
	usleep(1);
	CLR_WM_SCLK;
	usleep(1);
	USE_WM_SDA_AS_OUTPUT;
	if(state == 0x01)
		return NONE;
	else
		return OK;
}
void IIC_WM_Start()
{
	SET_WM_SDA;
	SET_WM_SCLK;
	usleep(1);
	CLR_WM_SDA;
	usleep(1);
	CLR_WM_SCLK;
	usleep(1);
}
void IIC_WM_Stop()
{
	CLR_WM_SDA;
	usleep(1);
	SET_WM_SCLK;
	usleep(1);
	SET_WM_SDA;
	usleep(1);
}
alt_u32 IIC_WM_Write_Byte(unsigned int Addr/*Low 7bit used*/,unsigned int Data/*Low 9bit used*/)
{
	alt_u32 All = ((Addr&0x7f)<<9)+(Data&0x1ff);
	alt_u32 i;
	alt_u32 Device_Address = DEVICE_ADDR_WR;
	IIC_WM_Start();
	for(i=0;i<8;i++)
	{
		if((Device_Address&0x80) == 0x80) SET_WM_SDA;else CLR_WM_SDA;
		usleep(1);
		SET_WM_SCLK;
		usleep(1);
		usleep(1);
		CLR_WM_SCLK;
		usleep(1);
		Device_Address<<=1;
	}
	if(IIC_WM_ACK() == NONE) return 1;
	for(i=0;i<8;i++)
	{
		if((All&0x8000) == 0x8000) SET_WM_SDA;else CLR_WM_SDA;
		usleep(1);
		SET_WM_SCLK;
		usleep(1);
		usleep(1);
		CLR_WM_SCLK;
		usleep(1);
		All<<=1;
	}
	if(IIC_WM_ACK() == NONE) return 2;
	for(i=0;i<8;i++)
	{
		if((All&0x8000) == 0x8000) SET_WM_SDA;else CLR_WM_SDA;
		usleep(1);
		SET_WM_SCLK;
		usleep(1);
		usleep(1);
		CLR_WM_SCLK;
		usleep(1);
		All<<=1;
	}
	if(IIC_WM_ACK() == NONE) return 3;
	IIC_WM_Stop();
	return 0;
}
void WM_CMD_Trans(WM8731_DEVICE_TYPE *WM8731_DEVICE)//
{
//	IIC_WM_Write_Byte(0x02,0				);//MUTE
//	IIC_WM_Write_Byte(0x03,0				);//MUTE
	IIC_WM_Write_Byte(0x0F,WM8731_DEVICE->reset_zero					);
	IIC_WM_Write_Byte(0x00,WM8731_DEVICE->left_line_in					);
	IIC_WM_Write_Byte(0x01,WM8731_DEVICE->right_line_in					);
	IIC_WM_Write_Byte(0x02,WM8731_DEVICE->left_head_out					);
	IIC_WM_Write_Byte(0x03,WM8731_DEVICE->right_head_out				);
	IIC_WM_Write_Byte(0x04,WM8731_DEVICE->analogue_audio_path_control	);
	IIC_WM_Write_Byte(0x05,WM8731_DEVICE->digital_audio_path_control	);
	IIC_WM_Write_Byte(0x06,WM8731_DEVICE->power_down_control			);
	IIC_WM_Write_Byte(0x07,WM8731_DEVICE->digital_audio_interface		);
	IIC_WM_Write_Byte(0x08,WM8731_DEVICE->sampling_control				);
	IIC_WM_Write_Byte(0x09,WM8731_DEVICE->active_control				);
}
void WM_StructDEInit(WM8731_DEVICE_TYPE *WM8731_DEVICE,alt_u32 IIS_BASE)
{
	//DSP 模式 首位空，wm8731主模式。48Khz sample rate
	WM8731_DEVICE->WM_IIS_base 					= (alt_u32)IIS_BASE;
	WM8731_DEVICE->active_control 				= 0x01;
	WM8731_DEVICE->analogue_audio_path_control 	= 0x10;
	WM8731_DEVICE->digital_audio_interface		= 0x53;
	WM8731_DEVICE->digital_audio_path_control	= 0x01;
	WM8731_DEVICE->left_head_out				= 0x51;
	WM8731_DEVICE->left_line_in					= 0x17;
	WM8731_DEVICE->power_down_control			= 0x00;
	WM8731_DEVICE->reset_zero					= 0x00;
	WM8731_DEVICE->right_head_out				= 0x51;
	WM8731_DEVICE->right_line_in				= 0x17;
#ifdef USE_12288000HZ_MCLK 	//Normal mode
	WM8731_DEVICE->sampling_control				= 0x00;
#endif
#ifdef USE_12000000HZ_MCLK	//USB mode
	WM8731_DEVICE->sampling_control				= 0x01;
#endif

}
void WM_DEInit(WM8731_DEVICE_TYPE *WM8731_DEVICE,alt_u32 IIS_BASE)//初始化为缺省值
{
	IIC_WM_GPIO_Init();
	WM_StructDEInit(WM8731_DEVICE,IIS_BASE);
	WM_CMD_Trans(WM8731_DEVICE);
}
void IIS_IP_Print_All_Reg(WM8731_DEVICE_TYPE *WM8731_DEVICE)
{
  printf("wrused:	%d\n",0x1ff&IORD(WM8731_DEVICE->WM_IIS_base,0));
  printf("start address:0x%x\n",IORD(WM8731_DEVICE->WM_IIS_base,1));
  printf("length:	%d\n",IORD(WM8731_DEVICE->WM_IIS_base,2));
  printf("threshold:	%d\n",0x1ff&IORD(WM8731_DEVICE->WM_IIS_base,3));
  printf("ctrl:	0x%x\n",0xf&IORD(WM8731_DEVICE->WM_IIS_base,4));
}

alt_u32 IIS_IP_Get_DAC_Used(WM8731_DEVICE_TYPE *WM8731_DEVICE)
{
	return IORD(WM8731_DEVICE->WM_IIS_base,0)&0x1ff;
}
//alt_u32 IIS_IP_Get_Write_Allow_Flag(WM8731_DEVICE_TYPE *WM8731_DEVICE)
//{
//	return IORD(WM8731_DEVICE->WM_IIS_base,0)&0x80000000;
//}
void IIS_IP_Global_Reset(WM8731_DEVICE_TYPE *WM8731_DEVICE)
{	alt_u32 buf;
	buf = (IORD(WM8731_DEVICE->WM_IIS_base,4)&0xf)|GLOBLE_RESET_MASK;
	IOWR(WM8731_DEVICE->WM_IIS_base,4,buf);
	buf = (IORD(WM8731_DEVICE->WM_IIS_base,4)&0xf)&(~GLOBLE_RESET_MASK);
	IOWR(WM8731_DEVICE->WM_IIS_base,4,buf);
}
alt_u32	IIS_Get_IRQ_Flag(WM8731_DEVICE_TYPE *WM8731_DEVICE)
{
	return (IORD(WM8731_DEVICE->WM_IIS_base,4)&0xf)&IRQ_FLAG_MASK;
}
void	IIS_Clear_IRQ_Flag(WM8731_DEVICE_TYPE *WM8731_DEVICE)
{
	IOWR(WM8731_DEVICE->WM_IIS_base,4,0xb&(0xf&IORD(WM8731_DEVICE->WM_IIS_base,4)));// clear irq flag
}
void	IIS_Set_IRQ_Flag(WM8731_DEVICE_TYPE *WM8731_DEVICE)
{
	IOWR(WM8731_DEVICE->WM_IIS_base,4,0x4|(0xf&IORD(WM8731_DEVICE->WM_IIS_base,4)));// clear irq flag
}
void	IIS_IRQ_Enable(WM8731_DEVICE_TYPE *WM8731_DEVICE)
{
	IOWR(WM8731_DEVICE->WM_IIS_base,4,IRQ_EN_MASK|(0xf&IORD(WM8731_DEVICE->WM_IIS_base,4)));
}
void	IIS_IRQ_Disable(WM8731_DEVICE_TYPE *WM8731_DEVICE)
{
	IOWR(WM8731_DEVICE->WM_IIS_base,4,(~IRQ_EN_MASK)&(0xf&IORD(WM8731_DEVICE->WM_IIS_base,4)));
}
void IIS_DMA_Go(WM8731_DEVICE_TYPE *WM8731_DEVICE)
{
	IOWR(WM8731_DEVICE->WM_IIS_base,4,GO_STOP_MASK|(0xf&IORD(WM8731_DEVICE->WM_IIS_base,4)));
}
void IIS_DMA_Stop(WM8731_DEVICE_TYPE *WM8731_DEVICE)
{
	IOWR(WM8731_DEVICE->WM_IIS_base,4,(~GO_STOP_MASK)&(0xf&IORD(WM8731_DEVICE->WM_IIS_base,4)));
}
void IIS_Set_DMA_Base(WM8731_DEVICE_TYPE *WM8731_DEVICE,void *base)//set DMA read start address
{
	IOWR(WM8731_DEVICE->WM_IIS_base,1,(alt_u32*)base);
}
void IIS_Set_DMA_TransformLength(WM8731_DEVICE_TYPE *WM8731_DEVICE,alt_u32 bytetoread)//set DMA read start address
{
	IOWR(WM8731_DEVICE->WM_IIS_base,2,bytetoread);		//transform length bytes
}
void IIS_DMA_Init(WM8731_DEVICE_TYPE *WM8731_DEVICE,void *base,alt_u32 bytetoread,alt_u32 threshold/*6 ~ 512*/,alt_32 IRQ_en)
{
	IOWR(WM8731_DEVICE->WM_IIS_base,4,0);				//clear go bit make state machine to load address and idle
	IOWR(WM8731_DEVICE->WM_IIS_base,1,(alt_u32*)base);	//set start address
	IOWR(WM8731_DEVICE->WM_IIS_base,2,bytetoread);		//transform length bytes
	IOWR(WM8731_DEVICE->WM_IIS_base,3,threshold);		//FIFO too little threshold number(6 ~ 512)
	if(IRQ_en)
		IIS_IRQ_Enable(WM8731_DEVICE->WM_IIS_base);
}

void WM_Set_Volume(WM8731_DEVICE_TYPE *WM8731_DEVICE,char dB/* -73 ~ 6 dB */)
{
	alt_u8 vol_bit;
	vol_bit = (alt_u8)(48+79*((dB+73.0)/79));
	WM8731_DEVICE->left_head_out	= vol_bit;
	WM8731_DEVICE->right_head_out	= vol_bit;
	IIC_WM_Write_Byte(0x02,vol_bit);
	IIC_WM_Write_Byte(0x03,vol_bit);
}
alt_u32 WM_Set_Sample_Rate(WM8731_DEVICE_TYPE *WM8731_DEVICE,alt_u32 sample_rate)
{
	alt_u32 sample_rate_bit  = 0;
#ifdef USE_12288000HZ_MCLK
	switch(sample_rate)
	{
	case SAMPLE_RATE_8K		:sample_rate_bit = 0x0c;break;
	case SAMPLE_RATE_32K	:sample_rate_bit = 0x18;break;
	case SAMPLE_RATE_48K	:sample_rate_bit = 0x00;break;
	case SAMPLE_RATE_96K	:sample_rate_bit = 0x1c;break;
	default:
	{
		fprintf(stderr,"Sample %d not available\n",(alt_32)sample_rate);
		return 1;
	}
	}
	WM8731_DEVICE->sampling_control			= sample_rate_bit;
#endif
#ifdef USE_12000000HZ_MCLK
	switch(sample_rate)
	{
	case SAMPLE_RATE_8K		:sample_rate_bit = 0x0d;break;
	case SAMPLE_RATE_32K	:sample_rate_bit = 0x19;break;
	case SAMPLE_RATE_44K1	:sample_rate_bit = 0x23;break;
	case SAMPLE_RATE_48K	:sample_rate_bit = 0x01;break;
	case SAMPLE_RATE_88K2	:sample_rate_bit = 0x3f;break;
	case SAMPLE_RATE_96K	:sample_rate_bit = 0x1d;break;
	default:
		{
			fprintf(stderr,"Sample %d not available\n",sample_rate);
			return 1;
		}
	}
	WM8731_DEVICE->sampling_control			= sample_rate_bit;
#endif

	IIC_WM_Write_Byte(0x08,WM8731_DEVICE->sampling_control);
	return 0;
}
void WM_MUTE(WM8731_DEVICE_TYPE *WM8731_DEVICE,alt_u32 n)
{
	if(n)
	{
		IIC_WM_Write_Byte(0x02,0x0				);//MUTE
		IIC_WM_Write_Byte(0x03,0x0				);//MUTE
	}
	else
	{
		IIC_WM_Write_Byte(0x02,WM8731_DEVICE->left_head_out				);//not MUTE
		IIC_WM_Write_Byte(0x03,WM8731_DEVICE->right_head_out			);//not MUTE
	}
}
