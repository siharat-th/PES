//
//  pesSatinOutline.cpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 3/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#include "pesSatinOutline.hpp"

using namespace std;

pesVec2f lineIntersection(pesVec2f p1, pesVec2f d1, pesVec2f p2, pesVec2f d2);

void pesSatinOutlineGenerate(pesPolyline &contour, float zigzagWidth, float inset, pesSatinOutline& result){
#ifndef NDEBUG
    cout << "Org + Mod version" << endl;
#endif
    contour.simplify();
    auto lines = contour.getVertices();
    if(contour.isClosed()){
        if(pesDist(lines.front().x, lines.front().y, lines.back().x, lines.back().y)>0.01){
            lines.push_back(lines.front());
        }
    }
    int numberOfPoints = static_cast<int>(lines.size());
    float halfThickness = zigzagWidth * 0.5f;
    int intermediateOutlineCount = 2 * numberOfPoints - 2;
    pesSatinOutline outline;
    outline.side1.resize(intermediateOutlineCount);
    outline.side2.resize(intermediateOutlineCount);
    
    for (int i = 1; i<numberOfPoints; i++) {
        // Perpendicular normalized vector
        pesVec2f v1 = lines[i] - lines[i - 1];
        v1.perpendicular();
        v1 = v1*-1;
        
        int j = (i - 1) * 2;
        pesVec2f vInset = v1*inset;

        pesVec2f temp = v1 * halfThickness;
        outline.side1[j] = temp + lines[i - 1] + vInset;
        outline.side1[j + 1] = temp + lines[i] + vInset;

        temp = v1 * -halfThickness;
        outline.side2[j] = temp + lines[i - 1] + vInset;
        outline.side2[j + 1] = temp + lines[i] + vInset;
    }
    
    result.side1.resize(intermediateOutlineCount);
    result.side2.resize(intermediateOutlineCount);
    
    result.side1[0] = outline.side1[0];
    result.side2[0] = outline.side2[0];
    
    float miterLimit = sqrt(zigzagWidth*zigzagWidth + zigzagWidth*zigzagWidth);
    
    // miter join
    for (int i = 3; i < intermediateOutlineCount; i += 2) {
        pesVec2f intersect;
        pesVec2f p1Start = outline.side1[i-3];
        pesVec2f p1End   = outline.side1[i-2];
        pesVec2f dir1    = (p1End - p1Start).normalize();
        p1End+=dir1*miterLimit;
        
        pesVec2f p2Start = outline.side1[i];
        pesVec2f p2End   = outline.side1[i-1];
        pesVec2f dir2    = (p2End - p2Start).normalize();
        p2End+=dir2*miterLimit;
        
//        if(pesLineSegmentIntersection(p1Start, p1End, p2Start, p2End, intersect)){
//            result.side1[(i-1)/2] = intersect;
//        }
//        else{
//            result.side1[(i-1)/2] = outline.side1[i-2].getMiddle(outline.side1[i-1]);
//        }
        
        result.side1[(i-1)/2] = lineIntersection(p1Start, dir1, p2Start, dir2);
    }
    // miter join
    for (int i = 3; i < intermediateOutlineCount; i += 2) {
        pesVec2f intersect;
        pesVec2f p1Start = outline.side2[i-3];
        pesVec2f p1End   = outline.side2[i-2];
        pesVec2f dir1    = (p1End - p1Start).normalize();
        p1End+=dir1*miterLimit;
        
        pesVec2f p2Start = outline.side2[i];
        pesVec2f p2End   = outline.side2[i-1];
        pesVec2f dir2    = (p2End - p2Start).normalize();
        p2End+=dir2*miterLimit;
        
//        if(pesLineSegmentIntersection(p1Start, p1End, p2Start, p2End, intersect)){
//            result.side2[(i-1)/2] = intersect;
//        }
//        else{
//            result.side2[(i-1)/2] = outline.side2[i-2].getMiddle(outline.side2[i-1]);
//        }
        
        result.side2[(i-1)/2] = lineIntersection(p1Start, dir1, p2Start, dir2);
    }
    
    if(contour.isClosed() && contour.size()>2){
        // Back substitue(miter join)
        pesVec2f intersect1, intersect2;
        
        pesVec2f p1Start = outline.side1[2 * numberOfPoints - 4];
        pesVec2f p1End   = outline.side1[2 * numberOfPoints - 3];
        pesVec2f dir1    = (p1End - p1Start).normalize();
        p1End+=dir1*miterLimit;
        
        pesVec2f p2Start = outline.side1[1];
        pesVec2f p2End   = outline.side1[0];
        pesVec2f dir2    = (p2End - p2Start).normalize();
        p2End+=dir2*miterLimit;
        
//        if(pesLineSegmentIntersection(p1Start, p1End, p2Start, p2End, intersect1)){
//            result.side1[0] = result.side1[numberOfPoints - 1] = intersect1;
//        }
//        else{
//            result.side1[0] = result.side1[numberOfPoints - 1] = p1End.getMiddle(p2End);
//        }
        result.side1[0] = result.side1[numberOfPoints - 1] = lineIntersection(p1Start, dir1, p2Start, dir2);
        
        p1Start = outline.side2[2 * numberOfPoints - 4];
        p1End   = outline.side2[2 * numberOfPoints - 3];
        dir1    = (p1End - p1Start).normalize();
        p1End+=dir1*miterLimit;
        
        p2Start = outline.side2[1];
        p2End   = outline.side2[0];
        dir2    = (p2End - p2Start).normalize();
        p2End+=dir2*miterLimit;
        
//        if(pesLineSegmentIntersection(p1Start, p1End, p2Start, p2End, intersect2)){
//            result.side2[0] = result.side2[numberOfPoints - 1] = intersect2;
//        }
//        else{
//            result.side2[0] = result.side2[numberOfPoints - 1] = p1End.getMiddle(p2End);
//        }
        result.side2[0] = result.side2[numberOfPoints - 1] = lineIntersection(p1Start, dir1, p2Start, dir2);
    }
    else if(contour.isClosed()==false){
        result.side1[numberOfPoints - 1] = outline.side1[2 * numberOfPoints - 3];
        result.side2[numberOfPoints - 1] = outline.side2[2 * numberOfPoints - 3];
    }
    
    result.length = numberOfPoints;
}

