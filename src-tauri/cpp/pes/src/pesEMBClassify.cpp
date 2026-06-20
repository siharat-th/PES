//
//  pesEMBClassify.cpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 19/2/2563 BE.
//  Copyright © 2563 SIHARAT THAMMAYA. All rights reserved.
//

#include "pesEMBClassify.hpp"

const int sharpAngleTreshold = 60;  // 30;

typedef struct classifyInfo_ {
    pesEMBClassifyType type = CLASSIFY_TYPE_UNKNOW;
    int sign = 0;
    std::set<int> indexs;
} classifyInfo;

struct OutPt {
    int Idx;
    pesVec2f Pt;
    OutPt* Next;
    OutPt* Prev;
};

// satin column classify
std::vector<pesEMBClassifyResult> pesEMBClassify::classifySatin(const pesStitchBlock& block) {
    std::vector<pesEMBClassifyResult> results;

    std::deque<pesVec2f> stitches;
    std::list<classifyInfo> infos;
    infos.emplace_back();
    int num = (int)block.size();

    for (int i = 0; i < num; i++) {
        stitches.push_back(block.polyline[i]);
        if (block.types[i] == NORMAL_STITCH) {
            if (stitches.size() == 3) {
                pesVec2f p0 = stitches.at(0);
                pesVec2f p1 = stitches.at(1);
                pesVec2f p2 = stitches.at(2);
                auto v0 = p0 - p1;
                auto v1 = p2 - p1;
                float angle = v0.angle(v1);
                auto sign = copysign(1, angle);
                float absAngle = fabs(angle);
                if (absAngle < sharpAngleTreshold && absAngle > 0) {
                    //                    if(absAngle>0){
                    if (infos.back().type != CLASSIFY_TYPE_SATIN &&
                        infos.back().type != CLASSIFY_TYPE_UNKNOW) {
                        infos.emplace_back();
                    }

                    if (infos.back().sign != sign) {
                        //  /\/\/\/\/\/\ classify zigzag pattern, angle between vec are toggle
                        //  numeric sign in orders (1, -1, 1, -1, ...)
                        infos.back().sign = sign;
                        infos.back().type = CLASSIFY_TYPE_SATIN;
                        infos.back().indexs.emplace(i - 2);
                        infos.back().indexs.emplace(i - 1);
                        infos.back().indexs.emplace(i);
                    } else {
                        if (infos.back().type == CLASSIFY_TYPE_SATIN &&
                            infos.back().indexs.size()) {
                            infos.emplace_back();
                        }
                        stitches.pop_front();
                    }
                } else {
                    if (infos.back().type == CLASSIFY_TYPE_SATIN && infos.back().indexs.size()) {
                        infos.emplace_back();
                    }
                    stitches.pop_front();
                }

                if (stitches.size()) stitches.pop_front();
            }
        } else {
            if (infos.back().type == CLASSIFY_TYPE_SATIN && infos.back().indexs.size()) {
                infos.emplace_back();
            }
            while (stitches.size() > 1) {
                stitches.pop_front();
            }
        }
    }

    infos.remove_if([](const classifyInfo& info) -> bool { return info.indexs.size() <= 3; });

    if (infos.size()) {
        for (auto& info : infos) {
            if (info.type == CLASSIFY_TYPE_SATIN) {
                pesEMBClassifyResult res;
                res.type = CLASSIFY_TYPE_SATIN;
                res.paths.resize(2);
                static pesColor cornFlowerBlue(100, 149, 237);
                static pesColor orange(255, 165, 0);
                res.paths[0].setStrokeColor(cornFlowerBlue);
                res.paths[1].setStrokeColor(orange);
                res.paths[0].setStrokeWidth(2);
                res.paths[1].setStrokeWidth(2);
                res.paths[0].setFilled(false);
                res.paths[1].setFilled(false);

                if (info.indexs.size() % 2 != 0) {
                    auto l = info.indexs.end();
                    l--;
                    info.indexs.erase(l);
                }

                // v.2 (non-segmentation)
                int cnt = 0;
                int size = (int)info.indexs.size();
                pesPolyline polylines[2];
                for (auto idx : info.indexs) {
                    if (res.firstIndex == -1) res.firstIndex = idx;
                    res.lastIndex = idx;
                    auto& poly = polylines[cnt % 2];
                    pesVec2f p = block.polyline[idx];
                    if (cnt >= size - 4 || poly.size() == 0) {
                        poly.addVertex(p);
                    } else {
                        bool skip = (cnt % 4) >= 2;
                        if (!skip) poly.addVertex(p);
                    }
                    cnt++;
                }

                polylines[0].simplify();
                polylines[1].simplify();

                for (int i = 0; i < 2; i++) {
                    for (auto& poly : polylines[i]) {
                        if (res.paths[i].getCommands().empty()) {
                            res.paths[i].moveTo(poly.x, poly.y);
                        } else {
                            res.paths[i].lineTo(poly.x, poly.y);
                        }
                    }
                }

                int s = (int)res.paths[1].getCommands().size();
                if (s >= 3) {
                    pesVec2f p0 = res.paths[1].getCommands().at(s - 3).to;
                    pesVec2f p1 = res.paths[1].getCommands().at(s - 2).to;
                    pesVec2f p2 = res.paths[1].getCommands().at(s - 1).to;
                    pesVec2f v0 = p1 - p0;
                    pesVec2f v1 = p2 - p1;
                    float angle = v0.angle(v1);
                    if (fabs(angle) > 60) {
                        res.paths[1].getCommands().erase(res.paths[1].getCommands().end() - 1);
                    }
                }

                // v.3 (segmentation)
                //                int cnt = 0;
                //                int size = (int)info.indexs.size();
                //                pesPolyline polylines[2];
                //                for(auto idx:info.indexs){
                //                    if(res.firstIndex==-1)
                //                        res.firstIndex = idx;
                //                    res.lastIndex = idx;
                //
                //                    auto & poly = polylines[cnt%2];
                //                    pesVec2f p = block.polyline[idx];
                //                    if(cnt>=size-2 || poly.size()==0){
                //                        poly.addVertex(p);
                //                    }
                //                    else{
                //                        bool skip = (cnt%4)>=2;
                //                        if(!skip)
                //                            poly.addVertex(p);
                //                    }
                //                    cnt++;
                //                }
                //
                //                pesPolyline p0 = polylines[0];
                //                pesPolyline p1 = polylines[1];
                //                p0.simplify(0.5);
                //                p1.simplify(0.5);
                //                if(p0.size()>p1.size()){
                //                    res.paths[0].moveTo(polylines[0][0].x, polylines[0][0].y);
                //                    res.paths[1].moveTo(polylines[1][0].x, polylines[1][0].y);
                //
                //                    for(int i=1; i<p0.size()-1; i++){
                //                        unsigned int closetIdx;
                //                        polylines[0].getClosestPoint(p0[i], &closetIdx);
                //                        res.paths[0].lineTo(polylines[0][closetIdx].x,
                //                        polylines[0][closetIdx].y);
                //                        res.paths[1].lineTo(polylines[1][closetIdx].x,
                //                        polylines[1][closetIdx].y);
                //                    }
                //
                //                    res.paths[0].lineTo(polylines[0][(int)polylines[0].size()-1].x,
                //                    polylines[0][(int)polylines[0].size()-1].y);
                //                    res.paths[1].lineTo(polylines[1][(int)polylines[1].size()-1].x,
                //                    polylines[1][(int)polylines[1].size()-1].y);
                //                }
                //                else{
                //                    res.paths[0].moveTo(polylines[0][0].x, polylines[0][0].y);
                //                    res.paths[1].moveTo(polylines[1][0].x, polylines[1][0].y);
                //
                //                    for(int i=1; i<p1.size()-1; i++){
                //                        unsigned int closetIdx;
                //                        polylines[1].getClosestPoint(p1[i], &closetIdx);
                //                        res.paths[0].lineTo(polylines[0][closetIdx].x,
                //                        polylines[0][closetIdx].y);
                //                        res.paths[1].lineTo(polylines[1][closetIdx].x,
                //                        polylines[1][closetIdx].y);
                //                    }
                //
                //                    res.paths[0].lineTo(polylines[0][(int)polylines[0].size()-1].x,
                //                    polylines[0][(int)polylines[0].size()-1].y);
                //                    res.paths[1].lineTo(polylines[1][(int)polylines[1].size()-1].x,
                //                    polylines[1][(int)polylines[1].size()-1].y);
                //                }

                results.push_back(res);
            }
        }
    }

    return results;
}

