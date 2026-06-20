//
//  PES_Gcode.cpp
//  PES3
//
//  Created by SIHARAT THAMMAYA on 8/5/2562 BE.
//  Copyright © 2562 Jimmy Software Co., Ltd. All rights reserved.
//

#include "pesGcode.hpp"
#include "pesUtility.hpp"

PES_Gcode::Command::Command(Type type)
:type(type)
{}

PES_Gcode::Command::Command(Type type , const pesVec2f & p, int spd)
:type(type)
,to(p)
,moveSpeed(spd)
{}

PES_Gcode::Command::Command(Type type , int param)
:type(type)
{
    if(type==Command::needleRPM)
        needleSpeed = param;
    else if(type==Command::delay)
        timeInSecond = param;
}

string PES_Gcode::Command::toString(){
    string ENDLINE = ""; //"\n";
    
    if(type==Command::moveTo){
        if(moveSpeed>=PES_GCODE_RAPID_SPD){
            return "G0" + pesVAArgsToString("\tX%3.1f Y%3.1f", to.x, to.y) + ENDLINE;
        }
        else{
            return "G1" + pesVAArgsToString("\tX%3.1f Y%3.1f\tF%i", to.x, to.y, moveSpeed) + ENDLINE;
        }
    }
    
    if(type==Command::needleTo){
        return "M0 G0" + pesVAArgsToString("\tX%3.1f Y%3.1f\tM4 S%i", to.x, to.y, needleSpeed) + ENDLINE;
    }
    
    if(type==Command::needleRPM){
        return "M4" + pesVAArgsToString(" S%i", needleSpeed) + ENDLINE;
    }
    
    if(type==Command::delay){
        return "G4" + pesVAArgsToString(" P%i", timeInSecond) + ENDLINE;
    }
    
    if(type==Command::stop){
        return "M0" + ENDLINE;
    }
    
    if(type==Command::end){
        return "M30" + ENDLINE;
    }
    
    if(type==Command::resetToZero){
        return "G92\tX0 Y0 Z0" + ENDLINE;
    }
    
    return ENDLINE;
}

PES_Gcode::PES_Gcode(){
    clear();
    // [GC:G0 G54 G17 G21 G90 G94 M5 M M9 T0 F0 S0]
}

PES_Gcode::~PES_Gcode(){
    clear();
}

void PES_Gcode::clear(){
    PES_Gcode::commands.clear();
}

void PES_Gcode::resetToZero(){
    addCommand(Command(Command::resetToZero));
}

void PES_Gcode::moveTo(float x, float y, int spd){
    if(commands.size() && commands.back().type==Command::needleTo){
        pesVec2f p0 = commands.back().to;
        
        int r0 = pesClamp(commands.back().needleSpeed - PES_GCODE_VEL_RPM, PES_GCODE_MIN_RPM, PES_GCODE_MAX_RPM);
        int r1 = pesClamp(r0 - PES_GCODE_VEL_RPM, PES_GCODE_MIN_RPM, PES_GCODE_MAX_RPM);
        int r2 = pesClamp(r1 - PES_GCODE_VEL_RPM, PES_GCODE_MIN_RPM, PES_GCODE_MAX_RPM);
        
//        // v1
//        tieKnot(p0, r0, r1, r2);
//        needleSlow();
//        needleRPM(0);
//        delay(1);
//        // end v1
        
        // v2 (not confirm yet)
        tieKnot(p0, r0, r1, r2);
        if(spd<PES_GCODE_RAPID_SPD){
            needleSlow();
            needleRPM(0);
            delay(1);
        }
        else{
            needleRPM(0);
        }
        // end v2
    }
//    addCommand(Command(Command::moveTo, {x, y}, spd));
    addCommand(Command(Command::moveTo, {x, y}, PES_GCODE_FEED_SPD));
}

