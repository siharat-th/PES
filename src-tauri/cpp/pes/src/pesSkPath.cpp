//
//  pesSkPath.cpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 2/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#include "pesSkPath.hpp"

pesSkPath::Command::Command(Type type)
:type(type)
{
}

//----------------------------------------------------------
pesSkPath::Command::Command(Type type , const pesVec3f & p)
:type(type)
,to(p)
{
}

//----------------------------------------------------------
pesSkPath::Command::Command(Type type , const pesVec3f & p, const pesVec3f & cp1, const pesVec3f & cp2)
:type(type)
,to(p)
,cp1(cp1)
,cp2(cp2)
{
}

//----------------------------------------------------------
pesSkPath::Command::Command(Type type , const pesVec3f & centre, float radiusX, float radiusY, float angleBegin, float angleEnd)
:type(type)
,to(centre)
,radiusX(radiusX)
,radiusY(radiusY)
,angleBegin(angleBegin)
,angleEnd(angleEnd)
{
}

pesSkPath::pesSkPath(){
    strokeWidth = 0;
    bFill = false;//true;
    prevCurveRes = 20;
    curveResolution = 20;
    circleResolution = 20;
    mode = COMMANDS;
    bHasChanged = false;
    bUseShapeColor = true;
    bNeedsPolylinesGeneration = false;
    clear();
}

void pesSkPath::append(const pesSkPath & path){
//    if(mode==COMMANDS){
//        for(auto & command: path.getCommands()){
//            addCommand(command);
//        }
//    }else{
//        for(auto & poly: path.getOutline()){
//            polylines.push_back(poly);
//        }
//    }
//    flagShapeChanged();
    
//    skpath.addPath(path,SkScalar(0), SkScalar(0));
}

void pesSkPath::clear(){
//    commands.clear();
    polylines.resize(1);
    polylines[0].clear();
//    flagShapeChanged();
    skpath.reset();
}

void pesSkPath::close(){
//    if(mode==COMMANDS){
//        addCommand(Command(Command::close));
//    }else{
//        lastPolyline().setClosed(true);
//    }
//    flagShapeChanged();
    skpath.close();
}

void pesSkPath::lineTo(float x, float y){
//    pesVec2f p(x,y);
//    if(mode==COMMANDS){
//        addCommand(Command(Command::lineTo,p));
//    }else{
//        lastPolyline().lineTo(x, y);
//    }
//    flagShapeChanged();
    skpath.lineTo(x, y);
}

void pesSkPath::moveTo(float x, float y){
//    pesVec2f p(x,y);
//    if(mode==COMMANDS){
//        addCommand(Command(Command::moveTo, p));
//    }else{
//        if(lastPolyline().size()>0) newSubPath();
//        lastPolyline().addVertex(p);
//    }
//    flagShapeChanged();
    skpath.moveTo(x, y);
}

//void pesSkPath::curveTo(float x, float y){
//    pesVec2f p(x,y);
//    if(mode==COMMANDS){
//        addCommand(Command(Command::curveTo, p));
//    }else{
//        lastPolyline().curveTo(p, curveResolution);
//    }
//    flagShapeChanged();
//}

void pesSkPath::bezierTo(const pesVec2f & cp1, const pesVec2f & cp2, const pesVec2f & p){
//    if(mode==COMMANDS){
//        addCommand(Command(Command::bezierTo,p,cp1,cp2));
//    }else{
//        lastPolyline().bezierTo(cp1,cp2,p,curveResolution);
//    }
//    flagShapeChanged();
    skpath.cubicTo(cp1.x, cp1.y, cp2.x, cp2.y, p.x, p.y);
}

void pesSkPath::quadBezierTo(const pesVec2f & cp1, const pesVec2f & cp2, const pesVec2f & p){
//    if(mode==COMMANDS){
//        addCommand(Command(Command::quadBezierTo,p,cp1,cp2));
//    }else{
//        lastPolyline().quadBezierTo(cp1,cp2,p,curveResolution);
//    }
//    flagShapeChanged();
    skpath.quadTo(cp2.x, cp2.y, p.x, p.y);
}

void pesSkPath::arc(const pesVec2f & centre, float radiusX, float radiusY, float angleBegin, float angleEnd){
//    if(mode==COMMANDS){
//        addCommand(Command(Command::arc,centre,radiusX,radiusY,angleBegin,angleEnd));
//    }else{
//        lastPolyline().arc(centre,radiusX,radiusY,angleBegin,angleEnd,circleResolution);
//    }
//    flagShapeChanged();
    
}

