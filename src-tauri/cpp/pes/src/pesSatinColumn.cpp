//
//  pesSatinColumn.cpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 3/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#include "pesSatinColumn.hpp"
#include "include/pathops/SkPathOps.h"
#include "pesPathUtility.hpp"

using namespace std;

void pesSatinColumn::preparedata(const pesPath& path0, const pesPath& path1) {
    int count = 0;
    bool isColumn = path0.getCommands().size() == path1.getCommands().size();
    pesPath paths[] = {path0, path1};
    for (int i = 0; i < 2; i++) {
        for (auto c : paths[i].getCommands()) {
            if (c.type == pesPath::Command::Type::_moveTo) {
                count++;
                //break;
            }
            if (c.type != pesPath::Command::Type::_moveTo &&
                c.type != pesPath::Command::Type::_bezierTo &&
                c.type != pesPath::Command::Type::_lineTo) {
                isColumn = false;
            }
        }
    }

    if (isColumn && count == 2) {
        vector<pesPath::Command> command0 = paths[0].getCommands();
        vector<pesPath::Command> command1 = paths[1].getCommands();
        int num = static_cast<int>(command0.size());
        for (int i = 1; i < num; i++) {
            // segment#0
            {
                pesPath::Command prevCmd = command0[i - 1];
                pesPath::Command currentCmd = command0[i];
                pesPolyline segment0;
                segment0.addVertex(prevCmd.to);

                if (currentCmd.type == pesPath::Command::Type::_lineTo)
                    segment0.lineTo(currentCmd.to.x, currentCmd.to.y);
                else
                    segment0.bezierTo(currentCmd.cp1, currentCmd.cp2, currentCmd.to);
                segments[0].push_back(segment0);
            }

            // segment#1
            {
                pesPath::Command prevCmd = command1[i - 1];
                pesPath::Command currentCmd = command1[i];
                pesPolyline segment1;
                segment1.addVertex(prevCmd.to);

                if (currentCmd.type == pesPath::Command::Type::_lineTo)
                    segment1.lineTo(currentCmd.to.x, currentCmd.to.y);
                else
                    segment1.bezierTo(currentCmd.cp1, currentCmd.cp2, currentCmd.to);
                segments[1].push_back(segment1);
            }
        }
    } else if (count == 2) {
        pesPolyline segment0 = paths[0].getOutline().front();
        pesPolyline segment1 = paths[1].getOutline().front();
        if (segment0.size() && segment1.size()) {
            segments[0].push_back(segment0);
            segments[1].push_back(segment1);
        }
    }
}

pesSatinColumn::pesSatinColumn(const vector<pesPath> & paths) {
    if(paths.size()==2){
        preparedata(paths[0], paths[1]);
    }
}

pesSatinColumn::pesSatinColumn(const pesPath& path0, const pesPath& path1) {
    preparedata(path0, path1);
}

pesSatinColumn::~pesSatinColumn(){
    segments[0].clear();
    segments[1].clear();
}

