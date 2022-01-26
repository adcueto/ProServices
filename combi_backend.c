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
static int							dataChanged = 1;    //Default to 1 so we send data to the ui once it connects
static int							runningState = 0;
static int   						timerEncoder = 0;
static int 							fd;                 //To open uart-serial port
static int							nread;				//To get uart data lenght
static char  						buffer_Tx[255];     //Transmit data buffer to relay-board
static char							buffer_Rx[255];		//Receive data buffer from relay-board
static combioven_update_event_t	    combioven_state;
static relayboard_update_event_t	relayboard_state;


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

/****************************************************************************************************************************
 *
 *                     D e f i n i t i o n   f o r    W A S H I N G    C Y C L E S <
 * 
 ****************************************************************************************************************************/
uint8_t Washing_Process(uint8_t modeSelected, uint8_t phaseStatus, uint32_t timeElapse)
{
	if(modeSelected == 1)  //MODE_QUICK_POLISH_EVENT
	{
		if(phaseStatus == 0)
		{
			sprintf(buffer_Tx,PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
			relayboard_state.washing_phase = 99;  //IDLE status
		}

		else if(timeElapse == 690)
		{	
			sprintf(buffer_Tx,PHASE_WASH_OUT);
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
		}

		else if(timeElapse == 420)
		{
			sprintf(buffer_Tx,PHASE_DRAIN_WASTE);
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
		}

		else if(timeElapse == 390)
		{
			sprintf(buffer_Tx,PHASE_DRYING_CAMERA);
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
		}
		//DrenarResiduosyVaciarCaldera()
		//LlenarCalderaSinPastillas()
		//EjecutarCleanJet()
		//DrenarResiduosyVaciarCaldera()
		//SecarCamara()
		//FinishState()
	}


	else if(modeSelected == 2)  //MODE_FULL_POLISH_EVENT
	{
		if(phaseStatus == 0)
		{
			sprintf(buffer_Tx,PHASE_DRAIN_WASTE);	
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
			relayboard_state.washing_phase = 99;  //IDLE status
		}

		else if(timeElapse == 1475)
		{	
			sprintf(buffer_Tx,PHASE_CLEAN_CARE);
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
		}

		else if(timeElapse == 1340)
		{
			sprintf(buffer_Tx,PHASE_PREHEAT_BOILER);
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
		}

		else if(timeElapse == 1165)
		{
			sprintf(buffer_Tx,PHASE_FILL_COLD_CAMERA);
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
		}

		else if(timeElapse == 1140)
		{
			sprintf(buffer_Tx,PHASE_RECYCLE_WATER);
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
		}

		else if(timeElapse == 780)
		{
			sprintf(buffer_Tx,PHASE_WASH_OUT);
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
		}

		else if(timeElapse == 540)
		{
			sprintf(buffer_Tx,PHASE_DRAIN_WASTE);	
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
		}

		else if(timeElapse == 510)
		{
			sprintf(buffer_Tx,PHASE_DRYING_CAMERA);
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
		}

		else if(timeElapse == 150)
		{
			sprintf(buffer_Tx,PHASE_COOLING_CAMERA);
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
		}
		/*
		DrenarResiduosyVaciarCaldera()
		LlenarCalderaConPastillas()
		PrecalentarVapor80C()
		EjecutarCleanJet()
		DrenarResiduosyVaciarCaldera()
		SecarCamara()
		FinishState()	*/	
	}

	else if(modeSelected == 3)  //MODE_CLEAN_INTER_EVENT  
	{
		if(phaseStatus == 0)
		{
			sprintf(buffer_Tx,PHASE_DRAIN_WASTE);	
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
			relayboard_state.washing_phase = 99;  //IDLE status
		}

		else if(timeElapse == 3512)
		{
			sprintf(buffer_Tx,PHASE_CLEAN_JET);
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
		}

		else if(timeElapse == 3392)			//REPETIR DESDE AQUI
		{	
			sprintf(buffer_Tx,PHASE_CLEAN_CARE);
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
		}

		else if(timeElapse == 1270)
		{
			sprintf(buffer_Tx,PHASE_PREHEAT_BOILER);
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
		}

		else if(timeElapse == 910)
		{
			sprintf(buffer_Tx,PHASE_FILL_COLD_CAMERA);
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
		}

		else if(timeElapse == 884)
		{
			sprintf(buffer_Tx,PHASE_RECYCLE_WATER);
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
		}

		else if(timeElapse == 584)			//REPETIR HASTA AQUI
		{
			sprintf(buffer_Tx,PHASE_CLEAN_JET);
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
		}
	
		else if(timeElapse == 584)
		{
			sprintf(buffer_Tx,PHASE_WASH_OUT);
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
		}

		else if(timeElapse == 404)
		{
			sprintf(buffer_Tx,PHASE_DRAIN_WASTE);	
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
		}

		else if(timeElapse == 374)
		{
			sprintf(buffer_Tx,PHASE_DRYING_CAMERA);
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
		}

		else if(timeElapse == 130)
		{
			sprintf(buffer_Tx,PHASE_COOLING_CAMERA);
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
		}
	}

}


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

		    if ( strcmp(buffer_Rx, MODE_PREHEAT ) == 0 ) {
				if (combioven_state.toggle_preheat == 1) {
					combioven_state.toggle_preheat = 0;
					combioven_state.toggle_state = 0;
					printf("toggle_preheat ");
				}
				printf(" Receive Change data to: %d\n",combioven_state.toggle_preheat);
				Clear_Buffer(buffer_Rx);
				//runningState=combioven_state.toggle_state;
				dataChanged = 1;
			}

			else if ( strcmp(buffer_Rx, MODE_COOLING ) == 0 ) {
				if (combioven_state.toggle_cooling == 1) {
					combioven_state.toggle_cooling = 0;
					printf(buffer_Rx,"toggle_cooling ");
				}
				printf(" Receive Change data to: %d\n",combioven_state.toggle_cooling);
			    Clear_Buffer(buffer_Rx);
				//runningState=combioven_state.toggle_state;
				dataChanged = 1;
			}


			else if ( strstr((char *)buffer_Rx, CURRENT_TEMP_PROBE ) != NULL ) {
        		for(int i=5; i<8; i++)
        		{
        			if( cpyValue[i-5] == ' ' ){
        				cpyValue[i-5] = '0';
					}
        			else
            			cpyValue[i-5] = buffer_Rx[i];
        		}
        		relayboard_state.current_probe_temperature = atoi(cpyValue);
				sprintf(buffer_Rx,"update_probe: %d", relayboard_state.current_probe_temperature );
				printf("%s",buffer_Rx);
			    Clear_Buffer(buffer_Rx);
				if(relayboard_state.current_probe_temperature != combioven_state.current_probe)
				{
					combioven_state.current_probe = relayboard_state.current_probe_temperature;
					dataChanged = 1;
				}	
			}
			

			else if ( strstr((char *)buffer_Rx, CURRENT_HUMIDITY ) != NULL ) {
        		for(int i=5; i<8; i++)
        		{
        			if( cpyValue[i-5] == ' ' ){
        				cpyValue[i-5] = '0';
					}
        			else
            			cpyValue[i-5] = buffer_Rx[i];
        		}
        		relayboard_state.current_humidity = atoi(cpyValue);
				sprintf(buffer_Rx,"update_HR: %d", relayboard_state.current_humidity );
				printf("%s",buffer_Rx);
			    Clear_Buffer(buffer_Rx);
				if(relayboard_state.current_humidity != combioven_state.current_humidity)
				{
					combioven_state.current_humidity = relayboard_state.current_humidity;
					dataChanged = 1;
				}	
			}


			else if ( strstr((char *)buffer_Rx, WARNING_MESSAGE	) != NULL ) {
        		int warningCode=0;
				for(int i=5; i<8; i++)
        		{
        			if( cpyValue[i-5] == ' ' ){
        				cpyValue[i-5] = '0';
					}
        			else
            			cpyValue[i-5] = buffer_Rx[i];
        		}

				warningCode = atoi(cpyValue);
				sprintf(buffer_Rx,"warning code:%d\n",warningCode);
				printf("%s",buffer_Rx);
			    Clear_Buffer(buffer_Rx);
				switch(warningCode)
				{
					case 0:		combioven_state.toggle_state=1;//Reset warning
								break;
					case 231:	combioven_state.toggle_state=4;//Water supply error
								break;
					case 116:	break;
					case 117:	break;
					case 118:	break;//Thermocouple opened
					case 510:   break;//Relayboard program failure
					case 511:	break;//Communication failure
					default: 	break;
				}	
				runningState=combioven_state.toggle_state;
				dataChanged = 1;	
			}

			else if ( strstr((char *)buffer_Rx, DOOR_CLOSED ) != NULL ) {
        		relayboard_state.door_status=1;
				if((combioven_state.toggle_state == 2) && (relayboard_state.washing_mode==0))
				{
					combioven_state.toggle_state=1;  //MANUAL MODE
					sleep_ms(5);
					sprintf(buffer_Tx,RUNNING_PROCESS);
			    	UART_Print(buffer_Tx);
					printf("%s",buffer_Tx);
				}
				
				else if (relayboard_state.washing_mode>0){
					combioven_state.toggle_state=3;  //WASHING MODE 
					sleep_ms(5);
					sprintf(buffer_Tx,RUNNING_PROCESS);
			    	UART_Print(buffer_Tx);
					printf("%s",buffer_Tx);
				}

				else if(runningState == 5)
				{
					combioven_state.toggle_state=0;
				}
				runningState=combioven_state.toggle_state;
				dataChanged = 1;	
			}

			else if ( strstr((char *)buffer_Rx, DOOR_OPEN ) != NULL ) {
        		relayboard_state.door_status=0;
				if((combioven_state.toggle_state ==1) || (combioven_state.toggle_state ==3) )
				{
					combioven_state.toggle_state=2;  //GENERATE PAUSE 
					sleep_ms(5);
					sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
			    	UART_Print(buffer_Tx);
					printf("%s",buffer_Tx);
				}

				else if(runningState == 5)
				{
					combioven_state.toggle_state=0;
				}
				runningState=combioven_state.toggle_state;
				dataChanged = 1;
			}
			
			else if ( (strcmp(buffer_Rx, ENCODER_INCREASE ) == 0) && (relayboard_state.encoder_activated == 1) ){
				if (relayboard_state.encoder_parameter == 1) {
					if(combioven_state.target_steam >= MAX_TARGET_PERCENT)
					   combioven_state.target_steam = MAX_TARGET_PERCENT;
					else 
					   combioven_state.target_steam = combioven_state.target_steam + 1;
					printf("increase_steam: %d\n", combioven_state.target_steam);
					Clear_Buffer(buffer_Rx);
					dataChanged = 1;
				}

				else if (relayboard_state.encoder_parameter == 2) {
				if(combioven_state.target_temperature >= MAX_TARGET_TEMPERATURE)
					   combioven_state.target_temperature = MAX_TARGET_TEMPERATURE;
				else 
					   combioven_state.target_temperature = combioven_state.target_temperature + 1;
					printf("increase_temperature: %d\n", combioven_state.target_temperature);
					Clear_Buffer(buffer_Rx);
					dataChanged = 1;
				}

				else if (relayboard_state.encoder_parameter == 3) {
					if(combioven_state.target_time >= MAX_TARGET_TIME)
					   combioven_state.target_time = MAX_TARGET_TIME;
					else 
					   combioven_state.target_time = combioven_state.target_time + 1;
					printf("increase_time: %d\n", combioven_state.target_time);
					Clear_Buffer(buffer_Rx);
					dataChanged = 1;
				}
				
				else if (relayboard_state.encoder_parameter == 4) {
					if(combioven_state.target_fanspeed >= MAX_TARGET_PERCENT)
					   combioven_state.target_fanspeed = MAX_TARGET_PERCENT;
					else 
					   combioven_state.target_fanspeed = combioven_state.target_fanspeed + 25;
					printf("increase_fanspeed: %d\n", combioven_state.target_fanspeed);
					Clear_Buffer(buffer_Rx);
					dataChanged = 1;
				}
				UART_Print(ENCODER_ENABLE);
			}

			else if ( (strcmp(buffer_Rx, ENCODER_DECREASE ) == 0) && (relayboard_state.encoder_activated == 1) ){
				if (relayboard_state.encoder_parameter == 1) {					
					if(combioven_state.target_steam <= 0 )
					   combioven_state.target_steam = 0;
					else 
					   combioven_state.target_steam = combioven_state.target_steam - 1;
					printf("decrease_steam: %d\n", combioven_state.target_steam);
					Clear_Buffer(buffer_Rx);
					dataChanged = 1;
				}

				else if (relayboard_state.encoder_parameter == 2) {		
					if(combioven_state.target_temperature <= 30)
					   combioven_state.target_temperature = 30;
					else 
					   combioven_state.target_temperature = combioven_state.target_temperature - 1;
					printf("decrease_temperature: %d\n", combioven_state.target_temperature );
					Clear_Buffer(buffer_Rx);
					dataChanged = 1;
				}

				else if (relayboard_state.encoder_parameter == 3) {
					if(combioven_state.target_time <= 0)
					   combioven_state.target_time = 0;
					else
					   combioven_state.target_time = combioven_state.target_time - 1;
					printf("decrease_time: %d\n", combioven_state.target_time);
					Clear_Buffer(buffer_Rx);
					dataChanged = 1;
				}
				
				else if (relayboard_state.encoder_parameter == 4) {
					if(combioven_state.target_fanspeed <= 25)
					   combioven_state.target_fanspeed = 25;
					else 
					   combioven_state.target_fanspeed = combioven_state.target_fanspeed - 25;
					printf("decrease_fanspeed: %d\n", combioven_state.target_fanspeed);
					Clear_Buffer(buffer_Rx);
					dataChanged = 1;
				}
				UART_Print(ENCODER_ENABLE);
			}

			else if ( (strcmp(buffer_Rx, ENCODER_ZERO_POSITION ) == 0) && (relayboard_state.encoder_activated == 1)) {
				if(timerEncoder >= 5){
					relayboard_state.encoder_activated = 0;
					//sprintf(buffer_Tx,ENCODER_DISABLE);
			    	//UART_Print(buffer_Tx);
					printf("encoder_disable: %s\n", ENCODER_DISABLE);
					switch (relayboard_state.encoder_parameter)
					{
					case 1:		sprintf(buffer_Tx,"#stem%3d",(uint8_t) combioven_state.target_steam);
								UART_Print(buffer_Tx);
								break;
					case 2:		sprintf(buffer_Tx,"#temp%3d",(uint8_t) combioven_state.target_temperature);
								UART_Print(buffer_Tx);
								break;
					case 3:		sprintf(buffer_Tx,"#timr%d",(uint32_t) combioven_state.target_time);
								break;
					case 4:		sprintf(buffer_Tx,"#sped%3d",(uint8_t) combioven_state.target_fanspeed);
								UART_Print(buffer_Tx);
								break;
					default:
						break;
					}
					printf("%s\n",buffer_Tx);
					sleep_ms(5);
				}
				else {
				   timerEncoder++;
				   sleep_ms(2);
				}
				Clear_Buffer(buffer_Rx);
			}


			else {
				//sprintf(buffer_Tx,"dataFail: %s", relayboard_state.current_probe_temperature );
				printf("undefined:%s",buffer_Rx);
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
				printf("Ok_COM \n");
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
			sprintf(buffer_Tx,"#stem%3d",(uint8_t) combioven_state.target_steam);
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
			sleep_ms(5);
			dataChanged = 1;
		} 
		
		else if (strcmp(event_name, UPDATE_TEMPERATURE_EVENT) == 0) {
			update_temperature_event_t *uidata = (update_temperature_event_t *)event_data;
			combioven_state.target_temperature = uidata->usertemp;
			sprintf(buffer_Tx,"#temp%3d",(uint16_t) combioven_state.target_temperature);
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
			sleep_ms(5);
			dataChanged = 1;
		} 

		else if (strcmp(event_name, UPDATE_TIME_EVENT) == 0) {
			update_time_event_t *uidata = (update_time_event_t *)event_data;
			combioven_state.target_time = uidata->timer;
			if(combioven_state.toggle_state==0 && relayboard_state.door_status==1){
				combioven_state.toggle_looptime=0;
				combioven_state.toggle_state=1;
				sprintf(buffer_Tx,RUNNING_PROCESS);
			    UART_Print(buffer_Tx);
			    printf("%s",buffer_Tx);
			}
			else
				combioven_state.toggle_state=1;
			sleep_ms(5);
			runningState=combioven_state.toggle_state;
			dataChanged = 1;
		} 

		else if (strcmp(event_name, UPDATE_FANSPEED_EVENT) == 0) {
			update_fanspeed_event_t *uidata = (update_fanspeed_event_t *)event_data;
			combioven_state.target_fanspeed =  uidata->percent;
			sprintf(buffer_Tx,"#sped%3d",(uint8_t) combioven_state.target_fanspeed);
			UART_Print(buffer_Tx);
			printf("%s\n",buffer_Tx);
			sleep_ms(5);
			dataChanged = 1;
		}

		else if (strcmp(event_name, UPDATE_TEMP_PROBE_EVENT) == 0) {
			update_temp_probe_event_t* uidata = (update_temp_probe_event_t*)event_data;
			combioven_state.target_probe = uidata->usertemp;
			sprintf(buffer_Tx, "#tepb%3d", (uint8_t)combioven_state.target_probe);
			UART_Print(buffer_Tx);
			printf("%s\n", buffer_Tx);
			sleep_ms(5);
			dataChanged = 1;
		}
		
		else if (strcmp(event_name, TOGGLE_RUNNING_EVENT) == 0) {
			if (combioven_state.toggle_state == 0) {
				combioven_state.toggle_state = 1;
				sprintf(buffer_Tx,RUNNING_PROCESS);
			    UART_Print(buffer_Tx);
				printf("%s",buffer_Tx);
			} 
			else {
				combioven_state.toggle_state = 0;
				sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
			    UART_Print(buffer_Tx);
				printf("%s",buffer_Tx);
			}
			printf("Change data to: %d\n",combioven_state.toggle_state);
			dataChanged = 1;
		} 

		else if (strcmp(event_name, MODE_CONVECTION_EVENT) == 0) {
				sprintf(buffer_Tx, MODE_CONVECTION);
				UART_Print(buffer_Tx);
				printf("%s\n", buffer_Tx);
		}

		else if (strcmp(event_name, MODE_COMBINED_EVENT) == 0) {
				sprintf(buffer_Tx, MODE_COMBINED);
				UART_Print(buffer_Tx);
				printf("%s\n", buffer_Tx);
		}

		else if (strcmp(event_name, MODE_STEAM_EVENT) == 0) {
				sprintf(buffer_Tx, MODE_STEAM);
				UART_Print(buffer_Tx);
				printf("%s\n", buffer_Tx);
		}
		
		else if (strcmp(event_name, TOGGLE_COOLING_EVENT) == 0) {
			if (combioven_state.toggle_cooling == 0) {
				sprintf(buffer_Tx, PAUSE_STOP_PROCESS);
				UART_Print(buffer_Tx);
				combioven_state.toggle_preheat 	= 0;
				combioven_state.toggle_looptime = 0;
				sleep_ms(5);
				combioven_state.toggle_cooling  = 1;
				combioven_state.toggle_state    = 0;
				combioven_state.toggle_probe    = 0;
				sprintf(buffer_Tx, MODE_COOLING);
			    UART_Print(buffer_Tx);
				printf("%s",buffer_Tx);
			} 
			
			else {
				combioven_state.toggle_cooling = 0;
				sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
			    UART_Print(buffer_Tx);
				printf("%s",buffer_Tx);
			}
			sleep_ms(5);
			printf("Change data to: %d\n",combioven_state.toggle_cooling);
			dataChanged = 1;
		}

		else if (strcmp(event_name, TOGGLE_PREHEAT_EVENT) == 0) {
			if (combioven_state.toggle_preheat == 0) {
				sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
			    UART_Print(buffer_Tx);
				combioven_state.toggle_cooling 	= 0;
				combioven_state.toggle_looptime = 0;
				sleep_ms(5);
				combioven_state.toggle_preheat  = 1;
				combioven_state.toggle_state	= 0;
				combioven_state.toggle_probe    = 0;
				sprintf(buffer_Tx,MODE_PREHEAT);
			    UART_Print(buffer_Tx);
				printf("%s",buffer_Tx);
			} 
			else {
				combioven_state.toggle_preheat = 0;
				sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
			    UART_Print(buffer_Tx);
				printf("%s",buffer_Tx);
			}
			sleep_ms(5);
			printf("Change data to: %d\n",combioven_state.toggle_preheat);
			dataChanged = 1;
		}

		else if (strcmp(event_name, TOGGLE_PROBE_EVENT) == 0) {
			if (combioven_state.toggle_probe == 0) {
				combioven_state.toggle_probe = 1;
				combioven_state.toggle_looptime = 0;
				sprintf(buffer_Tx, TOGGLE_PROBE_ENABLE);
				UART_Print(buffer_Tx);
				printf("%s", buffer_Tx);
			}
			else {
				combioven_state.toggle_probe = 0;
				sprintf(buffer_Tx, TOGGLE_PROBE_DISABLE);
				UART_Print(buffer_Tx);
				printf("%s", buffer_Tx);
			}
			printf("Change data to: %d\n", combioven_state.toggle_probe);
			dataChanged = 1;
		}

		else if (strcmp(event_name, TOGGLE_LOOPTIME_EVENT) == 0) {
			if (combioven_state.toggle_looptime == 0) {
				combioven_state.toggle_looptime = 1;
				combioven_state.toggle_state    = 1;
				sprintf(buffer_Tx,RUNNING_PROCESS);
			    UART_Print(buffer_Tx);
				printf("%s",buffer_Tx);
			} else {
				combioven_state.toggle_looptime = 0;
				combioven_state.toggle_state = 0;
				sprintf(buffer_Tx,PAUSE_STOP_PROCESS);
			    UART_Print(buffer_Tx);
				printf("%s",buffer_Tx);
			}
			dataChanged = 1;
		} 

		else if (strcmp(event_name, TOGGLE_SPRAY_EVENT) == 0) {
			if (combioven_state.toggle_state == 1) {
				sprintf(buffer_Tx, WATER_SPRAY_SHOT);
				UART_Print(buffer_Tx);
				printf("%s\n", buffer_Tx);
			}
		}

		else if (strcmp(event_name, ENABLE_ENCODER_EVENT) == 0) {
			enable_encoder_event_t* uidata = (enable_encoder_event_t*) event_data;
			relayboard_state.encoder_parameter = uidata->parameter;
			//relayboard_state.encoder_activated = 1;
			//timerEncoder = 0;
			//sprintf(buffer_Tx, ENCODER_ENABLE);
			//UART_Print(buffer_Tx);
			//sleep_ms(5);
			//printf("%s Sel: %d E: %d\n", buffer_Tx, relayboard_state.encoder_parameter, relayboard_state.encoder_activated);			
		}

		
		else if (strcmp(event_name, MODE_QUICK_POLISH_EVENT ) == 0) {
			combioven_state.toggle_state = 3;
			combioven_state.target_time = 721;
			relayboard_state.washing_mode=1;
			relayboard_state.washing_phase=0;
			sprintf(buffer_Tx, "#washing");
			UART_Print(buffer_Tx);
			sleep_ms(10);
			printf("%s\n", buffer_Tx);			
		}

		else if (strcmp(event_name, MODE_FULL_POLISH_EVENT ) == 0) {
			combioven_state.toggle_state = 3;
			combioven_state.target_time = 1501;
			relayboard_state.washing_mode=2;
			relayboard_state.washing_phase=0;
			sprintf(buffer_Tx, "#washing");
			UART_Print(buffer_Tx);
			sleep_ms(10);
			printf("%s\n", buffer_Tx);			
		}

		else if (strcmp(event_name, MODE_CLEAN_INTER_EVENT ) == 0) {
			combioven_state.toggle_state = 3;
			combioven_state.target_time =3541;
			relayboard_state.washing_mode=3;
			relayboard_state.washing_phase=0;
			sprintf(buffer_Tx, "#washing");
			UART_Print(buffer_Tx);
			sleep_ms(10);
			printf("%s\n", buffer_Tx);			
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

	memset(buffer,0,sizeof(buffer));
    char test[3]="Ok";
	printf("Serial COM relayboard init: %s\n",test);
	//UART_Print(test); 

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

	//allocate memory for the thermostat state
	memset(&combioven_state, 0, sizeof(combioven_state));
	memset(&relayboard_state, 0, sizeof(relayboard_state));
	//set initial state of frontend/backend structures 
	relayboard_state.current_cam_temperature   = 0;	
	relayboard_state.current_probe_temperature = 0;
	relayboard_state.current_humidity  = 0;
	relayboard_state.encoder_parameter = 0;
	relayboard_state.encoder_activated = 0;
	relayboard_state.washing_mode	= 0;
	relayboard_state.washing_phase	= 0;
	relayboard_state.door_status	= 1;
	combioven_state.target_temperature = 30;
	combioven_state.target_steam    = 0;
	combioven_state.target_time     = 0;
	combioven_state.target_fanspeed = 25;
	combioven_state.target_probe    = 0;
	combioven_state.current_probe   = 0;
	combioven_state.current_humidity= 0;
	combioven_state.toggle_preheat  = 0;
	combioven_state.toggle_cooling  = 0;
	combioven_state.toggle_state    = 0;
	combioven_state.toggle_probe    = 0;
	combioven_state.toggle_looptime = 0;
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

	while(1) {
		sleep_ms(SNOOZE_TIME);
		seconds = difftime(time(NULL),timer);

	    lock_mutex();
		if ( (seconds > 0.5) && (runningState == 1) && (combioven_state.toggle_looptime != 1) && (relayboard_state.door_status==1)) {
			if (combioven_state.target_time > 0){
				combioven_state.target_time -= 1;
				printf("seconds: %d\n",(uint32_t)combioven_state.target_time);
			}
			else {
				combioven_state.toggle_state = 5;
				runningState = combioven_state.toggle_state;
				sprintf(buffer_Tx,FINISHED_PROCESS);
			    UART_Print(buffer_Tx);
				printf("%s",buffer_Tx);
			}
			timer = time(NULL);
			dataChanged = 1;
		}

		else if( (seconds > 0.5) && (runningState == 3) && (relayboard_state.door_status==1))
		{			
			if (combioven_state.target_time > 0){				
				combioven_state.target_time -= 1;
				printf("seconds: %d\n",(uint32_t)combioven_state.target_time);
				Washing_Process(relayboard_state.washing_mode, relayboard_state.washing_phase, combioven_state.target_time);
			}
			else {
				combioven_state.toggle_state = 5;
				runningState = combioven_state.toggle_state;
				relayboard_state.washing_mode=0;
				sprintf(buffer_Tx,FINISHED_PROCESS);
			    UART_Print(buffer_Tx);
				printf("%s",buffer_Tx);
				sleep_ms(10);
			}
			timer = time(NULL);
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


/* working fine with COM feedback


*/