std::vector<pesEMBClassifyResult> pesEMBClassify::classifyUnknow(
        const pesStitchBlock& block, const std::vector<pesEMBClassifyResult>& satinResults) {
    std::vector<pesEMBClassifyResult> results;
    if (satinResults.size()) {
        if (satinResults.front().firstIndex > 0) {
            pesEMBClassifyResult res;
            res.type = pesEMBClassifyType::CLASSIFY_TYPE_UNKNOW;
            res.firstIndex = 0;
            res.lastIndex = satinResults.front().firstIndex;
            results.push_back(res);
        }
        for (int i = 1; i < (int)satinResults.size(); i++) {
            if (satinResults[i].firstIndex - satinResults[i - 1].lastIndex > 1) {
                pesEMBClassifyResult res;
                res.type = pesEMBClassifyType::CLASSIFY_TYPE_UNKNOW;
                res.firstIndex = satinResults[i - 1].lastIndex;
                res.lastIndex = satinResults[i].firstIndex;
                results.push_back(res);
            }
        }
        if (satinResults.back().lastIndex < (int)block.size() - 1) {
            pesEMBClassifyResult res;
            res.type = pesEMBClassifyType::CLASSIFY_TYPE_UNKNOW;
            res.firstIndex = satinResults.back().lastIndex;
            res.lastIndex = (int)block.size() - 1;
            results.push_back(res);
        }
    } else {
        pesEMBClassifyResult res;
        res.type = pesEMBClassifyType::CLASSIFY_TYPE_UNKNOW;
        res.firstIndex = 0;
        res.lastIndex = (int)block.size() - 1;
        results.push_back(res);
    }

    return results;
}

