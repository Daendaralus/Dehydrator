/*
MIT License
Copyright (c) 2020 Fabian Friederichs
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */ 

#ifndef PID_H_
#define PID_H_
#include <stdint.h>
#include <math.h>
#define PID_DELTA_T 0.02 // ~50hz
typedef struct
{
	float old_process_value;
	float integrator;
	float smd1;
	float smd2;
	float Kp;
	float Ti;
	float Td;
	float i_clamp;
	float offset;
	float smoothing_factor;
	float control_min;
	float control_max;
} pid_state_t;

void pid_init(pid_state_t* state, float pid_Kp, float pid_Ti, float pid_Td, float pid_i_clamp, float pid_offset, float smoothing_factor, float control_min, float control_max);
void pid_set_params(pid_state_t* state, float pid_Kp, float pid_Ti, float pid_Td, float pid_i_clamp, float pid_offset, float smoothing_factor, float control_min, float control_max);
float pid_step(pid_state_t* state, float process_value, float set_value);
void pid_reset(pid_state_t* state);

#endif /* PID_H_ */