void PES_Gcode::needleTo(float x, float y){
    if(commands.size() && commands.back().type==Command::moveTo){
        pesVec2f p0 = commands.back().to;
        needleRPM(PES_GCODE_MIN_RPM);
        int r0 = PES_GCODE_MIN_RPM;
        int r1 = r0 + PES_GCODE_VEL_RPM;
        int r2 = r1 + PES_GCODE_VEL_RPM;
        tieKnot(p0, r0, r1, r2);
    }
    
    int prev_needle_speed = 0;
    for(auto it=commands.rbegin(); it!=commands.rend(); it++){
        if(it->type==Command::needleTo || it->type==Command::needleRPM){
            prev_needle_speed = it->needleSpeed;
            break;
        }
    }
    
    Command c = Command(Command::needleTo, {x, y});
    int targetRPM = pesClamp(calcNeedleRPM(c.to), PES_GCODE_MIN_RPM, PES_GCODE_MAX_RPM);
    int interpolateRPM = PES_GCODE_MIN_RPM;
    if(prev_needle_speed==0){
        interpolateRPM = PES_GCODE_MIN_RPM;
    }
    else{
        int deltaRPM = targetRPM - prev_needle_speed;
        if(abs(deltaRPM)>PES_GCODE_VEL_RPM){
            if(deltaRPM<0){
                interpolateRPM = prev_needle_speed - PES_GCODE_VEL_RPM;
            }
            else{
                interpolateRPM = prev_needle_speed + PES_GCODE_VEL_RPM;
            }
        }
        else{
            interpolateRPM = targetRPM;
        }
    }
    c.needleSpeed = interpolateRPM;
    addCommand(c);
}

void PES_Gcode::needleRPM(int rpm){
    addCommand(Command(Command::needleRPM, rpm));
}

void PES_Gcode::needleSlow(){
    int count = 0;
    int step = PES_GCODE_VEL_RPM; // slow down needle rpm to 0 with step
    for(auto it=commands.rbegin(); it!=commands.rend(); it++){
        if(it->type!=Command::needleTo)
            break;
        
        int targetRPM = PES_GCODE_MIN_RPM + count*step;
        if(targetRPM>PES_GCODE_MAX_RPM)
            break;
        
        if(it->needleSpeed>targetRPM)
            it->needleSpeed = targetRPM;
        count++;
    }
}

void PES_Gcode::needleStop(){
    needleSlow();
    needleRPM(0);
}

float PES_Gcode::calcNeedleRPM(const pesVec2f & p){
    pesVec2f p0(0,0);
    for(auto it=commands.rbegin(); it!=commands.rend(); it++){
        if(it->type==Command::needleTo || it->type==Command::moveTo){
            p0.set(it->to.x, it->to.y);
            break;
        }
    }
    
    float distInMM = p.distance(p0);
    return 1138.42/sqrt(distInMM);
}

void PES_Gcode::tieKnot(const pesVec2f & p, int r0, int r1, int r2){
    Command c = Command(Command::needleTo, p);
    c.needleSpeed = PES_GCODE_MIN_RPM;
    
    // [CONTINGENCY_TIE_ON_THREE_SMALL] which uses three small stitches to tie on the thread
    int numKnots = 3;
    float dy[] = {-0.1, 0.1, 0};
    int r[] = {r0, r1, r2};
    for(int i=0; i<numKnots; i++){
        addCommand(c);
        commands.back().to.y += dy[i];
        commands.back().needleSpeed = r[i];
    }
}

void PES_Gcode::tieKnot(float x, float y, int r0, int r1, int r2){
    tieKnot({x, y}, r0, r1, r2);
}

void PES_Gcode::delay(int second){
    addCommand(Command(Command::delay, second));
}

void PES_Gcode::stopAt(float x, float y){
    needleSlow();
    
    pesVec2f p0(0,0);
    bool founded = false;
    for(auto it=commands.rbegin(); it!=commands.rend(); it++){
        if(it->type==Command::needleTo || it->type==Command::moveTo){
            p0.set(it->to.x, it->to.y);
            founded = true;
            break;
        }
    }
    
    if(founded){
        tieKnot(p0);
    }
    needleRPM(0);
    delay(1);
    moveTo(x, y, PES_GCODE_FEED_SPD);
    addCommand(Command(Command::stop));
    
    tieKnot(x, y);
}

void PES_Gcode::end(){
    needleSlow();
    
    pesVec2f p0(0,0);
    bool founded = false;
    for(auto it=commands.rbegin(); it!=commands.rend(); it++){
        if(it->type==Command::needleTo || it->type==Command::moveTo){
            p0.set(it->to.x, it->to.y);
            founded = true;
            break;
        }
    }
    
    if(founded){
        tieKnot(p0);
    }
    needleRPM(0);
    delay(1);
    
    addCommand(Command(Command::end));
}

void PES_Gcode::addCommand(const PES_Gcode::Command & command){
    if(commands.size() && commands.back().type==Command::end)
        return;
    commands.push_back(command);
}

vector<PES_Gcode::Command> & PES_Gcode::getCommands(){
    return commands;
}

const vector<PES_Gcode::Command> & PES_Gcode::getCommands() const{
    return commands;
}

