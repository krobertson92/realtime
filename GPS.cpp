#include "GPS.h"

#define NS_PER_MS	1000000
#define NS_PER_S	1000000000
#define DATA_COL_PERIOD_MS	10
#define DATA_ANL_PERIOD_MS  200


//GPS class

GPS::GPS() {
	subsys_name = GPS_NAME;
	subsys_num = SUBSYS_GPS;
	locBufferIndex=0;
	target=NULL;
	if(sem_init(&collect_analysis_sync, 0, 0) != 0){
		perror("Failed to init the compass collector/analysis sync sem \n");
	}
}

GPS::~GPS(){
	close(serial_port);
}

void GPS::init_sensor() {
	bool error=0;
	struct termios options_original;
	struct termios options;

	serial_port = open(GPS_PORT_NAME, O_RDWR | O_NONBLOCK);

	if (serial_port != -1){
		#ifdef GPS_DEBUG
			printf("Serial Port open\n");
		#endif
		tcgetattr(serial_port,&options_original);
		tcgetattr(serial_port, &options);
		cfsetispeed(&options, B57600);
		cfsetospeed(&options, B57600);
		options.c_cflag |= (CLOCAL | CREAD);
		options.c_lflag |= ICANON;
		if (tcsetattr(serial_port, TCSANOW, &options)!=0){
			printf("error %d from tcsetattr", errno);
			error -1;
			return;
		}
	}else{
		printf("Unable to open %s",GPS_PORT_NAME);
		printf("Error %d opening %s: %s",errno, GPS_PORT_NAME, strerror(errno));
	}

	//clear buffer
	char read_buffer[GPS_MAX_LENGTH + 1] = {0};
	while(read(serial_port,read_buffer, GPS_MAX_LENGTH)>0){
		printf(".");
	}

}

double GPS::getAngle(LatLon startLoc,LatLon eenndLoc){
	LatLon rotation=eenndLoc-startLoc;
	if(rotation.lat==0){
		rotation.lat+=0.00001f;
	}
	if(rotation.lon==0){
		rotation.lon+=0.00001f;
	}
	printf("\tDIFF %f %f\n",rotation.lat,rotation.lon);
	double angle=atan(rotation.lon/rotation.lat);
	if(rotation.lat<0){
		angle*=-1;
	}
	//printf("\tRANG %f\n",angle);
	angle=angle*360/(2*3.14159);///(3.14159);
	//angle*=-1;
	//printf("\tOANG %f\n",angle);
	//printf("What is going on? %f %f %f %f\n",atan(1/1)/(2*3.14159)*360,atan(-1/1)/(2*3.14159)*360,atan(1/-1)/(2*3.14159)*360,atan(-1/-1)/(2*3.14159)*360);
	angle*=-1;
	angle+=180;
	if(angle<0){angle+=360;}
	printf("\tANGL: %f Lat: %f %f Lon: %f %f\n",angle,startLoc.lat,eenndLoc.lat,startLoc.lon,eenndLoc.lon);
	return angle;
}

bool GPS::convert_data(char* input,int length,LatLon& output){
	char command[GPS_GPGAA_L+1];
	char latLonChar[20];
	float latLon[2];
	int latLonCounter=0;
	strncpy(command,input,GPS_GPGAA_L);
	command[GPS_GPGAA_L]='\0';
	int commaCount=0;

	// Check for GPGGA Command
	int gps_OS=-1;
	int i=0;
	if(strcmp(command,GPS_GPGAA)==0){
		gps_OS=GPS_GPGAA_OS;
		i+=GPS_GPGAA_L;
	}
	if(strcmp(command,GPS_GPGLL)==0){
		gps_OS=GPS_GPGLL_OS;
		i+=GPS_GPGLL_L;
	}
	if(strcmp(command,GPS_GPRMC)==0){
		gps_OS=GPS_GPRMC_OS;
		i+=GPS_GPRMC_L;
	}
	//printf("gps_OS: %d\n",gps_OS);
	if(gps_OS!=-1){
		// Now that the command is found, 
		//  we grab all the data between the two commas
		//  then convert it to a float
		for(i=GPS_GPGAA_L;i<length;i++){
			if(input[i]==','){
				commaCount++;
			}
			if(commaCount==gps_OS||commaCount==gps_OS+2){\
				for(int ii=i+1;ii<length;ii++){
					if(input[ii]==','){
						strncpy(latLonChar,input+i+1,ii-(i+1));
						latLon[latLonCounter++]=atof(latLonChar);
						i=ii-1;
						break;
					}
				}
			}
			if(commaCount==gps_OS+2){
				break;
			}
		}

		//Decode GPS data and store in outputs
		if(latLonCounter>=2){
			int   degrees[2];
			float minutes[2];
			float outputs[2];
			for(int i=0;i<2;i++){
				degrees[i]=latLon[i]/100;
				minutes[i]=latLon[i]-degrees[i]*100;
				outputs[i]=degrees[i]+minutes[i]/60;
			}
			output.lat=outputs[0];
			output.lon=outputs[1]*-1;
			//printf("AA %f %f\n",latLon[0],latLon[1]);
		}
		//printf("BB\n");
		return 1;
	}
	return 0;
}

