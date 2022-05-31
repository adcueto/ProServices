#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <sys/time.h>
#include <string.h>
#include <getopt.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>

#include <gre/greio.h>  /*Crank software headers*/
#include <combi_events.h> 

#ifdef WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h> // for usleep
#endif

#define COMBIOVEN_SEND_CHANNEL "combioven_frontend"
#define COMBIOVEN_RECEIVE_CHANNEL "combioven_backend"

#define SNOOZE_TIME 80

#define TRUE   1
#define FALSE -1

//G L O B A L   D E F I N E S
static int							dataChanged   = 1;    //Default to 1 so we send data to the ui once it connects
static int							runningState  = 0;
static char							previousState = 0;
static int 							fd;                 //To open uart-serial port
static int							nread;				//To get uart data lenght
static char  						buffer_Tx[128];     //Transmit data buffer to relay-board
static char							buffer_Rx[128];		//Receive data buffer from relay-board
static combioven_update_event_t	    combioven_state;
static relayboard_update_event_t	relayboard_state;
static recipe_info_event_t			recipe_active;
//static

#ifdef WIN32
static CRITICAL_SECTION lock;
static HANDLE thread1;relayboard_state.encoder_parameter == 1
static HANDLE thread2;
#else 
static pthread_mutex_t lock;
static pthread_t 	thread1;
static pthread_t    thread2;
#endif

/**
 * cross-platform function to create threads
 * @param start_routine This is the function pointer for the thread to run
 * @return 0 on success, otherwise an integer above 1
 */ 
int create_task(void *start_routine) {
#ifdef WIN32
	thread1 = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) start_routine, NULL, 0, NULL);
	if( thread1 == NULL ) {
		return 1;
	}

	thread2 = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) start_routine, NULL, 0, NULL);
	if( thread2 == NULL ) {
		return 1;
	}
	return 0;
#else
	return pthread_create( &thread1, NULL, start_routine, NULL);
	return pthread_create( &thread2, NULL, start_routine, NULL);
#endif
}

/**
 * cross platform mutex initialization
 * @return 0 on success, otherwise an integer above 1
 */ 
int init_mutex() {
#ifdef WIN32
	InitializeCriticalSection(&lock);
	return 0;
#else
	return pthread_mutex_init(&lock, NULL);
#endif
}

/**
 * cross platform mutex lock
 */ 
void lock_mutex() {
#ifdef WIN32
	EnterCriticalSection(&lock);
#else
	pthread_mutex_lock(&lock);
#endif
}

/**
 * cross platform mutex unlock
 */ 
void unlock_mutex() {
#ifdef WIN32
	LeaveCriticalSection(&lock);
#else
	pthread_mutex_unlock(&lock);
#endif
}

/**
 * cross-platform sleep
 */ 
void sleep_ms(int milliseconds) {
#ifdef WIN32
	Sleep(milliseconds);
#else
	usleep(milliseconds * 1000);
#endif
}

int convert_temperature(char unit, int temperature) {
	int converted_temp;

	if(unit == 'c') {
		converted_temp = (int)(((float)temperature - 32.0) * (5.0/9.0));
	} else if (unit == 'f') {
		converted_temp = (int)(((float)temperature * (9.0/5.0)) + 32.0);
	}
	return converted_temp;
}

void UART_Print(char *pData)
{
	write(fd, pData, strlen(pData) + 1);
	fd_set rd;
}

void Clear_Buffer(char *pData)
{
    memset(pData,0,sizeof(pData));
	tcflush(fd, TCIOFLUSH);
	nread = 0;
}

void 		Washing_Process(uint8_t modeSelected, uint8_t phaseStatus, uint32_t timeElapse);

/****************************************************************************************************************************
 *
 *                     D e f i n i t i o n   f o r   U A R T    r e c e i v e    t h r e a d <
 * 
 ****************************************************************************************************************************/
