/*
 * boat.c
 *
 *  Created on: Apr 4, 2016
 *      Author: harper
 */
#include "boat.h"
#include "led.h"
#include "servo.h"
#include "log.h"
#include "stdio.h"
#include "button.h"
#include "pwm.h"

// boat constants
#define BOAT_SPEED 1 // boat speed in m/s
#define LINE_LENGTH 1 // straight line distance in meters
#define SLANT_LENGTH 1 // slanted line distance in meters
#define SLANT_ANGLE 30
#define TURN_RADIUS 1 // semicircle turn radius in meters

#define PI 3.14159

unsigned long last_time = 0;     // last time through the loop
unsigned long turn_start_time;   // time when turning started
unsigned int isTurning = 0;
unsigned int turningFinished = 0;

unsigned int drive_phase = 0;
unsigned long last_phase_time = 0;
/*
 * boat_loop
 * ----------------------------
 * This routine is called in the main loop at a rate of ~100 times/sec.
 * The current timestamp in milliseconds and the current heading (0-359) is passed in.
 * Note: must use static variables if you need a persistence between calls.
 */
void boat_loop(unsigned long timestamp, double heading) {

	// Static variables.  Keep their values between calls to "boat_loop"
    static double old_heading=0.0;			// used to calculate the delta heading
    static unsigned long start_time;		// actual time the boat started.  Used as an offset to calculate elapsed time
    static int start=-1;					// has the boat started, -1=not ready, 0=ready, 1=started
    static double heading_zero;				// heading offset.  Used to zero heading when button is pressed
    static int first_time=1;                // flag to run one time routines
    
    unsigned long running_time;				// elapsed time since the mission started
    
    double P = 2.0;							// P constant from the PID algorithm
    //double I = 0.01;						    // I constant from the PID algorithm
    int target = 0;							// Current heading target that the boat should seek
    int error = 0;							// error between current heading and target heading
    int rudder = 0;							// calculated rudder angle
    
    double heading_rate;			            // Calculated delta between current and last reading of heading

    //--------------------------------------------------------------------------------
    // pre-start
    //--------------------------------------------------------------------------------

    // Run one time initialization routines.
    if (first_time ==1) {
        servo1_move_to_angle(90);
        servo2_move_to_angle(0);  // Start motor a zero speed. range 0-180
        set_pwm_duty_cycle_1(0.0);
        set_pwm_duty_cycle_2(0.0);
        first_time = 0;
    }

    // calculate heading turn rate.  this can be used to give a measure of how fast the boat's heading is drifting when the
    // boat is still.  If good, stable rate should be less than .005.
    heading_rate = heading - old_heading;
    old_heading = heading;

    // print heading every .5 seconds
    if ((timestamp - last_time) > 500) {
        printf("Heading: %lf, %lf\n", heading, heading_rate);
        last_time = timestamp;
    }
    
    // if the heading rate is less than some constant then turn on the Green LED
    if (fabs(heading_rate) < .005)
        set_led2(1);
    else
        set_led2(0);

    // Restart when button 1 is pressed
    // - reset heading
    // - reset start
    // - reset first_time
    if (launchpad_button_1_pressed() == 1) {
        heading_zero = heading;
        start=0;
        first_time=1;
    }
    
    // Calculate the corrected heading taking into account the heading_zero value
    heading = heading - heading_zero;
    if (heading > 180) 
        heading = heading - 360;
    else if (heading < 0.0)
        heading = heading + 360;

    // check for boat start.
    if (((calculateDifferenceBetweenAngles(heading, 90)) > 0.0) && (start==0)) {
        start = 1;
        start_time = timestamp;
        printf("Started\n");

        // Start motor controller connected to the servo2 port
        servo2_move_to_angle(90);  // range 0-180

        //set_pwm_duty_cycle_1(0.5);
        //set_pwm_duty_cycle_2(0.2);
    }

    // handle orange "running LED"
    // blinking: pre-start
    // solid: boat is being controlled by program
    if (start==1)  {
        set_sensorhub_led(1);
    } else {
        blink_sensorhub_led();
    }

    //--------------------------------------------------------------------------------
    // Main routine that runs after boat has started
    //--------------------------------------------------------------------------------
    if (start==1) {
        running_time = timestamp - start_time;			// calculate elapsed time
        
    //     // place timed events here
    //      if (running_time < 10000)
    //          target = 0;                                // run for 10s straight ahead
    //      else if (running_time < 20000)
    //          target = 270;                               // turn to 270 for 10s
    //      else
    //          target = 180;                              // turn to 270 for 10s

        target = calculateTargetTangentArc(running_time, timestamp);
        // target = calculateTargetSlantToArc(running_time, timestamp);

        
        // PID routine
        // bigger P causes boat to have more reaction to heading errors
        error = calculateDifferenceBetweenAngles(heading, target);
        rudder = P * error;

        // set limits on rudder movement
        if (rudder > 90) rudder  = 90;
        if (rudder < -90) rudder = -90;

        // set servo angles in response to PID
        servo1_move_to_angle(90 + rudder);              // if rudder is reversed, change + to -
        

        // Log debugging information
        // %lf: double
        // %f: float
        // %d: integer
        //MPL_LOGE("Heading: %lf, %lf\n", heading, heading_rate)

        //set_pwm_duty_cycle_1(rudder/90.0);
        //set_pwm_duty_cycle_2(rudder/90.0);
      
    }

    // Turn on LED if heading +- 5 degrees of the target
    if (abs(calculateDifferenceBetweenAngles(heading, target)) < 5.0) {
		set_led1(1);
	} else {
		set_led1(0);
    }

}

