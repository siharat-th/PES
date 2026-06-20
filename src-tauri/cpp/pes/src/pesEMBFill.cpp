//
//  pesEMBFill.cpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 3/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#include "pesEMBFill.hpp"
#include "pesUtility.hpp"
#include "pesDocument.hpp"
#include <pesPathUtility.hpp>
#include <include/pathops/SkPathOps.h>

using namespace std;

#ifndef M_PI_2
//#define M_PI_2 3.14159265358979323846
#define M_PI_2 1.57079632679489661923132169164
#endif


pesEMBFill::pesEMBFill(){
    _sections.clear();
    _rowspacing = 10.0 / 2.5;
    _sewDirection = 0;
}

pesEMBFill::pesEMBFill(const pesPath& path)
:_shape(path)
{
    _sections.clear();
    _rowspacing = 10.0 / 2.5;
    _sewDirection = 0;
}

pesEMBFill::~pesEMBFill(){
    clear();
}

void pesEMBFill::clear(){
    _shape.clear();
    _shapeOutline.clear();
    _patternResized.clear();
    _grids.clear();
    _sections.clear();
}

void pesEMBFill::setPesPath(const pesPath& path) {
    clear();
    _shape = path;
}

void pesEMBFill::setOutline(const pesPolyline& outline) {
    //_shapeOutline.clear();
    //_shapeOutline.addVertices(outline.getVertices());
    //_shapeOutline.close();
    //_shapeOutline.simplify();
    _shapeOutline = outline;
}

vector<pesPath> pesEMBFill::createTileGrid(float patternSizeInMM){
    // Suggest by Github Copilot - GPT 4o
    vector<pesPath> gridPaths;
    if (_shape.getOutline().empty()) {
        return gridPaths;
    }

    int ppmm = 10;
    if(_shape.getOutline().size()){
        float gridSize = patternSizeInMM * ppmm;
        pesRectangle bound = _shape.getBoundingBox();
        
        pesVec2f direction(1, 0);
        //        direction.rotate(_param.patternDirection);
        
        pesVec2f normal = direction;
        normal.rotateRad((float)M_PI_2);
        
        pesVec2f center = bound.getCenter();
        pesPath dummy;
        dummy.setMode(pesPath::Mode::POLYLINES);
        dummy.rectangle(bound.x, bound.y, bound.width, bound.height);
        dummy.translate(-center.x, -center.y);
        //        dummy.rotate(_param.patternDirection, ofVec3f(0,0,1));
        dummy.translate(center.x, center.y);
        pesRectangle newBound = dummy.getOutline()[0].getBoundingBox();
        
        float start = newBound.getMinY();
        start -= center.y;
        
        float startx = newBound.getMinX();
        startx -= center.x;
        
        pesVec2f lt = center + normal*start - direction*fabs(startx);
        pesVec2f rt = center + normal*start + direction*fabs(startx);
        pesVec2f lb = center - normal*start - direction*fabs(startx);
//        pesVec2f rb = center - normal*start + direction*fabs(startx);
        
        pesPath grid;
        pesVec2f p(0,0);
        grid.moveTo(p.x, p.y);
        p+=direction*gridSize;
        grid.lineTo(p.x, p.y);
        p+=normal*gridSize;
        grid.lineTo(p.x, p.y);
        p-=direction*gridSize;
        grid.lineTo(p.x, p.y);
        grid.close();
        
        _grids.clear();
        
        float w = lt.distance(rt);
        float h = lt.distance(lb);
        int numcol = (int)ceil(w / gridSize);
        int numrow = (int)ceil(h / gridSize);
        
        grid.translate(lt.x, lt.y);
        for(int row=0;row<numrow;row++){
            for(int col=0;col<numcol;col++){
                pesPath g = grid;
                pesVec2f t(0,0);
                t+=direction*(gridSize*col);
                t+=normal*(gridSize*row);
                g.translate(t.x, t.y);
                _grids.push_back(g);
            }
        }
    }

    return _grids;
}

deque<vector<pesEMBFill::Line>> pesEMBFill::createSections(float density, float sewDirection, float compensate, bool fading){
    _rowspacing = 10.0f / density;
    _sewDirection = sewDirection;
    vector<vector<pesEMBFill::Line>> rows_of_segments = intersect_region_with_grating(_rowspacing, sewDirection, compensate, fading);
    _sections = pull_runs(rows_of_segments);
    return _sections;
}

std::deque<vector<pesEMBFill::Line>> pesEMBFill::createSections2(const float density,
                                                                 const float sewDirection,
                                                                 const float compensate,
                                                                 const bool fading) {
    _rowspacing = 10.0f / density;
    _sewDirection = sewDirection;
    std::vector<std::vector<pesEMBFill::Line>> rows_of_segments = intersect_region_with_grating2(_rowspacing, sewDirection, compensate, fading);
    _sections = pull_runs2(rows_of_segments);
    return _sections;
}

std::deque<vector<pesEMBFill::Line>> pesEMBFill::createMFSections(float rowspacingInMM,
                                                                  float sewDirection,
                                                                  float compensate) {
    _rowspacing = rowspacingInMM * 10;
    _sewDirection = sewDirection;
    vector<vector<pesEMBFill::Line>> rows_of_segments = intersect_region_with_grating(_rowspacing, sewDirection, compensate);
    _sections = pull_runs(rows_of_segments);
    return _sections;
}

float snapToGrid(float v, float gridSize){
    // https://stackoverflow.com/questions/12136032/align-object-position-to-grid
    return std::floor(v/gridSize) * gridSize;
}

deque<vector<pesEMBFill::Line>> pesEMBFill::createCrossStitchSections(float rowspacingInMM, float sewDirection, float compensate){
    _rowspacing = rowspacingInMM * 10;
    _sewDirection = sewDirection;
    vector<vector<pesEMBFill::Line>> rows_of_segments = intersect_region_with_grating(_rowspacing, 0, compensate);
    _sections = pull_runs(rows_of_segments);
    
    // snap to grid
    for(auto & section : _sections){
        for(auto & line : section){
            line.start.x = snapToGrid(line.start.x, _rowspacing);
//            line.start.y = snapToGrid(line.start.y, _rowspacing);
            line.end.x = snapToGrid(line.end.x, _rowspacing);
//            line.end.y = snapToGrid(line.end.y, _rowspacing);
        }
    }
    
    return _sections;
}

vector<pesPolyline> pesEMBFill::toPatches(const pesEMBFill::Preset & preset, std::vector<bool> & jumps, bool fading){
    if(_sections.empty())
        createSections(10.0f/_rowspacing, _sewDirection, fading);
    pesVec2f _lastStitch(0,0);
    _preset = preset;
    float max_stitch_length = 5 * 10.0;
    
    vector<pesPolyline> patches;
    auto copy = _sections;
    
    while(_sections.size()){
        if(_lastStitch.length()>0){
            pesVec2f start_corner;
            int section_index = find_nearest_section(_sections, _lastStitch, start_corner);

            patches.push_back( connect_points(_lastStitch, start_corner, jumps));
            
            vector<Line> section = _sections.at(section_index);
            patches.push_back( section_from_corner( section, start_corner, _sewDirection, _rowspacing, max_stitch_length, preset, jumps));
            _sections.erase(_sections.begin()+section_index);
        }
        else{
            vector<Line> section = _sections.front();
            patches.push_back(section_to_patch(section, _sewDirection, _rowspacing, max_stitch_length, preset, jumps));
            _sections.pop_front();
        }
        
        // pesVec2f last = patches.back().getVertices().back();
        // _lastStitch = last;
        // Suggest by Github Copilot - GPT 4o
        _lastStitch = patches.back().getVertices().back();
    }
    
    _sections = copy;
    copy.clear();
    return patches;
}

vector<pesPolyline> pesEMBFill::toPatches(const pesPath & patternResized, std::vector<bool> & jumps, bool fading){
    if(_sections.empty())
        createSections(10.0f/_rowspacing, _sewDirection, fading);
    pesVec2f _lastStitch(0,0);
    _patternResized = patternResized;
    
    std::vector<pesPolyline> patches;
    
    std::deque<std::vector<pesEMBFill::Line>> copy = _sections;

    while(_sections.size()){
        if(_lastStitch.length()>0){
            pesVec2f start_corner;
            int section_index = find_nearest_section(_sections, _lastStitch, start_corner);
            patches.push_back( connect_points(_lastStitch, start_corner, jumps));
            
            std::vector<Line> section = _sections.at(section_index);
            patches.push_back( section_from_corner( section, start_corner, _sewDirection, _rowspacing, jumps));
            _sections.erase(_sections.begin()+section_index);
        }
        else{
            std::vector<Line> section = _sections.front();
            patches.push_back(section_to_patch(section, _sewDirection, _rowspacing, jumps));
            _sections.pop_front();
        }
        
        // pesVec2f last = patches.back().getVertices().back();
        // _lastStitch = last;
        // Suggest by Github Copilot - GPT 4o
        _lastStitch = patches.back().getVertices().back();
    }
    
    _sections = copy;
    copy.clear();
    return patches;
}


// for toPatches2 FILL_TYPE_NORMAL & FILL_TYPE_PATTERN
pesVec3f last_e_section_to_patch2(0, 0);
pesVec3f last_s_section_to_patch2(0, 0);

// for FILL_TYPE_NORMAL
std::vector<pesPolyline> pesEMBFill::toPatches2(const pesEMBFill::Preset& preset,
                                           std::vector<bool>& jumps,
                                           const bool fading) {
    if (_sections.empty()) {
        createSections2(10.0f / _rowspacing, _sewDirection, fading);
    }

    const float max_stitch_length = 5.0f * 10.0f;
    _preset = preset;
    
    std::vector<pesPolyline> patches;
    std::deque<std::vector<pesEMBFill::Line>> copy = _sections;
    pesVec2f lastStitch(0, 0);
    
    // first section
    if (_sections.size()) {
        auto& section = _sections.front();
        patches.push_back(section_to_patch2(section, _sewDirection, max_stitch_length, preset, jumps));
        _sections.pop_front();
        lastStitch = patches.back().getVertices().back();
    }

    // next sections
    while (_sections.size()) {
        pesVec2f start_corner;
        int section_index = find_nearest_section2(_sections, lastStitch, start_corner);
        if (section_index == -1) {
            break;
        }

        int id = abs(last_e_section_to_patch2.z) - 1;
        if (0 <= id && id < _shapeOutlines.size()) {
            const auto& outline = _shapeOutlines[id];
            //SkDebugf("outline-id: %d\n", id);
            patches.push_back(connect_points(lastStitch, start_corner, jumps, &outline));
        }
        else {
            patches.push_back(connect_points(lastStitch, start_corner, jumps));
        }

        auto& section = _sections.at(section_index);
        if (!start_corner.match(section[0].start, 1) && !start_corner.match(section[0].end, 1)) {
            std::reverse(section.begin(), section.end());
        }

        bool swap = !start_corner.match(section[0].start, 1);
        patches.push_back(section_to_patch2(section, _sewDirection, max_stitch_length, preset, jumps, swap));
        _sections.erase(_sections.begin() + section_index);      
        lastStitch = patches.back().getVertices().back();
    }
    
    _sections = copy;
    copy.clear();
    return patches;
}