bool GPS::updateWayPoint(){
	if(target==NULL) return false;
	if(target->inRange(getLocBufferAvg()))
		target=target->next;
}
bool GPS::addWayPoint(LatLon latlon,double radius,int index){
	GPSWayPoint* target_loop=target;
	index=target_loop==NULL?-2:index;
	int i;
	for(i=0;index!=-2&&(index==-1||i<index)&&target_loop->next!=NULL;i++){
		target_loop=target_loop->next;
	}
	if(index==-2||index==0){
		target=new GPSWayPoint(latlon,radius,target);
	}else{
		target_loop->next=new GPSWayPoint(latlon,radius,target_loop->next);
	}
}
void GPS::data_grab(LatLon& output){//float& output_lat,float& output_lon){
	LatLon outputB;
	if (serial_port != -1){
		char read_buffer[GPS_MAX_LENGTH + 1] = {0};
		read_buffer[0]='\0';
		int chars_read = read(serial_port,read_buffer, GPS_MAX_LENGTH);
		char* read_bufferA=read_buffer;
		for(int i=0;i<chars_read;i++){
			if(read_buffer[i]=='$'){
				read_bufferA=&read_buffer[i];
				chars_read-=i;
				break;
			}
		}
		if(chars_read>0){
			read_bufferA[chars_read]='\0';
			#ifdef GPS_DEBUG
				//printf(">>B %d %s\n",chars_read,read_bufferA);
			#endif

			//Conversion time
			convert_data(read_bufferA,chars_read,outputB);
			if(outputB.lat!=0){
				printf("\t>>C %f\t%f\n",outputB.lat,outputB.lon);
			}
		}else{
			#ifdef GPS_DEBUG
			printf("No GPS Data\n");
			#endif
		}
	}else{
		std::cout << "Bad GPS Serial" << std::endl;
	}
	output.lat=outputB.lat;
	output.lon=outputB.lon;
	return;
}

void GPS::collector(){
	LatLon gps_reading;
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC ,&t);
	while(1){
		t.tv_nsec+= DATA_COL_PERIOD_MS*NS_PER_MS;
		while(t.tv_nsec > NS_PER_S){
			t.tv_sec++;
			t.tv_nsec -= NS_PER_S;
		}
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);

		if(enabled){
			data_grab(gps_reading);
			//printf("GPS H: %f %f\n", gps_reading.lat, gps_reading.lon);
			if(gps_reading.lat!=0||gps_reading.lon!=0){
				if(1==1){//output_heading){
					printf("> %f\t%f\n", gps_reading.lat, gps_reading.lon);
				}
				setLocBuffer(gps_reading);
				sem_post(&collect_analysis_sync);
			}
		}
	}
}

void GPS::setLocBuffer(const GPS::LatLon location){
	locBuffer[locBufferIndex]=location;
	locBufferIndex=(locBufferIndex+1)%GPS_ROLLBUFF_SIZE;
}

GPS::LatLon GPS::getLocBufferAvg(){
	LatLon output;
	bool divid_cnt=0;
	for(int i=0;i<GPS_ROLLBUFF_SIZE;i++){
		output+=locBuffer[i];
		if(output.lat!=0){
			divid_cnt++;
		}
	}
	if(divid_cnt>0){
		output/=GPS_ROLLBUFF_SIZE;//divid_cnt;
	}
	//Skew Correction
	//output.lat+=0.0001;
	return output;
}

void GPS::analysis(){
	printf("Analysis\n");
	//wait for data
	sem_wait(&collect_analysis_sync);
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC ,&t);
	while(1){
		//printf("Analysis %f %f\n",getLocBufferAvg().lat,getLocBufferAvg().lon);
		t.tv_nsec+= DATA_ANL_PERIOD_MS*NS_PER_MS;
		while(t.tv_nsec > NS_PER_S){
			t.tv_sec++;
			t.tv_nsec -= NS_PER_S;
		}
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);
		if(target!=NULL){
			float angle=getAngle(getLocBufferAvg(),target->latLon);
			angle-=12;	// Magnetic north correction
			//printf("Angle: %f\n",angle);
			MESSAGE request_compass_data = MESSAGE(SUBSYS_GPS, SUBSYS_COMPASS, CPS_SET_HEADING,*((void**)(&angle))); //request current compass heading
			send_sys_message(&request_compass_data);
			updateWayPoint();
		}
	}
}

void* GPS::read_data(int command) {
	switch(command){
		case GPS_ADDWAYDATALAT:
			float inLat;
			std::cin >> inLat;
			return *((void**)(&inLat));
		case GPS_ADDWAYDATALON:
			float inLon;
			std::cin >> inLon;
			return *((void**)(&inLon));
		default:
			std::cout << "Unknown command passed to GPS subsystem for reading data! Command was : " << command << std::endl;
			return NULL;
			break;
	}
}

void GPS::handle_message(MESSAGE* message){
	GPSWayPoint* target_loop;
	switch(message->command){
		case GPS_DISABLE:
			enabled = 0;
			break;
		case GPS_ENABLE:
			std::cout << "Enable GPS" << std::endl;
			enabled = 1;
			break;
		case GPS_NO_DISPLAY:
			output_heading = 0;
			break;
		case GPS_DISPLAY:
			std::cout << "Enable Display GPS" << std::endl;
			output_heading = 1;
			break;
		case GPS_ADDWAY:
			addWayPoint(getLocBufferAvg(),0.000000001);
			target_loop=target;
			std::cout << "Added Waypoint: " << getLocBufferAvg() << std::endl;
			while(target_loop!=NULL){
				std::cout << " " << target_loop->latLon << " " << 0.000000001 << std::endl;
				target_loop=target_loop->next;
			}
			break;
		case GPS_ADDWAYDATALAT:
			temp_lat=(*(float*)&message->data);
			break;
		case GPS_ADDWAYDATALON:
			temp_lon=(*(float*)&message->data);
			break;
		case GPS_ADDWAYDATARUN:
			addWayPoint(LatLon(temp_lat,temp_lon),0.0002);
			break;
		default:
			std::cout << "Unknown command passed to compass subsystem! Command was : " << message->command << std::endl;
			break;
	}
}



/**
 *  \brief Opens a USB virtual serial port at ttyUSB0.
 * returns - the port's file descriptor or -1 on error.
 */