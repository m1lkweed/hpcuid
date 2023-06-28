// This file implements a conforming HPCUID generator in C, as generate_hpcuid().
// The functions start_timer() and _initial_hpcuid_setup() must be called before
// any UIDs are generated.
// (c)m1lkweed 2023, CC0

#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdatomic.h>

#include <ifaddrs.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <netpacket/packet.h>

typedef union{
	unsigned __int128 as_u128;
	struct{
		union{
			uint64_t discriminators;
			struct hpcuid_discriminators{
				uint32_t increment;
				uint32_t timestamp;
			}hpcuid_discriminators;
			struct{
				uint32_t increment;
				uint32_t timestamp;
			};
		};
		union{
			uint64_t globals;
			struct hpcuid_globals{
				uint32_t processid;
				uint32_t machineid:24;
				uint32_t _reserved:8;
			}hpcuid_globals;
			struct{
				uint32_t processid;
				uint32_t machineid:24;
				uint32_t _reserved:8;
			};
		};
	};
}hpcuid_t;

static const time_t hpcuid_epoch_time = 978307200UL; // 2001-01-01T00:00:00Z
struct hpcuid_discriminators local_discriminator = {};
struct hpcuid_globals global_values;

uint32_t get_mac_address(){
	union{
		char as_bytes[3];
		uint64_t as_u32;
	}mac_addr = {};
	struct ifaddrs *ifaddr = NULL;
	if(getifaddrs(&ifaddr) == -1){
		goto done; // assume machine is off-line. Machine id of 00:00:00 is valid
	}else{
		for(struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next){
			if((ifa->ifa_addr) && (ifa->ifa_addr->sa_family == AF_PACKET) && (ifa->ifa_name[0] == 'e')){
			// Newer linuxes use en*, older ones use ethX. No one is doing HPC over WiFi, so we're only considering ethernet MAC addresses
				const struct sockaddr_ll *s = (struct sockaddr_ll*)ifa->ifa_addr;
					if(s->sll_halen == 6){
						memcpy(mac_addr.as_bytes, &(s->sll_addr[3]), 3);
					}
				break;
			}
		}
		freeifaddrs(ifaddr);
	}
	done:
	return mac_addr.as_u32;
}

void discriminator_update(int){
	local_discriminator = (struct hpcuid_discriminators){
		.timestamp = time(NULL) - (hpcuid_epoch_time - 1),
		.increment = 0
	};
}

timer_t start_timer(void){
	timer_t timer;
	struct timespec time_left;
	clock_gettime(CLOCK_REALTIME, &time_left);
	struct itimerspec every_secondish = {{.tv_sec = 1}, {time_left.tv_sec - 1, 0}};
	struct sigaction timer_handler = {.sa_handler = discriminator_update};
	sigaction(SIGALRM, &timer_handler, NULL);
	timer_create(CLOCK_REALTIME, NULL, &timer);
	if(timer_getoverrun(timer) > 0){++every_secondish.it_value.tv_sec;}
	timer_settime(timer, TIMER_ABSTIME, &every_secondish, NULL);
	local_discriminator.timestamp = time(NULL) - (hpcuid_epoch_time - 1);
	return timer;
}

void _initial_hpcuid_setup(void){
	global_values = (struct hpcuid_globals){
		.machineid = get_mac_address(),
		.processid = getpid()
	};
}

extern inline hpcuid_t generate_hpcuid(){
	hpcuid_t ret = {
		.hpcuid_globals = global_values,
		.hpcuid_discriminators = local_discriminator
	};
	++local_discriminator.increment;
	return ret;
}

int main(){
	start_timer();
	_initial_hpcuid_setup();
	while(1){
		hpcuid_t foo = generate_hpcuid();
		char buf[32] = "";
		struct tm tmp = {};
		time_t timestamp = foo.timestamp + hpcuid_epoch_time;
		gmtime_r(&timestamp, &tmp);
		strftime(buf, 31, "%FT%TZ", &tmp);
		printf("HPCUID: %.16lx%.16lx\r\v"
			"MAC NIC: %.2d:%.2d:%.2d PID: %d Time: %s Count: %u\n",
			foo.globals, foo.discriminators,
			(foo.machineid >> 16) & 0xFF, (foo.machineid >> 8) & 0xFF,
			(foo.machineid) & 0xFF, foo.processid, buf, foo.increment
		);
		fflush(stdout);
	}
}
