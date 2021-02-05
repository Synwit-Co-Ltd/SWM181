#include "SWM181.h"


volatile uint32_t Period = 0,	//PWM���ڳ���
	              LWidth = 0;	//PWM�͵�ƽ���

void SerialInit(void);
void TestSignal(void);

int main(void)
{	
	uint32_t i;
	
	SystemInit();
	
	SerialInit();
	
	TestSignal();	//���������źŹ�HALL���ܲ���
	
	PORT_Init(PORTA, PIN4, FUNMUX_HALL_A, 1);		//PA4 -> HALL_A
	
	TIMR_Init(TIMR0, TIMR_MODE_TIMER, 0xFFFFFFFF, 0);
	
	TIMRG->HALLCR &= ~TIMRG_HALLCR_IEA_Msk;
	TIMRG->HALLCR |= (3 << TIMRG_HALLCR_IEA_Pos);	//HALL_A˫���ز����ж�
	
	IRQ_Connect(IRQ0_15_HALL, IRQ4_IRQ, 1);
	
	TIMR_Start(TIMR0);
	
	while(1==1)
	{
//		while((Period == 0) || (LWidth == 0));		//���Բ鿴��һ�����������
		printf("Period = %d, HWidth = %d, Duty = %%%d\r\n", Period, Period-LWidth, (Period-LWidth)*100/Period);
		
		for(i = 0; i < 5000000; i++);
	}
}

void IRQ4_Handler(void)
{
	TIMRG->HALLSR = (1 << TIMRG_HALLSR_IFA_Pos);	//����жϱ�־
	
	if(TIMRG->HALLSR & TIMRG_HALLSR_STA_Msk)		//������
	{
		LWidth = 0xFFFFFFFF - TIMRG->HALL_A;
	}
	else											//�½���
	{
		TIMR0->CTRL = 0;
		TIMR0->CTRL = 1;							//���´�0xFFFFFFFF�ݼ�
		
		if(LWidth != 0)								//�Ѳ⵽������
			Period = 0xFFFFFFFF - TIMRG->HALL_A;
	}
}

void TestSignal(void)
{
	PWM_InitStructure  PWM_initStruct;
	
	PWM_initStruct.clk_div = PWM_CLKDIV_4;		//F_PWM = 24M/4 = 6M
	
	PWM_initStruct.mode = PWM_MODE_INDEP;		//A·��B·�������					
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
	
	PORT_Init(PORTA, PIN5, FUNMUX_PWM1B_OUT, 0);
	
	PWM_Start(PWM1, 1, 1);
}

void SerialInit(void)
{
	UART_InitStructure UART_initStruct;
	
	PORT_Init(PORTA, PIN0, FUNMUX_UART0_RXD, 1);	//GPIOA.0����ΪUART0��������
	PORT_Init(PORTA, PIN1, FUNMUX_UART0_TXD, 0);	//GPIOA.1����ΪUART0�������
 	
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
* ��������: fputc()
* ����˵��: printf()ʹ�ô˺������ʵ�ʵĴ��ڴ�ӡ����
* ��    ��: int ch		Ҫ��ӡ���ַ�
*			FILE *f		�ļ����
* ��    ��: ��
* ע������: ��
******************************************************************************************************************************************/
int fputc(int ch, FILE *f)
{
	UART_WriteByte(UART0, ch);
	
	while(UART_IsTXBusy(UART0));
 	
	return ch;
}