void pesSatinOutlineGenerate(pesPolyline &contour, float zigzagWidth, pesSatinOutline& result){
    contour.simplify();
    auto lines = contour.getVertices();
    if(contour.isClosed()){
        if(pesDist(lines.front().x, lines.front().y, lines.back().x, lines.back().y)>0.01){
            lines.push_back(lines.front());
        }
    }
    int numberOfPoints = static_cast<int>(lines.size());
    float halfThickness = zigzagWidth * 0.5f;
    int intermediateOutlineCount = 2 * numberOfPoints - 2;
    pesSatinOutline outline;
    outline.side1.resize(intermediateOutlineCount);
    outline.side2.resize(intermediateOutlineCount);
    
    for (int i = 1; i<numberOfPoints; i++) {
        // Perpendicular normalized vector
        pesVec2f v1 = lines[i] - lines[i - 1];
        v1.perpendicular();
        v1 = v1*-1;
        
        int j = (i - 1) * 2;
        
        pesVec2f temp = v1 * halfThickness;
        outline.side1[j] = temp + lines[i - 1];
        outline.side1[j + 1] = temp + lines[i];
        
        temp = v1 * -halfThickness;
        outline.side2[j] = temp + lines[i - 1];
        outline.side2[j + 1] = temp + lines[i];
    }
    
    result.side1.resize(intermediateOutlineCount);
    result.side2.resize(intermediateOutlineCount);
    
    result.side1[0] = outline.side1[0];
    result.side2[0] = outline.side2[0];
    
    float miterLimit = sqrt(zigzagWidth*zigzagWidth + zigzagWidth*zigzagWidth);
    
    // miter join
    for (int i = 3; i < intermediateOutlineCount; i += 2) {
        pesVec2f intersect;
        pesVec2f p1Start = outline.side1[i-3];
        pesVec2f p1End   = outline.side1[i-2];
        pesVec2f dir1    = (p1End - p1Start).normalize();
        p1End+=dir1*miterLimit;
        
        pesVec2f p2Start = outline.side1[i];
        pesVec2f p2End   = outline.side1[i-1];
        pesVec2f dir2    = (p2End - p2Start).normalize();
        p2End+=dir2*miterLimit;
        
        if(pesLineSegmentIntersection(p1Start, p1End, p2Start, p2End, intersect)){
            result.side1[(i-1)/2] = intersect;
        }
        else{
            result.side1[(i-1)/2] = outline.side1[i-2].getMiddle(outline.side1[i-1]);
        }
    }
    // miter join
    for (int i = 3; i < intermediateOutlineCount; i += 2) {
        pesVec2f intersect;
        pesVec2f p1Start = outline.side2[i-3];
        pesVec2f p1End   = outline.side2[i-2];
        pesVec2f dir1    = (p1End - p1Start).normalize();
        p1End+=dir1*miterLimit;
        
        pesVec2f p2Start = outline.side2[i];
        pesVec2f p2End   = outline.side2[i-1];
        pesVec2f dir2    = (p2End - p2Start).normalize();
        p2End+=dir2*miterLimit;
        
        if(pesLineSegmentIntersection(p1Start, p1End, p2Start, p2End, intersect)){
            result.side2[(i-1)/2] = intersect;
        }
        else{
            result.side2[(i-1)/2] = outline.side2[i-2].getMiddle(outline.side2[i-1]);
        }
    }
    
    if(contour.isClosed() && contour.size()>2){
        // Back substitue(miter join)
        pesVec2f intersect1, intersect2;
        
        pesVec2f p1Start = outline.side1[2 * numberOfPoints - 4];
        pesVec2f p1End   = outline.side1[2 * numberOfPoints - 3];
        pesVec2f dir1    = (p1End - p1Start).normalize();
        p1End+=dir1*miterLimit;
        
        pesVec2f p2Start = outline.side1[1];
        pesVec2f p2End   = outline.side1[0];
        pesVec2f dir2    = (p2End - p2Start).normalize();
        p2End+=dir2*miterLimit;
        
        if(pesLineSegmentIntersection(p1Start, p1End, p2Start, p2End, intersect1)){
            result.side1[0] = result.side1[numberOfPoints - 1] = intersect1;
        }
        else{
            result.side1[0] = result.side1[numberOfPoints - 1] = p1End.getMiddle(p2End);
        }
        
        p1Start = outline.side2[2 * numberOfPoints - 4];
        p1End   = outline.side2[2 * numberOfPoints - 3];
        dir1    = (p1End - p1Start).normalize();
        p1End+=dir1*miterLimit;
        
        p2Start = outline.side2[1];
        p2End   = outline.side2[0];
        dir2    = (p2End - p2Start).normalize();
        p2End+=dir2*miterLimit;
        
        if(pesLineSegmentIntersection(p1Start, p1End, p2Start, p2End, intersect2)){
            result.side2[0] = result.side2[numberOfPoints - 1] = intersect2;
        }
        else{
            result.side2[0] = result.side2[numberOfPoints - 1] = p1End.getMiddle(p2End);
        }
    }
    else if(contour.isClosed()==false){
        result.side1[numberOfPoints - 1] = outline.side1[2 * numberOfPoints - 3];
        result.side2[numberOfPoints - 1] = outline.side2[2 * numberOfPoints - 3];
    }
    
    result.length = numberOfPoints;
}

