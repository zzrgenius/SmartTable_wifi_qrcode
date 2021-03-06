#include "stm32f10x.h" 
#include "sys_adc.h" 	
#include "stm32f10x_dma.h"
 
#include "stm32f10x_adc.h"  
  

#define AD_BUFSIZE 48
#define M 3 
u16 AD_Value[AD_BUFSIZE];
u16 adc_filter[M];
u16 gI_UsbOut;u16 gV_Battery;u16 gGas;
u8 tgs_flag = 0;
void sys_adc_init(void)
{
    ADC_InitTypeDef  ADC_InitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1,ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA|RCC_APB2Periph_GPIOC| RCC_APB2Periph_AFIO,ENABLE);
    GPIO_InitStructure.GPIO_Pin  =  ADC_IOUT_PIN |ADC_BATTERY_PIN | ADC_TAG_PIN ;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA,&GPIO_InitStructure); //

	GPIO_InitStructure.GPIO_Pin  =  TGS_PLUS_PIN   ;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(TGS_PLUS_PORT,&GPIO_InitStructure); //
    RCC_ADCCLKConfig(RCC_PCLK2_Div8);
    ADC_DeInit(ADC1);
	//ADC_StructInit(&ADC_InitStructure);  
    ADC_InitStructure.ADC_Mode                  =   ADC_Mode_Independent;  //独立模式
    ADC_InitStructure.ADC_ScanConvMode          =  	ENABLE;  	// DISABLE;    //连续多通道模式
    ADC_InitStructure.ADC_ContinuousConvMode    =   ENABLE;      //连续转换
    ADC_InitStructure.ADC_ExternalTrigConv      =   ADC_ExternalTrigConv_None; //转换不受外界决定
    ADC_InitStructure.ADC_DataAlign             =   ADC_DataAlign_Right;   //右对齐
    ADC_InitStructure.ADC_NbrOfChannel          =   M;       //扫描通道数
    ADC_Init(ADC1,&ADC_InitStructure);
 
	ADC_RegularChannelConfig(ADC1,ADC_Channel_0, 1,ADC_SampleTime_239Cycles5); //
	ADC_RegularChannelConfig(ADC1,ADC_Channel_1, 2,ADC_SampleTime_239Cycles5); //
	ADC_RegularChannelConfig(ADC1,ADC_Channel_2, 3,ADC_SampleTime_239Cycles5); //
  
	ADC_DMACmd(ADC1,ENABLE);//
    ADC_Cmd  (ADC1,ENABLE);             //使能或者失能指定的ADC
	//ADC_TempSensorVrefintCmd(ENABLE);
    //ADC_SoftwareStartConvCmd(ADC1,ENABLE);//使能或者失能指定的ADC的软件转换启动功能
    ADC_ResetCalibration(ADC1);//复位指定的ADC1的校准寄存器
    while(ADC_GetResetCalibrationStatus(ADC1));//获取ADC1复位校准寄存器的状态,设置状态则等�
    ADC_StartCalibration(ADC1);//开始指定ADC1的校准状态
    while(ADC_GetCalibrationStatus(ADC1)); //获取指定ADC1
}

void sys_adc_dma_config(void)
{
    DMA_InitTypeDef DMA_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1,ENABLE);
	
	NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel1_IRQn; 
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2; 
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0; 
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE; 
    NVIC_Init(&NVIC_InitStructure);          // Enable the DMA Interrupt 
	
    DMA_DeInit(DMA1_Channel1);//将DMA的通道1寄存器重设为缺省值
    DMA_InitStructure.DMA_PeripheralBaseAddr = (u32)&ADC1->DR; //DMA外设ADC基地址
    DMA_InitStructure.DMA_MemoryBaseAddr = (u32)&AD_Value;//DMA内存基地址
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC; //内存作为数据传输的目的地
    DMA_InitStructure.DMA_BufferSize  =	AD_BUFSIZE;//DMA通道的DMA缓存的大小
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;//外设地址寄存器不变
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable; //内存地址寄存器递增
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;//数据宽度为16位
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;//数据宽度为16位
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;//工作在 
    DMA_InitStructure.DMA_Priority = DMA_Priority_High; //DMA通道?x拥有高优先级
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;//DMA通道x没有设置为内存到内存传输
    DMA_Init(DMA1_Channel1,&DMA_InitStructure);//根据DMA_InitStruct中指定的参数初始化DMA的通道 
	DMA_ITConfig(DMA1_Channel1, DMA_IT_TC, ENABLE);   //open interrupt  use for filter
	
    DMA_Cmd(DMA1_Channel1,ENABLE); //启动DMA通道?
	ADC_SoftwareStartConvCmd(ADC1,ENABLE);
}


