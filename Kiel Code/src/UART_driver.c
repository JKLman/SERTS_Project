
#include "cmsis_os.h"
#include "stdio.h"
#include "stdint.h"                     // data type definitions
#include "string.h"
#include "stm32f4xx.h"
#include "Driver_USART.h" // USART API

// This tested 10/9/2016 and works, can call send and
// receive functions at the same time. asynchronous

/* USART Driver */
extern ARM_DRIVER_USART Driver_USART4;
void USART_callback(uint32_t event); // USART callback

// Two semaphores, one to protect the serial port and one to block
// the calling thread until the send or receive is completed
osSemaphoreId sid_UART_SEM_Send_block; // semaphore id
osSemaphoreDef (UART_SEM_Send_block); // semaphore object
osSemaphoreId sid_UART_SEM_Send; // semaphore id
osSemaphoreDef (UART_SEM_Send); // semaphore object


osSemaphoreId sid_UART_SEM_Rec_block; // semaphore id
osSemaphoreDef (UART_SEM_Rec_block); // semaphore object
osSemaphoreId sid_UART_SEM_Rec; // semaphore id
osSemaphoreDef (UART_SEM_Rec); // semaphore object

int16_t UART_Init(void){

	// This is the semaphore associated with UART
	// Start a 0 so that the calling thread will block until data sent or received.
	sid_UART_SEM_Send_block = osSemaphoreCreate (osSemaphore(UART_SEM_Send_block), 0);
  if (!sid_UART_SEM_Send_block) {
    return(-1); // Semaphore object not created, handle failure
  }
	sid_UART_SEM_Send = osSemaphoreCreate (osSemaphore(UART_SEM_Send), 1);
  if (!sid_UART_SEM_Send) {
    return(-1); // Semaphore object not created, handle failure
  }
	sid_UART_SEM_Rec_block = osSemaphoreCreate (osSemaphore(UART_SEM_Rec_block), 0);
  if (!sid_UART_SEM_Rec_block) {
    return(-1); // Semaphore object not created, handle failure
  }
	sid_UART_SEM_Rec = osSemaphoreCreate (osSemaphore(UART_SEM_Rec), 1);
  if (!sid_UART_SEM_Rec) {
    return(-1); // Semaphore object not created, handle failure
  }
	// Initialize the driver
	Driver_USART4.Initialize(  USART_callback);
	/*Power up the USART peripheral */
	Driver_USART4.PowerControl(ARM_POWER_FULL);
	/*Configure the USART to 9600 Bits/sec */
	Driver_USART4.Control(ARM_USART_MODE_ASYNCHRONOUS |
										ARM_USART_DATA_BITS_8 | // driver only works for 8 bits ?
										ARM_USART_PARITY_NONE |
										ARM_USART_STOP_BITS_1 |
										ARM_USART_FLOW_CONTROL_NONE, 9600);

	/* Enable Receiver and Transmitter lines */
	Driver_USART4.Control (ARM_USART_CONTROL_TX, 1);
	Driver_USART4.Control (ARM_USART_CONTROL_RX, 1);
	//Driver_USART4.Send("\nPress Enter to receive a message", 34);
	
	return(NULL);
}

void UART_receivestring(char* user_data, uint16_t maxnum){
	char rx_char;
	int i;
	osSemaphoreWait(sid_UART_SEM_Rec, osWaitForever); // check resource is available
	for(i=0;i<maxnum;i++){
		Driver_USART4.Receive(&rx_char,1);
		osSemaphoreWait(sid_UART_SEM_Rec_block, osWaitForever); // block until receive done
		user_data[i] = rx_char;
		if(rx_char==0){
			break;
		}
	}	
	osSemaphoreRelease(sid_UART_SEM_Rec); // release resource
}


void UART_send(char* user_data, uint16_t num)
{
	ARM_USART_STATUS st; // variable for checking tx_busy
	
	osSemaphoreWait(sid_UART_SEM_Send, osWaitForever); // check resource is available
	Driver_USART4.Send(user_data,num);
	st = Driver_USART4.GetStatus();
	// Poll the driver to check the tx_busy status
	st = Driver_USART4.GetStatus();
	while(st.tx_busy == 1){
		osDelay(1);
		st = Driver_USART4.GetStatus();
	}
	osSemaphoreRelease(sid_UART_SEM_Send); // release resource
}

void UART_receive(char* user_data, uint16_t num)
{
	osSemaphoreWait(sid_UART_SEM_Rec, osWaitForever); // check resource is available
	Driver_USART4.Receive(user_data,num);
	osSemaphoreWait(sid_UART_SEM_Rec_block, osWaitForever); // block until send done
	osSemaphoreRelease(sid_UART_SEM_Rec); // release resource
}

void USART_callback(uint32_t event)
{
    switch (event)
    {
			case ARM_USART_EVENT_RECEIVE_COMPLETE:
				osSemaphoreRelease(sid_UART_SEM_Rec_block); // unblcok calling thread
        break;
			case ARM_USART_EVENT_TRANSFER_COMPLETE:
        break;
			case ARM_USART_EVENT_SEND_COMPLETE:

        break;
			case ARM_USART_EVENT_TX_COMPLETE:
        break;
			case ARM_USART_EVENT_RX_TIMEOUT:
        break;
			case ARM_USART_EVENT_RX_OVERFLOW:
        break;
			case ARM_USART_EVENT_TX_UNDERFLOW:
        break;
    }
}