// for FILL_TYPE_PATTERN
std::vector<pesPolyline> pesEMBFill::toPatches2(const pesPath& patternResized,
                                           std::vector<bool>& jumps,
                                           const bool fading) {
    if (_sections.empty()) {
        createSections2(10.0f / _rowspacing, _sewDirection, fading);
    }
    _patternResized = patternResized;
    
    std::vector<pesPolyline> patches;
    std::deque<std::vector<pesEMBFill::Line>> copy = _sections;
    pesVec2f lastStitch(0, 0);

    // first section
    if (_sections.size()) {
        auto& section = _sections.front();
        patches.push_back(section_to_patch2(section, jumps));
        _sections.pop_front();
        lastStitch = patches.back().getVertices().back();
    }

    // next sections
    while (_sections.size()) {
        pesVec2f start_corner;
        int section_index = find_nearest_section2(_sections, lastStitch, start_corner);
        if (section_index == -1) {
            break;
        }

        int id = abs(last_e_section_to_patch2.z) - 1;
        if (0 <= id && id < _shapeOutlines.size()) {
            const auto& outline = _shapeOutlines[id];
            // SkDebugf("outline-id: %d\n", id);
            patches.push_back(connect_points(lastStitch, start_corner, jumps, &outline));
        } 
        else {
            patches.push_back(connect_points(lastStitch, start_corner, jumps));
        }
            
        auto& section = _sections.at(section_index);
        if (!start_corner.match(section[0].start, 1) && !start_corner.match(section[0].end, 1)) {
            std::reverse(section.begin(), section.end());
        }

        bool swap = !start_corner.match(section[0].start, 1);
        patches.push_back(section_to_patch2(section, jumps, swap));
        _sections.erase(_sections.begin() + section_index);
        lastStitch = patches.back().getVertices().back();
    }
    
    _sections = copy;
    copy.clear();
    return patches;
}

pesStitchBlock pesEMBFill::toStitches(const pesEMBFill::Preset & preset){
    if(_sections.empty())
        createSections(10.0f/_rowspacing, _sewDirection);
    pesVec2f _lastStitch(0,0);
    _preset = preset;
    float max_stitch_length = 5 * 10.0;
    
    pesStitchBlock stitches;
    
    deque<vector<pesEMBFill::Line>> copy = _sections;
    
    while(_sections.size()){
        if(_lastStitch.length()>0){
            pesVec2f start_corner;
            int section_index = find_nearest_section(_sections, _lastStitch, start_corner);
            
//            patches.push_back( connect_points(_lastStitch, start_corner));
            connect_points_stitches(_lastStitch, start_corner, stitches);
            
            vector<Line> section = _sections.at(section_index);
//            patches.push_back( section_from_corner( section, start_corner, _sewDirection, _rowspacing, max_stitch_length, preset));
            section_from_corner_stitches( section, start_corner, _sewDirection, _rowspacing, max_stitch_length, preset, stitches);
            _sections.erase(_sections.begin()+section_index);
        }
        else{
            vector<Line> section = _sections.front();
//            patches.push_back(section_to_patch(section, _sewDirection, _rowspacing, max_stitch_length, preset));
            section_to_patch_stitches(section, _sewDirection, _rowspacing, max_stitch_length, preset, stitches);
            _sections.pop_front();
        }
        
        pesVec2f last = stitches.polyline.getVertices().back();
        _lastStitch = last;
    }
    
    _sections = copy;
    copy.clear();
    return stitches;
}

pesStitchBlock pesEMBFill::toStitches(const pesPath & patternResized){
    if(_sections.empty())
        createSections(10.0f/_rowspacing, _sewDirection);
    pesVec2f _lastStitch(0,0);
    _patternResized = patternResized;
    
//    std::vector<pesPolyline> patches;
    pesStitchBlock stitches;
    
    std::deque<std::vector<pesEMBFill::Line>> copy = _sections;

    while(_sections.size()){
        if(_lastStitch.length()>0){
            pesVec2f start_corner;
            int section_index = find_nearest_section(_sections, _lastStitch, start_corner);
//            patches.push_back( connect_points(_lastStitch, start_corner));
            connect_points_stitches(_lastStitch, start_corner, stitches);
            
            std::vector<pesEMBFill::Line> section = _sections.at(section_index);
//            patches.push_back( section_from_corner( section, start_corner, _sewDirection, _rowspacing));
            section_from_corner_stitches( section, start_corner, _sewDirection, _rowspacing, stitches);
            _sections.erase(_sections.begin()+section_index);
        }
        else{
            std::vector<Line> section = _sections.front();
//            patches.push_back(section_to_patch(section, _sewDirection, _rowspacing));
            section_to_patch_stitches(section, _sewDirection, _rowspacing, stitches);
            _sections.pop_front();
        }
        
//        pesVec2f last = patches.back().getVertices().back();
        pesVec2f last = stitches.polyline.getVertices().back();
        _lastStitch = last;
    }
    
    _sections = copy;
    copy.clear();
//    return patches;
    return stitches;
}

int pesEMBFill::find_nearest_section(deque<vector<pesEMBFill::Line>> sections, pesVec2f last_stitch, pesVec2f &start_corner){
    int nearestCorner = -1;
    int nearestSection = -1;
    float mindist = FLT_MAX;
    for (int i = 0, ii = (int)sections.size(); i < ii; i++) {
        std::vector<pesEMBFill::Line> section = sections[i];
        std::vector<pesVec2f> corners = get_corner_points(section);
        float dist[4];
        dist[0] = fabs(perimeter_distance(corners[0], last_stitch));
        dist[1] = fabs(perimeter_distance(corners[1], last_stitch));
        dist[2] = fabs(perimeter_distance(corners[2], last_stitch));
        dist[3] = fabs(perimeter_distance(corners[3], last_stitch));
        
        for (int j = 0; j < 4; j++) {
            if (dist[j] < mindist) {
                mindist = dist[j];
                nearestSection = i;
                nearestCorner = j;
                start_corner = corners[nearestCorner];
            }
        }
    }
    
    //start_corner = get_corner_points(sections[nearestSection])[nearestCorner];
    return nearestSection;
}

int pesEMBFill::find_nearest_section2(std::deque<std::vector<pesEMBFill::Line>> sections,
                                      pesVec2f last_stitch,
                                      pesVec2f& start_corner) {
    float mindist = FLT_MAX;
    int nearestSection = -1;
    int section_count = -1;
    for (const auto& section : sections) {
        ++section_count;
        if (section.size() == 0) {
            continue;
        }
        if (section.size() == 1) {
            for (const auto& corner : {section.front().start, section.front().end}) {
                const auto dist = last_stitch.distance(corner);
                if (mindist > dist) {
                    mindist = dist;
                    nearestSection = section_count;
                    start_corner = corner;
                }
            }
        }
        else {
            for (const auto& corner : {section.front().start,
                                      section.front().end,
                                      section.back().start,
                                      section.back().end}) {
                const auto dist = last_stitch.distance(corner);
                if (mindist > dist) {
                    mindist = dist;
                    nearestSection = section_count;
                    start_corner = corner;
                }
            }
        } 
    }

    return nearestSection;
}

pesPolyline pesEMBFill::connect_points(const pesVec2f& p1,
                                       const pesVec2f& p2,
                                       std::vector<bool>& jumps,
                                       const pesPolyline* poutline) {
    // NooM >>>
    //const auto& outline = _shapeOutline;
    const auto& outline = poutline != nullptr ? *poutline : _shapeOutline;
    // NooM <<<
    
    const float ppmm = 10;
    const float running_stitch_length = 1.0f * ppmm;
    
    pesPolyline patch;

    unsigned int nearestIndex;
    outline.getClosestPoint(p1, &nearestIndex);
    float fpos = outline.getLengthAtIndex(nearestIndex);
    
    const float distance = perimeter_distance(p1, p2, &outline);
    const int stitches = abs(int(distance / running_stitch_length));
    const float direction = copysign(1.0f, distance);
    const float one_stitch = running_stitch_length * direction;
    const float outline_length = outline.getPerimeter();
    for (int i = 0; i < stitches; i++) {
        fpos = fpos + one_stitch;
        if (fpos > outline_length) {
            fpos = fpos - outline_length;
        }
        else if (fpos < 0.0) {
            fpos = outline_length - fpos;
        }

        pesVec2f stitch = outline.getPointAtLength(fpos);
        patch.addVertex(stitch);

        // NooM >>>
        //if (i == 0) {
        //    jumps.push_back(false);  // Jimmy
        //} else {
        //    jumps.push_back(false);
        //}
        jumps.push_back(false); // NooM
        // NooM <<<
    }
    return patch;
}

void pesEMBFill::connect_points_stitches(pesVec2f p1, pesVec2f p2, pesStitchBlock & stitches){
    int ppmm = 10;
    float running_stitch_length = 1.0f*ppmm; // 1mm
    
    pesPolyline & outline = _shapeOutline;
    
    unsigned int nearestIndex;
    outline.getClosestPoint(p1, &nearestIndex);
    float fpos = outline.getLengthAtIndex(nearestIndex);
    
    float distance = perimeter_distance(p1, p2);
    
    int numStitches = abs(int(distance / running_stitch_length));
    
    float direction = copysign(1.0f, distance);
    float one_stitch = running_stitch_length * direction;
    
    float outline_length = outline.getPerimeter();
    for(int i=0; i<numStitches; i++){
        
        fpos = fpos + one_stitch;
        if(fpos>outline_length){
            fpos = fpos - outline_length;
        }
        else if(fpos<0.0){
            fpos = outline_length - fpos;
        }
        
        pesVec2f stitch = outline.getPointAtLength(fpos);
        
        if(i==0){
            stitches.addJump(stitch);
        }
        else{
            stitches.addStitch(stitch);
        }
    }
}


// ============================================================================
// FILL_TYPE_NORMAL2 — graph-based outline travel (Ink/Stitch approach)
// ============================================================================

// forward declaration (defined later in this file)
pesVec2f east(float angle);

// --- OutlineGraph methods ---

int pesEMBFill::OutlineGraph::addNode(const pesVec2f& pos) {
    nodes.push_back({pos, {}});
    return (int)nodes.size() - 1;
}

void pesEMBFill::OutlineGraph::addEdge(int a, int b, float weight) {
    if (a == b) return;
    if (weight < 0.0f) weight = 0.0f;
    nodes[a].neighbors.push_back({b, weight});
    nodes[b].neighbors.push_back({a, weight});
}

int pesEMBFill::OutlineGraph::findNearestNode(const pesVec2f& pos) const {
    int best = -1;
    float bestDist = FLT_MAX;
    for (int i = 0; i < (int)nodes.size(); i++) {
        float d = nodes[i].position.distance(pos);
        if (d < bestDist) {
            bestDist = d;
            best = i;
        }
    }
    return best;
}

std::vector<int> pesEMBFill::OutlineGraph::shortestPath(int source, int target) const {
    int n = (int)nodes.size();
    if (source < 0 || target < 0 || source >= n || target >= n) return {};
    if (source == target) return {source};

    std::vector<float> dist(n, FLT_MAX);
    std::vector<int> prev(n, -1);

    // min-heap: (distance, nodeIndex)
    using PII = std::pair<float, int>;
    std::priority_queue<PII, std::vector<PII>, std::greater<PII>> pq;

    dist[source] = 0;
    pq.push({0, source});

    while (!pq.empty()) {
        auto [d, u] = pq.top(); pq.pop();
        if (d > dist[u]) continue;
        if (u == target) break;

        for (const auto& [v, w] : nodes[u].neighbors) {
            float newDist = dist[u] + w;
            if (newDist < dist[v]) {
                dist[v] = newDist;
                prev[v] = u;
                pq.push({newDist, v});
            }
        }
    }

    if (dist[target] == FLT_MAX) return {};  // no path

    std::vector<int> path;
    for (int at = target; at != -1; at = prev[at]) {
        path.push_back(at);
    }
    std::reverse(path.begin(), path.end());
    return path;
}

float pesEMBFill::OutlineGraph::pathLength(const std::vector<int>& path) const {
    float total = 0;
    for (int i = 1; i < (int)path.size(); i++) {
        total += nodes[path[i - 1]].position.distance(nodes[path[i]].position);
    }
    return total;
}

