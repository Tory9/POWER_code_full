#include "mppt.h"
#include "ina219_my.h"


#define DUTY_MIN           30
#define DUTY_MAX          230
#define DUTY_MID          127
    #define DUTY_STEP           6       // size of each perturbation
#define POWER_ALPHA       0.30      // EWMA smoothing factor (0-1)
#define POWER_DEAD_BAND    30     // mW – ignore smaller ΔP
#define VOLTAGE_HYST      10      // V – re-centre trigger

static int    duty_cycle   = DUTY_MID;
static int    direction    = +1;          // +1 = raising duty, –1 = lowering
static double p_filt       = 0.0;         // filtered power
static double last_p_filt  = 0.0;
static double v_baseline   = 0.0;
static bool   first_call   = true;

int mppt(double P, double V)
{
    //first pass: initialise reference values
    if (first_call) {
        p_filt      = P;
        last_p_filt = p_filt;
        v_baseline  = V;
        first_call  = false;

        ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0,
                                 duty_cycle, 0);
        return duty_cycle;
    }

    //1. power low-pass filter 
    p_filt = POWER_ALPHA * P + (1.0 - POWER_ALPHA) * p_filt;
    double dP = p_filt - last_p_filt;

    //2. perturb-and-observe with dead-band 
    if (dP >  POWER_DEAD_BAND) {            //power ↑ → stay on course 
        duty_cycle += direction * DUTY_STEP;
    } else if (dP < -POWER_DEAD_BAND) {     // power ↓ → reverse 
        direction   = -direction;
        duty_cycle += direction * DUTY_STEP;
    }
    // else |dP| ≤ dead-band  → treat as noise, hold position

    //3. recentre if PV voltage drifted a lot 
    if (fabs(V - v_baseline) > VOLTAGE_HYST) {
        duty_cycle  = DUTY_MID;
        direction   = +1;           // start climbing again
        v_baseline  = V;            // new reference
    }

    //4. clamp & apply 
    if (duty_cycle < DUTY_MIN) duty_cycle = DUTY_MIN;
    if (duty_cycle > DUTY_MAX) duty_cycle = DUTY_MAX;

    ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0,
                             duty_cycle, 0);

    //5. remember for next cycle 
    last_p_filt = p_filt;
    return duty_cycle;
}