void pesSkPath::arcNegative(const pesVec2f & centre, float radiusX, float radiusY, float angleBegin, float angleEnd){
//    if(mode==COMMANDS){
//        addCommand(Command(Command::arcNegative,centre,radiusX,radiusY,angleBegin,angleEnd));
//    }else{
//        lastPolyline().arcNegative(centre,radiusX,radiusY,angleBegin,angleEnd,circleResolution);
//    }
//    flagShapeChanged();
}

void pesSkPath::circle(float x, float y, float radius){
//    moveTo(x+radius, y);
//    arc(pesVec2f(x, y), radius, radius, 0, 360);
    skpath.Circle(x, y, radius);
}

void pesSkPath::circle(const pesVec2f & p, float radius){
    circle(p.x,p.y,radius);
}


void pesSkPath::rectangle(const pesRectangle & r){
    rectangle(r.x, r.y, r.width, r.height);
}

void pesSkPath::rectangle(const pesVec2f & p,float w,float h){
    rectangle(p.x, p.y, w, h);
}

void pesSkPath::rectangle(float x,float y,float w,float h){
//    moveTo(x,y);
//    lineTo(x+w,y);
//    lineTo(x+w,y+h);
//    lineTo(x,y+h);
//    close();
    skpath.addRect(x, y, x+w, y+h);
}

void pesSkPath::simplify(float tolerance){
//    if(mode==COMMANDS) generatePolylinesFromCommands();
//    for(int i=0;i<(int)polylines.size();i++){
//        polylines[i].simplify(tolerance);
//    }
}

void pesSkPath::scale(float x, float y){
//    if(mode==COMMANDS){
//        for(int j=0;j<(int)commands.size();j++){
//            commands[j].to.x*=x;
//            commands[j].to.y*=y;
//            if(commands[j].type==Command::bezierTo || commands[j].type==Command::quadBezierTo){
//                commands[j].cp1.x*=x;
//                commands[j].cp1.y*=y;
//                commands[j].cp2.x*=x;
//                commands[j].cp2.y*=y;
//            }
//            if(commands[j].type==Command::arc || commands[j].type==Command::arcNegative){
//                commands[j].radiusX *= x;
//                commands[j].radiusY *= y;
//            }
//        }
//    }else{
//        for(int i=0;i<(int)polylines.size();i++){
//            for(int j=0;j<(int)polylines[i].size();j++){
//                polylines[i][j].x*=x;
//                polylines[i][j].y*=y;
//            }
//        }
//    }
//    flagShapeChanged();
    skpath.makeScale(x, y);
}

void pesSkPath::translate(float x, float y){
//    pesVec2f p(x,y);
//    if(mode==COMMANDS){
//        for(int j=0;j<(int)commands.size();j++){
//            commands[j].to += p;
//            if(commands[j].type==Command::bezierTo || commands[j].type==Command::quadBezierTo){
//                commands[j].cp1 += p;
//                commands[j].cp2 += p;
//            }
//        }
//    }else{
//        for(int i=0;i<(int)polylines.size();i++){
//            for(int j=0;j<(int)polylines[i].size();j++){
//                polylines[i][j] += p;
//            }
//        }
//    }
//    flagShapeChanged();
    
    SkMatrix m;
    m.setTranslate(x, y);
    skpath.makeTransform(m);
}

void pesSkPath::flagShapeChanged(){
//    if(mode==COMMANDS){
//        bHasChanged = true;
//        bNeedsPolylinesGeneration = true;
//    }
////    else{
//////        bNeedsTessellation = true;
////    }
}

bool pesSkPath::hasChanged(){
    bool changed = bHasChanged;
    bHasChanged = false;
    return changed;
}

//void pesSkPath::newSubPath(){
//    if(mode==COMMANDS){
//    }else{
//        polylines.push_back(pesPolyline());
//    }
//}