pesVec2f lineIntersection(pesVec2f p1, pesVec2f d1, pesVec2f p2, pesVec2f d2) {
    float A1 = d1.y, B1 = -d1.x, C1 = A1 * p1.x + B1 * p1.y;
    float A2 = d2.y, B2 = -d2.x, C2 = A2 * p2.x + B2 * p2.y;

    float det = A1 * B2 - A2 * B1;
//    if (fabs(det) < 1e-6) return p2; // ถ้าเส้นขนาน ให้คืน p2
    if (fabs(det) < 1e-10) return p1.getMiddle(p2);
    
    float x = (C1 * B2 - C2 * B1) / det;
    float y = (A1 * C2 - A2 * C1) / det;
    return pesVec2f( x, y );
}

//void pesSatinOutlineGenerate(pesPolyline &contour, float zigzagWidth, float inset, pesSatinOutline& result){
////    contour.simplify();
////    auto lines = contour.getVertices();
////    if(contour.isClosed()){
////        if(pesDist(lines.front().x, lines.front().y, lines.back().x, lines.back().y)>0.01){
////            lines.push_back(lines.front());
////        }
////    }
////    int numberOfPoints = static_cast<int>(lines.size());
////    float halfThickness = zigzagWidth * 0.5f;
////    int intermediateOutlineCount = 2 * numberOfPoints - 2;
////    pesSatinOutline outline;
////    outline.side1.resize(intermediateOutlineCount);
////    outline.side2.resize(intermediateOutlineCount);
////    
////    for (int i = 1; i<numberOfPoints; i++) {
////        // Perpendicular normalized vector
////        pesVec2f v1 = lines[i] - lines[i - 1];
////        v1.perpendicular();
////        v1 = v1*-1;
////        
////        int j = (i - 1) * 2;
////        pesVec2f vInset = v1*inset;
////
////        pesVec2f temp = v1 * halfThickness;
////        outline.side1[j] = temp + lines[i - 1] + vInset;
////        outline.side1[j + 1] = temp + lines[i] + vInset;
////
////        temp = v1 * -halfThickness;
////        outline.side2[j] = temp + lines[i - 1] + vInset;
////        outline.side2[j + 1] = temp + lines[i] + vInset;
////    }
//    
//    contour.simplify();
//    auto vertices = contour.getVertices();
////    if(contour.isClosed()){
////        if(pesDist(lines.front().x, lines.front().y, lines.back().x, lines.back().y)>0.01){
////            lines.push_back(lines.front());
////        }
////    }
//    int numVertices = static_cast<int>(vertices.size());
//    float halfThickness = zigzagWidth * 0.5f;
//    bool isClosedShape = contour.isClosed() ;
//    int maxPoints = isClosedShape ? numVertices+1 : numVertices;
//    pesVec2f top[maxPoints], bottom[maxPoints];
//    
//    for(int i=0; i<numVertices - (isClosedShape ? 0 : 1); i++){
//        int next = (i+1) % numVertices;
//        pesVec2f d = vertices[next] - vertices[i];
//        pesVec2f n = d.perpendicular().normalize();
//        n.scale(halfThickness);
////        pesVec2f vInset = n*inset;
//        top[i] = vertices[i] + n;
//        bottom[i] = vertices[i] - n;
//    }
//    
//    // Compute intersections to ensure continuous stroke
//    result.length = maxPoints;
//    result.side1.resize(maxPoints);
//    result.side2.resize(maxPoints);
//    for (int i = 0; i < numVertices; i++){
//        if(!isClosedShape && (i==0 || i==numVertices-1)){
//            result.side1[i] = top[i];
//            result.side2[i] = bottom[i];
//            continue;
//        }
//        
//        int prev = (i - 1 + numVertices) % numVertices;
//        int next = (i + 1) % numVertices;
//        
//        result.side1[i] = lineIntersection(top[prev], vertices[i]-vertices[prev], top[next], vertices[next]-vertices[i]);
//        result.side2[i] = lineIntersection(bottom[prev], vertices[i]-vertices[prev], bottom[next], vertices[next]-vertices[i]);
//    }
//    cout << "New Version with inset" << endl;
//}
//
//void pesSatinOutlineGenerate(pesPolyline &contour, float zigzagWidth, pesSatinOutline& result){
//    contour.simplify();
//    auto vertices = contour.getVertices();
////    if(contour.isClosed()){
////        if(pesDist(lines.front().x, lines.front().y, lines.back().x, lines.back().y)>0.01){
////            lines.push_back(lines.front());
////        }
////    }
//    int numVertices = static_cast<int>(vertices.size());
//    float halfThickness = zigzagWidth * 0.5f;
//    bool isClosedShape = contour.isClosed() ;
//    int maxPoints = isClosedShape ? numVertices+1 : numVertices;
//    pesVec2f top[maxPoints], bottom[maxPoints];
//    
//    for(int i=0; i<numVertices - (isClosedShape ? 0 : 1); i++){
//        int next = (i+1) % numVertices;
//        pesVec2f d = vertices[next] - vertices[i];
//        pesVec2f n = d.perpendicular().normalize();
//        n.scale(halfThickness);
//        top[i] = vertices[i] + n;
//        bottom[i] = vertices[i] - n;
//    }
//    
//    // Compute intersections to ensure continuous stroke
//    result.length = maxPoints;
//    result.side1.resize(maxPoints);
//    result.side2.resize(maxPoints);
//    for (int i = 0; i < numVertices; i++){
//        if(!isClosedShape && (i==0 || i==numVertices-1)){
//            continue;
//        }
//        
//        int prev = (i - 1 + numVertices) % numVertices;
//        int next = (i + 1) % numVertices;
//        
//        result.side1[i] = lineIntersection(top[prev], vertices[i]-vertices[prev], top[next], vertices[next]-vertices[i]);
//        result.side2[i] = lineIntersection(bottom[prev], vertices[i]-vertices[prev], bottom[next], vertices[next]-vertices[i]);
//    }
//    cout << "New Version" << endl;
//}

