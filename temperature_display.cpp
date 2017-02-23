/* Convert RF signal into bits (temperature sensor version) 
 * Written by : Ray Wang (Rayshobby LLC)
 * http://rayshobby.net/?p=8827
 * Update: adapted to RPi using WiringPi 
 */

// ring buffer size has to be large enough to fit
// data between two successive sync signals


#include <wiringPi.h>
#include <stdlib.h>
#include <stdio.h>

#define RING_BUFFER_SIZE 256
#define SYNC_LENGTH 9000
#define SEP_LENGTH 500
#define BIT1_LENGTH 4000
#define BIT0_LENGTH 2000

#define DATA_PIN 12

unsigned long timings[RING_BUFFER_SIZE];
unsigned int syncIndex1 = 0;
unsigned int syncIndex2 = 0;
bool received = false;

bool isSync(unsigned int idx){
	unsigned long t0 = timings[(idx+RING_BUFFER_SIZE-1)%RING_BUFFER_SIZE];
	unsigned long t1 = timings[idx];
	if(t0>(SEP_LENGTH-100) && t0 < (SEP_LENGTH+100)
		&& t1 > (SYNC_LENGTH-1000) && t1 < (SYNC_LENGTH+1000)
		&& digitalRead(DATA_PIN) == HIGH){
		return true;
	}
	return false;
}
void handler(){
	static unsigned long duration = 0;
	static unsigned long lastTime = 0;
	static unsigned int ringIndex = 0;
	static unsigned int syncCount = 0;

	if(received == true){
		return;
	}

	long time = micros();
	duration = time - lastTime;
	lastTime = time;

	ringIndex = (ringIndex+1) % RING_BUFFER_SIZE;
	timings[ringIndex] = duration;

	if(isSync(ringIndex)){
		syncCount++;
		if(syncCount == 1){
			syncIndex1 = (ringIndex+1) % RING_BUFFER_SIZE;
		}
		else if(syncCount == 2){
			syncCount = 0;
			syncIndex2 = (ringIndex+1) % RING_BUFFER_SIZE;
			unsigned int changeCount = (syncIndex2 < syncIndex1) ? (syncIndex2 + RING_BUFFER_SIZE - syncIndex1) : (syncIndex2 - syncIndex1);
			if(changeCount != 66){
				received = false;
				syncIndex1 = 0;
				syncIndex2 = 0;
			}
			else {
				received = true;
			}
		}
	}
}

int main(int argc, char *argv[]){
	printf("Started:\n");
	if(wiringPiSetupGpio() == -1){
		printf("no wiring pi detected\n");
		return 0;
	}
	wiringPiISR(DATA_PIN,INT_EDGE_BOTH,&handler);
	while(true){
		if(received == true){
			system("/usr/local/bin/gpio edge 2 none");
//			wiringPiISR(-1,INT_EDGE_BOTH,&handler);
			for(unsigned int i = syncIndex1; i != syncIndex2; i = (i+2)%RING_BUFFER_SIZE){
				unsigned long t0 = timings[i],t1 = timings[(i+1)%RING_BUFFER_SIZE];
				if(t0>(SEP_LENGTH-200) && t0<(SEP_LENGTH+200)){
					if(t1>(BIT1_LENGTH-1000) && t1<(BIT1_LENGTH+1000)){
						printf("1");
					} else if(t1>(BIT0_LENGTH-1000) && t1 < (BIT0_LENGTH+1000)){
						printf("0");
					} else {
						printf("SYNC");
					}
				} else {
					printf("?%d?",t0);
				}
			}
			printf("\n");
			unsigned long temp = 0;
			bool negative = false;
			bool fail = false;
			for(unsigned int i =(syncIndex1+24)%RING_BUFFER_SIZE;
				i!=(syncIndex1+48)%RING_BUFFER_SIZE; i=(i+2)%RING_BUFFER_SIZE){
				unsigned long t0 = timings[i], t1 = timings[(i+1)%RING_BUFFER_SIZE];
				if(t0>(SEP_LENGTH-200) && t0<(SEP_LENGTH+200)){
					if(t1>(BIT1_LENGTH-1000) && t1<(BIT1_LENGTH+1000)){
						if( i== (syncIndex1+24)%RING_BUFFER_SIZE){
							negative = true;
						}
						temp = (temp << 1) + 1;
					} else if(t1>(BIT0_LENGTH-1000) && t1<(BIT0_LENGTH+1000)){
						temp = (temp << 1) + 0;
					} else {
						printf("not one or zero: %d\n",t1);
						fail = true;
					}
				} else {
					printf("wrong seporation length: %d\n",t0);
					fail = true;
				}
			}

			if(!fail){
				if(negative){
					temp = 4096 - temp;
					printf("-");
				}
				printf("%d C  %d F\n",(temp+5)/10,(temp*9/5+325)/10);
			} else {
				printf("Decoding Error.\n");
			}
			delay(1000);
			wiringPiISR(DATA_PIN,INT_EDGE_BOTH,&handler);
			received = false;
			syncIndex1 = 0;
			syncIndex2 = 0;
		}
	}
	exit(0);
}