// --- Build outline graph ---
// Inserts fill segment endpoints into the outline edges as graph nodes,
// creating a graph where Dijkstra can find shortest outline-following paths.

void pesEMBFill::buildOutlineGraph3() {
    _graph3.clear();

    // Spatial hash: quantize position to 0.5-unit grid for deduplication
    std::map<std::pair<int, int>, int> posToNode;
    auto getOrCreateNode = [&](const pesVec2f& p) -> int {
        auto key = std::make_pair((int)(p.x * 2), (int)(p.y * 2));
        auto it = posToNode.find(key);
        if (it != posToNode.end()) return it->second;
        int idx = _graph3.addNode(p);
        posToNode[key] = idx;
        return idx;
    };

    // Collect all fill segment endpoints
    std::vector<pesVec2f> fillEndpoints;
    for (const auto& section : _sections) {
        for (const auto& line : section) {
            fillEndpoints.push_back(pesVec2f(line.start.x, line.start.y));
            fillEndpoints.push_back(pesVec2f(line.end.x, line.end.y));
        }
    }

    // Helper: check if point lies on segment (within tolerance)
    auto pointOnSegment = [](const pesVec2f& pt, const pesVec2f& a, const pesVec2f& b, float tol) -> bool {
        float segLen = a.distance(b);
        if (segLen < 0.01f) return false;
        float d1 = a.distance(pt);
        float d2 = b.distance(pt);
        // Point is on segment if d1 + d2 ≈ segLen
        return (d1 + d2 - segLen) < tol;
    };

    // For each outline contour, insert fill endpoints and build edges
    for (const auto& outline : _shapeOutlines) {
        const auto& verts = outline.getVertices();
        int nv = (int)verts.size();
        if (nv < 2) continue;

        for (int i = 0; i < nv - 1; i++) {
            pesVec2f p1 = verts[i];
            pesVec2f p2 = verts[i + 1];
            float edgeLen = p1.distance(p2);
            if (edgeLen < 0.01f) continue;

            // Find fill endpoints that lie on this edge segment
            struct InsertPt {
                float t;
                pesVec2f pos;
            };
            std::vector<InsertPt> inserts;

            for (const auto& fp : fillEndpoints) {
                if (pointOnSegment(fp, p1, p2, 1.0f)) {
                    float t = p1.distance(fp) / edgeLen;
                    if (t > 0.001f && t < 0.999f) {
                        inserts.push_back({t, fp});
                    }
                }
            }

            std::sort(inserts.begin(), inserts.end(),
                [](const InsertPt& a, const InsertPt& b) { return a.t < b.t; });

            // Create edges: p1 -> insert[0] -> insert[1] -> ... -> p2
            int prevNode = getOrCreateNode(p1);
            for (const auto& ins : inserts) {
                int curNode = getOrCreateNode(ins.pos);
                float w = _graph3.nodes[prevNode].position.distance(ins.pos);
                _graph3.addEdge(prevNode, curNode, w);
                prevNode = curNode;
            }
            int endNode = getOrCreateNode(p2);
            float w = _graph3.nodes[prevNode].position.distance(p2);
            _graph3.addEdge(prevNode, endNode, w);
        }

        // Close the loop: last vertex -> first vertex
        if (nv >= 3) {
            int lastNode = getOrCreateNode(verts[nv - 1]);
            int firstNode = getOrCreateNode(verts[0]);
            float w = verts[nv - 1].distance(verts[0]);
            _graph3.addEdge(lastNode, firstNode, w);
        }
    }

    // No bridge edges: travel stays strictly on outline contours.
    // When source and target are on different outline components (disconnected),
    // travelOnGraph3 will use a JUMP stitch instead.

    // Count how many fill endpoints were actually inserted as graph nodes
    int insertedCount = 0;
    for (const auto& fp : fillEndpoints) {
        auto key = std::make_pair((int)(fp.x * 2), (int)(fp.y * 2));
        if (posToNode.find(key) != posToNode.end()) {
            insertedCount++;
        }
    }

    // Count total edges
    int totalEdges = 0;
    for (const auto& node : _graph3.nodes) {
        totalEdges += (int)node.neighbors.size();
    }
    totalEdges /= 2;  // undirected

    printf("[NORMAL2] Graph: %d nodes, %d edges, %d outlines, %d fill endpoints (%d inserted)\n",
           (int)_graph3.nodes.size(), totalEdges,
           (int)_shapeOutlines.size(), (int)fillEndpoints.size(), insertedCount);
    printf("[NORMAL2] Sections: %d\n", (int)_sections.size());
}

// --- createSections3 ---

std::deque<vector<pesEMBFill::Line>> pesEMBFill::createSections3(const float density,
                                                                  const float sewDirection,
                                                                  const float compensate,
                                                                  const bool fading) {
    _rowspacing = 10.0f / density;
    _sewDirection = sewDirection;
    std::vector<std::vector<pesEMBFill::Line>> rows_of_segments = intersect_region_with_grating2(_rowspacing, sewDirection, compensate, fading);
    _sections = pull_runs2(rows_of_segments);
    return _sections;
}

// --- find_nearest_section3: uses graph distance instead of Euclidean ---

int pesEMBFill::find_nearest_section3(std::deque<std::vector<pesEMBFill::Line>> sections,
                                      pesVec2f last_stitch,
                                      pesVec2f& start_corner) {
    float mindist = FLT_MAX;
    int nearestSection = -1;
    int section_count = -1;

    int fromNode = _graph3.findNearestNode(last_stitch);

    for (const auto& section : sections) {
        ++section_count;
        if (section.empty()) continue;

        std::vector<pesVec3f> corners;
        if (section.size() == 1) {
            corners = {section.front().start, section.front().end};
        } else {
            corners = {section.front().start, section.front().end,
                       section.back().start, section.back().end};
        }

        for (const auto& corner : corners) {
            pesVec2f c2d(corner.x, corner.y);
            int toNode = _graph3.findNearestNode(c2d);

            float dist;
            if (fromNode >= 0 && toNode >= 0) {
                auto path = _graph3.shortestPath(fromNode, toNode);
                if (!path.empty()) {
                    dist = _graph3.pathLength(path);
                } else {
                    // No graph path — fall back to Euclidean
                    dist = last_stitch.distance(c2d) * 2.0f;
                }
            } else {
                dist = last_stitch.distance(c2d) * 2.0f;
            }

            if (mindist > dist) {
                mindist = dist;
                nearestSection = section_count;
                start_corner = c2d;
            }
        }
    }
    return nearestSection;
}

// --- travelOnGraph3: travel along shortest graph path with running stitches ---

void pesEMBFill::travelOnGraph3(pesStitchBlock& block,
                                 const pesVec2f& from,
                                 const pesVec2f& to) {
    const float ppmm = 10.0f;
    const float running_stitch_length = 1.0f * ppmm;  // 1mm

    int fromNode = _graph3.findNearestNode(from);
    int toNode = _graph3.findNearestNode(to);

    if (fromNode < 0 || toNode < 0) {
        printf("[NORMAL2] travel: no nodes (from=%d, to=%d)\n", fromNode, toNode);
        return;
    }
    if (fromNode == toNode) {
        printf("[NORMAL2] travel: same node %d\n", fromNode);
        return;
    }

    auto path = _graph3.shortestPath(fromNode, toNode);
    if (path.size() < 2) {
        // Disconnected outline components — use JUMP
        printf("[NORMAL2] travel: JUMP (disconnected) from %d to %d\n", fromNode, toNode);
        block.addJump(to);
        return;
    }
    printf("[NORMAL2] travel: path %d->%d, %d hops, dist=%.1f\n",
           fromNode, toNode, (int)path.size()-1, _graph3.pathLength(path));

    // Walk along the graph path, placing running stitches every 1mm
    for (int i = 1; i < (int)path.size(); i++) {
        pesVec2f segStart = _graph3.nodes[path[i - 1]].position;
        pesVec2f segEnd = _graph3.nodes[path[i]].position;
        float segLen = segStart.distance(segEnd);

        if (segLen < 0.1f) continue;

        if (segLen <= running_stitch_length) {
            // Short segment — just add the endpoint
            block.addStitch(segEnd);
        } else {
            // Subdivide into running stitches
            pesVec2f dir = (segEnd - segStart).normalize();
            int nStitches = (int)(segLen / running_stitch_length);
            for (int s = 1; s <= nStitches; s++) {
                pesVec2f p = segStart + dir * (running_stitch_length * s);
                block.addStitch(p);
            }
            // Add final endpoint if not already close
            if (block.polyline.getVertices().back().distance(segEnd) > 0.5f) {
                block.addStitch(segEnd);
            }
        }
    }
}

// --- fillSection3: zigzag fill directly to pesStitchBlock ---

void pesEMBFill::fillSection3(pesStitchBlock& block,
                               const std::vector<Line>& section,
                               float angle, float max_stitch_length,
                               const Preset& preset,
                               bool isFirst, bool swap) {
    const auto unit_per_mm = 10.0f;
    const auto cMaxStitchLength = 0.1f * unit_per_mm;
    const auto cMaxTrimAtLength = pesGetDocument()->getTrimAtLength() * unit_per_mm;
    const auto num_stagger = (int)preset.row_stagger.size();

    if (num_stagger <= 0) return;

    std::vector<float> stagger;
    for (const auto& s : preset.row_stagger) {
        stagger.push_back(s * unit_per_mm);
    }

    bool firstPoint = true;
    pesVec2f last_end(0, 0);

    int rowIdx = 0;
    for (const auto& segment : section) {
        pesVec2f beg = segment.start;
        pesVec2f end = segment.end;

        if (swap) {
            std::swap(beg, end);
        }

        if (rowIdx < 3) {
            printf("[NORMAL2]   row%d: swap=%d beg=(%.1f,%.1f) end=(%.1f,%.1f)\n",
                   rowIdx, (int)swap, beg.x, beg.y, end.x, end.y);
        }
        rowIdx++;

        // First point of section: JUMP only if isFirst, otherwise NORMAL
        if (firstPoint && isFirst) {
            block.addJump(beg);
        }
        else if (firstPoint) {
            block.addStitch(beg);
        }
        else if (beg.distance(last_end) > cMaxTrimAtLength) {
            block.addJump(beg);
        }
        else {
            block.addStitch(beg);
        }
        firstPoint = false;

        const auto row_direction = (end - beg).normalize();
        const auto segment_length = end.distance(beg);
        const auto row_num = segment.lineNumber % num_stagger;
        const auto stagger_offset = stagger[row_num];
        const auto stitch_offset = fmodf(beg.dot(east(angle)) - stagger_offset, max_stitch_length);
        auto first_stitch = beg - stitch_offset * east(angle);

        if ((first_stitch - beg).dot(row_direction) < 0) {
            first_stitch += row_direction * max_stitch_length;
        }

        auto offset = (first_stitch - beg).length();
        while (offset < segment_length) {
            block.addStitch(beg + offset * row_direction);
            offset += max_stitch_length;
        }

        if ((end - block.polyline.getVertices().back()).length() > cMaxStitchLength) {
            block.addStitch(end);
        }

        last_end = end;
        swap = !swap;
    }
}

// --- toStitchBlock3: orchestrator ---