// std::vector<pesEMBClassifyResult> pesEMBClassify::classifyRunning(const pesStitchBlock & block, const
// std::vector<pesEMBClassifyResult> & unknowResults){
//    std::vector<pesEMBClassifyResult> results;
//
//    for(auto unknow:unknowResults){
//        pesEMBClassifyResult res;
//        res.type = pesEMBClassifyType::CLASSIFY_TYPE_RUNNING;
//        res.firstIndex = unknow.firstIndex;
//        res.lastIndex = unknow.lastIndex;
//        res.paths.resize(1);
//        res.paths[0].setStrokeColor(block.color);
//        res.paths[0].setStrokeWidth(1);
//        res.paths[0].setFilled(false);
//        for(int i=res.firstIndex; i<=res.lastIndex; i++){
//            auto p = block.polyline[i];
//            auto type = block.types[i];
//            if(type==JUMP_STITCH || type==TRIM_STITCH || res.paths[0].getCommands().empty()){
//                res.paths[0].moveTo(p.x, p.y);
//            }
//            else if(type==NORMAL_STITCH){
//                res.paths[0].lineTo(p.x, p.y);
//            }
//        }
//        results.push_back(res);
//    }
//
//    return results;
//}

std::vector<pesEMBClassifyResult> pes_classify(const pesStitchBlock& block) {
    return pesEMBClassify::classify(block);
}

// MARK: Utility fuction
bool SlopesEqual(const pesVec2f& pt1, const pesVec2f& pt2, const pesVec2f& pt3) {
    return pesIsFloatEqual((pt1.y - pt2.y) * (pt2.x - pt3.x), (pt1.x - pt2.x) * (pt2.y - pt3.y));
}