void *receive_uart_thread(void *arg) {
	
	printf("Opening UART port for receive\n");
	// Connect to UART port to receive messages
    int   nread;
	char  cpyValue[4] = {"000"};
	//char *ptr = NULL;

    memset(buffer_Rx,0,sizeof(buffer_Rx));
    tcflush(fd, TCIOFLUSH);
	
	fd_set rd;
	while (1)
	{
	    nread = read(fd, buffer_Rx, 8);
        //printf(" Rx: %s ,n:%d\n",buffer_Rx, nread);
        if ( nread == 8 && buffer_Rx[0]=='#')
        {
			lock_mutex();
			printf("Data received from UART %s\n", buffer_Rx);

		    if ( strstr((char*)buffer_Rx, MODE_PREHEAT ) != NULL ) {
				if (combioven_state.toggle_preheat == 1) {
					combioven_state.toggle_preheat 	= 0;

					printf("toggle_preheat");
					if((combioven_state.toggle_state == 3 || previousState == 3) && relayboard_state.completed_step == 0)
					{
						relayboard_state.completed_step=1;
					}
					
					else
					  combioven_state.toggle_state 	= 0;
				}
				
				printf(" Receive Change data to: %d\n",(uint8_t)combioven_state.toggle_preheat);
				Clear_Buffer(buffer_Rx);
				runningState=combioven_state.toggle_state;
				dataChanged = 1;
			}


			else if ( strstr((char*)buffer_Rx, MODE_COOLING ) != NULL ) {
				if (combioven_state.toggle_cooling == 1) {
					combioven_state.toggle_cooling 	= 0;
					combioven_state.toggle_state 	= 0;
					printf(buffer_Rx,"toggle_cooling ");
				}
				printf(" Receive Change data to: %d\n",(uint8_t)combioven_state.toggle_cooling);
			    Clear_Buffer(buffer_Rx);
				runningState=combioven_state.toggle_state;
				dataChanged = 1;
			}

			else if ( (strstr((char *)buffer_Rx, CURRENT_TEMP_PROBE ) != NULL ) && (combioven_state.toggle_probe == 1)) {
        		for(int i=5; i<8; i++)
        		{
        			if( buffer_Rx[i] == ' ' ){
        				cpyValue[i-5] = '0';
					}
        			else
            			cpyValue[i-5] = buffer_Rx[i];
        		}
        		relayboard_state.current_probe_temperature = atoi(cpyValue);
				sprintf(buffer_Rx,"Tpb:%3d",(uint16_t)relayboard_state.current_probe_temperature );
				printf("%s\n",buffer_Rx);
				if(relayboard_state.current_probe_temperature != combioven_state.current_probe)
				{
					combioven_state.current_probe = relayboard_state.current_probe_temperature;
					dataChanged = 1;
				}
				Clear_Buffer(buffer_Rx);
			}

			else if ( strstr((char *)buffer_Rx, CURRENT_HUMIDITY ) != NULL ) {
        		for(int i=5; i<8; i++)
        		{
        			if( buffer_Rx[i] == ' ' ){
        				cpyValue[i-5] = '0';
					}
        			else
            			cpyValue[i-5] = buffer_Rx[i];
        		}
        		relayboard_state.current_humidity = atoi(cpyValue);
				sprintf(buffer_Rx,"HR:%3d",(uint8_t)relayboard_state.current_humidity );
				printf("%s\n",buffer_Rx);
				if( (relayboard_state.current_humidity > combioven_state.current_humidity) || (relayboard_state.current_humidity > combioven_state.target_steam))
				{
					combioven_state.current_humidity = relayboard_state.current_humidity;
					dataChanged = 1;
				}
			    Clear_Buffer(buffer_Rx);
			}

			else if ( strstr((char *)buffer_Rx, CURRENT_CAM_TEMPERATURE ) != NULL ) {
        		for(int i=5; i<8; i++)
        		{
        			if( buffer_Rx[i] == ' ' ){
        				cpyValue[i-5] = '0';
					}
        			else
            			cpyValue[i-5] = buffer_Rx[i];
        		}
        		relayboard_state.current_cam_temperature = atoi(cpyValue);
			    Clear_Buffer(buffer_Rx);
			}

			else if ( strstr((char *)buffer_Rx, WARNING_MESSAGE	) != NULL ) {
        		uint16_t warningCode=0;
				for(int i=5; i<8; i++)
        		{
        			if( buffer_Rx[i] == ' ' ){
        				cpyValue[i-5] = '0';
					}
        			else
            			cpyValue[i-5] = buffer_Rx[i];
        		}

				warningCode = atoi(cpyValue);
				sprintf(buffer_Rx,"warning:%3d",(uint16_t)warningCode);
				printf("%s\n",buffer_Rx);
			    Clear_Buffer(buffer_Rx);
				if(warningCode != relayboard_state.warning_code){
					switch(warningCode)
					{
						case 0:		combioven_state.toggle_state=previousState;//Reset warning
									relayboard_state.warning_code=0;
									break;
						case 231:	previousState = combioven_state.toggle_state;
									combioven_state.toggle_state = 8;//Water supply error
									relayboard_state.warning_code = warningCode;
									break;
						case 111:	combioven_state.toggle_state = 9;
									relayboard_state.warning_code = warningCode;
									break;//Multipoint Thermocouple opened
						case 114:	combioven_state.toggle_state = 9;break;//Cold-Camera Thermocouple opened
						case 115:	combioven_state.toggle_state = 9;break;//Boiler Thermocouple opened
						case 116:	break;//Main-Camera Thermocouple opened
						case 510:   break;//Relayboard program failure
						case 511:	break;//Communication failure
						default: 	break;
					}	
				}
				runningState=combioven_state.toggle_state;
				sprintf(buffer_Rx,"prevS:%d",(uint8_t)previousState);
				printf("%s\n",buffer_Rx);
			    Clear_Buffer(buffer_Rx);
				dataChanged = 1;	
			}

			else if ( strstr((char *)buffer_Rx, DOOR_CLOSED ) != NULL ) {
        		relayboard_state.door_status=1;
				if( (runningState == 1) && (combioven_state.toggle_cooling == 0))
				{
					combioven_state.toggle_state=previousState;  
					sleep_ms(5);
					if(combioven_state.toggle_preheat){
						sprintf(buffer_Tx,MODE_PREHEAT);
			    		UART_Print(buffer_Tx);
						printf("%s\n",buffer_Tx);
					}

					else{
						sprintf(buffer_Tx,RUNNING_PROCESS);
			    		UART_Print(buffer_Tx);
						printf("%s\n",buffer_Tx);
					}
				}
				
				else if (runningState == 6){
					combioven_state.toggle_state=previousState;   
					if (recipe_active.typestep == 0 && previousState == 3)
					{
						relayboard_state.completed_step=1;
					}

					else if((combioven_state.toggle_state == 1) && (combioven_state.toggle_looptime==0) && (combioven_state.toggle_probe == 0))
					{
						sleep_ms(5);
						sprintf(buffer_Tx,MODE_PREHEAT);
			    		UART_Print(buffer_Tx);
						printf("%s\n",buffer_Tx);

					}

					else
					{
						sleep_ms(5);
						sprintf(buffer_Tx,RUNNING_PROCESS);
			    		UART_Print(buffer_Tx);
						printf("%s\n",buffer_Tx);
					}
				}

				else if(runningState == 7) //finish state
				{
					combioven_state.toggle_state=0;
				}
				runningState=combioven_state.toggle_state;
				dataChanged = 1;	
			}



			else if ( strstr((char *)buffer_Rx, DOOR_OPEN ) != NULL ) {
        		relayboard_state.door_status = 0;
				if(combioven_state.toggle_cooling == 0 && runningState >=1 && runningState <= 4 )
				{
					previousState = combioven_state.toggle_state;
					relayboard_state.encoder_activated = 8;   //STOP READ ENCODER
					combioven_state.toggle_state=6;  //GENERATE PAUSE 
					sleep_ms(5);
					sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
			    	UART_Print(buffer_Tx);
					printf("%s\n",buffer_Tx);
				}

				else if(runningState == 5) //finish state
				{
					previousState = runningState;
					sleep_ms(5);
					sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
			    	UART_Print(buffer_Tx);
					printf("%s\n",buffer_Tx);
					relayboard_state.completed_step = 1;
				}

				else if(runningState == 7) //finish state
				{
					combioven_state.toggle_state=0;
				}
				runningState=combioven_state.toggle_state;
				dataChanged = 1;
			}
			
			else if ( (strcmp(buffer_Rx, ENCODER_INCREASE ) == 0) && (relayboard_state.encoder_activated < 8) ){
				if (relayboard_state.encoder_parameter == 1) {
					if(combioven_state.target_steam >= MAX_TARGET_PERCENT)
					   combioven_state.target_steam = MAX_TARGET_PERCENT;
					else 
					   combioven_state.target_steam = combioven_state.target_steam + 1;
					Clear_Buffer(buffer_Rx);
					dataChanged = 1;
				}

				else if (relayboard_state.encoder_parameter == 2) {
				if(combioven_state.target_temperature >= MAX_TARGET_TEMPERATURE)
					   combioven_state.target_temperature = MAX_TARGET_TEMPERATURE;
				else 
					   combioven_state.target_temperature = combioven_state.target_temperature + 1;
					Clear_Buffer(buffer_Rx);
					dataChanged = 1;
				}

				else if (relayboard_state.encoder_parameter == 3) {
					if(combioven_state.target_time >= MAX_TARGET_TIME)
					   combioven_state.target_time = MAX_TARGET_TIME;
					else 
					   combioven_state.target_time = combioven_state.target_time + 2;
					Clear_Buffer(buffer_Rx);
					dataChanged = 1;
				}
				
				else if (relayboard_state.encoder_parameter == 4) {
					if(combioven_state.target_fanspeed >= MAX_TARGET_PERCENT)
					   combioven_state.target_fanspeed = MAX_TARGET_PERCENT;
					else 
					   combioven_state.target_fanspeed = combioven_state.target_fanspeed + 25;
					Clear_Buffer(buffer_Rx);
					dataChanged = 1;
				}

				else if (relayboard_state.encoder_parameter == 5) {
					if(combioven_state.target_probe >= 100)
					   combioven_state.target_probe = 100;
					else 
					   combioven_state.target_probe = combioven_state.target_probe + 1;
					Clear_Buffer(buffer_Rx);
					dataChanged = 1;
				}
				sprintf(buffer_Tx,ENCODER_ENABLE);
				UART_Print(buffer_Tx);
				printf("%s\n",buffer_Tx);
			}

			else if ( (strcmp(buffer_Rx, ENCODER_DECREASE ) == 0) && (relayboard_state.encoder_activated < 8) ){
				if (relayboard_state.encoder_parameter == 1) {					
					if(combioven_state.target_steam <= 0 )
					   combioven_state.target_steam = 0;
					else 
					   combioven_state.target_steam = combioven_state.target_steam - 1;
					Clear_Buffer(buffer_Rx);
					dataChanged = 1;
				}

				else if (relayboard_state.encoder_parameter == 2) {		
					if(combioven_state.target_temperature <= 30)
					   combioven_state.target_temperature = 30;
					else 
					   combioven_state.target_temperature = combioven_state.target_temperature - 1;
					Clear_Buffer(buffer_Rx);
					dataChanged = 1;
				}

				else if (relayboard_state.encoder_parameter == 3) {
					if(combioven_state.target_time <= 0)
					   combioven_state.target_time = 0;
					else
					   combioven_state.target_time = combioven_state.target_time - 2;
					Clear_Buffer(buffer_Rx);
					dataChanged = 1;
				}
				
				else if (relayboard_state.encoder_parameter == 4) {
					if(combioven_state.target_fanspeed <= 25)
					   combioven_state.target_fanspeed = 25;
					else 
					   combioven_state.target_fanspeed = combioven_state.target_fanspeed - 25;
					Clear_Buffer(buffer_Rx);
					dataChanged = 1;
				}

				else if (relayboard_state.encoder_parameter == 5) {
					if(combioven_state.target_probe <= 0)
					   combioven_state.target_probe = 0;
					else 
					   combioven_state.target_probe = combioven_state.target_probe - 1;
					Clear_Buffer(buffer_Rx);
					dataChanged = 1;
				}
				sprintf(buffer_Tx,ENCODER_ENABLE);
				UART_Print(buffer_Tx);
				printf("%s\n",buffer_Tx);
			}

			else if ( (strcmp(buffer_Rx, ENCODER_ZERO_POSITION ) == 0) && (relayboard_state.encoder_activated < 8)) {
				//printf("timE:%d\n",(uint8_t)relayboard_state.encoder_activated);
				Clear_Buffer(buffer_Rx);
				if( relayboard_state.encoder_activated == 6 )
				{
					relayboard_state.encoder_activated = 8;
					switch (relayboard_state.encoder_parameter)
					{
					case 1:		sprintf(buffer_Tx,"#stem%3d",(uint8_t)combioven_state.target_steam);
								UART_Print(buffer_Tx);
								break;
					case 2:		sprintf(buffer_Tx,"#temp%3d",(uint16_t)combioven_state.target_temperature);
								UART_Print(buffer_Tx);
								break;
					case 3:		sprintf(buffer_Tx,"timer:%d",(uint32_t)combioven_state.target_time);
								break;
					case 4:		sprintf(buffer_Tx,"#sped%3d",(uint8_t)combioven_state.target_fanspeed);
								UART_Print(buffer_Tx);
								break;
					case 5:		sprintf(buffer_Tx,"#tprb%3d",(uint16_t)combioven_state.target_probe);
								break;
					default:
						break;
					}
					printf("%s\n",buffer_Tx);
				}

				else 
				{
				   relayboard_state.encoder_activated = relayboard_state.encoder_activated + 1;
				}
				sleep_ms(1);
			}

			else {
				printf("undefined:%s\n",buffer_Rx);
				Clear_Buffer(buffer_Rx);
			}
			unlock_mutex();
		}


		else if( nread == 8 && buffer_Rx[0] != '#' )
    	{
    		Clear_Buffer(buffer_Rx);
    	}

		else if( nread == 2 )
    	{
			if ( strstr((char *)buffer_Rx, "Ok" ) != NULL ) {
				printf("Ok_COM\n");
			}
    		Clear_Buffer(buffer_Rx);
    	}
	}
}

