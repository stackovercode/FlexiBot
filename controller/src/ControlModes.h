#ifndef CONTROLMODES_H
#define CONTROLMODES_H

// Enum for control modes
enum ControlMode {
    INDIVIDUAL,
    GAIT
};

// Declare the current mode as an extern variable
extern ControlMode currentMode;

#endif // CONTROLMODES_H
