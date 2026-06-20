//
//  pesEMBFill.hpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 3/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#ifndef pesEMBFill_hpp
#define pesEMBFill_hpp

#include <stdio.h>
#include <queue>
#include <map>
#include "pesPolyline.hpp"
#include "pesPath.hpp"
#include "pesStitchBlock.hpp"

class pesEMBFill{
    
public:
    
    class Line{
    public:
        Line(const pesVec3f& e0, const pesVec3f& e1) : start(e0), end(e1), lineNumber(0) {
            //points.push_back(start);
            //points.push_back(end);
        }

        Line(const pesVec3f& e0, const pesVec3f& e1, int linenumber) : start(e0), end(e1), lineNumber(linenumber) {
            //points.push_back(start);
            //points.push_back(end);
        }

        //to be used in the future
        //Line(const std::vector<pesVec3f> pts, int linenumber) : points(pts), lineNumber(linenumber) {
        //    start = points.front();
        //    end = points.back();
        //}
        //
        //Line(const std::vector<pesVec3f> pts) : points(pts), lineNumber(0) {
        //    start = points.front();
        //    end = points.back();
        //}
        //
        //std::vector<pesVec3f> points;   

        pesVec3f start, end;
        int lineNumber;
    };
    
    typedef struct Preset_{
        std::vector<float> row_stagger; // in mm.
    }Preset;
    
    
    pesEMBFill();

    pesEMBFill(const pesPath& path);
    
    ~pesEMBFill();
    
    void clear();

    void setPesPath(const pesPath& path);

    void setOutline(const pesPolyline& outline);
    
    std::vector<pesPath> createTileGrid(float patternSizeInMM);

    std::deque<std::vector<pesEMBFill::Line>> createSections(float density, float sewDirection, float compensate=0, bool fading=false);

    std::deque<std::vector<pesEMBFill::Line>> createSections2(const float density,
                                                              const float sewDirection,
                                                              const float compensate = 0,
                                                              const bool fading = false);

    std::deque<std::vector<pesEMBFill::Line>> createMFSections(float rowspacingInMM, float sewDirection, float compensate=0);
    
    std::deque<std::vector<pesEMBFill::Line>> createCrossStitchSections(float rowspacingInMM, float sewDirection=0, float compensate=0);
    
    std::vector<pesPolyline> toPatches(const pesEMBFill::Preset & preset, std::vector<bool> & jumps, bool fading=false);

    std::vector<pesPolyline> toPatches(const pesPath & patternResized, std::vector<bool> & jumps, bool fading=false);

    // for FILL_TYPE_NORMAL
    std::vector<pesPolyline> toPatches2(const pesEMBFill::Preset& preset,
                                        std::vector<bool>& jumps,
                                        const bool fading = false);

    // for FILL_TYPE_PATTERN
    std::vector<pesPolyline> toPatches2(const pesPath& patternResized,
                                        std::vector<bool>& jumps,
                                        const bool fading = false);
    
    // experiment
    pesStitchBlock toStitches(const pesEMBFill::Preset& preset);
    pesStitchBlock toStitches(const pesPath& patternResized);
    
    int find_nearest_section(std::deque<std::vector<pesEMBFill::Line>> sections,
                             pesVec2f last_stitch,
                             pesVec2f& start_corner);

    int find_nearest_section2(std::deque<std::vector<pesEMBFill::Line>> sections,
                              pesVec2f last_stitch,
                              pesVec2f& start_corner);

    pesPolyline connect_points(const pesVec2f& p1,
                               const pesVec2f& p2,
                               std::vector<bool>& jumps,
                               const pesPolyline* poutline = nullptr);

    void connect_points_stitches(pesVec2f p1, pesVec2f p2, pesStitchBlock& stitches);

    // FILL_TYPE_NORMAL2 methods (independent from FILL_TYPE_NORMAL)
    std::deque<std::vector<pesEMBFill::Line>> createSections3(
        const float density, const float sewDirection,
        const float compensate = 0, const bool fading = false);

    int find_nearest_section3(
        std::deque<std::vector<pesEMBFill::Line>> sections,
        pesVec2f last_stitch, pesVec2f& start_corner);

    pesStitchBlock toStitchBlock3(const Preset& preset, const bool fading = false);

protected:

    std::vector<std::vector<pesEMBFill::Line>> intersect_region_with_grating(float row_spacing, float angle, float compensate=0, bool fading=false);
    
    std::vector<std::vector<pesEMBFill::Line>> intersect_region_with_grating2(const float row_spacing, const float angle, const float compensate = 0, const bool fading = false);
    
    std::deque<std::vector<pesEMBFill::Line>> pull_runs(std::vector<std::vector<pesEMBFill::Line>> &rows);
    
    std::deque<std::vector<pesEMBFill::Line>> pull_runs2(std::vector<std::vector<pesEMBFill::Line>> &rows);
    
    bool is_same_run(const pesEMBFill::Line &segment1, const pesEMBFill::Line &segment2);
    
