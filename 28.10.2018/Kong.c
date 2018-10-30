#include <conf.h>
#include <kernel.h>
#include <io.h>
#include <bios.h>

#include "maps.h"

#define TWO_KILO 2048

#define TICKS_IN_A_SECOND 18
#define CYCLE_UPDATER 1
#define CYCLE_DRAWER 6
#define SCHED_ARR_LENGTH 5

#define ARROW_UP 72
#define ARROW_DOWN 80
#define ARROW_RIGHT 77
#define ARROW_LEFT 75

#define KEY_W 17
#define KEY_A 30
#define KEY_S 31
#define KEY_D 32
#define KEY_SPACE 57
#define KEY_ENTER 28
#define KEY_ESC 1

extern struct intmap far *sys_imp;

/* Structs */
// Used to save the position of elements
typedef struct Position{
    int x;
    int y;
} position;

/* Time vars */
// Counting the seconds
int clock_seconds = 0;
// Counting the minutes
int clock_minutes = 0;
// External counting of ticks that has passed (in the file clkint.c)
extern int elapsed_time;

/* Schedule vars */
int sched_arr_pid[SCHED_ARR_LENGTH] = { -1 };
int sched_arr_int[SCHED_ARR_LENGTH] = { -1 };
int point_in_cycle;
int gcycle_length;
int gno_of_pids;

/* Input Queue vars */
// Stores the input (as chars)
int ch_input_array[TWO_KILO];
// the head of the queue
int input_array_head = -1;
// the tail of the queue
int input_array_tail = -1;

/* PIDs vars */
int receiver_pid;

/* Drawer vars */
// the whole display is represented here: 1 pixel = 1 cell
char display[SCREEN_SIZE + 1];
// a draft of the display, later we copy the draft to the display array
char display_draft[SCREEN_HEIGHT][SCREEN_WIDTH];

/* Player vars */
// The position of the player, init to (40, 12) just for testing
position playerPos = {40, 21};
char mario_model[3][3] = 
{
    " 0 ",
    "-#-",
    "| |"
};

/* Princess vars */
position princessPos = {36, 3};
char princess_model[2][2] = {
    "$$",
    "$$"
};

/* Kong vars */
position kongPos = {24, 6};
char kong_model[3][3] = 
{
    "_K_",
    "<#>",
    "V V"
};

// Handles the scan codes and ascii codes from the input
int scanCode_handler(int scan, int ascii){
    // if the user pressed CTRL+C
    // We want to terminate xinu (int 27 -> terminate xinu)
    if ((scan == 46) && (ascii == 3))
        asm INT 27;
    
    return scan;
}

// Routine #9
INTPROC _int9(int mdevno){
    int result = 0;
    int scan_code = 0;
    int ascii_code = 0;

    // Gets the input from the keyboard
    asm{
        MOV AH, 1
        INT 16H
        JZ SKIP_INPUT
        MOV AH, 0
        INT 16H
        MOV BYTE PTR scan_code, AH
        MOV BYTE PTR ascii_code, AL
    }

    result = scanCode_handler(scan_code, ascii_code);

    // Sends a msg (the msg is the result) to the receiver process
    send(receiver_pid, result);

    SKIP_INPUT:
}

// Sets our new routine instead of the old one
void set_int9(){
    int i;
    for(i = 0; i < 32; i++){
        if (sys_imp[i].ivec == 9){
            sys_imp[i].newisr = _int9;
            return;
        }
    }
}

SYSCALL schedule(int no_of_pids, int cycle_length, int pid1, ...){
    int i;
    int ps;
    int *iptr;

    disable(ps);

    gcycle_length = cycle_length;
    point_in_cycle = 0;
    gno_of_pids = no_of_pids;
    
    iptr = &pid1;
    for (i = 0; i < no_of_pids; i++){
        sched_arr_pid[i] = *iptr;
        iptr++;
        sched_arr_int[i] = *iptr;
        iptr++;
    }

    restore(ps);
}

