//
//  pesPath.cpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 2/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#include "pesPath.hpp"

using namespace std;

pesPath::Command::Command(Type type)
:type(type)
{
}

//----------------------------------------------------------
pesPath::Command::Command(Type type , const pesVec3f & p)
:type(type)
,to(p)
{
}

//----------------------------------------------------------
pesPath::Command::Command(Type type , const pesVec3f & p, const pesVec3f & cp1, const pesVec3f & cp2)
:type(type)
,to(p)
,cp1(cp1)
,cp2(cp2)
{
}

//----------------------------------------------------------
pesPath::Command::Command(Type type , const pesVec3f & centre, float radiusX, float radiusY, float angleBegin, float angleEnd)
:type(type)
,to(centre)
,radiusX(radiusX)
,radiusY(radiusY)
,angleBegin(angleBegin)
,angleEnd(angleEnd)
{
}

pesPath::pesPath():
strokeWidth(0),
bFill(false),
prevCurveRes(20),
curveResolution(20),
circleResolution(20),
mode(COMMANDS),
bHasChanged(false),
bUseShapeColor(true),
bNeedsPolylinesGeneration(false),
bVisible(true),
flags(FLAG_NONE),
roleType(0),
roleOffset(0),
fillType(pesPath::FILL_TYPE_DEFAULT),
fillSubtype(0),
fillAngle(0),
fillSize(0),
fillDensity(0),
fillSpacing(0),
fillPitch(0),
fillPullCompensate(0),
strokeType(0),
strokeSubtype(0),
strokeOffset(0),
strokeSize(0),
strokeDensity(0),
strokeSpacing(0),
strokePitch(0),
strokePullCompensate(0),
fillRule(0) 
{
    clear();
}

void pesPath::append(const pesPath & path){
    if(mode==COMMANDS){
        for(auto & command: path.getCommands()){
            addCommand(command);
        }
    }else{
        for(auto & poly: path.getOutline()){
            polylines.push_back(poly);
        }
    }
    flagShapeChanged();
}

void pesPath::clear(){
    commands.clear();
    // NooM: fixed
    if (polylines.size() > 1) {
        polylines.resize(1);
        polylines[0].clear();
    }
    flagShapeChanged();
}

void pesPath::close(){
    if(mode==COMMANDS){
        addCommand(Command(Command::Type::_close));
    }else{
        lastPolyline().setClosed(true);
    }
    flagShapeChanged();
}

void pesPath::lineTo(float x, float y){
    pesVec2f p(x,y);
    if(mode==COMMANDS){
        addCommand(Command(Command::Type::_lineTo,p));
    }else{
        lastPolyline().lineTo(x, y);
    }
    flagShapeChanged();
}

void pesPath::moveTo(float x, float y){
    pesVec2f p(x,y);
    if(mode==COMMANDS){
        addCommand(Command(Command::Type::_moveTo, p));
    }else{
        if(lastPolyline().size()>0) newSubPath();
        lastPolyline().addVertex(p);
    }
    flagShapeChanged();
}

void pesPath::curveTo(float x, float y){
    pesVec2f p(x,y);
    if(mode==COMMANDS){
        addCommand(Command(Command::Type::_curveTo, p));
    }else{
        lastPolyline().curveTo(p, curveResolution);
    }
    flagShapeChanged();
}

void pesPath::bezierTo(const pesVec2f & cp1, const pesVec2f & cp2, const pesVec2f & p){
    if(mode==COMMANDS){
        addCommand(Command(Command::Type::_bezierTo,p,cp1,cp2));
    }else{
        lastPolyline().bezierTo(cp1,cp2,p,curveResolution);
    }
    flagShapeChanged();
}

void pesPath::quadBezierTo(const pesVec2f & cp1, const pesVec2f & cp2, const pesVec2f & p){
    if(mode==COMMANDS){
        addCommand(Command(Command::Type::_quadBezierTo,p,cp1,cp2));
    }else{
        lastPolyline().quadBezierTo(cp1,cp2,p,curveResolution);
    }
    flagShapeChanged();
}