    std::vector<pesVec2f> get_corner_points(std::vector<pesEMBFill::Line> section);

    float perimeter_distance(const pesVec2f& p1,
                             const pesVec2f& p2,
                             const pesPolyline* poutline = nullptr);
    
    pesPolyline section_to_patch(std::vector<Line> &group_of_segments, float angle, float row_spacing, std::vector<bool> & jumps);
    pesPolyline section_to_patch(std::vector<Line> &group_of_segments, float angle, float row_spacing, float max_stitch_length, const Preset& preset, std::vector<bool> & jumps);
    pesPolyline section_from_corner( std::vector<pesEMBFill::Line> sections, pesVec2f start_corner, float angle, float row_spacing, std::vector<bool> & jumps);
    pesPolyline section_from_corner( std::vector<pesEMBFill::Line> section, pesVec2f start_corner, float angle, float row_spacing, float max_stitch_length, const Preset& preset, std::vector<bool> & jumps);

    // for FILL_TYPE_NORMAL
    pesPolyline section_to_patch2(const std::vector<Line>& group_of_segments,
                                  const float angle,
                                  const float max_stitch_length,
                                  const Preset& preset,
                                  std::vector<bool>& jumps,
                                  bool swap = false);

    // for FILL_TYPE_PATTERN
    pesPolyline section_to_patch2(const std::vector<Line>& group_of_segments,
                                  std::vector<bool>& jumps,
                                  bool swap = false);
    
    // experiment
    void section_to_patch_stitches(std::vector<Line> &group_of_segments, float angle, float row_spacing, float max_stitch_length, const Preset& preset, pesStitchBlock & stitches);
    void section_from_corner_stitches( std::vector<pesEMBFill::Line> section, pesVec2f start_corner, float angle, float row_spacing, float max_stitch_length, const Preset& preset, pesStitchBlock & stitches);
    void section_to_patch_stitches(std::vector<Line> &group_of_segments, float angle, float row_spacing, pesStitchBlock & stitches);
    void section_from_corner_stitches( std::vector<pesEMBFill::Line> section, pesVec2f start_corner, float angle, float row_spacing, pesStitchBlock & stitches);
    
    std::vector<pesVec2f> intersect(const pesPath & path, pesEMBFill::Line line);
    
private:
    // Graph for FILL_TYPE_NORMAL2 — Dijkstra-based outline travel (Ink/Stitch approach)
    struct OutlineGraph {
        struct Node {
            pesVec2f position;
            std::vector<std::pair<int, float>> neighbors;  // (nodeIndex, weight)
        };
        std::vector<Node> nodes;
        int addNode(const pesVec2f& pos);
        void addEdge(int a, int b, float weight);
        std::vector<int> shortestPath(int source, int target) const;
        float pathLength(const std::vector<int>& path) const;
        int findNearestNode(const pesVec2f& pos) const;
        void clear() { nodes.clear(); }
    };

    // FILL_TYPE_NORMAL2 private methods
    void buildOutlineGraph3();

    void fillSection3(pesStitchBlock& block,
                      const std::vector<Line>& section,
                      float angle, float max_stitch_length,
                      const Preset& preset,
                      bool isFirst, bool swap = false);

    void travelOnGraph3(pesStitchBlock& block,
                        const pesVec2f& from,
                        const pesVec2f& to);

    OutlineGraph _graph3;

    pesPath _patternResized;
    pesPath _shape;
    pesPolyline _shapeOutline;
    std::vector<pesPolyline> _shapeOutlines;
    
    std::vector<pesPath> _grids;
    
    std::deque<std::vector<Line>> _sections;
    float _rowspacing;
    float _sewDirection;
    Preset _preset;
};

namespace pesEMBFillPreset{
    
    pesEMBFill::Preset Traditional();
    pesEMBFill::Preset Brick();
    pesEMBFill::Preset Pillar();
    pesEMBFill::Preset Zigzag();
    pesEMBFill::Preset Wave();
    pesEMBFill::Preset Fan();
    
    pesEMBFill::Preset P1();
    pesEMBFill::Preset P2();
    pesEMBFill::Preset P3();
    pesEMBFill::Preset P4();
    pesEMBFill::Preset P5();
    pesEMBFill::Preset P6();
    pesEMBFill::Preset P7();
    pesEMBFill::Preset P8();
    pesEMBFill::Preset P9();
    pesEMBFill::Preset P10();
    
    enum Preset{
        TRADITIONAL = 0,
        BRICK,
        PILLAR,
        ZIGZAG,
        WAVE,
        FAN,
        
        PATT01,
        PATT02,
        PATT03,
        PATT04,
        PATT05,
        PATT06,
        PATT07,
        PATT08,
        PATT09,
        PATT10
    };
    
    void setDefaultPreset(pesEMBFillPreset::Preset preset);
    pesEMBFill::Preset getDefaultPreset();
}

#endif /* pesEMBFill_hpp */
