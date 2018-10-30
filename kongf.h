
#define GAME_OBJECT_LABEL_LENGTH 16

/* Structs */
// Used to save the position of elements
typedef struct Position{
    int x;
    int y;
} position;

// Used to store information about game objects in the game
typedef struct GameObject{
    // The label of the game object
    // Can be used to identified the object
    char label[GAME_OBJECT_LABEL_LENGTH] ;

    // The top left point of the game object
    position top_left_point;

    // The width of the game object
    int width;
    // The height of the game object
    int height;

    // The model of the game object
    char** model;
} gameObject;

/* SCREEN FUNCTIONS */

// Prints the display to the console
void print_to_screen();
// Wipes the entire screen
void wipe_entire_screen();
// Resets the color byte to the previous one (before the game)
void reset_output_to_screen();
// Saves the color byte
void save_out_to_screen();
// Wipes the dispaly draft so we can use it again
void wipe_display_draft();
// Saves the display draft to the display
void save_display_draft();

/* 'PHYSICS' FUNCTIONS (MAINLY COLLISION)*/
// Checks if there is a collision
// obj - the game object we want to check if it can move
// x_movement - how many pixels we move on the x axis
// y_movement - how many pixels we move on the y axis
int check_collision_with_map(gameObject* obj, int x_movement, int y_movement);
// Moves the gameobject
// The vars are the same as the check collision one.....
void move_object(gameObject* obj, int x_movement, int y_movement);
// Apply gravity the all the game objects
void apply_gravity_to_game_objects();

/* PLAYER FUNCTIONS */
// Makes the player jump
void player_jump();
// Handle the input from the player
void handle_player_movement(int input_scan_code);

/* DRAWING FUNCTIONS */
// Inserts the ladders from their map to the display draft
void insert_ladders_to_map(int level);
// Inserts a model to the display draft
// gameObj - the game object we want to add to the display draft (draw...)
void insert_model_to_draft(gameObject* gameObj);