void pesPath::arc(const pesVec2f & centre, float radiusX, float radiusY, float angleBegin, float angleEnd){
    if(mode==COMMANDS){
        addCommand(Command(Command::Type::_arc,centre,radiusX,radiusY,angleBegin,angleEnd));
    }else{
        lastPolyline().arc(centre,radiusX,radiusY,angleBegin,angleEnd,circleResolution);
    }
    flagShapeChanged();
}

void pesPath::arcNegative(const pesVec2f & centre, float radiusX, float radiusY, float angleBegin, float angleEnd){
    if(mode==COMMANDS){
        addCommand(Command(Command::Type::_arcNegative,centre,radiusX,radiusY,angleBegin,angleEnd));
    }else{
        lastPolyline().arcNegative(centre,radiusX,radiusY,angleBegin,angleEnd,circleResolution);
    }
    flagShapeChanged();
}

void pesPath::circle(float x, float y, float radius){
    moveTo(x+radius, y);
    arc(pesVec2f(x, y), radius, radius, 0, 360);
}

void pesPath::circle(const pesVec2f & p, float radius){
    circle(p.x,p.y,radius);
}


void pesPath::rectangle(const pesRectangle & r){
    rectangle(r.x, r.y, r.width, r.height);
}

void pesPath::rectangle(const pesVec2f & p,float w,float h){
    rectangle(p.x, p.y, w, h);
}

void pesPath::rectangle(float x,float y,float w,float h){
    moveTo(x,y);
    lineTo(x+w,y);
    lineTo(x+w,y+h);
    lineTo(x,y+h);
    close();
}

void pesPath::simplify(float tolerance, bool remake) {
    if (remake) {
        bNeedsPolylinesGeneration = true;
    }
    if(mode==COMMANDS) generatePolylinesFromCommands();
    for(int i=0;i<(int)polylines.size();i++){
        polylines[i].simplify(tolerance);
    }
}

// void pesPath::scale(float x, float y){
//     if(mode==COMMANDS){
//         for(int j=0;j<(int)commands.size();j++){
//             commands[j].to.x*=x;
//             commands[j].to.y*=y;
//             if(commands[j].type==Command::Type::_bezierTo || commands[j].type==Command::Type::_quadBezierTo){
//                 commands[j].cp1.x*=x;
//                 commands[j].cp1.y*=y;
//                 commands[j].cp2.x*=x;
//                 commands[j].cp2.y*=y;
//             }
//             if(commands[j].type==Command::Type::_arc || commands[j].type==Command::Type::_arcNegative){
//                 commands[j].radiusX *= x;
//                 commands[j].radiusY *= y;
//             }
//         }
//     }else{
//         for(int i=0;i<(int)polylines.size();i++){
//             for(int j=0;j<(int)polylines[i].size();j++){
//                 polylines[i][j].x*=x;
//                 polylines[i][j].y*=y;
//             }
//         }
//     }
//     flagShapeChanged();
// }
// Suggest by Github Copilot - GPT 4o
void pesPath::scale(float x, float y) {
    if (mode == COMMANDS) {
        for (auto& command : commands) {
            command.to.x *= x;
            command.to.y *= y;
            if (command.type == Command::Type::_bezierTo || command.type == Command::Type::_quadBezierTo) {
                command.cp1.x *= x;
                command.cp1.y *= y;
                command.cp2.x *= x;
                command.cp2.y *= y;
            }
            if (command.type == Command::Type::_arc || command.type == Command::Type::_arcNegative) {
                command.radiusX *= x;
                command.radiusY *= y;
            }
        }
    } else {
        for (auto& polyline : polylines) {
            for (auto& point : polyline) {
                point.x *= x;
                point.y *= y;
            }
        }
    }
    flagShapeChanged();
}

