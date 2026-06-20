//
//  pesCubicSuperPath.cpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 1/10/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#include "pesCubicSuperPath.hpp"

using namespace std;

class pesBezierSpline{
public:
    static void getCurveControlPoints(const vector<pesVec2f> &knots, vector<pesVec2f> &firstControlPoints, vector<pesVec2f> &secondControlPoints);
    
private:
    static vector<float> getFirstControlPoints(const vector<float> &rhs);
};


// MARK: - pesCubicSuperPath
pesCubicSuperPath::pesCubicSuperPath(){
    path.clear();
    knots.clear();
}

pesCubicSuperPath::~pesCubicSuperPath(){
    path.clear();
    knots.clear();
}

void pesCubicSuperPath::reset(){
    path.clear();
    knots.clear();
}

void pesCubicSuperPath::addCurvePoint(const pesVec2f & p){
    knots.emplace_back(KnotType::KNOT_TYPE_CURVE, p);
}

void pesCubicSuperPath::addCornerPoint(const pesVec2f & p){
    knots.emplace_back(KnotType::KNOT_TYPE_CORNER, p);
}

void pesCubicSuperPath::removeLastPoint(){
    if(knots.size()>0){
        knots.erase(knots.end()-1);
    }
}

void pesCubicSuperPath::calculateCSP(){
    vector<pesVec2f> k;
    int num = (int)knots.size();
    path.clear();
    for(int i=0; i<num; i++){
        const Knot knot = knots[i];
        if(i==0){
            path.moveTo(knot.p.x, knot.p.y);
            k.push_back(knot.p);
        }
        else
        {
            if(knot.type==KnotType::KNOT_TYPE_CORNER){
                k.push_back(knot.p);
                if(k.size()>1){
                    vector<pesVec2f> firstCP, secondCP;
                    pesBezierSpline::getCurveControlPoints(k, firstCP, secondCP);
                    for(int j=1; j<(int)k.size(); j++){
                        path.bezierTo(firstCP[j-1], secondCP[j-1], k[j]);
                    }
                    k.clear();
                    k.push_back(knot.p);
                }
            }
            else{
                k.push_back(knot.p);
            }
        }
    }
    
    if(k.size()>1){
        vector<pesVec2f> firstCP, secondCP;
        pesBezierSpline::getCurveControlPoints(k, firstCP, secondCP);
        for(int j=1; j<(int)k.size(); j++){
            path.bezierTo(firstCP[j-1], secondCP[j-1], k[j]);
        }
    }
    
//    SkDebugf("knotSize=%i, pathCommandSize=%i\n", knots.size(), path.getCommands().size());
//    std::cout << "knotSize=" << knots.size() << " ,pathCommandSize=" << path.getCommands().size() << std::endl;
    if(knots.size()==path.getCommands().size()){
        vector<pesPath::Command> & commands = path.getCommands();
        for(int i=0; i<commands.size(); i++){
//            if(i>0 && knots[i].type==KNOT_TYPE_CORNER && knots[i-1].type==KNOT_TYPE_CORNER && commands[i].type==pesPath::Command::_bezierTo){
            if(i>0 && knots[i].type==KNOT_TYPE_CORNER && commands[i].type==pesPath::Command::_bezierTo){
                commands[i].type = pesPath::Command::_lineTo;
            }
        }
    }
}


// MARK: - pesBezierSpline (Interpolate BezierSpline)
void pesBezierSpline::getCurveControlPoints(const vector<pesVec2f> &knots,
                                         vector<pesVec2f> &firstControlPoints,
                                         vector<pesVec2f> &secondControlPoints)
{
    int n = static_cast<int>(knots.size()) - 1;
    if (n < 1){
        std::cerr << "pesBezierSpline::getCurveControlPoints()" << "knots size<1" << std::endl;
        return;
    }
    
    if (n == 1)
    { // Special case: Bezier curve should be a straight line.
        firstControlPoints.resize(1);
        // 3P1 = 2P0 + P3
        firstControlPoints[0].x = (2 * knots[0].x + knots[1].x) / 3;
        firstControlPoints[0].y = (2 * knots[0].y + knots[1].y) / 3;
        
        secondControlPoints.resize(1);
        // P2 = 2P1 – P0
        secondControlPoints[0].x = 2 * firstControlPoints[0].x - knots[0].x;
        secondControlPoints[0].y = 2 * firstControlPoints[0].y - knots[0].y;
        return;
    }
    
    // Calculate first Bezier control points
    // Right hand side vector
    // vector<float> rhs; rhs.resize(n);
    vector<float> rhs(n);
    
    // Set right hand side X values
    for (int i = 1; i < n - 1; ++i)
        rhs[i] = 4 * knots[i].x + 2 * knots[i + 1].x;
    rhs[0] = knots[0].x + 2 * knots[1].x;
    rhs[n - 1] = (8 * knots[n - 1].x + knots[n].x) / 2.0f;
    // Get first control points X-values
    vector<float> x = getFirstControlPoints(rhs);
    
    // Set right hand side Y values
    for (int i = 1; i < n - 1; ++i)
        rhs[i] = 4 * knots[i].y + 2 * knots[i + 1].y;
    rhs[0] = knots[0].y + 2 * knots[1].y;
    rhs[n - 1] = (8 * knots[n - 1].y + knots[n].y) / 2.0f;
    // Get first control points Y-values
    vector<float> y = getFirstControlPoints(rhs);
    
    // Fill output vectors
    firstControlPoints.resize(n);
    secondControlPoints.resize(n);
    for (int i = 0; i < n; ++i)
    {
        // First control point
        firstControlPoints[i] = pesVec2f(x[i], y[i]);
        // Second control point
        if (i < n - 1)
            secondControlPoints[i] = pesVec2f(2 * knots[i + 1].x - x[i + 1], 2 * knots[i + 1].y - y[i + 1]);
        else
            secondControlPoints[i] = pesVec2f((knots[n].x + x[n - 1]) / 2, (knots[n].y + y[n - 1]) / 2);
    }
}

vector<float> pesBezierSpline::getFirstControlPoints(const vector<float> &rhs){
    int n = static_cast<int>(rhs.size());
    // vector<float> x; x.resize(n);
    // vector<float> tmp; tmp.resize(n);
    vector<float> x(n);
    vector<float> tmp(n);
    
    float b = 2.0;
    
    x[0] = rhs[0] / b;
    for (int i = 1; i < n; i++) // Decomposition and forward substitution.
    {
        tmp[i] = 1 / b;
        b = (i < n - 1 ? 4.0f : 3.5f) - tmp[i];
        x[i] = (rhs[i] - x[i - 1]) / b;
    }
    for (int i = 1; i < n; i++)
        x[n - i - 1] -= tmp[n - i] * x[n - i]; // Back substitution.
    
    return x;
}