bool SlopesEqual(const pesVec2f& pt1,
                 const pesVec2f& pt2,
                 const pesVec2f& pt3,
                 const pesVec2f& pt4) {
    return pesIsFloatEqual((pt1.y - pt2.y) * (pt3.x - pt4.x), (pt1.x - pt2.x) * (pt3.y - pt4.y));
}

double DistanceFromLineSqrd(const pesVec2f& pt, const pesVec2f& ln1, const pesVec2f& ln2) {
    // The equation of a line in general form (Ax + By + C = 0)
    // given 2 points (xπ,yπ) & (x≤,y≤) is ...
    //(yπ - y≤)x + (x≤ - xπ)y + (y≤ - yπ)xπ - (x≤ - xπ)yπ = 0
    // A = (yπ - y≤); B = (x≤ - xπ); C = (y≤ - yπ)xπ - (x≤ - xπ)yπ
    // perpendicular distance of point (x≥,y≥) = (Ax≥ + By≥ + C)/Sqrt(A≤ + B≤)
    // see http://en.wikipedia.org/wiki/Perpendicular_distance
    double A = double(ln1.y - ln2.y);
    double B = double(ln2.x - ln1.x);
    double C = A * ln1.x + B * ln1.y;
    C = A * pt.x + B * pt.y - C;
    return (C * C) / (A * A + B * B);
}

bool SlopesNearCollinear(const pesVec2f& pt1,
                         const pesVec2f& pt2,
                         const pesVec2f& pt3,
                         double distSqrd) {
    // this function is more accurate when the point that's geometrically
    // between the other 2 points is the one that's tested for distance.
    // ie makes it more likely to pick up 'spikes' ...
    if (fabs(pt1.x - pt2.x) > fabs(pt1.y - pt2.y)) {
        if ((pt1.x > pt2.x) == (pt1.x < pt3.x))
            return DistanceFromLineSqrd(pt1, pt2, pt3) < distSqrd;
        else if ((pt2.x > pt1.x) == (pt2.x < pt3.x))
            return DistanceFromLineSqrd(pt2, pt1, pt3) < distSqrd;
        else
            return DistanceFromLineSqrd(pt3, pt1, pt2) < distSqrd;
    } else {
        if ((pt1.y > pt2.y) == (pt1.y < pt3.y))
            return DistanceFromLineSqrd(pt1, pt2, pt3) < distSqrd;
        else if ((pt2.y > pt1.y) == (pt2.y < pt3.y))
            return DistanceFromLineSqrd(pt2, pt1, pt3) < distSqrd;
        else
            return DistanceFromLineSqrd(pt3, pt1, pt2) < distSqrd;
    }
}

bool PointsAreClose(pesVec2f pt1, pesVec2f pt2, double distSqrd) {
    double Dx = (double)pt1.x - pt2.x;
    double dy = (double)pt1.y - pt2.y;
    return ((Dx * Dx) + (dy * dy) <= distSqrd);
}

OutPt* ExcludeOp(OutPt* op) {
    OutPt* result = op->Prev;
    result->Next = op->Next;
    op->Next->Prev = result;
    result->Idx = 0;
    return result;
}

bool Pt2IsBetweenPt1AndPt3(const pesVec2f& pt1, const pesVec2f& pt2, const pesVec2f& pt3) {
    if ((pt1 == pt3) || (pt1 == pt2) || (pt3 == pt2))
        return false;
    else if (pt1.x != pt3.x)
        return (pt2.x > pt1.x) == (pt2.x < pt3.x);
    else
        return (pt2.y > pt1.y) == (pt2.y < pt3.y);
}

bool IsPointOnLine(const pesVec2f& p, const pesVec2f& l0, const pesVec2f& l1) {
    float dx0 = l1.x - l0.x;
    float dy0 = l1.y - l0.y;
    float dx1 = p.x - l0.x;
    float dy1 = p.y - l0.y;
    return pesIsFloatEqual(dx0 * dy1, dy0 * dx1);
}

