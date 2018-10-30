#include <conf.h>
#include <kernel.h>
#include <io.h>
#include <bios.h>

#include "maps.h"
#include "kongf.h"

#define TICKS_IN_A_SECOND 18
#define CYCLE_UPDATER 1
#define CYCLE_DRAWER 2
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

#define GAME_OBJECT_LABEL_LENGTH 16

#define MAX_GAME_OBJECTS 64
#define MAX_SAVED_INPUT 4

#define JUMP_DURATION_IN_TICKS 6

extern struct intmap far *sys_imp;

/* Time vars */
// Counting the ticks
int clock_ticks = 0;
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
// Saves the input from the player
int input_queue[MAX_SAVED_INPUT];
// Saves the count of input that was recevied
int input_queue_received = 0;
// The index of the recevied inputs
int input_queue_tail = 0;

/* PIDs vars */
int receiver_pid;
int time_handler_pid;

/* Drawer vars */
// the whole display is represented here: 1 pixel = 1 cell
char display[SCREEN_SIZE + 1];
// a draft of the display, later we copy the draft to the display array
char display_draft[SCREEN_HEIGHT][SCREEN_WIDTH];

/* Game Object vars */
// Used the save all the current game objects
gameObject* game_gameObjects[MAX_GAME_OBJECTS];

/* Player vars */
char mario_model[3][3] = 
{
    " 0 ",
    "-#-",
    "| |"
};
// The game object of the player
gameObject playerObject = {"Player", {40, 20}, 3, 3, mario_model};
// Saves the time that the player jumped (used to know if to apply gravity to the player)
int air_duration_elapsed = 0;

/* Princess vars */
char princess_model[2][2] = {
    "$$",
    "$$"
};
// The game object of the princess
gameObject princessObject = {"Princess", {35 ,2}, 2, 2, princess_model};

/* Kong vars */
char kong_model[3][3] = 
{
    "_K_",
    "<#>",
    "V V"
};
// The game object of kong
gameObject kongObject = {"Kong", {22, 5}, 3, 3, kong_model};

/* Hammer vars */
char hammer_model[1][2] = 
{
    "[-"
};
// The game object of THE HAMMER
gameObject hammerObject = {"Hammer", {2, 2}, 2, 1, hammer_model};

/* Gravity vars */
// Apply gravity every number of ticks
int apply_gravity_every_ticks = 5;
// The counter of passed ticks
int gravity_ticks = 5;

/* Output to screen vars */
// Saves the color byte of the screen before the game
char saved_color_byte;
// Is the user exited the game
int game_exited = 0;

// Wipes the entire screen
void wipe_entire_screen(){
    int i = 0;
    int j = 0;

    for (i = 0; i < SCREEN_HEIGHT; i++){
        for (j = 0; j < SCREEN_WIDTH; j++){
            display[i * SCREEN_WIDTH + j] = ' ';
        }
    }

    game_exited = 1;
    print_to_screen();
}

// Returns the output to white text and black
void reset_output_to_screen(){
    asm{
        MOV AX, 0B800h
        MOV ES, AX

        MOV AH, BYTE PTR saved_color_byte
    }
}

// Saves the color byte of the output to the console
// so we can reset it when closing the game
void save_out_to_screen(){
    asm{
        MOV AX, 0B800h
        MOV ES, AX

        MOV saved_color_byte, AH
    }
}

// Handles the scan codes and ascii codes from the input
int scanCode_handler(int scan, int ascii){
    // if the user pressed CTRL+C
    // We want to terminate xinu (int 27 -> terminate xinu)
    if ((scan == 46) && (ascii == 3)){
        // Resets the output to the screen
        reset_output_to_screen();
        wipe_entire_screen();
        asm INT 27;
    }
    
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

// Keeps track of time
void time_handler(){
    /* Time keeping vars */
    int deltaTime = 0;
    int deltaTime_counter = 0;
    int updater_last_call = 0;
    int deltaSeconds = 0;

    while(TRUE){
        // Waiting for the time routine to wake up this process
        // Basically waiting for a tick to pass
        receive();

        /* Time tracking */
        // delta time is the measurement of the time between calls
        deltaTime = elapsed_time - updater_last_call;
        // delta time counter is the counter of how many ticks passed
        deltaTime_counter += deltaTime;
        // Saving the counter to global use
        clock_ticks = deltaTime_counter;
        // saving the last call of the updater to measure the delta time
        updater_last_call = elapsed_time;

        gravity_ticks -= deltaTime;

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
            // Resetting the global ticks counter
            clock_ticks = 0;
        }
    }
}