//vector<pesSkPath> pesSkPath::getSubPath(){
//    vector<pesSkPath> subPath;
//    pesSkPath p;
//    for(int i=0;i<(int)commands.size();i++){
//        if(commands[i].type==pesSkPath::Command::moveTo && i>1 && commands[i-1].type!=pesSkPath::Command::close){
//            p.flagShapeChanged();
//            subPath.push_back(p);
//            p.clear();
//        }
//        
//        p.addCommand(commands[i]);
//        
//        if(commands[i].type==pesSkPath::Command::close){
//            p.flagShapeChanged();
//            subPath.push_back(p);
//            p.clear();
//        }
//    }
//    if(p.getCommands().size()){
//        p.flagShapeChanged();
//        subPath.push_back(p);
//        p.clear();
//    }
//    return subPath;
//}

void pesSkPath::setFilled(bool hasFill){
    if(bFill != hasFill){
        bFill = hasFill;
    }
}

bool pesSkPath::isFill(){
    return bFill;
}

void pesSkPath::setStrokeWidth(float width){
    strokeWidth = width;
}

void pesSkPath::setColor( const pesColor& color ){
    setFillColor(color);
    setStrokeColor(color);
}

void pesSkPath::setHexColor( int hex ){
    int alpha = ((hex & 0xFF000000) >> 24);
    if( alpha == 0 ) {
        alpha = 0xFF;
    }
    pesColor color;
    color.setHex(hex, alpha);
    setColor(color);
}

void pesSkPath::setFillColor(const pesColor & color){
    setUseShapeColor(true);
    fillColor = color;
}

void pesSkPath::setFillHexColor(int hex){
    int alpha = ((hex & 0xFF000000) >> 24);
    if( alpha == 0 ) {
        alpha = 0xFF;
    }    
    pesColor color;
    color.setHex(hex, alpha);
    setFillColor(color);
}

void pesSkPath::setStrokeColor(const pesColor & color){
    setUseShapeColor(true);
    strokeColor = color;
}

void pesSkPath::setStrokeHexColor( int hex ){
    pesColor color;
    color.setHex(hex);
    setStrokeColor(color);
}

void pesSkPath::setUseShapeColor(bool useColor){
    bUseShapeColor = useColor;
}

bool pesSkPath::getUseShapeColor() const{
    return bUseShapeColor;
}

pesColor pesSkPath::getFillColor() const{
    return fillColor;
}

pesColor pesSkPath::getStrokeColor() const{
    return strokeColor;
}

float pesSkPath::getStrokeWidth() const{
    return strokeWidth;
}

//pesPolyline & pesSkPath::lastPolyline(){
//    if(polylines.empty() || polylines.back().isClosed()){
//        polylines.push_back(pesPolyline());
//    }
//    return polylines.back();
//}


void pesSkPath::setMode(pesSkPath::Mode mode){
    this->mode = mode;
}

pesSkPath::Mode pesSkPath::getMode(){
    return mode;
}

//vector<pesSkPath::Command> & pesSkPath::getCommands(){
//    if(mode==POLYLINES){
//    }else{
//        flagShapeChanged();
//    }
//    return commands;
//}
//
//const vector<pesSkPath::Command> & pesSkPath::getCommands() const{
//    return commands;
//}
//
//const vector<pesPolyline> & pesSkPath::getOutline() const{
//    const_cast<pesSkPath*>(this)->generatePolylinesFromCommands();
//    return polylines;
//}
//
//bool pesSkPath::inside(float x, float y) const{
//    const vector<pesPolyline> & outlines = getOutline();
//    int size = (int)outlines.size();
//    if(size){
//        pesPolyline poly;
//        for(int i=0;i<size;i++){
//            poly.addVertices(outlines[i].getVertices());
//            if(outlines[i].isClosed()){
//                poly.addVertex(outlines[i][0]);
//            }
//        }
//        return poly.inside(x, y);
//    }
//    return false;
//}
//
//void pesSkPath::addCommand(const Command & command){
//    if((commands.empty() || commands.back().type==Command::close) && command.type!=Command::moveTo){
//        commands.push_back(Command(Command::moveTo, command.to));
//    }
//    commands.push_back(command);
//}
//
//void pesSkPath::rotate(float degree){
//    if(mode==COMMANDS){
//        for(int j=0;j<(int)commands.size();j++){
////            commands[j].to.rotate(degree);
//            commands[j].to.rotate(degree, pesVec3f(0,0,1));
//            
//            if(commands[j].type==Command::bezierTo || commands[j].type==Command::quadBezierTo){
////                commands[j].cp1.rotate(degree);
////                commands[j].cp2.rotate(degree);
//                commands[j].cp1.rotate(degree, pesVec3f(0,0,1));
//                commands[j].cp2.rotate(degree, pesVec3f(0,0,1));
//            }
//            if(commands[j].type==Command::arc || commands[j].type==Command::arcNegative){
//                commands[j].angleBegin += degree;
//                commands[j].angleEnd += degree;
//            }
//        }
//    }else{
//        for(int i=0;i<(int)polylines.size();i++){
//            for(int j=0;j<(int)polylines[i].size();j++){
//                polylines[i][j].rotate(degree);
//            }
//        }
//    }
//    flagShapeChanged();
//}
//
//void pesSkPath::rotateAround(float degree, const pesVec2f& pivot){
//    pesVec3f axis(0,0,1);
//    if(mode==COMMANDS){
//        for(int j=0;j<(int)commands.size();j++){
//             commands[j].to.rotate(degree, pivot, axis);
//            
//            if(commands[j].type==Command::bezierTo || commands[j].type==Command::quadBezierTo){
//                 commands[j].cp1.rotate(degree,pivot, axis);
//                 commands[j].cp2.rotate(degree,pivot, axis);
//            }
//            if(commands[j].type==Command::arc || commands[j].type==Command::arcNegative){
//                commands[j].angleBegin += degree;
//                commands[j].angleEnd += degree;
//            }
//        }
//    }else{
//        for(int i=0;i<(int)polylines.size();i++){
//            for(int j=0;j<(int)polylines[i].size();j++){
//                polylines[i][j].rotate(degree,pivot);
//            }
//        }
//    }
//    flagShapeChanged();
//}