pesStitchBlock pesEMBFill::toStitchBlock3(const Preset& preset, const bool fading) {
    if (_sections.empty()) {
        createSections3(10.0f / _rowspacing, _sewDirection, 0, fading);
    }

    const float max_stitch_length = 5.0f * 10.0f;
    _preset = preset;

    pesStitchBlock block;
    std::deque<std::vector<pesEMBFill::Line>> copy = _sections;

    printf("[NORMAL2] Sections: %d, Outlines: %d\n",
           (int)_sections.size(), (int)_shapeOutlines.size());

    // --- Helper: walk along a single outline from vertex fromIdx to toIdx ---
    auto walkSingleOutline = [&](pesStitchBlock& blk, int outlineIdx,
                                  int fromIdx, int toIdx) {
        const auto& verts = _shapeOutlines[outlineIdx].getVertices();
        int nv = (int)verts.size();
        if (fromIdx == toIdx || nv < 3) return;

        // Calculate forward and backward distance
        float fwdDist = 0, bwdDist = 0;
        {
            int i = fromIdx;
            int safety = 0;
            while (i != toIdx && safety++ < nv + 1) {
                int next = (i + 1) % nv;
                fwdDist += verts[i].distance(verts[next]);
                i = next;
            }
        }
        {
            int i = fromIdx;
            int safety = 0;
            while (i != toIdx && safety++ < nv + 1) {
                int prev = (i - 1 + nv) % nv;
                bwdDist += verts[i].distance(verts[prev]);
                i = prev;
            }
        }

        bool goFwd = fwdDist <= bwdDist;
        int i = fromIdx;
        int safety = 0;
        while (i != toIdx && safety++ < nv + 1) {
            int next = goFwd ? (i + 1) % nv : (i - 1 + nv) % nv;
            blk.addStitch(verts[next]);
            i = next;
        }
    };

    // --- Helper: find nearest outline and vertex for a point ---
    auto findNearestOutlineVertex = [&](const pesVec2f& pt, int& outOutline, int& outIdx) {
        float bestDist = FLT_MAX;
        outOutline = -1;
        outIdx = -1;
        for (int oi = 0; oi < (int)_shapeOutlines.size(); oi++) {
            const auto& verts = _shapeOutlines[oi].getVertices();
            for (int i = 0; i < (int)verts.size(); i++) {
                float d = pt.distance(verts[i]);
                if (d < bestDist) {
                    bestDist = d;
                    outOutline = oi;
                    outIdx = i;
                }
            }
        }
    };

    // --- Helper: travel from 'from' to 'to' along outlines ---
    // If same outline: walk along it
    // If different outlines: walk on outline1 → short jump → walk on outline2
    auto travelOnOutlines = [&](pesStitchBlock& blk, const pesVec2f& from,
                                 const pesVec2f& to) {
        int fromOutline = -1, fromIdx = -1;
        int toOutline = -1, toIdx = -1;
        findNearestOutlineVertex(from, fromOutline, fromIdx);
        findNearestOutlineVertex(to, toOutline, toIdx);

        if (fromOutline < 0 || toOutline < 0) return;

        if (fromOutline == toOutline) {
            // Same outline: simple walk
            printf("[NORMAL2] travel: same outline %d\n", fromOutline);
            walkSingleOutline(blk, fromOutline, fromIdx, toIdx);
        } else {
            // Different outlines: find closest bridge between the two outlines
            const auto& fromVerts = _shapeOutlines[fromOutline].getVertices();
            const auto& toVerts = _shapeOutlines[toOutline].getVertices();
            int bridgeFromIdx = -1, bridgeToIdx = -1;
            float closestDist = FLT_MAX;

            for (int i = 0; i < (int)fromVerts.size(); i++) {
                for (int j = 0; j < (int)toVerts.size(); j++) {
                    float d = fromVerts[i].distance(toVerts[j]);
                    if (d < closestDist) {
                        closestDist = d;
                        bridgeFromIdx = i;
                        bridgeToIdx = j;
                    }
                }
            }

            printf("[NORMAL2] travel: outline %d -> %d, bridge dist=%.1f\n",
                   fromOutline, toOutline, closestDist);

            // 1. Walk on outline1: fromIdx → bridgeFromIdx
            walkSingleOutline(blk, fromOutline, fromIdx, bridgeFromIdx);
            // 2. Short stitch to outline2 at bridgeToIdx
            blk.addStitch(toVerts[bridgeToIdx]);
            // 3. Walk on outline2: bridgeToIdx → toIdx
            walkSingleOutline(blk, toOutline, bridgeToIdx, toIdx);
        }
    };

    // --- Helper: find nearest section by Euclidean distance ---
    auto findNearestSection = [&](pesVec2f lastStitch, pesVec2f& out_corner) -> int {
        float mindist = FLT_MAX;
        int best = -1;
        for (int idx = 0; idx < (int)_sections.size(); idx++) {
            const auto& sec = _sections[idx];
            if (sec.empty()) continue;
            std::vector<pesVec3f> corners;
            if (sec.size() == 1) {
                corners = {sec.front().start, sec.front().end};
            } else {
                corners = {sec.front().start, sec.front().end,
                           sec.back().start, sec.back().end};
            }
            for (const auto& corner : corners) {
                pesVec2f c2d(corner.x, corner.y);
                float dist = lastStitch.distance(c2d);
                if (dist < mindist) {
                    mindist = dist;
                    best = idx;
                    out_corner = c2d;
                }
            }
        }
        return best;
    };

    // --- Main loop: nearest-neighbor, always try outline walk ---
    bool isFirst = true;

    while (_sections.size()) {
        pesVec2f start_corner;
        pesVec2f lastStitch = block.polyline.getVertices().empty()
            ? pesVec2f(0, 0)
            : block.polyline.getVertices().back();

        int section_index = findNearestSection(lastStitch, start_corner);
        if (section_index < 0) break;

        // Travel along outlines (walk outline1 → short jump → walk outline2)
        bool useJump = true;
        if (!isFirst) {
            travelOnOutlines(block, lastStitch, start_corner);
            useJump = false;
        }

        auto& section = _sections.at(section_index);

        // Orient section to match start_corner
        pesVec2f s0start(section[0].start.x, section[0].start.y);
        pesVec2f s0end(section[0].end.x, section[0].end.y);
        if (!start_corner.match(s0start, 1) && !start_corner.match(s0end, 1)) {
            std::reverse(section.begin(), section.end());
        }
        s0start = pesVec2f(section[0].start.x, section[0].start.y);
        bool swap = !start_corner.match(s0start, 1);

        printf("[NORMAL2] Section %d: %d rows, %s\n",
               section_index, (int)section.size(),
               useJump ? "JUMP" : "walk");

        fillSection3(block, section, _sewDirection, max_stitch_length, preset, useJump, swap);

        _sections.erase(_sections.begin() + section_index);
        isFirst = false;
    }

    _sections = copy;
    return block;
}

// ============================================================================


static const int __ccw = -1;
static const int __cw = 1;
std::vector<std::vector<pesEMBFill::Line>> pesEMBFill::intersect_region_with_grating(float _row_spacing, float degree, float compensate, bool fading){
    
    float row_spacing = _row_spacing;
    int outline_size = (int)_shape.getOutline().size();
    int first_outer_dir = _shape.getOutline()[0].getArea()>=0 ? __cw : __ccw;
    vector<pesPolyline> outlines = _shape.getOutline();
    
    _shapeOutline.clear();
    for(int i=0;i<(int)outlines.size();i++){
        int dir = outlines[i].getArea()>=0 ? __cw : __ccw;
        if(dir==first_outer_dir){
            _shapeOutline.addVertices(outlines[i].getVertices());
            _shapeOutline.close();
        }
    }
    _shapeOutline.simplify();
    
    int numContourGroups = 0;
    for(int i=0; i<outline_size; i++){
        int dir = outlines[i].getArea()>=0 ? __cw : __ccw;
        if(dir==first_outer_dir)
            numContourGroups++;
    }
    std::vector<std::vector<pesPolyline>> contourGroups;
    contourGroups.resize(numContourGroups);
    int groupIndex=0;
    for(int i=0; i<outline_size; i++){
        int dir = outlines[i].getArea()>=0 ? __cw : __ccw;
        if(dir==first_outer_dir){
            contourGroups[groupIndex++].push_back(outlines[i]);
        }
    }
    
    for(int i=0; i<outline_size; i++){
        int dir = outlines[i].getArea()>=0 ? __cw : __ccw;
        if(dir!=first_outer_dir){
            pesVec2f cxy = outlines[i].getCentroid2D();
            for(int j=0;j<(int)contourGroups.size();j++){
                if(contourGroups[j][0].getBoundingBox().inside(cxy.x, cxy.y))
                    contourGroups[j].push_back(outlines[i]);
            }
        }
    }
    
    vector<vector<Line>> rows; // ret
    for(int ii=0; ii<numContourGroups; ii++)
        //    for(int g=numContourGroups-1; g>=0; g--)
    {
        std::vector<pesPolyline> contours = contourGroups[ii];
        std::vector<int> contourDir;
        for(int i=0; i<(int)contours.size(); i++){
            contours[i].simplify();
        }
        
        pesRectangle bound = contours[0].getBoundingBox();
        for(int i=1; i<(int)contours.size(); i++){
            bound.growToInclude(contours[i].getBoundingBox());
        }
        
        if(contours.size()>1){
            contourDir.resize(contours.size());
            for(int i=0; i<(int)contours.size(); i++){
                
                if(contours[i].getArea()>=0){
//                    GUI_LogNotice("contourGroup", "%d is cw area=%g", i, contourGroup[i].getArea());
                    contourDir[i] = __cw;
                }
                else{
//                    GUI_LogNotice("contourGroup", "%d is ccw area=%g", i, contourGroup[i].getArea());
                    contourDir[i] = __ccw;
                }
            }
        }
        
        pesVec2f upper_left(bound.getMinX(), bound.getMinY());
        pesVec2f lower_right(bound.getMaxX(), bound.getMaxY());
        float length = (upper_left - lower_right).length();
        float half_length = length / 2.0f;
        
        pesVec2f direction(1, 0);
        direction.rotate(degree);
        
        pesVec2f normal = direction;
        normal.rotateRad((float)M_PI_2);
        
        pesVec2f center = bound.getCenter();
        
        pesPath dummy;
        dummy.setMode(pesPath::Mode::POLYLINES);
        dummy.rectangle(bound.x, bound.y, bound.width, bound.height);
        dummy.translate(-center.x, -center.y);
        dummy.rotate(degree);
        dummy.translate(center.x, center.y);
        pesRectangle newBound = dummy.getOutline()[0].getBoundingBox();
        float start = newBound.getMinY();
        float end = newBound.getMaxY();
        
        start -= center.y;
        end -= center.y;
        
        // pom - fade
        float start_y = start;
        float end_y = end;
        float length_y = end_y - start_y;
        float start_fade = start + length_y * 0.4;
        float max_row_spacing = row_spacing * 5;
        
        start -= fmod(start + normal.y * center.y, row_spacing);
        //        if(compensate!=0.0){
        //            start+=compensate;
        //            end-=compensate;
        //        }
        
        int lineNumber=0;
        while(start<end){
            pesVec2f p00 = center + normal*start - direction*half_length;
            pesVec2f p01 = center + normal*start + direction*half_length;
            
            pesEMBFill::Line grating_line(p00, p01);
            
            std::vector<Line> res;
            std::vector<pesVec2f> ret;
            int numSegment = (int)contours.size();
            for(int i=0; i<numSegment; i++){
                if(contours[i].isClosed()){
                    auto last = contours[i].getVertices().back();
                    if(last.x!=contours[i][0].x || last.y!=contours[i][0].y)
                        contours[i].addVertex(contours[i][0]);
                }
                auto vertices = contours[i].getVertices();
                for(int j=1; j<(int)vertices.size(); j++){
                    pesVec2f p0 = vertices[j-1];
                    pesVec2f p1 = vertices[j];
                    
                    pesVec2f out;
                    bool intersect = pesLineSegmentIntersection(grating_line.start, grating_line.end, p0, p1, out);
                    if(intersect){
                        ret.push_back(out);
                    }
                }
            }
            
            if(ret.size()>=2){
                
                pesSort(ret, [=](pesVec2f &a, pesVec2f &b)->bool{
                    if(p00.distance(a) < p00.distance(b)){
                        return true;
                    }
                    else{
                        return false;
                    }
                });
                
                
                //            GUI_LogNotice("intersect w/ grating line", "size=%ld", ret.size());
                for(int k=1; k<(int)ret.size(); k++){
                    pesVec2f p0 = ret[k-1];
                    pesVec2f p1 = ret[k];
                    pesVec2f middle = p0.getMiddle(p1);
                    
                    if(contours.size()==1){
                        if(contours[0].inside(middle)){
                            // Jimmy - compensate
                            p1 = p1 + (p1-p0).normalize() * compensate * 10;
                            p0 = p0 - (p1-p0).normalize() * compensate * 10;                            
                            pesEMBFill::Line line(p0, p1);
                            line.lineNumber = lineNumber;
                            res.push_back(line);
                        }
                    }
                    else if(contours.size()>1){ // (PolyWindingMode::POLY_WINDING_ODD)
                        int outer_dir = contourDir[0];
                        bool inside_hole = false;
                        // check with hole
                        for(int i=(int)contours.size()-1; i>=1; i--){
                            if(contourDir[i]!=outer_dir){
                                // this is a hole
                                if(contours[i].inside(middle)){
                                    inside_hole = true;
                                    break;
                                }
                            }
                        }
                        
                        // check with outline
                        if(inside_hole==false){
                            for(int i=(int)contours.size()-1; i>=0; i--){
                                if(contourDir[i]==outer_dir){
                                    // this is a outline
                                    if(contours[i].inside(middle)){
                                        // Jimmy - compensate
                                        p1 = p1 + (p1-p0).normalize() * compensate * 10;
                                        p0 = p0 - (p1-p0).normalize() * compensate * 10;
                                        pesEMBFill::Line line(p0, p1);
                                        line.lineNumber = lineNumber;
                                        res.push_back(line);
                                    }
                                }
                            }
                        }
                    }
                }
                
            }
            
//            fading = true;
            float cur_row_spacing = row_spacing;
            if(fading){
                if(start>=start_fade){
                    cur_row_spacing = pesMap(start, start_fade, length_y, row_spacing, max_row_spacing);
                }
            }
            
            if(res.empty()){
                start += cur_row_spacing;
                lineNumber++;
                //            GUI_LogNotice("not intersect w/ grating line");
                continue;
            }
            
            std::vector<Line> runs = res;
            pesSort(runs, [=](Line &a, Line &b)->bool{
                if(upper_left.distance(a.start) < upper_left.distance(b.start)){ //if(upper_left.distance(a.start) > upper_left.distance(b.start)){
                    return true;
                }
                else{
                    return false;
                }
            });
            
            
            rows.push_back(runs);
            //        GUI_LogNotice("intersect w/ grating line", "rows=%ld runs=%ld", rows.size(), runs.size());
            start += cur_row_spacing;
            lineNumber++;
        }// end while
        
        contours.clear();
        
    } // end for(numContourGroup)
    
    return rows;
    
}