// void pesPath::translate(float x, float y){
//     pesVec2f p(x,y);
//     if(mode==COMMANDS){
//         for(int j=0;j<(int)commands.size();j++){
//             commands[j].to += p;
//             if(commands[j].type==Command::Type::_bezierTo || commands[j].type==Command::Type::_quadBezierTo){
//                 commands[j].cp1 += p;
//                 commands[j].cp2 += p;
//             }
//         }
//     }else{
//         for(int i=0;i<(int)polylines.size();i++){
//             for(int j=0;j<(int)polylines[i].size();j++){
//                 polylines[i][j] += p;
//             }
//         }
//     }
//     flagShapeChanged();
// }
// Suggest by Github Copilot - GPT 4o
void pesPath::translate(float x, float y) {
    pesVec2f p(x, y);
    if (mode == COMMANDS) {
        for (auto& command : commands) {
            command.to += p;
            if (command.type == Command::Type::_bezierTo || command.type == Command::Type::_quadBezierTo) {
                command.cp1 += p;
                command.cp2 += p;
            }
        }
    } else {
        for (auto& polyline : polylines) {
            for (auto& point : polyline) {
                point += p;
            }
        }
    }
    flagShapeChanged();
}

void pesPath::flagShapeChanged(){
    if(mode==COMMANDS){
        bHasChanged = true;
        bNeedsPolylinesGeneration = true;
    }
}

bool pesPath::hasChanged(){
    bool changed = bHasChanged;
    bHasChanged = false;
    return changed;
}

void pesPath::newSubPath(){
    if(mode==COMMANDS){
    }else{
        polylines.push_back(pesPolyline());
    }
}

vector<pesPath> pesPath::getSubPath() const {
    vector<pesPath> subPath;
    pesPath p;
    p.fillRule = fillRule;
    //for(int i=0;i<(int)commands.size();i++){
    //    if(commands[i].type==pesPath::Command::Type::_moveTo && i>1 && commands[i-1].type!=pesPath::Command::Type::_close){
    //        p.flagShapeChanged();
    //        subPath.push_back(p);
    //        p.clear();
    //    }
    //    
    //    p.addCommand(commands[i]);
    //    
    //    if(commands[i].type==pesPath::Command::Type::_close){
    //        p.flagShapeChanged();
    //        subPath.push_back(p);
    //        p.clear();
    //    }
    //}

    for (const auto& command : commands) {
        if (command.type == Command::Type::_moveTo) {
            if (p.getCommands().size()) {
                p.flagShapeChanged();
                subPath.push_back(p);
                p.clear();
            }
        }

        p.addCommand(command);
    }

    if (p.getCommands().size()) {
        p.flagShapeChanged();
        subPath.push_back(p);
        p.clear();
    }

    return subPath;
}

void pesPath::setFilled(bool hasFill){
    if(bFill != hasFill){
        bFill = hasFill;
    }
}

bool pesPath::isFill(){
    return bFill;
}

void pesPath::setStrokeWidth(float width){
    strokeWidth = width;
}

void pesPath::setColor( const pesColor& color ){
    setFillColor(color);
    setStrokeColor(color);
}

void pesPath::setHexColor( int hex ){
    int alpha = ((hex & 0xFF000000) >> 24);
    if( alpha == 0 ) {
        alpha = 0xFF;
    }
    pesColor color;
    color.setHex(hex, alpha);
    setColor(color);
}

void pesPath::setFillColor(const pesColor & color){
    setUseShapeColor(true);
    fillColor = color;
}

void pesPath::setFillHexColor(int hex){
    int alpha = ((hex & 0xFF000000) >> 24);
    if( alpha == 0 ) {
        alpha = 0xFF;
    }
    pesColor color;
    color.setHex(hex, alpha);
    setFillColor(color);
}

void pesPath::setFillHexARGBColor(uint32_t hex) {
    setUseShapeColor(true);
    fillColor.setHexARGB(hex);
}

void pesPath::setStrokeColor(const pesColor & color){
    setUseShapeColor(true);
    strokeColor = color;
}

void pesPath::setStrokeHexColor( int hex ){
    pesColor color;
    color.setHex(hex);
    setStrokeColor(color);
}

void pesPath::setStrokeHexARGBColor(uint32_t hex) {
    setUseShapeColor(true);
    strokeColor.setHexARGB(hex);
}

void pesPath::setUseShapeColor(bool useColor) {
    bUseShapeColor = useColor;
}

bool pesPath::getUseShapeColor() const{
    return bUseShapeColor;
}

pesColor pesPath::getFillColor() const{
    return fillColor; }

