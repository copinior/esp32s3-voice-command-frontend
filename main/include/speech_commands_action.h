/* 
 * Public declarations for command feedback actions used by the
 * ESP32-S3 voice command frontend application.
*/
#ifndef _SPEECH_COMMANDS_ACTION_H_
#define _SPEECH_COMMANDS_ACTION_H_

void led_Task(void *arg);

void speech_commands_action(int command_id);

void wake_up_action(void);
#endif
