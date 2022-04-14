#include <stdint.h>


#define UPDATE_STEAM_EVENT "update_steam"
#define UPDATE_STEAM_FMT "1u1 percent"
typedef struct {
	uint8_t 		percent;
} update_steam_event_t;

#define UPDATE_TEMPERATURE_EVENT "update_temperature"
#define UPDATE_TEMPERATURE_FMT "2u1 usertemp"
typedef struct {
	uint16_t 		usertemp;
} update_temperature_event_t;

#define UPDATE_TIME_EVENT "update_time"
#define UPDATE_TIME_FMT "4u1 timer"
typedef struct {
	uint32_t 		timer;
} update_time_event_t;

#define UPDATE_FANSPEED_EVENT "update_fanspeed"
#define UPDATE_FANSPEED_FMT "1u1 percent"
typedef struct {
	uint8_t 		percent;
} update_fanspeed_event_t;

#define UPDATE_TEMP_PROBE_EVENT "update_temprobe"
#define UPDATE_TEMP_PROBE_FMT "1u1 usertemp"
typedef struct {
	uint8_t 		usertemp;
} update_temp_probe_event_t;

#define UPDATE_WASH_CYCLE_EVENT "update_washcycle"
#define UPDATE_WASH_CYCLE_FMT "1u1 usercycle"
typedef struct {
	uint8_t 		usercycle;
} update_wash_cycle_event_t;

/*
MODE_QUICK_POLISH_EVENT  	"mode_washing1"
MODE_FULL_POLISH_EVENT   	"mode_washing2"
MODE_CLEAN_INTER_EVENT  	"mode_washing3"
MODE_CLEAN_QUICK_EVENT   	"mode_washing4"
MODE_CLEAN_ECO_EVENT   		"mode_washing5"
MODE_CLEAN_MID_EVENT   		"mode_washing6"
MODE_CLEAN_FULL_EVENT   	"mode_washing7"
*/

#define ENABLE_ENCODER_EVENT "enable_encoder"
#define ENABLE_ENCODER_FMT "1u1 parameter"
typedef struct {
	uint8_t 		parameter;
} enable_encoder_event_t;

//MAIN DATA RECIPE FOR AUTOMATIC MODE
#define RECIPE_INFO_EVENT "update_recipeinfo"
#define RECIPE_INFO_FMT "1u1 totalsteps 1u1 actualstep 1u1 typestep"
typedef struct {
	uint8_t 		totalsteps;
	uint8_t			actualstep;
	uint8_t			typestep;
} recipe_info_event_t;

//MAIN OVEN PARAMETERS AND STATUS STRUCTURE 
#define COMBIOVEN_UPDATE_EVENT "combioven_update"
#define COMBIOVEN_UPDATE_FMT "4u1 target_time 2u1 target_temperature 2u1 current_probe 2u1 current_humidity 2u1 current_temperature 1u1 target_steam 1u1 target_fanspeed 1u1 target_probe 1u1 toggle_preheat 1u1 toggle_cooling 1u1 toggle_state 1u1 toggle_probe 1u1 toggle_looptime"
typedef struct {
	uint32_t		target_time;
	uint16_t 		target_temperature;
	uint16_t		current_probe;
	uint16_t		current_humidity;
	uint16_t		current_temperature;
	uint8_t 		target_steam;
	uint8_t			target_fanspeed;
	uint8_t			target_probe;		
	uint8_t 		toggle_preheat;
	uint8_t 		toggle_cooling;
	uint8_t			toggle_state;
	uint8_t			toggle_probe;
	uint8_t 		toggle_looptime;
	//uint8_t 		units;
} combioven_update_event_t;
/*
	toggle_state :  
	0 = stop
	1 = running free time / preheat / cooling / looptime
	2 = running manual mode with time Ok
	3 = running automatic by steps Ok
	4 = running washing Ok
	5 = ready for next step
	6 = pause / cerrar puerta
	7 = finished state
	8 = conectar agua
	9 = warning state
*/

//MAIN RELAY BOARD CONTROL AND STATUS STRUCTURE
typedef struct {
	uint16_t 		current_cam_temperature;
	uint16_t		current_probe_temperature;
	uint16_t		current_humidity;
	uint8_t			encoder_parameter;	
	uint8_t			encoder_activated;
	uint8_t 		door_status;
	uint8_t			washing_mode;
	uint8_t			washing_phase;
	uint8_t			completed_step;
	uint8_t			last_step;
	uint8_t			warning_code;
} relayboard_update_event_t;


//EVENTS INCOMING FRONT-END CRANK SOFTWARE
#define MODE_CONVECTION_EVENT   "mode_convection"
#define MODE_COMBINED_EVENT   	"mode_combined"
#define MODE_STEAM_EVENT   		"mode_steam"
#define MODE_LOAD_EVENT   		"mode_load"
#define TOGGLE_MANUAL_EVENT 	"toggle_manual"
#define TOGGLE_AUTOMATIC_EVENT 	"toggle_automatic"
#define TOGGLE_WASHING_EVENT	"toggle_washing"
#define TOGGLE_FINISHED_EVENT	"toggle_finished"
#define TOGGLE_PREHEAT_EVENT 	"toggle_preheat"
#define TOGGLE_COOLING_EVENT 	"toggle_cooling"
#define TOGGLE_LOOPTIME_EVENT   "toggle_looptime"
#define TOGGLE_PROBE_EVENT   	"toggle_probe"
#define TOGGLE_SPRAY_EVENT   	"toggle_spray"


//EVENTS IN/OUT COMING FROM UART RELAYBOARD 8 BYTES
#define MODE_CONVECTION         "#convect"
#define MODE_COMBINED			"#combine"
#define MODE_STEAM				"#steamhi"
#define MODE_PREHEAT		    "#preheat"
#define MODE_COOLING			"#cooling"
#define MODE_WASHING			"#washing"
#define GET_EXTERN_PROBE_TEMP	"#extrnpb"
#define CURRENT_HUMIDITY		"#curh"
#define CURRENT_TEMP_PROBE      "#tprb"
#define	CURRENT_CAM_TEMPERATURE	"#tcam"
#define WARNING_MESSAGE			"#wrng"
#define ENCODER_INCREASE		"#encodr+"
#define ENCODER_DECREASE		"#encodr-"
#define ENCODER_ZERO_POSITION   "#encodr0"
#define ENCODER_ENABLE			"#enabenc"
#define ENCODER_DISABLE			"#disaenc"
#define PAUSE_STOP_PROCESS		"#paustop"
#define RUNNING_PROCESS			"#running"
#define FINISHED_PROCESS		"#finishd"
#define WATER_SPRAY_SHOT		"#waspray"
#define DOOR_OPEN				"#dooropn"
#define DOOR_CLOSED				"#doorcls"

//WASH MANAGEMENT ROUTINES TO RELAYBOARD 8 BYTES
#define	PHASE_DRAIN_WASTE		"#phas001"
#define	PHASE_WASH_OUT			"#phas002"
#define	PHASE_COOLING_CAMERA	"#phas003"
#define	PHASE_DRYING_CAMERA		"#phas004"
#define	PHASE_PREHEAT_BOILER	"#phas005"
#define	PHASE_CLEAN_CARE		"#phas006"
#define	PHASE_CLEAN_JET			"#phas007"
#define	PHASE_FILL_COLD_CAMERA	"#phas008"
#define	PHASE_RECYCLE_WATER		"#phas009"

//GLobal Variables
#define MAX_TARGET_PERCENT 		100
#define MAX_TARGET_TEMPERATURE  300
#define MAX_TARGET_TIME			5999