std::vector<std::vector<pesEMBFill::Line>> pesEMBFill::intersect_region_with_grating2(
        const float row_spacing, 
        const float degree, 
        const float compensate, 
        const bool fading) {

    const float ppmm = 10.0f;

    // test Inverse
    // const int fillRule = _shape.fillRule | 2;

    const int fillRule = _shape.fillRule;
    // SkDebugf("fillRule: %d\n", fillRule);

    // experiment dynamic tolerance
    //const float density = ppmm / row_spacing;
    //const float density2x = density * 5.0f;
    //const auto mintolerance = 1.0f / density2x;
    //const float density4x = density * 4.0f;
    //const auto mintolerance = 1.0f / density4x;
    //SkDebugf("mintolerance: %g\n", mintolerance);

    auto addClosePointIfNeed = [](pesPolyline& polyline) {
        const auto& first = polyline.getVertices().front();
        const auto& last = polyline.getVertices().back();
        if (last.x != first.x || last.y != first.y) {
            polyline.addVertex(first);
        }  
    };

    // copy
    //vector<pesPolyline> outlines = _shape.getOutline();
    std::vector<pesPolyline> outlines;
    for (const auto& outline : _shape.getOutline()) {
        if (outline.size() > 2) {
            outlines.push_back(outline);
        }
    }

    int outline_size = (int)outlines.size();
    //SkDebugf("outlines:%d\n", outline_size);

    for (int i = 0; i < outline_size; i++) {
        auto& outline = outlines[i];
        addClosePointIfNeed(outline);
        //outline.simplify(mintolerance);
        outline.simplify(0.2f);
    }

    _shapeOutlines = outlines;
    
    // ============================================================================
    // ============================================================================

    std::vector<std::vector<Line>> rows;  // ret
    
    // outset 0.2mm for inverse fill bound
    const float outset = 0.2f * ppmm;

    pesRectangle bound = _shapeOutline.getBoundingBox();
    const pesVec2f center = bound.getCenter();
    bound.setFromCenter(center, bound.width + row_spacing, bound.height + row_spacing + row_spacing);
        
    const pesVec2f upper_left(bound.getMinX(), bound.getMinY());
    const pesVec2f lower_right(bound.getMaxX(), bound.getMaxY());
    const float length = (upper_left - lower_right).length();
    const float half_length = length / 2.0f;
    pesVec2f direction(1, 0);
    direction.rotate(degree);
        
    pesVec2f normal = direction;
    normal.rotateRad((float)M_PI_2);        
        
    pesPath dummy;
    dummy.setMode(pesPath::Mode::POLYLINES);
    dummy.rectangle(bound.x, bound.y, bound.width, bound.height);
    dummy.translate(-center.x, -center.y);
    dummy.rotate(degree);
    dummy.translate(center.x, center.y);

    const pesRectangle newBound = dummy.getOutline().front().getBoundingBox();
    float start = newBound.getMinY();
    float end = newBound.getMaxY();

    start -= center.y;
    end -= center.y;

    auto clip = pesPolyline::fromRect(bound);
    addClosePointIfNeed(clip);

    // pom - fade
    float start_y = start;
    float end_y = end;
    float length_y = end_y - start_y;
    float start_fade = start + length_y * 0.4f;
    float max_row_spacing = row_spacing * 5.0f;
        
    start -= fmod(start + normal.y * center.y, row_spacing);
        
    int lineNumber = 0;
    while (start < end) {
        pesVec2f p00 = center + normal * start - direction * half_length;
        pesVec2f p01 = center + normal * start + direction * half_length;
            
        std::vector<Line> res;
        std::vector<pesVec3f> ret;
        pesVec2f out;

        //ret.emplace_back(p00).z = 0;
        //ret.emplace_back(p01).z = 0;
        
        //clip
        {
            const auto& vertices = clip.getVertices();
            for (int j = 0, k = 1, kk = (int)vertices.size(); k < kk; j++, k++) {
                const auto& p0 = vertices[j];
                const auto& p1 = vertices[k];
                if (pesLineSegmentIntersection(p00, p01, p0, p1, out)) {
                    ret.emplace_back(out).z = 0;
                }
            }
        }

        if (ret.size() >= 2) {
            std::vector<pesVec3f> retvecs;
            for (int i = 0, id = 1; i < outline_size; i++, id++) {
                auto& outline = outlines[i];
                const auto& vertices = outline.getVertices();
                for (int j = 0, k = 1, kk = (int)vertices.size(); k < kk; j++, k++) {
                    const auto& p0 = vertices[j];
                    const auto& p1 = vertices[k];
                    if (pesLineSegmentIntersection(p00, p01, p0, p1, out)) {
                        auto rp0 = pesVec2f(p0).rotate(-degree, center);
                        auto rp1 = pesVec2f(p1).rotate(-degree, center);

                        // directions up or down.
                        auto dir = rp0.y < rp1.y ? __cw : __ccw;

                        // temporary combine the directions and outline-id together.
                        retvecs.emplace_back(out).z = dir * id;
                    }
                }
            }
            if (retvecs.size() >= 2 && (retvecs.size() % 2) == 0) {
                for (const auto& retvec : retvecs) {
                    ret.push_back(retvec);
                }
            }
        }
        if (ret.size() >= 2 && (ret.size() % 2) == 0) {
                
            pesSort(ret, [=](pesVec3f& a, pesVec3f& b) -> bool {
                return p00.distance(a) < p00.distance(b);
            });
            
            for (int sum = 0, j = 0, k = 1, kk = (int)ret.size() & (~1); k < kk; j++, k++) {

                // Winding: 0
                // EvenOdd: 1
                // InverseWinding: 2
                // InverseEvenOdd: 3

                // separate directions and outline-id.
                auto& combine = ret[j].z;
                const auto dir = combine == 0 ? 0 : combine > 0 ? __cw : __ccw;

                bool isFill = false;
                const bool isInverse = fillRule & 2;
                switch (fillRule) {
                    default:
                        continue;
                        break;

                    case 0:
                    case 2:
                        isFill = sum += dir;
                        break;

                    case 1:
                    case 3:
                        isFill = 1 & sum++;
                        break;
                }

                if (isInverse) {
                    isFill = !isFill;
                }

                if (isFill) {
                    pesVec2f p0 = ret[j];
                    pesVec2f p1 = ret[k];

                    // Jimmy - compensate
                    p1 = p1 + (p1 - p0).normalize() * compensate * 10;
                    p0 = p0 - (p1 - p0).normalize() * compensate * 10;

                    //pesEMBFill::Line line(p0, p1);
                    //line.lineNumber = lineNumber;

                    const auto id0 = ret[j].z;
                    const auto id1 = ret[k].z;
                    
                    pesEMBFill::Line line({p0.x, p0.y, id0}, {p1.x, p1.y, id1}, lineNumber);
                    res.push_back(line);

                    //res.push_back(pesEMBFill::Line(ret[j], ret[k], lineNumber));
                }
            }

            //to be used in the future
            //// join lines
            //std::vector<Line> resjoin;
            //std::vector<Line> join;
            //auto fnjoin = [&]() {
            //    //SkDebugf("join.size: %lu\n", join.size());
            //    if (join.size()) {
            //        std::vector<pesVec3f> points;
            //        points.push_back(join.front().start);
            //        for (const auto& l : join) {
            //            points.push_back(l.end);
            //        }
            //        int j = 0, k = points.size() - 1;
            //        pesVec2f p0 = ret[j];
            //        pesVec2f p1 = ret[k];
            //        // Jimmy - compensate
            //        p1 = p1 + (p1 - p0).normalize() * compensate * 10;
            //        p0 = p0 - (p1 - p0).normalize() * compensate * 10;
            //        ret[j].x = p0.x;
            //        ret[j].y = p0.y;
            //        ret[k].x = p1.x;
            //        ret[k].y = p1.y;
            //        resjoin.push_back(pesEMBFill::Line(points, lineNumber));
            //        join.clear();
            //    }
            //};
            ////SkDebugf("res.size: %lu\n", res.size());
            //if (res.size() > 1) {
            //    auto e = res.front().start;
            //    for (const auto& line : res) {
            //        const auto s = line.start;
            //        if (e != s) {
            //            //SkDebugf("[%g, %g, %g], [%g, %g, %g]\n", e.x, e.y, e.z, s.x, s.y, s.z);
            //            fnjoin();
            //        }
            //        join.push_back(line);
            //        e = line.end;
            //    }
            //    fnjoin();
            //    res = resjoin;
            //}
        } 
            
        float cur_row_spacing = row_spacing;

        if (fading) {
            if (start >= start_fade) {
                cur_row_spacing = pesMap(start, start_fade, length_y, row_spacing, max_row_spacing);
            }
        }
            
        if (!res.empty()) {
            rows.push_back(res);
        } 
            
        start += cur_row_spacing;
        lineNumber++;
    }// end while
        
    return rows;
}

