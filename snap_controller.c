#include <stdlib.h>
#include <stdio.h>
#include <pigpio.h>
#include <unistd.h>
//#include <sys/types.h>
#include <sys/wait.h>
#include <linux/input.h>
#include <errno.h>
#include <fcntl.h>
#include "snap_controller.h"

/* Global Variables */
int button_x 		= LOW;
int button_a 		= LOW;
int button_sq 	= LOW;
int button_tri 	= LOW;
int button1		= LOW;
int in_sm   		= 1;
int out_sm  		= 1;
int out_dis 		= 0;
int start_tick 	= 0;
int end_tick 		= 0;
int start_echo 	= 0;
int end_echo 	= 0;
int fd;

//
// CallBacks
//

static void _cb_button1(int gpio, int level, uint32_t tick)
{
    if(level == 1) {
       end_tick = tick;
       printf("Button pressed for %d\n",((end_tick>>0)-(start_tick>>0)));
    }
    
    if(level == 0) {
       start_tick = tick;
       printf("tick %d\n",(tick>>0));
       button1 = 1;
    }
}

static void _cb_echo(int gpio, int level, uint32_t tick)
{
    if(level == 0) {
       end_echo = tick;
       printf("Distance = %d\n",(((end_echo>>0)-(start_echo>>0))/58));
    }
    
    if(level == 1) {
       start_echo = tick;
       //printf("echo start %d\n",(tick>>0));
    }
}

//static void bt_cb (EV_P_ ev_io *w, int revents)
void _cb_bt (int _sig)
{
     //puts ("bt ready");
     get_bl_event();
 }

//
// State Machine Infrastructure
//

/* array and enum below must be in sync! */
int (* state[])(void) = { entry_state, idle_state, blink_state, get_dist, exit_state};

/* transitions from end state aren't needed */
struct transition state_transitions[] = {
    {entry, 	ok,			idle},
    {idle,  	btn_x,		dist},
    {idle, 		btn_a, 	blink},
    {idle, 		fail,     	end},
    {dist,    	ok,     		idle},
    {dist,    	btn_x,     dist},
    {dist,    	btn_a,     blink},
    {blink, 	ok,     		idle},
    {blink, 	fail,   		end},
    {blink, 	btn_x,		dist},
    {blink, 	btn_a,		blink}};

static enum state_codes lookup_transitions(enum state_codes current, enum ret_codes ret)
{
	int i = 0;
	enum state_codes temp = end;
	for (i = 0;; ++i) {
	  if (state_transitions[i].src_state == current && state_transitions[i].ret_code == ret) {
		temp = state_transitions[i].dst_state;
		break;
	  }
	}
	return temp;
}

//
// Main
//

int main()
{ 
	// State Machine Variables
	enum state_codes cur_state = ENTRY_STATE;
	enum ret_codes rc;
	int (* state_fun)(void);

	for (;;) {
        state_fun = state[cur_state];
        rc = state_fun();
        if (EXIT_STATE == cur_state)
            break;
        cur_state = lookup_transitions(cur_state, rc);
	}

	return 0;
} 

//
// Init Functions
//

int gpio_init () {
	// Initialize GPIOs
	if (gpioInitialise() < 0)
	{
      fprintf(stderr, "pigpio initialisation failed\n");
      return EXIT_FAILURE;
	}

	printf("Setup GPIOs\n");
 
	/* Set GPIO modes */
	gpioSetMode(LED,    PI_OUTPUT);
	gpioSetMode(BUTTON, PI_INPUT);
	gpioSetMode(TRIG,   PI_OUTPUT);
	gpioSetMode(ECHO,   PI_INPUT);
	gpioSetPullUpDown(BUTTON, PI_PUD_UP); // Sets a pull-down

	gpioSetAlertFunc(BUTTON, _cb_button1);
	gpioSetAlertFunc(ECHO,     _cb_echo);
	gpioWrite(TRIG, LOW); /* start in off state*/ 
	
	return EXIT_SUCCESS;
}

