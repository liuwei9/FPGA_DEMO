#include "xparameters.h"
#include "xgpio.h"
#include "xil_exception.h"
#include "xscugic.h"
#include "stdio.h"


#define SW_DEVICE_ID XPAR_SW_DEVICE_ID
#define LED_DEVICE_ID XPAR_LED_DEVICE_ID
#define XSCUGIC_DEVICE_ID XPAR_SCUGIC_0_DEVICE_ID
#define SW_INTERUPT_ID XPAR_FABRIC_SW_IP2INTC_IRPT_INTR
void inst_service(void * callbakRef);


XGpio SW_Gpio, LED_Gpio;
int main()
{
	printf("%d\r\n",320);

	XGpio_Initialize(&LED_Gpio, LED_DEVICE_ID);
	XGpio_Initialize(&SW_Gpio, SW_DEVICE_ID);
	XGpio_SetDataDirection(&LED_Gpio, 1, 0x00000000);
	XGpio_SetDataDirection(&SW_Gpio, 1, 0x00000003);
	XGpio_InterruptEnable(&SW_Gpio,0x00000003);
	XGpio_InterruptGlobalEnable(&SW_Gpio);

	XScuGic xscugic_inst;
	XScuGic_Config *scufig;
	scufig = XScuGic_LookupConfig(XSCUGIC_DEVICE_ID);
	XScuGic_CfgInitialize(&xscugic_inst, scufig,scufig->CpuBaseAddress);
	XScuGic_SetPriorityTriggerType(&xscugic_inst, SW_INTERUPT_ID, 0xA0, 0x1);
	XScuGic_Connect(&xscugic_inst, SW_INTERUPT_ID, inst_service, &SW_Gpio);
	XScuGic_Enable(&xscugic_inst, SW_INTERUPT_ID);

	Xil_ExceptionInit();
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT, (Xil_ExceptionHandler)XScuGic_InterruptHandler, &xscugic_inst);
	Xil_ExceptionEnable();

	while(1);
}

void inst_service(void * callbakRef)
{
	XGpio* sw = (XGpio*)callbakRef;
	XGpio_InterruptDisable(sw, 0x3);
	int key = 0;
	key = XGpio_DiscreteRead(sw, 1);
	XGpio_DiscreteWrite(&LED_Gpio, 1, key);
	XGpio_InterruptClear(sw, 0x3);
	XGpio_InterruptEnable(sw, 0x3);

}