// Handles the drawing to the 'screen'
void drawer(){
    while (TRUE){
        receive();
        printf(display);
    }
}

// Copies the display_draft to the display so it can be draw
void save_display_draft(){
    int i = 0;
    int j = 0;
    
    for (i = 0; i < SCREEN_HEIGHT; i++){
        for (j = 0; j < SCREEN_WIDTH; j++){
            display[SCREEN_WIDTH * i + j] = display_draft[i][j];
        }
    }
    // Setting the null char (the last char of the array)
    display[SCREEN_SIZE] = '\0';
}

// Inserts the ladders to the map
// level - the level num
void insert_ladders_to_map(int level){
    int i = 0;
    int j = 0;

    // Using a pointer to know what (level) ladders to draw
    char* ladder_map_ptr = NULL;
    if (level == 1){
        ladder_map_ptr = &ladders_level_1[0];
    }

    // Copying the ladders to the draft
    for (i = 0; i < SCREEN_HEIGHT; i++){
        for (j = 0; j < SCREEN_WIDTH; j++){
            // Getting the current cell in the matrix of ladders
            char currentPixel = ladder_map_ptr[SCREEN_WIDTH * i + j];
            // if it's a ladder we want to add it to the draft
            if (currentPixel == '|' || currentPixel == '_'){
                display_draft[i][j] = currentPixel;
            }
        }
    }
}

// Inserts the mario model to the dispaly draft
void insert_mario_model_to_draft(){
    // Head level
    display_draft[playerPos.y - 1][playerPos.x - 1] = mario_model[0][0];
    display_draft[playerPos.y - 1][playerPos.x] = mario_model[0][1];
    display_draft[playerPos.y - 1][playerPos.x + 1] = mario_model[0][2];
    
    // Body level
    display_draft[playerPos.y][playerPos.x - 1] = mario_model[1][0];
    display_draft[playerPos.y][playerPos.x] = mario_model[1][1];
    display_draft[playerPos.y][playerPos.x + 1] = mario_model[1][2];

    // Feet level
    display_draft[playerPos.y + 1][playerPos.x - 1] = mario_model[2][0];
    display_draft[playerPos.y + 1][playerPos.x] = mario_model[2][1];
    display_draft[playerPos.y + 1][playerPos.x + 1] = mario_model[2][2];
}

// Inserts the pricess model to the dispaly draft
void insert_princess_model_to_draft(){
    // Head level
    display_draft[princessPos.y - 1][princessPos.x - 1] = princess_model[0][0];
    display_draft[princessPos.y - 1][princessPos.x] = princess_model[0][1];

    // Feet+Body level
    display_draft[princessPos.y][princessPos.x - 1] = princess_model[1][0];
    display_draft[princessPos.y][princessPos.x] = princess_model[1][1];
}

// Inserts the kong model to the dispaly draft
void insert_kong_to_display_draft(){
        // Head level
    display_draft[kongPos.y - 1][kongPos.x - 1] = kong_model[0][0];
    display_draft[kongPos.y - 1][kongPos.x] = kong_model[0][1];
    display_draft[kongPos.y - 1][kongPos.x + 1] = kong_model[0][2];
    
    // Body level
    display_draft[kongPos.y][kongPos.x - 1] = kong_model[1][0];
    display_draft[kongPos.y][kongPos.x] = kong_model[1][1];
    display_draft[kongPos.y][kongPos.x + 1] = kong_model[1][2];

    // Feet level
    display_draft[kongPos.y + 1][kongPos.x - 1] = kong_model[2][0];
    display_draft[kongPos.y + 1][kongPos.x] = kong_model[2][1];
    display_draft[kongPos.y + 1][kongPos.x + 1] = kong_model[2][2];
}