// This function is used to better print to the console
// Avoiding flickering, more color options and shit...
void print_to_screen(){
    int i = 0;
    int j = 0;
    char current_pixel;
    // The default color byte that we use to print to the console
    // the default is white text with black background
    // 15 = 00001111
    char default_color_byte = 15;

    // Init the screen to top left point
    asm{
        MOV AX, 0B800h
        MOV ES, AX
        MOV DI, 0
    }

    for (i = 0; i < SCREEN_HEIGHT; i++){
        for (j = 0; j < SCREEN_WIDTH; j++){
            // Getting the char we need to print
            current_pixel = display[i * SCREEN_WIDTH + j];

            // Printing the char to the console
            // and advancing the index to the next cell of the console
            asm{
                MOV AL, BYTE PTR current_pixel
                MOV AH, BYTE PTR default_color_byte

                MOV ES:[DI], AX
                ADD DI, 2
            }
        }
    }
}

// Handles the drawing to the 'screen'
void drawer(){
    while (TRUE){
        receive();
        // if the game was exited we dont want to keep drawing to the screen
        if (game_exited) return;
        print_to_screen();
        //printf(display);
    }
}

// Checks for collisions
// Returns 1 for no collisions
int check_collision_with_map(gameObject* obj, int x_movement, int y_movement){
    // Taking the top left point of the model of the game object
    position top_left = obj->top_left_point;

    // Getting the dimensions of the game object's model
    int model_height = obj->height;
    int model_width = obj->width;

    // Used to know from where to start checking for collision
    // Init with the top left point cords
    int check_pos_x = top_left.x;
    int check_pos_y = top_left.y;

    int i = 0;

    // if we have any movement on the x axis
    if (x_movement != 0){
        // Assuming that the x movement is to the left (negative)
        check_pos_x -= 1;
        // if the x movement is to the right (positive) we want to
        // add the width of the model of the game object plus 1 (because we subtracted it)
        if (x_movement > 0) check_pos_x += model_width + 1;


        // We want to check the pixels to the right/left of the model
        // meaning if the model height is 3, we need to check 3 pixels to the right/left
        // of the model for collision
        for (i = top_left.y; i < top_left.y + model_height; i++){
            // Check for collision with the map elements
            if (map_1[i][check_pos_x] == 'z' || map_1[i][check_pos_x] == 'Z') return 0;
        }
    }

    // if we have any movement on the y axis
    if (y_movement != 0){
        // Assuming that the y movement is up (negative)
        check_pos_y -= 1;
        // if the y movement is down (positive) we want to
        // add the height of the model of the game object plus 1 (because we subtracted it)
        if (y_movement > 0) check_pos_y += model_height + 1;

        // We want to check the pixels up/down of the model
        // meaning if the model width is 2, we need to check 2 pixels above/below
        // of the model for collision
        for (i = top_left.x; i < top_left.x + model_width; i++){
            // Check for collision with the map elements
            if (map_1[check_pos_y][i] == 'z' || map_1[check_pos_y][i] == 'Z') return 0;
        }
    }

    // if we dont have any collisions we can move
    return 1;
}

// Movement with collisions
// Moves the object if there are no collisions
// If we want to move any object we want to use this function
void move_object(gameObject* obj, int x_movement, int y_movement){
    int check_movement_result;

    // Checks for collisions
    check_movement_result = check_collision_with_map(obj, x_movement, y_movement);
    //if (strstr(obj->label, "Player") != NULL)
    //    printf("%s, %d (%d, %d)\n", obj->label, check_movement_result, 
    //    obj->top_left_point.x, obj->top_left_point.y);
    // If the new movement is valid (1 = no collisions)
    if (check_movement_result == 1){
        (obj->top_left_point).x += x_movement;
        (obj->top_left_point).y += y_movement;
    }
}

// Makes the player jump
void player_jump(){
    // Checks if the player is grounded
    if (!check_collision_with_map(&playerObject, 0, 1)){
        move_object(&playerObject, 0, -1);
        //move_object(&playerObject, 0, -1);
        //move_object(&playerObject, -1, 0);
        air_duration_elapsed = elapsed_time;
    }
}