const auto __embNoneLine = pesEMBFill::Line(pesVec3f::zero(), pesVec3f::zero());
deque<vector<pesEMBFill::Line>> pesEMBFill::pull_runs(vector<vector<pesEMBFill::Line>> &rows){
    std::deque<std::vector<Line>> runs;
    int count = 0;
    int size = 0;
    while ((size = (int)rows.size()) > 0) {
        std::vector<Line> run;
        Line prev = __embNoneLine;
        bool prevIsNone = true;
        
        for (int row_num = 0; row_num < size; row_num++) {
            vector<Line> row = rows[row_num];
            Line &first = row[0];
            if(prevIsNone==false && is_same_run(prev, first)==false){
                break;
            }

            run.push_back(first);
            prev = first;
            prevIsNone = false;
            rows[row_num].erase(rows[row_num].begin());
            
        }// end for
        
        runs.push_back(run);
        
        pesRemove(rows, [=](vector<Line> &row)->bool{
            return row.empty();
        });
        
        count+=1;
        
    }// end while
    return runs;
}

std::deque<std::vector<pesEMBFill::Line>> pesEMBFill::pull_runs2(std::vector<std::vector<pesEMBFill::Line>>& rows) {
    const float max_stitch_length = _rowspacing * 8.0f;

    const auto fnissamerun = [&](const pesVec2f& s0, const pesVec2f& e0, const pesVec2f& s1, const pesVec2f& e1) {
        if (s0.distance(s1) > max_stitch_length) return false;
        if (e0.distance(e1) > max_stitch_length) return false;
        return true;
    };

    std::deque<std::vector<Line>> runs;
    std::vector<std::vector<pesEMBFill::Line>> temprows;

    const auto fnpullruns = [&]() {
        while (temprows.size()) {
            std::vector<pesEMBFill::Line> run;
            auto prev = __embNoneLine;
            bool prevIsNone = true;

            for (auto& row : temprows) {
                const auto& front = row.front();
                if (!prevIsNone && !fnissamerun(prev.start, prev.end, front.start, front.end)) {
                    break;
                }
                run.push_back(front);
                prev = front;
                prevIsNone = false;
                row.erase(row.begin());
            }
            runs.push_back(run);
            pesRemove(temprows, [](const auto& row) -> bool { return row.empty(); });
        }
        temprows.clear();
    };

    if (rows.size()) {
        size_t prevcolumns = rows.front().size();
        for (auto& row : rows) {
            const size_t columns = row.size();
            if (prevcolumns != columns) {
                prevcolumns = columns;
                fnpullruns();
            }
            temprows.push_back(row);
        }
        fnpullruns();
    }
    return runs;
}

bool pesEMBFill::is_same_run(const pesEMBFill::Line &segment1, const pesEMBFill::Line &segment2){
    float max_stitch_length = 5 * 10;
    if(segment1.start.distance(segment2.start)>max_stitch_length)
        return false;
    
    if(segment1.end.distance(segment2.end)>max_stitch_length)
        return false;
    
    return true;
}

vector<pesVec2f> pesEMBFill::get_corner_points(vector<pesEMBFill::Line> section){
    pesVec2f corner[4] = {section[0].start, section[0].end, section.back().start, section.back().end};
    std::vector<pesVec2f> c;
    c.resize(4);
    c[0] = corner[0];
    c[1] = corner[1];
    c[2] = corner[2];
    c[3] = corner[3];
    return c;
}

float pesEMBFill::perimeter_distance(const pesVec2f& p1,
                                     const pesVec2f& p2,
                                     const pesPolyline* poutline) {
    // # how far around the perimeter (and in what direction) do I need to go
    // # to get from p1 to p2?
    
    const auto& outline = poutline != nullptr ? *poutline : _shapeOutline;
    
    unsigned int nearestIndex_p1, nearestIndex_p2;
    outline.getClosestPoint(p1, &nearestIndex_p1);
    outline.getClosestPoint(p2, &nearestIndex_p2);
    
    float fpos_p1 = outline.getLengthAtIndex(nearestIndex_p1);
    float fpos_p2 = outline.getLengthAtIndex(nearestIndex_p2);
    
    //        float distance = (p2_projection - p1_projection).length();
    float distance = fpos_p2 - fpos_p1;
    float outline_length = outline.getPerimeter();
    
    if (fabs(distance) > outline_length / 2.0){
        // # if we'd have to go more than halfway around, it's faster to go
        // # the other way
        if (distance < 0)
            return distance + outline_length;
        else if (distance > 0)
            return distance - outline_length;
        else
            // # this ought not happen, but just for completeness, return 0 if
            // # p1 and p0 are the same point
            return 0;
    }
    else
        return distance;
}

pesVec2f east(float angle){
    // "east" is the name of the direction that is to the right along a row
    // return ofPoint(1, 0).rotate(-angle, ofVec3f(0,0,1));
    return pesVec2f(1, 0).rotate(angle);
}

pesPolyline pesEMBFill::section_to_patch(vector<Line> &group_of_segments, float angle, float row_spacing, float max_stitch_length, const Preset& preset, std::vector<bool> & jumps){
    pesPolyline patch;
    
    bool swap = false;
    pesVec3f last_end(0,0);
    
    int pixels_per_mm = 10;
    int num_stagger = (int)preset.row_stagger.size();
    
    if(num_stagger>0){
        std::vector<float> stagger;
        for(auto s : preset.row_stagger){
            stagger.push_back(s*pixels_per_mm);
        }
        
        for(const auto& segment : group_of_segments){
            
            pesVec2f beg = segment.start;
            pesVec2f end = segment.end;
            if(swap){
                std::swap(beg, end);
            }
            
            pesVec2f row_direction = (end-beg).normalize();
            float segment_length = end.distance(beg);
            
            //// only stitch the first point if it's a reasonable distance away from the last stitch
            //if(last_end.length()==0 || beg.distance(last_end)>=0.3*pixels_per_mm){
            //    patch.addStitch(beg);
            //}
            //if(last_end.length()==0 || beg.distance(last_end)>=row_spacing*0.9){
            //    patch.addVertex(beg);
            //}
            const float unit_per_mm = 10;
            if(last_end.length()==0){
                jumps.push_back(true);
            }
            else if(beg.distance(last_end)>pesGetDocument()->getTrimAtLength() * unit_per_mm){ // Jimmy - fill end before expected
                jumps.push_back(true);
            }
            else{
                jumps.push_back(false);
            }
            patch.addVertex(beg);
            
            int row_num = segment.lineNumber%num_stagger;
            float stagger_offset = stagger[row_num];
            float stitch_offset = fmodf(beg.dot(east(angle)) - stagger_offset , max_stitch_length);
            pesVec2f first_stitch = beg - stitch_offset * east(angle);
            
            // we might have chosen our first stitch just outside this row, so move back in
            //        if(((first_stitch - beg) * row_direction).x < 0)
            if((first_stitch - beg).dot(row_direction) < 0)
                first_stitch += row_direction * max_stitch_length;
            
            float offset = (first_stitch - beg).length();
            
            while(offset < segment_length){
                patch.addVertex(beg + offset * row_direction);
                jumps.push_back(false);
                offset += max_stitch_length;
            }
            
            if ((end - patch.getVertices().back()).length() >= 0.1 * pixels_per_mm){
                patch.addVertex(end);
                jumps.push_back(false);
            }
            
            last_end = end;
            swap = !swap;
        }
    }

    return patch;
}

pesPolyline pesEMBFill::section_to_patch(vector<Line> &group_of_segments, float angle, float row_spacing, std::vector<bool> & jumps){
    
    pesPolyline patch;
    bool swap = false;
    pesVec2f last_end(0,0);
    
    int ppmm = 10;
    
    for(const auto& segment : group_of_segments){
        
        pesVec2f beg = segment.start;
        pesVec2f end = segment.end;
        if(swap){
            std::swap(beg, end);
        }
        
        //pesVec2f row_direction = (end-beg).normalize();
        //float segment_length = end.distance(beg);
        
        //// only stitch the first point if it's a reasonable distance away from the last stitch
        //if(last_end.length()==0 || beg.distance(last_end)>=row_spacing*0.9){
        //    patch.addVertex(beg);
        //}
        const float unit_per_mm = 10;
        if(last_end.length()==0){
            jumps.push_back(true);
        }
        else if(beg.distance(last_end)>pesGetDocument()->getTrimAtLength() * unit_per_mm){
            jumps.push_back(true);
        }
        else{
            jumps.push_back(false);
        }
        patch.addVertex(beg);
        
        vector<pesPath> acc_grids;
        for(auto grid : _grids){
            auto & vertices = grid.getOutline().front().getVertices();
            int n = (int)vertices.size();
            if(n>1){
                pesVec2f result;
                bool hasIntersect = false;
                for(int i=1; i<n; i++){
                    pesVec2f p0 = vertices[i-1];
                    pesVec2f p1 = vertices[i];
                    bool b = pesLineSegmentIntersection(beg, end, p0, p1, result);
                    if(b){
                        acc_grids.push_back(grid);
                        hasIntersect = true;
                        break;
                    }
                }
                
                if(!hasIntersect){
                    if(grid.inside(beg.x, beg.y)||grid.inside(end.x, end.y)){
                        acc_grids.push_back(grid);
                    }
                }
            }
        }
        if(acc_grids.size()){
            pesEMBFill::Line line(beg, end);
            vector<pesVec2f> emb_points;
            for(auto acc_grid : acc_grids){
                auto v = acc_grid.getOutline()[0].getVertices()[0];
                _patternResized.translate(v.x, v.y);
                auto points = intersect(_patternResized, line);
                _patternResized.translate(-v.x, -v.y);
                if(points.size()){
                    emb_points.insert(emb_points.end(), points.begin(), points.end());
                    points.clear();
                }
            }
            
            
            if(emb_points.size()>1){
                pesSort(emb_points, [=](pesVec2f &a, pesVec2f &b)->bool{
                    if(line.start.distance(a) < line.start.distance(b)){
                        return true;
                    }
                    else{
                        return false;
                    }
                });
                
                for(auto stitch : emb_points){
                    patch.addVertex(stitch);
                    jumps.push_back(false);
                }
                emb_points.clear();
            }
        }
        
        
        if ((end - patch.getVertices().back()).length() > 0.1 * ppmm){
            patch.addVertex(end);
            jumps.push_back(false);
        }
        
        last_end = end;
        swap = !swap;
    }
    
    return patch;
}

// FILL_TYPE_NORMAL
pesPolyline pesEMBFill::section_to_patch2(const std::vector<Line>& group_of_segments,
                                          const float angle,
                                          const float max_stitch_length,
                                          const Preset& preset,
                                          std::vector<bool>& jumps,
                                          bool swap) {
    const auto unit_per_mm = 10.0f;
    const auto cMaxStitchLength = 0.1f * unit_per_mm;
    const auto cMaxTrimAtLength = pesGetDocument()->getTrimAtLength() * unit_per_mm;
    const auto num_stagger = (int)preset.row_stagger.size();

    pesPolyline patch;    
    pesVec2f last_end(0, 0);
        
    if (num_stagger > 0) {
        std::vector<float> stagger;
        for (const auto& s : preset.row_stagger) {
            stagger.push_back(s * unit_per_mm);
        }
        
        for (const auto& segment : group_of_segments) {
            pesVec2f beg = segment.start;
            pesVec2f end = segment.end;

            if (swap) {
                std::swap(beg, end);
                last_s_section_to_patch2 = segment.end;
                last_e_section_to_patch2 = segment.start;
            } 
            else {
                last_s_section_to_patch2 = segment.start;
                last_e_section_to_patch2 = segment.end;
            }
            
            if (last_end.length() == 0) {
                jumps.push_back(true);
            }
            else if (beg.distance(last_end) > cMaxTrimAtLength) { // Jimmy - fill end before expected
                jumps.push_back(true);
            }
            else {
                jumps.push_back(false);
            }
            patch.addVertex(beg);

            const auto row_direction = (end - beg).normalize();
            const auto segment_length = end.distance(beg);
            const auto row_num = segment.lineNumber % num_stagger;
            const auto stagger_offset = stagger[row_num];
            const auto stitch_offset = fmodf(beg.dot(east(angle)) - stagger_offset , max_stitch_length);
            auto first_stitch = beg - stitch_offset * east(angle);
            
            // we might have chosen our first stitch just outside this row, so move back in
            if ((first_stitch - beg).dot(row_direction) < 0) {
                first_stitch += row_direction * max_stitch_length;
            }
            
            auto offset = (first_stitch - beg).length();
            while (offset < segment_length) {
                patch.addVertex(beg + offset * row_direction);
                jumps.push_back(false);
                offset += max_stitch_length;
            }
            
            if ((end - patch.getVertices().back()).length() > cMaxStitchLength) {
                patch.addVertex(end);
                jumps.push_back(false);
            }
            
            last_end = end;
            swap = !swap;
        }
    }
    
    return patch;
}

