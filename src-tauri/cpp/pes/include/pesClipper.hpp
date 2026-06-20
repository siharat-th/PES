//
//  pesClipper.hpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 9/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#ifndef pesClipper_hpp
#define pesClipper_hpp

#include <stdio.h>
#include <vector>
#include "clipper.hpp"
#include "pesPath.hpp"

enum pesPolyWindingMode{
    PES_POLY_WINDING_ODD = ClipperLib::pftEvenOdd,
    PES_POLY_WINDING_NONZERO = ClipperLib::pftNonZero,
    PES_POLY_WINDING_POSITIVE = ClipperLib::pftPositive,
    PES_POLY_WINDING_NEGATIVE = ClipperLib::pftNegative,
};

class pesClipper: public ClipperLib::Clipper
{
public:
    pesClipper();
    virtual ~pesClipper() override;

    pesClipper(const pesClipper&) = default;
    pesClipper(pesClipper&&) = default;
    pesClipper& operator=(const pesClipper&) = default;
    pesClipper& operator=(pesClipper&&) = default;      
    
    std::vector<pesPolyline> getClipped(ClipperLib::ClipType clipType,
                                         pesPolyWindingMode subFillType = PES_POLY_WINDING_ODD,
                                         pesPolyWindingMode clipFillType = PES_POLY_WINDING_ODD,
                                         ClipperLib::cInt scale = DEFAULT_CLIPPER_SCALE);
    
//    pesRectangle getBounds(ClipperLib::cInt scale = DEFAULT_CLIPPER_SCALE) const;
    pesRectangle getBounds(ClipperLib::cInt scale = DEFAULT_CLIPPER_SCALE);
    
    bool addRectangle(const pesRectangle& rectangle,
                      ClipperLib::PolyType PolyTyp,
                      bool autoClose = false,
                      ClipperLib::cInt scale = DEFAULT_CLIPPER_SCALE);
    
    bool addPolyline(const pesPolyline& polyline,
                     ClipperLib::PolyType PolyTyp,
                     bool autoClose = false,
                     ClipperLib::cInt scale = DEFAULT_CLIPPER_SCALE);
    
    bool addPolylines(const std::vector<pesPolyline>& polylines,
                      ClipperLib::PolyType PolyTyp,
                      bool autoClose = false,
                      ClipperLib::cInt scale = DEFAULT_CLIPPER_SCALE);
    
    bool addPath(const pesPath& path,
                 ClipperLib::PolyType PolyTyp,
                 bool autoClose = false,
                 ClipperLib::cInt scale = DEFAULT_CLIPPER_SCALE);
    
    static void close(pesPolyline& polyline);
    static void close(std::vector<pesPolyline>& polylines);
    
    static bool areAllClosed(const std::vector<pesPolyline>& polylines);
    
    static ClipperLib::IntPoint toClipper(const pesVec2f& vertex,
                                          ClipperLib::cInt scale);
    
    static ClipperLib::IntRect toClipper(const pesRectangle& rectangle,
                                         ClipperLib::cInt scale);
    
    static ClipperLib::Path toClipper(const pesPolyline& polyline,
                                      ClipperLib::cInt scale);
    
    static ClipperLib::Paths toClipper(const std::vector<pesPolyline>& polylines,
                                       ClipperLib::cInt scale);
    
    static ClipperLib::Paths toClipper(const pesPath& path,
                                       ClipperLib::cInt scale);
    
    static pesRectangle toPes(const ClipperLib::IntRect& rectangle,
                             ClipperLib::cInt scale);
    
    static pesVec2f toPes(const ClipperLib::IntPoint& point,
                        ClipperLib::cInt scale);
    
    static pesPolyline toPes(const ClipperLib::Path& path,
                             bool isClosed,
                             ClipperLib::cInt scale);
    
    static std::vector<pesPolyline> toPes(const ClipperLib::Paths& paths,
                                          bool isClosed,
                                          ClipperLib::cInt scale);
    