// void SimplifyPoly(const pesPolyline& in_poly, pesPolyline & out_poly, double distance){
//    size_t size = in_poly.size();
//
//    if (size == 0)
//    {
//      out_poly.clear();
//      return;
//    }
//
//    OutPt* outPts = new OutPt[size];
//    for (int i = 0; i < size; ++i)
//    {
//      outPts[i].Pt = in_poly[i];
//
//      outPts[i].Next = &outPts[(i+1)%size];
//      outPts[i].Next->Prev = &outPts[i];
//      outPts[i].Idx = 0;
//    }
//
//    double distSqrd = distance * distance;
//    OutPt* op = &outPts[0];
//    while (op->Idx == 0 && op->Next != op->Prev)
//      {
//        if (PointsAreClose(op->Pt, op->Prev->Pt, distSqrd))
//        {
//            cout << "PointsAreClose(1)" << endl;
//          op = ExcludeOp(op);
//          size--;
//        }
//        else if (PointsAreClose(op->Prev->Pt, op->Next->Pt, distSqrd))
//        {
//            cout << "PointsAreClose(2)" << endl;
//          ExcludeOp(op->Next);
//          op = ExcludeOp(op);
//          size -= 2;
//        }
//        else if (SlopesNearCollinear(op->Prev->Pt, op->Pt, op->Next->Pt, distSqrd))
//        {
//            cout << "SlopesNearCollinear" << endl;
//          op = ExcludeOp(op);
//          size--;
//        }
//        else
//        {
//          op->Idx = 1;
//          op = op->Next;
//        }
//      }
//
////      if (size < 3)
////          size = 0;
//      out_poly.clear();
//      out_poly.resize(size);
//      for (int i = 0; i < size; ++i)
//      {
//        out_poly[i] = op->Pt;
//        op = op->Next;
//      }
//      delete [] outPts;
//}

struct Segment {
    pesVec2f c0, c1;
    const float epsilon = 0.1;

    //    bool isParallelTo(Segment s)
    //    {
    //        int dx0 = c1.x - c0.x;
    //        int dy0 = c1.y - c0.y;
    //        int dx1 = s.c1.x - s.c0.x;
    //        int dy1 = s.c1.y - s.c0.y;
    //        return dx0 * dy1 == dy0 * dx1;
    //    }

    bool isFloatEqual(float a, float b) { return fabs(a - b) <= epsilon; }

    bool isParallelTo(Segment s) {
        float dx0 = c1.x - c0.x;
        float dy0 = c1.y - c0.y;
        float dx1 = s.c1.x - s.c0.x;
        float dy1 = s.c1.y - s.c0.y;
        return isFloatEqual((dx0 * dy1), (dy0 * dx1));
    }

    bool isPointOnLine(pesVec2f c) { return isPointOnLine(c.x, c.y); }

    //    bool isPointOnLine(float x, float y)
    //    {
    //        int dx0 = c1.x - c0.x;
    //        int dy0 = c1.y - c0.y;
    //        int dx1 = x - c0.x;
    //        int dy1 = y - c0.y;
    //        return dx0 * dy1 == dy0 * dx1;
    //    }

    bool isPointOnLine(float x, float y) {
        float dx0 = c1.x - c0.x;
        float dy0 = c1.y - c0.y;
        float dx1 = x - c0.x;
        float dy1 = y - c0.y;
        return isFloatEqual((dx0 * dy1), (dy0 * dx1));
    }

    bool isColinearWith(Segment s) { return isParallelTo(s) && isPointOnLine(s.c0.x, s.c0.y); }
};

std::vector<pesEMBClassifyResult> pesEMBClassify::classifyRunning(
        const pesStitchBlock& block, const std::vector<pesEMBClassifyResult>& unknowResults) {
    std::vector<pesEMBClassifyResult> results;

    for (auto unknow : unknowResults) {
        pesEMBClassifyResult res;
        res.type = pesEMBClassifyType::CLASSIFY_TYPE_RUNNING;
        res.firstIndex = unknow.firstIndex;
        res.lastIndex = unknow.lastIndex;
        res.paths.resize(1);
        res.paths[0].setStrokeColor(block.color);
        res.paths[0].setStrokeWidth(1);
        res.paths[0].setFilled(false);
        for (int i = res.firstIndex; i <= res.lastIndex; i++) {
            auto p = block.polyline[i];
            auto type = block.types[i];
            if (type == JUMP_STITCH || type == TRIM_STITCH || res.paths[0].getCommands().empty()) {
                res.paths[0].moveTo(p.x, p.y);
            } else if (type == NORMAL_STITCH) {
                res.paths[0].lineTo(p.x, p.y);
            }
        }
        results.push_back(res);
    }

    return results;
}