// FILL_TYPE_PATTERN
pesPolyline pesEMBFill::section_to_patch2(const std::vector<Line>& group_of_segments,
                                          std::vector<bool>& jumps,
                                          bool swap) {

    const float unit_per_mm = 10.0f;
    const auto cMaxStitchLength = 0.1f * unit_per_mm;
    const auto cMaxTrimAtLength = pesGetDocument()->getTrimAtLength() * unit_per_mm;

    pesPolyline patch;
    pesVec2f last_end(0, 0);
    
    for(const auto& segment : group_of_segments){
        pesVec2f beg = segment.start;
        pesVec2f end = segment.end;
        if (swap) {
            std::swap(beg, end);
            last_s_section_to_patch2 = segment.end;
            last_e_section_to_patch2 = segment.start;
        } 
        else {
            last_s_section_to_patch2 = segment.start;
            last_e_section_to_patch2 = segment.end;
        }

        if (last_end.length() == 0) {
            jumps.push_back(true);
        }
        else if (beg.distance(last_end) > cMaxTrimAtLength) {
            jumps.push_back(true);
        }
        else{
            jumps.push_back(false);
        }
        patch.addVertex(beg);
        
        std::vector<pesPath> acc_grids;
        for(const auto& grid : _grids){
            const auto& vertices = grid.getOutline().front().getVertices();
            int n = (int)vertices.size();
            if (n > 1) {
                pesVec2f result;
                auto hasIntersect = false;
                for (int h = 0, i = 1; i < n; h++, i++) {
                    auto& p0 = vertices[h];
                    auto& p1 = vertices[i];
                    if (pesLineSegmentIntersection(beg, end, p0, p1, result)) {
                        hasIntersect = true;
                        break;
                    }
                }
                
                if (hasIntersect) {
                    acc_grids.push_back(grid);
                }
                else {
                    if (grid.inside(beg.x, beg.y) || grid.inside(end.x, end.y)) {
                        acc_grids.push_back(grid);
                    }
                }
            }
        }

        if (acc_grids.size()) {
            pesEMBFill::Line line(beg, end);
            std::vector<pesVec2f> emb_points;
            for(const auto& acc_grid : acc_grids){
                const auto& v = acc_grid.getOutline().front().getVertices().front();
                _patternResized.translate(v.x, v.y);
                auto points = intersect(_patternResized, line);
                _patternResized.translate(-v.x, -v.y);
                if (points.size()) {
                    emb_points.insert(emb_points.end(), points.begin(), points.end());
                }
            }
                       
            if (emb_points.size() > 1) {
                pesSort(emb_points, [=](const auto& a, const auto& b) -> bool {
                    return line.start.distance(a) < line.start.distance(b);
                });
                
                for (const auto& stitch : emb_points) {
                    patch.addVertex(stitch);
                    jumps.push_back(false);
                }
                emb_points.clear();
            }
        }
        
        if ((end - patch.getVertices().back()).length() > cMaxStitchLength) {
            patch.addVertex(end);
            jumps.push_back(false);
        }
        
        last_end = end;
        swap = !swap;
    }

    return patch;
}



void pesEMBFill::section_to_patch_stitches(vector<Line> &group_of_segments, float angle, float row_spacing, float max_stitch_length, const Preset& preset, pesStitchBlock & stitches){
    bool swap = false;
    pesVec2f last_end(0,0);
    
    int pixels_per_mm = 10;
    int num_stagger = (int)preset.row_stagger.size();
    bool firstStitches = true;
    
    if(num_stagger>0){
        std::vector<float> stagger;
        for(auto s : preset.row_stagger){
            stagger.push_back(s*pixels_per_mm);
        }
        
        for(const auto& segment : group_of_segments){
            
            pesVec2f beg = segment.start;
            pesVec2f end = segment.end;
            if(swap){
                std::swap(beg, end);
            }
            
            pesVec2f row_direction = (end-beg).normalize();
            float segment_length = end.distance(beg);
            
            if (last_end.length() == 0) {
                stitches.addJump(beg);
                firstStitches = false;
            }
            else if (beg.distance(last_end) > row_spacing * 2) {
                stitches.addJump(beg);
                firstStitches = false;
            }
            else{
                stitches.addStitch(beg);
            }
            
            int row_num = segment.lineNumber % num_stagger;
            float stagger_offset = stagger[row_num];
            float stitch_offset = fmodf(beg.dot(east(angle)) - stagger_offset , max_stitch_length);
            pesVec2f first_stitch = beg - stitch_offset * east(angle);
            
            if((first_stitch - beg).dot(row_direction) < 0)
                first_stitch += row_direction * max_stitch_length;
            
            float offset = (first_stitch - beg).length();
            
            while (offset < segment_length) {
                if (firstStitches) {
                    stitches.addJump(beg + offset * row_direction);
                    firstStitches = false;
                }
                else{
                    stitches.addStitch(beg + offset * row_direction);
                }
                offset += max_stitch_length;
            }
            
            if ((end - stitches.polyline.getVertices().back()).length() >= 0.1 * pixels_per_mm){
                if(firstStitches){
                    stitches.addJump(end);
                    firstStitches = false;
                }
                else{
                    stitches.addStitch(end);
                }
            }
            
            last_end = end;
            swap = !swap;
        }
    }
}

void pesEMBFill::section_to_patch_stitches(vector<Line> &group_of_segments, float angle, float row_spacing, pesStitchBlock & stitches){
    
//    pesPolyline patch;
//    bool first_segment = true;
    bool swap = false;
    pesVec2f last_end(0,0);
    
    int ppmm = 10;
    bool firstStitches = true;
    
    for(const auto& segment : group_of_segments){
        
        pesVec2f beg = segment.start;
        pesVec2f end = segment.end;
        if(swap){
            std::swap(beg, end);
        }
        
//        pesVec2f row_direction = (end-beg).normalize();
//        float segment_length = end.distance(beg);
        
        // only stitch the first point if it's a reasonable distance away from the last stitch
//        if(last_end.length()==0 || beg.distance(last_end)>=row_spacing*0.9){
////            patch.addVertex(beg);
//            if(firstStitches){
//                stitches.addJump(beg);
//                firstStitches = false;
//            }
//            else{
//                stitches.addStitch(beg);
//            }
//        }
        
        if(last_end.length()==0){
            stitches.addJump(beg);
            firstStitches = false;
        }
        else if(beg.distance(last_end)>row_spacing*2){
            stitches.addJump(beg);
            firstStitches = false;
        }
        else{
            stitches.addStitch(beg);
        }
        
        vector<pesPath> acc_grids;
        for(auto grid : _grids){
            auto & vertices = grid.getOutline().front().getVertices();
            int n = (int)vertices.size();
            if(n>1){
                pesVec2f result;
                bool hasIntersect = false;
                for(int i=1; i<n; i++){
                    pesVec2f p0 = vertices[i-1];
                    pesVec2f p1 = vertices[i];
                    bool b = pesLineSegmentIntersection(beg, end, p0, p1, result);
                    if(b){
                        acc_grids.push_back(grid);
                        hasIntersect = true;
                        break;
                    }
                }
                
                if(!hasIntersect){
                    if(grid.inside(beg.x, beg.y)||grid.inside(end.x, end.y)){
                        acc_grids.push_back(grid);
                    }
                }
            }
        }
        if(acc_grids.size()){
            pesEMBFill::Line line(beg, end);
            vector<pesVec2f> emb_points;
            for(auto acc_grid : acc_grids){
                auto v = acc_grid.getOutline()[0].getVertices()[0];
                _patternResized.translate(v.x, v.y);
                auto points = intersect(_patternResized, line);
                _patternResized.translate(-v.x, -v.y);
                if(points.size()){
                    emb_points.insert(emb_points.end(), points.begin(), points.end());
                    points.clear();
                }
            }
            
            
            if(emb_points.size()>1){
                pesSort(emb_points, [=](pesVec2f &a, pesVec2f &b)->bool{
                    if(line.start.distance(a) < line.start.distance(b)){
                        return true;
                    }
                    else{
                        return false;
                    }
                });
                
                for(auto stitch : emb_points){
//                    patch.addVertex(stitch);
                    if(firstStitches){
                        stitches.addJump(stitch);
                        firstStitches = false;
                    }
                    else{
                        stitches.addStitch(stitch);
                    }
                }
                emb_points.clear();
            }
        }
        
        
//        if ((end - patch.getVertices().back()).length() > 0.1 * ppmm)
//            patch.addVertex(end);
        
        if ((end - stitches.polyline.getVertices().back()).length() > 0.1 * ppmm){
//            patch.addVertex(end);
            if(firstStitches){
                stitches.addJump(end);
                firstStitches = false;
            }
            else{
                stitches.addStitch(end);
            }
        }
        
        last_end = end;
        swap = !swap;
    }
    
//    return patch;
}

pesPolyline pesEMBFill::section_from_corner( vector<pesEMBFill::Line> section, pesVec2f start_corner, float angle, float row_spacing, std::vector<bool> & jumps){
    
//    if(start_corner!=section[0].start && start_corner!=section[0].end){
//        std::reverse(section.begin(), section.end());
//    }
//
//    if(start_corner!=section[0].start){
//        for(int i=0; i<(int)section.size(); i++){
//            std::swap(section[i].start, section[i].end);
//        }
//    }
    
    if(!start_corner.match(section[0].start, 1) && !start_corner.match(section[0].end)){
        std::reverse(section.begin(), section.end());
    }

    if(!start_corner.match(section[0].start, 1)){
        for(int i=0; i<(int)section.size(); i++){
            std::swap(section[i].start, section[i].end);
        }
    }
    
    return section_to_patch(section, angle, row_spacing, jumps);
}

void pesEMBFill::section_from_corner_stitches( std::vector<pesEMBFill::Line> section, pesVec2f start_corner, float angle, float row_spacing, pesStitchBlock & stitches){
    
//    if(start_corner!=section[0].start && start_corner!=section[0].end){
//        std::reverse(section.begin(), section.end());
//    }
//
//    if(start_corner!=section[0].start){
//        for(int i=0; i<(int)section.size(); i++){
//            std::swap(section[i].start, section[i].end);
//        }
//    }
    
    if(!start_corner.match(section[0].start, 1) && !start_corner.match(section[0].end)){
        std::reverse(section.begin(), section.end());
    }

    if(!start_corner.match(section[0].start, 1)){
        for(int i=0; i<(int)section.size(); i++){
            std::swap(section[i].start, section[i].end);
        }
    }
    
    section_to_patch_stitches(section, angle, row_spacing, stitches);
}