/****************************************************************************************************************************
 *
 *                     D e f i n i t i o n   f o r   t h e   f r o n t   r e c e i v e    t h r e a d <
 * 
 ****************************************************************************************************************************/
void *receive_front_thread(void *arg) {
	gre_io_t					*handle;
	gre_io_serialized_data_t	*nbuffer = NULL;
	char *event_addr;
	char *event_name;
	char *event_format;
	void *event_data;
	int	  ret;
	int   nbytes;

	printf("Opening a channel for receive\n");
	// Connect to a channel to receive messages
	handle = gre_io_open(COMBIOVEN_RECEIVE_CHANNEL, GRE_IO_TYPE_RDONLY);
	if (handle == NULL) {
		fprintf(stderr, "Can't open receive channel\n");
		return 0;
	}

	nbuffer = gre_io_size_buffer(NULL, 100);

	while (1) {
		ret = gre_io_receive(handle, &nbuffer);
		if (ret < 0) {
			return 0;
		}

		event_name = NULL;
		nbytes = gre_io_unserialize(nbuffer, &event_addr, &event_name, &event_format, &event_data);
		if (!event_name) {
			printf("Missing event name\n");
			return 0;
		}

		printf("Received Event %s nbytes: %d format: %s\n", event_name, nbytes, event_format);

		lock_mutex();
		/*if (strcmp(event_name, INCREASE_TEMPERATURE_EVENT) == 0) {
			increase_temperature_event_t *uidata = (increase_temperature_event_t *)event_data;
			combioven_state.target_temperature = combioven_state.target_temperature + uidata->num;
			sprintf(buffer_Tx,"%d",(uint16_t) combioven_state.target_temperature);
			UART_Print(buffer_Tx);
			dataChanged = 1;
		} 
		
		else if (strcmp(event_name, DECREASE_TEMPERATURE_EVENT) == 0) {
			decrease_temperature_event_t *uidata = (decrease_temperature_event_t *)event_data;
			combioven_state.target_temperature = combioven_state.target_temperature - uidata->num;
			sprintf(buffer_Tx,"%d",(uint16_t) combioven_state.target_temperature);
			dataChanged = 1;
		} */ 
		
		
		if (strcmp(event_name, UPDATE_STEAM_EVENT) == 0) {
			update_steam_event_t *uidata = (update_steam_event_t *)event_data;
			combioven_state.target_steam = uidata->percent;
			sprintf(buffer_Tx,"#stem%3d",(uint8_t)combioven_state.target_steam);
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
			sleep_ms(5);
			dataChanged = 1;
		} 
		
		else if (strcmp(event_name, UPDATE_TEMPERATURE_EVENT) == 0) {
			update_temperature_event_t *uidata = (update_temperature_event_t *)event_data;
			combioven_state.target_temperature = uidata->usertemp;
			sprintf(buffer_Tx,"#temp%3d",(uint16_t)combioven_state.target_temperature);
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
			sleep_ms(5);
			dataChanged = 1;
		} 

		else if (strcmp(event_name, UPDATE_TIME_EVENT) == 0) {
			update_time_event_t *uidata = (update_time_event_t *)event_data;
			combioven_state.target_time = uidata->timer;
			combioven_state.toggle_looptime = 0;
			sprintf(buffer_Tx,"time:%d",(uint32_t)combioven_state.target_time);
			printf("%s\n",buffer_Tx);
			dataChanged = 1;
		}

		else if (strcmp(event_name, UPDATE_FANSPEED_EVENT) == 0) {
			update_fanspeed_event_t *uidata = (update_fanspeed_event_t *)event_data;
			combioven_state.target_fanspeed =  uidata->percent;
			sprintf(buffer_Tx,"#sped%3d",(uint8_t)combioven_state.target_fanspeed);
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
			sleep_ms(5);
			dataChanged = 1;
		}

		else if (strcmp(event_name, UPDATE_TEMP_PROBE_EVENT) == 0) {
			update_temp_probe_event_t* uidata = (update_temp_probe_event_t*)event_data;
			combioven_state.target_probe = uidata->usertemp;
			sprintf(buffer_Tx,"Tprobe:%3d\n",(uint16_t)combioven_state.target_probe);
			printf("%s\n",buffer_Tx);
			dataChanged = 1;
		}

		else if (strcmp(event_name, RECIPE_INFO_EVENT) == 0) {
			recipe_info_event_t* uidata = (recipe_info_event_t*)event_data;
			recipe_active.totalsteps 	= uidata->totalsteps;
			recipe_active.actualstep 	= uidata->actualstep;
			recipe_active.typestep	= uidata->typestep;
			sprintf(buffer_Tx,"steps:%d now:%d mode:%d",(uint8_t)recipe_active.totalsteps,(uint8_t)recipe_active.actualstep,(uint8_t)recipe_active.typestep);
			printf("%s\n",buffer_Tx);
			relayboard_state.completed_step=0;
			combioven_state.toggle_state = 0;
		}

		else if (strcmp(event_name, MODE_CONVECTION_EVENT) == 0) {
			sprintf(buffer_Tx,MODE_CONVECTION);
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
		}

		else if (strcmp(event_name, MODE_COMBINED_EVENT) == 0) {
			sprintf(buffer_Tx,MODE_COMBINED);
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
		}

		else if (strcmp(event_name, MODE_STEAM_EVENT) == 0) {
			sprintf(buffer_Tx,MODE_STEAM);
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
		}

		else if (strcmp(event_name, MODE_LOAD_EVENT) == 0) {
			relayboard_state.completed_step = 0;
		}

		else if (strcmp(event_name, TOGGLE_SPRAY_EVENT) == 0) {
			if ((runningState >=1 && runningState <= 3) && (combioven_state.toggle_cooling ==0)) {
				sprintf(buffer_Tx,WATER_SPRAY_SHOT);
				UART_Print(buffer_Tx);
				printf("%s\n",buffer_Tx);
			}
		}

		else if (strcmp(event_name, ENABLE_ENCODER_EVENT) == 0) {
			enable_encoder_event_t* uidata = (enable_encoder_event_t*)event_data;
			relayboard_state.encoder_parameter = uidata->parameter;
			relayboard_state.encoder_activated = 0;
			sprintf(buffer_Tx,ENCODER_ENABLE);
			UART_Print(buffer_Tx); 
			printf("%s Sel: %d E: %d\n",buffer_Tx,(uint8_t)relayboard_state.encoder_parameter,(uint8_t)relayboard_state.encoder_activated);
			sleep_ms(5);			
		}


//************************************************************************************//	
//		E V E N T S    W I T H    S T A R T  / S T O P   F U N C T I O N A L I T Y    //
//************************************************************************************//

		else if (strcmp(event_name, TOGGLE_MANUAL_EVENT) == 0) {
			if (runningState != 2) {
				sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
			    UART_Print(buffer_Tx);
				printf("%s\n",buffer_Tx);
				sleep_ms(5);
				combioven_state.toggle_cooling	= 0;
				combioven_state.toggle_preheat	= 0;
				if(relayboard_state.door_status==1){
					sprintf(buffer_Tx,RUNNING_PROCESS);
			    	UART_Print(buffer_Tx);
					printf("%s\n",buffer_Tx);
					combioven_state.toggle_state  = 2;
					combioven_state.current_temperature = 0;
					sleep_ms(2);
				}
				else{
					combioven_state.toggle_state	= 6;	//Set UI message Close Door
					previousState=2;						//When Door is close will run
				}
			} 
			else {
				combioven_state.toggle_state = 0;
				sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
			    UART_Print(buffer_Tx);
				printf("%s\n",buffer_Tx);
			}
			printf("Change data to: %d\n",(uint8_t)combioven_state.toggle_state);
			dataChanged = 1;
		} 

		else if (strcmp(event_name, TOGGLE_AUTOMATIC_EVENT) == 0) {
			if (runningState != 3) {
				sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
			    UART_Print(buffer_Tx);
				printf("%s\n",buffer_Tx);
				sleep_ms(5);
				combioven_state.toggle_state = 3;
				if(recipe_active.typestep == 1)  //0: Wait_User action / 1: Preheat/Cooling / 2: Time 
				{
					sprintf(buffer_Tx,MODE_PREHEAT);
			    	UART_Print(buffer_Tx);
					printf("%s\n",buffer_Tx);
					combioven_state.toggle_preheat  = 1;
					combioven_state.current_temperature = 0;
				}
				
				else if(recipe_active.typestep == 2)
				{
					sprintf(buffer_Tx,RUNNING_PROCESS);
			    	UART_Print(buffer_Tx);
					printf("%s\n",buffer_Tx);
					relayboard_state.completed_step = 0;
				}

				else if(recipe_active.typestep == 3)
				{
					sprintf(buffer_Tx,RUNNING_PROCESS);
			    	UART_Print(buffer_Tx);
					printf("%s\n",buffer_Tx);
					relayboard_state.completed_step = 0;
					combioven_state.toggle_probe  = 1;
					combioven_state.current_probe = 0;
					combioven_state.current_temperature = 0;
				}

				else
				{
					sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
			    	UART_Print(buffer_Tx);
					printf("%s\n",buffer_Tx);
					combioven_state.toggle_preheat  = 5;
				}
				sleep_ms(5);
			} 

			else {
				combioven_state.toggle_state = 0;
				sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
			    UART_Print(buffer_Tx);
				printf("%s\n",buffer_Tx);
			}
			printf("Change data to: %d\n",(uint8_t)combioven_state.toggle_state);
			dataChanged = 1;
		} 

		else if (strcmp(event_name, TOGGLE_WASHING_EVENT) == 0) {
			if (runningState != 4) {
				sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
				UART_Print(buffer_Tx);
				printf("%s\n", buffer_Tx);
				sleep_ms(5);
				sprintf(buffer_Tx, MODE_WASHING);
				UART_Print(buffer_Tx);
				printf("%s\n",buffer_Tx);
			}
			else {
				combioven_state.toggle_state = 0;
				sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
				UART_Print(buffer_Tx);
				printf("%s\n",buffer_Tx);
			}
			printf("Change data to: %d\n",(uint8_t)combioven_state.toggle_state);
			dataChanged = 1;
		}

		else if (strcmp(event_name, UPDATE_WASH_CYCLE_EVENT) == 0) {
			update_wash_cycle_event_t* uidata = (update_wash_cycle_event_t*)event_data;
			relayboard_state.washing_mode = uidata->usercycle;
			relayboard_state.washing_phase = 0;
			combioven_state.toggle_state   = 4;
			sprintf(buffer_Tx,RUNNING_PROCESS);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
			printf("cycle:%d length:%d\n",(uint8_t)relayboard_state.washing_mode,(uint32_t)combioven_state.target_time);
			dataChanged = 1;
		}

		else if (strcmp(event_name, TOGGLE_FINISHED_EVENT) == 0) {
			if (runningState == 7) {
				sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
				UART_Print(buffer_Tx);
				printf("%s\n", buffer_Tx);
				combioven_state.toggle_cooling	= 0;
				combioven_state.toggle_looptime = 0;
				combioven_state.toggle_probe	= 0;
				combioven_state.toggle_preheat	= 0;
				combioven_state.toggle_state	= 0;
			}
			sleep_ms(5);
			printf("Change data to: %d\n",(uint8_t)combioven_state.toggle_state);
			dataChanged = 1;
		}


		else if (strcmp(event_name, TOGGLE_PREHEAT_EVENT) == 0) {
			if (combioven_state.toggle_preheat == 0) {
				sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
			    UART_Print(buffer_Tx);
				printf("%s\n",buffer_Tx);
				combioven_state.toggle_cooling 	= 0;
				combioven_state.toggle_looptime = 0;
				combioven_state.toggle_probe    = 0;
				combioven_state.current_temperature = 0;
				if(relayboard_state.door_status==1){
					sprintf(buffer_Tx,MODE_PREHEAT);
			    	UART_Print(buffer_Tx);
					printf("%s\n",buffer_Tx);
					combioven_state.toggle_preheat  = 1;
					combioven_state.toggle_state	= 1;
					sleep_ms(2);
				}
				else{
					combioven_state.toggle_state	= 6;	//Set UI message Close Door
					combioven_state.toggle_preheat  = 1;	//Set UI Toggle_Preheat but message Close Door
					previousState=1;						//When Door is close will run
				}
			} 

			else {
				combioven_state.toggle_preheat = 0;
				combioven_state.toggle_state   = 0;
				sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
			    UART_Print(buffer_Tx);
				printf("%s\n",buffer_Tx);
			}
			sleep_ms(5);
			printf("Change data to: %d\n",(uint8_t)combioven_state.toggle_preheat);
			dataChanged = 1;
		}

		else if (strcmp(event_name, TOGGLE_COOLING_EVENT) == 0) {
			if (combioven_state.toggle_cooling == 0) {
				sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
				UART_Print(buffer_Tx);
				printf("%s\n", buffer_Tx);
				combioven_state.toggle_preheat 	= 0;
				combioven_state.toggle_looptime = 0;
				combioven_state.toggle_probe    = 0;
				combioven_state.current_temperature = 0;
				combioven_state.target_fanspeed = 100;
				combioven_state.target_temperature = 60;
				sprintf(buffer_Tx,"#temp%3d",(uint16_t)combioven_state.target_temperature);
				UART_Print(buffer_Tx);
				sleep_ms(2);
				sprintf(buffer_Tx,"#sped%3d",(uint8_t)combioven_state.target_fanspeed);
				UART_Print(buffer_Tx);
				printf("%s\n", buffer_Tx);
				sleep_ms(2);
				combioven_state.toggle_cooling  = 1;
				combioven_state.toggle_state    = 1;
				combioven_state.current_temperature = 300;	
				sprintf(buffer_Tx,MODE_COOLING);
			    UART_Print(buffer_Tx);
				printf("%s\n",buffer_Tx);
			} 
			
			else {
				combioven_state.toggle_cooling 	= 0;
				combioven_state.toggle_state    = 0;
				sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
			    UART_Print(buffer_Tx);
				printf("%s\n",buffer_Tx);
			}
			sleep_ms(5);
			printf("Change data to: %d\n",(uint8_t)combioven_state.toggle_cooling);
			dataChanged = 1;
		}


		else if (strcmp(event_name, TOGGLE_PROBE_EVENT) == 0) {
			if (combioven_state.toggle_probe == 0) {
				sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
				UART_Print(buffer_Tx);
				printf("%s\n", buffer_Tx);
				sleep_ms(2);
				combioven_state.toggle_looptime = 0;
				combioven_state.toggle_cooling	= 0;
				combioven_state.target_time		= 0;
				combioven_state.toggle_preheat	= 0;
				combioven_state.current_temperature = 0;
				combioven_state.current_probe = 0;
				if(relayboard_state.door_status==1){
					sprintf(buffer_Tx,RUNNING_PROCESS);
			    	UART_Print(buffer_Tx);
					printf("%s\n",buffer_Tx);
					combioven_state.toggle_probe  = 1;
					combioven_state.toggle_state  = 1;
					sleep_ms(2);
				}
				else{
					combioven_state.toggle_state	= 6;	//Set UI message Close Door
					combioven_state.toggle_preheat  = 1;	//Set UI Toggle_Preheat but message Close Door
					previousState=1;						//When Door is close will run
				}
			}
			else {
				combioven_state.toggle_probe = 0;
				combioven_state.toggle_state = 0;
				sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
				UART_Print(buffer_Tx);
				printf("%s\n",buffer_Tx);
			}
			sleep_ms(5);
			printf("Change data to: %d\n",(uint8_t)combioven_state.toggle_probe);
			dataChanged = 1;
		}


		else if (strcmp(event_name, TOGGLE_LOOPTIME_EVENT) == 0) {
			if (combioven_state.toggle_looptime == 0) {
				sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
			    UART_Print(buffer_Tx);
				printf("%s\n",buffer_Tx);
				combioven_state.toggle_cooling 	= 0;
				combioven_state.toggle_preheat  = 0;
				sleep_ms(5);
				combioven_state.toggle_looptime = 1;
				combioven_state.toggle_state    = 1;
				sprintf(buffer_Tx,RUNNING_PROCESS);
			    UART_Print(buffer_Tx);
				printf("%s\n",buffer_Tx);
			} else {
				combioven_state.toggle_looptime = 0;
				combioven_state.toggle_state 	= 0;
				sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
			    UART_Print(buffer_Tx);
				printf("%s\n",buffer_Tx);
			}
			sleep_ms(5);
			printf("Change data to: %d\n",(uint8_t)combioven_state.toggle_looptime);
			dataChanged = 1;
		} 	
		
		/*		
		else if (strcmp(event_name, TOGGLE_UNITS_EVENT) == 0) {
			if (combioven_state.units == 0) {
				//Celsius
				combioven_state.units = 1;
				combioven_state.target_temperature = convert_temperature('c', combioven_state.target_temperature);
				//relayboard_state.current_cam_temperature = convert_temperature('c', combioven_state.current_temperature);
			} else {
				//Farenheit
				combioven_state.units = 0;
				combioven_state.target_temperature = convert_temperature('f', combioven_state.target_temperature);
				//relayboard_state.current_cam_temperature = convert_temperature('f', combioven_state.current_temperature);
			}
			dataChanged = 1;
		}  */
		runningState=combioven_state.toggle_state;
		unlock_mutex();
	}

	//Release the buffer memory, close the send handle
	gre_io_free_buffer(nbuffer);
	gre_io_close(handle);
}