void pesSatinColumn::doUnderlay(pesPolyline & patch){
    const float compensate = 0.5 * 10;
    const float zigzagWidth = 3 * 10;
    
    if(segments[0].size()==segments[1].size()){
        int num = (int)segments[0].size();
#ifndef __EMSCRIPTEN__
#ifndef NDEBUG
        cout << "doUnderlay num=" << num << endl;
#endif
#endif
        for(int i=0; i<num; i++){
            float len0 = segments[0][i].getTotalDistance();
            float len1 = segments[1][i].getTotalDistance();
            int num_points = (int)round( max(len0, len1) / zigzagWidth);
            if(num_points<3){
#ifndef __EMSCRIPTEN__
#ifndef NDEBUG
                cout << "length too short " << (int)round(max(len0, len1)) << " (skip underlay)" << std::endl;
#endif
#endif
                return;
            }
            if(num_points%2==0)
                num_points+=1;
            auto side0 = segments[0][i].getResampledByCount2(num_points);
            auto side1 = segments[1][i].getResampledByCount2(num_points);
            
            if(side0.size()==side1.size()){
                for(int ii=0; ii<side0.size(); ii++){
                    side0[ii] = side0[ii] - (side0[ii]-side1[ii]).normalize() * compensate;
                    side1[ii] = side1[ii] - (side1[ii]-side0[ii]).normalize() * compensate;
                }
                
                if(i>0){
                    pesPolyline centerWalk;
                    for(int ic=0; ic<segments[0][i-1].size(); ic++){
                        pesVec2f p0 = segments[0][i-1].getVertices()[ic];
                        pesVec2f p1 = segments[1][i-1].getVertices()[ic];
                        pesVec2f mid = p0.getMiddle(p1);
                        centerWalk.addVertex(mid);
                    }
                    int num = centerWalk.getPerimeter() / zigzagWidth;
                    centerWalk = centerWalk.getResampledByCount2(num);
                    for(int ic=0; ic<num; ic++){
                        patch.addVertex(centerWalk[ic]);
                    }
#ifndef __EMSCRIPTEN__
#ifndef NDEBUG
                    cout << "doUnderlay-centerWalk id=" << i << endl;
#endif
#endif
                }
                
                for(int j=0; j<side0.size()-1; j++){
                    patch.addVertex(side0[j]);
                    patch.addVertex(side1[++j]);
                }
                for(int k=side0.size()-1; k>0; k--){
                    patch.addVertex(side0[k]);
                    patch.addVertex(side1[--k]);
                }
//                patch.addVertex(side0[0]);
            }
            else{
#ifndef __EMSCRIPTEN__
#ifndef NDEBUG
                cout << "Size miss match" << endl;
#endif
#endif
            }
        }
    }
    
//    if(segments[0].size()==segments[1].size()){
//        float zigzagSpace = zigzagWidth;
//        unsigned int ppmm = param.pixels_per_mm;
//        const float tol = param.satin_auto_shorten_tolerance_mm * ppmm;
//        const float shorten_offset = param.satin_shorten_offset_mm * ppmm;
//        const bool bAutoShorten = param.satinAutoShorten;
//        
//        int num = (int)segments[0].size();
//        for(int i=0; i<num; i++){
//            float len0 = segments[0][i].getPerimeter();
//            float len1 = segments[1][i].getPerimeter();
//            int num_points = (int)round( max(len0, len1) / zigzagSpace);
//            
//            // Auto shorten
//            bool bShorten = false;
//            int shortenSide = 0;
//            if(bAutoShorten){
//                float sp0 = len0/num_points;
//                float sp1 = len1/num_points;
//                if(MIN(sp0, sp1)<tol){
//                    bShorten = true;
//                    if(sp1<sp0) shortenSide = 1;
//                }
//            }
//            
//            auto side0 = segments[0][i].getResampledByCount2(num_points);
//            auto side1 = segments[1][i].getResampledByCount2(num_points);
//            
//            if(side0.size()== side1.size()){
//                // satin: do a zigzag pattern, alternating between the paths.  The
//                // zigzag looks like this to make the satin stitches look perpendicular
//                // to the column:
//                //
//                // /|/|/|/|/|/|/|/|
//                
//                float pullCompensate = param.pull_compensation_mm * param.pixels_per_mm;
//                if(pullCompensate!=0.0){
//                    float offset_px = pullCompensate;
//                    for(int j=0; j<(int)side0.size(); j++){
//                        side0[j] = side0[j] + (side0[j]-side1[j]).normalize() * offset_px;
//                        side1[j] = side1[j] + (side1[j]-side0[j]).normalize() * offset_px;
//                    }
//                }
//                
//                //                if(i==0)
//                {
//                    patch.addVertex(side0[0]);
//                }
//                
//                patch.addVertex(side1[0]);
//                for(int j=1; j<(int)side0.size(); j++){
//                    if(bShorten && j%2==1){
//                        float offset = shorten_offset;
//                        float half = side0[j].distance(side1[j])/2;
//                        if(offset>half)
//                            offset=half;
//                        if(shortenSide==0){
//                            patch.addVertex(side0[j] + pesVec2f(side1[j]-side0[j]).normalize() *  offset);
//                            patch.addVertex(side1[j]);
//                        }
//                        else if(shortenSide==1){
//                            patch.addVertex(side0[j]);
//                            patch.addVertex(side1[j] + pesVec2f(side0[j]-side1[j]).normalize() *  offset);
//                        }
//                    }
//                    else{
//                        // add pair points
//                        patch.addVertex(side0[j]);
//                        patch.addVertex(side1[j]);
//                    }
//                }
//                
//            }
//        }
//    }
//    else{
////        GUI_LogError("SatinColumnCSP") << "csp segments size not equal";
//    }
    
}

