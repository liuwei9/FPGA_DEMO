#include "xparameters.h"
#include "xaxidma.h"
#include "xscugic.h"
#include "xil_exception.h"
#include "xil_printf.h"

#define DMA_DEVICE_ID XPAR_AXIDMA_0_DEVICE_ID
#define RX_INTR_ID XPAR_FABRIC_AXIDMA_0_S2MM_INTROUT_VEC_ID
#define TX_INTR_ID XPAR_FABRIC_AXIDMA_0_MM2S_INTROUT_VEC_ID
#define SCUGIC_DEVICE_ID XPAR_PS7_SCUGIC_0_DEVICE_ID
#define DDR_BASE_ADDR XPAR_PS7_DDR_0_S_AXI_BASEADDR
#define MEM_BASE_ADDR (DDR_BASE_ADDR + 0x1000000)
#define TX_BUFFER_BASE (MEM_BASE_ADDR + 0x00100000)
#define RX_BUFFER_BASE (MEM_BASE_ADDR + 0x00300000)
#define MAX_PKT_LEN 0x100
#define RESET_TIMEOUT_COUNTER 10000

void rx_intr_handler(void * callbakRef);
void tx_intr_handler(void * callbakRef);

volatile int tx_done;
volatile int rx_done;
volatile int error;

int main()
{
	XAxiDma axidma;
	XAxiDma_Config *axidma_cfg;
	axidma_cfg = XAxiDma_LookupConfig(DMA_DEVICE_ID);
	XAxiDma_CfgInitialize(&axidma, axidma_cfg);

	XScuGic intc;
	XScuGic_Config *intc_cfg;
	intc_cfg = XScuGic_LookupConfig(SCUGIC_DEVICE_ID);
	XScuGic_CfgInitialize(&intc, intc_cfg, intc_cfg->CpuBaseAddress);

	Xil_ExceptionInit();
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT, (Xil_ExceptionHandler) XScuGic_InterruptHandler, &intc);
	Xil_ExceptionEnable();

	XScuGic_SetPriorityTriggerType(&intc, RX_INTR_ID, 0xA3, 0x3);
	XScuGic_SetPriorityTriggerType(&intc, TX_INTR_ID, 0xA3, 0x3);
	XScuGic_Connect(&intc, RX_INTR_ID, rx_intr_handler, &axidma);
	XScuGic_Connect(&intc, TX_INTR_ID, tx_intr_handler, &axidma);
	XScuGic_Enable(&intc, RX_INTR_ID);
	XScuGic_Enable(&intc, TX_INTR_ID);

	XAxiDma_IntrEnable(&axidma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DMA_TO_DEVICE);
	XAxiDma_IntrEnable(&axidma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);

	tx_done = 0;
	rx_done = 0;
	error = 0;

	u8* tx_buffer_ptr = (u8*) TX_BUFFER_BASE;
	u8* rx_buffer_ptr = (u8*) RX_BUFFER_BASE;
	for (int i = 0; i < MAX_PKT_LEN; ++i)
	{
		tx_buffer_ptr[i] = i&0xFF;
	}
	Xil_DCacheFlushRange((INTPTR) tx_buffer_ptr, MAX_PKT_LEN);
	XAxiDma_SimpleTransfer(&axidma, (UINTPTR) tx_buffer_ptr, MAX_PKT_LEN,XAXIDMA_DMA_TO_DEVICE);
	XAxiDma_SimpleTransfer(&axidma, (UINTPTR) rx_buffer_ptr, MAX_PKT_LEN,XAXIDMA_DEVICE_TO_DMA);
	Xil_DCacheFlushRange((UINTPTR) rx_buffer_ptr, MAX_PKT_LEN);

	while (!tx_done && !rx_done && !error)
		;

	if (error)
	{
		if (tx_done)
		{
			xil_printf("发送已完成\n\r");
		}
		else
		{
			xil_printf("发送失败\r\n");
		}
		if (rx_done)
		{
			xil_printf("接收已完成\r\n");
		}
		else
		{
			xil_printf("接收失败\r\n");
		}
		return 1;
	}
	for (int i = 0; i < MAX_PKT_LEN; ++i)
	{
		xil_printf("rx_buffer_ptr:%d\r\ntx_buffer_ptr:%d\r\n",rx_buffer_ptr[i], tx_buffer_ptr[i]);
		if (rx_buffer_ptr[i] != tx_buffer_ptr[i])
		{
			xil_printf("传输出现数据错误\r\n");
			xil_printf("rx_buffer_ptr:%d\r\ntx_buffer_ptr:%d\r\n",rx_buffer_ptr[i], tx_buffer_ptr[i]);
			return 2;
		}

	}
	xil_printf("传输成功\r\n");
	return 0;
}

void rx_intr_handler(void * callbakRef)
{
	xil_printf("rx_intr_handler\r\n");
	int timeout;
	XAxiDma * axidma = (XAxiDma*) callbakRef;
	u32 irq_status;
	irq_status = XAxiDma_IntrGetIrq(axidma, XAXIDMA_DEVICE_TO_DMA);
	XAxiDma_IntrAckIrq(axidma, irq_status, XAXIDMA_DEVICE_TO_DMA);
	if (irq_status & XAXIDMA_IRQ_ERROR_MASK)
	{
		error = 1;
		XAxiDma_Reset(axidma);
		timeout = RESET_TIMEOUT_COUNTER;
		while (timeout)
		{
			if (XAxiDma_ResetIsDone(axidma))
			{
				break;
			}
			timeout -= 1;
		}
		return;
	}
	if (irq_status & XAXIDMA_IRQ_IOC_MASK)
		rx_done = 1;
}

void tx_intr_handler(void * callbakRef)
{
	xil_printf("tx_intr_handler\r\n");
	int timeout;
	XAxiDma * axidma = (XAxiDma*) callbakRef;
	u32 irq_status;
	irq_status = XAxiDma_IntrGetIrq(axidma, XAXIDMA_DMA_TO_DEVICE);
	XAxiDma_IntrAckIrq(axidma, irq_status, XAXIDMA_DMA_TO_DEVICE);
	if (irq_status & XAXIDMA_IRQ_ERROR_MASK)
	{
		error = 1;
		XAxiDma_Reset(axidma);
		timeout = RESET_TIMEOUT_COUNTER;
		while (timeout)
		{
			if (XAxiDma_ResetIsDone(axidma))
			{
				break;
			}
			timeout -= 1;
		}
		return;
	}
	if (irq_status & XAXIDMA_IRQ_IOC_MASK)
		tx_done = 1;
}
