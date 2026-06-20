//
//  pesClipper.cpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 9/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#include "pesClipper.hpp"

using namespace std;

const ClipperLib::cInt pesClipper::DEFAULT_CLIPPER_SCALE = 1000000000;
const double pesClipper::DEFAULT_MITER_LIMIT = 2; //3
const double pesClipper::DEFAULT_ARC_TOLERANCE = 0.25;

pesClipper::pesClipper()
{
}


pesClipper::~pesClipper()
{
}


pesRectangle pesClipper::getBounds(ClipperLib::cInt scale)
{
    return toPes(GetBounds(), scale);
}


std::vector<pesPolyline> pesClipper::getClipped(ClipperLib::ClipType clipType,
                                              pesPolyWindingMode subFillType,
                                              pesPolyWindingMode clipFillType,
                                              ClipperLib::cInt scale)
{
    std::vector<pesPolyline> results;
    
//    try
//    {
        ClipperLib::Paths out;
        
        bool success = Execute(clipType,
                               out,
                               toClipper(subFillType),
                               toClipper(clipFillType));
        
        results = toPes(out, true, scale);
        
        if (!success)
        {
//            GUI_LogError("pesClipper::getClipped") << "Failed to create clipped paths.";
        }
//    }
//    catch (const std::exception& exc)
//    {
////        GUI_LogError("pesClipper::getClipped") << exc.what();
//    }
    
    return results;
}


bool pesClipper::addRectangle(const pesRectangle& rectangle,
                           ClipperLib::PolyType PolyTyp,
                           bool autoClose,
                           ClipperLib::cInt scale)
{
    pesPolyline p;
    p.fromRect(rectangle);
    return addPolyline(p,
                       PolyTyp,
                       autoClose,
                       scale);
}


bool pesClipper::addPolyline(const pesPolyline& polyline,
                          ClipperLib::PolyType PolyTyp,
                          bool autoClose,
                          ClipperLib::cInt scale)
{
    auto _polyline = polyline;
    if (autoClose) close(_polyline);
    return AddPath(toClipper(_polyline, scale), PolyTyp, _polyline.isClosed());
}

bool pesClipper::addPolylines(const std::vector<pesPolyline>& polylines,
                           ClipperLib::PolyType PolyTyp,
                           bool autoClose,
                           ClipperLib::cInt scale)
{
    auto _polylines = polylines;
    
    if (autoClose)
    {
        close(_polylines);
    }
    
    bool isClosed = autoClose ? true : areAllClosed(_polylines);
    
    return AddPaths(toClipper(_polylines, scale), PolyTyp, isClosed);
}


bool pesClipper::addPath(const pesPath& paths,
                      ClipperLib::PolyType PolyTyp,
                      bool autoClose,
                      ClipperLib::cInt scale)
{
    return addPolylines(paths.getOutline(), PolyTyp, autoClose, scale);
}


void pesClipper::close(pesPolyline& polyline)
{
    polyline.close();
}


void pesClipper::close(std::vector<pesPolyline>& polylines)
{
    for (auto& polyline: polylines) close(polyline);
}


bool pesClipper::areAllClosed(const std::vector<pesPolyline>& polylines)
{
    for (auto& polyline: polylines) if (!polyline.isClosed()) return false;
    return true;
}


ClipperLib::IntRect pesClipper::toClipper(const pesRectangle& rectangle,
                                       ClipperLib::cInt scale)
{
    ClipperLib::IntRect rect;
    rect.left = rectangle.getMinX() * scale;
    rect.right = rectangle.getMaxX() * scale;
    rect.top = rectangle.getMinY() * scale;
    rect.bottom = rectangle.getMaxY() * scale;
    return rect;
}


ClipperLib::IntPoint pesClipper::toClipper(const pesVec2f& vertex,
                                        ClipperLib::cInt scale)
{
    return ClipperLib::IntPoint(vertex.x * scale, vertex.y * scale);
}


ClipperLib::Path pesClipper::toClipper(const pesPolyline& polyline,
                                    ClipperLib::cInt scale)
{
    ClipperLib::Path path;
    
    for (auto& vertex: polyline.getVertices())
    {
        path.push_back(toClipper(vertex, scale));
    }
    
    return path;
}


ClipperLib::Paths pesClipper::toClipper(const std::vector<pesPolyline>& polylines,
                                     ClipperLib::cInt scale)
{
    ClipperLib::Paths paths;
    for (auto& polyline: polylines) paths.push_back(toClipper(polyline, scale));
    return paths;
}


ClipperLib::Paths pesClipper::toClipper(const pesPath& path, ClipperLib::cInt scale)
{
    return toClipper(path.getOutline(), scale);
}


pesVec2f pesClipper::toPes(const ClipperLib::IntPoint& point,
                      ClipperLib::cInt scale)
{
    pesVec2f vertex;
    vertex.x = point.X / scale;
    vertex.y = point.Y / scale;
    return vertex;
}


pesRectangle pesClipper::toPes(const ClipperLib::IntRect& rectangle, ClipperLib::cInt scale)
{
    return pesRectangle(rectangle.left / scale,
                        rectangle.top / scale,
                        (rectangle.right - rectangle.left) / scale,
                        (rectangle.bottom - rectangle.top) / scale);
}


