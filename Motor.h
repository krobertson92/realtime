#ifndef _motor_h_
#define _motor_h_

#include <stdint.h>

#include "Actuator.h"

#define MOTOR	"MOTOR"
#define SUBSYS_MOTOR	9
/**
 * \class Motor
 * \brief motor control
 * 
 * receives motor control commands and commands the motor accordingly
 */
class Motor : public Actuator {
	public:
	
		/**
		 * \brief Actuator default constructor
		 * 
		 * sets subsystem parameters
		 */
		Motor();
		
		~Motor();
		
		void init_device();
		
		/**
		 * \brief controls the mechanical system
		 * 
		 * 
		 */
		void mech_control();
		
		void mech_command(char *value);
		
		
	protected:
	
		/**
		 * \brief handles messages sent to the compass subsystem
		 */
		void handle_message(char* message);
		
		int motor_fd;
		char motor_filepath[40];
		volatile uint32_t* gptimer_reg;
};

#endif
