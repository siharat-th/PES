//
//  PES_Gcode.hpp
//  PES3
//
//  Created by SIHARAT THAMMAYA on 8/5/2562 BE.
//  Copyright © 2562 Jimmy Software Co., Ltd. All rights reserved.
//

#ifndef pesGcode_hpp
#define pesGcode_hpp

#include <stdio.h>
#include "pesRectangle.hpp"
#include <string>
#include <vector>

using namespace std;

// Spindle(rpm)
const int PES_GCODE_MIN_RPM = 100;
const int PES_GCODE_MAX_RPM = 600;
const int PES_GCODE_VEL_RPM = 50;

// Move(mm/min)
const int PES_GCODE_RAPID_SPD = 27000; // G0: Rapid Move
const int PES_GCODE_FEED_SPD  =  2400; // G1: Linear Cutting Move

class PES_Gcode{
public:
    PES_Gcode();
    ~PES_Gcode();
    
    void clear();
    
    void resetToZero();
    void moveTo(float x, float y, int spd=PES_GCODE_RAPID_SPD);
    void needleTo(float x, float y);
    void stopAt(float x, float y);
    void end();
    
    void needleRPM(int rpm);
    void needleStop();
    void needleSlow();
    void delay(int second);
    void tieKnot(const pesVec2f & p, int r0=PES_GCODE_MIN_RPM, int r1=PES_GCODE_MIN_RPM, int r2=PES_GCODE_MIN_RPM);
    void tieKnot(float x, float y, int r0=PES_GCODE_MIN_RPM, int r1=PES_GCODE_MIN_RPM, int r2=PES_GCODE_MIN_RPM);
    
    struct Command{
        enum Type{
            moveTo = 0,
            needleTo,
            needleRPM,
            delay,
            stop,
            end,
            resetToZero
        };
        
        // for resetToZero, stop and end
        Command(Type type);
        
        // for moveTo and needleTo
        Command(Type type , const pesVec2f & p, int spd=PES_GCODE_RAPID_SPD);
        
        // for needleRPM and delay
        Command(Type type , int param);
        
        string toString();
        
        Type type;
        pesVec2f to;
        int moveSpeed; // mm/min
        int needleSpeed; // round/min
        int timeInSecond;
    };
    
    vector<Command> & getCommands();
    const vector<Command> & getCommands() const;
    
private:
    void addCommand(const Command & command);
    float calcNeedleRPM(const pesVec2f & p);
    
    vector<Command>     commands;
};


#endif /* PES_Gcode_hpp */