int gamepad_init () {
	// Gamepad Variables
	int grabbed;
	char *filename;

	printf("Connect to Gamepad\n");
   
	/* Setup Gamepad */
	filename = DEV_INPUT_EVENT;

	if (!filename) {
		fprintf(stderr, "Device not found\n");
		return EXIT_FAILURE;
	}

	if ((fd = open(filename, O_RDONLY | O_NOCTTY | O_NDELAY)) < 0) {
		perror("");
		if (errno == EACCES && getuid() != 0) {
			fprintf(stderr, "You do not have access to %s. Try "
				"running as root instead.\n", filename);
		}
		return EXIT_FAILURE;
    }
	signal(SIGIO, _cb_bt);
	fcntl(fd, F_SETOWN, getpid());
	if (fcntl(fd, F_SETFL, O_ASYNC | O_NONBLOCK) < 0) { perror("fcntl"); return 1; }
    //free(filename);
 
	grabbed = ioctl(fd, EVIOCGRAB, (void *) 1);
	ioctl(fd, EVIOCGRAB, (void *) 0);
	if (grabbed) {
		printf("This device is grabbed by another process. Try switching VT.\n");
		return EXIT_FAILURE;
	}

	int version;
	unsigned short id[4];

	if (ioctl(fd, EVIOCGVERSION, &version)) {
		perror("Can't get bluetooth controller version");
		return EXIT_FAILURE;
	}
	printf("Input driver version is %d.%d.%d\n", 
       version >> 16, (version >> 8) & 0xff, version & 0xff);

	ioctl(fd, EVIOCGID, id);
	printf("Input device ID: bus 0x%x vendor 0x%x product 0x%x version 0x%x\n",
	id[ID_BUS], id[ID_VENDOR], id[ID_PRODUCT], id[ID_VERSION]);
	
	return EXIT_SUCCESS;
}

//
// State Functions
//

int blink_state() 
{
	double start;

	printf("Blink \n");

	last_event = ok;

	start = time_time();
 
	while ((time_time() - start) < 5.0)
	{
		gpioWrite(LED, 1); /* on */
		time_sleep(0.5);
		gpioWrite(LED, 0); /* off */
		time_sleep(0.5);

		//gpioSetTimerFunc(0, 100, (gpioTimerFunc_t)gpioTrigger(LED,100,LOW));
	}

	//gpioSetTimerFunc(0, 1000, NULL);
	gpioWrite(LED, 0); /* off */

	return last_event;
}

void sonarTrigger(void)
{
   /* trigger a sonar reading */

   gpioWrite(TRIG, PI_ON);

   gpioDelay(10); /* 10us trigger pulse */

   gpioWrite(TRIG, PI_OFF);
}

int get_dist () 
{
	printf("Acquiring distance \n");
 
	last_event = ok;
 
   //Send trig pulse
   gpioTrigger(TRIG,20,HIGH);
   sleep(1);
 
   return last_event;
}

int get_bl_event() {
	struct input_event ev;
	unsigned int size;
	int i;
	//printf("Read BL event\n");
	
	for(i=0; i<4 ; i++) {

		size = read(fd, &ev, sizeof(struct input_event));

		if (size < sizeof(struct input_event)) {
			printf("expected %u bytes, got %u\n", sizeof(struct input_event), size);
			perror("\nerror reading");
			return EXIT_FAILURE;
		}
	
		if(ev.type == 1) {
			if(ev.code == 114) { 
				button_x = ev.value; 
				if(ev.value == 1) {
					last_event = btn_x;
				}
			}
			if(ev.code == 113) { 
				button_a = ev.value; 
				if(ev.value == 1) {
					last_event = btn_a;
				}
			}
			//printf("Event: time %ld.%06ld, ", ev.time.tv_sec, ev.time.tv_usec);
			//printf("type: %i, code: %i, value: %i\n", ev.type, ev.code, ev.value);
		} 
	}
	
	return ok;

}

int entry_state () 
{
	printf("Entry \n");
	 
	if (gpio_init()) {
		printf("GPIO Initialization FAILED\n");
		return EXIT_FAILURE;
	}
 
 	if (gamepad_init()) {
		printf("Gamepad Initialization FAILED\n");
		return EXIT_FAILURE;
	}

	return ok;
}

int idle_state () 
{
	printf("Idle \n");

	last_event = ok;

	while((button1 == LOW) & (last_event == ok))
	{
		sleep(.5);
	}

	if(button1 == HIGH) {
		return fail;
	}

	return last_event;
}

int exit_state () 
{
	printf("Exit \n");

	// close bluetooth file
	close(fd);
	// disable GPIOs
	gpioTerminate();

	return ok;
}
