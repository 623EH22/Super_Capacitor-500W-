#include "PID.h"

static inline float Normalization_func(float exp, float real) {
    float error = exp - real;
    if (error > 180.0f){
        error -= 360.0f;
    } else if (error < -180.0f){
        error += 360.0f;
    }
    return error;
}

static inline float Limit_Max(float value , float max)
{
	if(value > max) value = max;
	if(value < -max) value = -max;
	return value;
}

static inline float Limit_Max_Min(float value , float max , float min)
{
	if(value > max) value = max;
	if(value < min) value = min;
	return value;
}

bool Pid_Handle_Init(Pid_t *pid,const float Kpid[7],const Pid_Mode Mode)
{
	if(pid == NULL || Kpid == NULL) return false;
	pid->Mode = Mode;
	pid->Kp = Kpid[0];
	pid->Ki = Kpid[1];
	pid->Kd = Kpid[2];
	pid->Max_Iout = Kpid[3];
	pid->Min_Iout = Kpid[4];
	pid->Max_Out = Kpid[5];
	pid->Min_Out = Kpid[6];
	pid->Err[0] = 0.0f;
	pid->Err[1] = 0.0f;
	pid->Err[2] = 0.0f;
	pid->Pout = 0.0f;
	pid->Iout = 0.0f;
	pid->Dout = 0.0f;
	pid->Tout = 0.0f;
	pid->Exp 	=	0.0f;
	pid->Real = 0.0f;
	return true;
}

float PID_Calculate(Pid_t *pid,const float real,const float exp)
{
	if(pid == NULL) return 0.0f;
	
	pid->Err[2] = pid->Err[1];
	pid->Err[1] = pid->Err[0];
	
	pid->Exp = exp;
	pid->Real = real;

	pid->Err[0] = exp - real;
	
	if(pid->Mode == Pos_Pid){
		pid->Pout =  pid->Kp * pid->Err[0];
		pid->Iout += (pid->Ki * pid->Err[0]);
		pid->Dout =  pid->Kd * (pid->Err[0] - pid->Err[1]);
		
		pid->Iout = Limit_Max_Min(pid->Iout,pid->Max_Iout, pid->Min_Iout);

		pid->Tout = pid->Pout + pid->Iout + pid->Dout;
		
		pid->Tout = Limit_Max_Min(pid->Tout,pid->Max_Out, pid->Min_Out);
	}else if(pid->Mode == Inc_Pid)
	{
		pid->Pout = pid->Kp * (pid->Err[0] - pid->Err[1]);
		pid->Iout = pid->Ki * pid->Err[0];
		
		pid->Iout = Limit_Max_Min(pid->Iout,pid->Max_Iout, pid->Min_Iout);
		
		pid->Dout = pid->Kd * (pid->Err[0] - 2.0f*pid->Err[1] + pid->Err[2]);
		
		pid->Tout += pid->Pout + pid->Iout + pid->Dout;
		
		pid->Tout = Limit_Max_Min(pid->Tout,pid->Max_Out, pid->Min_Out);
	}
	return pid->Tout;
}

bool PID_Refresh(Pid_t *pid)
{
	if(pid == NULL) return false;
	pid->Err[0] = 0.0f;
	pid->Err[1] = 0.0f;
	pid->Err[2] = 0.0f;
	pid->Pout = 0.0f;
	pid->Iout = 0.0f;
	pid->Dout = 0.0f;
	pid->Tout = 0.0f;
	pid->Exp  = 0.0f;
	pid->Real = 0.0f;
	return true;
}