// "Reset" the display_draft so we can use it again
void wipe_display_draft(){
    int i = 0;
    int j = 0;

    for (i = 0; i < SCREEN_HEIGHT; i++){
        for (j = 0; j < SCREEN_WIDTH; j++){
            display_draft[i][j] = map_1[i][j];
            //display_draft[i][j] = ' ';
        }
    }
}

// Handles the updating of stuff and shit
void updater(){
    /* Time keeping vars */
    int deltaTime = 0;
    int deltaTime_counter = 0;
    int updater_last_call = 0;
    int deltaSeconds = 0;

    while (TRUE){
        receive();

        /* Time tracking */
        // delta time is the measurement of the time between calls
        deltaTime = elapsed_time - updater_last_call;
        // delta time counter is the counter of how many ticks passed
        deltaTime_counter += deltaTime;
        // saving the last call of the updater to measure the delta time
        updater_last_call = elapsed_time;

        wipe_display_draft();

        // Checks if a second has passed
        if (deltaTime_counter >= TICKS_IN_A_SECOND){
            clock_seconds++;
            // At least a minute has passed
            if (clock_seconds >= 60){
                // Gets how many seconds we are passed the 60 seconds mark
                deltaSeconds = clock_seconds - 60;
                // Resets the seconds clock
                clock_seconds = 0;
                // Adds to it the delta seconds so that we dont lose any seconds
                clock_seconds += deltaSeconds;
                // A minute has passed!
                clock_minutes++;
            }
            //printf("%d:%d\n", clock_minutes, clock_seconds);
            // We want every second to reset the counter
            deltaTime_counter = 0;
        }

        //display_draft[playerPos.y][playerPos.x] = '*';
        insert_ladders_to_map(1);
        insert_princess_model_to_draft();
        insert_mario_model_to_draft();
        insert_kong_to_display_draft();

        save_display_draft();
    }
}

// Handles the scan code of the input from the keybaord
void handle_player_movement(int input_scan_code){
    // Using the input change the position of the player
    if ((input_scan_code == ARROW_UP) || (input_scan_code == KEY_W)){
        playerPos.y--;
    }else if ((input_scan_code == ARROW_RIGHT) || (input_scan_code == KEY_D)){
        playerPos.x++;
    }else if ((input_scan_code == ARROW_LEFT) || (input_scan_code == KEY_A)){
        playerPos.x--;
    }else if ((input_scan_code == ARROW_DOWN) || (input_scan_code == KEY_S)){
        playerPos.y++;
    }

    // Makes sure that the player is in the boundaries of the screen
    if (playerPos.x > SCREEN_WIDTH) playerPos.x = SCREEN_WIDTH;
    if (playerPos.x < 0) playerPos.x = 0;
    if (playerPos.y > SCREEN_HEIGHT) playerPos.y = SCREEN_HEIGHT;
    if (playerPos.y < 0) playerPos.y = 0;

    //printf("Player position: (%d, %d)\n", playerPos.x, playerPos.y);
}

// Handles the input from the user
void receiver(){
    while (TRUE){
        // Waits for the input from routine #9 to be sent
        ch_input_array[++input_array_tail] = receive();
        //printf("Received");
        // if the head of the queue is -1 we dont want it to be -1 so yeah.... 0
        if (input_array_head == -1) input_array_head = 0;

        // Checks the scan code for input
        handle_player_movement(ch_input_array[input_array_tail]);
    }
}

xmain(){
    int up_pid, draw_pid, recv_pid;

    resume(draw_pid = create(drawer, INITSTK, INITPRIO + 1, "DRAWER", 0));
    resume(recv_pid = create(receiver, INITSTK, INITPRIO + 3, "RECEIVER", 0));
    resume(up_pid = create(updater, INITSTK, INITPRIO, "UPDATER", 0));

    // Changes routine #9 to ours
    set_int9();

    // Sets the id of the receiver so we can send msgs to it
    receiver_pid = recv_pid;

    // Schedules the drawer and updater
    schedule(2, CYCLE_DRAWER, draw_pid, 0, up_pid, CYCLE_UPDATER);
}