//pesRectangle pesSkPath::getBoundingBox() const{
////    pesRectangle bound(0, 0, 0, 0);
//    pesRectangle bound;
//    bool first=true;
//    for(auto & o:getOutline()){
//        if(first){
//            bound.set(o.getBoundingBox());
//            first = false;
//        }
//        else{
//            bound.growToInclude(o.getBoundingBox());
//        }
////        if(bound.isEmpty()){
////            bound.set(o.getBoundingBox());
////        }
////        else{
////            bound.growToInclude(o.getBoundingBox());
////        }
//    }
//    return bound;
//}

//void pesSkPath::generatePolylinesFromCommands(){
////    if(mode==POLYLINES || commands.empty()) return;
////
////    if(bNeedsPolylinesGeneration || curveResolution!=prevCurveRes){
////        prevCurveRes = curveResolution;
////
////        polylines.clear();
////
////        int j=-1;
////        for(int i=0; i<(int)commands.size();i++){
////            switch(commands[i].type){
////                case Command::moveTo:
////                    polylines.push_back(pesPolyline());
////                    j++;
////                    polylines[j].addVertex(commands[i].to);
////                    break;
////                case Command::lineTo:
////                    polylines[j].addVertex(commands[i].to);
////                    break;
////                case Command::curveTo:
////                    polylines[j].curveTo(commands[i].to.x, commands[i].to.y, curveResolution);
////                    break;
////                case Command::bezierTo:
////                    polylines[j].bezierTo(commands[i].cp1,commands[i].cp2,commands[i].to, curveResolution);
////                    break;
////                case Command::quadBezierTo:
////                    polylines[j].quadBezierTo(commands[i].cp1,commands[i].cp2,commands[i].to, curveResolution);
////                    break;
////                case Command::arc:
////                    polylines[j].arc(commands[i].to,commands[i].radiusX,commands[i].radiusY,commands[i].angleBegin,commands[i].angleEnd, circleResolution);
////                    break;
////                case Command::arcNegative:
////                    polylines[j].arcNegative(commands[i].to,commands[i].radiusX,commands[i].radiusY,commands[i].angleBegin,commands[i].angleEnd, circleResolution);
////                    break;
////                case Command::close:
////                    polylines[j].setClosed(true);
////                    break;
////            }
////        }
////
////        bNeedsPolylinesGeneration = false;
////    }
//}

//void pesSkPath::setCurveResolution(int _curveResolution){
//    curveResolution = _curveResolution;
//}
//
//int pesSkPath::getCurveResolution() const{
//    return curveResolution;
//}
//
//void pesSkPath::setCircleResolution(int _circleResolution){
//    circleResolution = _circleResolution;
//}
//
//int pesSkPath::getCircleResolution() const{
//    return circleResolution;
//}