const float DT_PERCENT_THRESHOLD = 40.0f;  // mean 40.0%
void pesSatinColumn::doSatin(float zigzagSpace, pesPolyline & patch){
    if(segments[0].size()==segments[1].size()){
        
        unsigned int ppmm = param.pixels_per_mm;
        const float tol = param.satin_auto_shorten_tolerance_mm * ppmm;
        const float shorten_offset = param.satin_shorten_offset_mm * ppmm;
        const bool bAutoShorten = param.satinAutoShorten;

        int stitchcount = 0;
        auto prev0 = pesVec2f(SK_ScalarMax, SK_ScalarMax);
        auto prev1 = pesVec2f(SK_ScalarMin, SK_ScalarMin);
        
        int num = static_cast<int>(segments[0].size());
        for(int i=0; i<num; i++){
            float len0 = segments[0][i].getPerimeter();
            float len1 = segments[1][i].getPerimeter();
            int num_points = (int)round( max(len0, len1) / zigzagSpace);
            
            int shortenSide = 0;
            if (bAutoShorten) {
                const float min = MIN(len0, len1);
                const float max = MAX(len0, len1);
                const float diff = (min - max) / max;
                const float diff_percent = diff * 100.0f;
                if (diff_percent < -DT_PERCENT_THRESHOLD) {
                    const float sp0 = len0 / num_points;
                    const float sp1 = len1 / num_points;
                    if (MIN(sp0, sp1) < tol) {
                        shortenSide = sp1 < sp0 ? 1 : -1;
                    }
                }
            }

            auto side0 = segments[0][i].getResampledByCount2(num_points);
            auto side1 = segments[1][i].getResampledByCount2(num_points);
            
            if(side0.size()== side1.size()){
                // satin: do a zigzag pattern, alternating between the paths.  The
                // zigzag looks like this to make the satin stitches look perpendicular
                // to the column:
                //
                // /|/|/|/|/|/|/|/|
                
                float pullCompensate = param.pull_compensation_mm * param.pixels_per_mm;
                if(pullCompensate!=0.0){
                    float offset_px = pullCompensate;
                    for(int j=0; j<(int)side0.size(); j++){
                        side0[j] = side0[j] + (side0[j]-side1[j]).normalize() * offset_px;
                        side1[j] = side1[j] + (side1[j]-side0[j]).normalize() * offset_px;
                    }
                }
                
                auto v0 = side0[0];
                auto v1 = side1[0];

                if (stitchcount == 0) {
                    prev0 = v0;
                    prev1 = v1;
                    stitchcount++;
                    patch.addVertex(prev0);
                    patch.addVertex(prev1);
                } else {
                    if (prev0 != v0 && prev1 != v1) {
                        prev0 = v0;
                        prev1 = v1;
                        stitchcount++;
                        patch.addVertex(prev0);
                        patch.addVertex(prev1);
                    }
                }

                for (int j = 1, jj = (int)side0.size(); j < jj; j++) {
                    v0 = side0[j];
                    v1 = side1[j];
                    if (shortenSide && stitchcount++ % 2 == 1) {
                        float offset = shorten_offset;
                        float half = v0.distance(v1) / 2;
                        if (offset > half) offset = half;
                        if (shortenSide < 0) {
                            patch.addVertex(v0 + pesVec2f(v1 - v0).normalize() * offset);
                            patch.addVertex(v1);
                        } else {
                            patch.addVertex(v0);
                            patch.addVertex(v1 + pesVec2f(v0 - v1).normalize() * offset);
                        }
                    } else {
                        // add pair points
                        patch.addVertex(v0);
                        patch.addVertex(v1);
                    }
                }

                if (side0.size() > 1) {
                    prev0 = v0;
                    prev1 = v1;
                }
            }
        }
    }
    else{
//        GUI_LogError("SatinColumnCSP") << "csp segments size not equal";
    }
}