// Apply gravity to all the game objects
void apply_gravity_to_game_objects(){
    int i = 0;

    for (i = 0; i < 2; i++){
        if(strstr(game_gameObjects[i]->label, "Player"))
            if ((elapsed_time - air_duration_elapsed) >= JUMP_DURATION_IN_TICKS)
                move_object(game_gameObjects[i], 0, 1);
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
        ladder_map_ptr = ladders_level_1[0];
    }
	if (level == 2){
        ladder_map_ptr = ladders_level_2[0];
    }
	if (level == 3){
        ladder_map_ptr = ladders_level_3[0];
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

// Used to save models of game objects to the display draft
void insert_model_to_draft(gameObject* gameObj){
    // To iterate the display draft height
    int i = 0;
    // To iterate the display draft width
    int j = 0;
    // To iterate the object model
    int k = 0;

    /* Vars to ease of use */
    // The position of the top left point of the model of the object
    position objectPos = gameObj->top_left_point;

    int top_left_x = objectPos.x;
    int top_left_y = objectPos.y;

    // Getting the model dimensions
    int model_height = gameObj->height;
    int model_width = gameObj->width;

    // Getting the object's model
    char* objectModel = gameObj->model;

    // We need to start saving to the draft from the
    // top left point (kinda the position of the object)
    // and keep drawing from there
    // top_left_y <= i < top_left_y + model height
    // top_left_x <= j < top_left_x + model width
    for (i = top_left_y; i < top_left_y + model_height; i++){
        for (j = top_left_x; j < top_left_x + model_width; j++){
            display_draft[i][j] = objectModel[k];
            k++;
        }
    }
}

// "Reset" the display_draft so we can use it again
void wipe_display_draft(){
    int i = 0;
    int j = 0;

    for (i = 0; i < SCREEN_HEIGHT; i++){
        for (j = 0; j < SCREEN_WIDTH; j++){
            display_draft[i][j] = map_1[i][j];
        }
    }
}

// Handles the scan code of the input from the keybaord
void handle_player_movement(int input_scan_code){
    // Using the input change the position of the player
    //position playerPos* = &(&playerObject)->top_left_point;
    position* playerPos = &(playerObject.top_left_point);

    // if the player is not grounded we dont want it to control mario
    if (check_collision_with_map(&playerObject, 0, 1)) return;

    if ((input_scan_code == ARROW_UP) || (input_scan_code == KEY_W)){
        //move_object(&playerObject, 0, -1);
        player_jump();
    }else if ((input_scan_code == ARROW_RIGHT) || (input_scan_code == KEY_D)){
        move_object(&playerObject, 1, 0);
    }else if ((input_scan_code == ARROW_LEFT) || (input_scan_code == KEY_A)){
        move_object(&playerObject, -1, 0);
    }else if ((input_scan_code == ARROW_DOWN) || (input_scan_code == KEY_S)){
        move_object(&playerObject, 0, 1);
    }

    // Makes sure that the player is in the boundaries of the screen
    //if (playerPos.x > SCREEN_WIDTH) playerPos.x = SCREEN_WIDTH;
    //if (playerPos.x < 0) playerPos.x = 0;
    //if (playerPos.y > SCREEN_HEIGHT) playerPos.y = SCREEN_HEIGHT;
    //if (playerPos.y < 0) playerPos.y = 0;

    //printf("Player position: (%d, %d)\n", playerPos.x, playerPos.y);
}

// Handles the updating of stuff and shit
void updater(){
    int prev_second = 0;

    int i = 0;

    while (TRUE){
        receive();

        for (i = 0; i < input_queue_received; i++){
            handle_player_movement(input_queue[i]);
            //printf("%d: [%d]: (%d, %d)\n", 
            //i, input_queue[i], playerObject.top_left_point.x, playerObject.top_left_point.y);
        }
        input_queue_received = 0;
        input_queue_tail = 0;

        wipe_display_draft();

        insert_ladders_to_map(1);

        if (gravity_ticks <= 0){
            apply_gravity_to_game_objects();
            gravity_ticks = apply_gravity_every_ticks;
        }

        insert_model_to_draft(&princessObject);
        insert_model_to_draft(&playerObject);
        insert_model_to_draft(&kongObject);

        save_display_draft();
        prev_second = clock_seconds;
    }
}

// Handles the input from the user
void receiver(){
    while (TRUE){
        // Getting the input from the routine
        input_queue[input_queue_tail] = receive();
        // Next cell in queue
        input_queue_tail++;
        // Making sure we dont overflow
        if (input_queue_tail >= MAX_SAVED_INPUT) input_queue_tail = 0;
        
        // Saving how many recevied input we got saved
        input_queue_received++;
    }
}

void manager(){
    while (TRUE){

    }
}

xmain(){
    int up_pid, draw_pid, recv_pid, timer_pid, manager_pid;

    // Saves the color byte
    save_out_to_screen();

    resume(timer_pid = create(time_handler, INITSTK, INITPRIO, "KONG: TIME HANDLER", 0));
    resume(draw_pid = create(drawer, INITSTK, INITPRIO + 1, "KONG: DRAWER", 0));
    resume(recv_pid = create(receiver, INITSTK, INITPRIO + 3, "KONG: RECEIVER", 0));
    resume(up_pid = create(updater, INITSTK, INITPRIO, "KONG: UPDATER", 0));
    resume(manager_pid = create(manager, INITSTK, INITPRIO, "KONG: MANAGER", 0));

    // Changes routine #9 to ours
    set_int9();

    // Sets the id of the receiver so we can send msgs to it
    receiver_pid = recv_pid;

    // Sets the id of the time handler so we can send msgs to it
    time_handler_pid = timer_pid;

    // Schedules the drawer and updater
    schedule(3, CYCLE_DRAWER, draw_pid, 0, up_pid, CYCLE_UPDATER, 0, CYCLE_UPDATER, manager_pid);
    // Keeping track of the game objects of the game
    game_gameObjects[0] = &playerObject;

    return;
}
