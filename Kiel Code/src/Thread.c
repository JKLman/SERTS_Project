
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
	Restart_Song = 4,
	Get_Files = 5,
	File_Transfer_Complete = 6,
	Resume_Music = 7,
	Stop_Music = 8
};

enum state
{
	NoState,
	Standby,
	FileLoad,
	Playing,
	Pause,
	NextSong,
	PreviousSong
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
#define Set_File_char "6"
#define Resume_File_char "7"

//UART TX Definitions
#define StartFileList_msg "9"
#define EndFileList_msg "8"

//Global Variables
FILE *f;
int16_t Audio_Buffer_1[BUF_LEN];
int16_t Audio_Buffer_2[BUF_LEN];
char requested_file_name[50];

bool newFile = false;


//Semaphores
osSemaphoreDef(SEM0_);
osSemaphoreId(SEM0);

//Message Queues
osMessageQId mid_MsgQueue;
osMessageQDef (MsgQueue, MSGQUEUE_OBJECTS, int32_t);

osMessageQId mid_CMDQueue;
osMessageQDef(CMDQueue, 1, uint32_t);

osMessageQId mid_FSQueue;
osMessageQDef(FSQueue, 1, uint32_t);

//Prototypes
void FS (void const *arg);
void Control_Thread(void const *arg);
void RX_Command_Thread(void const *arg);
void Process_Event(uint16_t);

//Thread Handlers
osThreadDef(FS, osPriorityNormal, 1, 0);
osThreadDef(Control_Thread, osPriorityNormal, 1, 0);
osThreadDef(RX_Command_Thread, osPriorityNormal, 1, 0);

void Init_Thread (void) 
{

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

	//Create a command queue
	mid_CMDQueue = osMessageCreate(osMessageQ(CMDQueue), NULL);
	if(!mid_CMDQueue)return; //Queue creation failed, handle the failure
	
	//Create a FS queue
	mid_FSQueue = osMessageCreate(osMessageQ(FSQueue), NULL);
	if(!mid_FSQueue)return; //Queue creation failed, handle the failure
	
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
			if(event == Get_Files)
			{
				Current_State = FileLoad;
				LED_On(LED_Blue);
				osMessagePut(mid_FSQueue, Get_Files, osWaitForever);
			}
			else if (event == Play_Music)
			{
				Current_State = Playing;
				osMessagePut(mid_FSQueue, Play_Music, osWaitForever);
			}

		break;
		
		case FileLoad:
			if(event == File_Transfer_Complete) //Wait untill the file transfer is complete
			{
				LED_Off(LED_Blue);
				Current_State = Standby;
			}
		break;
		
		case Playing:
			LED_On(LED_Green);
			if(event == Pause_Music)
			{
				Current_State = Pause;
				LED_Off(LED_Green);
				osMessagePut(mid_FSQueue, Pause_Music, osWaitForever);
			}
			else if(event == Next_Song)
			{
				Current_State = Standby;
				osMessagePut(mid_FSQueue, Stop_Music, osWaitForever);
			}
		break;
		
		case Pause:
			LED_On(LED_Red);
			if(event == Resume_Music)
			{
				Current_State = Playing;
				LED_Off(LED_Red);
				osMessagePut(mid_FSQueue, Resume_Music, osWaitForever);
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


void RX_Command_Thread(void const *arg)
{
	char rx_char[2] = {0, 0};
		while (1)
		{
			UART_receive(rx_char, 1);
			
			//Gets command to play music
			if(!strcmp(rx_char, Play_Files_char))
			{			
				osMessagePut (mid_CMDQueue, Play_Music, osWaitForever);
			}
			
			
			//Gets command to pause music
			else if(!strcmp(rx_char, Pause_Files_char))
			{
				osMessagePut (mid_CMDQueue, Pause_Music, osWaitForever);
			}
			
			
			//Gets command to fast foward music
			else if(!strcmp(rx_char, FF_Files_char))
			{
			  char tempName[50];
				UART_receivestring(tempName, 50);
				if(strcmp(tempName, requested_file_name))
				{
					strncpy(requested_file_name, tempName, 50);
					newFile = true;
					osMessagePut(mid_CMDQueue, Next_Song, osWaitForever);
				  osMessagePut (mid_CMDQueue, Play_Music, osWaitForever);
				}
				
			}
			
			
			//Gets command to rewind music
			else if(!strcmp(rx_char, RW_Files_char))
			{
				char tempName[50];
				UART_receivestring(tempName, 50);
				if(strcmp(tempName, requested_file_name))
				{
					strncpy(requested_file_name, tempName, 50);
					newFile = true;
					osMessagePut(mid_CMDQueue, NextSong, osWaitForever);
				  osMessagePut (mid_CMDQueue, Play_Music, osWaitForever);
				}
				
			}
			
			
			//Gets command to post files to GUI
			else if(!strcmp(rx_char, Get_Files_char))
			{
				osMessagePut(mid_CMDQueue, Get_Files, osWaitForever);
			}
			
			
			//Gets filename to play
			else if(!strcmp(rx_char, Set_File_char))
			{
				UART_receivestring(requested_file_name, 50);
			}
			
			
			//Gets command to resume music
			else if(!strcmp(rx_char, Resume_File_char))
			{
				osMessagePut(mid_CMDQueue, Resume_Music, osWaitForever);
			}
		}
	}




void FS (void const *argument)
{
	usbStatus ustatus; // USB driver status variable
	uint8_t drivenum = 0; // Using U0: drive number
	char *drive_name = "U0:"; // USB drive name
	fsStatus fstatus; // file system status variable
	static uint8_t rtrn = 0;
	int knownBuffer = BUFFER1ID;
	fsFileInfo info;
	osEvent evt;
	
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
		
		
		//From here, have the different things the FS can do
		
		
		while(1)
		{
			info.fileID=0;
			evt = osMessageGet (mid_FSQueue, osWaitForever);
			if(evt.status == osEventMessage)
			{
				switch(evt.value.v)
				{

					
					case Get_Files:
							// file system and drive are good to go
							UART_send(StartFileList_msg, 2);
							while(ffind("*.*", &info) == fsOK) //Look for .wav files to send back
							{
								int nameLength = 0;
								char * fileName;
								fileName = strcat(info.name, "\n");
								nameLength = strlen(fileName);

								UART_send(fileName, nameLength);
							}
							UART_send(EndFileList_msg, 2);
							osMessagePut(mid_CMDQueue, File_Transfer_Complete, osWaitForever);
					break;
							

							
							
					case Play_Music:
					knownBuffer = BUFFER1ID;
					f = fopen (requested_file_name,"r");// open a file on the USB device
					//f = fopen ("Test.wav","r");// open a file on the USB device
					if (f != NULL)
					{
						//fread((void *)&header, sizeof(header), 1, f);
						//Initialize the audio output
						rtrn = BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_AUTO, 0x46, 44100);
						if (rtrn != AUDIO_OK)return;
				
						//Load buffer 1 with information
						fread(Audio_Buffer_1, 2, BUF_LEN, f);
				
						BSP_AUDIO_OUT_SetMute(AUDIO_MUTE_OFF);
						
						BSP_AUDIO_OUT_Play((uint16_t *)Audio_Buffer_1, 2*BUF_LEN);
					}
					case Resume_Music:	
						if(evt.value.v == Resume_Music )
						{
							BSP_AUDIO_OUT_SetMute(AUDIO_MUTE_OFF);
							BSP_AUDIO_OUT_ChangeBuffer((uint16_t*)Audio_Buffer_1, BUF_LEN);
							knownBuffer = BUFFER1ID;
						}
						
						int end = 0;
						while(!end)
						{
								BSP_AUDIO_OUT_SetMute(AUDIO_MUTE_OFF);
								if(knownBuffer == BUFFER1ID)
								{
									knownBuffer = BUFFER2ID;
									//Load buffer 2 with information
									if(fread(&Audio_Buffer_2, 2, BUF_LEN, f) < BUF_LEN)
									{
										end = 1;
										break;
									}
									osMessagePut(mid_MsgQueue, BUFFER1ID, osWaitForever);
									osSemaphoreWait(SEM0, osWaitForever);
								}
								else //knownBuffer == BUFFER2ID
								{
									knownBuffer = BUFFER1ID;
									//Load buffer 1 with information
									if(fread(&Audio_Buffer_1, 2, BUF_LEN, f) < BUF_LEN)
									{
										end = 1;
										break;
									}
										
										osMessagePut(mid_MsgQueue, BUFFER2ID, osWaitForever);
										osSemaphoreWait(SEM0, osWaitForever);
								
								}
								
								evt = osMessageGet (mid_FSQueue, 0);
								if(evt.status == osEventMessage)
								{
									if(evt.value.v == Pause_Music)
									{
										BSP_AUDIO_OUT_SetMute(AUDIO_MUTE_ON);
										break;
									}
									else if (evt.value.v == Stop_Music)
									{
										BSP_AUDIO_OUT_SetMute(AUDIO_MUTE_ON);
										newFile = false;
										fclose(f);
										break;
									}

								}
								if(end)
								{
									BSP_AUDIO_OUT_SetMute(AUDIO_MUTE_ON);
									fclose(f);
								}
							}	
						break;	
						}
					}
			}
		}
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