    static std::string toString(ClipperLib::ClipType clipType);
    static std::string toString(ClipperLib::PolyType polyType);
    static std::string toString(ClipperLib::PolyFillType polyfillType);
    static std::string toString(ClipperLib::InitOptions initOption);
    static std::string toString(ClipperLib::JoinType joinType);
    static std::string toString(ClipperLib::EndType endType);
    
    static pesPolyWindingMode next(pesPolyWindingMode s);
    static ClipperLib::PolyFillType next(ClipperLib::PolyFillType s);
    static ClipperLib::ClipType next(ClipperLib::ClipType s);
    
    static pesPolyWindingMode toPes(ClipperLib::PolyFillType polyfillType);
    static ClipperLib::PolyFillType toClipper(pesPolyWindingMode windingMode);
    
    static std::vector<pesPolyline> simplifyPolyline(const pesPolyline& polyline,
                                                      pesPolyWindingMode windingMode = PES_POLY_WINDING_ODD,
                                                      ClipperLib::cInt scale = DEFAULT_CLIPPER_SCALE);
    
    static std::vector<pesPolyline> simplifyPolylines(const std::vector<pesPolyline>& polylines,
                                                       pesPolyWindingMode windingMode = PES_POLY_WINDING_ODD,
                                                       ClipperLib::cInt scale = DEFAULT_CLIPPER_SCALE);
    
    static std::vector<pesPolyline> simplifyPath(const pesPath& path,
                                                  pesPolyWindingMode windingMode = PES_POLY_WINDING_ODD,
                                                  ClipperLib::cInt scale = DEFAULT_CLIPPER_SCALE);
    
    static std::vector<pesPolyline> getOffsets(const std::vector<pesPolyline>& polylines,
                                                double offset,
                                                ClipperLib::JoinType jointype = ClipperLib::jtSquare,
                                                ClipperLib::EndType endtype = ClipperLib::etOpenSquare,
                                                double miterLimit = DEFAULT_MITER_LIMIT,
                                                double arcTolerance = DEFAULT_ARC_TOLERANCE,
                                                ClipperLib::cInt scale = DEFAULT_CLIPPER_SCALE);
    
    static std::vector<pesPolyline> getOffsets(const std::vector<pesPath*> & paths,
                                                double offset,
                                                ClipperLib::JoinType jointype = ClipperLib::jtSquare,
                                                ClipperLib::EndType endtype = ClipperLib::etOpenSquare,
                                                double miterLimit = DEFAULT_MITER_LIMIT,
                                                double arcTolerance = DEFAULT_ARC_TOLERANCE,
                                                ClipperLib::cInt scale = DEFAULT_CLIPPER_SCALE);
    
    static std::vector<pesPolyline> getOffsets(const std::vector<pesPath> & paths,
                                                double offset,
                                                ClipperLib::JoinType jointype = ClipperLib::jtSquare,
                                                ClipperLib::EndType endtype = ClipperLib::etOpenSquare,
                                                double miterLimit = DEFAULT_MITER_LIMIT,
                                                double arcTolerance = DEFAULT_ARC_TOLERANCE,
                                                ClipperLib::cInt scale = DEFAULT_CLIPPER_SCALE);
    
    static pesPolyline reversPolyline(const pesPolyline& polyline,
                                       ClipperLib::cInt scale = DEFAULT_CLIPPER_SCALE);
    
    static std::vector<pesPolyline> reversePolylines(const std::vector<pesPolyline>& polylines,
                                                      ClipperLib::cInt scale = DEFAULT_CLIPPER_SCALE);
    
    static const ClipperLib::cInt DEFAULT_CLIPPER_SCALE;
    static const double DEFAULT_MITER_LIMIT;
    static const double DEFAULT_ARC_TOLERANCE;
    
};

#endif /* pesClipper_hpp */
