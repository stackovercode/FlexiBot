#ifndef CALIBRATION_H
#define CALIBRATION_H

#include "limb_control.h"

class Calibration {
private:
    LimbControl* limbs;
    int numLimbs;
    const int calibrationPulseIncrement = 10; 
    const int calibrationDelay = 50;          

public:
    Calibration(LimbControl* limbsArray, int limbsCount)
        : limbs(limbsArray), numLimbs(limbsCount) {}

    void calibrateLimb(int limbIndex) {
        if (limbIndex < 0 || limbIndex >= numLimbs) {
            Serial.println("Invalid limb index for calibration!");
            return;
        }

        Serial.print("Starting calibration for Limb ");
        Serial.println(limbIndex + 1);
        displayMessage("Calibrating Limb " + String(limbIndex + 1));

       
        calibrateMotor(limbIndex, 0);
        calibrateMotor(limbIndex, 1); 

        Serial.print("Calibration completed for Limb ");
        Serial.println(limbIndex + 1);
        displayMessage("Calibration Done");
    }

    // Calibrate all limbs
    void calibrateAllLimbs() {
        for (int i = 0; i < numLimbs; i++) {
            calibrateLimb(i);
            delay(500); // Delay between limb calibrations
        }
        Serial.println("All limbs calibrated.");
        displayMessage("All Limbs Calibrated");
    }

private:
    void calibrateMotor(int limbIndex, int motorOffset) {
        int motorIndex = limbIndex * 2 + motorOffset;
        if (motorIndex >= numLimbs) {
            Serial.println("Motor index out of range during calibration!");
            return;
        }

        Serial.print("Calibrating Motor ");
        Serial.println(motorIndex + 1);
        displayMessage("Calibrating M" + String(motorIndex + 1));

        limbs[motorIndex].setPulse(1450);
        delay(1000); 

        for (int pulse = 1450; pulse <= 1500; pulse += calibrationPulseIncrement) {
            limbs[motorIndex].setPulse(pulse);
            delay(calibrationDelay);
        }

        limbs[motorIndex].setPulse(1450);
        delay(500);

        Serial.print("Motor ");
        Serial.print(motorIndex + 1);
        Serial.println(" calibrated.");
    }


    void displayMessage(const String& message) {
        // Assuming you have access to the display object
        // extern Adafruit_SSD1306 display;
        // display.clearDisplay();
        // display.setCursor(0, 0);
        // display.println(message);
        // display.display();
      Serial.println(message);
    }
};

#endif // CALIBRATION_H
