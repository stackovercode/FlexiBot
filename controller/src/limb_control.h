#ifndef LIMB_CONTROL_H
#define LIMB_CONTROL_H

#include <Adafruit_PWMServoDriver.h>

#ifndef PCA9685_FREQ
#define PCA9685_FREQ 400
#endif

class LimbControl {
private:
    Adafruit_PWMServoDriver &pwmDriver; 
    uint8_t pwmChannel;               

public:
    LimbControl(Adafruit_PWMServoDriver &pwm, uint8_t channel)
      : pwmDriver(pwm), pwmChannel(channel) {}

    void init() {
        stopMotor(); 
    }

    void stopMotor() {
        setPulse(1550);
    }


    void setRPM(int desiredRPM, bool clockwise) {
        desiredRPM = constrain(desiredRPM, 0, 133); 
        static uint16_t currentPulseWidth = 1450; 

        uint16_t targetPulseWidth;
        if (clockwise) {
            targetPulseWidth = 1450 - ((desiredRPM * 950) / 133);
        } else {
            targetPulseWidth = 1550 + ((desiredRPM * 950) / 133);
        }

        while (currentPulseWidth != targetPulseWidth) {
            if (currentPulseWidth < targetPulseWidth) {
                currentPulseWidth++;
            } else {
                currentPulseWidth--;
            }
            setPulse(currentPulseWidth);
            delay(5);
        }

        Serial.print("Direction: ");
        Serial.print(clockwise ? "CW" : "CCW");
        Serial.print(" | Target RPM: ");
        Serial.print(desiredRPM);
        Serial.print(" -> Pulse Width: ");
        Serial.println(targetPulseWidth);
    }

    // -----------------------------------------------------------
    // Adjusts based on PCA9685_FREQ
    // -----------------------------------------------------------
    void setPulse(uint16_t pulseWidth) {
        pulseWidth = constrain(pulseWidth, 500, 2500);
        uint32_t periodMicrosec;

        if (PCA9685_FREQ == 333) {
            periodMicrosec = 3000;  
        }
        else if (PCA9685_FREQ == 400) {
            periodMicrosec = 2500;
        }
        else if (PCA9685_FREQ == 500) {
            periodMicrosec = 2000; 
        }
        else {
            periodMicrosec = 1000000UL / PCA9685_FREQ; 
        }

        uint32_t pwmValue = (uint32_t)pulseWidth * 4096UL / periodMicrosec;
        if (pwmValue > 4095) pwmValue = 4094;

        Serial.print("Channel ");
        Serial.print(pwmChannel);
        Serial.print(" => Pulse: ");
        Serial.print(pulseWidth);
        Serial.print(" µs => ");
        Serial.print(pwmValue);
        Serial.print(" ticks (out of 4095) at ");
        Serial.print(PCA9685_FREQ);
        Serial.println(" Hz");

        pwmDriver.setPWM(pwmChannel, 0, pwmValue);
    }


    void smoothSpeedChange(int desiredRPM, bool clockwise) {
        desiredRPM = constrain(desiredRPM, 0, 133);
        static uint16_t currentPulseWidth = 1450;
        uint16_t targetPulseWidth;
        if (clockwise) {
            targetPulseWidth = 1450 - ((desiredRPM * 950) / 133);
        } else {
            targetPulseWidth = 1550 + ((desiredRPM * 950) / 133);
        }
        while (currentPulseWidth != targetPulseWidth) {
            if (currentPulseWidth < targetPulseWidth) {
                currentPulseWidth++;
            } else {
                currentPulseWidth--;
            }
            setPulse(currentPulseWidth);
            delay(5);
        }
        Serial.print("Smooth Speed -> RPM: ");
        Serial.print(desiredRPM);
        Serial.print(" | Pulse: ");
        Serial.println(targetPulseWidth);
    }

    void rotateClockwise(uint16_t pulseWidth) {
        pulseWidth = constrain(pulseWidth, 500, 1400);
        setPulse(pulseWidth);
    }

    void rotateCounterClockwise(uint16_t pulseWidth) {
        pulseWidth = constrain(pulseWidth, 1600, 2500);
        setPulse(pulseWidth);
    }
};

#endif
