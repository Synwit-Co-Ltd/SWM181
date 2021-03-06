#include "SWM181.h"

#include "rtthread.h"


void TaskADC(void *arg);
void TaskPWM(void *arg);

struct rt_thread taskADC;
static uint8_t stackADC[256];
static uint8_t priorityADC = 4;

struct rt_thread taskPWM;
static uint8_t stackPWM[256];
static uint8_t priorityPWM = 5;

struct rt_mailbox queueADC;
static uint32_t mbPool[16];

void SerialInit(void);

int main(void)
{	
//	SystemInit();		在 rt_hw_board_init() 中调用
	
	SerialInit();
	
	rt_mb_init(&queueADC, "ADC", mbPool, sizeof(mbPool)/sizeof(mbPool[0]), RT_IPC_FLAG_FIFO);
		
	rt_thread_init(&taskADC, "ADC", TaskADC, RT_NULL, stackADC, sizeof(stackADC), priorityADC, 20);
	rt_thread_startup(&taskADC);
	
	rt_thread_init(&taskPWM, "PWM", TaskPWM, RT_NULL, stackPWM, sizeof(stackPWM), priorityPWM, 20);
	rt_thread_startup(&taskPWM);
	
	GPIO_Init(GPIOA, PIN5, 1, 0, 0, 0);
	
	while(1)
	{
		GPIO_InvBit(GPIOA, PIN5);
		
		rt_thread_delay(500 / (1000 / RT_TICK_PER_SECOND));
	}
}

/****************************************************************************************************************************************** 
* 函数名称:	TaskADC()
* 功能说明: 启动ADC采集任务
* 输    入: void *arg		传给任务的参数
* 输    出: 无
* 注意事项: 无
******************************************************************************************************************************************/
void TaskADC(void *arg)
{
	SDADC_InitStructure SDADC_initStruct;
	
	PORT_Init(PORTC, PIN2, PORTC_PIN2_SDADC_CH3P, 0);	//PC.2 => SDADC.CH3P
	
	SDADC_initStruct.clk_src = SDADC_CLKSRC_HRC_DIV8;
	SDADC_initStruct.channels = SDADC_CH3;
	SDADC_initStruct.out_cali = SDADC_OUT_CALIED;
	SDADC_initStruct.refp_sel = SDADC_REFP_AVDD;
	SDADC_initStruct.trig_src = SDADC_TRIGSRC_TIMR3;
	SDADC_initStruct.Continue = 0;						//非连续模式，即单次模式
	SDADC_initStruct.EOC_IEn = 1;	
	SDADC_initStruct.OVF_IEn = 0;
	SDADC_initStruct.HFULL_IEn = 0;
	SDADC_initStruct.FULL_IEn = 0;
	SDADC_Init(SDADC, &SDADC_initStruct);				//配置SDADC
	
	SDADC_Config_Set(SDADC, SDADC_CFG_A, SDADC_CFG_GAIN_1, 1, 1);
	SDADC_Config_Cali(SDADC, SDADC_CFG_A, SDADC_CALI_COM_GND, 0);
	SDADC_Config_Sel(SDADC, SDADC_CFG_A, SDADC_CH3);
	
	SDADC_Open(SDADC);									//使能SDADC
	
	IRQ_Connect(IRQ0_15_SDADC, IRQ5_IRQ, 1);
	
	TIMR_Init(TIMR3, TIMR_MODE_TIMER, SystemCoreClock/10, 0);	//每秒钟采样10次
	
	TIMR_Start(TIMR3);
	
	// 线程运行结束会在线程最后部分执行rt_thread_exit函数而更改为关闭状态
}

void IRQ5_Handler(void)
{
	int16_t res;
	uint32_t chn;
	
	SDADC_IntEOCClr(SDADC);			//清除中断标志
	
	res = SDADC_Read(SDADC, &chn);
	
	rt_mb_send(&queueADC, res + 32768);
}

/****************************************************************************************************************************************** 
* 函数名称:	TaskPWM()
* 功能说明: 等待ADC转换结果，根据ADC转换结果设置PWM占空比
* 输    入: void *arg		传给任务的参数
* 输    出: 无
* 注意事项: 无
******************************************************************************************************************************************/
void TaskPWM(void *arg)
{
	rt_uint32_t duty;
	PWM_InitStructure  PWM_initStruct;
	
	PWM_initStruct.clk_div = PWM_CLKDIV_4;		//F_PWM = 24M/4 = 6M
	
	PWM_initStruct.mode = PWM_MODE_INDEP;		//A路和B路独立输出					
	PWM_initStruct.cycleA = 10000;				//6M/10000 = 600Hz			
	PWM_initStruct.hdutyA =  2500;				//2500/10000 = 25%
	PWM_initStruct.initLevelA = 1;
	PWM_initStruct.cycleB = 10000;
	PWM_initStruct.hdutyB =  5000;				//5000/10000 = 50%
	PWM_initStruct.initLevelB = 1;
	PWM_initStruct.HEndAIEn = 0;
	PWM_initStruct.NCycleAIEn = 0;
	PWM_initStruct.HEndBIEn = 0;
	PWM_initStruct.NCycleBIEn = 0;
	
	PWM_Init(PWM1, &PWM_initStruct);
	
	PORT_Init(PORTB, PIN5, FUNMUX_PWM1B_OUT, 0);
	
	PWM_Start(PWM1, 1, 1);
	
	while(1)
	{
		if(rt_mb_recv(&queueADC, &duty, 100) == RT_EOK)
		{
			PWM_SetHDuty(PWM1, PWM_CH_A, 10000 * duty / 65536);
			PWM_SetHDuty(PWM1, PWM_CH_B, 10000 - PWM_GetHDuty(PWM1, PWM_CH_A));
		}
	}
}


void SerialInit(void)
{
	UART_InitStructure UART_initStruct;
	
	PORT_Init(PORTA, PIN0, FUNMUX_UART0_RXD, 1);	//GPIOA.0配置为UART0输入引脚
	PORT_Init(PORTA, PIN1, FUNMUX_UART0_TXD, 0);	//GPIOA.1配置为UART0输出引脚
 	
 	UART_initStruct.Baudrate = 57600;
	UART_initStruct.DataBits = UART_DATA_8BIT;
	UART_initStruct.Parity = UART_PARITY_NONE;
	UART_initStruct.StopBits = UART_STOP_1BIT;
	UART_initStruct.RXThresholdIEn = 0;
	UART_initStruct.TXThresholdIEn = 0;
	UART_initStruct.TimeoutIEn = 0;
 	UART_Init(UART0, &UART_initStruct);
	UART_Open(UART0);
}

/****************************************************************************************************************************************** 
* 函数名称: fputc()
* 功能说明: printf()使用此函数完成实际的串口打印动作
* 输    入: int ch		要打印的字符
*			FILE *f		文件句柄
* 输    出: 无
* 注意事项: 无
******************************************************************************************************************************************/
int fputc(int ch, FILE *f)
{
	UART_WriteByte(UART0, ch);
	
	while(UART_IsTXBusy(UART0));
 	
	return ch;
}
