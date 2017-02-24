#ifndef  SNAP_CONTROLLER_H_ 
#define SNAP_CONTROLLER_H_

#define ECHO        24
#define TRIG        23
#define BUTTON      18
#define LED         17
#define EXIT_STATE  end
#define ENTRY_STATE entry
#define LOW         0
#define HIGH        1
#define MICROSECDONDS_PER_CM  29.14 // calculated from seed of sound at 20C   1e6/34321
#define DEV_INPUT_EVENT "/dev/input/event0"

enum state_codes { entry, idle, blink, dist, end};
enum ret_codes { ok, fail, bt_1, btn_a, btn_tri, btn_sq, btn_x};
enum ret_codes last_event;

struct transition {
    enum state_codes src_state;
    enum ret_codes   ret_code;
    enum state_codes dst_state;
};

//
// Function Prototypes
//
int get_dist(void); 
int get_bl_event(void); 
int gpio_init(void); 
int gamepad_init(void); 
int entry_state(void);
int idle_state(void);
int blink_state(void);
int exit_state(void);
void sonnarTrigger(void);


#endif // SNAP_CONTROLLER_