void pesSatinColumn::doSatin2(float zigzagSpace, pesPolyline& patch) {
    if (segments[0].size() == segments[1].size()) {
        unsigned int ppmm = param.pixels_per_mm;
        const float tol = param.satin_auto_shorten_tolerance_mm * ppmm;
        const float shorten_offset = param.satin_shorten_offset_mm * ppmm;
        const bool bAutoShorten = param.satinAutoShorten;

        int stitchcount = 0;
        auto prev0 = pesVec2f(SK_ScalarMax, SK_ScalarMax);
        auto prev1 = pesVec2f(SK_ScalarMin, SK_ScalarMin);

        int num = (int)segments[0].size();
        pesPolyline segment0;
        pesPolyline segment1;
        // float minpoints = 10.0 / zigzagSpace * 1.667f;

        for (int i = 0, ii = num - 1; i < num; i++) {
            segment0.addVertices(segments[0][i].getVertices());
            segment1.addVertices(segments[1][i].getVertices());

            float len0 = segment0.getPerimeter();
            float len1 = segment1.getPerimeter();
            float fnum_points = max(len0, len1) / zigzagSpace;

            if (fnum_points < 1.667f) {
                if (i < ii) {
                    continue;
                }
                if (fnum_points < 1) {
                    fnum_points = 1;
                }
            }

            int num_points = (int)round(fnum_points);

            // Auto shorten
            int shortenSide = 0;
            if (bAutoShorten) {
                const float min = MIN(len0, len1);
                const float max = MAX(len0, len1);
                const float diff = (min - max) / max;
                const float diff_percent = diff * 100.0f;
                if (diff_percent < -DT_PERCENT_THRESHOLD) {
                    const float sp0 = len0 / num_points;
                    const float sp1 = len1 / num_points;
                    if (MIN(sp0, sp1) < tol) {
                        shortenSide = sp1 < sp0 ? 1 : -1;
                    }
                }
            }

            auto side0 = segment0.getResampledByCount2(num_points);
            auto side1 = segment1.getResampledByCount2(num_points);

            segment0.clear();
            segment1.clear();

            if (side0.size() == side1.size()) {
                // satin: do a zigzag pattern, alternating between the paths.  The
                // zigzag looks like this to make the satin stitches look perpendicular
                // to the column:
                //
                // /|/|/|/|/|/|/|/|

                float pullCompensate = param.pull_compensation_mm * param.pixels_per_mm;
                if (pullCompensate != 0.0) {
                    float offset_px = pullCompensate;
                    for (int j = 0; j < (int)side0.size(); j++) {
                        side0[j] = side0[j] + (side0[j] - side1[j]).normalize() * offset_px;
                        side1[j] = side1[j] + (side1[j] - side0[j]).normalize() * offset_px;
                    }
                }

                auto v0 = side0[0];
                auto v1 = side1[0];

                if (stitchcount == 0) {
                    prev0 = v0;
                    prev1 = v1;
                    stitchcount++;
                    patch.addVertex(prev0);
                    patch.addVertex(prev1);
                } else {
                    if (prev0 != v0 && prev1 != v1) {
                        prev0 = v0;
                        prev1 = v1;
                        stitchcount++;
                        patch.addVertex(prev0);
                        patch.addVertex(prev1);
                    }
                }

                for (int j = 1, jj = (int)side0.size(); j < jj; j++) {
                    v0 = side0[j];
                    v1 = side1[j];
                    if (shortenSide && stitchcount++ % 2 == 1) {
                        float offset = shorten_offset;
                        float half = v0.distance(v1) / 2;
                        if (offset > half) offset = half;
                        if (shortenSide < 0) {
                            patch.addVertex(v0 + pesVec2f(v1 - v0).normalize() * offset);
                            patch.addVertex(v1);
                        } else {
                            patch.addVertex(v0);
                            patch.addVertex(v1 + pesVec2f(v0 - v1).normalize() * offset);
                        }
                    } else {
                        // add pair points
                        patch.addVertex(v0);
                        patch.addVertex(v1);
                    }
                }

                if (side0.size() > 1) {
                    prev0 = v0;
                    prev1 = v1;
                }
            }
        }
    }
}

