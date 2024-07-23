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

#include <sys/ipc.h>
#include <sys/msg.h>

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
static int							dataChanged   = 1;  //Default to 1 so we send data to the ui once it connects
static int							runningState  = 0;	//Actual state-machine
static char							previousState = 0;	//Previous state-machine
static int 							fd;                 //To open uart-serial port
static int							nread;				//To get uart data lenght
static char  						buffer_Tx[128];     //Transmit data buffer to relay-board
static char							buffer_Rx[128];		//Receive data buffer from relay-board
static combioven_update_event_t	    combioven_state;	//Structure for UI-Backend updated channel
static relayboard_update_event_t	relayboard_state;	//Structure for Backend - PowerPCB communication
static recipe_info_event_t			recipe_active;		//Structure for recipe data update
static enable_encoder_opt_event_t	encoder_options;	//Structure for encoder options enabled
static combi_mode_t 				mode_operation;
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

void Update_Parameters(combioven_update_event_t *combiOven, relayboard_update_event_t	*relayboard);
void Washing_Process(relayboard_update_event_t *relayboard, uint8_t modeSelected, uint8_t phaseStatus, uint32_t timeElapse);

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
					if((combioven_state.toggle_state == RUN_AUTO_STATE || previousState == RUN_AUTO_STATE || combioven_state.toggle_state == RUN_MULTILEVEL_STATE || previousState == RUN_MULTILEVEL_STATE ) && relayboard_state.completed_step == 0)
					{
						relayboard_state.completed_step=1;
					}
					
					else
					  combioven_state.toggle_state 	= STOP_OVEN_STATE;
				}
				
				printf(" Receive Change data to: %d\n",(uint8_t)combioven_state.toggle_preheat);
				Clear_Buffer(buffer_Rx);
				runningState=combioven_state.toggle_state;
				dataChanged = 1;
			}


			else if ( strstr((char*)buffer_Rx, MODE_COOLING ) != NULL ) {
				if (combioven_state.toggle_cooling == 1) {
					combioven_state.toggle_cooling 	= 0;
					printf("State data to: %d\n",(uint8_t)combioven_state.toggle_state);
					if(combioven_state.toggle_state == RUN_WASHING_STATE)
					{
						sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
						UART_Print(buffer_Tx);
						printf("%s\n", buffer_Tx);
						sleep_ms(500);
						sprintf(buffer_Tx,MODE_WASHING);
						UART_Print(buffer_Tx);
						printf("%s\n", buffer_Tx);
						sleep_ms(500);
					}
					 
					else
					{
						combioven_state.toggle_state = STOP_OVEN_STATE;
					}
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
				else if(combioven_state.toggle_state == WARNING_STATE)
				{
					sprintf(buffer_Tx,RUNNING_PROCESS);
			    	UART_Print(buffer_Tx);
					printf("%s\n",buffer_Tx);
					combioven_state.toggle_state=previousState;
					dataChanged=1;
				}
				Clear_Buffer(buffer_Rx);
			}

			else if ( strstr((char *)buffer_Rx, CURRENT_HUMIDITY ) != NULL ) {
        		for(int i=5; i<8; i++)
        		{
        			if( buffer_Rx[i] == ' '){
        				cpyValue[i-5] = '0';
					}
        			else
            			cpyValue[i-5] = buffer_Rx[i];
        		}
        		relayboard_state.current_humidity = atoi(cpyValue);
				sprintf(buffer_Rx,"HR:%3d",(uint8_t)relayboard_state.current_humidity );
				printf("%s\n",buffer_Rx);
				if( (relayboard_state.current_humidity >= combioven_state.current_humidity) || (relayboard_state.current_humidity >= combioven_state.target_steam))
				{
					combioven_state.current_humidity = relayboard_state.current_humidity;
					dataChanged = 1;
				}
			    Clear_Buffer(buffer_Rx);
			}

			else if ( strstr((char *)buffer_Rx, CURRENT_CAM_TEMPERATURE ) != NULL ) {
        		for(int i=5; i<8; i++)
        		{
        			if( buffer_Rx[i] == ' '){
        				cpyValue[i-5] = '0';
					}
        			else
            			cpyValue[i-5] = buffer_Rx[i];
        		}
        		relayboard_state.current_cam_temperature = atoi(cpyValue);
				Clear_Buffer(buffer_Rx);
				dataChanged = 1;
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
//				sprintf(buffer_Rx,"warning:%3d",(uint16_t)warningCode);
//				printf("%s\n",buffer_Rx);
//			    Clear_Buffer(buffer_Rx);
				if(warningCode != relayboard_state.warning_code){
					switch(warningCode)
					{
						case 0:		combioven_state.toggle_state=previousState;//Reset warning
									relayboard_state.warning_code=0;
									break;

						case 231:	previousState = combioven_state.toggle_state;
									combioven_state.toggle_state = CONNECT_WATER_STATE;
									relayboard_state.warning_code = warningCode;
									break;//Water supply error
						
						case 131:	previousState = combioven_state.toggle_state;
									combioven_state.toggle_state = CONNECT_WATER_STATE;
									relayboard_state.warning_code = warningCode;
									break;//Boiler level sensor failure

						case 111:   previousState = combioven_state.toggle_state;
									combioven_state.toggle_state = WARNING_STATE;
									relayboard_state.warning_code = warningCode;
									break;//Multipoint Thermocouple opened

						case 114:	previousState = combioven_state.toggle_state;
									combioven_state.toggle_state = WARNING_STATE;
									relayboard_state.warning_code = warningCode;
									break;//Cold-Camera Thermocouple opened

						case 115:	previousState = combioven_state.toggle_state;
									combioven_state.toggle_state = WARNING_STATE;
									relayboard_state.warning_code = warningCode;
									break;//Boiler Thermocouple opened

						case 116:	previousState = combioven_state.toggle_state;
									combioven_state.toggle_state = WARNING_STATE;
									relayboard_state.warning_code = warningCode;
									break;//Main-Camera Thermocouple opened
									
						case 510:   break;//Relayboard program failure
						case 511:	break;//Communication failure
						default: 	break;
					}	
				}
				runningState=combioven_state.toggle_state;
//				sprintf(buffer_Rx,"prevS:%d",(uint8_t)previousState);
//				printf("%s\n",buffer_Rx);
				sleep_ms(5);
			    Clear_Buffer(buffer_Rx);
				dataChanged = 1;	
			}


			else if ( strstr((char *)buffer_Rx, DOOR_OPEN ) != NULL ) {
        		relayboard_state.door_status = 0;
				if((combioven_state.toggle_cooling == 0) && (runningState >= RUN_SUB_STATE && runningState <= RUN_WASHING_STATE) )
				{
					previousState = combioven_state.toggle_state;
					relayboard_state.encoder_activated	= ENCODER_DISABLE;   //STOP READ ENCODER
					combioven_state.toggle_state 		= PAUSE_BY_DOOR_STATE;  //GENERATE PAUSE 
					sleep_ms(5);
					sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
			    	UART_Print(buffer_Tx);
					printf("%s\n",buffer_Tx);
				}

				else if(runningState == RDY_NEXTSTEP_STATE) //ready for next step state
				{
					previousState = runningState;
					sleep_ms(5);
					sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
			    	UART_Print(buffer_Tx);
					printf("%s\n",buffer_Tx);
					relayboard_state.completed_step = 1;
				}

				else if(runningState == FINISHED_STATE) //finish state
				{	
					combioven_state.toggle_state=STOP_OVEN_STATE;
				}
				runningState=combioven_state.toggle_state;
				dataChanged = 1;
			}
			

			else if ( strstr((char *)buffer_Rx, DOOR_CLOSED ) != NULL ) {
        		relayboard_state.door_status=1;
				if( (runningState == RUN_SUB_STATE) && (combioven_state.toggle_cooling == 0))
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
				
				else if (runningState == PAUSE_BY_DOOR_STATE){
					combioven_state.toggle_state=previousState;  
					if ((recipe_active.typestep == 0) && (previousState == RUN_AUTO_STATE || previousState == RUN_MULTILEVEL_STATE) )
					{
						relayboard_state.completed_step=1;
						//printf("complStep\n");
					}
					
					else if(((combioven_state.toggle_state == RUN_SUB_STATE) && (combioven_state.toggle_looptime==0) && (combioven_state.toggle_probe == 0)) || (recipe_active.typestep == 1 && previousState == RUN_AUTO_STATE) ) //|| (recipe_active.typestep == 1 && previousState == RUN_MULTILEVEL_STATE))
					{
						sleep_ms(5);
						sprintf(buffer_Tx,MODE_PREHEAT);
			    		UART_Print(buffer_Tx);
						printf("%s\n",buffer_Tx);
					}

					else if (previousState == RUN_WASHING_STATE && combioven_state.toggle_cooling==1)
					{
						sleep_ms(5);
						sprintf(buffer_Tx,MODE_COOLING);
			    		UART_Print(buffer_Tx);
						printf("%s\n",buffer_Tx);
					} 

					else
					{
						sleep_ms(5);
						sprintf(buffer_Tx,RUNNING_PROCESS);
			    		UART_Print(buffer_Tx);
						printf("%s\n",buffer_Tx);
						if(previousState == RUN_MULTILEVEL_STATE && relayboard_state.hitemp_timeout>1)
						{
							sprintf(buffer_Tx, "#temp%3d", (uint16_t)combioven_state.target_temperature);
							UART_Print(buffer_Tx);
							printf("%s\n",buffer_Tx);
						}

						else if(relayboard_state.hitemp_timeout>1)
						{
							relayboard_state.hitemp_timeout = 1800;
						}
					}
				}

				else if(runningState == FINISHED_STATE) //finish state
				{
					combioven_state.toggle_state=STOP_OVEN_STATE;
				}
				runningState=combioven_state.toggle_state;
				dataChanged = 1;	
			}


			else if ( (strcmp(buffer_Rx, ENCODER_INCREASE ) == 0) && (relayboard_state.encoder_activated < ENCODER_DISABLE) ){
				if (relayboard_state.encoder_parameter == 1) {
					if(combioven_state.target_steam >= MAX_TARGET_PERCENT)
					   combioven_state.target_steam = MAX_TARGET_PERCENT;
					else 
					   combioven_state.target_steam = combioven_state.target_steam + 1;
					Clear_Buffer(buffer_Rx);
					dataChanged = 1;
				}

				else if (relayboard_state.encoder_parameter == 2) {

					if(mode_operation == COMBI_MODE_STEAM)
					{
						if(combioven_state.target_temperature >= MAX_TARGET_TEMP_STEAM)
							combioven_state.target_temperature = MAX_TARGET_TEMP_STEAM;
						else 
							combioven_state.target_temperature = combioven_state.target_temperature + 1;
					}
					else {
						if(combioven_state.target_temperature >= MAX_TARGET_TEMPERATURE)
							combioven_state.target_temperature = MAX_TARGET_TEMPERATURE;
						else 
							combioven_state.target_temperature = combioven_state.target_temperature + 1;
					}
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

				else if (relayboard_state.encoder_parameter == 6) {							//Steam recipe-parameter
					if(encoder_options.nowvalue  >= encoder_options.maxvalue )
					   encoder_options.nowvalue  = encoder_options.maxvalue;
					else 
					   encoder_options.nowvalue = encoder_options.nowvalue + encoder_options.stepvalue;
					Clear_Buffer(buffer_Rx);
					combioven_state.encoder_data = encoder_options.nowvalue;
					combioven_state.encoder_parameter = relayboard_state.encoder_parameter;
					dataChanged = 1;
				}
				
				relayboard_state.encoder_activated=0;
				sprintf(buffer_Tx,ENCODER_ENABLE);
				UART_Print(buffer_Tx);
				printf("%s\n",buffer_Tx);
			}

			else if ( (strcmp(buffer_Rx, ENCODER_DECREASE ) == 0) && (relayboard_state.encoder_activated < ENCODER_DISABLE) ){
				if (relayboard_state.encoder_parameter == 1) {								//Steam manual-parameter
					if(combioven_state.target_steam <= 0 )
					   combioven_state.target_steam = 0;
					else 
					   combioven_state.target_steam = combioven_state.target_steam - 1;
					Clear_Buffer(buffer_Rx);
					dataChanged = 1;
				}

				else if (relayboard_state.encoder_parameter == 2) {							//Temperature manual-parameter		
					if(combioven_state.target_temperature <= 30)
					   combioven_state.target_temperature = 30;
					else 
					   combioven_state.target_temperature = combioven_state.target_temperature - 1;
					Clear_Buffer(buffer_Rx);
					dataChanged = 1;
				}

				else if (relayboard_state.encoder_parameter == 3) {							//Time manual-parameter
					if(combioven_state.target_time <= 0)
					   combioven_state.target_time = 0;
					else
					   combioven_state.target_time = combioven_state.target_time - 2;
					Clear_Buffer(buffer_Rx);
					dataChanged = 1;
				}
				
				else if (relayboard_state.encoder_parameter == 4) {							//Speed manual-parameter
					if(combioven_state.target_fanspeed <= 25)
					   combioven_state.target_fanspeed = 25;
					else 
					   combioven_state.target_fanspeed = combioven_state.target_fanspeed - 25;
					Clear_Buffer(buffer_Rx);
					dataChanged = 1;
				}

				else if (relayboard_state.encoder_parameter == 5) {							//Probe manual-parameter
					if(combioven_state.target_probe <= 0)
					   combioven_state.target_probe = 0;
					else 
					   combioven_state.target_probe = combioven_state.target_probe - 1;
					Clear_Buffer(buffer_Rx);
					dataChanged = 1;
				}

				else if (relayboard_state.encoder_parameter == 6) {							//Editor recipe-parameter
					if(encoder_options.nowvalue  <= encoder_options.minvalue )
					   encoder_options.nowvalue  = encoder_options.minvalue;
					else 
					   encoder_options.nowvalue = encoder_options.nowvalue - encoder_options.stepvalue;
					Clear_Buffer(buffer_Rx);
					combioven_state.encoder_data = encoder_options.nowvalue;
					combioven_state.encoder_parameter = relayboard_state.encoder_parameter;
					dataChanged = 1;
				}
				
				relayboard_state.encoder_activated=0;
				sprintf(buffer_Tx,ENCODER_ENABLE);
				UART_Print(buffer_Tx);
				printf("%s\n",buffer_Tx);
			}

			else if ( (strcmp(buffer_Rx, ENCODER_ZERO_POSITION ) == 0) && (relayboard_state.encoder_activated < ENCODER_DISABLE)) {
				//printf("timE:%d\n",(uint8_t)relayboard_state.encoder_activated);	
				if( relayboard_state.encoder_activated == (ENCODER_DISABLE - 2) )
				{
					relayboard_state.encoder_activated = ENCODER_DISABLE;
					sleep_ms(200);
					Update_Parameters(&combioven_state, &relayboard_state);
					switch (relayboard_state.encoder_parameter)
					{
					
					case 3:		sprintf(buffer_Tx,"timer:%d",(uint32_t)combioven_state.target_time);
								printf("%s\n",buffer_Tx);
								break;
					
					case 5:		sprintf(buffer_Tx,"#tprb%3d",(uint16_t)combioven_state.target_probe);
								printf("%s\n",buffer_Tx);
								break;
					
					default:	break;
					}
					sleep_ms(10);
					relayboard_state.encoder_parameter = 0;
					combioven_state.encoder_parameter = relayboard_state.encoder_parameter;
					dataChanged = 1;
				}

				else 
				{
				   relayboard_state.encoder_activated = relayboard_state.encoder_activated + 1;
				}
				sleep_ms(1);
				Clear_Buffer(buffer_Rx);
				
			}

			else 
			{
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
			sleep_ms(1);	
			dataChanged = 1;
		} 
		
		else if (strcmp(event_name, UPDATE_TEMPERATURE_EVENT) == 0) {
			update_temperature_event_t *uidata = (update_temperature_event_t *)event_data;
			combioven_state.target_temperature = uidata->usertemp;
			sprintf(buffer_Tx,"#temp%3d",(uint16_t)combioven_state.target_temperature);
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
			sleep_ms(1);
			if (combioven_state.target_temperature > 260)
			{
				relayboard_state.hitemp_timeout = 1800;			//start a timer for hi temperature
			}
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
			sleep_ms(1);
			dataChanged = 1;
		}

		else if (strcmp(event_name, UPDATE_TEMP_PROBE_EVENT) == 0) {
			update_temp_probe_event_t* uidata = (update_temp_probe_event_t*)event_data;
			combioven_state.target_probe = uidata->usertemp;
			sprintf(buffer_Tx,"Tprobe:%3d",(uint16_t)combioven_state.target_probe);
			printf("%s\n",buffer_Tx);
			dataChanged = 1;
		}

		else if (strcmp(event_name, UPDATE_TEMP_DELTA_EVENT) == 0) {
			update_temp_delta_event_t* uidata = (update_temp_delta_event_t*)event_data;
			relayboard_state.delta_temperature = uidata->userdelta;
			sprintf(buffer_Tx, "Tdelta:%3d",(uint16_t)relayboard_state.delta_temperature);
			printf("%s\n", buffer_Tx);
			dataChanged = 1;
		}

		else if (strcmp(event_name, RECIPE_INFO_EVENT) == 0) {
			recipe_info_event_t* uidata = (recipe_info_event_t*)event_data;
			recipe_active.totalsteps 	= uidata->totalsteps;
			recipe_active.actualstep 	= uidata->actualstep;
			recipe_active.typestep		= uidata->typestep;
			recipe_active.currlevel		= uidata->currlevel;
			sprintf(buffer_Tx,"steps:%d now:%d mode:%d level:%d",(uint8_t)recipe_active.totalsteps,(uint8_t)recipe_active.actualstep,(uint8_t)recipe_active.typestep, (uint8_t)recipe_active.currlevel);
			printf("%s\n",buffer_Tx);
			sleep_ms(10);
			relayboard_state.completed_step	= 0;
			combioven_state.toggle_state 	= STOP_OVEN_STATE;
		}

		else if (strcmp(event_name, TOGGLE_RELAY_SERVICE_EVENT) == 0) {
			toggle_relay_service_event_t* uidata = (toggle_relay_service_event_t*)event_data;
			relayboard_state.toggle_relay = uidata->relay;
			sprintf(buffer_Tx, "#serv%3d",(uint8_t)relayboard_state.toggle_relay);
			UART_Print(buffer_Tx); 
			printf("%s\n", buffer_Tx);
			dataChanged = 1;
		}

		else if (strcmp(event_name, MODE_CONVECTION_EVENT) == 0) {
			mode_operation = COMBI_MODE_CONVECTION;
			sprintf(buffer_Tx,MODE_CONVECTION);
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
		}

		else if (strcmp(event_name, MODE_RECIPE_CONVECTION_EVENT) == 0) {
			mode_operation = COMBI_MODE_CONVECTION;
		}

		else if (strcmp(event_name, MODE_COMBINED_EVENT) == 0) {
			mode_operation = COMBI_MODE_COMBINED;
			sprintf(buffer_Tx,MODE_COMBINED);
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
		}

	   else if (strcmp(event_name, MODE_RECIPE_COMBINED_EVENT) == 0) {
			mode_operation = COMBI_MODE_COMBINED;
		}


		else if (strcmp(event_name, MODE_STEAM_EVENT) == 0) {
			mode_operation = COMBI_MODE_STEAM;
			sprintf(buffer_Tx,MODE_STEAM);
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
		}

		else if (strcmp(event_name, MODE_RECIPE_STEAM_EVENT) == 0) {
			mode_operation = COMBI_MODE_STEAM;
		}

		else if (strcmp(event_name, MODE_LOAD_EVENT) == 0) {
			relayboard_state.completed_step = 0;
		}

		else if (strcmp(event_name, TOGGLE_SPRAY_EVENT) == 0) {
			if ((runningState >=RUN_SUB_STATE && runningState <= RUN_AUTO_STATE) && (combioven_state.toggle_cooling ==0)) {
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


		else if (strcmp(event_name, ENABLE_ENCODER_OPT_EVENT) == 0) {
			enable_encoder_opt_event_t* uidata = (enable_encoder_opt_event_t*)event_data;
			relayboard_state.encoder_parameter = uidata->parameter;
			relayboard_state.encoder_activated = 0;
			encoder_options.minvalue = uidata->minvalue;
			encoder_options.maxvalue = uidata->maxvalue;
			encoder_options.nowvalue = uidata->nowvalue;
			encoder_options.stepvalue = uidata->stepvalue;
			sprintf(buffer_Tx, ENCODER_ENABLE);
			UART_Print(buffer_Tx);
			//printf("%s Min: %d Max: %d Now: %d\n", buffer_Tx, (uint32_t)encoder_options.minvalue, (uint32_t)encoder_options.maxvalue, (uint32_t)encoder_options.nowvalue);
			sleep_ms(5);
			//if (relayboard_state.encoder_parameter == 3)
			//{
			//	combioven_state.target_time = combioven_state.target_time + 2;
			//	dataChanged = 1;
			//}
		} 


//************************************************************************************//	
//		E V E N T S    W I T H    S T A R T  / S T O P   F U N C T I O N A L I T Y    //
//************************************************************************************//

		else if (strcmp(event_name, TOGGLE_MANUAL_EVENT) == 0) {
			if (runningState != RUN_MANUAL_STATE) {
				sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
			    UART_Print(buffer_Tx);
				printf("%s\n",buffer_Tx);
				sleep_ms(100);
				combioven_state.toggle_cooling		= 0;
				combioven_state.toggle_preheat		= 0;
				if(relayboard_state.door_status	==	1	){
					sprintf(buffer_Tx,RUNNING_PROCESS);
			    	UART_Print(buffer_Tx);
					printf("%s\n",buffer_Tx);
					combioven_state.toggle_state  		= RUN_MANUAL_STATE;
					combioven_state.current_temperature = 0;
					sleep_ms(10);
				}

				else{
					combioven_state.toggle_state	= PAUSE_BY_DOOR_STATE;	//Set UI message Close Door
					previousState					= RUN_MANUAL_STATE;	//When Door is close will run
				}
			}
			else {
				combioven_state.toggle_state = STOP_OVEN_STATE;
				sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
			    UART_Print(buffer_Tx);
				printf("%s\n",buffer_Tx);
			}
			printf("Change data to: %d\n",(uint8_t)combioven_state.toggle_state);
			dataChanged = 1;
		} 

		else if (strcmp(event_name, TOGGLE_AUTOMATIC_EVENT) == 0) {
			if (runningState != RUN_AUTO_STATE) {
				sprintf(buffer_Tx, PAUSE_STOP_PROCESS);
				UART_Print(buffer_Tx);
				printf("%s\n", buffer_Tx);
				sleep_ms(1000);
				//0: Wait_User action / 1: Preheat/Cooling / 2: Running Start  / 3: coreProbe / 4: Delta-T / 5: Hold
				if (relayboard_state.door_status == 1)
				{
					combioven_state.toggle_state = RUN_AUTO_STATE;
					combioven_state.current_temperature = 0;
					switch (recipe_active.typestep)
					{
					case 1:		sprintf(buffer_Tx, MODE_PREHEAT);
								UART_Print(buffer_Tx);
								printf("%s\n", buffer_Tx);
								combioven_state.toggle_preheat 		= 1;
								combioven_state.current_temperature = 0;
								break;

					case 2:		sprintf(buffer_Tx, RUNNING_PROCESS);
								UART_Print(buffer_Tx);
								printf("%s\n", buffer_Tx);
								relayboard_state.completed_step 	= 0;
								break;

					case 3:		sprintf(buffer_Tx, RUNNING_PROCESS);
								UART_Print(buffer_Tx);
								printf("%s\n", buffer_Tx);
								relayboard_state.completed_step 	= 0;
								combioven_state.toggle_probe 		= 1;
								combioven_state.current_probe		= 0;
								combioven_state.current_temperature = 0;
								break;

					case 4:		sprintf(buffer_Tx, RUNNING_PROCESS);
								UART_Print(buffer_Tx);
								printf("%s\n", buffer_Tx);
								relayboard_state.completed_step 	= 0;
								combioven_state.toggle_probe 		= 1;
								combioven_state.current_probe 		= 0;
								combioven_state.current_temperature = 0;
								break;

					case 5:		sprintf(buffer_Tx, RUNNING_PROCESS);
								UART_Print(buffer_Tx);
								printf("%s\n", buffer_Tx);
								relayboard_state.completed_step 	= 0;
								combioven_state.current_probe 		= 0;
								combioven_state.current_temperature = 0;
								combioven_state.toggle_probe 		= 0;
								combioven_state.toggle_looptime 	= 1;
								break;

					default:	sprintf(buffer_Tx, PAUSE_STOP_PROCESS);
								UART_Print(buffer_Tx);
								printf("%s\n", buffer_Tx);
								break;
					}
				}

				else 
				{
					combioven_state.toggle_state = PAUSE_BY_DOOR_STATE;	//Set UI message Close Door
					previousState = RUN_AUTO_STATE;					//When Door is close will run
					sprintf(buffer_Tx, PAUSE_STOP_PROCESS);
					UART_Print(buffer_Tx);
					printf("%s\n", buffer_Tx);
					
					switch (recipe_active.typestep)
					{
					case 1:		combioven_state.toggle_preheat 		= 1;
								combioven_state.current_temperature = 0;
								break;

					case 2:		relayboard_state.completed_step 	= 0;
								break;

					case 3:		relayboard_state.completed_step 	= 0;
								combioven_state.toggle_probe 		= 1;
								combioven_state.current_probe		= 0;
								combioven_state.current_temperature = 0;
								break;

					case 4:		relayboard_state.completed_step 	= 0;
								combioven_state.toggle_probe 		= 1;
								combioven_state.current_probe 		= 0;
								combioven_state.current_temperature = 0;
								break;

					case 5:		relayboard_state.completed_step 	= 0;
								combioven_state.current_probe 		= 0;
								combioven_state.current_temperature = 0;
								combioven_state.toggle_probe 		= 0;
								combioven_state.toggle_looptime 	= 1;
								break;

					default:	sprintf(buffer_Tx, PAUSE_STOP_PROCESS);
								UART_Print(buffer_Tx);
								printf("%s\n", buffer_Tx);
								break;
					}
				}
			}

			else 
			{
				combioven_state.toggle_state = STOP_OVEN_STATE;
				sprintf(buffer_Tx, PAUSE_STOP_PROCESS);
				UART_Print(buffer_Tx);
				printf("%s\n", buffer_Tx);
			}
			sleep_ms(10);
			printf("Change data to: %d\n", (uint8_t)combioven_state.toggle_state);
			dataChanged = 1;
		}

		else if (strcmp(event_name, TOGGLE_MULTILEVEL_EVENT) == 0) {
			if (runningState != RUN_MULTILEVEL_STATE) {
				sprintf(buffer_Tx, PAUSE_STOP_PROCESS);
				UART_Print(buffer_Tx);
				printf("%s\n", buffer_Tx);
				sleep_ms(1000);
				//0: Wait_User action / 1: Preheat/Cooling / 2: Running Start
				if (relayboard_state.door_status == 1)
				{
					combioven_state.toggle_state = RUN_MULTILEVEL_STATE;
					combioven_state.current_temperature = 0;
					switch (recipe_active.typestep)
					{
					case 1:		sprintf(buffer_Tx, MODE_PREHEAT);
								UART_Print(buffer_Tx);
								printf("%s\n", buffer_Tx);
								combioven_state.toggle_preheat = 1;
								combioven_state.current_temperature = 0;
								break;

					case 2:		sprintf(buffer_Tx, RUNNING_PROCESS);
								UART_Print(buffer_Tx);
								printf("%s\n", buffer_Tx);
								relayboard_state.completed_step = 0;
								break;

					default:	sprintf(buffer_Tx, PAUSE_STOP_PROCESS);
								UART_Print(buffer_Tx);
								printf("%s\n", buffer_Tx);
								break;
					}
				}

				else 
				{
					combioven_state.toggle_state = PAUSE_BY_DOOR_STATE;		//Set UI message Close Door
					previousState = RUN_MULTILEVEL_STATE;					//When Door is close will run
					sprintf(buffer_Tx, PAUSE_STOP_PROCESS);
					UART_Print(buffer_Tx);
					printf("%s\n", buffer_Tx);
					
					switch (recipe_active.typestep)
					{
					case 1:		combioven_state.toggle_preheat 		= 1;
								combioven_state.current_temperature = 0;
								break;

					case 2:		relayboard_state.completed_step 	= 0;
								break;

					default:	sprintf(buffer_Tx, PAUSE_STOP_PROCESS);
								UART_Print(buffer_Tx);
								printf("%s\n", buffer_Tx);
								break;
					}
				}
			}

			else 
			{
				combioven_state.toggle_state = STOP_OVEN_STATE;
				sprintf(buffer_Tx, PAUSE_STOP_PROCESS);
				UART_Print(buffer_Tx);
				printf("%s\n", buffer_Tx);
			}
			sleep_ms(10);
			printf("Change data to: %d\n",(uint8_t)combioven_state.toggle_state);
			dataChanged = 1;
		}


		else if (strcmp(event_name, TOGGLE_WASHING_EVENT) == 0) {
			if (runningState != RUN_WASHING_STATE) {
				sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
				UART_Print(buffer_Tx);
				printf("%s\n", buffer_Tx);
				sleep_ms(100);
			}
			else {
				combioven_state.toggle_state = STOP_OVEN_STATE;
				sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
				UART_Print(buffer_Tx);
				printf("%s\n",buffer_Tx);
			}
			sleep_ms(10);
			printf("Change data to: %d\n",(uint8_t)combioven_state.toggle_state);
			dataChanged = 1;
		}

		else if (strcmp(event_name, UPDATE_WASH_CYCLE_EVENT) == 0) {
			update_wash_cycle_event_t* uidata = (update_wash_cycle_event_t*)event_data;
			relayboard_state.washing_mode = uidata->usercycle;
			sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
			sleep_ms(500);
			relayboard_state.washing_phase 				= 0;
			relayboard_state.current_cam_temperature 	= 300;
			combioven_state.target_fanspeed 			= 100;
			combioven_state.target_temperature 			= 60;		
			combioven_state.toggle_cooling	= 1;
			Update_Parameters(&combioven_state, &relayboard_state); 
			if (relayboard_state.door_status == 1){
					sleep_ms(100);
					combioven_state.toggle_state  	= RUN_WASHING_STATE;
					sprintf(buffer_Tx, MODE_COOLING);
					UART_Print(buffer_Tx);
					printf("%s\n",buffer_Tx);
			}

			else {
					combioven_state.toggle_state = PAUSE_BY_DOOR_STATE;		//Set UI message Close Door
					previousState = RUN_WASHING_STATE;					//When Door is close will run
					sprintf(buffer_Tx, PAUSE_STOP_PROCESS);
					UART_Print(buffer_Tx);
					printf("%s\n", buffer_Tx);
			}
			sleep_ms(50);
			printf("Change data to: %d\n",(uint8_t)combioven_state.toggle_state);
			dataChanged = 1;
		}

		else if (strcmp(event_name, TOGGLE_FINISHED_EVENT) == 0) {
			if (runningState == FINISHED_STATE) {
				sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
				UART_Print(buffer_Tx);
				printf("%s\n", buffer_Tx);
				combioven_state.toggle_cooling		= 0;
				combioven_state.toggle_looptime 	= 0;
				combioven_state.toggle_probe		= 0;
				combioven_state.toggle_preheat		= 0;
				combioven_state.toggle_state		= STOP_OVEN_STATE;
			}
			sleep_ms(50);
			printf("Change data to: %d\n",(uint8_t)combioven_state.toggle_state);
			dataChanged = 1;
		}


		else if (strcmp(event_name, TOGGLE_PREHEAT_EVENT) == 0) {
			if (combioven_state.toggle_preheat == 0) {
				sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
			    UART_Print(buffer_Tx);
				sleep_ms(100);
				printf("%s\n",buffer_Tx);
				combioven_state.toggle_cooling 		= 0;
				combioven_state.toggle_looptime 	= 0;
				combioven_state.toggle_probe    	= 0;
				combioven_state.current_temperature = 0;
				combioven_state.current_humidity	= 0;
				if(relayboard_state.door_status	==	1	){
					sprintf(buffer_Tx,MODE_PREHEAT);
			    	UART_Print(buffer_Tx);
					printf("%s\n",buffer_Tx);
					combioven_state.toggle_preheat  = 1;
					combioven_state.toggle_state	= RUN_SUB_STATE;
					sleep_ms(10);
				}
				else{
					combioven_state.toggle_state	= PAUSE_BY_DOOR_STATE;	//Set UI message Close Door
					combioven_state.toggle_preheat  = 1;	//Set UI Toggle_Preheat but message Close Door
					previousState=RUN_SUB_STATE;						//When Door is close will run
				}
			} 

			else {
				combioven_state.toggle_preheat 		= 0;
				combioven_state.toggle_state   		= STOP_OVEN_STATE;
				sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
			    UART_Print(buffer_Tx);
				printf("%s\n",buffer_Tx);
			}
			sleep_ms(10);
			printf("Change data to: %d\n",(uint8_t)combioven_state.toggle_preheat);
			dataChanged = 1;
		}


		else if (strcmp(event_name, TOGGLE_COOLING_EVENT) == 0) {
			if (combioven_state.toggle_cooling == 0) {
				sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
				UART_Print(buffer_Tx);
				sleep_ms(10);
				printf("%s\n", buffer_Tx);
				combioven_state.toggle_preheat 		= 0;
				combioven_state.toggle_looptime 	= 0;
				combioven_state.toggle_probe    	= 0;
				combioven_state.current_temperature = 0;
				combioven_state.current_humidity	= 0;
				combioven_state.target_fanspeed 	= 100;
				combioven_state.target_temperature 	= 60;
				combioven_state.toggle_cooling  	= 1;
				combioven_state.toggle_state    	= RUN_SUB_STATE;
				combioven_state.current_temperature = 300;
				if(relayboard_state.encoder_activated < ENCODER_DISABLE)
				{
					relayboard_state.encoder_activated = ENCODER_DISABLE;
					sprintf(buffer_Tx,ENCODER_OFF);
			    	UART_Print(buffer_Tx);
					sleep_ms(20);
					printf("%s\n",buffer_Tx);
					Update_Parameters(&combioven_state, &relayboard_state);				
				}
				sprintf(buffer_Tx,MODE_COOLING);
			    UART_Print(buffer_Tx);
				printf("%s\n",buffer_Tx);
			} 
			
			else {
				combioven_state.toggle_cooling 		= 0;
				combioven_state.toggle_state    	= STOP_OVEN_STATE;
				sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
			    UART_Print(buffer_Tx);
				printf("%s\n",buffer_Tx);
			}
			sleep_ms(10);
			printf("Change data to: %d\n",(uint8_t)combioven_state.toggle_cooling);
			dataChanged = 1;
		}


		else if (strcmp(event_name, TOGGLE_PROBE_EVENT) == 0) {
			if (combioven_state.toggle_probe == 0) {
				sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
				UART_Print(buffer_Tx);
				sleep_ms(100);
				printf("%s\n", buffer_Tx);
				combioven_state.toggle_looptime 	= 0;
				combioven_state.toggle_cooling		= 0;
				combioven_state.target_time			= 0;
				combioven_state.toggle_preheat		= 0;
				combioven_state.current_temperature = 0;
				combioven_state.current_probe 		= 0;
				if(relayboard_state.door_status	==	1	){
					sprintf(buffer_Tx,RUNNING_PROCESS);
			    	UART_Print(buffer_Tx);
					printf("%s\n",buffer_Tx);
					combioven_state.toggle_probe  	= 1;
					combioven_state.toggle_state  	= RUN_SUB_STATE;
					sleep_ms(10);
				}
				else{
					combioven_state.toggle_state	= PAUSE_BY_DOOR_STATE;	//Set UI message Close Door
					combioven_state.toggle_probe  	= 1;	//Set UI Toggle_Probe but message Close Door
					previousState=RUN_SUB_STATE;						//When Door is close will run
				}
			}
			else {
				combioven_state.toggle_probe 		= 0;
				combioven_state.toggle_state 		= STOP_OVEN_STATE;
				sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
				UART_Print(buffer_Tx);
				printf("%s\n",buffer_Tx);
			}
			sleep_ms(10);
			printf("Change data to: %d\n",(uint8_t)combioven_state.toggle_probe);
			dataChanged = 1;
		}


		else if (strcmp(event_name, TOGGLE_LOOPTIME_EVENT) == 0) {
			if (combioven_state.toggle_looptime == 0) {
				sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
			    UART_Print(buffer_Tx);
				printf("%s\n",buffer_Tx);
				sleep_ms(50);
				combioven_state.toggle_cooling 		= 0;
				combioven_state.toggle_preheat  	= 0;
				if(relayboard_state.door_status	==	1	){
					sprintf(buffer_Tx,RUNNING_PROCESS);
			    	UART_Print(buffer_Tx);
					printf("%s\n",buffer_Tx);
					combioven_state.toggle_looptime = 1;
					combioven_state.toggle_state	= RUN_SUB_STATE;
					sleep_ms(10);
				}
				else{
					combioven_state.toggle_state	= PAUSE_BY_DOOR_STATE;	//Set UI message Close Door
					combioven_state.toggle_looptime = 1;	//Set UI Toggle_LoopTime but message Close Door
					previousState=RUN_SUB_STATE;						//When Door is close will run
				}
			} 
			else {
				combioven_state.toggle_looptime 	= 0;
				combioven_state.toggle_state 		= STOP_OVEN_STATE;
				sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
			    UART_Print(buffer_Tx);
				printf("%s\n",buffer_Tx);
			}
			sleep_ms(10);
			printf("Change data to: %d\n",(uint8_t)combioven_state.toggle_looptime);
			dataChanged = 1;
		}


		else if (strcmp(event_name, SET_WARNING_STATE_EVENT) == 0) {
			set_warning_state_event_t* uidata = (set_warning_state_event_t*)event_data;
			combioven_state.toggle_state = uidata->warningcode;
			if(combioven_state.toggle_state == OVERHEAT_STATE){
				combioven_state.toggle_looptime 	= 0;
				combioven_state.toggle_preheat  	= 0;
				combioven_state.toggle_probe 		= 0;
				combioven_state.toggle_cooling 		= 0;
				sprintf(buffer_Tx, "#serv009");    //Turn off LED bar of LEFT side 
				UART_Print(buffer_Tx); 
				printf("%s\n", buffer_Tx);
				sleep_ms(500);
				sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
				UART_Print(buffer_Tx);
				printf("%s\n",buffer_Tx);
				sleep_ms(10);
			}
			printf("Change data to: %d\n",(uint8_t)combioven_state.toggle_state);
			dataChanged = 1;
		}

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
	relayboard_state.encoder_activated 			= ENCODER_DISABLE;
	relayboard_state.washing_mode				= 0;
	relayboard_state.washing_phase				= 0;
	relayboard_state.door_status				= 1;
	relayboard_state.completed_step				= 0;
	relayboard_state.last_step					= 0;
	relayboard_state.warning_code				= 0;
	relayboard_state.toggle_relay				= 0;
	relayboard_state.delta_temperature			= 0;	
	relayboard_state.hitemp_timeout				= 0;
	combioven_state.target_temperature			= 30;	//30
	combioven_state.encoder_data				= 0;
	combioven_state.target_steam    			= 0;
	combioven_state.target_time     			= 0;
	combioven_state.target_fanspeed 			= 25;
	combioven_state.target_probe    			= 10; //10
	combioven_state.current_probe   			= 0;
	combioven_state.current_humidity			= 0;
	combioven_state.toggle_preheat  			= 0;
	combioven_state.toggle_cooling  			= 0;
	combioven_state.toggle_state    			= 0;
	combioven_state.toggle_probe    			= 0;
	combioven_state.toggle_looptime 			= 0;
	combioven_state.encoder_parameter			= 0;
	recipe_active.totalsteps					= 0;
	recipe_active.actualstep					= 0;		
	recipe_active.typestep						= 0;
	recipe_active.currlevel						= 0;
	encoder_options.minvalue 					= 0;
	encoder_options.maxvalue 					= 0;
	encoder_options.nowvalue 					= 0;
	encoder_options.parameter					= 0;
	//combioven_state.units = 1; //0-Farenheit 1-Celsius

	if (init_mutex() != 0) {
		fprintf(stderr,"Mutex init failed\n");
		return 0;
	}
   // open UI port


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
	1 = running preheat / cooling / looptime / external probe / delta
	2 = running manual mode with time 
	3 = running automatic by steps Ok
	4 = running Multilevel
	5 = running washing Ok
	6 = ready for next step
	7 = pause / cerrar puerta
	8 = finished state
	9 = conectar agua
	10 = warning state 
*/
	while(1) {
		sleep_ms(SNOOZE_TIME);
		seconds = difftime(time(NULL),timer);

	    lock_mutex();
		//printf("state:%d run:%d step:%d completed:%d\n",(uint8_t)combioven_state.toggle_state, (uint8_t)runningState, (uint8_t)recipe_active.actualstep, (uint8_t)relayboard_state.completed_step);
		//running free time / preheat / cooling / looptime
		if( (seconds > 5) && (runningState == STOP_OVEN_STATE) && (relayboard_state.encoder_activated == ENCODER_DISABLE))
		{
			sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
			UART_Print(buffer_Tx);
			//printf("%s\n",buffer_Tx);
			recipe_active.actualstep			= 0;
			combioven_state.toggle_preheat 		= 0;
			combioven_state.toggle_cooling		= 0;
			combioven_state.toggle_probe		= 0;
			combioven_state.toggle_looptime		= 0;
			timer = time(NULL);
		}


		else if ( (seconds > 1) && (runningState == RUN_SUB_STATE) ) //
		{ 
			if ( (combioven_state.toggle_preheat == 1) && (relayboard_state.door_status==1) && (relayboard_state.current_cam_temperature != combioven_state.current_temperature))
			{
				combioven_state.current_temperature = relayboard_state.current_cam_temperature;
				printf("heat:%3d\n",(uint16_t)combioven_state.current_temperature);
				dataChanged = 1;
				timer = time(NULL);
			}
		
			else if (combioven_state.toggle_cooling == 1)
			{
				combioven_state.current_temperature = relayboard_state.current_cam_temperature;
				printf("cool:%3d\n",(uint16_t)combioven_state.current_temperature);
				//printf("targ:%3d\n",(uint16_t)combioven_state.target_temperature);
				dataChanged = 1;
				timer = time(NULL);
			}

			else if ((combioven_state.toggle_looptime == 1) && (relayboard_state.door_status==1))
			{	if (relayboard_state.hitemp_timeout > 1000)
				{
					//printf("timeout: %d\n", (uint32_t)relayboard_state.hitemp_timeout);
					relayboard_state.hitemp_timeout -= 1;
				}
				else if (relayboard_state.hitemp_timeout == 1000)
				{
					relayboard_state.hitemp_timeout = 0;
					printf("timeout: %d\n", (uint32_t)relayboard_state.hitemp_timeout);
					combioven_state.target_temperature = 250;
					sprintf(buffer_Tx, "#temp%3d", (uint16_t)combioven_state.target_temperature);
					UART_Print(buffer_Tx);
					printf("%s\n", buffer_Tx);
					sleep_ms(10);
					dataChanged = 1;
				}
				timer = time(NULL);
			}

			else if ((combioven_state.toggle_probe == 1) && (relayboard_state.door_status==1))
			{		
				combioven_state.current_temperature = relayboard_state.current_cam_temperature;
				if((combioven_state.current_probe>=combioven_state.target_probe) && (combioven_state.current_temperature>combioven_state.target_temperature)){
					combioven_state.toggle_state = FINISHED_STATE;
					combioven_state.toggle_probe = 0;
					relayboard_state.current_cam_temperature = 0; //se agrega para reiniciar la lectura
					runningState = combioven_state.toggle_state;
					sprintf(buffer_Tx,FINISHED_PROCESS);
			    	UART_Print(buffer_Tx);
					printf("%s\n",buffer_Tx);	
					dataChanged=1;
					timer = time(NULL);			
				}

				else if(seconds > 2) 
				{
					sprintf(buffer_Tx,GET_EXTERN_PROBE_TEMP);
			    	UART_Print(buffer_Tx);
					//printf("fckHere\n");
					//printf("%s\n",buffer_Tx);
					timer = time(NULL);
				}
			}
		}
		
		//running manual mode with time Ok
		else if ( (seconds > 0.5) && (runningState == RUN_MANUAL_STATE) && (combioven_state.toggle_looptime != 1) && (relayboard_state.door_status==1)) 
		{
			if (combioven_state.target_time > 0){
				combioven_state.target_time -= 1;
				printf("seconds: %d\n",(uint32_t)combioven_state.target_time);
				if (relayboard_state.hitemp_timeout > 1)
				{
					relayboard_state.hitemp_timeout -= 1;
				}
				
				else if (relayboard_state.hitemp_timeout == 1)
				{
					relayboard_state.hitemp_timeout = 0;
					//printf("timeout: %d\n", (uint32_t)relayboard_state.hitemp_timeout);
					combioven_state.target_temperature = 250;
					sprintf(buffer_Tx, "#temp%3d", (uint16_t)combioven_state.target_temperature);
					UART_Print(buffer_Tx);
					//printf("%s\n", buffer_Tx);
					sleep_ms(10);
				}
			}

			else {
				combioven_state.toggle_state = FINISHED_STATE;
				runningState = combioven_state.toggle_state;
				sprintf(buffer_Tx,FINISHED_PROCESS);
			    UART_Print(buffer_Tx);
				printf("%s\n",buffer_Tx);
			}
			timer = time(NULL);
			dataChanged = 1;
		}

		//running automatic by steps Ok
		else if ((seconds > 0.5) && (runningState == RUN_AUTO_STATE) && (relayboard_state.door_status==1))
		{
			//TYPE_STEP: 0:User_Action / 1:Preheat / 2:Timer / 3:TempCore / 4:DeltaCore
			if(recipe_active.typestep == 0)
			{
				if( relayboard_state.completed_step == 1 ){
					sleep_ms(500);
					combioven_state.toggle_state 		= RDY_NEXTSTEP_STATE;
					relayboard_state.completed_step		= 0;
					runningState = combioven_state.toggle_state;
					dataChanged = 1;
				}
				timer = time(NULL);
			}

			else if(recipe_active.typestep == 1)
			{
				if( relayboard_state.completed_step==1 ){
					sleep_ms(500);
					combioven_state.toggle_state 		= RDY_NEXTSTEP_STATE;
					relayboard_state.completed_step		= 0 ;
					runningState = combioven_state.toggle_state;
					dataChanged = 1;
				}

				else if ((relayboard_state.completed_step == 0) && (relayboard_state.current_cam_temperature != combioven_state.current_temperature)) {
					combioven_state.current_temperature = relayboard_state.current_cam_temperature;
					dataChanged = 1;
				}
				timer = time(NULL);
			}
			
			else if(recipe_active.typestep == 2)
			{
				if (combioven_state.target_time > 0 ) {
					combioven_state.target_time -= 1;
					printf("seconds:%d\n",(uint32_t)combioven_state.target_time);
					if (relayboard_state.hitemp_timeout > 0)
					{
						relayboard_state.hitemp_timeout -= 1;
					}
					else if (relayboard_state.hitemp_timeout == 1)
					{
						relayboard_state.hitemp_timeout = 0;
						//printf("timeout: %d\n", (uint32_t)relayboard_state.hitemp_timeout);
						combioven_state.target_temperature = 250;
						sprintf(buffer_Tx, "#temp%3d", (uint16_t)combioven_state.target_temperature);
						UART_Print(buffer_Tx);
						//printf("%s\n", buffer_Tx);
						sleep_ms(10);
					}
				}

				else if ( (combioven_state.target_time == 0) && (recipe_active.actualstep < recipe_active.totalsteps)) {
					sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
					UART_Print(buffer_Tx);
					printf("%s", buffer_Tx);
					sleep_ms(500);
					combioven_state.toggle_state 		= RDY_NEXTSTEP_STATE;
					relayboard_state.completed_step		= 0;
					runningState = combioven_state.toggle_state;
				}

				else if ( (combioven_state.target_time == 0) && (recipe_active.actualstep == recipe_active.totalsteps)) {
					combioven_state.toggle_state 		= FINISHED_STATE;
					runningState = combioven_state.toggle_state;
					sprintf(buffer_Tx,FINISHED_PROCESS);
					UART_Print(buffer_Tx);
					printf("%s",buffer_Tx);
				}
				timer = time(NULL);
				dataChanged = 1;
			}

			else if(recipe_active.typestep == 3) // && (relayboard_state.delta_temperature == 0))  //cookingMode = TcoreProbe
			{
				combioven_state.current_temperature = relayboard_state.current_cam_temperature;
				if ((combioven_state.current_probe >= combioven_state.target_probe) && (combioven_state.current_temperature >= combioven_state.target_temperature)) 
				{
					if(recipe_active.actualstep == recipe_active.totalsteps) 
					{
						combioven_state.toggle_state 	= FINISHED_STATE;
						runningState = combioven_state.toggle_state;
						sprintf(buffer_Tx, FINISHED_PROCESS);
						UART_Print(buffer_Tx);
						printf("%s", buffer_Tx);
					}
					
					else
					{
						sleep_ms(500);
						combioven_state.toggle_state 	= RDY_NEXTSTEP_STATE;
						relayboard_state.completed_step = 0;
						runningState = combioven_state.toggle_state;
					}
					
				}

				else
				{
					sprintf(buffer_Tx, GET_EXTERN_PROBE_TEMP);
					UART_Print(buffer_Tx);
					printf("%s\n", buffer_Tx);
				}
				timer = time(NULL);
				dataChanged = 1;
			}

			else if (recipe_active.typestep == 4) //&& (relayboard_state.delta_temperature != 0)) //cookingMode = TcoreDelta
			{
				int checkDelta = combioven_state.target_temperature - relayboard_state.delta_temperature;
				//printf("ckDelta:%d\n",checkDelta);
				if (combioven_state.current_probe > checkDelta) 
				{
					if ( (combioven_state.current_probe>=combioven_state.target_probe) && (recipe_active.actualstep == recipe_active.totalsteps)){
						combioven_state.toggle_state 		= FINISHED_STATE;
						runningState = combioven_state.toggle_state;
						sprintf(buffer_Tx, FINISHED_PROCESS);
						UART_Print(buffer_Tx);
						printf("%s", buffer_Tx);
					}

					else if ((combioven_state.current_probe >= combioven_state.target_probe) && (recipe_active.actualstep < recipe_active.totalsteps)){
						sleep_ms(500);
						combioven_state.toggle_state 		= FINISHED_STATE;
						relayboard_state.completed_step 	= 0;
						runningState = combioven_state.toggle_state;
						//printf("probeOkDeltaNEXT\n");
					}
				
					else
					{
						sleep_ms(100);
						combioven_state.target_temperature = combioven_state.current_probe + relayboard_state.delta_temperature;
						sprintf(buffer_Tx,"#temp%3d",(uint16_t)combioven_state.target_temperature);
						UART_Print(buffer_Tx);
						printf("%s\n", buffer_Tx);
						sleep_ms(2);	
					}
				}
			
				else
				{
					sprintf(buffer_Tx, GET_EXTERN_PROBE_TEMP);
					UART_Print(buffer_Tx);
					printf("%s\n", buffer_Tx);
					combioven_state.current_temperature = relayboard_state.current_cam_temperature;
				}
				timer = time(NULL);
				dataChanged = 1;
			}
		}

		//Multi-Level state
		else if ((seconds > 0.5) && (runningState == RUN_MULTILEVEL_STATE) && (relayboard_state.door_status==1) )
		{
			//printf("state:%d run:%d step:%d completed:%d\n",(uint8_t)combioven_state.toggle_state, (uint8_t)runningState, (uint8_t)recipe_active.actualstep, (uint8_t)relayboard_state.completed_step);
			//TYPE_STEP: 0:User_Action / 1:Preheat / 2:Timer 
			if(recipe_active.typestep == 0)
			{
				if( relayboard_state.completed_step == 1 ){
					sleep_ms(500);
					combioven_state.toggle_state 		= RDY_NEXTSTEP_STATE;
					relayboard_state.completed_step		= 0;
					runningState = combioven_state.toggle_state;
					//printf("load completed\n");
					//timer = time(NULL);
					dataChanged = 1;
				}
			}

			else if(recipe_active.typestep == 1)
			{
				if( relayboard_state.completed_step==1 ){
					sleep_ms(500);
					combioven_state.toggle_state 		= RDY_NEXTSTEP_STATE;
					relayboard_state.completed_step		= 0 ;
					runningState = combioven_state.toggle_state;
					//printf("next_type1..\n");
					//timer = time(NULL);
					dataChanged = 1;
				}

				else if ((relayboard_state.completed_step == 0) && (relayboard_state.current_cam_temperature != combioven_state.current_temperature)) {
					combioven_state.current_temperature = relayboard_state.current_cam_temperature;
					printf("nowtemp:%d\n",(uint16_t)combioven_state.current_temperature);
					//timer = time(NULL);
					dataChanged = 1;
				}
			}
			
			else if(recipe_active.typestep == 2)
			{
				if (combioven_state.target_time > 0 ) {
					combioven_state.target_time -= 1;
					printf("seconds:%d\n",(uint32_t)combioven_state.target_time);
					if (relayboard_state.hitemp_timeout > 0)
					{
						relayboard_state.hitemp_timeout -= 1;
					}
					else if (relayboard_state.hitemp_timeout == 1)
					{
						relayboard_state.hitemp_timeout = 0;
						//printf("timeout: %d\n", (uint32_t)relayboard_state.hitemp_timeout);
						combioven_state.target_temperature = 250;
						sprintf(buffer_Tx, "#temp%3d",(uint16_t)combioven_state.target_temperature);
						UART_Print(buffer_Tx);
						printf("%s\n", buffer_Tx);
						sleep_ms(10);
					}
					dataChanged = 1;
				}

				else if ( (combioven_state.target_time == 0) && (recipe_active.actualstep < recipe_active.totalsteps)) {
					sprintf(buffer_Tx, "#aler%3d",((uint8_t)recipe_active.currlevel));
					UART_Print(buffer_Tx);
					printf("%s", buffer_Tx);
					
					previousState = combioven_state.toggle_state;
					combioven_state.toggle_state = RDY_NEXTSTEP_STATE;
					relayboard_state.completed_step		= 0;
					runningState = RUN_MULTILEVEL_STATE;
					dataChanged = 1;
				}

				else if ( (combioven_state.target_time == 0) && (recipe_active.actualstep == recipe_active.totalsteps)) {
					sprintf(buffer_Tx, "#aler%3d",((uint8_t)recipe_active.currlevel));
					UART_Print(buffer_Tx);
					printf("%s", buffer_Tx);
					
					combioven_state.toggle_state 		= FINISHED_STATE;
					runningState = combioven_state.toggle_state;
					sprintf(buffer_Tx,FINISHED_PROCESS);
					UART_Print(buffer_Tx);
					printf("%s",buffer_Tx);
					dataChanged = 1;
				}
			}
			timer = time(NULL);	
		}	

		//running washing cheking temperature before cycle 
		else if( (seconds > 0.5) && (runningState == RUN_WASHING_STATE) && (combioven_state.toggle_cooling == 1) )
		{	
			combioven_state.current_temperature = relayboard_state.current_cam_temperature;
			if (combioven_state.current_temperature > combioven_state.target_temperature)
			{
				printf("cur_temp:%d\n",(uint16_t)combioven_state.current_temperature);
				dataChanged = 1;
				timer = time(NULL);
			}
		}
		
		//running washing Ok
		else if( (seconds > 0.5) && (runningState == RUN_WASHING_STATE) && (relayboard_state.door_status==1) && (combioven_state.toggle_cooling == 0))
		{
			//printf("state:%d run:%d subpr:%d wcycle:%d\n",(uint8_t)combioven_state.toggle_state, (uint8_t)runningState, (uint8_t)relayboard_state.washing_phase, (uint8_t)relayboard_state.washing_mode);
			if (combioven_state.target_time > 0) 
			{
				combioven_state.target_time -= 1;
				printf("seconds: %d\n",(uint32_t)combioven_state.target_time);
				Washing_Process(&relayboard_state,relayboard_state.washing_mode,relayboard_state.washing_phase,combioven_state.target_time);
			}			

			else 
			{
				combioven_state.toggle_state = FINISHED_STATE;
				runningState = combioven_state.toggle_state;
				sprintf(buffer_Tx,FINISHED_PROCESS);
			    UART_Print(buffer_Tx);
				printf("%s",buffer_Tx);
			}
			timer = time(NULL);
			dataChanged = 1;
		}

		//ready for next step
		else if((seconds > 0.5) && (runningState == RDY_NEXTSTEP_STATE) )
		{
			if ((relayboard_state.door_status == 1) && (relayboard_state.completed_step == 1))
			{
				sleep_ms(100);
				combioven_state.toggle_state 	= RDY_NEXTSTEP_STATE;
				relayboard_state.completed_step = 0;
				runningState = combioven_state.toggle_state;
				dataChanged = 1;
				//printf("stay_in.\n");
			}
			timer = time(NULL);
		}

		//Finish state
		else if ((seconds > 4) && (runningState == FINISHED_STATE))
		{
			recipe_active.totalsteps					= 0;
			recipe_active.actualstep					= 0;		
			recipe_active.typestep						= 0;
			combioven_state.toggle_state 				= STOP_OVEN_STATE;
			runningState = combioven_state.toggle_state;
			printf("Change data to: %d\n",(uint8_t)combioven_state.toggle_state);
			dataChanged = 1;
			timer = time(NULL);
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

void Update_Parameters(combioven_update_event_t *combiOven, relayboard_update_event_t *relayboard)
{
	sleep_ms(500);
	sprintf(buffer_Tx,"#stem%3d",(uint8_t)combioven_state.target_steam);
	UART_Print(buffer_Tx);
	printf("%s",buffer_Tx);
	sleep_ms(500);
	sprintf(buffer_Tx,"#temp%3d",(uint16_t)combioven_state.target_temperature);
	UART_Print(buffer_Tx);
	printf("%s",buffer_Tx);				
	sleep_ms(500);
	sprintf(buffer_Tx,"#sped%3d",(uint8_t)combioven_state.target_fanspeed);
	UART_Print(buffer_Tx);
	printf("%s",buffer_Tx);
	if (combioven_state.target_temperature > 260)
	{
		relayboard_state.hitemp_timeout = 1800;			//start a timer for hi temperature
	}
}

/****************************************************************************************************************************
 *
 *                     D e f i n i t i o n   f o r    W A S H I N G    C Y C L E S <
 * 
 ****************************************************************************************************************************/
void Washing_Process(relayboard_update_event_t *relayboard, uint8_t modeSelected, uint8_t phaseStatus, uint32_t timeElapse)
{

	switch (modeSelected)
	{
	case 1:		//// *****	E	N	J	U	A	G	U	E		 R	A	P	I	D	O   ******* ///
		if (phaseStatus == 0) {
			sleep_ms(1000);
			sprintf(buffer_Tx, PHASE_DRAIN_WASTE);		//25 sec
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
			relayboard_state.washing_phase = 99;  		//IDLE status
		}

		else if (timeElapse == 695) {					//520 sec include Drain_Waste
			sprintf(buffer_Tx, PHASE_WASH_OUT);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 175 ){	//135 secs
			sprintf(buffer_Tx, PHASE_CLEAN_JET);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 25 ) {	//25 secs
			sprintf(buffer_Tx, PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}
		break;


	case 2:		//// *****	D	E	S	C	A	L	C	I	F	I	C	A	D	O   ******* ///
		if (phaseStatus == 0) { 						//25 sec
			sleep_ms(1000);
			sprintf(buffer_Tx, PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
			relayboard_state.washing_phase = 99;  	//IDLE status
		}

		else if (timeElapse == 1415) {					//360 sec	
			sprintf(buffer_Tx, PHASE_PREHEAT_BOILER);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if(timeElapse == 1055) {
			sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
			UART_Print(buffer_Tx);
			printf("%s", buffer_Tx);
		}

		else if(timeElapse == 1025) {
			sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
			UART_Print(buffer_Tx);
			printf("%s", buffer_Tx);
		}


		else if (timeElapse == 1020) {					//90 sec	//DISPENSAR DESCALCIFICANTE:
			sprintf(buffer_Tx, PHASE_SUPPLY_DISCALER);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}


		else if (timeElapse == 930) {					//520 sec
			sprintf(buffer_Tx, PHASE_WASH_OUT);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 325 || timeElapse == 160){	//140
			sprintf(buffer_Tx, PHASE_CLEAN_JET);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 185 || timeElapse == 25) {	//25 sec
			sprintf(buffer_Tx, PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}
		break;


	case 3:	//// *****	L	A	V	A	D	O		 E	C	O	3600 SEC ******* ///
		if (phaseStatus == 0) {							//28 sec
			sleep_ms(1000);
			sprintf(buffer_Tx, PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
			relayboard_state.washing_phase = 99;  		//IDLE status
		}

		else if (timeElapse == 2432) {					//60 sec
			sprintf(buffer_Tx, PHASE_CLEAN_JET);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}
//		****************** REPETIR CICLO  *******************************
		else if (timeElapse == 2373	|| timeElapse == 1478) {  	// 27 sec
			sprintf(buffer_Tx, PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 2346 || timeElapse == 1451) {	//420,190 sec  
			sprintf(buffer_Tx, PHASE_PREHEAT_BOILER);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 1926 || timeElapse == 1261) {	//10 sec	
			sprintf(buffer_Tx, PHASE_FILL_COLD_CAMERA);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 1916	){						 	//20 sec	//DISPENSAR DETERGENTE:
			sprintf(buffer_Tx, PHASE_SUPPLY_SOAP);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);	
		}

		else if (timeElapse == 1898 || timeElapse == 1251) { //420 sec
			sprintf(buffer_Tx, PHASE_RECYCLE_WATER);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}
//		*********************** FIN DE REPETIR CICLO ***************
		else if (timeElapse == 754 || timeElapse == 581) {  //28 sec
			sprintf(buffer_Tx, PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

//		*********************** CICLOS DE ENJUAGUE  ***************
		else if (timeElapse == 725 || timeElapse == 553 || timeElapse == 382 || timeElapse == 210) {  //145 sec 
			sprintf(buffer_Tx, PHASE_CLEAN_JET);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 409 || timeElapse == 237 || timeElapse == 65) {  //27 sec 
			sprintf(buffer_Tx, PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 38) {						//9 sec	
			sprintf(buffer_Tx, PHASE_FILL_COLD_CAMERA);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 29) {  //27 sec
			sprintf(buffer_Tx, PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}
		break;



	case 4:	   //// *****	L	A	V	A	D	O 			I	N	T	E	R	M	E	D	I	O   2460 SEC ******* ///
		if (phaseStatus == 0) {							//28 sec
			sleep_ms(1000);
			sprintf(buffer_Tx, PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
			relayboard_state.washing_phase = 99;  		//IDLE status
		}

		else if (timeElapse == 3572) {					//120 sec
			sprintf(buffer_Tx, PHASE_CLEAN_JET);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}
//		****************** REPETIR CICLO  *******************************
		else if (timeElapse == 3452	|| timeElapse == 2137) {  	// 27 sec
			sprintf(buffer_Tx, PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 3425 || timeElapse == 2110) {	//540,360 sec  
			sprintf(buffer_Tx, PHASE_PREHEAT_BOILER);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 2885 || timeElapse == 1750) {	//8 sec	
			sprintf(buffer_Tx, PHASE_FILL_COLD_CAMERA);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 2870	){						 	//20 sec	//DISPENSAR DETERGENTE:
			sprintf(buffer_Tx, PHASE_SUPPLY_SOAP);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);	
		}

		else if (timeElapse == 2840 || timeElapse == 1730) { 	//720 sec
			sprintf(buffer_Tx, PHASE_RECYCLE_WATER);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}
//		*********************** FIN DE REPETIR CICLO ***************
		else if (timeElapse == 1022 || timeElapse == 760) {  //27 sec
			sprintf(buffer_Tx, PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 995 ) {						//90 sec	//DISPENSAR DESCALCIFICANTE:
			sprintf(buffer_Tx, PHASE_SUPPLY_DISCALER);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);	
		}

//		*********************** CICLOS DE ENJUAGUE  ***************
		else if (timeElapse == 905 || timeElapse == 733 || timeElapse == 561 || timeElapse == 389) {  //145 sec 
			sprintf(buffer_Tx, PHASE_CLEAN_JET);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 588 || timeElapse == 416 || timeElapse == 239) {  //27 sec 
			sprintf(buffer_Tx, PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 212) {						//9 sec	
			sprintf(buffer_Tx, PHASE_FILL_COLD_CAMERA);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 176 ) {						//8 sec	
			sprintf(buffer_Tx, PHASE_WASH_OUT);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 203 || timeElapse == 29) {  //27 sec
			sprintf(buffer_Tx, PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}
		break;		
		


	case 5:			//// *****	L	A	V	A	D	O		R	E	G	U	L	A	R	5700 SEC  ******* ///
		if (phaseStatus == 0) {							//28 sec
			sleep_ms(1000);
			sprintf(buffer_Tx, PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
			relayboard_state.washing_phase = 99;  		//IDLE status
		}

		else if (timeElapse == 5672) {					//150 sec
			sprintf(buffer_Tx, PHASE_CLEAN_JET);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

//		****************** REPETIR CICLO  *******************************
		else if (timeElapse == 5522	|| timeElapse == 3967) {  	// 27 sec
			sprintf(buffer_Tx, PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 5495 || timeElapse == 3940) {	//600,600 sec  
			sprintf(buffer_Tx, PHASE_PREHEAT_BOILER);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 4895 || timeElapse == 4885 || timeElapse == 3340 || timeElapse == 3330 ) { 
			sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
			UART_Print(buffer_Tx);
			printf("%s", buffer_Tx);
		}

		else if (timeElapse == 4875 || timeElapse == 3320) {	//8 sec	
			sprintf(buffer_Tx, PHASE_FILL_COLD_CAMERA);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 4865 || timeElapse == 3300){						 	//20 sec	//DISPENSAR DETERGENTE:
			sprintf(buffer_Tx, PHASE_SUPPLY_SOAP);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);	
		}

		else if (timeElapse == 4845 || timeElapse == 3280) { 	//900 sec
			sprintf(buffer_Tx, PHASE_RECYCLE_WATER);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}
//		*********************** FIN DE REPETIR CICLO ***************
		else if (timeElapse == 2412 || timeElapse == 1484) {  //27 sec
			sprintf(buffer_Tx, PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 2385 ) {						//90 sec	//DISPENSAR DESCALCIFICANTE:
			sprintf(buffer_Tx, PHASE_SUPPLY_DISCALER);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);	
		}

//		*********************** CICLOS DE ENJUAGUE  ***************
		else if (timeElapse == 2295 || timeElapse == 1457 || timeElapse == 1280 || timeElapse == 1103) {  //145 sec 
			sprintf(buffer_Tx, PHASE_CLEAN_JET);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 2150 || timeElapse == 1307 || timeElapse == 1130 || timeElapse == 953) {  //27 sec 
			sprintf(buffer_Tx, PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 2123) {						//90 sec
			sprintf(buffer_Tx, PHASE_COOLING_CAMERA);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 2033) {						//9 sec	
			sprintf(buffer_Tx, PHASE_FILL_COLD_CAMERA);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 2024 || timeElapse == 926 ) {	//540 sec	
			sprintf(buffer_Tx, PHASE_WASH_OUT);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 386 || timeElapse == 239 || timeElapse == 92) {		//	120 sec
			sprintf(buffer_Tx, PHASE_CLEAN_JET);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 266 || timeElapse == 119 || timeElapse == 29) {  //27 sec
			sprintf(buffer_Tx, PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}
		break;


	case 6:			//// *****	L	A	V	A	D 	O		I	N	T	E	N	S	O	9000 SECS  ******* ///
		if (phaseStatus == 0) {							//28 sec
			sleep_ms(1000);
			sprintf(buffer_Tx, PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
			relayboard_state.washing_phase = 99;  		//IDLE status
		}

		else if (timeElapse == 8972 ) {					//150 sec
			sprintf(buffer_Tx, PHASE_CLEAN_JET);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

//		****************** REPETIR CICLO  *******************************
		else if (timeElapse == 8822	|| timeElapse == 6638 || timeElapse == 4481) {  	// 27 sec
			sprintf(buffer_Tx, PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 8790 || timeElapse == 6608 || timeElapse == 4454) {	//420,600 sec  
			sprintf(buffer_Tx, PHASE_PREHEAT_BOILER);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 8370 || timeElapse == 6188 || timeElapse == 6175 || timeElapse == 4034 || timeElapse == 4024) { 
			sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
			UART_Print(buffer_Tx);
			printf("%s", buffer_Tx);
		}


		else if (timeElapse == 8365 || timeElapse == 6165 || timeElapse == 4014) {	//10 sec	
			sprintf(buffer_Tx, PHASE_FILL_COLD_CAMERA);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 8330 || timeElapse == 6140 || timeElapse == 3990){						 	//20 sec	//DISPENSAR DETERGENTE:
			sprintf(buffer_Tx, PHASE_SUPPLY_SOAP);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);	
		}

		else if (timeElapse == 8300 || timeElapse == 6100 || timeElapse == 3930) { 	//1200 sec
			sprintf(buffer_Tx, PHASE_RECYCLE_WATER);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}
//		*********************** FIN DE REPETIR CICLO ***************
		else if (timeElapse == 2324 ) {  //27 sec
			sprintf(buffer_Tx, PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 2297 ) {						//90 sec	//DISPENSAR DESCALCIFICANTE:
			sprintf(buffer_Tx, PHASE_SUPPLY_DISCALER);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);	
		}

//		*********************** CICLOS DE ENJUAGUE  ***************
		else if (timeElapse == 2207 || timeElapse == 2030 || timeElapse == 1853 || timeElapse == 1676) {  //150 sec 
			sprintf(buffer_Tx, PHASE_CLEAN_JET);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 2057 || timeElapse == 1880 || timeElapse == 1703 || timeElapse == 1376) {  //27 sec 
			sprintf(buffer_Tx, PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}
	/*
		else if (timeElapse == 1349) {						//10 sec	
			sprintf(buffer_Tx, PHASE_FILL_COLD_CAMERA);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		} 
	*/

		else if (timeElapse == 1339 || timeElapse == 652) {	 //540 sec	
			sprintf(buffer_Tx, PHASE_WASH_OUT);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 799 || timeElapse == 112) {	//	120 sec
			sprintf(buffer_Tx, PHASE_CLEAN_JET);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}

		else if (timeElapse == 679 || timeElapse == 27) {  //27 sec
			sprintf(buffer_Tx, PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
		}
		break;

	default:
		break;
	}
}

/*
if(relayboard_state.encoder_activated < ENCODER_DISABLE)
{
		relayboard_state.encoder_activated = ENCODER_DISABLE;
		sprintf(buffer_Tx,ENCODER_OFF);
    	UART_Print(buffer_Tx);
		sleep_ms(20);
		printf("%s\n",buffer_Tx);
		Update_Parameters(&combioven_state);				
}
*/
