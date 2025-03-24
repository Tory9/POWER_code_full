#include "mppt.h"

int DUTY_CYCLE = 127;
double P_prev = 1;
double V_prev = 1;

int mppt(double P, double V){


    if (P > P_prev) {
        //printf("power UP\n");
        if (V > V_prev) {
            DUTY_CYCLE+= DUTY_STEP;
            //printf("volatge UP\n");
        } else if(V < V_prev){
            DUTY_CYCLE-= DUTY_STEP;
            //printf("volatage DOWN\n");
        }
    } else {
        //printf("power DOWN\n");
        if (V > V_prev) {
            DUTY_CYCLE-= DUTY_STEP;
            //printf("volatge UP\n");
        } else if(V < V_prev){
            DUTY_CYCLE+= DUTY_STEP;
            //printf("volatge DOWN\n");
        }
    }

    // Ensure duty cycle remains in valid range
    if (V >= 14) DUTY_CYCLE = DUTY_CYCLE - 5;
    if (DUTY_CYCLE < 30) DUTY_CYCLE = 30;
    if (DUTY_CYCLE > 230) DUTY_CYCLE = 230;

    ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, DUTY_CYCLE, 0);

    V_prev = V;
    P_prev = P;
    //printf("DUTYYYYY CYCLE: %d , PBUS: %.04f mW \n",  DUTY_CYCLE , P_prev);

    return DUTY_CYCLE;

}