pesPolyline pesEMBFill::section_from_corner( vector<pesEMBFill::Line> section, pesVec2f start_corner, float angle, float row_spacing, float max_stitch_length, const Preset& preset, std::vector<bool> & jumps){
    
//    if(start_corner!=section[0].start && start_corner!=section[0].end){
//        std::reverse(section.begin(), section.end());
//    }
//
//    if(start_corner!=section[0].start){
//        for(int i=0; i<(int)section.size(); i++){
//            std::swap(section[i].start, section[i].end);
//        }
//    }
    
    if(!start_corner.match(section[0].start, 1) && !start_corner.match(section[0].end)){
        std::reverse(section.begin(), section.end());
    }

    if(!start_corner.match(section[0].start, 1)){
        for(int i=0; i<(int)section.size(); i++){
            std::swap(section[i].start, section[i].end);
        }
    }
    
    return section_to_patch(section, angle, row_spacing, max_stitch_length, preset, jumps);
}

void pesEMBFill::section_from_corner_stitches( vector<pesEMBFill::Line> section, pesVec2f start_corner, float angle, float row_spacing, float max_stitch_length, const Preset& preset, pesStitchBlock & stitches){
    
//    if(start_corner!=section[0].start && start_corner!=section[0].end){
//        std::reverse(section.begin(), section.end());
//    }
//
//    if(start_corner!=section[0].start){
//        for(int i=0; i<(int)section.size(); i++){
//            std::swap(section[i].start, section[i].end);
//        }
//    }
    
    if(!start_corner.match(section[0].start, 1) && !start_corner.match(section[0].end)){
        std::reverse(section.begin(), section.end());
    }

    if(!start_corner.match(section[0].start, 1)){
        for(int i=0; i<(int)section.size(); i++){
            std::swap(section[i].start, section[i].end);
        }
    }
    
    return section_to_patch_stitches(section, angle, row_spacing, max_stitch_length, preset, stitches);
}

std::vector<pesVec2f> pesEMBFill::intersect(const pesPath& path, pesEMBFill::Line line) {
    std::vector<pesVec2f> points;
    for(const auto& outline : path.getOutline()){
        const auto& vertices = outline.getVertices();
        int n = (int)vertices.size();
        if(n>1){
            pesVec2f result;
            for (int h = 0, i = 1; i < n; h++, i++) {
                pesVec2f p0 = vertices[h];
                pesVec2f p1 = vertices[i];
                bool b = pesLineSegmentIntersection(line.start, line.end, p0, p1, result);
                if(b)
                    points.push_back(result);
            }
        }
    }
    if(points.size()>1){
        pesSort(points, [=](pesVec2f &a, pesVec2f &b)->bool{
            if(line.start.distance(a) < line.start.distance(b)){
                return true;
            }
            else{
                return false;
            }
        });
    }
    return points;
}


// MARK: - pesEMBFillPreset
namespace pesEMBFillPreset{
    
    static pesEMBFillPreset::Preset __preset = pesEMBFillPreset::TRADITIONAL;
    void setDefaultPreset(pesEMBFillPreset::Preset preset){
        __preset = preset;
    }
    
    pesEMBFill::Preset getDefaultPreset(){
        switch (__preset) {
            case pesEMBFillPreset::TRADITIONAL:
                return pesEMBFillPreset::Traditional();
                
            case pesEMBFillPreset::BRICK:
                return pesEMBFillPreset::Brick();
                
            case pesEMBFillPreset::PILLAR:
                return pesEMBFillPreset::Pillar();
                
            case pesEMBFillPreset::ZIGZAG:
                return pesEMBFillPreset::Zigzag();
                
            case pesEMBFillPreset::WAVE:
                return pesEMBFillPreset::Wave();
                
            case pesEMBFillPreset::FAN:
                return pesEMBFillPreset::Fan();
                
            case pesEMBFillPreset::PATT01:
                return pesEMBFillPreset::P1();
                
            case pesEMBFillPreset::PATT02:
                return pesEMBFillPreset::P2();
                
            case pesEMBFillPreset::PATT03:
                return pesEMBFillPreset::P3();
                
            case pesEMBFillPreset::PATT04:
                return pesEMBFillPreset::P4();
                
            case pesEMBFillPreset::PATT05:
                return pesEMBFillPreset::P5();
                
            case pesEMBFillPreset::PATT06:
                return pesEMBFillPreset::P6();
                
            case pesEMBFillPreset::PATT07:
                return pesEMBFillPreset::P7();
                
            case pesEMBFillPreset::PATT08:
                return pesEMBFillPreset::P8();
                
            case pesEMBFillPreset::PATT09:
                return pesEMBFillPreset::P9();
                
            case pesEMBFillPreset::PATT10:
                return pesEMBFillPreset::P10();
                
            default:
                return pesEMBFillPreset::Traditional();
        }
    }
    
    
    
    pesEMBFill::Preset Traditional(){
        pesEMBFill::Preset patt;
        patt.row_stagger.push_back(0);
        patt.row_stagger.push_back(1);
        patt.row_stagger.push_back(2);
        patt.row_stagger.push_back(3);
        return patt;
    }
    
    pesEMBFill::Preset Brick(){
        pesEMBFill::Preset patt;
        patt.row_stagger.push_back(0);
        patt.row_stagger.push_back(2);
        return patt;
    }
    
    pesEMBFill::Preset Pillar(){
        pesEMBFill::Preset patt;
        patt.row_stagger.push_back(0);
        return patt;
    }
    
    pesEMBFill::Preset Zigzag(){
        pesEMBFill::Preset pattern;
        pattern.row_stagger.push_back(0.0);
        pattern.row_stagger.push_back(0.5);
        pattern.row_stagger.push_back(1.0);
        pattern.row_stagger.push_back(1.5);
        pattern.row_stagger.push_back(2.0);
        pattern.row_stagger.push_back(2.5);
        pattern.row_stagger.push_back(3.0);
        pattern.row_stagger.push_back(2.5);
        pattern.row_stagger.push_back(2.0);
        pattern.row_stagger.push_back(1.5);
        pattern.row_stagger.push_back(1.0);
        pattern.row_stagger.push_back(0.5);
        return pattern;
    }
    
    pesEMBFill::Preset Wave(){
        pesEMBFill::Preset patt;
        int num_stagger = 12;
        float step = (float)(TWO_PI / num_stagger);
        for(int i=0; i<num_stagger; i++){
            float angle = step * i;
            patt.row_stagger.push_back(sin(angle));
        }
        return patt;
    }
    
    pesEMBFill::Preset Fan(){
        pesEMBFill::Preset patt;
        int num_stagger = 12;
        float step = (float)(PI / num_stagger);
        for(int i=0; i<num_stagger; i++){
            float angle = step * i;
            patt.row_stagger.push_back(sin(angle)*2);
        }
        return patt;
    }
    
    pesEMBFill::Preset P1(){
        pesEMBFill::Preset patt;
        patt.row_stagger.push_back(0);
        patt.row_stagger.push_back(0);
        patt.row_stagger.push_back(2);
        patt.row_stagger.push_back(2);
        
        return patt;
    }
    
    pesEMBFill::Preset P2(){
        pesEMBFill::Preset patt;
        patt.row_stagger.push_back(0.0f);
        patt.row_stagger.push_back(0.1f);
        patt.row_stagger.push_back(0.2f);
        patt.row_stagger.push_back(0.3f);
        patt.row_stagger.push_back(0.4f);
        patt.row_stagger.push_back(2.0f);
        patt.row_stagger.push_back(2.1f);
        patt.row_stagger.push_back(2.2f);
        patt.row_stagger.push_back(2.3f);
        patt.row_stagger.push_back(2.4f);
        
        return patt;
    }
    
    pesEMBFill::Preset P3(){
        pesEMBFill::Preset patt;
        
        patt.row_stagger.push_back(2);
        patt.row_stagger.push_back(1.5);
        patt.row_stagger.push_back(1);
        patt.row_stagger.push_back(0.5);
        patt.row_stagger.push_back(0);
        patt.row_stagger.push_back(2);
        patt.row_stagger.push_back(2.5);
        patt.row_stagger.push_back(3);
        patt.row_stagger.push_back(3.5);
        patt.row_stagger.push_back(0);
        return patt;
    }
    
    pesEMBFill::Preset P4(){
        pesEMBFill::Preset patt;
        
        int num_stagger = 4;
        float step = (float)(M_PI_2 / num_stagger);
        for(int i=0; i<=num_stagger; i++){
            float angle = step * i;
            patt.row_stagger.push_back(-sin(angle));
        }
        for(int i=0; i<=num_stagger; i++){
            float angle = step * i;
            patt.row_stagger.push_back(sin(angle));
        }
        
        return patt;
    }
    
    pesEMBFill::Preset P5(){
        
        pesEMBFill::Preset patt;
        patt.row_stagger.push_back(0.0f);
        patt.row_stagger.push_back(0.4f);
        patt.row_stagger.push_back(0.8f);
        patt.row_stagger.push_back(0.4f);
        patt.row_stagger.push_back(0.0f);
        patt.row_stagger.push_back(2.0f);
        patt.row_stagger.push_back(1.6f);
        patt.row_stagger.push_back(1.2f);
        patt.row_stagger.push_back(1.6f);
        patt.row_stagger.push_back(2.0f);
        
        return patt;
    }
    
    pesEMBFill::Preset P6(){
        pesEMBFill::Preset patt;
        patt.row_stagger.push_back(0.0f);
        patt.row_stagger.push_back(2.4f);
        patt.row_stagger.push_back(0.8f);
        patt.row_stagger.push_back(3.2f);
        patt.row_stagger.push_back(1.6f);
        
        return patt;
    }
    
    pesEMBFill::Preset P7(){
        pesEMBFill::Preset patt;
        patt.row_stagger.push_back(0.0f);
        patt.row_stagger.push_back(0.4f);
        patt.row_stagger.push_back(1.6f);
        patt.row_stagger.push_back(2.0f);
        patt.row_stagger.push_back(3.2f);
        patt.row_stagger.push_back(3.6f);
        patt.row_stagger.push_back(0.8f);
        patt.row_stagger.push_back(1.2f);
        patt.row_stagger.push_back(2.4f);
        patt.row_stagger.push_back(2.8f);
        
        return patt;
    }
    
    pesEMBFill::Preset P8(){
        pesEMBFill::Preset patt;
        patt.row_stagger.push_back(0.0f);
        patt.row_stagger.push_back(3.6f);
        patt.row_stagger.push_back(3.2f);
        patt.row_stagger.push_back(2.8f);
        patt.row_stagger.push_back(2.4f);
        patt.row_stagger.push_back(2.0f);
        patt.row_stagger.push_back(1.6f);
        patt.row_stagger.push_back(1.2f);
        patt.row_stagger.push_back(0.8f);
        patt.row_stagger.push_back(0.4f);
        
        return patt;
    }
    
    pesEMBFill::Preset P9(){
        pesEMBFill::Preset patt;
        patt.row_stagger.push_back(0.0f);
        patt.row_stagger.push_back(0.0f);
        patt.row_stagger.push_back(0.0f);
        patt.row_stagger.push_back(0.0f);
        patt.row_stagger.push_back(0.5f);
        patt.row_stagger.push_back(0.5f);
        patt.row_stagger.push_back(0.5f);
        patt.row_stagger.push_back(0.5f);
        
        return patt;
    }
    
    pesEMBFill::Preset P10(){
        pesEMBFill::Preset patt;
        patt.row_stagger.push_back(0.0f);
        patt.row_stagger.push_back(0.0f);
        patt.row_stagger.push_back(0.0f);
        patt.row_stagger.push_back(0.0f);
        patt.row_stagger.push_back(2.0f);
        patt.row_stagger.push_back(2.0f);
        patt.row_stagger.push_back(2.0f);
        patt.row_stagger.push_back(2.0f);
        
        return patt;
    }
    
}
