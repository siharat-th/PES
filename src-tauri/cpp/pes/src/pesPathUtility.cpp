//
//  pesPathUtility.cpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 30/9/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#include "pesPathUtility.hpp"
#include "skia-ext/pes_skpath_compat.h"

template <typename skVisitFunc>
void skVisitPath(const SkPath& p, skVisitFunc&& f) {
    SkPath::RawIter iter(p);
    SkPoint pts[4];
    SkPath::Verb verb;
    while ((verb = iter.next(pts)) != SkPath::kDone_Verb) {
        f(verb, pts, iter);
    }
}

pesPath toPes(const SkPath& path){
    pesPath p;
    p.fillRule = static_cast<int>(path.getFillType());
    skVisitPath(path, [&p](SkPath::Verb verb, const SkPoint pts[4], SkPath::RawIter iter) {
        switch (verb) {
            case SkPath::kMove_Verb:
                p.moveTo(pts[0].x(), pts[0].y());
                break;
                
            case SkPath::kLine_Verb:
                p.lineTo(pts[1].x(), pts[1].y());
                break;
                
            case SkPath::kQuad_Verb:
            {
                pesVec2f from(pts[0].x(), pts[0].y());
                pesVec2f cp(pts[1].x(), pts[1].y());
                pesVec2f to(pts[2].x(), pts[2].y());
                p.quadBezierTo(from, cp, to);
                break;
            }
                
            case SkPath::kConic_Verb:
            {
                SkPoint quads[5];
                SkPath::ConvertConicToQuads(pts[0], pts[1], pts[2], iter.conicWeight(), quads, 1);
                pesVec2f from(quads[0].x(), quads[0].y());
                pesVec2f cp(quads[1].x(), quads[1].y());
                pesVec2f to(quads[2].x(), quads[2].y());
                p.quadBezierTo(from, cp, to);
                pesVec2f from2(quads[2].x(), quads[2].y());
                pesVec2f cp2(quads[3].x(), quads[3].y());
                pesVec2f to2(quads[4].x(), quads[4].y());
                p.quadBezierTo(from2, cp2, to2);
                break;
            }
                
            case SkPath::kCubic_Verb:
            {
                pesVec2f cp1(pts[1].x(), pts[1].y());
                pesVec2f cp2(pts[2].x(), pts[2].y());
                pesVec2f to(pts[3].x(), pts[3].y());
                p.bezierTo(cp1, cp2, to);
                break;
            }
                
            case SkPath::kClose_Verb:
                p.close();
                break;

            case SkPath::kDone_Verb:
                SkASSERT(false);
                break;
                
            default:
                break;
        }
    });
    
    return p;
}



SkPath toSk(const pesPath& path){
    PesPath p;
    p.setFillType(static_cast<SkPathFillType>(path.fillRule));
    for (const auto &c : path.getCommands()) {
        switch (c.type) {
            case pesPath::Command::Type::_moveTo:
                p.moveTo(c.to.x, c.to.y);
                break;
                
            case pesPath::Command::Type::_lineTo:
                p.lineTo(c.to.x, c.to.y);
                break;
                
            case pesPath::Command::Type::_quadBezierTo:
                p.quadTo(c.cp2.x, c.cp2.y, c.to.x, c.to.y);
                break;
                
            case pesPath::Command::Type::_bezierTo:
                p.cubicTo(c.cp1.x, c.cp1.y, c.cp2.x, c.cp2.y, c.to.x, c.to.y);
                break;
                
            case pesPath::Command::Type::_close:
                p.close();
                break;
                
            default:
                break;
        }
    }
    
    return p;
}
