#include <iostream>
#include <time.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>     /* atoi */


#include "Motor.h"

#define MOTOR_READ_DEBUG

#define PWM_IOCTL_SET_FREQ         (1) 
									
#define REG_BASE				0x48000000
#define GPTIMER10_OFFSET		0x86000
#define TIMER_LOAD_REG			0x02c

#define FORWARD_FAST	"10000"
#define FORWARD_SLOW	"14000"
#define FORWARD_MID		"12000"
#define BACKWARD_MID	"19000"
#define BACKWARD_FAST	"20000"
#define BACKWARD_SLOW	"18000"
#define SPEED_STOP		"15500"

#define DIR_FORWARD		1
#define DIR_BACKWARD	0

#define MOTOR_DEBUG


Motor::Motor(){
	subsys_name = MOTOR;
	subsys_num = SUBSYS_MOTOR;
	if(sem_init(&motor_cmd_ctrl, 0, 0) != 0){
		perror("Failed to init the motor subsys command / mech control sync sem \n");
	}
}

Motor::~Motor() {
	set_new_pwm_duty_cycle(SPEED_STOP);
	close(motor_fd);
}
		
void Motor::init_device(){
	
	sprintf(motor_filepath,"/dev/pwm9");
	if ((motor_fd = open(motor_filepath,O_RDWR)) < 0) {
		perror("Failed to open the bus for motor.\n");
	}

	direction = DIR_FORWARD;
	set_new_pwm_duty_cycle(SPEED_STOP);
}

void Motor::mech_command(char *value){
	#ifdef MOTOR_DEBUG
		std::cout << "mech command. Duty cycle: " << value[0] << value[1] << std::endl;
	#endif
	write(motor_fd, value, 5);
}

void Motor::mech_control(){
	while(1){
		sem_wait(&motor_cmd_ctrl);
		#ifdef MOTOR_DEBUG
			motor_duty_cycle[5] = '\0';
			std::cout << "issuing motor command. Duty cycle: " << motor_duty_cycle << std::endl;
		#endif
		if(enabled){
			mech_command(motor_duty_cycle);
		}
	}
}
void* Motor::read_data(int command) {
	int data;
	char chardat[6];
	void* ret = 0;
	#ifdef MOTOR_READ_DEBUG
		char* read_test;
	#endif
	switch(command){
		case MOT_FAST:
		case MOT_SLOW:
		case MOT_STOP:
		case MOT_MID:
		case MOT_DISABLE:
		case MOT_ENABLE:
			return NULL;
			break;
		case MOT_SET_SPEED:
			std::cin >> chardat;
			chardat[5] = '\0';
			#ifdef MOTOR_READ_DEBUG
				std::cout << "data set to: " << chardat << std::endl;
			#endif
			memcpy(&data, chardat, 2);
			#ifdef MOTOR_READ_DEBUG
				read_test = ((char*)&data);
				std::cout << "memory copied to void* ret: "<< read_test[0] << read_test[1] << std::endl;
			#endif
			return *((void**)(&data)); 
			break;
		case MOT_DIRECTION:
		case MOT_SET_MIN_PRIO:
			std::cin >> data;
			return *((void**)(&data)); 
			break;
		default:
			std::cout << "Unknown command passed to motor subsystem for reading data! Command was : " << command << std::endl;
			return NULL;
			break;
	}
}

void Motor::set_new_pwm_duty_cycle(const char* value){
	memcpy(motor_duty_cycle,value,5);
	#ifdef MOTOR_DEBUG
		motor_duty_cycle[5] = '\0';
		std::cout << "motor duty cycle set to: " << motor_duty_cycle << std::endl;
	#endif
	sem_post(&motor_cmd_ctrl);
}

void Motor::handle_message(MESSAGE* message){
	#ifdef MOTOR_DEBUG
		char* data_test;
	#endif
	
	int speed;
	int new_speed;
	
	switch(message->command) {
		case MOT_FAST:
			#ifdef MOTOR_DEBUG
				std::cout << "Setting motor fast!" << std::endl;
			#endif
			set_new_pwm_duty_cycle((direction==1) ? FORWARD_FAST : BACKWARD_FAST);
			break;
		case MOT_SLOW:
			#ifdef MOTOR_DEBUG
				std::cout << "Setting motor slow!" << std::endl;
				std::cout << "direction: " << direction << std::endl;
			#endif
			set_new_pwm_duty_cycle((direction==1) ? FORWARD_SLOW : BACKWARD_SLOW);
			break;
		case MOT_STOP:
			set_new_pwm_duty_cycle(SPEED_STOP);
			break;
		case MOT_MID:
			set_new_pwm_duty_cycle((direction==1) ? FORWARD_MID : BACKWARD_MID);
			break;
		case MOT_DIRECTION:
			direction = (*(int*)&message->data);
			/*motor_duty_cycle[5] = '\0';
			speed = atoi(motor_duty_cycle);
			if((direction==1 && speed>15500)){
				new_speed = 30000 - speed;
				sprintf(motor_duty_cycle,"%d",new_speed);
				motor_duty_cycle[5] = '\0';
				std::cout << "new speed: " << motor_duty_cycle << std::endl;
				set_new_pwm_duty_cycle(motor_duty_cycle);
			}else if(direction==0 && speed<15500){*/
				sprintf(motor_duty_cycle, "15500");
				set_new_pwm_duty_cycle(motor_duty_cycle);
			//}
			break;
		case MOT_SET_SPEED:
			#ifdef MOTOR_DEBUG
				std::cout << "Motor set speed: reading message data" << std::endl;
				data_test = ((char*)message->data);
				std::cout << "Setting motor speed manually: " << data_test[0] << data_test[1] << std::endl;
			#endif
			set_new_pwm_duty_cycle((char*)message->data);
			break;
		case MOT_DISABLE:
			enabled = 0;
			break;
		case MOT_ENABLE:
			enabled = 1;
			break;
		case MOT_SET_MIN_PRIO:
			break;
		case MOT_RET_SPEED:
			data_request.to = message->from;
			data_request.command = MOT_RET_SPEED;
			#ifdef MOTOR_DEBUG
				motor_duty_cycle[5] = '\0';
				std::cout << "motor duty cycle is: " << motor_duty_cycle << std::endl;
			#endif
			data_request.data = ((void*)(motor_duty_cycle));
			send_sys_message(&data_request);
			break;
		default:
			break;
	}
}