bool pesSatinColumn::doSatin3(std::vector<pesPolyline>& patchs,
                              SkPath& remainingArea,
                              SkPath& selfArea,
                              bool bEnableMakeSatinNoneOverlap) {
    bool groupskiped = false;
    if (segments[0].size() == segments[1].size()) {
        unsigned int ppmm = param.pixels_per_mm;
        float pullCompensate = param.pull_compensation_mm * ppmm;
        const float tol = param.satin_auto_shorten_tolerance_mm * ppmm;
        const float shorten_offset = param.satin_shorten_offset_mm * ppmm;
        const bool bAutoShorten = param.satinAutoShorten;
        float zigzagSpace = param.zigzag_spacing_mm;
        
        const float C90DIR = 90 * DEG_TO_RAD;
        const float C45DIR = 45 * DEG_TO_RAD;

        pesPolyline segment0;
        pesPolyline segment1;
        bool selfskiped = false;
        float selfx50, selfy50;

        patchs.emplace_back();

        const float pullpixel = 1.0f;
        int stitchcount = 0;
        auto prev0 = pesVec2f(SK_ScalarMax, SK_ScalarMax);
        auto prev1 = pesVec2f(SK_ScalarMin, SK_ScalarMin);

        auto skpself = SkPath(selfArea);
        skpself.setFillType(SkPathFillType::kEvenOdd);

        std::vector<SkPath> skpintersects;
        int nSelfIntersect = 0;

        if (bEnableMakeSatinNoneOverlap) {
            auto skpintersect = SkPath(selfArea);
            Simplify(skpintersect, &skpintersect);
            Op(skpintersect, skpself, SkPathOp::kDifference_SkPathOp, &skpintersect);
            AsWinding(skpintersect, &skpintersect);
            skpintersect.setFillType(SkPathFillType::kWinding);
            for (int verb = 0, verbs = (int)skpintersect.countVerbs(); verb < verbs; verb++) {
                if (skpintersect.getVerb(verb) == SkPath::Verb::kMove_Verb) {
                    nSelfIntersect++;
                }
            }
            if (nSelfIntersect > 0) {
                auto pp = toPes(skpintersect);
                auto pps = pp.getSubPath();
                for (int i = 0, ii = (int)pps.size(); i < ii; i++) {
                    skpintersects.push_back(toSk(pps[i]));
                }
            }
        }

        if (skpself.isEmpty()) {
            nSelfIntersect = 0;
        }

        bool isfirstsegment = true;
        for (int i = 0, ii = (int)segments[0].size(), iii = ii - 1; i < ii; i++) {
            segment0.addVertices(segments[0][i].getVertices());
            segment1.addVertices(segments[1][i].getVertices());

            if (segment0.size() == 0) {
                continue;
            }

            float len0 = segment0.getPerimeter();
            float len1 = segment1.getPerimeter();
            float fnum_points = max(len0, len1) / zigzagSpace;
            bool islastsegment = i >= iii;

            if (fnum_points < 1.667f) {
                if (!islastsegment) {
                    continue;
                }
                if (fnum_points < 1) {
                    fnum_points = 1;
                }
            }

            int num_points = (int)round(fnum_points);

            // Auto shorten
            int shortenSide = 0;
            if (bAutoShorten) {
                const float min = MIN(len0, len1);
                const float max = MAX(len0, len1);
                const float diff = (min - max) / max;
                const float diff_percent = diff * 100.0f;
                if (diff_percent < -DT_PERCENT_THRESHOLD) {
                    const float sp0 = len0 / num_points;
                    const float sp1 = len1 / num_points;
                    if (MIN(sp0, sp1) < tol) {
                        shortenSide = sp1 < sp0 ? 1 : -1;
                    }
                }
            }

            auto vertices0 = segment0.getResampledByCount2(num_points);
            auto vertices1 = segment1.getResampledByCount2(num_points);
            if (vertices0.size() == 0 || vertices0.size() != vertices1.size()) {
                continue;
            }

            pesPolyline side0;
            pesPolyline side1;
            std::vector<bool> skips;

            if (!bEnableMakeSatinNoneOverlap) {
                for (int k = 0, kk = vertices0.size(); k < kk; k++) {
                    side0.addVertex(vertices0[k]);
                    side1.addVertex(vertices1[k]);
                    skips.push_back(false);
                }
            } 
            else {
                for (int k = 0, kk = vertices0.size(), kkk = kk - 1; k < kk; k++) {
                    bool issecondstitch = k == 1;
                    bool islaststitch = k >= kkk;

                    side0.addVertex(vertices0[k]);
                    side1.addVertex(vertices1[k]);

                    const auto v0 = vertices0[k];
                    const auto v1 = vertices1[k];

                    const auto v0x = v0.x;
                    const auto v0y = v0.y;
                    const auto v1x = v1.x;
                    const auto v1y = v1.y;

                    const auto pullout0 = v0 + (v0 - v1).normalize() * pullpixel;
                    const auto pullout1 = v1 + (v1 - v0).normalize() * pullpixel;

                    if (nSelfIntersect > 0) {
                        if (skpself.contains(pullout0.x, pullout0.y) &&
                            skpself.contains(pullout1.x, pullout1.y)) {
                            const auto dx = v1x - v0x;
                            const auto dy = v1y - v0y;
                            const auto x25 = 0.25f * dx + v0x;
                            const auto y25 = 0.25f * dy + v0y;
                            const auto x50 = 0.50f * dx + v0x;
                            const auto y50 = 0.50f * dy + v0y;
                            const auto x75 = 0.75f * dx + v0x;
                            const auto y75 = 0.75f * dy + v0y;
                            if (!skpself.contains(x25, y25) && 
                                !skpself.contains(x50, y50) &&
                                !skpself.contains(x75, y75)) {
                                selfx50 = x50;
                                selfy50 = y50;
                                if (!selfskiped) {
                                    if (isfirstsegment && issecondstitch) {
                                        skips[0] = true;
                                    }
                                }
                                selfskiped = true;
                                skips.push_back(true);
                                continue;
                            }
                        }

                        if (selfskiped) {
                            selfskiped = false;
                            for (auto& skp : skpintersects) {
                                if (!skp.isEmpty() && skp.contains(selfx50, selfy50)) {
                                    skpself.addPath(skp);
                                    nSelfIntersect--;
                                    skp = SkPath();
                                    break;
                                }
                            }

                            if (islastsegment && islaststitch) {
                                skips.push_back(true);
                                continue;
                            }
                        }
                    }

                    if (!remainingArea.isEmpty()) {
                        if (((remainingArea.contains(pullout0.x, pullout0.y) || remainingArea.contains(v0x, v0y)) &&
                              remainingArea.contains(pullout1.x, pullout1.y))
                            ||
                            ((remainingArea.contains(pullout1.x, pullout1.y) || remainingArea.contains(v1x, v1y)) &&
                              remainingArea.contains(pullout0.x, pullout0.y))
                        ) {
                            const auto dx = v1x - v0x;
                            const auto dy = v1y - v0y;
                            const auto x25 = 0.25f * dx + v0x;
                            const auto y25 = 0.25f * dy + v0y;
                            const auto x50 = 0.50f * dx + v0x;
                            const auto y50 = 0.50f * dy + v0y;
                            const auto x75 = 0.75f * dx + v0x;
                            const auto y75 = 0.75f * dy + v0y;
                            if (remainingArea.contains(x25, y25) && 
                                remainingArea.contains(x50, y50) &&
                                remainingArea.contains(x75, y75)) {
                                skips.push_back(true);
                                continue;
                            }
                        }
                    }
                    
                    skips.push_back(false);
                }
            }

            if (side0.size() == 0) {
                continue;
            }

            segment0.clear();
            segment1.clear();

            if (side0.size() == side1.size()) {
                // satin: do a zigzag pattern, alternating between the paths.  The
                // zigzag looks like this to make the satin stitches look perpendicular
                // to the column:
                //
                // /|/|/|/|/|/|/|/|

                //if (pullCompensate != 0.0) {
                //    const float C90DIR = 90 * DEG_TO_RAD;
                //    auto mid = side0[0].getMiddle(side1[0]);
                //    auto prev = mid;
                //    int j = 0, jj = (int)side0.size(), k = 1;
                //    for (; k < jj; j++, k++) {
                //        auto next = side0[k].getMiddle(side1[k]);
                //        auto d0 = next - mid;
                //        auto d1 = side1[j] - mid;
                //        auto dot = d0.dot(d1);
                //        float angle = dot == 0 ? C90DIR : acosf(dot / d0.length() / d1.length());
                //        float rscale = 1 / cosf(C90DIR - angle);
                //        float r = d1.length() / rscale;
                //
                //        side0[j] = side0[j] + (side0[j] - side1[j]).normalize() * pullCompensate * rscale;
                //        side1[j] = side1[j] + (side1[j] - side0[j]).normalize() * pullCompensate * rscale;
                //
                //        prev = mid;
                //        mid = next;
                //    }
                //
                //    auto midnext = mid + (mid - prev).normalize();
                //    auto d0 = midnext - mid;
                //    auto d1 = side1[j] - mid;
                //    auto dot = d0.dot(d1);
                //    float angle = dot == 0 ? C90DIR : acosf(dot / d0.length() / d1.length());
                //    float rscale = 1 / cosf(C90DIR - angle);
                //    float z = d1.length() / rscale;
                //
                //    side0[j] = side0[j] + (side0[j] - side1[j]).normalize() * pullCompensate * rscale;
                //    side1[j] = side1[j] + (side1[j] - side0[j]).normalize() * pullCompensate * rscale;
                //}

                //auto v0 = side0[0];
                //auto v1 = side1[0];
                //auto mask = masks[0];

                //if (stitchcount == 0) {
                //    prev0 = v0;
                //    prev1 = v1;
                //    stitchcount++;
                //    patch.addVertex(prev0);
                //    patch.addVertex(prev1);
                //} 
                //else {
                //    if (prev0 != v0 && prev1 != v1) {
                //        prev0 = v0;
                //        prev1 = v1;
                //        stitchcount++;
                //        patch.addVertex(prev0);
                //        patch.addVertex(prev1);
                //    }
                //}

                const int jj = (int)side0.size();
                const int jjj = jj - 1;
                //if (!islastsegment) {
                int patchcount = 0;
                int l = 0;
                int r = 0;

                for (int j = 0; j < jj; j++) {
                    if (skips[j]) {
                        if (patchcount > 0) {
                            if (l > 0 && skips[l - 1] == true) {
                                skips[--l] = false;
                            }
                            if (r < jjj && skips[r + 1] == true) {
                                skips[++r] = false;
                            }
                            patchcount = 0;
                        }
                    } else {
                        if (patchcount == 0) {
                            l = r = j;
                        } else {
                            r = j;
                        }
                        patchcount++;
                    }
                }
                if (patchcount > 0) {
                    if (l > 0 && skips[l - 1] == true) {
                        skips[--l] = false;
                    }
                    //if (r < jjj && skips[r + 1] == true) {
                    //    skips[++r] = false;
                    //}
                    //patchcount = 0;
                }
                
                for (int j = 0, k = 1; j < jj; j++, k++) {
                    if (skips[j]) {
                        if (stitchcount > 0) {
                            if (stitchcount == 1) {
                                patchs.back().clear();
                                patchs.pop_back();
                            }
                            patchs.emplace_back();
                            stitchcount = 0;
                        }
                        continue;
                    }

                    auto& patch = patchs.back();
                    auto v0 = side0[j];
                    auto v1 = side1[j];

                    if (j == 0) {
                        if (prev0 == v0 && prev1 == v1) {
                            continue;
                        }
                    }
                    bool islaststitch = j >= jjj;
                    if (shortenSide && stitchcount % 2 == 1 && !(islastsegment && islaststitch)) {
                        //float offset = shorten_offset;
                        //float half = v0.distance(v1) / 2;
                        //if (offset > half) offset = half;

                        auto getscale = [&]() -> float {
                            const auto m0 = v0.getMiddle(v1);
                            const auto m1 = islaststitch? (m0 + (m0 - prev0.getMiddle(prev1)).normalize()) : side0[k].getMiddle(side1[k]);
                            const auto d0 = m1 - m0;
                            const auto d1 = v1 - m0;
                            const auto dot = d0.dot(d1);
                            float angle = dot == 0 ? C90DIR : acosf(dot / d0.length() / d1.length());
                            if (angle < C45DIR) angle += C45DIR;
                            angle = abs(fmod(angle - C45DIR, C90DIR) + C45DIR);
                            const float rscale = 1.0f / cosf(C90DIR - angle);
                            return rscale;
                        };

                        if (shortenSide < 0) {
                            //patch.addVertex(v0 + pesVec2f(v1 - v0).normalize() * offset);
                            if (prev0.distance(v0) <= zigzagSpace) {
                                const float maxoffset = v0.distance(v1) * 0.3f;
                                const float offset = min(shorten_offset * getscale(), maxoffset);
                                const auto m = prev1.getMiddle(v1);
                                patch.addVertex(v0 + pesVec2f(m - v0).normalize() * offset);
                                //patch.addVertex(v0 + pesVec2f(v1 - v0).normalize() * offset);
                            } 
                            else {
                                patch.addVertex(v0);
                            }                            
                            patch.addVertex(v1);
                        } 
                        else {
                            patch.addVertex(v0);
                            //patch.addVertex(v1 + pesVec2f(v0 - v1).normalize() * offset);
                            if (prev1.distance(v1) <= zigzagSpace) {
                                const float maxoffset = v0.distance(v1) * 0.3f;
                                const float offset = min(shorten_offset * getscale(), maxoffset);
                                const auto m = islaststitch? (v0 + (v0 - prev0.getMiddle(prev1)).normalize()) : v0.getMiddle(side0[k]);
                                patch.addVertex(v1 + pesVec2f(m - v1).normalize() * offset);
                                //patch.addVertex(v1 + pesVec2f(v0 - v1).normalize() * offset);
                            } 
                            else {
                                patch.addVertex(v1);
                            }
                        }
                    }
                    else {
                        // add pair points
                        patch.addVertex(v0);
                        patch.addVertex(v1);
                    }
                    stitchcount++;
                    prev0 = v0;
                    prev1 = v1;
                }
            }
            isfirstsegment = false;
        }

        if (stitchcount == 1 && patchs.size() > 1) {
            patchs.back().clear();
            patchs.pop_back();
        }

        // SkDebugf("\n\npatchs size: %u\n", patchs.size());
        //for (auto& patch : patchs) {
        //    SkDebugf("-patch size: %u\n", patch.size());
        //    if (patch.size() <= 2) {
        //        patch.clear();
        //    }
        //}
    }

    return groupskiped;
}