std::vector<pesEMBClassifyResult> pesEMBClassify::classifyFilled(const pesStitchBlock& block,
                                                            const pesEMBClassifyResult& unknow) {
    std::vector<pesEMBClassifyResult> results;

    std::deque<pesVec2f> stitches;
    std::list<classifyInfo> infos;
    infos.emplace_back();

    for (int i = unknow.firstIndex; i <= unknow.lastIndex; i++) {
        stitches.push_back(block.polyline[i]);
        if (block.types[i] == NORMAL_STITCH) {
            if (stitches.size() == 3) {
                pesVec2f p0 = stitches.at(0);
                pesVec2f p1 = stitches.at(1);
                pesVec2f p2 = stitches.at(2);
                Segment s0{p0, p1};
                Segment s1{p1, p2};
                float angle = (p1 - p0).angle(p2 - p1);

                if (s0.isColinearWith(s1)) {
                    if (infos.back().type != CLASSIFY_TYPE_FILLED &&
                        infos.back().type != CLASSIFY_TYPE_UNKNOW) {
                        infos.emplace_back();
                    }

                    if (angle == 0 || angle == 180 || angle == -180) {
                        infos.back().type = CLASSIFY_TYPE_FILLED;
                        infos.back().indexs.emplace(i - 2);
                        infos.back().indexs.emplace(i - 1);
                        infos.back().indexs.emplace(i);
                    } else {
#ifndef NDEBUG
                        std::cout << "fail classify filled angle!=0&&180 angle:" << angle
                                  << std::endl;
#endif
                        if (infos.back().type == CLASSIFY_TYPE_FILLED &&
                            infos.back().indexs.size()) {
                            infos.emplace_back();
                        }
                        stitches.pop_front();
                    }
                } else {
                    if (infos.back().type == CLASSIFY_TYPE_FILLED && infos.back().indexs.size()) {
                        infos.emplace_back();
                    }
                    stitches.pop_front();
                }

                if (stitches.size()) stitches.pop_front();
            }
        } else {
            if (infos.back().type == CLASSIFY_TYPE_FILLED && infos.back().indexs.size()) {
                infos.emplace_back();
            }
            while (stitches.size() > 1) {
                stitches.pop_front();
            }
        }
    }

    infos.remove_if([](const classifyInfo& info) -> bool { return info.indexs.size() < 3; });

    if (infos.size()) {
        std::vector<classifyInfo> vinfos;
        for (auto& inf : infos) {
            vinfos.push_back(inf);
        }

        for (int i = 0; i < (int)vinfos.size(); i++) {
            auto info = vinfos[i];
            int f = *info.indexs.begin();
            int l = *(--info.indexs.end());
            auto bf = block.polyline[f];
            auto bl = block.polyline[l];

            bool isParallelTo = false;
            Segment cur{bf, bl};
            if (i > 0) {
                auto prev_info = vinfos[i - 1];
                auto prev_bf = block.polyline[*prev_info.indexs.begin()];
                auto prev_bl = block.polyline[*(--prev_info.indexs.end())];
                Segment prev{prev_bf, prev_bl};
                isParallelTo = cur.isParallelTo(prev) && !cur.isColinearWith(prev);
            }

            if (!isParallelTo && i < (int)vinfos.size() - 1) {
                auto next_info = vinfos[i + 1];
                auto next_bf = block.polyline[*next_info.indexs.begin()];
                auto next_bl = block.polyline[*(--next_info.indexs.end())];
                Segment next{next_bf, next_bl};
                isParallelTo = cur.isParallelTo(next) && !cur.isColinearWith(next);
            }

            if (isParallelTo) {
                pesEMBClassifyResult res;
                res.type = CLASSIFY_TYPE_FILLED;
                res.paths.resize(1);
                res.paths[0].setFillColor(block.color);
                res.paths[0].setStrokeColor(block.color);
                res.paths[0].setStrokeWidth(1);
                res.paths[0].setFilled(false);
                res.firstIndex = f;
                res.lastIndex = l;
                res.paths[0].moveTo(bf.x, bf.y);
                res.paths[0].lineTo(bl.x, bl.y);
                results.push_back(res);
            } else {
            }
        }
    }

    return results;
}

