
#include "cmsis_os.h"  									// CMSIS RTOS header file
#include "Board_LED.h"
#include "UART_driver.h"
#include "stdint.h"                     // data type definitions
#include "stdio.h"                      // file I/O functions
#include "rl_usb.h"                     // Keil.MDK-Pro::USB:CORE
#include "rl_fs.h"                      // Keil.MDK-Pro::File System:CORE
#include "stm32f4xx_hal.h"
#include "stm32f4_discovery.h"
#include "stm32f4_discovery_audio.h"
#include "math.h"
#include "arm_math.h" // header for DSP library
#include <stdio.h>

enum commands
{
	Play_Music = 1,
	Pause_Music = 2,
	Next_Song = 3,
	Previous_Song = 4,
	Get_Files = 5
};

enum state
{
	NoState,
	Standby,
	FileLoad,
	Playing,
	Pause
};

// WAVE file header format
typedef struct WAVHEADER {
	unsigned char riff[4];						// RIFF string
	uint32_t overall_size;				// overall size of file in bytes
	unsigned char wave[4];						// WAVE string
	unsigned char fmt_chunk_marker[4];		// fmt string with trailing null char
	uint32_t length_of_fmt;					// length of the format data
	uint16_t format_type;					// format type. 1-PCM, 3- IEEE float, 6 - 8bit A law, 7 - 8bit mu law
	uint16_t channels;						// no.of channels
	uint32_t sample_rate;					// sampling rate (blocks per second)
	uint32_t byterate;						// SampleRate * NumChannels * BitsPerSample/8
	uint16_t block_align;					// NumChannels * BitsPerSample/8
	uint16_t bits_per_sample;				// bits per sample, 8- 8bits, 16- 16 bits etc
	unsigned char data_chunk_header [4];		// DATA string or FLLR string
	uint32_t data_size;						// NumSamples * NumChannels * BitsPerSample/8 - size of the next chunk that will be read
} WAVHEADER;


// LED Definitions
#define LED_Green   0
#define LED_Orange  1
#define LED_Red     2
#define LED_Blue    3

// Audio Player Definitions
#define BUFFER1ID 1
#define BUFFER2ID 2

#define NUM_CHAN 2
#define NUM_POINTS 1024
#define BUF_LEN NUM_CHAN*NUM_POINTS

//Queue Definitions
#define MSGQUEUE_OBJECTS 1

//UART RX Definitions
#define Play_Files_char "1"
#define Pause_Files_char "2"
#define FF_Files_char "3"
#define RW_Files_char "4"
#define Get_Files_char "5"

//Global Variables
FILE *f;
int16_t Audio_Buffer_1[BUF_LEN];
int16_t Audio_Buffer_2[BUF_LEN];


//Semaphores
osSemaphoreDef(SEM0_);
osSemaphoreId(SEM0);

//Message Queues
osMessageQId mid_MsgQueue;
osMessageQDef (MsgQueue, MSGQUEUE_OBJECTS, int32_t);

osMessageQId mid_CMDQueue;
osMessageQDef(CMDQueue, 1, uint32_t);

//Prototypes
void FS (void const *arg);
void Control_Thread(void const *arg);
void RX_Command_Thread(void const *arg);
void Process_Event(uint16_t);

//Thread Handlers
osThreadDef(FS, osPriorityNormal, 1, 0);
osThreadDef(Control_Thread, osPriorityNormal, 1, 0);
osThreadDef(RX_Command_Thread, osPriorityNormal, 1, 0);

void Init_Thread (void) {

	//Create Thread IDs
	osThreadId FS_id;
	osThreadId RX_Command_Thread_id;
	osThreadId Control_Thread_id;
	
	//Run INIT routines
	LED_Initialize(); // Initialize the LEDs
	UART_Init(); // Initialize the UART
	
	//Thread Generation
	
	//Create FS Thread
  FS_id = osThreadCreate (osThread (FS), NULL);         // create the thread
  if (FS_id == NULL)
	{                                        
   ; // Failed to create a thread
  }
	
	//Create Control Thread
	Control_Thread_id = osThreadCreate(osThread (Control_Thread), NULL);
	if(Control_Thread_id == NULL)
	{
		;//Failed to create a new thread
	}
	
	//Create RX Command Thread
	RX_Command_Thread_id = osThreadCreate(osThread (RX_Command_Thread), NULL);
	if(RX_Command_Thread_id == NULL)
	{
		;//Failed to create a new thread
	}
	
	//Create a semaphore
	SEM0 = osSemaphoreCreate(osSemaphore(SEM0_), 0);

	//Create a message queue
	mid_MsgQueue = osMessageCreate(osMessageQ(MsgQueue), NULL);
	if(!mid_MsgQueue)return; //Queue creation failed, handle the failure

	//Create a command queueu
	mid_CMDQueue = osMessageCreate(osMessageQ(CMDQueue), NULL);
	if(!mid_CMDQueue)return; //Queue creation failed, handle the failure
	
}

void Process_Event(uint16_t event)
{
	//State Machine List
	//NoState,
	//Standby,
	//FileLoad,
	//Playing,
	//Pause
	
	static uint16_t Current_State = Standby;

	switch(Current_State)
	{
	
		case Standby:
			LED_On(LED_Blue);
			LED_Off(LED_Green);
			LED_Off(LED_Orange);
			LED_Off(LED_Red);
		
			if(event == Get_Files)
			{
				Current_State = FileLoad;
				LED_On(LED_Green);
				LED_Off(LED_Blue);
				LED_Off(LED_Orange);
				LED_Off(LED_Red);
			}
			if (event == Play_Music)
			{
				Current_State = Playing;
				LED_On(LED_Red);
				LED_Off(LED_Green);
				LED_Off(LED_Orange);
				LED_Off(LED_Blue);
			}

		break;
		
		case FileLoad:


			osDelay(3000); //Used to simulate loading files. Delete for final production
			Current_State = Standby;
		break;
		
		case Playing:

			if(event == Pause_Music)
			{
				Current_State = Pause;
				LED_On(LED_Orange);
				LED_Off(LED_Green);
				LED_Off(LED_Blue);
				LED_Off(LED_Red);
			}
		break;
		
		case Pause:

			if(event == Play_Music)
			{
				Current_State = Playing;
				LED_On(LED_Red);
				LED_Off(LED_Green);
				LED_Off(LED_Orange);
				LED_Off(LED_Blue);
			}
		break;
			
		default:
			;//Do nothing
			break;
			
	}	
}

