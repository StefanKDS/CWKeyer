/**
 * @file    trainer.h
 * @brief   Morse trainer (port of the ESP trainer screens), main loop context.
 */
#ifndef TRAINER_H
#define TRAINER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    TRAINER_HEAR = 0,       /**< listen to groups, continue with E or T        */
    TRAINER_GIVE_RANDOM,    /**< letter is shown, user keys it                  */
    TRAINER_GIVE_AZ,        /**< A..Z in order, user keys it                    */
    TRAINER_LISTEN_REPEAT,  /**< letter is played, user keys it back            */
    TRAINER_MODE_COUNT
} trainer_mode_t;

typedef enum
{
    FB_NONE = 0,
    FB_CORRECT,
    FB_WRONG,
} trainer_feedback_t;

typedef struct
{
    trainer_mode_t     mode;
    char               target[12];   /**< expected letter or played group  */
    char               answer;       /**< last keyed character             */
    trainer_feedback_t feedback;
    bool               reveal;       /**< show letter / group              */
    uint16_t           correct;
    uint16_t           total;
} trainer_state_t;

void trainer_start(trainer_mode_t mode);
void trainer_stop(void);
void trainer_next(void);
void trainer_repeat(void);                 /**< play target again            */
void trainer_toggle_reveal(void);
void trainer_on_keyed_char(char c);        /**< keyed character from paddles */
void trainer_poll(void);                   /**< feedback timer, main loop    */
const trainer_state_t *trainer_state(void);
const char *trainer_mode_name(trainer_mode_t m);

#endif