//定时器 定时 扫描按键
void TimerTGSConfig(u16 arr,u16 psc)
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;

	NVIC_InitTypeDef NVIC_InitStructure;
	/* Enable the TIM2 gloabal Interrupt */
	NVIC_InitStructure.NVIC_IRQChannel = TIM4_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);	

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);

	TIM_DeInit(TIM4);                                           //重新将Timer设置为缺省值

	TIM_InternalClockConfig(TIM4);                              //采用内部时钟给TIM2提供时钟源      
	TIM_TimeBaseStructure.TIM_Prescaler = psc;			//36000-1;               //预分频系数为36000-1，这样计数器时钟为72MHz/36000 = 2kHz       
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;     //设置时钟分割      
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; //设置计数器模式为向上计数模式       
	TIM_TimeBaseStructure.TIM_Period = arr;             //0xffff;                  //设置计数溢出大小， 就产生一个更新事件
	TIM_TimeBaseInit(TIM4,&TIM_TimeBaseStructure);              //将配置应用到TIM2中
	TIM_ClearITPendingBit(TIM4, TIM_IT_Update);	//清除中断标志
	TIM_ITConfig(TIM4,TIM_IT_Update,ENABLE);	//开中断
	TIM_Cmd(TIM4, ENABLE);
}
 //u16 adc_filter[3];

 
//Temperature= (1.42 - ADC_Value*3.3/4096)*1000/4.35 + 25
u16 test_tgs;
void filter(void)
{
	u8 i;
	u32  sum1 = 0;
	u32  sum2 = 0;
	u32  sum3 = 0;

	for(i=0;i<(AD_BUFSIZE/3);i++)
	{
		sum1 += AD_Value[3*i+0];
		sum2 += AD_Value[3*i+1] ;
		// sum3 += AD_Value[3*i+2] ;


	}
	if(tgs_flag)
	{
		test_tgs = AD_Value[AD_BUFSIZE-1];
//		for(i=0;i<(AD_BUFSIZE/3);i++)
//		{
//			sum3 += AD_Value[3*i+2] ;
//		}  
	}
	adc_filter[0] = (u16) (sum1/(AD_BUFSIZE/3)) ;
	adc_filter[1] = (u16) (sum2/(AD_BUFSIZE/3)) ;
	adc_filter[2] = (u16) (sum3/(AD_BUFSIZE/3)) ;
	gI_UsbOut = (u16)(adc_filter[0]*66*1000/4096);  // usb 输出电流 单位 mA
	gV_Battery = (u16)(adc_filter[1]*33*(75+49.9)/(4096*49.9));   // 电池电压  0.1V
	gGas  = (u16)((4095/(test_tgs*1.1))*100-160)>>1;  //  (Rs/Ro)*100
	//  Temperature = (1.42 - adc_filter[1]*3.3/4096)*1000/4.35 + 25;
	//   adc_filter[2] = (u16) (sum3/(AD_BUFSIZE));

}
void bsp_adc_config(void)
{
	sys_adc_init();
	sys_adc_dma_config();
	TimerTGSConfig(50,1439);  //1ms 中断
}
u16 CurrDataCounterEnd = 0;
void DMA1_Channel1_IRQHandler(void)
{
	if(DMA_GetITStatus(DMA1_IT_TC1) != RESET)
	{
		DMA_Cmd(DMA1_Channel1,DISABLE);
		CurrDataCounterEnd=DMA_GetCurrDataCounter(DMA1_Channel1);
		DMA_ClearITPendingBit(DMA1_IT_TC1);
		filter();		
		DMA_SetCurrDataCounter(DMA1_Channel1,AD_BUFSIZE);
		DMA_Cmd(DMA1_Channel1,ENABLE);
	}
}
 
void TIM4_IRQHandler(void)
{
	static u16 tgs_ticks = 0;
	
	if(TIM_GetITStatus(TIM4, TIM_IT_Update) != RESET ) /*检查TIM4更新中断发生与否*/
    {
		tgs_ticks++;
		if(tgs_ticks > 997)
		{
			tgs_flag = 1;
			TGS_PLUS_ENABLE;
		}
		if(tgs_ticks == 1000)
		{
			tgs_ticks = 0;
			TGS_PLUS_DISABLE;
			tgs_flag = 0;
		}
		
        TIM_ClearITPendingBit(TIM4, TIM_IT_Update); /*清除TIMx更新中断标志 */
	}	 
	
}