pesPolyline pesClipper::toPes(const ClipperLib::Path& path,
                           bool isClosed,
                           ClipperLib::cInt scale)
{
    pesPolyline polyline;
    for (auto& point: path) polyline.addVertex(toPes(point, scale));
    if (isClosed) polyline.close();
    return polyline;
}


std::vector<pesPolyline> pesClipper::toPes(const ClipperLib::Paths& paths,
                                        bool isClosed,
                                        ClipperLib::cInt scale)
{
    std::vector<pesPolyline> results;
    for (auto& path: paths) results.push_back(toPes(path, isClosed, scale));
    return results;
}


std::string pesClipper::toString(ClipperLib::ClipType clipType)
{
    switch (clipType)
    {
        case ClipperLib::ctIntersection: return "ctIntersection";
        case ClipperLib::ctUnion: return "ctUnion";
        case ClipperLib::ctDifference: return "ctDifference";
        case ClipperLib::ctXor: return "ctXor";
    }
    
    return "UNKNOWN";
}


std::string pesClipper::toString(ClipperLib::PolyType polyType)
{
    switch (polyType)
    {
        case ClipperLib::ptSubject: return "ptSubject";
        case ClipperLib::ptClip: return "ptClip";
    }
    
    return "UNKNOWN";
}


std::string pesClipper::toString(ClipperLib::PolyFillType polyfillType)
{
    switch (polyfillType)
    {
        case ClipperLib::pftEvenOdd: return "pftEvenOdd";
        case ClipperLib::pftNonZero: return "pftNonZero";
        case ClipperLib::pftPositive: return "pftPositive";
        case ClipperLib::pftNegative: return "pftNegative";
    }
    
    return "UNKNOWN";
}


std::string pesClipper::toString(ClipperLib::InitOptions initOption)
{
    switch (initOption)
    {
        case ClipperLib::ioReverseSolution: return "ioReverseSolution";
        case ClipperLib::ioStrictlySimple: return "ioStrictlySimple";
        case ClipperLib::ioPreserveCollinear: return "ioPreserveCollinear";
    }
    
    return "UNKNOWN";
}


std::string pesClipper::toString(ClipperLib::JoinType joinType)
{
    switch (joinType)
    {
        case ClipperLib::jtSquare: return "jtSquare";
        case ClipperLib::jtRound: return "jtRound";
        case ClipperLib::jtMiter: return "jtMiter";
    }
    
    return "UNKNOWN";
}


std::string pesClipper::toString(ClipperLib::EndType endType)
{
    switch (endType)
    {
        case ClipperLib::etClosedPolygon: return "etClosedPolygon";
        case ClipperLib::etClosedLine: return "etClosedLine";
        case ClipperLib::etOpenButt: return "etOpenButt";
        case ClipperLib::etOpenSquare: return "etOpenSquare";
        case ClipperLib::etOpenRound: return "etOpenRound";
    }
    
    return "UNKNOWN";
}


pesPolyWindingMode pesClipper::next(pesPolyWindingMode s)
{
    switch (s)
    {
        case PES_POLY_WINDING_ODD: return PES_POLY_WINDING_NONZERO;
        case PES_POLY_WINDING_NONZERO: return PES_POLY_WINDING_POSITIVE;
        case PES_POLY_WINDING_POSITIVE: return PES_POLY_WINDING_NEGATIVE;
        case PES_POLY_WINDING_NEGATIVE: return PES_POLY_WINDING_ODD;
    }
    
    return PES_POLY_WINDING_NONZERO;
}


ClipperLib::PolyFillType pesClipper::next(ClipperLib::PolyFillType s)
{
    switch (s)
    {
        case ClipperLib::pftEvenOdd: return ClipperLib::pftNonZero;
        case ClipperLib::pftNonZero: return ClipperLib::pftPositive;
        case ClipperLib::pftPositive: return ClipperLib::pftNegative;
        case ClipperLib::pftNegative: return ClipperLib::pftEvenOdd;
    }
    
    return ClipperLib::pftEvenOdd;
}


ClipperLib::ClipType pesClipper::next(ClipperLib::ClipType s)
{
    switch (s)
    {
        case ClipperLib::ctIntersection: return ClipperLib::ctUnion;
        case ClipperLib::ctUnion: return ClipperLib::ctDifference;
        case ClipperLib::ctDifference: return ClipperLib::ctXor;
        case ClipperLib::ctXor: return ClipperLib::ctIntersection;
    }
    
    return ClipperLib::ctIntersection;
}


pesPolyWindingMode pesClipper::toPes(ClipperLib::PolyFillType polyfillType)
{
    switch (polyfillType)
    {
        case ClipperLib::pftEvenOdd: return PES_POLY_WINDING_ODD;
        case ClipperLib::pftNonZero: return PES_POLY_WINDING_NONZERO;
        case ClipperLib::pftPositive: return PES_POLY_WINDING_POSITIVE;
        case ClipperLib::pftNegative: return PES_POLY_WINDING_NEGATIVE;
    }
    
    return PES_POLY_WINDING_ODD;
}


