#include <iostream>
#include <stdlib.h>
#include "SystemControl.h"
#include "Compass.h"
#include "shirtt.h"

static void* sys_receive_task(void* c) {
	setup_rt_task(5);
	SystemControl* s = (SystemControl*)c;
	s->recieve_sys_messages();
}

SystemControl::SystemControl() {
	 // setup message queue attributes
	attr.mq_maxmsg = MAX_MSG;
	attr.mq_msgsize = MSG_SIZE;
	attr.mq_flags = 0;
	sys_mq = mq_open(MQ_SYSTEM, O_RDWR | O_CREAT, 0664, &attr);
	if(sys_mq == ERROR){
		perror("failed to open sys mq from system");
		exit(-1);
	}
	iret_mq_receiver = pthread_create( &tMQReceiver, NULL, &sys_receive_task, (void *)(this));
}

SystemControl::~SystemControl() {
	for(int i=0;i<NUM_SUBSYSTEMS;i++){
		delete[] subsys[i];
	}
	pthread_cancel(tMQReceiver);
	mq_close(sys_mq);
	mq_unlink(MQ_SYSTEM);
}

void SystemControl::init() {
	subsys[SUBSYS_COMPASS] = new Compass();
	subsys[SUBSYS_COMPASS]->init();
	subsys[SUBSYS_COMPASS]->send_message((char *)std::string("hello").c_str());
}

void SystemControl::recieve_sys_messages() {
	char message[MSG_SIZE];
	unsigned int priority;
	ssize_t size;
	while(1){
		if((size = mq_receive(sys_mq, message, MSG_SIZE, &priority)) == ERROR){
			perror("Recieve Failed!");
		}else{
			//handle_message(message);
			unsigned int subsys_num = (unsigned int)message[MSG_SIZE-1];
			std::cout << "Sys message received from " << subsys[subsys_num]->subsys_name<< " (" << subsys_num << "):" << std::endl;
			std::cout << message << std::endl<<std::endl;
		}
	}
}