std::vector<pesEMBClassifyResult> pesEMBClassify::classify(const pesStitchBlock& block) {
    // (1)classify satin
    std::vector<pesEMBClassifyResult> satinResults = classifySatin(block);

    // (2)for every stitch that not satin => classify it as unknow
    std::vector<pesEMBClassifyResult> unknowResults = classifyUnknow(block, satinResults);

    // (3)classify filled
    std::vector<pesEMBClassifyResult> remainUnknowResults;
    for (auto unknow : unknowResults) {
        std::vector<pesEMBClassifyResult> filledResults = classifyFilled(block, unknow);
        if (filledResults.size() > 2) {
            //            cout << "filled results: " << filledResults.size() << endl;

            // clipper here not allow here!!! why not?
            //            pesPath complexPath;
            //            std::vector<pesPolyline> simplifiedPath;
            //            ClipperLib::ClipType currentClipperType = ClipperLib::ctUnion;
            //            ClipperLib::JoinType currentClipperJoinType = ClipperLib::jtMiter;
            //            ClipperLib::EndType currentClipperEndType  = ClipperLib::etClosedPolygon;
            //            float miterLimit = 2;
            //            float offsetDelta = 1.0;
            //
            //            std::vector<pesPolyline> clipSubjects;
            //            for(auto &r:filledResults){
            //                std::vector<pesPolyline>outlines = r.paths.front().getOutline();
            //                clipSubjects.insert(clipSubjects.end(), outlines.begin(),
            //                outlines.end());
            //            }
            //
            //            pesClipper clipper;
            //            clipper.addPolylines(clipSubjects, ClipperLib::ptSubject, true);
            //
            //            std::vector<pesPolyline> clips = clipper.getClipped(currentClipperType);
            //            std::vector<pesPolyline> offsets = pesClipper::getOffsets(clips,
            //                                                                      offsetDelta,
            //                                                                      currentClipperJoinType,
            //                                                                      currentClipperEndType,
            //                                                                      miterLimit);
            //
            ////            satinResults.insert(satinResults.end(), filledResults.begin(),
            /// filledResults.end());
            //            pesEMBClassifyResult filledResult;
            //            filledResult.type = pesEMBClassifyType::CLASSIFY_TYPE_FILLED;
            //            filledResult.firstIndex = filledResults.front().firstIndex;
            //            filledResult.lastIndex = filledResults.back().lastIndex;
            //            filledResult.paths.resize(1);
            ////            {{ 75, 107, 175}, "Cornflower Blue", "070"},
            //            filledResult.paths.front().setStrokeColor(pesColor(75, 107, 175));
            //            filledResult.paths.front().setStrokeWidth(1.0);
            //            filledResult.paths.front().setFillColor(block.color);
            //            filledResult.paths.front().setFilled(false);
            //            for(auto & offset:offsets){
            //                for(int i=0; i<offset.size(); i++){
            //                    if(i==0){
            //                        filledResult.paths.front().moveTo(offset[i].x, offset[i].y);
            //                    }
            //                    else{
            //                        filledResult.paths.front().lineTo(offset[i].x, offset[i].y);
            //                    }
            //                }
            //                if(offset.isClosed()){
            //                    filledResult.paths.front().close();
            //                }
            //            }
            //            satinResults.push_back(filledResult);

            // check this !!
            satinResults.insert(satinResults.end(), filledResults.begin(), filledResults.end());

            if (filledResults.front().firstIndex > unknow.firstIndex) {
                pesEMBClassifyResult res;
                res.type = pesEMBClassifyType::CLASSIFY_TYPE_UNKNOW;
                res.firstIndex = unknow.firstIndex;
                res.lastIndex = filledResults.front().firstIndex;
                remainUnknowResults.push_back(res);
            }

            for (int i = 1; i < (int)filledResults.size(); i++) {
                if (filledResults[i].firstIndex - filledResults[i - 1].lastIndex > 2) {
                    pesEMBClassifyResult res;
                    res.type = pesEMBClassifyType::CLASSIFY_TYPE_UNKNOW;
                    res.firstIndex = filledResults[i - 1].lastIndex;
                    res.lastIndex = filledResults[i].firstIndex;
                    remainUnknowResults.push_back(res);
                }
            }

            if (filledResults.back().lastIndex < unknow.lastIndex) {
                pesEMBClassifyResult res;
                res.type = pesEMBClassifyType::CLASSIFY_TYPE_UNKNOW;
                res.firstIndex = filledResults.back().lastIndex;
                res.lastIndex = unknow.lastIndex;
                remainUnknowResults.push_back(res);
            }
        } else {
            remainUnknowResults.push_back(unknow);
        }
    }

    // (4)classify running from remaining unknow
    std::vector<pesEMBClassifyResult> runningResults = classifyRunning(block, remainUnknowResults);
    satinResults.insert(satinResults.end(), runningResults.begin(), runningResults.end());

    return satinResults;
}