/*
 * calculateDifferenceBetweenAngles
 * ---------------------------------
 * Return the difference between two angles in a 0-360 system
 * - returns +-179
 */
int calculateDifferenceBetweenAngles(int angle1, int angle2) {
   int delta;

    delta = (angle1 - angle2 + 360) % 360;
       if (delta > 180) delta = delta - 360;

     return delta;
}

int calculateTargetTangentArc(unsigned long running_time, unsigned long timestamp)
{
    int target;

    if (!isTurning)
    {
        // go straight forwards at the beginning
        if (running_time < 1000 * LINE_LENGTH / BOAT_SPEED)
        {
            target = 0;
        }
        // go straight backwards at the end, after turning
        else if (turningFinished)
        {
            target = 180;
        }
        // initialize turning
        else
        {
            isTurning = 1;
            turn_start_time = timestamp;
        }
    }
    // turn to heading calculated via constants
    else
    {
        // target angle is arc length (time * speed) divided by radius, converted to degrees
        target = (180.0 * PI) * (timestamp - turn_start_time) * (BOAT_SPEED / TURN_RADIUS);
        target = 0 - target;

        // if calculated heading is back to start, end turning
        if (target < -180)
        {
            isTurning = 0;
            turningFinished = 1;
        }
    }
    return target;
}

int calculateTargetSlantToArc(unsigned long running_time, unsigned long timestamp)
{
    int target;

    switch (drive_phase)
    {
    case 0:
        if (running_time < 1000 * LINE_LENGTH / BOAT_SPEED)
            target = 0;
        else
        {
            drive_phase++;
            last_phase_time = timestamp;
        }
        break;

    case 1:
        if (running_time - last_phase_time < 1000 * SLANT_LENGTH / BOAT_SPEED)
            target = 0 - SLANT_ANGLE;
        else
        {
            drive_phase++;
            last_phase_time = timestamp;
        }
        break;
    
    case 2:
        target = (180.0 * PI) * (timestamp - last_phase_time) * (BOAT_SPEED / TURN_RADIUS);
        target = 0 - (SLANT_ANGLE +  target);

        if (target < SLANT_ANGLE - 180)
        {
            drive_phase++;
            last_phase_time = timestamp;
        }

    case 3:
        if (running_time - last_phase_time < 1000 * SLANT_LENGTH / BOAT_SPEED)
            target = SLANT_ANGLE - 180;
        else
        {
            drive_phase++;
            last_phase_time = timestamp;
        }
        break;

    case 4:
        target = 180;
        break;
    }

    return target;
}