ClipperLib::PolyFillType pesClipper::toClipper(pesPolyWindingMode windingMode)
{
    switch (windingMode)
    {
        case PES_POLY_WINDING_ODD: return ClipperLib::pftEvenOdd;
        case PES_POLY_WINDING_NONZERO: return ClipperLib::pftNonZero;
        case PES_POLY_WINDING_POSITIVE: return ClipperLib::pftPositive;
        case PES_POLY_WINDING_NEGATIVE: return ClipperLib::pftNegative;
    }
    
    return ClipperLib::pftEvenOdd;
}




std::vector<pesPolyline> pesClipper::simplifyPolyline(const pesPolyline& polyline,
                                                    pesPolyWindingMode windingMode,
                                                    ClipperLib::cInt scale)
{
    ClipperLib::Paths out;
    ClipperLib::SimplifyPolygon(toClipper(polyline, scale),
                                out,
                                toClipper(windingMode));
    return toPes(out, true, scale);
}

std::vector<pesPolyline> pesClipper::simplifyPolylines(const std::vector<pesPolyline>& polylines,
                                                     pesPolyWindingMode windingMode,
                                                     ClipperLib::cInt scale)
{
    ClipperLib::Paths out;
    ClipperLib::SimplifyPolygons(toClipper(polylines, scale),
                                 out,
                                 toClipper(windingMode));
    return toPes(out, true, scale);
}

std::vector<pesPolyline> pesClipper::simplifyPath(const pesPath& path,
                                                pesPolyWindingMode windingMode,
                                                ClipperLib::cInt scale)
{
    ClipperLib::Paths out;
    ClipperLib::SimplifyPolygons(toClipper(path, scale),
                                 out,
                                 toClipper(windingMode));
    
    return toPes(out, true, scale);
}


std::vector<pesPolyline> pesClipper::getOffsets(const std::vector<pesPolyline>& polylines,
                                              double offset,
                                              ClipperLib::JoinType jointype,
                                              ClipperLib::EndType endtype,
                                              double miterLimit,
                                              double arcTolerance,
                                              ClipperLib::cInt scale)
{
    offset *= scale;
    miterLimit *= scale;
    arcTolerance *= scale;
    
    ClipperLib::Paths out;
    
    ClipperLib::ClipperOffset clipperOffset(miterLimit, arcTolerance);
    clipperOffset.AddPaths(toClipper(polylines, scale), jointype, endtype);
    clipperOffset.Execute(out, offset);
    
    return toPes(out, true, scale);
}

std::vector<pesPolyline> pesClipper::getOffsets(const std::vector<pesPath*> & paths,
                                              double offset,
                                              ClipperLib::JoinType jointype,
                                              ClipperLib::EndType endtype,
                                              double miterLimit,
                                              double arcTolerance,
                                              ClipperLib::cInt scale)
{
    vector<pesPolyline> clipSubjects;
    // Fixed by Github Copilot - GPT 4o
    for(const auto p:paths){
        // setStrokeWidth = 1 so the path hasOutline()=true, getOutline() will return tessellatedContour.
        p->setStrokeWidth(1);
        
        vector<pesPolyline> outlines = p->getOutline();
        clipSubjects.insert(clipSubjects.end(), outlines.begin(), outlines.end());
    }
    
    pesClipper clipper;
    clipper.addPolylines(clipSubjects, ClipperLib::ptSubject, true);
    
    return getOffsets(clipper.getClipped(ClipperLib::ctUnion), offset, jointype, endtype, miterLimit, arcTolerance, scale);
}

std::vector<pesPolyline> pesClipper::getOffsets(const std::vector<pesPath> & paths,
                                              double offset,
                                              ClipperLib::JoinType jointype,
                                              ClipperLib::EndType endtype,
                                              double miterLimit,
                                              double arcTolerance,
                                              ClipperLib::cInt scale)
{
    vector<pesPolyline> clipSubjects;
    for(auto p:paths){
        // setStrokeWidth = 1 so the path hasOutline()=true, getOutline() will return tessellatedContour.
        p.setStrokeWidth(1);
        
        vector<pesPolyline> outlines = p.getOutline();
        clipSubjects.insert(clipSubjects.end(), outlines.begin(), outlines.end());
    }
    
    pesClipper clipper;
    clipper.addPolylines(clipSubjects, ClipperLib::ptSubject, true);
    
    return getOffsets(clipper.getClipped(ClipperLib::ctUnion), offset, jointype, endtype, miterLimit, arcTolerance, scale);
}


pesPolyline pesClipper::reversPolyline(const pesPolyline& polyline,
                                     ClipperLib::cInt scale)
{
    ClipperLib::Path p = toClipper(polyline, scale);
    ClipperLib::ReversePath(p);
    return toPes(p, true, scale);
}


std::vector<pesPolyline> pesClipper::reversePolylines(const std::vector<pesPolyline>& polylines,
                                                    ClipperLib::cInt scale)
{
    ClipperLib::Paths p = toClipper(polylines, scale);
    ClipperLib::ReversePaths(p);
    return toPes(p, true, scale);
}
