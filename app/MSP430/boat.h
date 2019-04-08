/*
 * boat.h
 *
 *  Created on: Apr 4, 2016
 *      Author: harper
 */

#ifndef APP_MSP430_BOAT_H_
#define APP_MSP430_BOAT_H_

void boat_loop(unsigned long, double);
int calculateDifferenceBetweenAngles(int angle1, int angle2);

int calculateTargetTangentArc(unsigned long running_time, unsigned long timestamp);
int calculateTargetSlantToArc(unsigned long running_time, unsigned long timestamp);


#endif /* APP_MSP430_BOAT_H_ */