int pesPath::getFillHexColor() const { 
    return fillColor.getHex(); 
}

uint32_t pesPath::getFillHexARGBColor() const { 
    return fillColor.getHexARGB(); 
}

pesColor pesPath::getStrokeColor() const{
    return strokeColor; }

int pesPath::getStrokeHexColor() const { 
    return strokeColor.getHex(); 
}

uint32_t pesPath::getStrokeHexARGBColor() const { 
    return strokeColor.getHexARGB(); 
}

float pesPath::getStrokeWidth() const{
    return strokeWidth;
}

pesPolyline & pesPath::lastPolyline(){
    if(polylines.empty() || polylines.back().isClosed()){
        polylines.push_back(pesPolyline());
    }
    return polylines.back();
}


void pesPath::setMode(pesPath::Mode mode){
    this->mode = mode;
}

pesPath::Mode pesPath::getMode(){
    return mode;
}

vector<pesPath::Command> & pesPath::getCommands(){
    if(mode==POLYLINES){
    }else{
        flagShapeChanged();
    }
    return commands;
}

const vector<pesPath::Command> & pesPath::getCommands() const{
    return commands;
}

const vector<pesPolyline> & pesPath::getOutline() const{
    const_cast<pesPath*>(this)->generatePolylinesFromCommands();
    return polylines;
}

bool pesPath::inside(float x, float y) const{
    const vector<pesPolyline> & outlines = getOutline();
    int size = (int)outlines.size();
    if(size){
        pesPolyline poly;
        for(int i=0;i<size;i++){
            poly.addVertices(outlines[i].getVertices());
            if(outlines[i].isClosed()){
                poly.addVertex(outlines[i][0]);
            }
        }
        return poly.inside(x, y);
    }
    return false;
}

void pesPath::addCommand(const Command & command){
    if((commands.empty() || commands.back().type==Command::Type::_close) && command.type!=Command::Type::_moveTo){
        commands.push_back(Command(Command::Type::_moveTo, command.to));
    }
    commands.push_back(command);
}

// void pesPath::rotate(float degree){
//     if(mode==COMMANDS){
//         for(int j=0;j<(int)commands.size();j++){
// //            commands[j].to.rotate(degree);
//             commands[j].to.rotate(degree, pesVec3f(0,0,1));
            
//             if(commands[j].type==Command::Type::_bezierTo || commands[j].type==Command::Type::_quadBezierTo){
// //                commands[j].cp1.rotate(degree);
// //                commands[j].cp2.rotate(degree);
//                 commands[j].cp1.rotate(degree, pesVec3f(0,0,1));
//                 commands[j].cp2.rotate(degree, pesVec3f(0,0,1));
//             }
//             if(commands[j].type==Command::Type::_arc || commands[j].type==Command::Type::_arcNegative){
//                 commands[j].angleBegin += degree;
//                 commands[j].angleEnd += degree;
//             }
//         }
//     }else{
//         for(int i=0;i<(int)polylines.size();i++){
//             for(int j=0;j<(int)polylines[i].size();j++){
//                 polylines[i][j].rotate(degree);
//             }
//         }
//     }
//     flagShapeChanged();
// }
// GPT
void pesPath::rotate(float degree) {
    pesVec3f axis(0, 0, 1);
    if (mode == COMMANDS) {
        for (auto& command : commands) {
            command.to.rotate(degree, axis);
            if (command.type == Command::Type::_bezierTo || command.type == Command::Type::_quadBezierTo) {
                command.cp1.rotate(degree, axis);
                command.cp2.rotate(degree, axis);
            }
            if (command.type == Command::Type::_arc || command.type == Command::Type::_arcNegative) {
                command.angleBegin += degree;
                command.angleEnd += degree;
            }
        }
    } else {
        for (auto& polyline : polylines) {
            for (auto& point : polyline) {
                point.rotate(degree);
            }
        }
    }
    flagShapeChanged();
}

// void pesPath::rotateAround(float degree, const pesVec2f& pivot){
//     pesVec3f axis(0,0,1);
//     if(mode==COMMANDS){
//         for(int j=0;j<(int)commands.size();j++){
//              commands[j].to.rotate(degree, pivot, axis);
            
