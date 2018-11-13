
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


//Prototypes
void FS (void const *arg);

//Thread Handlers
osThreadDef (FS, osPriorityNormal, 1, 0);            // define Thread_1

void Init_Thread (void) {

	osThreadId id_FS; // holds the returned thread create ID
	
	LED_Initialize(); // Initialize the LEDs
	UART_Init(); // Initialize the UART
	
  id_FS = osThreadCreate (osThread (FS), NULL);         // create the thread
  if (id_FS == NULL)
	{                                        
    // Failed to create a thread
  };
	
	SEM0 = osSemaphoreCreate(osSemaphore(SEM0_), 0);
	mid_MsgQueue = osMessageCreate(osMessageQ(MsgQueue), NULL);
	if(!mid_MsgQueue)
	{
		;
	}
	
}



void FS (void const *argument)
	{
	usbStatus ustatus; // USB driver status variable
	uint8_t drivenum = 0; // Using U0: drive number
	char *drive_name = "U0:"; // USB drive name
	fsStatus fstatus; // file system status variable
	WAVHEADER header;
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