void Control_Thread (void const *arg) 
	{
	osEvent evt;
	Process_Event(0);
	while(1)
	{
		evt = osMessageGet (mid_CMDQueue, osWaitForever);
		if(evt.status == osEventMessage) {
			Process_Event(evt.value.v);
		}
	}
}


void RX_Command_Thread(void const *arg) {
	char rx_char[2] = {0, 0};

  //Play_Music,
	//Pause_Music,
	//Next_Song,
	//Previous_Song,
	//Get_Files
	
	while (1)
		{
			UART_receive(rx_char, 1);
			
			if(!strcmp(rx_char, Play_Files_char))
			{			
				osMessagePut (mid_CMDQueue, Play_Music, osWaitForever);
			}
			else if(!strcmp(rx_char, Pause_Files_char))
			{
				osMessagePut (mid_CMDQueue, Pause_Music, osWaitForever);
			}
			else if(!strcmp(rx_char, FF_Files_char))
			{
				osMessagePut(mid_CMDQueue, Next_Song, osWaitForever);
			}
			else if(!strcmp(rx_char, RW_Files_char))
			{
				osMessagePut(mid_CMDQueue, Previous_Song, osWaitForever);
			}
			else if(!strcmp(rx_char, Get_Files_char))
			{
				osMessagePut(mid_CMDQueue, Get_Files, osWaitForever);
			}
		}
	}




void FS (void const *argument)
	{
	usbStatus ustatus; // USB driver status variable
	uint8_t drivenum = 0; // Using U0: drive number
	char *drive_name = "U0:"; // USB drive name
	fsStatus fstatus; // file system status variable
	//WAVHEADER header;
	static uint8_t rtrn = 0;
	int knownBuffer = BUFFER1ID;
	
	ustatus = USBH_Initialize (drivenum); // initialize the USB Host
	if (ustatus == usbOK)
		{
		// loop until the device is OK, may be delay from Initialize
		ustatus = USBH_Device_GetStatus (drivenum); // get the status of the USB device
		while(ustatus != usbOK)
		{
			ustatus = USBH_Device_GetStatus (drivenum); // get the status of the USB device
		}
		// initialize the drive
		fstatus = finit (drive_name);
		if (fstatus != fsOK)
		{
			// handle the error, finit didn't work
		} // end if
		// Mount the drive
		fstatus = fmount (drive_name);
		if (fstatus != fsOK)
		{
			// handle the error, fmount didn't work
		} // end if 
		// file system and drive are good to go
		f = fopen ("Test.wav","r");// open a file on the USB device
		if (f != NULL)
		{
			//fread((void *)&header, sizeof(header), 1, f);
			//Initialize the audio output
			rtrn = BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_AUTO, 0x46, 44100);
			if (rtrn != AUDIO_OK)return;
	
			//Load buffer 1 with information
			fread(Audio_Buffer_1, 2, BUF_LEN, f);
	
	
			BSP_AUDIO_OUT_Play((uint16_t *)Audio_Buffer_1, 2*BUF_LEN);
			while(!feof(f))
			{
				if(knownBuffer == BUFFER1ID)
				{
					knownBuffer = BUFFER2ID;
					//Load buffer 2 with information
					fread(Audio_Buffer_2, 2, BUF_LEN, f);
					
					osMessagePut(mid_MsgQueue, BUFFER1ID, osWaitForever);
					osSemaphoreWait(SEM0, osWaitForever);
				}
				else //knownBuffer == BUFFER2ID
				{
					knownBuffer = BUFFER1ID;
					//Load buffer 1 with information
					fread(Audio_Buffer_1, 2, BUF_LEN, f);
					
					osMessagePut(mid_MsgQueue, BUFFER2ID, osWaitForever);
					osSemaphoreWait(SEM0, osWaitForever);
				}
			}
			BSP_AUDIO_OUT_SetMute(AUDIO_MUTE_ON);
			fclose (f); // close the file
		} // end if file opened
	} // end if USBH_Initialize

} // end Thread

void BSP_AUDIO_OUT_TransferComplete_CallBack(void)
{
	osEvent evt;
	evt = osMessageGet (mid_MsgQueue, 0);
	if(evt.status == osEventMessage)
	{
		osSemaphoreRelease(SEM0);
		if(BUFFER1ID == evt.value.v)
		{
			BSP_AUDIO_OUT_ChangeBuffer((uint16_t*)Audio_Buffer_2, BUF_LEN);
		}
		else // BUFFER2ID == evt.value.v
		{
			BSP_AUDIO_OUT_ChangeBuffer((uint16_t*)Audio_Buffer_1, BUF_LEN);
		}
	}
}


/* This function is called when half of the requested buffer has been transferred. */
void BSP_AUDIO_OUT_HalfTransfer_CallBack(void)
{
	;
}


/* This function is called when an Interrupt due to transfer error or peripheral
   error occurs. */
void BSP_AUDIO_OUT_Error_CallBack(void)
{
	while(1)
	{
		;
	}
}