//             if(commands[j].type==Command::Type::_bezierTo || commands[j].type==Command::Type::_quadBezierTo){
//                  commands[j].cp1.rotate(degree,pivot, axis);
//                  commands[j].cp2.rotate(degree,pivot, axis);
//             }
//             if(commands[j].type==Command::Type::_arc || commands[j].type==Command::Type::_arcNegative){
//                 commands[j].angleBegin += degree;
//                 commands[j].angleEnd += degree;
//             }
//         }
//     }else{
//         for(int i=0;i<(int)polylines.size();i++){
//             for(int j=0;j<(int)polylines[i].size();j++){
//                 polylines[i][j].rotate(degree,pivot);
//             }
//         }
//     }
//     flagShapeChanged();
// }
// GPT
void pesPath::rotateAround(float degree, const pesVec2f& pivot) {
    pesVec3f axis(0, 0, 1);
    if (mode == COMMANDS) {
        for (auto& command : commands) {
            command.to.rotate(degree, pivot, axis);
            if (command.type == Command::Type::_bezierTo || command.type == Command::Type::_quadBezierTo) {
                command.cp1.rotate(degree, pivot, axis);
                command.cp2.rotate(degree, pivot, axis);
            }
            if (command.type == Command::Type::_arc || command.type == Command::Type::_arcNegative) {
                command.angleBegin += degree;
                command.angleEnd += degree;
            }
        }
    } else {
        for (auto& polyline : polylines) {
            for (auto& point : polyline) {
                point.rotate(degree, pivot);
            }
        }
    }
    flagShapeChanged();
}

pesRectangle pesPath::getBoundingBox() const{
    pesRectangle bound;
    if(getOutline().size()==0)
        return pesRectangle(0, 0, 0, 0);
    else{
        bound.set(getOutline()[0].getBoundingBox());
        for(auto & o:getOutline()){
            bound.growToInclude(o.getBoundingBox());
        }
    }
    
    return bound;
}

void pesPath::generatePolylinesFromCommands(){
    if(mode==POLYLINES || commands.empty()) return;
    
    if(bNeedsPolylinesGeneration || curveResolution!=prevCurveRes){
        prevCurveRes = curveResolution;
        
        polylines.clear();
        int j=-1;
        
        for(int i=0; i<(int)commands.size();i++){
            switch(commands[i].type){
                case Command::Type::_moveTo:
                    polylines.push_back(pesPolyline());
                    j++;
                    polylines[j].addVertex(commands[i].to);
                    break;
                case Command::Type::_lineTo:
                    polylines[j].addVertex(commands[i].to);
                    break;
                case Command::Type::_curveTo:
                    polylines[j].curveTo(commands[i].to.x, commands[i].to.y, curveResolution);
                    break;
                case Command::Type::_bezierTo:
                    polylines[j].bezierTo(commands[i].cp1,commands[i].cp2,commands[i].to, curveResolution);
                    break;
                case Command::Type::_quadBezierTo:
                    polylines[j].quadBezierTo(commands[i].cp1,commands[i].cp2,commands[i].to, curveResolution);
                    break;
                case Command::Type::_arc:
                    polylines[j].arc(commands[i].to,commands[i].radiusX,commands[i].radiusY,commands[i].angleBegin,commands[i].angleEnd, circleResolution);
                    break;
                case Command::Type::_arcNegative:
                    polylines[j].arcNegative(commands[i].to,commands[i].radiusX,commands[i].radiusY,commands[i].angleBegin,commands[i].angleEnd, circleResolution);
                    break;
                case Command::Type::_close:
                    polylines[j].setClosed(true);
                    break;
            }
        }
        
        bNeedsPolylinesGeneration = false;
    }
}

void pesPath::setCurveResolution(int _curveResolution){
    curveResolution = _curveResolution;
}

int pesPath::getCurveResolution() const{
    return curveResolution;
}

void pesPath::setCircleResolution(int _circleResolution){
    circleResolution = _circleResolution;
}

int pesPath::getCircleResolution() const{
    return circleResolution;
}