// MARK: pesSatinColumnGeneretor

pesSatinColumnGenerator::pesSatinColumnGenerator()
:cnt(0)
{
    csp[0].reset();
    csp[1].reset();
}

pesSatinColumnGenerator::~pesSatinColumnGenerator(){
    csp[0].reset();
    csp[1].reset();
}

void pesSatinColumnGenerator::reset(){
    csp[0].reset();
    csp[1].reset();
    cnt=0;
}

void pesSatinColumnGenerator::addCurvePoint(const pesVec2f & p){
    csp[cnt%2].addCurvePoint(p);
    cnt++;
}

void pesSatinColumnGenerator::addCornerPoint(const pesVec2f & p){
    csp[cnt%2].addCornerPoint(p);
    cnt++;
}

void pesSatinColumnGenerator::calculateCSP(){
    csp[0].calculateCSP();
    csp[1].calculateCSP();
}

vector<pesPath> pesSatinColumnGenerator::getSatinColumnPaths(){
    vector<pesPath> paths;
    paths.push_back(csp[0].path);
    paths.push_back(csp[1].path);
    return paths;
}

//#include "pesEngine.h"
//void example(){
//    pesSatinColumnGenerator gen;
//
//    gen.addCornerPoint(pesVec2f(0,0));      // moveTo
//    gen.addCornerPoint(pesVec2f(0,10));     // moveTo
//
//    gen.addCornerPoint(pesVec2f(10,0));     // lineTo
//    gen.addCornerPoint(pesVec2f(10,10));    // lineTo
//
//    gen.addCurvePoint(pesVec2f(15, 5));     // bezierTo
//    gen.addCurvePoint(pesVec2f(15, 15));    // bezierTo
//
//    gen.calculateCSP();                     // calculate cubic super path
//
//    pesData data;
//    data.parameter.setType(pesData::OBJECT_TYPE_SCALABLE_SATINCOLUMN);
//    data.paths = gen.getSatinColumnPaths();
//    data.applyFill();
//
//    pesGetDocument()->addObject(data);
//}
