#ifndef BODY_CONTROL_H
#define BODY_CONTROL_H

#include "limb_control.h"

class BodyControl {
private:
    LimbControl& motor1; 
    LimbControl& motor2; 

public:
    BodyControl(LimbControl& m1, LimbControl& m2)
        : motor1(m1), motor2(m2) {}

    void init() {
        motor1.stopMotor();
        motor2.stopMotor();
    }

    // Rotating both motors clockwise
    void moveForward(uint16_t pulseWidth, int duration) {
        motor1.rotateClockwise(pulseWidth);
        motor2.rotateClockwise(pulseWidth);
        delay(duration);
        stop();
    }

    // Rotating both motors counterclockwise
    void moveBackward(uint16_t pulseWidth, int duration) {
        motor1.rotateCounterClockwise(pulseWidth);
        motor2.rotateCounterClockwise(pulseWidth);
        delay(duration);
        stop();
    }

    // Stop both motors
    void stop() {
        motor1.stopMotor();
        motor2.stopMotor();
    }

    // Rotate Motor 1 Clockwise
    void rotateMotor1Clockwise(uint16_t pulseWidth, unsigned long duration) {
        Serial.println("Motor 1 rotating clockwise...");
        motor1.rotateClockwise(pulseWidth);
        delay(duration);
        motor1.stopMotor();
    }

    // Rotate Motor 1 Counterclockwise
    void rotateMotor1Counterclockwise(uint16_t pulseWidth, unsigned long duration) {
        Serial.println("Motor 1 rotating counterclockwise...");
        motor1.rotateCounterClockwise(pulseWidth);
        delay(duration);
        motor1.stopMotor();
    }

    // Rotate Motor 2 Clockwise
    void rotateMotor2Clockwise(uint16_t pulseWidth, unsigned long duration) {
        Serial.println("Motor 2 rotating clockwise...");
        motor2.rotateClockwise(pulseWidth);
        delay(duration);
        motor2.stopMotor();
    }

    // Rotate Motor 2 Counterclockwise
    void rotateMotor2Counterclockwise(uint16_t pulseWidth, unsigned long duration) {
        Serial.println("Motor 2 rotating counterclockwise...");
        motor2.rotateCounterClockwise(pulseWidth);
        delay(duration);
        motor2.stopMotor();
    }

    // Fine control Motor 1
    void rotateMotor1Fine(uint16_t pulseWidth, unsigned long duration) {
        Serial.print("Motor 1 fine control at pulse width: ");
        Serial.println(pulseWidth);
        motor1.setPulse(pulseWidth);
        delay(duration);
        motor1.stopMotor();
    }

    // Fine control Motor 2
    void rotateMotor2Fine(uint16_t pulseWidth, unsigned long duration) {
        Serial.print("Motor 2 fine control at pulse width: ");
        Serial.println(pulseWidth);
        motor2.setPulse(pulseWidth);
        delay(duration);
        motor2.stopMotor();
    }
};

#endif
