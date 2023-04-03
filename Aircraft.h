#ifndef __AIRCRAFT_H__
#define __AIRCRAFT_H__

typedef struct Aircraft_s
{
	uint32_t id;
	double position[3];
	double velocity[3];

	uint32_t in_ATC_tracking_range; // 1 = in range; 0 = outside range

	uint32_t reserved;

} Aircraft_t;  // Current size is 64 bytes

#endif // __AIRCRAFT_H__