std::vector<pesVec2f> pesSatinOutlineRenderStitches(const pesSatinOutline& result, float zigzagSpacing){
    std::vector<pesVec2f> stitches;
    
    if (result.length>0) {
        pesVec2f curTop(0, 0);
        pesVec2f curBottom(0, 0);

        pesPolyline segment0;
        pesPolyline segment1;
        
        for (int j = 0, len = result.length - 1, jj = len - 1; j < len; j++) {
            pesVec2f p1 = result.side1[j];
            pesVec2f p2 = result.side1[j + 1];
            pesVec2f p3 = result.side2[j];
            pesVec2f p4 = result.side2[j + 1];
            
            pesVec2f topDiff = p2 - p1;
            pesVec2f bottomDiff = p4 - p3;
            
            pesVec2f midLeft = p1.getMiddle(p3);
            pesVec2f midRight = p2.getMiddle(p4);
            pesVec2f midDiff = midLeft - midRight;
            
            float topLength     = topDiff.length();
            float midLength     = midDiff.length();
            float bottomLength  = bottomDiff.length();
            float maxLength     = max(max(topLength, midLength), bottomLength);
            int numberOfSteps   = (int)round(maxLength / zigzagSpacing);
            if(numberOfSteps<1)
                numberOfSteps = 1;
            
            pesVec2f topStep = topDiff / (float)numberOfSteps;
            pesVec2f bottomStep = bottomDiff / (float)numberOfSteps;
            curTop.set(p1.x, p1.y);
            curBottom.set(p3.x, p3.y);
            
            for (int i = 0; i<numberOfSteps; i++) {
                //stitches.push_back(curTop);
                //stitches.push_back(curBottom);
                segment0.addVertex(curTop);
                segment1.addVertex(curBottom);
                curTop += topStep;
                curBottom += bottomStep;
            }

            if (!segment0.size() || !segment1.size()) {
                continue;
            }

            float len0 = segment0.getPerimeter();
            float len1 = segment1.getPerimeter();
            float fnum_points = max(len0, len1) / zigzagSpacing;
            
            if (fnum_points < 1.667 && j < jj) {
                continue;
                if (fnum_points < 1) fnum_points = 1;
            }

            int num_points = (int)round(fnum_points);

            auto side0 = segment0.getResampledByCount2(num_points);
            auto side1 = segment1.getResampledByCount2(num_points);

            if (!side0.size() || !side1.size()) {
                continue;
            }

            // fixed bug occured some last loop
            if (j == jj) {
                const pesVec2f origin = pesVec2f(0, 0);
                for (int k = 0; k < (int)side0.size(); k++) {
                    if (side0[k] == origin && side1[k] == origin) continue;
                    stitches.push_back(side0[k]);
                    stitches.push_back(side1[k]);
                }
            } 
            else {
                for (int k = 0; k < (int)side0.size(); k++) {
                    stitches.push_back(side0[k]);
                    stitches.push_back(side1[k]);
                }
            }            
            segment0.clear();
            segment1.clear();
        }
        stitches.push_back(curTop);
        stitches.push_back(curBottom);
    }
    return stitches;
}
