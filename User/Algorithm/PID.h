#ifndef PID_H
#define PID_H

#include "stdlib.h"
#include "stdio.h"
#include "stdbool.h"
#include "math.h"
#include "arm_math.h"

typedef enum
{
	Pos_Pid,		/*位置式PID*/
	Inc_Pid			/*增量式PID*/
}Pid_Mode;

typedef struct
{
	Pid_Mode Mode;
	float Kp , Ki , Kd;
	float Max_Out , Max_Iout, Min_Out , Min_Iout;
	volatile float Exp , Real;
	volatile float Pout , Iout , Dout , Tout;
	volatile float Err[3];
}Pid_t;

bool Pid_Handle_Init(Pid_t *pid,const float Kpid[7],const Pid_Mode Mode);
float PID_Calculate(Pid_t *pid,const float real ,const float exp);
bool PID_Refresh(Pid_t *pid);

#endif