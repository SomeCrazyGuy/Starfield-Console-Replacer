#include "main.h"

#include <time.h>


static uint64_t State[4];


static uint64_t splitmix64(uint64_t state) {
	uint64_t result = (state += 0x9E3779B97f4A7C15);
	result = (result ^ (result >> 30)) * 0xBF58476D1CE4E5B9;
	result = (result ^ (result >> 27)) * 0x94D049BB133111EB;
	return result ^ (result >> 31);
}


static uint64_t random_next() {
	static bool init = false;
	
	uint64_t* s = State;
	if (!init) {
		init = true;
		uint64_t tmp = time(NULL);
		tmp = splitmix64(tmp);
		s[0] = tmp;
		tmp = splitmix64(tmp);
		s[1] = tmp;
		tmp = splitmix64(tmp);
		s[2] = tmp;
		tmp = splitmix64(tmp);
		s[3] = tmp;
	}
	
	uint64_t const result = s[0] + s[3];
	uint64_t const t = s[1] << 17;

	s[2] ^= s[0];
	s[3] ^= s[1];
	s[1] ^= s[2];
	s[0] ^= s[3];

	s[2] ^= t;
	s[3] = (s[3] << 45) | (s[3] >> (64 - 45));

	return result;
}


extern uint64_t random_in_range(uint64_t min, uint64_t max) {
	ASSERT(max > min);
	uint64_t range = max - min;
	auto choice = random_next() % (range + 1);
	return choice + min;
}