/*****************************************************************************************************************************
 * 
 *                     	M   A   I   N     &      U  I     U  P   D  A  T  E       T   H   R   E   A  D
 * 
 * ****************************************************************************************************************************/

int main(int argc, char **argv) {
	gre_io_t					*send_handle;
	gre_io_serialized_data_t	*nbuffer = NULL;
	combioven_update_event_t 	event_UI_data;    //Events from UI frontend
	relayboard_update_event_t 	event_IO_data;    //Events froms IO backend
	int 						ret;
	time_t						timer = time(NULL);
	double						seconds;
	int 						initFlag = 255;
	char 						*ptr = NULL;
	

	//serial commuication variables
    int nread=0, n=0,i=0;
	char buffer[32];
    struct termios oldtio,newtio;
	speed_t speed = B115200;
    fd = open("/dev/ttymxc2", O_RDWR | O_NONBLOCK| O_NOCTTY | O_NDELAY); 
    if (fd < 0)	{
        printf("Can't Open Serial Port!\n");
        exit(0);	
    }
	//get current serial port settings and save
    tcgetattr(fd,&oldtio);
    bzero(&newtio,sizeof(newtio));
    newtio.c_cflag = speed|CS8|CLOCAL|CREAD;
    newtio.c_cflag &= ~CSTOPB;
    newtio.c_cflag &= ~PARENB;
    newtio.c_iflag = IGNPAR;  
    newtio.c_oflag = 0;
    tcflush(fd,TCIFLUSH);  
    tcsetattr(fd,TCSANOW,&newtio);     //commit the serial port 
    tcgetattr(fd,&oldtio);

	//HANDSHAKE COMMUNICATION WITH RELAYBOARD

	memset(buffer,0,sizeof(buffer));
    char test[3]="Ok";
	printf("Serial COM relayboard init: %s\n",test); 
	//FOR TEST WHITOUT HANDSHAKE 

	write(fd, test, strlen(test) + 1);
	fd_set rd;
    while(1){
		//FD_ZERO(&rd);
	    //FD_SET(fd,&rd);
	    //ret = select(fd+1,&rd,NULL,NULL,NULL);
	    nread = read(fd, &buffer[n], 1);
        if (strlen(test) == strlen(buffer))
        {
			initFlag=strcmp(buffer,test);
			if (initFlag == 0)
			{
				printf("Conection with relay board finished: %s\n",buffer);
            	memset(buffer,0,sizeof(buffer));
            	tcflush(fd, TCIOFLUSH);
				break;
			}
			else
			{
				memset(buffer,0,sizeof(buffer));
            	tcflush(fd, TCIOFLUSH);
				write(fd, test, strlen(test) + 1);
	            fd_set rd;
				sleep_ms(200);
			}
        }
		else 
		{
			UART_Print(test);
			sleep_ms(200);
		}
        n += nread;
	} 
    //close(fd);  //Close port '/dev/ttymxc2'
    //end of serial port communication initialize
    // END OF TESTING WITHOUT HANDSHAKE

	//allocate memory for the thermostat state
	memset(&combioven_state, 0, sizeof(combioven_state));
	memset(&relayboard_state, 0, sizeof(relayboard_state));
	memset(&recipe_active, 0, sizeof(recipe_active));
	//set initial state of frontend/backend structures 
	relayboard_state.current_cam_temperature   	= 0;	
	relayboard_state.current_probe_temperature 	= 0;
	relayboard_state.current_humidity  			= 0;
	relayboard_state.encoder_parameter 			= 0;
	relayboard_state.encoder_activated 			= 8;
	relayboard_state.washing_mode				= 0;
	relayboard_state.washing_phase				= 0;
	relayboard_state.door_status				= 1;
	relayboard_state.completed_step				= 0;
	relayboard_state.last_step					= 0;
	relayboard_state.warning_code				= 0;	
	combioven_state.target_temperature			= 30;
	combioven_state.target_steam    			= 0;
	combioven_state.target_time     			= 0;
	combioven_state.target_fanspeed 			= 25;
	combioven_state.target_probe    			= 0;
	combioven_state.current_probe   			= 0;
	combioven_state.current_humidity			= 0;
	combioven_state.toggle_preheat  			= 0;
	combioven_state.toggle_cooling  			= 0;
	combioven_state.toggle_state    			= 0;
	combioven_state.toggle_probe    			= 0;
	combioven_state.toggle_looptime 			= 0;
	recipe_active.totalsteps					= 0;
	recipe_active.actualstep					= 0;		
	recipe_active.typestep						= 0;
	//combioven_state.units = 1; //0-Farenheit 1-Celsius

	if (init_mutex() != 0) {
		fprintf(stderr,"Mutex init failed\n");
		return 0;
	}
   // open port


	printf("Trying to open the connection to the frontend\n");
	while(1) {
	 // Connect to a channel to send messages (write)
		sleep_ms(SNOOZE_TIME);
		send_handle = gre_io_open(COMBIOVEN_SEND_CHANNEL, GRE_IO_TYPE_WRONLY);
		if(send_handle != NULL) {
			printf("Send channel: %s successfully opened\n", COMBIOVEN_SEND_CHANNEL);
			break;
		}
	}

	if (create_task(receive_front_thread) != 0) {
		fprintf(stderr,"Front fhread create failed\n");
		return 0;
	}

	memset(&event_UI_data, 0, sizeof(event_UI_data));

	if (create_task(receive_uart_thread) != 0) {
		fprintf(stderr,"UART thread create failed\n");
		return 0;
	}

	memset(&event_UI_data, 0, sizeof(event_IO_data));
/*
	toggle_state :  
	0 = stop
	1 = running free time / preheat / cooling / looptime / external probe
	2 = running manual mode with time 
	3 = running automatic by steps Ok
	4 = running washing Ok
	5 = ready for next step
	6 = pause / cerrar puerta
	7 = finished state
	8 = conectar agua
	9 = warning state
*/
	while(1) {
		sleep_ms(SNOOZE_TIME);
		seconds = difftime(time(NULL),timer);

	    lock_mutex();
		//printf("state:%d step:%d completed:%d\n",(uint8_t) runningState, (uint8_t)recipe_active.actualstep, (uint8_t)relayboard_state.completed_step);
		//running free time / preheat / cooling / looptime
		if( (seconds > 4) && (runningState == 0) && (relayboard_state.encoder_activated == 8))
		{
			sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
			recipe_active.actualstep=0;
			combioven_state.toggle_preheat =0;
			timer = time(NULL);
		}

		else if ( (seconds > 1) && (runningState == 1) ) //
		{
			if ( (combioven_state.toggle_preheat == 1) && (relayboard_state.door_status==1) && (relayboard_state.current_cam_temperature != combioven_state.current_temperature))
			{
				combioven_state.current_temperature = relayboard_state.current_cam_temperature;
				printf("heat:%3d\n",(uint16_t)combioven_state.current_temperature);
				dataChanged = 1;
			}
		
			else if (combioven_state.toggle_cooling == 1)
			{
				combioven_state.current_temperature = relayboard_state.current_cam_temperature;
				printf("cool:%3d\n",(uint16_t)combioven_state.current_temperature);
				dataChanged = 1;
			}

			else if ((combioven_state.toggle_probe == 1) && (relayboard_state.door_status==1))
			{		
				combioven_state.current_temperature = relayboard_state.current_cam_temperature;
				if((combioven_state.current_probe>=combioven_state.target_probe) && (combioven_state.current_temperature>combioven_state.target_temperature)){
					combioven_state.toggle_state = 7;
					combioven_state.toggle_probe = 0;
					runningState = combioven_state.toggle_state;
					sprintf(buffer_Tx,FINISHED_PROCESS);
			    	UART_Print(buffer_Tx);
					printf("%s\n",buffer_Tx);	
					dataChanged=1;				
				}

				else 
				{
					sprintf(buffer_Tx,GET_EXTERN_PROBE_TEMP);
			    	UART_Print(buffer_Tx);
					printf("%s\n",buffer_Tx);
				}
			}
			timer = time(NULL);
		}
		
		//running manual mode with time Ok
		else if ( (seconds > 0.5) && (runningState == 2) && (combioven_state.toggle_looptime != 1) && (relayboard_state.door_status==1)) {
			if (combioven_state.target_time > 0){
				combioven_state.target_time -= 1;
				printf("seconds: %d\n",(uint32_t)combioven_state.target_time);
			}

			else {
				combioven_state.toggle_state = 7;
				runningState = combioven_state.toggle_state;
				sprintf(buffer_Tx,FINISHED_PROCESS);
			    UART_Print(buffer_Tx);
				printf("%s\n",buffer_Tx);
			}
			timer = time(NULL);
			dataChanged = 1;
		}

		//running automatic by steps Ok
		else if ((seconds > 1) && (runningState == 3) && (relayboard_state.door_status==1))
		{
			if(recipe_active.typestep == 0)
			{
				if( relayboard_state.completed_step==1 ){
					combioven_state.toggle_state = 5;
					relayboard_state.completed_step=0;
					runningState = combioven_state.toggle_state;
					printf("next_step..\n");
					timer = time(NULL);
					dataChanged = 1;
					sleep_ms(500);
				}
			}

			else if(recipe_active.typestep == 1)
			{
				if( relayboard_state.completed_step==1 ){
					combioven_state.toggle_state = 5;
					relayboard_state.completed_step=0;
					runningState = combioven_state.toggle_state;
					printf("next_step..\n");
					timer = time(NULL);
					dataChanged = 1;
					sleep_ms(500);
				}

				else if ((relayboard_state.completed_step == 0) && (relayboard_state.current_cam_temperature != combioven_state.current_temperature)) {
					combioven_state.current_temperature = relayboard_state.current_cam_temperature;
					printf("nowtemp:%d\n",(uint16_t)combioven_state.current_temperature);
					dataChanged = 1;
				}
			}
			
			else if(recipe_active.typestep == 2)
			{
				if (combioven_state.target_time > 0 ) {
					combioven_state.target_time -= 1;
					printf("seconds:%d\n",(uint32_t)combioven_state.target_time);
				}

				else if ( (combioven_state.target_time == 0) && (recipe_active.actualstep < recipe_active.totalsteps)) {
					//sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
					//UART_Print(buffer_Tx);
					//printf("%s", buffer_Tx);
					combioven_state.toggle_state = 5;
					relayboard_state.completed_step=0;
					runningState = combioven_state.toggle_state;
					sleep_ms(500);
				}

				else if ( (combioven_state.target_time == 0) && (recipe_active.actualstep == recipe_active.totalsteps)) {
					combioven_state.toggle_state = 7;
					runningState = combioven_state.toggle_state;
					sprintf(buffer_Tx,FINISHED_PROCESS);
					UART_Print(buffer_Tx);
					printf("%s",buffer_Tx);
				}

				timer = time(NULL);
				dataChanged = 1;
			}

			else if(recipe_active.typestep == 3)
			{
				if ((combioven_state.current_probe >= combioven_state.target_probe) && (combioven_state.current_temperature >= combioven_state.target_temperature)) {
					if(recipe_active.actualstep == recipe_active.totalsteps) 
					{
						combioven_state.toggle_state = 7;
						runningState = combioven_state.toggle_state;
						sprintf(buffer_Tx, FINISHED_PROCESS);
						UART_Print(buffer_Tx);
						printf("%s", buffer_Tx);
					}
					
					else
					{
						combioven_state.toggle_state = 5;
						relayboard_state.completed_step = 0;
						runningState = combioven_state.toggle_state;
						sleep_ms(500);
					}
					dataChanged = 1;
				}

				else
				{
					sprintf(buffer_Tx, GET_EXTERN_PROBE_TEMP);
					UART_Print(buffer_Tx);
					printf("%s\n", buffer_Tx);
				}
				timer = time(NULL);
			}
		}
		
		//running washing Ok
		else if( (seconds > 0.5) && (runningState == 4) && (relayboard_state.door_status==1) )
		{	
			if(   combioven_state.target_time > 0)
			{			
				combioven_state.target_time -= 1;
				printf("seconds: %d\n",(uint32_t)combioven_state.target_time);
				Washing_Process(relayboard_state.washing_mode, relayboard_state.washing_phase, combioven_state.target_time);
			}
			else 
			{
				combioven_state.toggle_state = 7;
				runningState = combioven_state.toggle_state;
				sprintf(buffer_Tx,FINISHED_PROCESS);
			    UART_Print(buffer_Tx);
				printf("%s",buffer_Tx);
			}
			timer = time(NULL);
			dataChanged = 1;
		}

		//Finish state
		else if ((seconds > 5) && (runningState == 7))
		{
			combioven_state.toggle_state = 0;
			runningState = combioven_state.toggle_state;
			printf("Change data to: %d\n",(uint8_t)combioven_state.toggle_state);
			dataChanged = 1;
		}
		unlock_mutex();

	
		if (dataChanged) {
			lock_mutex();
			event_UI_data = combioven_state;
			dataChanged = 0;
			unlock_mutex();

			// Serialize the data to a buffer
			nbuffer = gre_io_serialize(nbuffer, NULL, COMBIOVEN_UPDATE_EVENT, COMBIOVEN_UPDATE_FMT, &event_UI_data, sizeof(event_UI_data));
			if (!nbuffer) {
				fprintf(stderr, "Can't serialized data to buffer, exiting\n");
				break;
			}

			// Send the serialized event buffer
			ret = gre_io_send(send_handle, nbuffer);
			fprintf(stderr, "Send update event\n");
			if (ret < 0) {
				fprintf(stderr, "Send failed, exiting\n");
				break;
			}
		}
	}

	//Release the buffer memory, close the send handle
	gre_io_free_buffer(nbuffer);
	gre_io_close(send_handle);
	return 0;
}