//#ifdef PES_ENGINE_TEST
void pes_classify_runtest() {
    pesPolyline poly;
    //---->
    poly.addVertex(0, 0);
    poly.lineTo(2.5, 0);
    poly.lineTo(5, 0);
    poly.lineTo(10, 0);

    //<----
    poly.lineTo(10, 2);
    poly.lineTo(5, 2);
    poly.lineTo(2.5, 2);
    poly.lineTo(0, 2);

    bool b = SlopesEqual(poly[0], poly[1], poly[2]);
    b = SlopesEqual(poly[1], poly[2], poly[3]);

    b = SlopesEqual(poly[2], poly[3], poly[4]);

    b = SlopesEqual(poly[4], poly[5], poly[6], poly[7]);

    poly.lineTo(0, 0);
    poly.lineTo(-2, -2);
    poly.lineTo(-5, -5);
    poly.lineTo(-10, -10);
    b = SlopesEqual(poly[8], poly[9], poly[10]);
    b = SlopesEqual(poly[9], poly[10], poly[11]);

    b = IsPointOnLine(poly[8], poly[8], poly[11]);
    b = IsPointOnLine(poly[9], poly[8], poly[11]);
    b = IsPointOnLine(poly[10], poly[8], poly[11]);
    b = IsPointOnLine(poly[11], poly[8], poly[11]);
    b = IsPointOnLine(pesVec2f(-12, -12), poly[8], poly[11]);

    Segment s0{poly[0], poly[3]};
    Segment s1{poly[4], poly[7]};

    b = s0.isParallelTo(s1);
    b = s0.isColinearWith(s1);

    Segment s2{poly[8], poly[9]};
    Segment s3{poly[10], poly[11]};
    Segment s4{poly[11], poly[10]};

    b = s2.isParallelTo(s3);
    b = s2.isColinearWith(s3);
    b = s2.isColinearWith(s4);

    auto v0 = poly[1] - poly[0];
    auto v1 = poly[2] - poly[1];
    float angle = v0.angle(v1);
    auto v2 = poly[1] - poly[2];
    angle = v0.angle(v2);

    auto v3 = poly[1] - poly[2];
    auto v4 = poly[0] - poly[1];
    angle = v3.angle(v4);

    Segment r{poly[0], poly[3]};
    Segment l{poly[4], poly[7]};
    b = r.isParallelTo(l);
    b = r.isColinearWith(l);

#ifndef NDEBUG
    std::cout << "end test" << std::endl;
#endif
}
//#endif
