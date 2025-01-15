#ifndef GAIT_CONTROL_H
#define GAIT_CONTROL_H

#include "limb_control.h"

class GaitControl {
private:
    LimbControl* limbs;
    int numLimbs;

    enum GaitState {
        STOP,
        CRAWLING,
        FASTCRAWL,
        WALKING
    } state;

    unsigned long lastStepMs_crawl = 0;
    uint8_t crawlStep = 0;
    const unsigned long stepInterval_crawl = 1500;

    unsigned long lastStepMs_fast = 0;
    uint8_t fastStep = 0;
    const unsigned long stepInterval_fast = 1500;

public:
    enum GaitStatesEnum {
        STOP_STATE     = STOP,
        CRAWLING_STATE = CRAWLING,
        FASTCRAWL_STATE= FASTCRAWL,
        WALKING_STATE  = WALKING
    };

    GaitControl(LimbControl* limbArray, int limbCount) 
      : limbs(limbArray), numLimbs(limbCount), state(STOP)
    {}

    void init() {
    }

    void setState(GaitStatesEnum newState) {
        state = (GaitState)newState;
        switch(state) {
            case CRAWLING:
                Serial.println("[GaitControl] => CRAWLING");
                crawlStep = 0;
                lastStepMs_crawl = millis();
                break;
            case FASTCRAWL:
                Serial.println("[GaitControl] => FASTCRAWL");
                fastStep = 0;
                lastStepMs_fast = millis();
                break;
            case WALKING:
                Serial.println("[GaitControl] => WALKING");
                break;
            case STOP:
            default:
                Serial.println("[GaitControl] => STOP");
                stopAllLimbs();
                break;
        }
    }

    void update() {
        switch(state) {
            case CRAWLING:
                updateCrawling();
                break;
            case FASTCRAWL:
                updateFastCrawl();
                break;
            case WALKING:
                updateWalking();
                break;
            case STOP:
            default:
                stopAllLimbs();
                break;
        }
    }

private:
    // --------------------------------------------------------
    // Slow 8-step CRAWLING (Not completed)
    // --------------------------------------------------------
    void updateCrawling() {
        unsigned long now = millis();
        if (now - lastStepMs_crawl < stepInterval_crawl) return;
        lastStepMs_crawl = now;
        /*
           8-step cycle. 
           - Limb1 => (M1=0, M2=1)
           - Limb2 => (M3=2, M4=3)
           - Limb3 => (M5=4, M6=5)
           - Limb4 => (M7=6, M8=7)
          
          "Bend down" => bottom tendon pulls
          "Bend up"   => top tendon pulls
        */

        switch(crawlStep) {
          case 0:
            Serial.println("[Crawl] Step0: Limb1 bend down");
            bendLimbDown(0,1);
            break;
          case 1:
            Serial.println("[Crawl] Step1: Limb1 bend up");
            bendLimbUp(0,1);
            break;
          case 2:
            Serial.println("[Crawl] Step2: Limb2 bend down");
            bendLimbDown(2,3);
            break;
          case 3:
            Serial.println("[Crawl] Step3: Limb2 bend up");
            bendLimbUp(2,3);
            break;
          case 4:
            Serial.println("[Crawl] Step4: Limb3 bend down");
            bendLimbDown(4,5);
            break;
          case 5:
            Serial.println("[Crawl] Step5: Limb3 bend up");
            bendLimbUp(4,5);
            break;
          case 6:
            Serial.println("[Crawl] Step6: Limb4 bend down");
            bendLimbDown(6,7);
            break;
          case 7:
          default:
            Serial.println("[Crawl] Step7: Limb4 bend up");
            bendLimbUp(6,7);
            break;
        }

        crawlStep = (crawlStep + 1) % 8;
    }

    // --------------------------------------------------------
    // FASTCRAWL (Not completed)
    // --------------------------------------------------------
    void updateFastCrawl() {
        unsigned long now = millis();
        if (now - lastStepMs_fast < stepInterval_fast) return;
        lastStepMs_fast = now;


        switch(fastStep) {
          case 0:
            Serial.println("[FastCrawl] Step0: compress L1, anchor L2, stop L3,4");
            compressLimb(0,1);
            anchorLimbDown(2,3);
            stopLimb(4,5);
            stopLimb(6,7);
            break;
          case 1:
            Serial.println("[FastCrawl] Step1: stop L1, anchor L2, compress L3, stop L4");
            stopLimb(0,1);
            anchorLimbDown(2,3);
            compressLimb(4,5);
            stopLimb(6,7);
            break;
          case 2:
            Serial.println("[FastCrawl] Step2: anchor L1, stop L2, compress L3, stop L4");
            anchorLimbDown(0,1);
            stopLimb(2,3);
            compressLimb(4,5);
            stopLimb(6,7);
            break;
          case 3:
            Serial.println("[FastCrawl] Step3: anchor L1, stop L2, stop L3, compress L4");
            anchorLimbDown(0,1);
            stopLimb(2,3);
            stopLimb(4,5);
            compressLimb(6,7);
            break;
          case 4:
            Serial.println("[FastCrawl] Step4: compress L1, anchor L2, anchor L3, anchor L4");
            compressLimb(0,1);
            anchorLimbDown(2,3);
            anchorLimbDown(4,5);
            anchorLimbDown(6,7);
            break;
          case 5:
            Serial.println("[FastCrawl] Step5: anchor L1, compress L2, anchor L3, anchor L4");
            anchorLimbDown(0,1);
            compressLimb(2,3);
            anchorLimbDown(4,5);
            anchorLimbDown(6,7);
            break;
          case 6:
            Serial.println("[FastCrawl] Step6: anchor L1, anchor L2, compress L3, anchor L4");
            anchorLimbDown(0,1);
            anchorLimbDown(2,3);
            compressLimb(4,5);
            anchorLimbDown(6,7);
            break;
          case 7:
          default:
            Serial.println("[FastCrawl] Step7: anchor L1, anchor L2, anchor L3, compress L4");
            anchorLimbDown(0,1);
            anchorLimbDown(2,3);
            anchorLimbDown(4,5);
            compressLimb(6,7);
            break;
        }

        fastStep = (fastStep + 1) % 8;
    }

    void updateWalking() {
        // Not implemented
        Serial.println("[GaitControl] Walking is not implemented.");
        stopAllLimbs();
    }
    void stopAllLimbs() {
        for(int i = 0; i < 8; i++){
            limbs[i].stopMotor();
        }
    }

    void stopLimb(int topIdx, int botIdx) {
        limbs[topIdx].stopMotor();
        limbs[botIdx].stopMotor();
    }

    void bendLimbUp(int topIdx, int botIdx) {
        limbs[topIdx].rotateClockwise(700);
        limbs[botIdx].stopMotor();
    }

    void bendLimbDown(int topIdx, int botIdx) {
        limbs[botIdx].rotateClockwise(700);
        limbs[topIdx].stopMotor();
    }

    void compressLimb(int topIdx, int botIdx) {
        limbs[topIdx].rotateClockwise(700);
        limbs[botIdx].rotateClockwise(700);
    }

    void anchorLimbDown(int topIdx, int botIdx) {
        limbs[topIdx].stopMotor();
        limbs[botIdx].rotateClockwise(700);
    }

};

#endif // GAIT_CONTROL_H