/****************************************************************************************************************************
 *
 *                     D e f i n i t i o n   f o r    W A S H I N G    C Y C L E S <
 * 
 ****************************************************************************************************************************/
void Washing_Process(uint8_t modeSelected, uint8_t phaseStatus, uint32_t timeElapse)
{

	switch (modeSelected)
	{
	case 1:
		if (phaseStatus == 0) {
			sprintf(buffer_Tx, PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
			relayboard_state.washing_phase = 99;  //IDLE status
		}

		else if (timeElapse == 695) {
			sprintf(buffer_Tx, PHASE_WASH_OUT);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 420) {
			sprintf(buffer_Tx, PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 390) {
			sprintf(buffer_Tx, PHASE_DRYING_CAMERA);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}
		break;


	case 2:
		if (phaseStatus == 0) {
			sprintf(buffer_Tx, PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
			relayboard_state.washing_phase = 99;  //IDLE status
		}

		else if (timeElapse == 1475) {
			sprintf(buffer_Tx, PHASE_CLEAN_CARE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 1300) {
			sprintf(buffer_Tx, PHASE_PREHEAT_BOILER);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 1165) {
			sprintf(buffer_Tx, PHASE_FILL_COLD_CAMERA);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 1140) {
			sprintf(buffer_Tx, PHASE_RECYCLE_WATER);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 780) {
			sprintf(buffer_Tx, PHASE_WASH_OUT);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 540) {
			sprintf(buffer_Tx, PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 510) {
			sprintf(buffer_Tx, PHASE_DRYING_CAMERA);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 150) {
			sprintf(buffer_Tx, PHASE_COOLING_CAMERA);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}
		break;


	case 3:
		if (phaseStatus == 0) {							//25 sec
			sprintf(buffer_Tx, PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
			relayboard_state.washing_phase = 99;  		//IDLE status
		}

		else if (timeElapse == 2431) {					//	120 sec
			sprintf(buffer_Tx, PHASE_CLEAN_JET);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 2311 || timeElapse == 1581) {	//	180 sec  // REPETIR DESDE AQUI   
			sprintf(buffer_Tx, PHASE_PREHEAT_BOILER);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 2131 || timeElapse == 1461) {	//	25 sec	
			sprintf(buffer_Tx, PHASE_FILL_COLD_CAMERA);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		//DISPENSAR DETERGENTE

		else if (timeElapse == 2106 || timeElapse == 1436) {  // 600 sec
			sprintf(buffer_Tx, PHASE_RECYCLE_WATER);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 1606 || timeElapse == 836) {  // 25 sec
			sprintf(buffer_Tx, PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 811) {					//	180 sec	
			sprintf(buffer_Tx, PHASE_CLEAN_JET);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 631 || timeElapse == 401) {  // 25 sec    // REPETIR HASTA AQUI
			sprintf(buffer_Tx, PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 606) {	//	25 sec	
			sprintf(buffer_Tx, PHASE_FILL_COLD_CAMERA);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 581) {	//	180 sec
			sprintf(buffer_Tx, PHASE_RECYCLE_WATER);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 450) {	//	180 sec
			sprintf(buffer_Tx, PHASE_RECYCLE_WATER);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 240) {  // 300 sec
			sprintf(buffer_Tx, PHASE_DRYING_CAMERA);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 120) {
			sprintf(buffer_Tx, PHASE_COOLING_CAMERA);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}
		break;


	case 4:
		if (phaseStatus == 0) {				//	25 sec
			sprintf(buffer_Tx, PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
			relayboard_state.washing_phase = 99;  //IDLE status
		}

		else if (timeElapse == 3935) {		//	120 sec
			sprintf(buffer_Tx, PHASE_CLEAN_JET);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 3815) {		//	180 sec
			sprintf(buffer_Tx, PHASE_DRYING_CAMERA); //sprintf(buffer_Tx,PHASE_PREHEAT_BOILER);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 3635 || timeElapse == 3160) {	//	25 sec   //REPETIR DESDE AQUI		
			sprintf(buffer_Tx, PHASE_FILL_COLD_CAMERA);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 3610 || timeElapse == 3135) {	//  900 sec
			sprintf(buffer_Tx, PHASE_RECYCLE_WATER);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 2685) {		//	25 sec	
			sprintf(buffer_Tx, PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 2660) {		//	240-300 sec
			sprintf(buffer_Tx, PHASE_CLEAN_JET);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 2540) {		//  420 sec
			sprintf(buffer_Tx, PHASE_WASH_OUT);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 2120) {		// 25 sec
			sprintf(buffer_Tx, PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 2095) {	 	// 240-300 sec	

			sprintf(buffer_Tx, PHASE_CLEAN_CARE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 1905 || timeElapse == 1430) { //25 sec	
			sprintf(buffer_Tx, PHASE_FILL_COLD_CAMERA);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 1880 || timeElapse == 1405) { //900 sec // REPETIR HASTA AQUI
			sprintf(buffer_Tx, PHASE_RECYCLE_WATER);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 1000) {  		//  420 sec
			sprintf(buffer_Tx, PHASE_WASH_OUT);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 600) {  		// 25 sec
			sprintf(buffer_Tx, PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 575) {		//	180 sec
			sprintf(buffer_Tx, PHASE_DRYING_CAMERA);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 395) {  		// 300 sec
			sprintf(buffer_Tx, PHASE_DRYING_CAMERA);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 245) {
			sprintf(buffer_Tx, PHASE_COOLING_CAMERA);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}
		break;

	case 5:
		break;

	case 6:
		break;

	case 7:
		break;

	default:
		break;
	}
}
