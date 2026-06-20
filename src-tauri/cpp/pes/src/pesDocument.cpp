//
//  pesDocument.cpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 12/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//
#include "include/core/SkColorFilter.h"
#include "include/core/SkBlendMode.h"
#include "pesDocument.hpp"
#include "pesEMBClassify.hpp"
#include "pesEncoder.hpp"
#include "pesSatinOutline.hpp"
#include "pesUtility.hpp"
#include "json.hpp"
#include "include/core/SkEncodedImageFormat.h"
#include "include/encode/SkPngEncoder.h"
#include "include/utils/SkBase64.h"
#include "include/core/SkData.h"
#include "include/core/SkScalar.h"
#include "include/core/SkSurface.h"
#include "include/core/SkVertices.h"
#include "pesPathUtility.hpp"

#include "include/utils/SkTextUtils.h"

using namespace nlohmann;
//using namespace std;

// backward compatible
const int MaxFontSizes = 20;
const int fontSizes[MaxFontSizes] = {4,  5,  6,  7,  8,  9,  10, 11, 12, 13,
                               14, 15, 16, 18, 20, 22, 24, 26, 28, 30};

//char strAppNameMax32[32 + 1] = {0};

// int pesBackgroundWidth = 100; // default 100 mm.(1cm)
// int pesBackgroundHeight = 100; // default 100 mm.(1cm)

const char* pesDefaultText = "ภิญญ์จักรปัก สวัสดีค่ะ";


// backward-compattible
void reCreatePES2_Text(pesData& data);
void reCreatePES2_Shape(pesData& data);

static void release_direct_surface_storage(void* pixels, void* context);

pesDocument::pesDocument() {
    strAppName = "PESLib";
    strName = "Untitled";
    pesBackgroundWidth = 100; // default 100 mm.(10cm.)
    pesBackgroundHeight = 100; // default 100 mm.(10cm.)
    pesHasChanged = false;
}

pesDocument::~pesDocument(){
    __pesDataList.clear();
}

static shared_ptr<pesDocument> __documentInstance = nullptr;
//vector<shared_ptr<pesData>> __pesDataList;

bool _ready = false;

pesDocument* pesDocument::getInstance() {
    if (__documentInstance == nullptr) {
        if (!_ready) {
            loadAssets();
        }
        __documentInstance = make_shared<pesDocument>(pesDocument());
    }
    return __documentInstance.get();
}

pesDocument * pesDocument::createInstance(){
    if (!_ready) {
        loadAssets();
    }
    return new pesDocument();
}

std::string pesDocument::getAppName() {
//    return std::string(strAppNameMax32);
    return strAppName;
}

void pesDocument::setName(const std::string name) {
    strName = name;
}
std::string pesDocument::getName() const {
    return strName;
}

void pesDocument::setAppName(const std::string strMax32) {
//    strAppNameMax32[0] = '\0';
//    strncpy(strAppNameMax32, strMax32.c_str(), 32);
//    strAppNameMax32[32] = '\0';
    
    strAppName = strMax32;
}

shared_ptr<pesData> pesDocument::getDataObject(int idx) const { 
    return __pesDataList[idx]; 
}

void pesDocument::newDocument() {
    __pesDataList.clear();
    __pesGroups.clear();
    __nextGroupId = 1;
    strAppName = "PESLib";
    strName = "Untitled";
    pesHasChanged = false;
    updateInfo();
}

const std::vector<std::shared_ptr<pesData>>& pesDocument::getDataObjects() const { 
    return __pesDataList;
}

std::vector<std::shared_ptr<pesData>>& pesDocument::getDataObjects() {
    return __pesDataList;
}

pesData::Parameter& pesDocument::getDataParameter(int idx) const { 
    return __pesDataList[idx]->parameter; 
}

void pesDocument::setDataParameterText(int idx, std::string str) { 
    __pesDataList[idx]->parameter.text = str; 
}
void pesDocument::setDataParameterFont(int idx, std::string str) { 
    __pesDataList[idx]->parameter.fontName = str; 
}
void pesDocument::setDataParameterFontSize(int idx, int fontSize) {
    __pesDataList[idx]->parameter.fontSize = fontSize;
}
void pesDocument::setDataParameterVisible(int idx, bool visible) { 
    __pesDataList[idx]->parameter.visible = visible; 
}
void pesDocument::setDataParameterLocked(int idx, bool locked) {
    if (__pesDataList[idx]->parameter.type != pesData::ObjectType::OBJECT_TYPE_BACKGROUND) {
        __pesDataList[idx]->parameter.locked = locked;
    }
}
void pesDocument::setDataParameterTextEffect(int idx, int effectIndex) { 
    __pesDataList[idx]->parameter.shapeIndex = effectIndex; 
}
void pesDocument::setDataParameterTextEffectAngle(int idx, float value) {
    __pesDataList[idx]->parameter.angleValue = value;
}
void pesDocument::setDataParameterTextEffectRadius(int idx, float value) {
    __pesDataList[idx]->parameter.radiusValue = value;
}

void pesDocument::setDataParameterTextExtraLetterSpace(int idx, int value){
    __pesDataList[idx]->parameter.extraLetterSpace = value;
}
void pesDocument::setDataParameterTextExtraSpace(int idx, int value){
    __pesDataList[idx]->parameter.extraSpace = value;
}

void pesDocument::setDataParameterFillColor(int idx, int colorIndex) {
    __pesDataList[idx]->parameter.fillColorIndex = colorIndex;
    //for(auto & p : __pesDataList[idx]->paths){
    //    p.setFillColor(pesGetBrotherColor(colorIndex));
    //}
    for(auto & block : __pesDataList[idx]->fillBlocks){
        block.setColorFromIndex(colorIndex);
    }    
}
void pesDocument::setDataParameterStrokeColor( int idx, int colorIndex ) {
    __pesDataList[idx]->parameter.colorIndex = colorIndex;
    //for(auto & p : __pesDataList[idx]->paths){
    //    p.setStrokeColor(pesGetBrotherColor(colorIndex));
    //}
    for(auto & block : __pesDataList[idx]->strokeBlocks){
        block.setColorFromIndex(colorIndex);
    }
}
void pesDocument::setDataParameterFillTypeIndex(int idx, int fillTypeIndex) { 
    __pesDataList[idx]->parameter.fillTypeIndex = fillTypeIndex;
    __pesDataList[idx]->parameter.fillType = (pesData::FillType)(pesData::FILL_TYPE_NONE + fillTypeIndex);
}
void pesDocument::setDataParameterStrokeTypeIndex(int idx, int strokeTypeIndex) { 
    __pesDataList[idx]->parameter.strokeTypeIndex = strokeTypeIndex;
    __pesDataList[idx]->parameter.strokeType = (pesData::StrokeType)(pesData::STROKE_TYPE_NONE + strokeTypeIndex);
}
void pesDocument::setDataParameterTextBorder(int idx, bool isBorder) { 
    __pesDataList[idx]->parameter.border = isBorder; 
}
void pesDocument::setDataParameterTextBorderGapX(int idx, int value) {
    __pesDataList[idx]->parameter.borderGap = value;
}
void pesDocument::setDataParameterTextBorderGapY(int idx, int value) {
    __pesDataList[idx]->parameter.borderGapY = value;
}
void pesDocument::setDataParameterTextBorderColor(int idx, int colorIndex) {
    __pesDataList[idx]->parameter.borderColorIndex = colorIndex;
}
void pesDocument::setDataParameterTextItalic(int idx, bool isItalic) { 
    __pesDataList[idx]->parameter.italic = isItalic; 
}
void pesDocument::setDataParameterTextDensity(int idx, float density) { 
    __pesDataList[idx]->parameter.fill.density = density; 
}
void pesDocument::setDataParameterTextPullCompensate(int idx, float pullCompensate) { 
    __pesDataList[idx]->parameter.pullCompensate = pullCompensate; 
}

void pesDocument::setDataParameterStrokeRunPitch(int idx, float value){
    __pesDataList[idx]->parameter.strokeRunPitch = value;
}
void pesDocument::setDataParameterStrokeWidth(int idx, float value){
    __pesDataList[idx]->parameter.strokeWidth = value;
}
void pesDocument::setDataParameterStrokeDensity(int idx, float value){
    __pesDataList[idx]->parameter.strokeDensity = value;
}
void pesDocument::setDataParameterStrokeRunningInset(int idx, float value){
    __pesDataList[idx]->parameter.strokeRunningInset = value;
}

void pesDocument::setDataParameterStrokeMotifSize(int idx, float value){
    __pesDataList[idx]->parameter.motif.size = value;
}
void pesDocument::setDataParameterStrokeMotifSpacing(int idx, float value){
    __pesDataList[idx]->parameter.motif.spacing = value;
}

void pesDocument::setDataParameterFillDensity(int idx, float value){
    __pesDataList[idx]->parameter.fill.density = value;
}
void pesDocument::setDataParameterFillDirection(int idx, float value){
    __pesDataList[idx]->parameter.fill.sewDirection = value;
}

void pesDocument::setDataParameterFillPatternDensity(int idx, float value){
    __pesDataList[idx]->parameter.patternFill.density = value;
}
void pesDocument::setDataParameterFillPatternDirection(int idx, float value){
    __pesDataList[idx]->parameter.patternFill.sewDirection = value;
}
void pesDocument::setDataParameterFillPatternSize(int idx, float value){
    __pesDataList[idx]->parameter.patternFill.patternSize = value;
}

void pesDocument::setDataParameterFillMotifSize(int idx, float value){
    __pesDataList[idx]->parameter.motifFill.size = value;
}
void pesDocument::setDataParameterFillMotifSpacing(int idx, float value){
    __pesDataList[idx]->parameter.motifFill.spacing = value;
}
void pesDocument::setDataParameterFillMotifRowSpacing(int idx, float value){
    __pesDataList[idx]->parameter.motifFill.rowSpacing = value;
}

void pesDocument::setDataParameterFillUnderlay(int idx, bool value) {
    __pesDataList[idx]->parameter.fill.underlay = value;
}

void pesDocument::setDataParameterFillPatternUnderlay(int idx, bool value) {
    __pesDataList[idx]->parameter.patternFill.underlay = value;
}

void pesDocument::setDataParameterAutoSatinFillNoneOverlap(int idx, bool value) {
    __pesDataList[idx]->parameter.autoSatinFill.noneOverlap = value;
}

// vector<shared_ptr<pesData>> & pesDocument::getDataObjects(){
//    return __pesDataList;
//}

int pesDocument::addObject(pesData& data, bool nolimit) {
    int cnt = getObjectCount();
    if (nolimit || (cnt < pesMaxObjectCount)) {
        data.document = this;
        __pesDataList.push_back(make_shared<pesData>(data));
    }
    else{
        SkDebugf("pesDocument::addObject() : Cannot add object\n");
    }
    updateInfo();
    return getObjectCount();
}

int pesDocument::addObjectToXY(pesData& data, float x, float y, bool nolimit) {
    data.translate(x, y);
    //updateInfo();
    return addObject(data, nolimit);
}

int pesDocument::classifyObject(pesData& data) {
#ifndef NDEBUG
    cout << "classifyObject" << endl;
#endif
    pesStitchBlockList blocks;
    data.getStitchBlockList(blocks);
    if (blocks.size()) {
        for (auto& block : blocks) {
            auto results = pes_classify(block);

            pesData dataSatin;
            dataSatin.parameter.setType(pesData::OBJECT_TYPE_SCALABLE_SATINCOLUMN);
            dataSatin.parameter.isSatinColumnPath = true;
            dataSatin.parameter.fillType = pesData::FillType::FILL_TYPE_SATIN_COLUMN;
            dataSatin.parameter.strokeType = pesData::StrokeType::STROKE_TYPE_NONE;
            dataSatin.parameter.colorIndex = dataSatin.parameter.fillColorIndex = block.colorIndex;

            pesData dataRunning;
            dataRunning.parameter.setType(pesData::OBJECT_TYPE_SCALABLE_SVG_FILE);
            dataRunning.parameter.fillType = pesData::FillType::FILL_TYPE_NONE;
            dataRunning.parameter.strokeType = pesData::StrokeType::STROKE_TYPE_RUNNING;
            dataRunning.parameter.colorIndex = block.colorIndex;
            dataRunning.parameter.useColorFromPicker = false;

            pesData dataFilled;
            dataFilled.parameter.setType(pesData::OBJECT_TYPE_SCALABLE_SVG_FILE);
            dataFilled.parameter.fillType = pesData::FillType::FILL_TYPE_NORMAL;
            dataFilled.parameter.strokeType = pesData::StrokeType::STROKE_TYPE_RUNNING;
            dataFilled.parameter.colorIndex = block.colorIndex;
            dataFilled.parameter.useColorFromPicker = false;

            if (results.size()) {
                for (auto& r : results) {
                    if (r.type == pesEMBClassifyType::CLASSIFY_TYPE_SATIN && r.paths.size() == 2) {
                        dataSatin.paths.insert(dataSatin.paths.end(), r.paths.begin(),
                                               r.paths.end());
                    } else if (r.type == pesEMBClassifyType::CLASSIFY_TYPE_RUNNING &&
                               r.paths.size()) {
                        dataRunning.paths.insert(dataRunning.paths.end(), r.paths.begin(),
                                                 r.paths.end());
                    } else if (r.type == pesEMBClassifyType::CLASSIFY_TYPE_FILLED &&
                               r.paths.size()) {
                        //                        cout << "insert filled path" << endl;
                        dataFilled.paths.insert(dataFilled.paths.end(), r.paths.begin(),
                                                r.paths.end());
                    }
                }
            }

            if (dataRunning.paths.size()) {
                __pesDataList.push_back(make_shared<pesData>(dataRunning));
            }
            if (dataFilled.paths.size()) {
                //                cout << "insert dataFilled" << endl;
                __pesDataList.push_back(make_shared<pesData>(dataFilled));
            } else {
                //                cout << "cannot insert dataFilled (paths empty)" << endl;
            }
            if (dataSatin.paths.size()) {
                __pesDataList.push_back(make_shared<pesData>(dataSatin));
            }
        }
    }
    data.clear();
    return getObjectCount();
}

void pesDocument::clipPathForObject(int idx) {
    // Suggest by Github Copilot - GPT 4o
    if (idx < 0 || idx >= __pesDataList.size()) {
        std::cerr << "Index out of bounds" << std::endl;
        return;
    }

#ifndef NDEBUG
    cout << "cmd size : " << __pesDataList[idx]->paths.front().getCommands().size() << endl;
    cout << "fillType : " << __pesDataList[idx]->parameter.fillType << endl;
#endif

    if (__pesDataList[idx]->parameter.fillType == pesData::FillType::FILL_TYPE_NORMAL &&
        __pesDataList[idx]->paths.front().getCommands().size() == 2) {
        //        pesPath complexPath;
        //        std::vector<pesPolyline> simplifiedPath;
        ClipperLib::ClipType currentClipperType = ClipperLib::ctUnion;
        ClipperLib::JoinType currentClipperJoinType = ClipperLib::jtMiter;
        ClipperLib::EndType currentClipperEndType = ClipperLib::etClosedPolygon;
        float miterLimit = 2;
        float offsetDelta = 1.0;

#ifndef NDEBUG
        cout << "offsetDelta : " << offsetDelta << endl;
#endif

        std::vector<pesPolyline> clipSubjects;
        for (auto& p : __pesDataList[idx]->paths) {
            vector<pesPolyline> outlines = p.getOutline();
            clipSubjects.insert(clipSubjects.end(), outlines.begin(), outlines.end());
        }

#ifndef NDEBUG
        cout << "subjects(autoClose=false) : " << clipSubjects.size() << endl;
#endif

        //        pesClipper clipper;
        //        clipper.addPolylines(clipSubjects, ClipperLib::ptSubject, false);
        //
        //        std::vector<pesPolyline> clips = clipper.getClipped(currentClipperType);
        //        cout << "clips(autoClose=false) : " << clips.size() << endl;
        //        std::vector<pesPolyline> offsets = pesClipper::getOffsets(clips,
        //                                                                  offsetDelta,
        //                                                                  currentClipperJoinType,
        //                                                                  currentClipperEndType,
        //                                                                  miterLimit);

        //        std::vector<pesPolyline> offsets = pesClipper::getOffsets(clipSubjects,
        //                                                                  offsetDelta,
        //                                                                  currentClipperJoinType,
        //                                                                  currentClipperEndType,
        //                                                                  miterLimit);

        std::vector<pesPolyline> offsets = pesClipper::getOffsets(clipSubjects, offsetDelta);

#ifndef NDEBUG
        cout << "clip offsets : " << offsets.size() << endl;
#endif
        vector<pesPath>& paths = __pesDataList[idx]->paths;
        auto fillColor = paths.front().getFillColor();
        paths.clear();
        paths.resize(1);

        pesPath& path = paths.back();
        path.setStrokeColor(pesColor(75, 107, 175));
        path.setStrokeWidth(1.0);
        path.setFillColor(fillColor);
        path.setFilled(false);
        for (auto& offset : offsets) {
            for (int i = 0; i < offset.size(); i++) {
                if (i == 0) {
                    path.moveTo(offset[i].x, offset[i].y);
                } else {
                    path.lineTo(offset[i].x, offset[i].y);
                }
            }
            if (offset.isClosed()) {
                path.close();
            }
        }
        __pesDataList[idx]->paths = paths;
        __pesDataList[idx]->parameter.strokeType = pesData::StrokeType::STROKE_TYPE_NONE;
    } 
    else {
#ifndef NDEBUG
        cout << "cmd size : " << __pesDataList[idx]->paths.front().getCommands().size() << endl;
        cout << "fillType : " << __pesDataList[idx]->parameter.fillType << endl;
#endif
    }
}

bool pesDocument::moveObjectBack(int idx) {
    if (idx > 0 && idx < getObjectCount()) {
        std::swap(__pesDataList[idx], __pesDataList[idx - 1]);
        return true;
    }
    return false;
}

bool pesDocument::moveObjectFront(int idx) {
    if (idx >= 0 && idx < (getObjectCount() - 1)) {
        std::swap(__pesDataList[idx], __pesDataList[idx + 1]);
        return true;
    }
    return false;
}

bool pesDocument::mergeObject(int idx) {
    if (idx >= 0 && (idx + 1) < getObjectCount()) {
        bool canMerge = __pesDataList[idx]->canMerge(*__pesDataList[idx + 1]);
        if(canMerge){
            __pesDataList[idx]->rotate(__pesDataList[idx]->parameter.rotateDegree);
            __pesDataList[idx + 1]->rotate(__pesDataList[idx + 1]->parameter.rotateDegree);
            __pesDataList[idx]->parameter.rotateDegree = 0;
        }
        bool merged = __pesDataList[idx]->merge(*__pesDataList[idx + 1]);
        if (merged) deleteObject(idx + 1);
        updateInfo();
        return merged;
    }
    return false;
}

bool pesDocument::duplicateObject(int idx) {
    const int tt = 25;
    if (idx >= 0 && idx < getObjectCount() && getObjectCount() < pesMaxObjectCount) {
        pesData data = *__pesDataList[idx];
        auto it = __pesDataList.begin();
        it = __pesDataList.insert(it + (idx + 1), make_shared<pesData>(data));
        __pesDataList[idx + 1]->translate(tt, tt);
        __pesDataList[idx + 1]->recalculate();
        updateInfo();
        return true;
    }
    return false;
}

bool pesDocument::deleteObject(int idx) {
    if (idx >= 0 && idx < getObjectCount()) {
        __pesDataList[idx]->clear();
        __pesDataList.erase(__pesDataList.begin() + idx);
        updateInfo();
        return true;
    }
    return false;
}

// --- Layer groups ---------------------------------------------------------
// Groups are pure organizational metadata over the flat object list; a group
// is never a pesData object. Membership is per-object (Parameter::groupId).

const std::vector<pesGroup>& pesDocument::getGroups() const {
    return __pesGroups;
}

int pesDocument::createGroup(const std::string& name, int parentId) {
    pesGroup g;
    g.id = __nextGroupId++;
    g.parentId = parentId;
    g.name = name;
    g.collapsed = false;
    // place the new group after existing siblings
    int maxOrder = 0;
    for (const auto& e : __pesGroups)
        if (e.order > maxOrder) maxOrder = e.order;
    g.order = maxOrder + 1;
    __pesGroups.push_back(g);
    flagHasChanged();
    return g.id;
}

bool pesDocument::renameGroup(int id, const std::string& name) {
    for (auto& g : __pesGroups) {
        if (g.id == id) { g.name = name; flagHasChanged(); return true; }
    }
    return false;
}

bool pesDocument::deleteGroup(int id) {
    bool found = false;
    for (size_t i = 0; i < __pesGroups.size(); i++) {
        if (__pesGroups[i].id == id) {
            __pesGroups.erase(__pesGroups.begin() + i);
            found = true;
            break;
        }
    }
    if (!found) return false;
    // ungroup any members so no object points at a dead group
    for (auto& d : __pesDataList)
        if (d->parameter.groupId == id) d->parameter.groupId = 0;
    flagHasChanged();
    return true;
}

bool pesDocument::setGroupCollapsed(int id, bool collapsed) {
    for (auto& g : __pesGroups) {
        if (g.id == id) { g.collapsed = collapsed; flagHasChanged(); return true; }
    }
    return false;
}

bool pesDocument::setGroupOrder(int id, int order) {
    for (auto& g : __pesGroups) {
        if (g.id == id) { g.order = order; flagHasChanged(); return true; }
    }
    return false;
}

void pesDocument::setObjectGroup(int idx, int groupId) {
    if (idx >= 0 && idx < getObjectCount()) {
        __pesDataList[idx]->parameter.groupId = groupId;
        flagHasChanged();
    }
}

bool pesDocument::isGroupScalable(int id) const {
    // A group is scalable only if every member object is scalable. An empty
    // group is treated as scalable (nothing blocks it yet).
    for (const auto& d : __pesDataList) {
        if (d->parameter.groupId == id && !d->isScalable())
            return false;
    }
    return true;
}

//extern std::string PES5_ObjectTypeToString(int type);
//
//std::string pesDocument::getLayerTypeString( int i ) {
//    auto type = pesGetDocument()->getDataParameter(i).type;
//    return PES5_ObjectTypeToString(type);
//}
//
//bool ImageToBase64DataURI(sk_sp<SkImage> img, SkString* dst) {
//    sk_sp<SkData>pngData = img->encodeToData( SkEncodedImageFormat::kPNG, 100 );
//
//    size_t len = SkBase64::Encode(pngData->data(), pngData->size(), nullptr);
//
//    // The PNG can be almost arbitrarily large. We don't want to fill our logs with enormous URLs.
//    // Infra says these can be pretty big, as long as we're only outputting them on failure.
//    static const size_t kMaxBase64Length = 1024 * 1024;
//    if (len > kMaxBase64Length) {
//        dst->printf("Encoded image too large (%u bytes)", static_cast<uint32_t>(len));
//        return false;
//    }
//
//    dst->resize(len);
//    SkBase64::Encode(pngData->data(), pngData->size(), dst->data());
//    dst->prepend("data:image/png;base64,");
//    return true;
//}
//
//extern sk_sp<SkImage> PES5_GetPesImage(int index);
//
//std::string pesDocument::getLayerImageString( int i ) {
//    const auto img = PES5_GetPesImage(i);
//    if (img == nullptr) {
//        //SkDebugf("img[%d] == nullptr\n", i);
//        return "";
//    }
//    SkString s;
//    ImageToBase64DataURI( img, &s );
//    return std::string( s.c_str() );
//}
//
//extern void PES5_SetObjectSelectedIndex(int index);
//extern void PES5_SelectLayerList( int index );
//
//void pesDocument::selectLayer( int index ) {
//    SkDebugf( "Select: %i\n", index );
//    PES5_SelectLayerList( index );
//    PES5_SetObjectSelectedIndex(index);
//}


int pesDocument::getObjectCount() const { return static_cast<int>(__pesDataList.size()); }

pesVec2f __pesHoopSize(100, 100);
void pesDocument::setHoopSizeInMM(float w, float h) { __pesHoopSize.set(w, h); }

void pesDocument::setHoopSizeInMM(const pesVec2f& size) { __pesHoopSize.set(size.x, size.y); }

pesVec2f pesDocument::getHoopSizeInMM() const { return __pesHoopSize; }

float __pesTrimAtLength = 8;
void pesDocument::setTrimAtLength(float mm) { __pesTrimAtLength = mm; }

float pesDocument::getTrimAtLength() { return __pesTrimAtLength; }

bool pesDocument::loadPPESFromMemory(uintptr_t /* uint8_t*  */ iptr, size_t length) {
    if (iptr && length) {
        uint8_t* puint8 = reinterpret_cast<uint8_t*>(iptr);
        pesBuffer buffer((const char *)puint8, length);
        //sk_sp<SkData> data = SkData::MakeFromMalloc(puint8, length);

        return loadPPESFromBuffer( buffer );
    }
    return false;
}

//bool pesDocument::loadPPESFromBuffer(const pesBuffer& ppesBuff) const {
bool pesDocument::loadPPESFromBuffer(const pesBuffer& ppesBuff){
//    strAppNameMax32[0] = '\0';
    strAppName = "";
    __pesGroups.clear();   // layer groups (PPES v504+); empty for legacy files
    __nextGroupId = 1;
    vector<pesData> datas;
    int numLayer = 0;
    int curLayer = -1;
    int dataOffset = 0;
    bool hasLayerTypePES = false;
    bool hasPESData = false;
    bool hasPINNData = false;
    bool success = false;
    int PPES_FileVersion = 0;

    pesBackgroundWidth = 100; // default 100 mm.(10cm.)
    pesBackgroundHeight = 100; // default 100 mm.(10cm.)

    pesBuffer buff = const_cast<pesBuffer&>(ppesBuff);

    // replace "\r\n" with "\n"
    // replace "\r" with "\n"
//    char* src = buff.getData();
//    char* dst = src;
//    char* end = src + ppesBuff.size();
//    for (; src < end; *dst++ = *src++) {
//        if (*src == '\r') {
//            if (*(src+1) == '\n') {
//                src++;
//            } else {
//                *src = '\n';
//            }
//        }
//    }

    if (ppesBuff.size()) {
        int i = 0;
        for (pesBuffer::Line it = buff.getLines().begin(), end = buff.getLines().end(); it != end; ++it, ++i) {
            string line = *it;
            if( i == 0 && line != string("#PPES") ) {
                return false;
            }
            //if( i < 10 ) {
            //    SkDebugf( "Line %i: %s\n", i+1, line.c_str() );
            //}

            if (!line.empty()) {
                dataOffset += (int)(line.size() + 1);
                if (line == "[PESDATA]") {
                    hasPESData = true;
                    break;
                }
                if (line == "[PINNDATA]") {
                    hasPINNData = true;
                    break;
                }
                std::vector<string> sep = pesSplitString(line, "=");
                if (sep.size() >= 2) {
                    if (sep[0] == "Appname") {
//                        strncpy(strAppNameMax32, sep[1].c_str(), 32);
//                        strAppNameMax32[32] = '\0';
                        //SkDebugf( "AppName: %s\n", sep[1].c_str());
                        strAppName = sep[1];
                    } else if (sep[0] == "Version") {
                        if( sep[1] == string("5.00 Beta") ) {
                            PPES_FileVersion = 500;
                        }
                        else {
                            PPES_FileVersion = pesToInt(sep[1]);
                        }                        
                        //SkDebugf( "PPES Version: %i\n", PPES_FileVersion);
                    } else if (sep[0] == "numLayer") {
                        numLayer = pesToInt(sep[1]);
                        while ((int)datas.size() < numLayer) {
                            datas.emplace_back();
                        }
                    } else if (sep[0] == "BackgroundWidth") {
                        pesBackgroundWidth = pesToInt(sep[1]);
                        //SkDebugf("Background width=%i\n", pesBackgroundWidth);
                    } else if (sep[0] == "BackgroundHeight") {
                        pesBackgroundHeight = pesToInt(sep[1]);
                        //SkDebugf("Background height=%i\n", pesBackgroundHeight);
                    } else if (sep[0] == "LayerId") {
                        curLayer = pesToInt(sep[1]);
                    }
                    // Layer groups (PPES v504+). These header keys are unknown
                    // to old readers, so they are ignored there. Group.id always
                    // precedes its other Group.* keys, so .back() is the current.
                    else if (sep[0] == "numGroups") {
                        __pesGroups.clear();
                        __pesGroups.reserve(pesToInt(sep[1]));
                    } else if (sep[0] == "Group.id") {
                        pesGroup g; g.id = pesToInt(sep[1]);
                        __pesGroups.push_back(g);
                    } else if (sep[0] == "Group.parentId") {
                        if (!__pesGroups.empty()) __pesGroups.back().parentId = pesToInt(sep[1]);
                    } else if (sep[0] == "Group.order") {
                        if (!__pesGroups.empty()) __pesGroups.back().order = pesToInt(sep[1]);
                    } else if (sep[0] == "Group.collapsed") {
                        if (!__pesGroups.empty()) __pesGroups.back().collapsed = pesToInt(sep[1]) != 0;
                    } else if (sep[0] == "Group.name") {
                        if (!__pesGroups.empty()) __pesGroups.back().name = sep[1];
                    }

                    if (curLayer >= 0 && curLayer < numLayer) {
                        if (sep[0] == "type") {
                            datas[curLayer].parameter.type = (pesData::ObjectType)pesToInt(sep[1]);
                        } else if (sep[0] == "text") {
                            datas[curLayer].parameter.text = sep[1];
                            if (datas[curLayer].parameter.type == pesData::ObjectType::OBJECT_TYPE_PES) {
                                datas[curLayer].parameter.text = pesDefaultText;
                                hasLayerTypePES = true;
                            }
                        } else if (sep[0] == "groupId") {
                            datas[curLayer].parameter.groupId = pesToInt(sep[1]);
                        } else if (sep[0] == "fontIndex") {
                            datas[curLayer].parameter.fontIndex = pesToInt(sep[1]);
                        } else if (sep[0] == "fontSizeIndex") {
                            datas[curLayer].parameter.fontSizeIndex = pesToInt(sep[1]);
                        } else if (sep[0] == "colorIndex") {
                            datas[curLayer].parameter.colorIndex = pesToInt(sep[1]);
                        } else if (sep[0] == "baselineIndex") {
                            datas[curLayer].parameter.baselineIndex = pesToInt(sep[1]);
                        } else if (sep[0] == "radiusValue") {
                            datas[curLayer].parameter.radiusValue = pesToFloat(sep[1]);
                            if (datas[curLayer].parameter.type == pesData::ObjectType::OBJECT_TYPE_SCALABLE_SVG_FILE &&
                                datas[curLayer].parameter.radiusValue < 1) {
                                datas[curLayer].parameter.radiusValue = -1;
                            }
                        } else if (sep[0] == "angleValue") {
                            datas[curLayer].parameter.angleValue = pesToFloat(sep[1]);
                            if (datas[curLayer].parameter.type == pesData::ObjectType::OBJECT_TYPE_SCALABLE_SVG_FILE &&
                                datas[curLayer].parameter.angleValue < 1) {
                                datas[curLayer].parameter.angleValue = -1;
                            }
                        } else if (sep[0] == "rotateValue") {
                            datas[curLayer].parameter.rotateValue = pesToFloat(sep[1]);
                        } else if (sep[0] == "centerx") {
                            datas[curLayer].centerx = pesToInt(sep[1]);
                        } else if (sep[0] == "centery") {
                            datas[curLayer].centery = pesToInt(sep[1]);
                        } else if (sep[0] == "numColorChange") {
                            datas[curLayer].numColorChange = pesToInt(sep[1]);
                        } else if (sep[0] == "byteLength") {
                            datas[curLayer].byteLength = pesToInt(sep[1]);
                        }
                        // MARK: v.0.6
                        else if (sep[0] == "shapeIndex") {
                            datas[curLayer].parameter.shapeIndex = pesToInt(sep[1]);
                            // v0.6=>v0.9 backward compatible
                            if (datas[curLayer].parameter.shapeIndex == 3)
                                datas[curLayer].parameter.shapeIndex = 8;
                        } else if (sep[0] == "width") {
                            datas[curLayer].parameter.width = pesToFloat(sep[1]);
                        } else if (sep[0] == "height") {
                            datas[curLayer].parameter.height = pesToFloat(sep[1]);
                        } else if (sep[0] == "lineWidth") {
                            datas[curLayer].parameter.lineWidth = pesToFloat(sep[1]);
                        } else if (sep[0] == "density") {
                            datas[curLayer].parameter.density = pesToFloat(sep[1]);
                        }
                        // MARK: v 1.00
                        else if (sep[0] == "extraLetterSpace") {
                            datas[curLayer].parameter.extraLetterSpace = pesToInt(sep[1]);
                        } else if (sep[0] == "extraSpace") {
                            datas[curLayer].parameter.extraSpace = pesToInt(sep[1]);
                        }
                        // MARK: v 1.90
                        else if (sep[0] == "locked") {
                            datas[curLayer].parameter.locked = pesToInt(sep[1]) == 0 ? false : true;
                        } else if (sep[0] == "visible") {
                            datas[curLayer].parameter.visible =
                                    pesToInt(sep[1]) == 0 ? false : true;
                        }
                        /*
                         else if (sep[0] == "background") {
                         datas[curLayer].parameter.background = sep[1];
                         GUI_Log("Load backgound: %s\n",
                         datas[curLayer].parameter.background.c_str());
                         }
                         */

                        // pom
                        // MARK: PES3
                        else if (sep[0] == "Tag") {
                            datas[curLayer].parameter.tag = sep[1];
                            ;
                        } else if (sep[0] == "convertToStitch") {
                            datas[curLayer].parameter.convertToStitch =
                                    pesToInt(sep[1]) == 0 ? false : true;
                        } else if (sep[0] == "strokeType") {
                            datas[curLayer].parameter.strokeType =
                                    (pesData::StrokeType)pesToInt(sep[1]);
                        } else if (sep[0] == "strokeTypeIndex") {
                            datas[curLayer].parameter.strokeTypeIndex = pesToInt(sep[1]);
                        } else if (sep[0] == "strokeRunPitch") {
                            datas[curLayer].parameter.strokeRunPitch = pesToFloat(sep[1]);
                        } else if (sep[0] == "strokeWidth") {
                            datas[curLayer].parameter.strokeWidth = pesToFloat(sep[1]);
                        } else if (sep[0] == "strokeDensity") {
                            datas[curLayer].parameter.strokeDensity = pesToFloat(sep[1]);
                        } else if (sep[0] == "strokeRunningInset") {
                            datas[curLayer].parameter.strokeRunningInset = pesToFloat(sep[1]);
                        } else if (sep[0] == "strokeJoinType") {
                            datas[curLayer].parameter.strokeJoinType =
                                    pesData::JoinType(pesToInt(sep[1]));
                        } else if (sep[0] == "strokeJoinTypeIndex") {
                            datas[curLayer].parameter.strokeJoinTypeIndex = pesToInt(sep[1]);
                        } else if (sep[0] == "isCubicSuperPath") {
                            datas[curLayer].parameter.isSatinColumnPath =
                                    pesToInt(sep[1]) == 0 ? false : true;
                        } else if (sep[0] == "isPPEFPath") {
                            datas[curLayer].parameter.isPPEFPath =
                                    pesToInt(sep[1]) == 0 ? false : true;
                        } else if (sep[0] == "isTTFPath") {
                            datas[curLayer].parameter.isTTFPath =
                                    pesToInt(sep[1]) == 0 ? false : true;
                        } else if (sep[0] == "bound.x") {
                            datas[curLayer].parameter.bound.x = pesToFloat(sep[1]);
                        } else if (sep[0] == "bound.y") {
                            datas[curLayer].parameter.bound.y = pesToFloat(sep[1]);
                        } else if (sep[0] == "bound.width") {
                            datas[curLayer].parameter.bound.width = pesToFloat(sep[1]);
                        } else if (sep[0] == "bound.height") {
                            datas[curLayer].parameter.bound.height = pesToFloat(sep[1]);
                        }
                        // motif
                        else if (sep[0] == "motif.size") {
                            datas[curLayer].parameter.motif.size = pesToFloat(sep[1]);
                        } else if (sep[0] == "motif.spacing") {
                            datas[curLayer].parameter.motif.spacing = pesToFloat(sep[1]);
                        } else if (sep[0] == "motif.runPitch") {
                            datas[curLayer].parameter.motif.runPitch = pesToFloat(sep[1]);
                        } else if (sep[0] == "motif.patternIndex") {
                            datas[curLayer].parameter.motif.patternIndex = pesToInt(sep[1]);
                        } else if (sep[0] == "motif.patternName") {
                            datas[curLayer].parameter.motif.patternName = sep[1];
                        }
                        // fill
                        else if (sep[0] == "fillType") {
                            datas[curLayer].parameter.fillType =
                                    (pesData::FillType)pesToInt(sep[1]);
                        } else if (sep[0] == "fillTypeIndex") {
                            datas[curLayer].parameter.fillTypeIndex = pesToInt(sep[1]);
                        } else if (sep[0] == "fillColorIndex") {
                            datas[curLayer].parameter.fillColorIndex = pesToInt(sep[1]);
                        }
                        // preset fill
                        else if (sep[0] == "fill.underlay") {
                            datas[curLayer].parameter.fill.underlay =
                                    pesToInt(sep[1]) == 0 ? false : true;
                        } else if (sep[0] == "fill.density") {
                            datas[curLayer].parameter.fill.density = pesToFloat(sep[1]);
                        } else if (sep[0] == "fill.sewDirection") {
                            datas[curLayer].parameter.fill.sewDirection = pesToInt(sep[1]);
                        } else if (sep[0] == "fill.presetIndex") {
                            datas[curLayer].parameter.fill.presetIndex = pesToInt(sep[1]);
                        }
                        // patternFill
                        else if (sep[0] == "patternFill.underlay") {
                            datas[curLayer].parameter.patternFill.underlay =
                                    pesToInt(sep[1]) == 0 ? false : true;
                        } else if (sep[0] == "patternFill.density") {
                            datas[curLayer].parameter.patternFill.density = pesToFloat(sep[1]);
                        } else if (sep[0] == "patternFill.sewDirection") {
                            datas[curLayer].parameter.patternFill.sewDirection = pesToInt(sep[1]);
                        } else if (sep[0] == "patternFill.patternIndex") {
                            datas[curLayer].parameter.patternFill.patternIndex = pesToInt(sep[1]);
                        } else if (sep[0] == "patternFill.patternName") {
                            datas[curLayer].parameter.patternFill.patternName = sep[1];
                        } else if (sep[0] == "patternFill.patternSize") {
                            datas[curLayer].parameter.patternFill.patternSize = pesToFloat(sep[1]);
                        }
                        // motifFill(PES5.0)
                        else if (sep[0] == "motifFill.size") {
                            datas[curLayer].parameter.motifFill.size = pesToFloat(sep[1]);
                        } else if (sep[0] == "motifFill.spacing") {
                            datas[curLayer].parameter.motifFill.spacing = pesToFloat(sep[1]);
                        } else if (sep[0] == "motifFill.rowSpacing") {
                            datas[curLayer].parameter.motifFill.rowSpacing = pesToFloat(sep[1]);
                        } else if (sep[0] == "motifFill.patternIndex") {
                            datas[curLayer].parameter.motifFill.patternIndex = pesToInt(sep[1]);
                        } else if (sep[0] == "motifFill.patternName") {
                            datas[curLayer].parameter.motifFill.patternName = sep[1];
                        }
                        // autoSatinFill
                        else if (sep[0] == "autoSatinFill.noneOverlap") {
                            datas[curLayer].parameter.autoSatinFill.noneOverlap = pesToInt(sep[1]) == 0 ? false : true;
                        }
                        // pom(PES4.0)
                        else if (sep[0] == "italic") {
                            datas[curLayer].parameter.italic = pesToInt(sep[1]) == 0 ? false : true;
                        } else if (sep[0] == "border") {
                            datas[curLayer].parameter.border = pesToInt(sep[1]) == 0 ? false : true;
                        } else if (sep[0] == "pullCompensate") {
                            datas[curLayer].parameter.pullCompensate = pesToFloat(sep[1]);
                        } else if (sep[0] == "fontWeight") {
                            datas[curLayer].parameter.fontWeight = pesToInt(sep[1]);
                        } else if (sep[0] == "borderColorIndex") {
                            datas[curLayer].parameter.borderColorIndex = pesToInt(sep[1]);
                        } else if (sep[0] == "waveMagnitude") {
                            datas[curLayer].parameter.waveMagnitude = pesToInt(sep[1]);
                        } else if (sep[0] == "slantUpAngle") {
                            datas[curLayer].parameter.slantUpAngle = pesToInt(sep[1]);
                        } else if (sep[0] == "slantDownAngle") {
                            datas[curLayer].parameter.slantDownAngle = pesToInt(sep[1]);
                        } else if (sep[0] == "fadeUpSlant") {
                            datas[curLayer].parameter.fadeUpSlant = pesToInt(sep[1]);
                        } else if (sep[0] == "fadeDownSlant") {
                            datas[curLayer].parameter.fadeDownSlant = pesToInt(sep[1]);
                        } else if (sep[0] == "ppefScaleX") {
                            datas[curLayer].parameter.ppefScaleX = pesToFloat(sep[1]);
                        } else if (sep[0] == "ppefScaleY") {
                            datas[curLayer].parameter.ppefScaleY = pesToFloat(sep[1]);
                        } else if (sep[0] == "lastFontSizeIndex") {
                            datas[curLayer].parameter.lastFontSizeIndex = pesToInt(sep[1]);
                        }
                        // END pom(PES4.0)
                        else if (sep[0] == "borderGap") {
                            datas[curLayer].parameter.borderGap = pesToInt(sep[1]);
                        } else if (sep[0] == "borderGapY") {
                            datas[curLayer].parameter.borderGapY = pesToInt(sep[1]);
                        } else if (sep[0] == "ppefReverseChar") {
                            datas[curLayer].parameter.ppefReverseChar =
                                    pesToInt(sep[1]) == 0 ? false : true;
                        } else if (sep[0] == "ppefReverseSegment") {
                            datas[curLayer].parameter.ppefReverseSegment =
                                    pesToInt(sep[1]) == 0 ? false : true;
                        }
                        // pes5
                        else if (sep[0] == "fontName") {
                            datas[curLayer].parameter.fontName = sep[1];
                        } else if (sep[0] == "fontSize") {
                            datas[curLayer].parameter.fontSize = pesToInt(sep[1]);
                        } else if (sep[0] == "lastFontSize") {
                            datas[curLayer].parameter.lastFontSize = pesToInt(sep[1]);
                        } else if (sep[0] == "useColorFromPicker") {
                            datas[curLayer].parameter.useColorFromPicker =
                                    pesToInt(sep[1]) == 0 ? false : true;
                        } else if (sep[0] == "rotateDegree") {
                            datas[curLayer].parameter.rotateDegree = pesToFloat(sep[1]);
                        }
                        // PES 5.0 (Monogram colors)
                        else if(sep[0]=="colors"){
                            datas[curLayer].parameter.colors.clear();
                            std::vector<std::string> splits = pesSplitString(sep[1], ",");
                            int cnt = (int)splits.size();
                            if(cnt){
                                for(int ic=0; ic<cnt; ic++){
                                    datas[curLayer].parameter.colors.push_back( pesToInt(splits[ic]) );
                                }
                            }
                        }
                        // pom - Fill fading
                        else if(sep[0]=="fillFading"){
                            datas[curLayer].parameter.fillFading = pesToInt(sep[1]) == 0 ? false : true;
                        }
                    }
                }
            }
        }

        if (hasPESData) {
            char* ptr = &buff.getData()[dataOffset];
            while (ptr[0] != '#') {
                ptr++;
                dataOffset++;
            }

            for (int i = 0; i < numLayer; i++) {
                if (datas[i].parameter.type == pesData::ObjectType::OBJECT_TYPE_PES) {
                    int length = datas[i].byteLength;
                    pesBuffer pesbuff;
                    pesbuff.set(ptr, length);
                    //                    auto param = datas[i].parameter;
                    datas[i].loadPESFromBuffer(pesbuff);
                    //                    datas[i].parameter = param;
                    pesbuff.clear();
                    ptr += length;
                }
            }
        } else if (hasPINNData) {
            char* ptr = &buff.getData()[dataOffset];

            for (int i = 0; i < numLayer; i++) {
                //                if (datas[i].parameter.type == pesData::ObjectType::OBJECT_TYPE_PES ||
                //                    datas[i].parameter.type == pesData::OBJECT_TYPE_PES2_TEXT)
                if (datas[i].parameter.type == pesData::ObjectType::OBJECT_TYPE_PES) {
                    while (ptr[0] != '#') {
                        ptr++;
                        dataOffset++;
                    }

                    int length = datas[i].byteLength;
                    if (length > 0) {
                        pesBuffer pinnbuff;
                        pinnbuff.set(ptr, length);

                        //                    auto param = datas[i].parameter;
                        bool isPINNFont = datas[i].parameter.type == pesData::OBJECT_TYPE_PES2_TEXT
                                                  ? true
                                                  : false;
                        datas[i].loadPINNFromBuffer(pinnbuff, false, isPINNFont);
                        //                    datas[i].parameter = param;
                        pinnbuff.clear();
                        ptr += length;
                    }
                } else if (datas[i].parameter.type == pesData::ObjectType::OBJECT_TYPE_BACKGROUND) {
                    while (ptr[0] != '\x89') {
                        ptr++;
                        dataOffset++;
                    }
                    int length = datas[i].byteLength;
                    if (length > 0) {
                        //                    datas[i].parameter.backgroundBuffer = pesBuffer(ptr,
                        //                    length); jsBufferToFile(
                        //                    string("/Users/Jim/testx.png"),
                        //                    datas[i].parameter.backgroundBuffer, true);
                        //                    SDL_Texture *tex = GUI_LoadTextureFromBuffer((unsigned
                        //                    char *)datas[i].parameter.backgroundBuffer.getData(),
                        //                    (int)(datas[i].parameter.backgroundBuffer.size()) );
                        //
                        //                    int w, h;
                        //                    SDL_QueryTexture(tex, NULL, NULL, &w, &h);
                        //                    GUI_Log("background size: %i, %i\n", w, h);
                        //                    datas[i].textureBackground = tex;

                        datas[i].parameter.backgroundBuffer = pesBuffer(ptr, length);
                        sk_sp<SkData> encoded = SkData::MakeWithCopy(ptr, length);
                        datas[i].parameter.textureBackground = SkImage::MakeFromEncoded(encoded);
                        //SkDebugf("image size: %i, %i\n",
                        //         datas[i].parameter.textureBackground->width(),
                        //         datas[i].parameter.textureBackground->height());
                        
                        ptr += length;
                    }
                } else if (datas[i].parameter.type >= pesData::OBJECT_TYPE_SCALABLE_SVG_FILE &&
                           datas[i].parameter.type <= pesData::OBJECT_TYPE_SCALABLE_PPEF_TEXT_V2) {
                    
                    if (datas[i].parameter.isPPEFPath &&
                        datas[i].parameter.type != pesData::OBJECT_TYPE_SCALABLE_PPEF_TEXT &&
                        datas[i].parameter.type != pesData::OBJECT_TYPE_SCALABLE_PPEF_TEXT_V2)
                    {
                        datas[i].parameter.type = pesData::OBJECT_TYPE_SCALABLE_PPEF_TEXT;

                        // backward compatible convert file from pes4 to pes5
                        if (datas[i].parameter.fontName == "") {
                            // pes4: colorIndex start 0
                            // pes5: colorIndex start 1
                            int colorIndex = datas[i].parameter.colorIndex;
                            datas[i].parameter.colorIndex = colorIndex + 1;
                            int fillColorIndex = datas[i].parameter.fillColorIndex;
                            datas[i].parameter.fillColorIndex = fillColorIndex + 1;
                            int borderColorIndex = datas[i].parameter.borderColorIndex;
                            datas[i].parameter.borderColorIndex = borderColorIndex + 1;
                            
                            // pes4: use fontIndex
                            // pes5: use fontName
                            int fontIndex = datas[i].parameter.fontIndex;
                            if (fontIndex > -1 && fontIndex < 29) {
                                if (fontIndex < 10) {
                                    fontIndex = fontIndex + 1;
                                } 
                                else if (fontIndex == 10) {
                                    fontIndex = 999;
                                }

                                char buff[9];
                                buff[0] = 0;
                                buff[8] = 0;
                                sprintf(buff, "Thai%03d", fontIndex);
                                datas[i].parameter.fontName = std::string(buff);
                            }

                            // pes4: use fontSizeIndex and lastFontSizeIndex
                            // pes5: use fontSize
                            // try fontSizeIndex first, if not then lastFontSizeIndex
                            int fontSizeIndex = datas[i].parameter.fontSizeIndex;
                            if (!(fontSizeIndex > -1 && fontSizeIndex < MaxFontSizes)) {
                                int lastFontSizeIndex = datas[i].parameter.lastFontSizeIndex;
                                if (lastFontSizeIndex > -1 && lastFontSizeIndex < MaxFontSizes) {
                                    fontSizeIndex = lastFontSizeIndex;
                                }
                            }
                            if (fontSizeIndex > -1 && fontSizeIndex < MaxFontSizes) {
                                datas[i].parameter.lastFontSizeIndex = fontSizeIndex;
                                int fontSize = fontSizes[fontSizeIndex];
                                datas[i].parameter.fontSize = fontSize;
                                datas[i].parameter.lastFontSize = fontSize;
                            }

                            // motif not use with ppef, 
                            // mark flags for calculate and store fixed-font-size in future 
                            datas[i].parameter.motif.size = 0;
                        }
                    }
                    else if (datas[i].parameter.isTTFPath &&
                               datas[i].parameter.type != pesData::OBJECT_TYPE_SCALABLE_TTF_TEXT)
                    {
                        datas[i].parameter.type = pesData::OBJECT_TYPE_SCALABLE_TTF_TEXT;
                    }
                    else if (datas[i].parameter.isSatinColumnPath &&
                               datas[i].parameter.type !=
                                       pesData::OBJECT_TYPE_SCALABLE_SATINCOLUMN)
                    {
                        datas[i].parameter.type = pesData::OBJECT_TYPE_SCALABLE_SATINCOLUMN;
                    }

                    while (ptr[0] != '<') {
                        ptr++;
                        dataOffset++;
                    }
                    int length = datas[i].byteLength;

                    pesBuffer svgBuff = pesBuffer(ptr, length);

                    auto bk = datas[i].parameter;
                    if (datas[i].parameter.isPPEFPath) {
                        datas[i].loadPPEFFromSVGBuffer(svgBuff);
                        datas[i].parameter.type = bk.type;
                        datas[i].parameter.fillType = bk.fillType;
                        datas[i].parameter.strokeType = bk.strokeType;
                    } else if (datas[i].parameter.isTTFPath) {
                        datas[i].loadTTFFromSVGBuffer(svgBuff);
                        datas[i].parameter.fillType = bk.fillType;
                        datas[i].parameter.strokeType = bk.strokeType;
                    } else if (datas[i].parameter.isSatinColumnPath) {
                        datas[i].loadSatinColumnFromSVGBuffer(svgBuff);
                    } else {
                        // Load SVG
                        datas[i].loadSVGFromBuffer(svgBuff, false);
                        //SkDebugf( "SVG FillType: %i\n", datas[i].parameter.fillType );
                        //SkDebugf( "SVG StrokeType: %i\n", datas[i].parameter.strokeType );
                        //SkDebugf( "SVG useColorFromPicker: %i\n", datas[i].parameter.useColorFromPicker );
                        datas[i].parameter.fillType = bk.fillType;
                        datas[i].parameter.strokeType = bk.strokeType;
                        datas[i].parameter.useColorFromPicker = bk.useColorFromPicker;
                        //SkDebugf( "PARAM FillType: %i\n", datas[i].parameter.fillType );
                        //SkDebugf( "PARAM FillAngle: %i\n", datas[i].parameter.fill.sewDirection);
                        //SkDebugf( "PARAM StrokeType: %i\n", datas[i].parameter.strokeType );
                        //SkDebugf( "PARAM useColorFromPicker: %i\n", datas[i].parameter.useColorFromPicker );
                        //SkDebugf( "Num Paths: %i\n", (int)datas[i].paths.size() );
                        if( PPES_FileVersion <= 501 ) {
                            for( int p=0; p<datas[i].paths.size(); p++ ) {
                                datas[i].paths[p].fillAngle = datas[i].parameter.fill.sewDirection;
                                datas[i].paths[p].fillDensity = datas[i].parameter.fill.density;
                                datas[i].paths[p].fillPullCompensate = datas[i].parameter.pullCompensate;

                                datas[i].paths[p].strokeDensity = datas[i].parameter.strokeDensity;
                                datas[i].paths[p].strokePullCompensate = datas[i].parameter.pullCompensate;

                                if( datas[i].paths[p].fillAngle == 0 ) {
                                    datas[i].paths[p].fillAngle = -1;
                                }
                            }
                        }
                    }

                    if (datas[i].parameter.bound.isEmpty() == false) {
                        if( datas[i].parameter.type != pesData::OBJECT_TYPE_SCALABLE_TTF_TEXT &&   // NooM  - no scale for TTF
                            datas[i].parameter.type != pesData::OBJECT_TYPE_SCALABLE_SVG_FILE  ) { // Jimmy - no scale for SVG
                            pesRectangle src = datas[i].getScalableBoundingBox();
                            float sx = datas[i].parameter.bound.width / src.width;
                            float sy = datas[i].parameter.bound.height / src.height;
                            if (sx != 1.0 || sy != 1.0) {
                                for (auto& path : datas[i].paths) {
                                    path.scale(sx, sy);
                                }
                                src = datas[i].getScalableBoundingBox();
                            }
                            pesVec2f dxy = datas[i].parameter.bound.getCenter() - src.getCenter();
                            if (dxy.length()) {
                                datas[i].translate(dxy.x, dxy.y);
                            }

                        }

                        //                        datas[i].recalculate();
                        //                        src = datas[i].getScalableBoundingBox();
                    }

                    if (datas[i].parameter.fillType != pesData::FillType::FILL_TYPE_NONE &&
                        datas[i].parameter.fillType != pesData::FillType::FILL_TYPE_COLOR) {
                        if (datas[i].parameter.isPPEFPath)
                        {
                            if(datas[i].parameter.type == pesData::OBJECT_TYPE_SCALABLE_PPEF_TEXT){
                                datas[i].applyPPEFFill();
                                if (datas[i].fillBlocks.size() > 0) {
//                                    datas[i].fillBlocks[0].setColorFromIndex(datas[i].parameter.colorIndex + 1);
                                    datas[i].fillBlocks[0].setColorFromIndex(datas[i].parameter.fillColorIndex);
                                    if (datas[i].parameter.border && datas[i].fillBlocks.size() == 2) {
//                                        datas[i].fillBlocks[1].setColorFromIndex(datas[i].parameter.borderColorIndex + 1);
                                        datas[i].fillBlocks[1].setColorFromIndex(datas[i].parameter.borderColorIndex);
                                    }
                                }
                            }
                            else if(datas[i].parameter.type == pesData::OBJECT_TYPE_SCALABLE_PPEF_TEXT_V2){
                                datas[i].applyPPEF_V2_Fill();
                            }
                        }
                        else
                        {
                            if (datas[i].parameter.fillType == pesData::FILL_TYPE_PATTERN) {
                                datas[i].parameter.patternFill.patternBuffer = embeded_pattern::getBuffer(datas[i].parameter.patternFill.patternName);
                            }
                            else if (datas[i].parameter.fillType == pesData::FILL_TYPE_MOTIF) {
                                datas[i].parameter.motifFill.patternBuffer = embeded_motif::getBuffer(datas[i].parameter.motifFill.patternName);
                            }
                            datas[i].applyFill();
                        }
                    }
                    if (datas[i].parameter.strokeType != pesData::StrokeType::STROKE_TYPE_NONE) {
                        if (datas[i].parameter.strokeType == pesData::STROKE_TYPE_MOTIF) {
                            datas[i].parameter.motif.patternBuffer = embeded_motif::getBuffer(datas[i].parameter.motif.patternName);
                        }
                        datas[i].applyStroke();
                    }
                    svgBuff.clear();

                    ptr += length;
                }
            }
        }

        for (int i = 0; i < numLayer; i++) {
            if (datas[i].parameter.type == pesData::ObjectType::OBJECT_TYPE_SHAPE) {
                reCreatePES2_Shape(datas[i]);
            }
        }
    } else {
        return success;
    }

    //    for( int i=0; i<datas.size(); i++ ) {
    //        if( datas[i].parameter.isPPEFPath )
    //            recalcPPFPath( datas[i] );
    //    }

    if (datas.size()) {
        __pesDataList.clear();
        for (auto& data : datas) {
            // __pesDataList.push_back(make_shared<pesData>(data));
            addObject(data);
        }
        datas.clear();
        success = true;
    }
    // keep group ids monotonic so freshly created groups never collide
    for (const auto& g : __pesGroups)
        if (g.id >= __nextGroupId) __nextGroupId = g.id + 1;
    updateInfo();
    return success;
}


bool pesDocument::loadJSONFromBuffer(const pesBuffer & jsonBuff, const pesBuffer & jbinBuff){
    vector<pesData> datas;
    int numLayer = 0;
    int curLayer = -1;
    int dataOffset = 0;
    bool hasLayerTypePES = false;
    bool hasPESData = false;
    bool hasPINNData = false;
    bool success = false;
    
//    pesBackgroundWidth = 0;
//    pesBackgroundHeight = 0;
    pesBackgroundWidth = 100; // default 100 mm.(10cm.)
    pesBackgroundHeight = 100; // default 100 mm.(10cm.)
    
    
    pesBuffer buff = const_cast<pesBuffer &>(jsonBuff);
    pesBuffer ppes;
    if(buff.size()){
        json jppes = json::parse(buff.getText());
        pesBackgroundWidth = jppes["BackgroundWidth"];
        pesBackgroundHeight = jppes["BackgroundHeight"];
        numLayer = jppes["numLayer"];
        json jlayers = jppes["layers"];
        for(auto & jlayer : jlayers){
            // read a layer
            pesData data;
            int LayerId = jlayer["LayerId"];
            data.parameter.type = jlayer["type"];
            data.parameter.text = jlayer["text"];
            data.parameter.fontIndex = jlayer["fontIndex"];
            data.parameter.fontSizeIndex = jlayer["fontSizeIndex"];
            data.parameter.colorIndex = jlayer["colorIndex"];
            data.parameter.baselineIndex = jlayer["baselineIndex"];
            data.parameter.radiusValue = jlayer["radiusValue"];
            data.parameter.angleValue = jlayer["angleValue"];
            // v 1.00
            data.parameter.extraLetterSpace = jlayer["extraLetterSpace"];
            data.parameter.extraSpace = jlayer["extraSpace"];
            // v 1.90
            data.parameter.locked = jlayer["locked"];
            data.parameter.visible = jlayer["visible"];
            data.centerx = jlayer["centerx"];
            data.centery = jlayer["centery"];
            
            if(data.parameter.type==pesData::ObjectType::OBJECT_TYPE_PES){
                data.parameter.rotateValue = jlayer["rotateValue"];
                data.numColorChange = jlayer["numColorChange"];
                data.byteLength = jlayer["byteLength"];
            }
            if(data.parameter.type == pesData::ObjectType::OBJECT_TYPE_BACKGROUND){
                data.byteLength = jlayer["byteLength"];
            }
            if(data.isScalable()){
                data.parameter.convertToStitch = jlayer["convertToStitch"];
                data.parameter.strokeType = jlayer["strokeType"];
                data.parameter.strokeTypeIndex = jlayer["strokeTypeIndex"];
                data.parameter.strokeRunPitch = jlayer["strokeRunPitch"];
                data.parameter.strokeWidth = jlayer["strokeWidth"];
                data.parameter.strokeDensity = jlayer["strokeDensity"];
                data.parameter.strokeRunningInset = jlayer["strokeRunningInset"];
                data.parameter.strokeJoinType = jlayer["strokeJoinType"];
                data.parameter.strokeJoinTypeIndex = jlayer["strokeJoinTypeIndex"];
                data.parameter.isSatinColumnPath = jlayer["isCubicSuperPath"];
                data.parameter.isPPEFPath = jlayer["isPPEFPath"];
                data.parameter.isTTFPath = jlayer["isTTFPath"];
                
                json jmotif = jlayer["motif"];
                json jfill = jlayer["fill"];
                json jpreset = jlayer["preset"];
                json jpatternFill = jlayer["patternFill"];
                // motif
                data.parameter.motif.size = jmotif["size"];
                data.parameter.motif.spacing = jmotif["spacing"];
                data.parameter.motif.runPitch = jmotif["runPitch"];
                data.parameter.motif.patternIndex = jmotif["patternIndex"];
                data.parameter.motif.patternName = jmotif["patternName"];
                // fill
                data.parameter.fillType = jfill["fillType"];
                data.parameter.fillTypeIndex = jfill["fillTypeIndex"];
                data.parameter.fillColorIndex = jfill["fillColorIndex"];
                // preset fill
                data.parameter.fill.underlay = jpreset["underlay"];
                data.parameter.fill.density = jpreset["density"];
                data.parameter.fill.sewDirection = jpreset["sewDirection"];
                data.parameter.fill.presetIndex = jpreset["presetIndex"];
                // pattern fill
                data.parameter.patternFill.underlay = jpatternFill["underlay"];
                data.parameter.patternFill.density = jpatternFill["density"];
                data.parameter.patternFill.sewDirection = jpatternFill["sewDirection"];
                data.parameter.patternFill.patternIndex = jpatternFill["patternIndex"];
                data.parameter.patternFill.patternName = jpatternFill["patternName"];
                data.parameter.patternFill.patternSize = jpatternFill["patternSize"];
                
                data.byteLength = jlayer["byteLength"];
                
                // pes4.0
                data.parameter.italic = jlayer["italic"];
                data.parameter.border = jlayer["border"];
                data.parameter.pullCompensate = jlayer["pullCompensate"];
                data.parameter.fontWeight = jlayer["fontWeight"];
                data.parameter.shapeIndex = jlayer["shapeIndex"];
                data.parameter.borderColorIndex = jlayer["borderColorIndex"];
                data.parameter.waveMagnitude = jlayer["waveMagnitude"];
                data.parameter.slantUpAngle = jlayer["slantUpAngle"];
                data.parameter.slantDownAngle = jlayer["slantDownAngle"];
                data.parameter.fadeUpSlant = jlayer["fadeUpSlant"];
                data.parameter.fadeDownSlant = jlayer["fadeDownSlant"];
                data.parameter.ppefScaleX = jlayer["ppefScaleX"];
                data.parameter.ppefScaleY = jlayer["ppefScaleY"];
                data.parameter.lastFontSizeIndex = jlayer["lastFontSizeIndex"];
                data.parameter.ppefReverseChar = jlayer["ppefReverseChar"];
                data.parameter.ppefReverseSegment = jlayer["ppefReverseSegment"];
                data.parameter.borderGap = jlayer["borderGap"];
                data.parameter.borderGapY = jlayer["borderGapY"];
            
                data.parameter.bound.x = jlayer["bound.x"];
                data.parameter.bound.y = jlayer["bound.y"];
                data.parameter.bound.width = jlayer["bound.width"];
                data.parameter.bound.height = jlayer["bound.height"];
            }
            if(data.parameter.type==pesData::ObjectType::OBJECT_TYPE_SHAPE){
                data.parameter.rotateValue = jlayer["rotateValue"];
                data.parameter.shapeIndex = jlayer["shapeIndex"];
                data.parameter.width = jlayer["width"];
                data.parameter.height = jlayer["height"];
                data.parameter.lineWidth = jlayer["lineWidth"];
                data.parameter.density = jlayer["density"];
            }
            else if(data.parameter.type==pesData::OBJECT_TYPE_PES2_TEXT){
                data.parameter.rotateValue = jlayer["rotateValue"];
                data.byteLength = jlayer["byteLength"];
            }
            data.parameter.tag = jlayer["Tag"];
            datas.push_back(data);
        }
    }
    
    if(jbinBuff.size()){
        hasPESData = true;
        hasPINNData = true;
        dataOffset = 0;
        const char* ptr = &jbinBuff.getData()[dataOffset];
        for(int i=0; i<numLayer; i++){
            if(datas[i].parameter.type == pesData::ObjectType::OBJECT_TYPE_PES || datas[i].parameter.type == pesData::OBJECT_TYPE_PES2_TEXT){
                while (ptr[0] != '#') {
                    ptr++;
                    dataOffset++;
                }
                int length = datas[i].byteLength;
                pesBuffer pinnbuff;
                pinnbuff.set(ptr, length);
                bool isPINNFont = datas[i].parameter.type == pesData::OBJECT_TYPE_PES2_TEXT ? true : false;
                datas[i].loadPINNFromBuffer(pinnbuff, false, isPINNFont);
                pinnbuff.clear();
                ptr += length;
            }
//            else if(datas[i].parameter.type == pesData::ObjectType::OBJECT_TYPE_BACKGROUND ){
//                while (ptr[0] != '\x89') {
//                    ptr++;
//                    dataOffset++;
//                }
//                int length = datas[i].byteLength;
//                datas[i].parameter.backgroundBuffer = pesBuffer(ptr, length);
//                // load image here
//                ptr+=length;
//            }
            else if( datas[i].parameter.type >= pesData::OBJECT_TYPE_SCALABLE_SVG_FILE && datas[i].parameter.type<=pesData::OBJECT_TYPE_SCALABLE_SATINCOLUMN){
                if(datas[i].parameter.isPPEFPath && datas[i].parameter.type!=pesData::OBJECT_TYPE_SCALABLE_PPEF_TEXT){
                    datas[i].parameter.type = pesData::OBJECT_TYPE_SCALABLE_PPEF_TEXT;
                }
                else if(datas[i].parameter.isTTFPath && datas[i].parameter.type!=pesData::OBJECT_TYPE_SCALABLE_TTF_TEXT){
                    datas[i].parameter.type = pesData::OBJECT_TYPE_SCALABLE_TTF_TEXT;
                }
                else if(datas[i].parameter.isSatinColumnPath && datas[i].parameter.type!=pesData::OBJECT_TYPE_SCALABLE_SATINCOLUMN){
                    datas[i].parameter.type = pesData::OBJECT_TYPE_SCALABLE_SATINCOLUMN;
                }
                while (ptr[0] != '<') {
                    ptr++;
                    dataOffset++;
                }
                int length = datas[i].byteLength;
                pesBuffer svgBuff = pesBuffer(ptr, length);
                if(datas[i].parameter.isPPEFPath){
                    datas[i].loadPPEFFromSVGBuffer(svgBuff);
                }
                else if(datas[i].parameter.isTTFPath){
                    datas[i].loadTTFFromSVGBuffer(svgBuff);
                }
                else if(datas[i].parameter.isSatinColumnPath){
                    datas[i].loadSatinColumnFromSVGBuffer(svgBuff);
                }
                else{
                    datas[i].loadSVGFromBuffer(svgBuff, false);
                }
                
                if(datas[i].parameter.bound.isEmpty()==false){
                    pesRectangle src = datas[i].getScalableBoundingBox();
                    float sx = datas[i].parameter.bound.width / src.width;
                    float sy = datas[i].parameter.bound.height / src.height;
                    if(sx!=1.0 || sy!=1.0){
                        for(auto & path:datas[i].paths){
                            path.scale(sx, sy);
                        }
                        src = datas[i].getScalableBoundingBox();
                    }
                    pesVec2f dxy = datas[i].parameter.bound.getCenter() - src.getCenter();
                    if(dxy.length()){
                        datas[i].translate(dxy.x, dxy.y);
                    }
                }
                
                if(datas[i].parameter.fillType!=pesData::FillType::FILL_TYPE_NONE && datas[i].parameter.fillType!=pesData::FillType::FILL_TYPE_COLOR ){
                    if(datas[i].parameter.isPPEFPath){
                        datas[i].applyPPEFFill();
                        if( datas[i].fillBlocks.size() > 0 ) {
                            datas[i].fillBlocks[0].setColorFromIndex(datas[i].parameter.colorIndex+1);
                            if(datas[i].parameter.border && datas[i].fillBlocks.size()==2){
                                datas[i].fillBlocks[1].setColorFromIndex(datas[i].parameter.borderColorIndex+1);
                            }
                        }
                    }
                    else{
                        datas[i].applyFill();
                    }
                }
                if(datas[i].parameter.strokeType!=pesData::StrokeType::STROKE_TYPE_NONE){
                    datas[i].applyStroke();
                }
                svgBuff.clear();
                ptr+=length;
            }
        }// end for(numLayer)
        
        for(int i=0; i<numLayer; i++){
            if (datas[i].parameter.type == pesData::ObjectType::OBJECT_TYPE_SHAPE) {
                reCreatePES2_Shape(datas[i]);
            }
        }
    }
    else{
        return success;
    }
    
    if(datas.size()){
        pesGetDocument()->newDocument();
        for(auto & data:datas){
            pesGetDocument()->addObject(data);
        }
        datas.clear();
        success = true;
    }
    
    return success;
}



// void reCreatePES2_Text(pesData & data){
//    pesVec2f prevCenter = data.getBoundingBox().getCenter();
// TODO: need PES2 font loader

//    std::filesystem::path basepath = "PINN_FONTS"; // Oops we cannot use filesystem in libPES
// pom - resolved by store stitchData in ppes file and load it back in the next time.
//}

#define SHAPE_LINE 0
#define SHAPE_TRIANGLE 1
#define SHAPE_RECT 2
#define SHAPE_SATIN_LINE 4
#define SHAPE_SHARP_TRIANGLE 5
#define SHAPE_SHARP_RECT 6
#define SHAPE_CIRCLE 8
#define SHAPE_ECLIPSE 8

void reCreatePES2_Shape(pesData& data) {
    float lineWidth = data.parameter.lineWidth * 100;
    float shapeWidth = data.parameter.width * 100;
    float shapeHeight = data.parameter.height * 100;

    pesVec2f prevCenter = data.getBoundingBox().getCenter();
    pesPath path;
    int shapeIndex = data.parameter.shapeIndex;
    if (shapeIndex == SHAPE_LINE || shapeIndex == SHAPE_SATIN_LINE) {
        path.moveTo(-shapeWidth * 0.5, 0);
        path.lineTo(shapeWidth * 0.5, 0);
    } else if (shapeIndex == SHAPE_RECT || shapeIndex == SHAPE_SHARP_RECT) {
        path.rectangle(-shapeWidth / 2, -shapeHeight / 2, shapeWidth, shapeHeight);
    } else if (shapeIndex == SHAPE_ECLIPSE) {
        path.arc(pesVec2f(0, 0), shapeWidth * 0.5, shapeHeight * 0.5, 0, 360);
        path.close();
    } else if (shapeIndex == SHAPE_TRIANGLE) {
        pesVec2f p1(0, -shapeHeight / 2);
        pesVec2f p2(shapeWidth / 2, shapeHeight / 2);
        pesVec2f p3(-shapeWidth / 2, shapeHeight / 2);
        path.moveTo(p2.x, p2.y);
        path.moveTo(p1.x, p1.y);
        path.moveTo(p3.x, p3.y);
        path.close();
    }

    pesSatinOutline result;
    float zigzagWidth = lineWidth;
    float zigzagSpacing = 10 / 2.5;
    pesPolyline outline = path.getOutline().front();
    pesSatinOutlineGenerate(outline, zigzagWidth, 0, result);
    vector<pesVec2f> satin = pesSatinOutlineRenderStitches(result, zigzagSpacing);

    pesStitchBlock block;
    block.setColorFromIndex(data.parameter.colorIndex);
    for (auto& s : satin) {
        if (block.size() <= 0)
            block.addJump(s);
        else
            block.addStitch(s);
    }
    data.fillBlocks.clear();
    data.fillBlocks.push_back(block);
    data.recalculate();

    // base line
    if (data.parameter.baselineIndex != 0) {
        float minx = data.getBoundingBox().getMinX();
        float maxx = data.getBoundingBox().getMinX();

        float value = data.parameter.angleValue;
        float rad = pesMap(value, 0.0f, 1.0f, 0.0f, (float)PI);
        float minrad = -rad;
        float maxrad = rad;

        int multiply = 3;
        float radius = pesMap(data.parameter.radiusValue, 0.0f, 1.0f, 0.0f, 500.0f * multiply);

        // MARK: Adjust radius
        //        if (oldParam.baselineIndex == 0) {
        //            int l = pesdataList[index].width;
        //            float r = (float)(l * 3 / TWO_PI);
        //            pesdataList[index].parameter.radiusValue = guiMap(r, 0.0, 500 * 3, 0.0, 1.0);
        //            radius = guiMap(pesdataList[index].parameter.radiusValue, 0.0f, 1.0f, 0.0f,
        //            500.0f*multiply);
        //
        //            pesdataList[index].parameter.angleValue = guiMap(60, 0, 180, 0, 1);
        //            rad = guiMap(pesdataList[index].parameter.angleValue, 0.0f, 1.0f, 0.0f,
        //            (float)PI); minrad = -rad; maxrad = rad;
        //        }

        if (data.parameter.baselineIndex == 2)  // Circle-Bottom
        {
            radius += data.getBoundingBox().height;
            radius *= -1;
            minrad *= -1;
            maxrad *= -1;
        }

        for (int i = 0; i < (int)data.fillBlocks.size(); i++) {
            pesStitchBlock& block = data.fillBlocks[i];
            for (int j = 0; j < (int)block.size(); j++) {
                pesVec2f stitch = block.polyline[j];
                float rad = pesMap(stitch.x, minx, maxx, minrad, maxrad);
                pesVec2f s(0.0f, (stitch.y - data.getBoundingBox().height * 0.5f) - radius);
                s.rotateRad(rad);
                block.polyline[j].set(s.x, s.y);
            }
        }
        data.calcBoundingBox();
    }

    if (data.parameter.rotateValue != 0.5f) {
        float degree = pesMap(data.parameter.rotateValue, 0.0f, 1.0f, -180, 180);
        data.rotate(degree);
    }

    pesVec2f d = prevCenter - data.getBoundingBox().getCenter();
    if (d.length()) {
        data.translate(d.x, d.y);
    }
}

void pesPECEncodeJump(pesBuffer& buff, int x, int types) {
    int outputVal = abs(x) & 0x7FF;
    unsigned int orPart = 0x80;

    if (types & TRIM_STITCH) {
        orPart |= 0x20;  // orPart=0xA0
    }
    if (types & JUMP_STITCH) {
        orPart |= 0x10;  // orPart=0x90
    }
    // otherwise orPart=0x80 (fake jump stitch)

    if (x < 0) {
        outputVal = x + 0x1000 & 0x7FF;
        outputVal |= 0x800;
    }

    unsigned char jump[2] = {0};
    jump[0] = (unsigned char)(((outputVal >> 8) & 0x0F) | orPart);
    jump[1] = (unsigned char)(outputVal & 0xFF);

    buff.append((const char*)jump, 2);
}

static void pesPECEncodeStop(pesBuffer& buff, unsigned char val) {
    unsigned char stop[3] = {0xFE, 0xB0, val};
    buff.append((const char*)stop, 3);
}

pesBuffer CreatePECStitch(const pesData& pPesData) {
    // build relative stich&encode
    double thisX = 0.0;
    double thisY = 0.0;
    unsigned char stopCode = 2;

    pesData data = const_cast<pesData&>(pPesData);
    pesStitchBlockList blocks;
    data.getStitchBlockList(blocks);

    pesBuffer stitches;
    for (int i = 0; i < (int)blocks.size(); i++) {
        pesStitchBlock& block = blocks[i];
        if (i > 0) {
            // write change color(start new block) 0xFE 0xE0 0xNN<-use stopCode
            pesPECEncodeStop(stitches, stopCode);
            if (stopCode == (unsigned char)2) {
                stopCode = (unsigned char)1;
            } else {
                stopCode = (unsigned char)2;
            }
        }
        for (int j = 0; j < (int)block.polyline.size(); j++) {
            pesVec2f stitch = block.polyline[j];
            int type = block.types[j];

            int deltaX = (int)round(stitch.x - thisX);
            int deltaY = (int)round(stitch.y - thisY);
            thisX += (double)deltaX;
            thisY += (double)deltaY;

            // when 2 or more pes are merged into may we can found end flag before last stitches
            // so we just skip this stitch
            if (type == STOP_STITCH || type == END_STITCH) {
                continue;
            } else if (deltaX < 63 && deltaX > -64 && deltaY < 63 && deltaY > -64 &&
                       (!(type & (JUMP_STITCH | TRIM_STITCH)))) {
                // normal stitch x y pair in to 2 bytes
                unsigned char delta[2];
                delta[0] = (deltaX < 0) ? (unsigned char)(deltaX + 0x80) : (unsigned char)deltaX;
                delta[1] = (deltaY < 0) ? (unsigned char)(deltaY + 0x80) : (unsigned char)deltaY;
                stitches.append((const char*)delta, 2);
            } else {
                // this can be jump, trim or fake jump (hi length stitch)
                // MARK: Incase deltaX/deltaY >2047 or <-2048 make double jump or multiple jump
                if (abs(deltaX) > 2047 || abs(deltaY) > 2048) {
                    //                    double _max = (double)MAX((float)abs(deltaX),
                    //                    (float)abs(deltaX)); // <- why my program work? find max
                    //                    with same two variable for what?
                    double _max = (double)MAX((float)abs(deltaX), (float)abs(deltaY));
                    int nround = (int)ceil((_max + 1) / 2048);
                    for (int i = 0; i < nround; i++) {
                        float stepX = (deltaX / (float)nround);
                        float stepY = (deltaY / (float)nround);
                        pesPECEncodeJump(stitches, (int)round(stepX), type);
                        pesPECEncodeJump(stitches, (int)round(stepY), type);
                    }
                } else {
                    pesPECEncodeJump(stitches, deltaX, type);
                    pesPECEncodeJump(stitches, deltaY, type);
                }
            }
        }
    }
    // end of stitch data
    //    const unsigned char endofstitch[1] = {0xFF};
    //    stitches.append((const char*)endofstitch, 1);

    const unsigned char endofstitch[2] = {0xFF, 0x00};
    stitches.append((const char*)endofstitch, 2);

    return stitches;
}

pesBuffer CreatePINNBuffer(const pesData& pPesData) {
    pesBuffer pinnBuff;
    char numColor = (char)pPesData.colors.size();
    char numColorChange = numColor - 1;
    pinnBuff.append("#PINN");
    pinnBuff.appendU8(numColorChange);
    auto colors = pPesData.colors;
    for (int i = 0; i < numColor; i++) {
        char c = colors[i];
        pinnBuff.appendU8(c);
    }

    pesBuffer pecStitches = CreatePECStitch(pPesData);
    pinnBuff.append(pecStitches.getData(), pecStitches.size());
    pecStitches.clear();

    return pinnBuff;
}

const string PES_LIB_VERSION = "504"; // 504: layer groups (numGroups/Group.*/groupId)
pesBuffer pesDocument::getPPESBuffer() const {
    pesBuffer buff;

    if (__pesDataList.size()) {
        std::string szBuffer;
        szBuffer += "#PPES\n";
//        if (strAppNameMax32[0] == '\0') {
//            szBuffer += "Appname=PESLib\n";
//        } else {
//            szBuffer += pesVAArgsToString("Appname=%s\n", std::string(strAppNameMax32).c_str());
//        }
        if (strAppName == "") {
            szBuffer += "Appname=PESLib\n";
        } else {
            szBuffer += pesVAArgsToString("Appname=%s\n", strAppName.c_str());
        }
        szBuffer += "Codename=PESEngine\n";
        szBuffer += pesVAArgsToString("Version=%s\n", PES_LIB_VERSION.c_str());
        szBuffer += pesVAArgsToString("numLayer=%i\n", __pesDataList.size());

//        szBuffer += pesVAArgsToString("BackgroundWidth=%i\n", __pesHoopSize.x);
//        szBuffer += pesVAArgsToString("BackgroundHeight=%i\n\n", __pesHoopSize.y);
        szBuffer += pesVAArgsToString("BackgroundWidth=%i\n", pesBackgroundWidth);
        szBuffer += pesVAArgsToString("BackgroundHeight=%i\n\n", pesBackgroundHeight);

        // Layer groups (PPES v504+). Written in the header — far from the binary
        // [PINNDATA] region, whose loader scans forward for a magic byte — so a
        // '#' in a group name can never be mistaken for stitch data. Old readers
        // ignore these unknown keys (loads as an ungrouped flat list).
        szBuffer += pesVAArgsToString("numGroups=%i\n", (int)__pesGroups.size());
        for (const auto& g : __pesGroups) {
            szBuffer += pesVAArgsToString("Group.id=%i\n", g.id);
            szBuffer += pesVAArgsToString("Group.parentId=%i\n", g.parentId);
            szBuffer += pesVAArgsToString("Group.order=%i\n", g.order);
            szBuffer += pesVAArgsToString("Group.collapsed=%i\n", g.collapsed ? 1 : 0);
            szBuffer += pesVAArgsToString("Group.name=%s\n", g.name.c_str());
        }
        szBuffer += "\n";

        vector<pesBuffer> buffers;
        for (int i = 0; i < getObjectCount(); i++) {
            if (__pesDataList[i]->parameter.type == pesData::OBJECT_TYPE_PES2_TEXT) {
                pesBuffer buff = CreatePINNBuffer(*__pesDataList[i]);
                buffers.push_back(buff);
                buff.clear();
            }
            if (__pesDataList[i]->parameter.type == pesData::ObjectType::OBJECT_TYPE_PES) {
                pesBuffer buff = CreatePINNBuffer(*__pesDataList[i]);
                buffers.push_back(buff);
                buff.clear();
            }
            else if (__pesDataList[i]->parameter.type == pesData::ObjectType::OBJECT_TYPE_BACKGROUND){
                pesBuffer buff = __pesDataList[i]->parameter.backgroundBuffer;
                buffers.push_back(buff);
                buff.clear();
            }
            else if (__pesDataList[i]->isScalable()) {
                buffers.push_back(__pesDataList[i]->getSVGBuffer());
            }
        }

        int bufferIdx = 0;
        for (int i = 0; i < getObjectCount(); i++) {
            pesData::Parameter param = __pesDataList[i]->parameter;
            if (param.type == pesData::ObjectType::OBJECT_TYPE_PES) {
                param.baselineIndex = 0;
            }

            szBuffer += pesVAArgsToString("LayerId=%i\n", i);
            szBuffer += pesVAArgsToString("type=%i\n", param.type);
            szBuffer += pesVAArgsToString("text=%s\n", param.text.c_str());
            szBuffer += pesVAArgsToString("fontIndex=%i\n", param.fontIndex);
            szBuffer += pesVAArgsToString("fontSizeIndex=%i\n", param.fontSizeIndex);
            szBuffer += pesVAArgsToString("colorIndex=%i\n", param.colorIndex);
            szBuffer += pesVAArgsToString("baselineIndex=%i\n", param.baselineIndex);
            szBuffer += pesVAArgsToString("radiusValue=%.2f\n", param.radiusValue);
            szBuffer += pesVAArgsToString("angleValue=%.2f\n", param.angleValue);

            // v 1.00
            szBuffer += pesVAArgsToString("extraLetterSpace=%i\n", param.extraLetterSpace);
            szBuffer += pesVAArgsToString("extraSpace=%i\n", param.extraSpace);
            // end v 1.00

            // v 1.90
            szBuffer += pesVAArgsToString("locked=%i\n", param.locked);
            szBuffer += pesVAArgsToString("visible=%i\n", param.visible);
            /*
             if (param.background.length() > 0)
             szBuffer += pesVAArgsToString("background=%s\n", param.background.c_str());
             */
            // end v 1.90

            szBuffer += pesVAArgsToString("centerx=%i\n", __pesDataList[i]->centerx);
            szBuffer += pesVAArgsToString("centery=%i\n", __pesDataList[i]->centery);
            if (param.type == pesData::ObjectType::OBJECT_TYPE_PES) {
                szBuffer += pesVAArgsToString("rotateValue=%.2f\n", 0.5);
                szBuffer +=
                        pesVAArgsToString("numColorChange=%i\n", __pesDataList[i]->numColorChange);
                szBuffer += pesVAArgsToString("byteLength=%i\n", buffers[bufferIdx++].size());
            }
            if (param.type == pesData::ObjectType::OBJECT_TYPE_BACKGROUND) {
                szBuffer += pesVAArgsToString("byteLength=%i\n", buffers[bufferIdx++].size());
            }
            // pom
            if (__pesDataList[i]->isScalable()) {
                szBuffer += pesVAArgsToString("convertToStitch=%i\n", param.convertToStitch);
                szBuffer += pesVAArgsToString("strokeType=%i\n", (int)param.strokeType);
                szBuffer += pesVAArgsToString("strokeTypeIndex=%i\n", param.strokeTypeIndex);
                szBuffer += pesVAArgsToString("strokeRunPitch=%.2f\n", param.strokeRunPitch);
                szBuffer += pesVAArgsToString("strokeWidth=%.2f\n", param.strokeWidth);
                szBuffer += pesVAArgsToString("strokeDensity=%.2f\n", param.strokeDensity);
                szBuffer +=
                        pesVAArgsToString("strokeRunningInset=%.2f\n", param.strokeRunningInset);
                szBuffer += pesVAArgsToString("strokeJoinType=%i\n", param.strokeJoinType);
                szBuffer +=
                        pesVAArgsToString("strokeJoinTypeIndex=%i\n", param.strokeJoinTypeIndex);
                szBuffer += pesVAArgsToString("isCubicSuperPath=%i\n", param.isSatinColumnPath);
                szBuffer += pesVAArgsToString("isPPEFPath=%i\n", param.isPPEFPath);
                szBuffer += pesVAArgsToString("isTTFPath=%i\n", param.isTTFPath);
                // motif
                szBuffer += pesVAArgsToString("motif.size=%.2f\n", param.motif.size);
                szBuffer += pesVAArgsToString("motif.spacing=%.2f\n", param.motif.spacing);
                szBuffer += pesVAArgsToString("motif.runPitch=%.2f\n", param.motif.runPitch);
                szBuffer += pesVAArgsToString("motif.patternIndex=%i\n", param.motif.patternIndex);
                szBuffer += pesVAArgsToString("motif.patternName=%s\n",
                                              param.motif.patternName.c_str());
                // fill
                szBuffer += pesVAArgsToString("fillType=%i\n", param.fillType);
                szBuffer += pesVAArgsToString("fillTypeIndex=%i\n", param.fillTypeIndex);
                szBuffer += pesVAArgsToString("fillColorIndex=%i\n", param.fillColorIndex);
                // preset fill
                szBuffer += pesVAArgsToString("fill.underlay=%i\n", param.fill.underlay);
                szBuffer += pesVAArgsToString("fill.density=%.2f\n", param.fill.density);
                szBuffer += pesVAArgsToString("fill.sewDirection=%i\n", param.fill.sewDirection);
                szBuffer += pesVAArgsToString("fill.presetIndex=%i\n", param.fill.presetIndex);
                // patternFill
                szBuffer += pesVAArgsToString("patternFill.underlay=%i\n", param.patternFill.underlay);
                szBuffer += pesVAArgsToString("patternFill.density=%.2f\n", param.patternFill.density);
                szBuffer += pesVAArgsToString("patternFill.sewDirection=%i\n", param.patternFill.sewDirection);
                szBuffer += pesVAArgsToString("patternFill.patternIndex=%i\n", param.patternFill.patternIndex);
                szBuffer += pesVAArgsToString("patternFill.patternName=%s\n", param.patternFill.patternName.c_str());
                szBuffer += pesVAArgsToString("patternFill.patternSize=%.2f\n",param.patternFill.patternSize);
                // motifFill(PES5.0)
                szBuffer += pesVAArgsToString("motifFill.size=%.2f\n", param.motifFill.size);
                szBuffer += pesVAArgsToString("motifFill.spacing=%.2f\n", param.motifFill.spacing);
                szBuffer += pesVAArgsToString("motifFill.rowSpacing=%.2f\n", param.motifFill.rowSpacing);
                szBuffer += pesVAArgsToString("motifFill.patternIndex=%i\n", param.motifFill.patternIndex);
                szBuffer += pesVAArgsToString("motifFill.patternName=%s\n", param.motifFill.patternName.c_str());
                
                // autoSantinFill
                szBuffer += pesVAArgsToString("autoSatinFill.noneOverlap=%i\n", param.autoSatinFill.noneOverlap);
                
                szBuffer += pesVAArgsToString("byteLength=%i\n", buffers[bufferIdx++].size());

                // pom(PES4.0)
                szBuffer += pesVAArgsToString("italic=%i\n", param.italic);
                szBuffer += pesVAArgsToString("border=%i\n", param.border);
                szBuffer += pesVAArgsToString("pullCompensate=%.2f\n", param.pullCompensate);
                szBuffer += pesVAArgsToString("fontWeight=%i\n", param.fontWeight);
                szBuffer += pesVAArgsToString("shapeIndex=%i\n", param.shapeIndex);  // Jimmy
                szBuffer += pesVAArgsToString("borderColorIndex=%i\n", param.borderColorIndex);
                szBuffer += pesVAArgsToString("waveMagnitude=%i\n", param.waveMagnitude);
                szBuffer += pesVAArgsToString("slantUpAngle=%i\n", param.slantUpAngle);
                szBuffer += pesVAArgsToString("slantDownAngle=%i\n", param.slantDownAngle);
                szBuffer += pesVAArgsToString("fadeUpSlant=%i\n", param.fadeUpSlant);
                szBuffer += pesVAArgsToString("fadeDownSlant=%i\n", param.fadeDownSlant);
                szBuffer += pesVAArgsToString("ppefScaleX=%.2f\n", param.ppefScaleX);
                szBuffer += pesVAArgsToString("ppefScaleY=%.2f\n", param.ppefScaleY);
                szBuffer += pesVAArgsToString("lastFontSizeIndex=%i\n", param.lastFontSizeIndex);
                szBuffer += pesVAArgsToString("ppefReverseChar=%i\n", param.ppefReverseChar);
                szBuffer += pesVAArgsToString("ppefReverseSegment=%i\n", param.ppefReverseSegment);
                // END pom
                szBuffer += pesVAArgsToString("borderGap=%i\n", param.borderGap);
                szBuffer += pesVAArgsToString("borderGapY=%i\n", param.borderGapY);

//                const pesRectangle& bound = __pesDataList[i]->getScalableBoundingBox();
                pesRectangle bound = __pesDataList[i]->getBoundingBox();
                szBuffer += pesVAArgsToString("bound.x=%.2f\n", bound.x);
                szBuffer += pesVAArgsToString("bound.y=%.2f\n", bound.y);
                szBuffer += pesVAArgsToString("bound.width=%.2f\n", bound.width);
                szBuffer += pesVAArgsToString("bound.height=%.2f\n", bound.height);
            }
            if (param.type == pesData::ObjectType::OBJECT_TYPE_SHAPE) {
                // MARK: since v.0.60
                szBuffer += pesVAArgsToString("rotateValue=%.2f\n", param.rotateValue);
                szBuffer += pesVAArgsToString("shapeIndex=%i\n", param.shapeIndex);
                szBuffer += pesVAArgsToString("width=%.1f\n", param.width);
                szBuffer += pesVAArgsToString("height=%.1f\n", param.height);
                szBuffer += pesVAArgsToString("lineWidth=%.1f\n", param.lineWidth);
                szBuffer += pesVAArgsToString("density=%.1f\n", param.density);
            } else if (param.type == pesData::OBJECT_TYPE_PES2_TEXT) {
                szBuffer += pesVAArgsToString("rotateValue=%.2f\n", param.rotateValue);
                szBuffer += pesVAArgsToString("byteLength=%i\n", buffers[bufferIdx++].size());
            }
            szBuffer += pesVAArgsToString("Tag=%s\n", param.tag.c_str());
            szBuffer += pesVAArgsToString("groupId=%i\n", param.groupId); // PPES v504+

            // pes5
            szBuffer += pesVAArgsToString("fontName=%s\n", param.fontName.c_str());
            szBuffer += pesVAArgsToString("fontSize=%i\n", param.fontSize);
            szBuffer += pesVAArgsToString("lastFontSize=%i\n", param.lastFontSize);
            szBuffer += pesVAArgsToString("useColorFromPicker=%i\n", param.useColorFromPicker);
            szBuffer += pesVAArgsToString("rotateDegree=%.2f\n", param.rotateDegree);
            
            // PES5.0 Monogram Colors
            int numColors = (int)param.colors.size();
            if(numColors){
                szBuffer += "colors=";
                for(int j=0; j<numColors; j++){
                    szBuffer += pesVAArgsToString("%i", param.colors[j]);
                    if((j+1)<numColors){
                        szBuffer += ",";
                    }
                }
                szBuffer += "\n";
            }
            
            // pom - Fill fading
            szBuffer += pesVAArgsToString("fillFading=%i\n", param.fillFading);

            szBuffer += "\n";
        }

        buff.append(szBuffer);
        buff.append("[PINNDATA]\n");

        for (int i = 0; i < (int)buffers.size(); i++) {
            buff.append(buffers[i]);
            buffers[i].clear();
        }
    }

    return buff;
}

pesBuffer pesDocument::getPNGBuffer(bool dpi300) const {
    // bicubic blurry (useful for small piece)
    static constexpr SkCubicResampler bspline = {1, 0};
    // bicubic like photoshop
    static constexpr SkCubicResampler cardinal = {0, 0.75f};

    static const auto defaultSampling = SkSamplingOptions();
    static const auto blurSampling = SkSamplingOptions(bspline);
    static const auto bicubicSampling = SkSamplingOptions(cardinal);

    const auto& sampling = bicubicSampling;

    pesBuffer buff;
    pesVec2f hoop = getHoopSizeInMM();
    const int UNIT_PER_MM = 10;
    const float scale = dpi300 ? 1.1811f : 1.0f;
    const int w = (int)(scale * hoop.x * UNIT_PER_MM);
    const int h = (int)(scale * hoop.y * UNIT_PER_MM);

    SkImageInfo imageInfo = SkImageInfo::Make(w, h, SkColorType::kRGBA_8888_SkColorType, SkAlphaType::kUnpremul_SkAlphaType);
    size_t pixelLen = size_t(4) * w * h;
    void* pixelPtr = sk_malloc_throw(pixelLen);
    if (pixelPtr) {
        sk_sp<SkSurface> surface(SkSurface::MakeRasterDirectReleaseProc(imageInfo, pixelPtr, size_t(4) * w, release_direct_surface_storage, pixelPtr));
        SkCanvas* canvas = surface->getCanvas();
        //canvas->clear(0xffffffff);
        canvas->clear(SK_ColorTRANSPARENT);
        canvas->translate(SkScalarHalf(w), SkScalarHalf(h));
        canvas->scale(scale, scale);

        //int cnt = getObjectCount();
        //for(int i=0; i<cnt; i++){
        //const auto& doc = this;
        int i = -1;
        for (const auto& pes : __pesDataList) {
            ++i;
            if (pes->parameter.visible == false) {
                continue;
            }
            sk_sp<SkImage> img;
            if (pes->isScalable() && pes->getTotalStitches() == 0) {
                img = this->makePesScalableImageSnapshot(i);
            }
            else{
                if(pes->parameter.type==pesData::OBJECT_TYPE_BACKGROUND){
                    img = this->makePesBackgroundImageSnapshot(i);
                }
                else{
                    img = this->makePesImageSnapshot(i);
                }
            }
            pesRectangle bound(pes->getBoundingBox());
            SkRect dst = SkRect::MakeLTRB(-bound.width/2, -bound.height/2, bound.width/2, bound.height/2);
            canvas->save();
            {
                canvas->translate(bound.getCenter().x, bound.getCenter().y);
                float rotateDegree = pes->parameter.rotateDegree;
                canvas->rotate(rotateDegree);
                canvas->drawImageRect(img, dst, sampling);
            }
            canvas->restore();
        }
        canvas->flush();
        sk_sp<SkImage> image  = surface->makeImageSnapshot();
        //sk_sp<SkData> pngData = image->encodeToData();
        auto pngData = SkImageToPngData(image, dpi300 ? 11811 : 10000);
        buff.set((const char*)pngData->data(), pngData->size());
    }
    else{
        SkDebugf("getPNGBuffer() sk_malloc_fail\n");
    }
    
    return buff;
}

//pesBuffer pesDocument::getThumbnailPNGBuffer(int w, int h, int index) { return pesBuffer(); }

SkImageInfo imageInfoThumbnailPNG = SkImageInfo::Make( 400, 400, SkColorType::kRGBA_8888_SkColorType, SkAlphaType::kUnpremul_SkAlphaType);
sk_sp<SkSurface> surfaceThumbnailPNG = SkSurface::MakeRaster(imageInfoThumbnailPNG);

pesBuffer pesDocument::getThumbnailPNGBuffer(int wmax400,
                                             int hmax400,
                                             const int index,
                                             const float lineWidth) const { 

    auto drawStitches = [](SkCanvas* canvas, pesRectangle bound, pesData* pes, auto& parameter, const float lineWidth) -> void {
        canvas->save();
        const auto center = bound.getCenter();
        canvas->translate(center.x, center.y);
        canvas->rotate(parameter.rotateDegree);
        canvas->translate(-center.x, -center.y);

        const bool hideJump = true;

        SkPaint paint;
        paint.setStyle(SkPaint::Style::kStroke_Style);
        // paint.setAntiAlias(true);
        SkPaint::Cap cap = SkPaint::Cap::kButt_Cap;
        paint.setStrokeCap(cap);
        paint.setStrokeWidth(lineWidth);

        pesStitchBlockList blocks;
        pes->getStitchBlockList(blocks);

        SkColor colorJump = SK_ColorRED;
        SkColor colorReal = SK_ColorBLUE;

        float v0x, v0y, v1x, v1y;

        for (int bl = 0, blocksize = (int)blocks.size(); bl < blocksize; bl++) {
            pesStitchBlock& block = blocks[bl];
            pesColor c = block.color;
            colorReal = SkColorSetARGB(255, c.r, c.g, c.b);
            paint.setColor(colorReal);
            const auto vertices = block.polyline.getVertices();
            const auto types = block.types;
            // Draw Stitch
            for (int i = 0, j = 1, verticesize = (int)vertices.size(); j < verticesize; i = j++) {
                const auto type = types[j];
                if (hideJump && (type == JUMP_STITCH || type == TRIM_STITCH)) 
                    continue;
                const pesVec2f& v0 = vertices[i];
                const pesVec2f& v1 = vertices[j];
                v0x = v0.x;
                v0y = v0.y;
                v1x = v1.x;
                v1y = v1.y;

                if (cap != SkPaint::Cap::kButt_Cap) {
                    const float vCx = (v0x + v1x) / 2.0f;
                    const float vCy = (v0y + v1y) / 2.0f;
                    const float length = v0.distance(v1);
                    const float ratio = (length - lineWidth) / length;
                    v0x = (v0x - vCx) * ratio + vCx;
                    v0y = (v0y - vCy) * ratio + vCy;
                    v1x = (v1x - vCx) * ratio + vCx;
                    v1y = (v1y - vCy) * ratio + vCy;
                }
                canvas->drawLine(v0x, v0y, v1x, v1y, paint);
            }
        }
        canvas->restore();
    };
 
    if (wmax400 < 10) {
        wmax400 = 10;
    } 
    else if (wmax400 > 400) {
        wmax400 = 400;
    }

    if (hmax400 < 10) {
        hmax400 = 10;
    } 
    else if (hmax400 > 400) {
        hmax400 = 400;
    }

    pesBuffer buff;
    const int cnt = (int)__pesDataList.size();
    if (!cnt) {
        return buff;
    }
    if (index >= cnt) {
        return buff;
    }
    pesRectangle docBound;
    if (index < 0) {
        docBound = getBoundingBox(true, false);
    } 
    else {
        docBound = __pesDataList[index]->getRotatedBoundingBox();
    }

    const float margin = 3.0f;
    const float double_margin = margin * 2.0f;
    const int t_w = wmax400 - double_margin;
    const int t_h = hmax400 - double_margin;
    const float t_scale = MIN(t_w / docBound.width, t_h / docBound.height);
    const float x = t_scale * docBound.x;
    const float y = t_scale * docBound.y;
    const float w = t_scale * docBound.width;
    const float h = t_scale * docBound.height;

    if (surfaceThumbnailPNG->width() && surfaceThumbnailPNG->height()) {
        SkCanvas* canvas = surfaceThumbnailPNG->getCanvas();
        canvas->clear(SK_ColorTRANSPARENT);
        canvas->save();
        canvas->translate(-x + (t_w - w) / 2 + margin, -y + (t_h - h) / 2 + margin);
        canvas->scale(t_scale, t_scale);

        auto fndrawpes = [&](pesData *pes, int i) {
            if (pes) {
                const auto& parameter = pes->parameter;
                if (!parameter.visible) {
                    return;
                }
                const pesRectangle bound(pes->getBoundingBox());
                if (bound.width == 0 || bound.height == 0) {
                    return;
                }

                sk_sp<SkImage> img;
                if (pes->isScalable()) {
                    if (pes->getTotalStitches() == 0) {
                        img = makePesScalableImageSnapshot(i);
                    }
                    else {
                        drawStitches(canvas, bound, pes, parameter, lineWidth);
                        return;
                    }
                }
                else {
                    if (parameter.type == pesData::OBJECT_TYPE_BACKGROUND) {
                        img = makePesBackgroundImageSnapshot(i);
                    }
                    else {
                        drawStitches(canvas, bound, pes, parameter, lineWidth);
                        return;
                    }
                }
                canvas->save();
                const auto center = bound.getCenter();
                canvas->translate(center.x, center.y);
                canvas->rotate(parameter.rotateDegree);
                SkRect dst = SkRect::MakeLTRB(
                    -bound.width / 2, 
                    -bound.height / 2, 
                     bound.width / 2, 
                     bound.height / 2);
                canvas->drawImageRect(img, dst, SkSamplingOptions(SkFilterMode::kLinear));
                canvas->restore();
            }
        };

        if (index < 0) {
            int i = -1;
            for (const auto& pesDataItem : __pesDataList) {
                fndrawpes(pesDataItem.get(), ++i);
            }
        }
        else {
            fndrawpes(__pesDataList[index].get(), index);
        }
        canvas->flush();
        const SkIRect srcBounds = {0, 0, wmax400, hmax400};
        sk_sp<SkImage> image = surfaceThumbnailPNG->makeImageSnapshot(srcBounds);
        //sk_sp<SkData> pngData = image->encodeToData();
        auto pngData = SkImageToPngData(image, 10000);
        buff.set((const char*)pngData->data(), pngData->size());
        canvas->restore();
    } 
    else {
        SkDebugf("surfaceThumbnailPNG size:%d x %d\n",
                 surfaceThumbnailPNG->width(),
                 surfaceThumbnailPNG->height());
    }
    return buff;
}

pesBuffer pesDocument::getPreviewPNGBuffer(bool dpi300) const {
    pesBuffer buff;
    pesData data = pesMergeAllData(*this);
    if(data.getTotalStitches() > 0) {
        sk_sp<SkImage> image = makeImageSnapshot(data, -1.0f, -1.0f, -1000000, -1000000, dpi300);
        //sk_sp<SkData> pngData = image->encodeToData();
        auto pngData = SkImageToPngData(image, dpi300 ? 11811 : 10000);
        buff.set((const char*)pngData->data(), pngData->size());
    }
    
    data.clear();
    return buff;
}

pesBuffer pesDocument::exportBufferAs(const std::string as, const std::string options) const {
    if (as == "PPES") {
        return this->getPPESBuffer();
    } 
    else if (as == "PES") {
        bool orderByColor = false;
        bool center = false;
        std::vector<std::string> params = pesSplitString(options, ",");
        int len = params.size();
        if (len > 0) {
            auto param = params[0];
            if (pesStringIsDigits(param)) {
                orderByColor = pesToInt(param);
            } 
            else {
                if (param == "true") {
                    orderByColor = true;
                }
                else if (param == "false") {
                    orderByColor = false;
                }
            }
        }
        if (len > 1) {
            auto param = params[1];
            if (pesStringIsDigits(param)) {
                center = pesToInt(param);
            } 
            else {
                if (param == "true") {
                    center = true;
                }
                else if (param == "false") {
                    center = false;
                }
            }
        }
        return this->getPES5Buffer(orderByColor, center);
    }
    else if (as == "PNG") {
        bool previewStitch = (options == "1" || options == "true");
        if(previewStitch) {
            return this->getPreviewPNGBuffer();
        }
        else{
            return this->getPNGBuffer();
        }
    }
    else if (as == "DST") {
        return this->getDSTBuffer();
    }
    else if (as == "EXP") {
        return this->getEXPBuffer();
    }
    else if (as == "JEF") {
        return this->getJEFBuffer();
    }
    else if (as == "XXX") {
        return this->getXXXBuffer();
    }
    else if (as == "SVG") {
        int objIndex = -1;
        int forScanNcut = 1;
        std::vector<std::string> params = pesSplitString(options, ",");
        int len = params.size();
        if (len > 0) {
            auto param = params[0];
            if (pesStringIsDigits(param)) {
                objIndex = pesToInt(param);
            }
        }
        if (len > 1) {
            auto param = params[1];
            if (pesStringIsDigits(param)) {
                forScanNcut = pesToInt(param);
            }
        }
        if (objIndex > -1 && objIndex < this->getObjectCount()) {
            return __pesDataList[objIndex]->getSVGBuffer(forScanNcut);
        }
    }
    return pesBuffer();
}

pesBuffer pesDocument::getJSONBuffer(pesBuffer & data) const{
    pesBuffer buff;
    json j_ppes;
    json j_layers;

    if(__pesDataList.size()){
        // header
//        if (strAppNameMax32[0] == '\0') {
//            j_ppes["Appname"] = "PESLib";
//        } else {
//            j_ppes["Appname"] = strAppNameMax32;
//        }
        if (strAppName == "") {
            j_ppes["Appname"] = "PESLib";
        } else {
            j_ppes["Appname"] = strAppName;
        }
        j_ppes["Codename"] = "PESEngine";
        j_ppes["Version"] = PES_LIB_VERSION.c_str();
        j_ppes["numLayer"] = __pesDataList.size();
//        j_ppes["BackgroundWidth"] = __pesHoopSize.x;
//        j_ppes["BackgroundHeight"] = __pesHoopSize.y;
        j_ppes["BackgroundWidth"] = pesBackgroundWidth;
        j_ppes["BackgroundHeight"] = pesBackgroundHeight;

        vector<pesBuffer> buffers;
        for(int i=0; i<getObjectCount(); i++){
            if(__pesDataList[i]->parameter.type == pesData::OBJECT_TYPE_PES2_TEXT){
                pesBuffer buff = CreatePINNBuffer(*__pesDataList[i]);
                buffers.push_back(buff);
                buff.clear();
            }
            if (__pesDataList[i]->parameter.type == pesData::ObjectType::OBJECT_TYPE_PES) {
                pesBuffer buff = CreatePINNBuffer(*__pesDataList[i]);
                buffers.push_back(buff);
                buff.clear();
            }
            else if( __pesDataList[i]->isScalable() ) {
                buffers.push_back( __pesDataList[i]->getSVGBuffer());
            }
        }
        

        int bufferIdx = 0;
        for (int i = 0; i <getObjectCount(); i++) {
            pesData::Parameter param = __pesDataList[i]->parameter;
            if (param.type == pesData::ObjectType::OBJECT_TYPE_PES) {
                param.baselineIndex = 0;
            }

            json j_layer;
            j_layer["LayerId"] = i;
            j_layer["type"] = param.type;
            j_layer["text"] = param.text.c_str();
            j_layer["fontIndex"] = param.fontIndex;
            j_layer["fontSizeIndex"] = param.fontSizeIndex;
            j_layer["colorIndex"] = param.colorIndex;
            j_layer["baselineIndex"] = param.baselineIndex;
            j_layer["radiusValue"] = param.radiusValue;
            j_layer["angleValue"] = param.angleValue;

//           v 1.00
            j_layer["extraLetterSpace"] = param.extraLetterSpace;
            j_layer["extraSpace"] = param.extraSpace;

//            // v 1.90
            j_layer["locked"] = param.locked;
            j_layer["visible"] = param.visible;

            j_layer["centerx"] = __pesDataList[i]->centerx;
            j_layer["centery"] = __pesDataList[i]->centery;

            if(param.type == pesData::ObjectType::OBJECT_TYPE_PES){
                j_layer["rotateValue"] = 0.5;
                j_layer["numColorChange"] = __pesDataList[i]->numColorChange;
                j_layer["byteLength"] = buffers[bufferIdx++].size();
            }
            if (param.type == pesData::ObjectType::OBJECT_TYPE_BACKGROUND){
                j_layer["byteLength"] = buffers[bufferIdx++].size();
            }

            if(__pesDataList[i]->isScalable()){
                j_layer["convertToStitch"] = param.convertToStitch;
                j_layer["strokeType"] = param.strokeType;
                j_layer["strokeTypeIndex"] = param.strokeTypeIndex;
                j_layer["strokeRunPitch"] = param.strokeRunPitch;
                j_layer["strokeWidth"] = param.strokeWidth;
                j_layer["strokeDensity"] = param.strokeDensity;
                j_layer["strokeRunningInset"] = param.strokeRunningInset;
                j_layer["strokeJoinType"] = param.strokeJoinType;
                j_layer["strokeJoinTypeIndex"] = param.strokeJoinTypeIndex;
                j_layer["isCubicSuperPath"] = param.isSatinColumnPath;
                j_layer["isPPEFPath"] = param.isPPEFPath;
                j_layer["isTTFPath"] = param.isTTFPath;

                json j_motif, j_fill, j_preset, j_patternFill;
                // motif
                j_motif["size"] = param.motif.size;
                j_motif["spacing"] = param.motif.spacing;
                j_motif["runPitch"] = param.motif.runPitch;
                j_motif["patternIndex"] = param.motif.patternIndex;
                j_motif["patternName"] = param.motif.patternName.c_str();
                // fill
                j_fill["fillType"] = param.fillType;
                j_fill["fillTypeIndex"] = param.fillTypeIndex;
                j_fill["fillColorIndex"] = param.fillColorIndex;
                // preset fill
                j_preset["underlay"] = param.fill.underlay;
                j_preset["density"] = param.fill.density;
                j_preset["sewDirection"] = param.fill.sewDirection;
                j_preset["presetIndex"] = param.fill.presetIndex;
                // pattern fill
                j_patternFill["underlay"] = param.patternFill.underlay;
                j_patternFill["density"] = param.patternFill.density;
                j_patternFill["sewDirection"] = param.patternFill.sewDirection;
                j_patternFill["patternIndex"] = param.patternFill.patternIndex;
                j_patternFill["patternName"] = param.patternFill.patternName.c_str();
                j_patternFill["patternSize"] = param.patternFill.patternSize;
                j_layer["byteLength"] = buffers[bufferIdx++].size();

//                j_layer.push_back(j_motif);
//                j_layer.push_back(j_fill);
//                j_layer.push_back(j_preset);
//                j_layer.push_back(j_patternFill);
                j_layer["motif"] = j_motif;
                j_layer["fill"] = j_fill;
                j_layer["preset"] = j_preset;
                j_layer["patternFill"] = j_patternFill;

                // pom(PES4.0)
                j_layer["italic"] = param.italic;
                j_layer["border"] = param.border;
                j_layer["pullCompensate"] =  param.pullCompensate;
                j_layer["fontWeight"] = param.fontWeight;
                j_layer["shapeIndex"] = param.shapeIndex; // Jimmy
                j_layer["borderColorIndex"] = param.borderColorIndex;
                j_layer["waveMagnitude"] = param.waveMagnitude;
                j_layer["slantUpAngle"] = param.slantUpAngle;
                j_layer["slantDownAngle"] = param.slantDownAngle;
                j_layer["fadeUpSlant"] = param.fadeUpSlant;
                j_layer["fadeDownSlant"] = param.fadeDownSlant;
                j_layer["ppefScaleX"] = param.ppefScaleX;
                j_layer["ppefScaleY"] = param.ppefScaleY;
                j_layer["lastFontSizeIndex"]= param.lastFontSizeIndex;
                j_layer["ppefReverseChar"]= param.ppefReverseChar;
                j_layer["ppefReverseSegment"] = param.ppefReverseSegment;
                // END pom
                j_layer["borderGap"] = param.borderGap;
                j_layer["borderGapY"] = param.borderGapY;

                const pesRectangle & bound = __pesDataList[i]->getScalableBoundingBox();
                j_layer["bound.x"] = bound.x;
                j_layer["bound.y"] = bound.y;
                j_layer["bound.width"] = bound.width;
                j_layer["bound.height"] = bound.height;
            } // endif
            if(param.type == pesData::ObjectType::OBJECT_TYPE_SHAPE){
                j_layer["rotateValue"] = param.rotateValue;
                j_layer["shapeIndex"] = param.shapeIndex;
                j_layer["width"] = param.width;
                j_layer["height"] = param.height;
                j_layer["lineWidth"] = param.lineWidth;
                j_layer["density"] = param.density;
            }
            else if (param.type == pesData::OBJECT_TYPE_PES2_TEXT){
                j_layer["rotateValue"] = param.rotateValue;
                j_layer["byteLength"] = buffers[bufferIdx++].size();
            }
            j_layer["Tag"] = param.tag.c_str();
            j_layers.push_back(j_layer);
        } // end for
        j_ppes["layers"] = j_layers;

        for (int i = 0; i < (int)buffers.size(); i++) {
            buff.append(buffers[i]);
            buffers[i].clear();
        }
        if(buff.size()){
            data = buff;
        }
    }

//    pesBuffer out;
//    out.append(j_ppes.dump());
//    if(buff.size()){
//        out.append("\n[PINNDATA]\n");
//        out.append(buff);
//    }
    pesBuffer out;
    out.append(j_ppes.dump());
    return out;
}

pesData pesDocument::mergeAll() const {
    return pesMergeAllData(*this);
}

pesData pesMergeAllData(const pesDocument& doc) {
    pesData mergedData;
    mergedData.parameter.setType(pesData::ObjectType::OBJECT_TYPE_PES);

    int prevColor = -1;
    for (int i = 0; i < doc.getObjectCount(); i++) {
        if( doc.getDataObjects()[i]->parameter.visible == false ) {
            continue;
        }
        pesStitchBlockList blocks;
        doc.getDataObjects()[i]->getStitchBlockList(blocks);
        float rotateDegree = doc.getDataObjects()[i]->parameter.rotateDegree;
        pesVec2f center = doc.getDataObjects()[i]->getBoundingBox().getCenter();
        if (blocks.size()) {
            for (auto& block : blocks) {
                if (rotateDegree != 0) {
                    block.rotateAround(rotateDegree, center);
                }

                int color = block.colorIndex;
                if (color != prevColor) {
                    prevColor = color;
                    mergedData.fillBlocks.push_back(block);
                } else {
                    auto& back = mergedData.fillBlocks.back(); 
                    //back.polyline.insert(
                    //        block.polyline.getVertices(),
                    //        (int)back.polyline.size());
                    back.polyline.addVertices(block.polyline.getVertices());
                    back.types.insert(back.types.end(), block.types.begin(), block.types.end());
                }
            }
        }
    }
    pesRemove(mergedData.fillBlocks,
              [](pesStitchBlock& block) { return block.polyline.size() <= 0; });

    mergedData.recalculate();
    return mergedData;
}

#include "include/encode/SkPngEncoder.h"
#include "include/core/SkStream.h"
//#include "src/images/SkImageEncoderFns.h"
//#include "src/images/SkImageEncoderPriv.h"
////#include <third_party/externals/libpng/pngset.c>
//
////#include <third_party/externals/libpng/png.h>
////#include <pngconf.h>
//
//class SkPngEncoderMod : public SkPngEncoder {
//public:
//    static std::unique_ptr<SkEncoder> Make(SkWStream* dst,
//                                           const SkPixmap& src,
//                                           const Options& options);
//
//protected:
//    std::unique_ptr<SkPngEncoderMgr> getEncoderMgr() {
//        return std::move(fEncoderMgr);
//    }
//};
//
//std::unique_ptr<SkEncoder> SkPngEncoderMod::Make(SkWStream* dst,
//                                              const SkPixmap& src,
//                                              const Options& options) {
//    //if (!SkPixmapIsValid(src)) {
//    //    return nullptr;
//    //}
//    //
//    //std::unique_ptr<SkPngEncoderMgr> encoderMgr = SkPngEncoderMgr::Make(dst);
//    //if (!encoderMgr) {
//    //    return nullptr;
//    //}
//    //
//    //if (!encoderMgr->setHeader(src.info(), options)) {
//    //    return nullptr;
//    //}
//    //
//    //if (!encoderMgr->setColorSpace(src.info(), options)) {
//    //    return nullptr;
//    //}
//    //
//    //if (!encoderMgr->writeInfo(src.info())) {
//    //    return nullptr;
//    //}
//    //
//    //encoderMgr->chooseProc(src.info());
//    //
//    //return std::unique_ptr<SkPngEncoder>(new SkPngEncoder(std::move(encoderMgr), src));
//    
//    auto encoder = SkPngEncoder::Make(dst, src, options);
//    if (encoder.get()) {
//        auto pngencodermod = (SkPngEncoderMod*)encoder.get();
//        auto em = pngencodermod->getEncoderMgr();
//        //png_set_pHYs(em->pngPtr(), em->infoPtr(), 10000, 10000, 1);
//        if (!em->writeInfo(src.info())) {
//            return nullptr;
//        }
//        return encoder;
//    }
//    return nullptr;
//}

sk_sp<SkData> SkImageToPngData(sk_sp<SkImage> img, const uint32_t ppm) {
    auto rasterimg = img->makeRasterImage();
    if (!rasterimg.get()) {
        return SkData::MakeEmpty();
    }

    SkPixmap pixmap;
    if (!rasterimg->peekPixels(&pixmap)) {
        return SkData::MakeEmpty();
    }

    SkDynamicMemoryWStream buf;
    bool success = SkPngEncoder::Encode(&buf, pixmap, SkPngEncoder::Options(), ppm);
    return success ? buf.detachAsData() : SkData::MakeEmpty();
}

pesBuffer pesDocument::getPES1Buffer() const {
    pesData mergedData = pesMergeAllData(*this);
    return getPES1Buffer(mergedData);
}

pesBuffer pesDocument::getPES1Buffer(pesData& mergedData) const { 
    return pesCreatePES1Buffer(&mergedData);
}

pesBuffer pesDocument::getPES5Buffer(bool orderByColor, bool center) const {
    pesData mergedData = pesMergeAllData(*this);
    return getPES5Buffer(mergedData, orderByColor, center);
}

pesBuffer pesDocument::getPES5Buffer(pesData& mergedData,
                                     bool orderByColor,
                                     bool center) const {
    if (orderByColor) {
        mergedData.orderByColor();
    }
    int options = pes_OPTION_NONE;
    if (!center) {
        options |= pes_OPTION_NOT_CENTER_PEC;
    }
    return pesCreatePES5Buffer(&mergedData, options);
}

pesBuffer pesDocument::getPINNBuffer() const {
    pesData mergedData = pesMergeAllData(*this);
    return getPINNBuffer(mergedData);
}

pesBuffer pesDocument::getPINNBuffer(pesData& mergedData) const { 
    return pesCreatePINNBuffer(&mergedData);
}

static void pesVerticalFlip(pesData& mergedData) {
    int miny = mergedData.getBoundingBox().getMinY();
    int maxy = mergedData.getBoundingBox().getMaxY();
    for (int i = 0; i < (int)mergedData.fillBlocks.size(); i++) {
        pesStitchBlock& block = mergedData.fillBlocks[i];
        for (int j = 0; j < (int)block.size(); j++) {
            pesVec2f& stitch = block.polyline[j];
            stitch.y = pesMap(stitch.y, miny, maxy, maxy, miny);
        }
    }
}

pesBuffer pesDocument::getDSTBuffer() const {
    pesData mergedData = pesMergeAllData(*this);
    pesVerticalFlip(mergedData);
    return getDSTBuffer(mergedData);
}

pesBuffer pesDocument::getDSTBuffer(pesData& mergedData) const { 
    return pesCreateDSTBuffer(&mergedData);
}

pesBuffer pesDocument::getEXPBuffer() const {
    pesData mergedData = pesMergeAllData(*this);
    pesVerticalFlip(mergedData);
    return getEXPBuffer(mergedData);
}

pesBuffer pesDocument::getEXPBuffer(pesData& mergedData) const { 
    return pesCreateEXPBuffer(&mergedData);
}

pesBuffer pesDocument::getXXXBuffer() const {
    pesData mergedData = pesMergeAllData(*this);
    pesVerticalFlip(mergedData);
    return getXXXBuffer(mergedData);
}

pesBuffer pesDocument::getXXXBuffer(pesData& mergedData) const {
    return pesCreateXXXBuffer(&mergedData);
}

pesBuffer pesDocument::getJEFBuffer() const {
    pesData mergedData = pesMergeAllData(*this);
    pesVerticalFlip(mergedData);
    return getJEFBuffer(mergedData);
}

pesBuffer pesDocument::getJEFBuffer(pesData& mergedData) const { 
    return pesCreateJEFBuffer(&mergedData);
}

pesBuffer pesDocument::getGCodeBuffer() const {
    pesData mergedData = pesMergeAllData(*this);
    return getGCodeBuffer(mergedData);
}

pesBuffer pesDocument::getGCodeBuffer(pesData& mergedData) const { 
    return pesCreateGcodeBuffer(&mergedData);
}

//pesBuffer pesDocument::getDXFBuffer() const {
//    pesData mergedData = pesMergeAllData(*this);
//    pesBuffer buff = pesCreateDXFBuffer(&mergedData);
//    return buff;
//}

static bool __doc_dirty = false;
bool pesDocument::isDirty() const { return __doc_dirty; }
void pesDocument::setDirty(bool dirty) { __doc_dirty = dirty; }

pesRectangle pesDocument::getBoundingBox(bool onlyVisible, bool onlyStitch) const { 
    pesRectangle docBound;
    if (onlyVisible) {
        for (const auto& pesData : __pesDataList) {
            if (pesData->parameter.visible) {
                if (onlyStitch) {
                    if (pesData->getTotalStitches() > 0) {
                        if (docBound.isEmpty()) {
                            docBound.set(pesData->getRotatedBoundingBox());
                        } else {
                            docBound.growToInclude(pesData->getRotatedBoundingBox());
                        }
                    }
                } else {
                    if (docBound.isEmpty()) {
                        docBound.set(pesData->getRotatedBoundingBox());
                    } else {
                        docBound.growToInclude(pesData->getRotatedBoundingBox());
                    }
                }
            }
        }
    } 
    else {
        for (const auto& pesData : __pesDataList) {
            if (onlyStitch) {
                if (pesData->getTotalStitches() > 0) {
                    if (docBound.isEmpty()) {
                        docBound.set(pesData->getRotatedBoundingBox());
                    } else {
                        docBound.growToInclude(pesData->getRotatedBoundingBox());
                    }
                }
            } else {
                if (docBound.isEmpty()) {
                    docBound.set(pesData->getRotatedBoundingBox());
                } else {
                    docBound.growToInclude(pesData->getRotatedBoundingBox());
                }
            }
        }
    }
    return docBound;
}

// MARK: C Wrapper
pesDocument* pesGetDocument() { return pesDocument::getInstance(); }

//#include "include/core/SkData.h"
//#include "include/core/SkScalar.h"
//#include "include/core/SkSurface.h"
//#include "include/core/SkVertices.h"
//#include "modules/pes/include/pesPathUtility.hpp"

static void release_direct_surface_storage(void* pixels, void* context) {
    //if (pixels == context) {
    //    // SkDebugf("expected release context\n");
    //}
    sk_free(pixels);
}

sk_sp<SkImage> pesDocument::makePesScalableImageSnapshot(int idx, std::string filter) const {
    auto pesdata = __pesDataList[idx];
    if (!pesdata->paths.size()) {
        return nullptr;
    }
    float t_w = 96.0;
    float t_h = 96.0;

    //SkDebugf("filter:%s\n", filter.c_str());
    bool transformToLocalFilter = false;
    std::vector<std::string> params = pesSplitString(filter, ",");
    int len = params.size();
    //SkDebugf("params.size:%d\n", params.size());
    if (len > 1) {
        //SkDebugf("params[0]:%s\n", params[0].c_str());
        //SkDebugf("params[1]:%s\n", params[1].c_str());
        filter = params[0];
        if (pesStringIsDigits(params[1])) {
            transformToLocalFilter = pesToInt(params[1]) != 0;
        } 
        else {
            std::string strLower = std::string(params[1]);
            std::transform(strLower.begin(), strLower.end(), strLower.begin(), [](unsigned char c) {
                return std::tolower(c);
            });
            //SkDebugf("strLower:%s\n", strLower.c_str());
            if (strLower == "true" || strLower == "fitresult") {
                transformToLocalFilter = true;
            }
        }
    }

    if (len > 2 && pesStringIsDigits(params[2])) {
        t_w = t_h = pesToFloat(params[2]);
    }

    if (len > 3 && pesStringIsDigits(params[3])) {
        t_h = pesToFloat(params[3]);
    }

    //SkDebugf("transformToLocalFilter:%s\n", transformToLocalFilter ? "true" : "false");

    const auto& paths = pesdata->paths;

    // auto bound = pesdata->getBoundingBox();

    //SkDebugf("bound:%.01f,%.01f,%.01f,%.01f\n", bound.x, bound.y, bound.width, bound.height);
    //pesdata->calcBoundingBox();

    // calculate actual bound (exclude invisible path)
    pesRectangle bound;
    pesRectangle rectFilter;

    if (filter.length()) {
        int pidx = 0;
        const int plen = paths.size();
        for (; pidx < plen; pidx++) {
            const auto& path = paths[pidx];
            if (!pesIsStringStartsWith(path.group_id, filter)) {
                continue;
            }
            rectFilter.set(path.getBoundingBox());
            break;
        }
        for (; pidx < plen; pidx++) {
            const auto& path = paths[pidx];
            if (!pesIsStringStartsWith(path.group_id, filter)) {
                continue;
            }
            rectFilter.growToInclude(path.getBoundingBox());
        }
    }

    if (filter.length() && transformToLocalFilter) {
        bound.set(rectFilter.x, rectFilter.y, rectFilter.width, rectFilter.height);
    } 
    else {
        bound = pesdata->getStrokeBoundingBox();
    }

    //SkDebugf("actual bound:%.01f,%.01f,%.01f,%.01f\n", bound.x, bound.y, bound.width, bound.height);

    // scales, not only x-axis
    const float pes_scale = std::min(abs(pesdata->parameter.ppefScaleX), abs(pesdata->parameter.ppefScaleY));
    const float margin = 3.0f;
    const float double_margin = 2.0f * margin;
    if (bound.width > 0 && bound.height > 0) {
        float fw = bound.width + double_margin;
        float fh = bound.height + double_margin;
        float t_scale = 1.0f;
        int w = (int)ceil(fw * t_scale);
        int h = (int)ceil(fh * t_scale);

        // up scale & up resolution for best quality image
        const float scale = 1.0f;

        if( filter.length() ) {
            t_scale = MIN(t_w / fw, t_h / fh);
            w = (int)ceil(fw * t_scale);
            h = (int)ceil(fh * t_scale);
        } 
        else {
            // up scale & up resolution for best quality image
            t_scale = scale;
            w = (int)ceil(fw * t_scale);
            h = (int)ceil(fh * t_scale);
        }

        SkImageInfo imageInfo = SkImageInfo::Make(w, h, SkColorType::kRGBA_8888_SkColorType, SkAlphaType::kUnpremul_SkAlphaType);
        size_t pixelLen = size_t(4) * w * h;         // // it's 8888, so 4 bytes per pixel
        void* pixelPtr = sk_malloc_throw(pixelLen);  // sk_malloc_canfail(pixelLen);
        if (pixelPtr) {
            sk_sp<SkSurface> surface(
                SkSurface::MakeRasterDirectReleaseProc(
                    imageInfo, 
                    pixelPtr, 
                    size_t(4) * w, 
                    release_direct_surface_storage, pixelPtr));
            SkCanvas* canvas = surface->getCanvas();

            // canvas->translate((-bound.x + margin) * t_scale, (-bound.y + margin) * t_scale);
            canvas->translate(SkScalarHalf(w), SkScalarHalf(h));
            canvas->translate((-bound.x - SkScalarHalf(bound.width))  * t_scale,
                              (-bound.y - SkScalarHalf(bound.height)) * t_scale);

            if (transformToLocalFilter || t_scale == scale) {
                canvas->scale(t_scale, t_scale);
            }

            canvas->clear(SK_ColorTRANSPARENT);
            pesRectangle rect;
            rect.set(0,0,0,0);
            if( pesdata->parameter.visible ) {
                SkPaint paint;
                paint.setAntiAlias(true);
                paint.setStrokeCap(SkPaint::Cap::kButt_Cap);
                paint.setStrokeJoin(SkPaint::Join::kBevel_Join);

                SkPaint strokeBlendDiff;
                strokeBlendDiff.setBlendMode(SkBlendMode::kDifference);

                //pesRectangle rectFilter;
                //rectFilter.set(0,0,0,0);
                if (filter.length() && !transformToLocalFilter) {
                    rect.set(bound.x * t_scale, bound.y * t_scale, w, h );
                    //for (auto& path : paths) {
                    //    if (!pesIsStringStartsWith(path.group_id, filter)) {
                    //        continue;
                    //    }
                    //    rectFilter = rectFilter.getUnion(path.getBoundingBox());
                    //}
                }

                if( rect.isEmpty() == false ) {
                    float scx = (rect.width * 0.90) / rectFilter.width;
                    float scy = (rect.height * 0.90) / rectFilter.height;
                    float sc = std::min( scx, scy );

                    float sx = rectFilter.x * sc;
                    float sy = rectFilter.y * sc;

                    float tx = rect.x + (rect.width - rectFilter.width * sc) / 2;
                    float ty = rect.y + (rect.height - rectFilter.height * sc) / 2;

                    canvas->translate( tx-sx, ty-sy );
                    canvas->scale( sc, sc );
                }

                for (auto& path : paths) {
                    if( filter.length() ) {
                        if( !pesIsStringStartsWith( path.group_id, filter ) ) {
                            continue;
                        }   
                    }
                    else {
                        if( path.bVisible == false ) {
                            continue;
                        }
                    }                
 
                    SkPath skpath = toSk(path);
                    if (path.isFill()) {
                        pesColor c = path.getFillColor();
                        paint.setColor(SkColorSetRGB(c.r, c.g, c.b));
                        paint.setStyle(SkPaint::Style::kFill_Style);
                        canvas->drawPath(skpath, paint);
                    }

                    // fixed stroke width
                    // reproduce
                    // - resize shape to negative width
                    // reasons
                    // - ppefScaleX can be negative
                    // - scales, not only x-axis
                    // - from SkPaint::setStrokeWidth (see: SkPaint.h)  
                    //   negative stroke-widths are invalid,
                    //   setting a negative width will have no effect.
                    if (path.isStroke()) {
                        if (skpath.isInverseFillType()) {
                            skpath.toggleInverseFillType();
                        }

                        auto strokewidth = abs(path.getStrokeWidth());
                        if (path.isFill() && path.getCommands().size() > 2) {
                            strokewidth = abs(strokewidth * pes_scale);
                        }
                        paint.setStrokeWidth(strokewidth);

                        //auto r = SkStrokeRec::GetInflationRadius(SkPaint::Join::kBevel_Join,
                        //                                         0,
                        //                                         SkPaint::Cap::kButt_Cap,
                        //                                         strokewidth);

                        pesColor c = path.getStrokeColor();
                        paint.setColor(SkColorSetRGB(c.r, c.g, c.b));
                        paint.setStyle(SkPaint::Style::kStroke_Style);
                        //canvas->saveLayer(nullptr, &strokeBlendDiff);
                        canvas->drawPath(skpath, paint);
                        //canvas->restore();
                    }
                    skpath.reset();
                }
                canvas->flush();
            }
            return surface->makeImageSnapshot();
        } else {
            SkDebugf("makePesScalableImageSnapshot() sk_malloc_fail\n");
        }
    }
    return nullptr;
}

sk_sp<SkImage> pesDocument::makePesBackgroundImageSnapshot(int idx) const {
    auto pesdata = __pesDataList[idx];
    if (pesdata->parameter.visible) {
        return pesdata->parameter.textureBackground;
    }
    return nullptr;
}

//sk_sp<SkImage> pesDocument::makePesImageSnapshot(int idx) {
//    auto pesdata = __pesDataList[idx];
//    auto bound = pesdata->getBoundingBox();
//    if (bound.width > 0 && bound.height > 0) {
//        int w = (int)ceil(bound.width);
//        int h = (int)ceil(bound.height);
//
//        SkImageInfo imageInfo = SkImageInfo::Make(w, h, SkColorType::kRGBA_8888_SkColorType,
//                                                  SkAlphaType::kUnpremul_SkAlphaType);
//        size_t pixelLen = w * h * 4;                 // // it's 8888, so 4 bytes per pixel
//        void* pixelPtr = sk_malloc_throw(pixelLen);  // sk_malloc_canfail(pixelLen);
//        if (pixelPtr) {
//            sk_sp<SkSurface> surface(SkSurface::MakeRasterDirectReleaseProc(
//                    imageInfo, pixelPtr, w * 4, release_direct_surface_storage, pixelPtr));
//            SkCanvas* canvas = surface->getCanvas();
//            canvas->translate(-bound.x, -bound.y);
//            canvas->clear(SK_ColorTRANSPARENT);
//            SkPaint paint;
//            paint.setStyle(SkPaint::kStroke_Style);
//            paint.setStrokeWidth(1);
//            paint.setAntiAlias(true);
//
//            pesStitchBlockList blocks;
//            pesdata->getStitchBlockList(blocks);
//            for (auto& block : blocks) {
//                pesColor c = block.color;
//                paint.setColor(SkColorSetRGB(c.r, c.g, c.b));
//                auto vertices = block.polyline.getVertices();
//                SkPath skpath;
//                for (int i = 0; i < vertices.size(); i++) {
//                    auto v = vertices[i];
//                    if (i == 0) {
//                        skpath.moveTo(v.x, v.y);
//                    } else {
//                        skpath.lineTo(v.x, v.y);
//                    }
//                    canvas->drawCircle(v.x, v.y, 1, paint);
//                }
//                canvas->drawPath(skpath, paint);
//                skpath.reset();
//            }
//
//            canvas->flush();
//            return surface->makeImageSnapshot();
//        } else {
//            SkDebugf("makePesImageSnapshot() sk_malloc_fail\n");
//        }
//    }
//    return nullptr;
//}

//sk_sp<SkImage> GetResourceAsImage
#include "tools/Resources.h"
#include "tools/ToolUtils.h"
#include <string>
#ifdef __EMSCRIPTEN__
#include "emscripten.h"
#include "emscripten/fetch.h"
#endif
#include <include/effects/SkGradientShader.h>
#include <include/utils/SkParsePath.h>
#include <include/core/SkStrokeRec.h>

#ifdef __EMSCRIPTEN__
void pes_emscriptenDownloadAsset( void *widget,
    std::string url,
    void (*onsuccess)(struct emscripten_fetch_t *fetch),
    void (*onerror)(struct emscripten_fetch_t *fetch) ) {
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);

    strcpy(attr.requestMethod, "GET");

    //std::string full_url = std::string( "/resources/") + url;
    std::string full_url;
    if (url[0] == '/') {
        full_url = std::string(url);
    } else if (url.find("resources/") == 0) {
        // full_url = std::string("/") + url;
        full_url = std::string("") + url;
    } else {
        // full_url = std::string("/resources/") + url;
        full_url = GetResourcePath(url.c_str()).c_str();
    }
    
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.userData = (void *)widget;
    attr.onsuccess = onsuccess;
    attr.onerror = onerror;
    emscripten_fetch(&attr, full_url.c_str());
}
#endif

void pesLoadOrDownloadAsset(const char* path, std::function<void(sk_sp<SkData> data)> onLoaded){
#ifdef __EMSCRIPTEN__
    struct lamda_t{
        std::function<void(sk_sp<SkData> data)> onLoaded;
    };
    lamda_t* plamda = new lamda_t();
    plamda->onLoaded = onLoaded;
    pes_emscriptenDownloadAsset( plamda, path,
        [](struct emscripten_fetch_t *fetch) {
            lamda_t* plamda = (lamda_t*)fetch->userData;
            sk_sp<SkData> data = SkData::MakeWithCopy( fetch->data, fetch->numBytes );
            if(data && data->size()){
                plamda->onLoaded(data);
            }
            delete plamda;
        },
        [](struct emscripten_fetch_t *fetch) {
            SkDebugf( "PES5_LoadOrDownloadAsset() Download failed\n" );
        }
    );
#else
    sk_sp<SkData> data = GetResourceAsData(path);
    if(data && data->size()){
        onLoaded(data);
    }
#endif
}

const int linestitch20x4_png_raw_size = 372;
uint8_t linestitch20x4_png_raw[linestitch20x4_png_raw_size] = {
137, 80,  78,  71,  13,  10,  26,  10,  0,   0,   0,   13,  73,  72,  68,  82,  0,   0,
0,   20,  0,   0,   0,   4,   8,   6,   0,   0,   0,   142, 95,  31,  150, 0,   0,   0,
9,   112, 72,  89,  115, 0,   0,   11,  19,  0,   0,   11,  19,  1,   0,   154, 156, 24,
0,   0,   0,   32,  99,  72,  82,  77,  0,   0,   122, 37,  0,   0,   128, 131, 0,   0,
249, 255, 0,   0,   128, 233, 0,   0,   117, 48,  0,   0,   234, 96,  0,   0,   58,  152,
0,   0,   23,  111, 146, 95,  197, 70,  0,   0,   0,   250, 73,  68,  65,  84,  120, 218,
84,  204, 65,  78,  195, 48,  16,  64,  209, 177, 221, 38,  61,  1,   21,  93,  112, 145,
100, 1,   135, 170, 122, 15,  54,  220, 162, 89,  112, 151, 176, 71,  40,  34,  165, 149,
51,  246, 140, 61,  147, 226, 192, 2,   9,   193, 151, 254, 246, 153, 195, 225, 112, 223,
52,  205, 211, 205, 118, 123, 107, 0,   216, 57,  151, 172, 115, 108, 192, 80,  89,  10,
207, 170, 81,  69,  98,  202, 25,  137, 104, 34,  34,  143, 136, 23,  68,  252, 136, 204,
103, 10,  225, 20,  2,   14,  215, 235, 181, 136, 40,  152, 227, 241, 184, 180, 109, 107,
156, 179, 80,  85,  53,  212, 117, 13,  235, 245, 26,  254, 86,  74,  1,   21,  1,   81,
5,   102, 254, 153, 8,   66,  140, 128, 136, 224, 189, 255, 28,  199, 241, 181, 239, 251,
103, 211, 117, 221, 87,  211, 52,  224, 156, 131, 170, 170, 96,  179, 217, 192, 106, 181,
250, 7,   46,  203, 2,   170, 10,  34,  2,   41,  165, 95,  52,  18,  65,  12,  1,   188,
159, 96,  24,  134, 203, 75,  223, 119, 102, 191, 223, 63,  180, 109, 251, 184, 219, 237,
238, 172, 181, 98,  173, 77,  214, 217, 108, 193, 166, 178, 148, 52,  207, 115, 202, 57,
199, 44,  25,  99,  136, 33,  198, 232, 67,  8,   23,  68,  60,  19,  243, 59,  78,  211,
192, 204, 111, 170, 42,  170, 10,  223, 3,   0,   197, 252, 186, 46,  167, 222, 86,  0,
0,   0,   0,   0,   73,  69,  78,  68,  174, 66,  96,  130
};

sk_sp<SkImage> __lineStitch = SkImage::MakeFromEncoded(SkData::MakeWithoutCopy(linestitch20x4_png_raw, linestitch20x4_png_raw_size));

sk_sp<SkImage> __lineStitch_40x8  = nullptr;
sk_sp<SkImage> __lineStitch_60x12 = nullptr;
sk_sp<SkImage> __lineStitch_80x16 = nullptr;

sk_sp<SkSurface> __surface_20x4  = SkSurface::MakeRaster(SkImageInfo::Make(20,  4, SkColorType::kRGBA_8888_SkColorType, SkAlphaType::kUnpremul_SkAlphaType));
sk_sp<SkSurface> __surface_40x8  = SkSurface::MakeRaster(SkImageInfo::Make(40,  8, SkColorType::kRGBA_8888_SkColorType, SkAlphaType::kUnpremul_SkAlphaType));
sk_sp<SkSurface> __surface_60x12 = SkSurface::MakeRaster(SkImageInfo::Make(60, 12, SkColorType::kRGBA_8888_SkColorType, SkAlphaType::kUnpremul_SkAlphaType));
sk_sp<SkSurface> __surface_80x16 = SkSurface::MakeRaster(SkImageInfo::Make(80, 16, SkColorType::kRGBA_8888_SkColorType, SkAlphaType::kUnpremul_SkAlphaType));

bool loadAssets(bool reload) {
    if (reload) {
        _ready = false;
        __lineStitch_40x8 = nullptr;
        __lineStitch_60x12 = nullptr;
        __lineStitch_80x16 = nullptr;
    }

    if (__lineStitch_40x8 == nullptr)
        pesLoadOrDownloadAsset("texture/lineStitch-40x8.png", [=](sk_sp<SkData> data) {
            __lineStitch_40x8 = SkImage::MakeFromEncoded(data);
            if (__lineStitch_40x8 && __lineStitch_60x12 && __lineStitch_80x16) _ready = true;
        });
    if (__lineStitch_60x12 == nullptr)
        pesLoadOrDownloadAsset("texture/lineStitch-60x12.png", [=](sk_sp<SkData> data) {
            __lineStitch_60x12 = SkImage::MakeFromEncoded(data);
            if (__lineStitch_40x8 && __lineStitch_60x12 && __lineStitch_80x16) _ready = true;
        });
    if (__lineStitch_80x16 == nullptr)
        pesLoadOrDownloadAsset("texture/lineStitch-80x16.png", [=](sk_sp<SkData> data) {
            __lineStitch_80x16 = SkImage::MakeFromEncoded(data);
            if (__lineStitch_40x8 && __lineStitch_60x12 && __lineStitch_80x16) _ready = true;
        });
    
    //return &ready;
    return _ready;
}

// TODO: should be change name to makePesImageSnapshot makeStitchImageSnapshot
sk_sp<SkImage> pesDocument::makeImageSnapshot(pesData& pesdata, float maxw, float maxh, float x, float y, bool dpi300) const {
    // bicubic blurry (useful for small piece)
    static constexpr SkCubicResampler bspline = {1, 0};
    // bicubic like photoshop
    static constexpr SkCubicResampler cardinal = {0, 0.75f};

    static const auto defaultSampling = SkSamplingOptions();
    static const auto blurSampling = SkSamplingOptions(bspline);
    static const auto bicubicSampling = SkSamplingOptions(cardinal);

    bool hasTexture = true;
    //if (__lineStitch == nullptr) {
    //    hasTexture = false;
    //    // SkDebugf("makePesImageSnapshot() load texture fail\n");
    //}

    auto bound = pesdata.getBoundingBox();
    // scales, not only x-axis
    //const float pes_scale = std::min(abs(pesdata.parameter.ppefScaleX), abs(pesdata.parameter.ppefScaleY));
    //float margin = 3.0f * pes_scale;
    float margin = 3.0f;
    float double_margin = 2.0f * margin;
    if (bound.width > 0 && bound.height > 0) {
        if (maxw == -1) {
            maxw = bound.width;
        }

        if (maxh == -1) {
            maxh = bound.height;
        }

        if (x == -1000000) {
            x = bound.x;
        }

        if (y == -1000000) {
            y = bound.y;
        }

        // up scale & up resolution for best quality image
        const float scale = dpi300 ? 1.1811f : 1.0f;

        const SkSamplingOptions sampling = bicubicSampling;

        int w = (int)ceil(scale * (maxw + double_margin));
        int h = (int)ceil(scale * (maxh + double_margin));

        SkImageInfo imageInfo = SkImageInfo::Make(w, h, SkColorType::kRGBA_8888_SkColorType, SkAlphaType::kUnpremul_SkAlphaType);
        size_t pixelLen = size_t(4) * w * h;         // // it's 8888, so 4 bytes per pixel
        void* pixelPtr = sk_malloc_throw(pixelLen);  // sk_malloc_canfail(pixelLen);
        if (pixelPtr) {
            sk_sp<SkSurface> surface(SkSurface::MakeRasterDirectReleaseProc(imageInfo, pixelPtr, size_t(4) * w, release_direct_surface_storage, pixelPtr));
            SkCanvas* canvas = surface->getCanvas();
            //canvas->translate((- x + margin), (- y + margin));
            canvas->translate(SkScalarHalf(w), SkScalarHalf(h));
            canvas->scale(scale, scale);
            canvas->translate(-x - SkScalarHalf(bound.width),
                              -y - SkScalarHalf(bound.height));
            canvas->clear(SK_ColorTRANSPARENT);

            // Draw SVG shape faintly as reference background behind stitches
            if (pesdata.parameter.visible && pesdata.isScalable() && !pesdata.paths.empty()) {
                printf("[makeImageSnapshot] Drawing %d SVG paths at alpha=0x15\n", (int)pesdata.paths.size());
                SkPaint bgPaint;
                bgPaint.setAntiAlias(true);
                for (auto& path : pesdata.paths) {
                    if (path.bVisible == false) continue;
                    SkPath skpath = toSk(path);
                    if (path.isFill()) {
                        pesColor c = path.getFillColor();
                        bgPaint.setColor(SkColorSetARGB(0x15, c.r, c.g, c.b));
                        bgPaint.setStyle(SkPaint::Style::kFill_Style);
                        canvas->drawPath(skpath, bgPaint);
                    }
                    if (path.isStroke()) {
                        pesColor c = path.getStrokeColor();
                        bgPaint.setColor(SkColorSetARGB(0x15, c.r, c.g, c.b));
                        bgPaint.setStrokeWidth(abs(path.getStrokeWidth()));
                        bgPaint.setStyle(SkPaint::Style::kStroke_Style);
                        canvas->drawPath(skpath, bgPaint);
                    }
                    skpath.reset();
                }
            }

            if (pesdata.parameter.visible && pesdata.getTotalStitches()) {
                SkPaint paint;
                paint.setStyle(SkPaint::kStroke_Style);
                paint.setAntiAlias(true);
                paint.setStrokeCap(SkPaint::Cap::kRound_Cap);

                pesStitchBlockList blocks;
                pesdata.getStitchBlockList(blocks, true);

                // 0: Line, 1: Texture(default), 2: Underlay
                const int drawingmode = getDrawingMode();
                const bool hideJump = !isShowJumpStitch();

                for (int bl = 0; bl < blocks.size(); bl++) {
                    pesStitchBlock& block = blocks[bl];
                    pesColor c = block.color;
                    const SkColor colorReal = SkColorSetARGB(0xff, c.r, c.g, c.b);
                    constexpr SkColor colorHiLight = SkColorSetARGB(0x60, 0xff, 0xff, 0xff);
                    auto vertices = block.polyline.getVertices();
                    auto types = block.types;

                    // Draw Line
                    if (hasTexture == false || drawingmode == 0) {
                        for (int i = 0, j = 1, len = (int)vertices.size(); j < len; i = j++) {
                            auto type = types[j];
                            if (hideJump && (type == JUMP_STITCH || type == TRIM_STITCH)) continue;
                            const pesVec2f& v0 = vertices[i];
                            const pesVec2f& v1 = vertices[j];
                            pesVec2f vC;
                            vC.x = (v0.x + v1.x) / 2;
                            vC.y = (v0.y + v1.y) / 2;

                            const float length = v0.distance(v1);
                            pesVec2f v00 = v0;
                            v00.y = ((v00.y - vC.y) * ((length - 2.5) / length)) + vC.y;
                            v00.x = ((v00.x - vC.x) * ((length - 2.5) / length)) + vC.x;
                            pesVec2f v10 = v1;
                            v10.x = ((v10.x - vC.x) * ((length - 2.5) / length)) + vC.x;
                            v10.y = ((v10.y - vC.y) * ((length - 2.5) / length)) + vC.y;

                            pesVec2f v01 = v0;
                            v01.x = ((v01.x - vC.x) * 0.9) + vC.x;
                            v01.y = ((v01.y - vC.y) * 0.9) + vC.y;
                            pesVec2f v11 = v1;
                            v11.x = ((v11.x - vC.x) * 0.4) + vC.x;
                            v11.y = ((v11.y - vC.y) * 0.4) + vC.y;

                            pesVec2f v02 = v0;
                            v02.x = ((v02.x - vC.x) * 0.8) + vC.x;
                            v02.y = ((v02.y - vC.y) * 0.8) + vC.y;
                            pesVec2f v12 = v1;
                            v12.x = ((v12.x - vC.x) * 0.2) + vC.x;
                            v12.y = ((v12.y - vC.y) * 0.2) + vC.y;

                            if ((type == JUMP_STITCH || type == TRIM_STITCH)) {
                                paint.setColor(SK_ColorRED);
                                paint.setStrokeWidth(1);
                                canvas->drawLine(v0.x, v0.y, v1.x, v1.y, paint);
                            } else {
                                // border of thread color depends on luminance of thread color.
                                int bl = (0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b) / 2.5;
                                paint.setColor(0xFF000000 + (bl << 16) + (bl << 8) + bl);
                                paint.setStrokeWidth(3.5);
                                canvas->drawLine(v00.x, v00.y, v10.x, v10.y, paint);

                                paint.setColor(colorReal);
                                paint.setStrokeWidth(3);
                                canvas->drawLine(v00.x, v00.y, v10.x, v10.y, paint);
                                paint.setColor(colorHiLight);
                                paint.setStrokeWidth(2);
                                canvas->drawLine(v01.x, v01.y, v11.x, v11.y, paint);
                                paint.setColor(colorHiLight);
                                paint.setStrokeWidth(1);
                                canvas->drawLine(v02.x, v02.y, v12.x, v12.y, paint);
                            }
                        }
                    } 
                    else if (drawingmode == 1) {
                        const float threadWidth = 4.0f;
                        const float threadOffsetY = -threadWidth / 2.0f;

                        sk_sp<SkImage> lineStitch = __lineStitch;
                        sk_sp<SkSurface> surface = __surface_20x4;
                        if (_ready) {
                            if (scale > 4) {
                                lineStitch = __lineStitch_80x16;
                                surface = __surface_80x16;
                            }
                            else if (scale > 3) {
                                lineStitch = __lineStitch_60x12;
                                surface = __surface_60x12;
                            }
                            else if (scale > 2 || dpi300) {
                                lineStitch = __lineStitch_40x8;
                                surface = __surface_40x8;
                            }
                        }

                        //const int hEdgeW = lineStitch->height() / 2;
                        //const int vEdgeW = lineStitch->width()  / 4;
                        //const auto centerRect = SkIRect::MakeLTRB(vEdgeW / 2,
                        //                                          hEdgeW,
                        //                                          lineStitch->width() - vEdgeW,
                        //                                          lineStitch->height() - hEdgeW);

                        auto surfaceTemp = surface->makeSurface(surface->imageInfo());
                        if (surfaceTemp == nullptr) {
                            surfaceTemp = surface;
                        }

                        const SkRect srcRect = SkRect::MakeWH(lineStitch->width(), lineStitch->height());
                        paint.setStyle(SkPaint::kFill_Style);
                        paint.setColor(colorReal);
                        paint.setColorFilter(SkColorFilters::Blend(colorReal, SkBlendMode::kModulate));

                        SkCanvas* canvasTemp = surfaceTemp->getCanvas();
                        canvasTemp->clear(SK_ColorTRANSPARENT);
                        canvasTemp->drawImageRect(lineStitch,
                                                  srcRect,
                                                  srcRect,
                                                  defaultSampling,
                                                  &paint,
                                                  SkCanvas::kFast_SrcRectConstraint);
                        /*
                        // lighting
                        const float lw0 = .60f * lineStitch->height();
                        const float lw1 = .50f * lineStitch->height();
                        const float cy = .50f * lineStitch->height();
                        const float hlw0 = .50f * lw0;
                        const float hlw1 = .50f * lw1;
                        const float x00 = .20f * lineStitch->width() + hlw0;
                        const float x01 = .60f * lineStitch->width() - hlw0;
                        const float x10 = .30f * lineStitch->width() + hlw1;
                        const float x11 = .50f * lineStitch->width() - hlw1;

                        SkPaint p;
                        p.setStyle(SkPaint::kStroke_Style);
                        p.setAntiAlias(true);
                        p.setStrokeCap(SkPaint::Cap::kRound_Cap);
                        p.setColor(SkColorSetARGB(0x40, 0xff, 0xff, 0xff));
                        p.setStrokeWidth(lw0);
                        canvasTemp->drawLine(x00, cy, x01, cy, p);
                        p.setStrokeWidth(lw1);
                        canvasTemp->drawLine(x10, cy, x11, cy, p);

                        sk_sp<SkImage> img = surfaceTemp->makeImageSnapshot();

                        // clear effects
                        paint.setColorFilter(nullptr);
                        paint.setShader(nullptr);
                        paint.setBlendMode(SkBlendMode::kSrcOver);
                        */

                        // /*
                        paint.setColorFilter(nullptr);

                        // lighting
                        const float cx = .45f * lineStitch->width();
                        const float cy = .5f * lineStitch->height();
                        const float r = .45f * lineStitch->width();

                        const SkColor4f colors[] = {
                            {1.0f, 1.0f, 1.0f, 0.6f},
                            {0.5f, 0.5f, 0.5f, 0.0f},
                        };
                        auto shader = SkGradientShader::MakeRadial({cx, cy},
                                                                    r,
                                                                    colors,
                                                                    nullptr,
                                                                    nullptr,
                                                                    std::size(colors),
                                                                    SkTileMode::kClamp);
                        paint.setShader(std::move(shader));
                        paint.setBlendMode(SkBlendMode::kSrcATop);
                        canvasTemp->drawPaint(paint);

                        sk_sp<SkImage> img = surfaceTemp->makeImageSnapshot();

                        // clear effects
                        paint.setColorFilter(nullptr);
                        paint.setShader(nullptr);
                        paint.setBlendMode(SkBlendMode::kSrcOver);
                        // */

                        const auto srcW = srcRect.width();
                        const auto srcH = srcRect.height();
                    
                        const auto srcThreadBRect = SkRect::MakeXYWH(0, 0, srcH, srcH);
                        const auto srcThreadMRect = SkRect::MakeXYWH(srcH, 0, srcW - srcH - srcH, srcH);
                        const auto srcThreadERect = SkRect::MakeXYWH(srcThreadMRect.fRight, 0, srcH, srcH);

                        const auto minlength = threadWidth * 3;

                        for (int i = 0, j = 1, len = (int)vertices.size(); j < len; i = j++) {
                            auto type = types[j];
                            if (hideJump && (type == JUMP_STITCH || type == TRIM_STITCH)) continue;
                            const pesVec2f& v0 = vertices[i];
                            const pesVec2f& v1 = vertices[j];
                            const float length = v0.distance(v1);
                            const float angle = atan2f(v1.y - v0.y, v1.x - v0.x) * RAD_TO_DEG;
                            const SkRect destRect = SkRect::MakeXYWH(0, threadOffsetY, length, threadWidth);

                            canvas->save();
                            canvas->translate(v0.x, v0.y);
                            canvas->rotate(angle);
                            //canvas->drawImageRect(img, destRect, sampling);
                            //canvas->drawImageNine(img.get(), centerRect, destRect, SkFilterMode::kLinear);
                            
                            if (length < minlength) {
                                canvas->drawImageRect(img, destRect, sampling);
                            } 
                            else {
                                const auto destThreadBRect = SkRect::MakeXYWH(0, threadOffsetY, threadWidth, threadWidth);
                                const auto destThreadMRect = SkRect::MakeXYWH(threadWidth, threadOffsetY, length - threadWidth - threadWidth, threadWidth);
                                const auto destThreadERect = SkRect::MakeXYWH(destThreadMRect.fRight, threadOffsetY, threadWidth, threadWidth);
                                canvas->drawImageRect(img, srcThreadBRect, destThreadBRect, sampling, nullptr, SkCanvas::kFast_SrcRectConstraint);
                                canvas->drawImageRect(img, srcThreadMRect, destThreadMRect, sampling, nullptr, SkCanvas::kFast_SrcRectConstraint);
                                canvas->drawImageRect(img, srcThreadERect, destThreadERect, sampling, nullptr, SkCanvas::kFast_SrcRectConstraint);
                            
                            }
                            
                            canvas->restore();
                        }
                    } 
                    else if (drawingmode == 2) {
                        SkPaint paint;
                        paint.setAntiAlias(true);
                        paint.setStyle(SkPaint::Style::kStroke_Style);
                        paint.setStrokeCap(SkPaint::Cap::kButt_Cap);
                        paint.setStrokeJoin(SkPaint::Join::kBevel_Join);
                        paint.setStrokeWidth(0);

                        // Draw Line
                        for (int i = 0, j = 1, verticesize = (int)vertices.size(); j < verticesize;
                             i = j++) {
                            const auto type = types[j];
                            if (hideJump && (type == JUMP_STITCH || type == TRIM_STITCH)) continue;
                            const pesVec2f& v0 = vertices[i];
                            const pesVec2f& v1 = vertices[j];

                            if ((type == JUMP_STITCH || type == TRIM_STITCH)) {
                                paint.setColor(SK_ColorRED);
                            } else {
                                paint.setColor(colorReal);
                            }
                            canvas->drawLine(v0.x, v0.y, v1.x, v1.y, paint);
                        }
                    }
                }
            }

            canvas->flush();
            return surface->makeImageSnapshot();
        } 
        else {
            SkDebugf("makePesImageSnapshot() sk_malloc_fail\n");
        }
    }
    return nullptr;
}

sk_sp<SkImage> pesDocument::makePesImageSnapshot(int idx) const {
    return makeImageSnapshot(*__pesDataList[idx]);
}

//sk_sp<SkImage> pesDocument::makePesImageSnapshot2(int idx) {
//    //if(__lineStitch==nullptr){
//    //    //__lineStitch = GetResourceAsImage("texture/lineStitch.png");
//    //    //SkDebugf("makePesImageSnapshot() loading texture\n");
//    //}
//    
//    bool hasTexture = true;
//    if(__lineStitch == nullptr){
//        hasTexture = false;
//        //SkDebugf("makePesImageSnapshot() load texture fail\n");
//    }
//    
//    auto pesdata = __pesDataList[idx];
//    auto bound = pesdata->getBoundingBox();
//    if (bound.width > 0 && bound.height > 0) {
//        int w = (int)ceil(bound.width);
//        int h = (int)ceil(bound.height);
//
//        SkImageInfo imageInfo = SkImageInfo::Make(w, h, SkColorType::kRGBA_8888_SkColorType,
//                                                  SkAlphaType::kUnpremul_SkAlphaType);
//        size_t pixelLen = size_t(4) * w * h;         // // it's 8888, so 4 bytes per pixel
//        void* pixelPtr = sk_malloc_throw(pixelLen);  // sk_malloc_canfail(pixelLen);
//        if (pixelPtr) {
//            sk_sp<SkSurface> surface(SkSurface::MakeRasterDirectReleaseProc(
//                    imageInfo, pixelPtr, size_t(4) * w, release_direct_surface_storage, pixelPtr));
//
//            SkCanvas* canvas = surface->getCanvas();
//            canvas->translate(-bound.x, -bound.y);
//            canvas->clear(SK_ColorTRANSPARENT);
//            SkPaint paint;
//            paint.setAntiAlias(true);
//            paint.setStyle(SkPaint::Style::kStroke_Style);
//            paint.setStrokeCap(SkPaint::Cap::kButt_Cap);
//            paint.setStrokeJoin(SkPaint::Join::kBevel_Join);
//            paint.setStrokeWidth(1);
//
//            pesStitchBlockList blocks;
//            pesdata->getStitchBlockList(blocks);
//            for (auto& block : blocks) {
//                pesColor c = block.color;
//                paint.setColor(SkColorSetARGB(128, c.r, c.g, c.b));
//                auto vertices = block.polyline.getVertices();
//                if(hasTexture==false){
//                    SkPath skpath;
//                    for (int i = 1; i < (int)vertices.size(); i++) {
//                        auto v = vertices[i];
//                        auto v0 = vertices[i-1];
//                        // skip jump
//                        if(v.distance(v0)>=100){
//                            continue;
//                        }
//                        paint.setColor(SkColorSetARGB(192, 30, 30, 30));
//                        paint.setStrokeWidth( 3 );
//                        canvas->drawLine( v.x, v.y, v0.x, v0.y, paint );
//                        paint.setColor(SkColorSetRGB(c.r, c.g, c.b));
//                        paint.setStrokeWidth( 2 );
//                        canvas->drawLine( v.x, v.y, v0.x, v0.y, paint );
//                    }
//
//                    skpath.reset();
//                }
//                else{
//                    paint.setStyle(SkPaint::kFill_Style);
//                    paint.setColor(SkColorSetARGB(128, c.r, c.g, c.b));
//                    paint.setColorFilter(SkColorFilters::Blend(SkColorSetRGB(c.r, c.g, c.b), SkBlendMode::kModulate));
//
//                    for(int j=1; j<(int)vertices.size(); j++){
//                        auto stitch = vertices[j];
//                        auto prevStitch = vertices[j - 1];
//                        pesVec2f prev(prevStitch);
//                        pesVec2f cur(stitch);
//                        pesVec2f dir = cur - prev;
//                        float length = prev.distance(cur);
//                        // skip jump
//                        if(length>=100){
//                            continue;
//                        }
//                        pesVec2f right(1,0);
//                        float angle = right.angle(dir.normalize());
//                        float threadWidth = 4; // 2*GUI_scale
//                        
//                        SkRect dest = SkRect::MakeXYWH(0, 0, length, threadWidth);
//                        canvas->save();
//                        canvas->translate(prev.x, prev.y);
//                        canvas->rotate(angle);
//                        //canvas->drawImageRect(__lineStitch, __lineStitch->bounds(), dest, &paint);
//                        canvas->drawImageRect(__lineStitch,SkRect::MakeWH(__lineStitch->width(), __lineStitch->height()), dest,SkSamplingOptions(SkFilterMode::kLinear), &paint,SkCanvas::kFast_SrcRectConstraint);
//                        canvas->restore();
//                    }
//                }
//            }
//
//            canvas->flush();
//            return surface->makeImageSnapshot();
//        } else {
//            SkDebugf("makePesImageSnapshot2() sk_malloc_fail\n");
//        }
//    }
//    return nullptr;
//}

void pesDocument::drawPesObject(SkCanvas* canvas, int idx, float scale) const {
    // bicubic blurry (useful for small piece)
    static constexpr SkCubicResampler bspline = {1, 0};
    // bicubic like photoshop
    static constexpr SkCubicResampler cardinal = {0, 0.75f};

    static const auto defaultSampling = SkSamplingOptions();
    static const auto blurSampling = SkSamplingOptions(bspline);
    static const auto bicubicSampling = SkSamplingOptions(cardinal);

    bool hasTexture = true;
    //if (__lineStitch == nullptr) {
    //    hasTexture = false;
    //}

    auto pesdata = __pesDataList[idx];
    
    const SkSamplingOptions sampling = bicubicSampling;

    if (pesdata->getTotalStitches()) {
        auto bound = pesdata->getBoundingBox();

        canvas->save();
        //canvas->translate(-bound.x, -bound.y);
        //canvas->translate(-bound.width / 2, -bound.height / 2);
        canvas->translate(-bound.x - SkScalarHalf(bound.width),
                          -bound.y - SkScalarHalf(bound.height));
        if (pesdata->parameter.visible) {
            SkPaint paint;
            paint.setStyle(SkPaint::kStroke_Style);
            paint.setAntiAlias(true);
            paint.setStrokeCap(SkPaint::Cap::kRound_Cap);

            pesStitchBlockList blocks;
            pesdata->getStitchBlockList(blocks, true);

            // 0: Line, 1: Texture(default), 2: Underlay
            const int drawingmode = getDrawingMode();
            const bool hideJump = !isShowJumpStitch();

            for (int bl = 0; bl < blocks.size(); bl++) {
                pesStitchBlock& block = blocks[bl];
                pesColor c = block.color;
                const SkColor colorReal = SkColorSetARGB(0xff, c.r, c.g, c.b);
                constexpr SkColor colorHiLight = SkColorSetARGB(0x60, 0xff, 0xff, 0xff);
                auto vertices = block.polyline.getVertices();
                auto types = block.types;
                // Draw Stitch
                if (hasTexture == false || drawingmode == 0) {
                    for (int i = 0, j = 1, len = (int)vertices.size(); j < len; i = j++) {
                        auto type = types[j];
                        if (hideJump && (type == JUMP_STITCH || type == TRIM_STITCH)) continue;
                        const pesVec2f& v0 = vertices[i];
                        const pesVec2f& v1 = vertices[j];
                        pesVec2f vC;
                        vC.x = (v0.x + v1.x) / 2;
                        vC.y = (v0.y + v1.y) / 2;

                        const float length = v0.distance(v1);
                        pesVec2f v00 = v0;
                        v00.y = ((v00.y - vC.y) * ((length - 2.5) / length)) + vC.y;
                        v00.x = ((v00.x - vC.x) * ((length - 2.5) / length)) + vC.x;
                        pesVec2f v10 = v1;
                        v10.x = ((v10.x - vC.x) * ((length - 2.5) / length)) + vC.x;
                        v10.y = ((v10.y - vC.y) * ((length - 2.5) / length)) + vC.y;

                        pesVec2f v01 = v0;
                        v01.x = ((v01.x - vC.x) * 0.9) + vC.x;
                        v01.y = ((v01.y - vC.y) * 0.9) + vC.y;
                        pesVec2f v11 = v1;
                        v11.x = ((v11.x - vC.x) * 0.4) + vC.x;
                        v11.y = ((v11.y - vC.y) * 0.4) + vC.y;

                        pesVec2f v02 = v0;
                        v02.x = ((v02.x - vC.x) * 0.8) + vC.x;
                        v02.y = ((v02.y - vC.y) * 0.8) + vC.y;
                        pesVec2f v12 = v1;
                        v12.x = ((v12.x - vC.x) * 0.2) + vC.x;
                        v12.y = ((v12.y - vC.y) * 0.2) + vC.y;

                        if ((type == JUMP_STITCH || type == TRIM_STITCH)) {
                            paint.setColor(SK_ColorRED);
                            paint.setStrokeWidth(1);
                            canvas->drawLine(v0.x, v0.y, v1.x, v1.y, paint);
                        } else {
                            // border of thread color depends on luminance of thread color.
                            int bl = (0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b) / 2.5;
                            paint.setColor(0xFF000000 + (bl << 16) + (bl << 8) + bl);
                            paint.setStrokeWidth(3.5);
                            canvas->drawLine(v00.x, v00.y, v10.x, v10.y, paint);

                            paint.setColor(colorReal);
                            paint.setStrokeWidth(3);
                            canvas->drawLine(v00.x, v00.y, v10.x, v10.y, paint);
                            paint.setColor(colorHiLight);
                            paint.setStrokeWidth(2);
                            canvas->drawLine(v01.x, v01.y, v11.x, v11.y, paint);
                            paint.setColor(colorHiLight);
                            paint.setStrokeWidth(1);
                            canvas->drawLine(v02.x, v02.y, v12.x, v12.y, paint);
                        }
                    }
                }
                else if (drawingmode == 1) {
                    sk_sp<SkImage> lineStitch = __lineStitch;
                    sk_sp<SkSurface> surface = __surface_20x4;
                    if (_ready) {
                        if (scale > 4) {
                            lineStitch = __lineStitch_80x16;
                            surface = __surface_80x16;
                        }
                        else if (scale > 3) {
                            lineStitch = __lineStitch_60x12;
                            surface = __surface_60x12;
                        }
                        else if (scale > 2) {
                            lineStitch = __lineStitch_40x8;
                            surface = __surface_40x8;
                        }
                    }

                    //const int hEdgeW = lineStitch->height() / 2;
                    //const int vEdgeW = lineStitch->width()  / 4;
                    //const auto centerRect = SkIRect::MakeLTRB(vEdgeW / 2,
                    //                                          hEdgeW,
                    //                                          lineStitch->width() - vEdgeW,
                    //                                          lineStitch->height() - hEdgeW);

                    auto surfaceTemp = surface->makeSurface(surface->imageInfo());
                    if (surfaceTemp == nullptr) {
                        surfaceTemp = surface;
                    }

                    const float threadWidth = 4.0f;
                    const float threadOffsetY = -threadWidth / 2.0f;
                    const SkRect srcRect = SkRect::MakeWH(lineStitch->width(), lineStitch->height());

                    paint.setStyle(SkPaint::kFill_Style);
                    paint.setColor(colorReal);
                    paint.setColorFilter(SkColorFilters::Blend(colorReal, SkBlendMode::kModulate));

                    SkCanvas* canvasTemp = surfaceTemp->getCanvas();
                    canvasTemp->clear(SK_ColorTRANSPARENT);
                    canvasTemp->drawImageRect(lineStitch,
                                                   srcRect,
                                                   srcRect,
                                                   defaultSampling,
                                                   &paint,
                                                   SkCanvas::kFast_SrcRectConstraint);

                    paint.setColorFilter(nullptr);
                    
                    // lighting
                    const float cx = .45f * lineStitch->width();
                    const float cy = .5f * lineStitch->height();
                    const float r  = .45f * lineStitch->width();

                    const SkColor4f colors[] = {
                        {1.0f, 1.0f, 1.0f, 0.6f},
                        {0.5f, 0.5f, 0.5f, 0.0f},
                    };
                    auto shader = SkGradientShader::MakeRadial({cx, cy},
                                                               r,
                                                               colors,
                                                               nullptr,
                                                               nullptr,
                                                               std::size(colors),
                                                               SkTileMode::kClamp);
                    paint.setShader(std::move(shader));
                    paint.setBlendMode(SkBlendMode::kSrcATop);
                    canvasTemp->drawPaint(paint);

                    sk_sp<SkImage> img = surfaceTemp->makeImageSnapshot();

                    // clear effects
                    paint.setColorFilter(nullptr);
                    paint.setShader(nullptr);
                    paint.setBlendMode(SkBlendMode::kSrcOver);
                   
                    const auto srcW = srcRect.width();
                    const auto srcH = srcRect.height();
                    
                    const auto srcThreadBRect = SkRect::MakeXYWH(0, 0, srcH, srcH);
                    const auto srcThreadMRect = SkRect::MakeXYWH(srcH, 0, srcW - srcH - srcH, srcH);
                    const auto srcThreadERect = SkRect::MakeXYWH(srcThreadMRect.fRight, 0, srcH, srcH);

                    const auto minlength = threadWidth * 3;

                    for (int i = 0, j = 1, len = (int)vertices.size(); j < len; i = j++) {
                        auto type = types[j];
                        if (hideJump && (type == JUMP_STITCH || type == TRIM_STITCH)) continue;
                        const pesVec2f& v0 = vertices[i];
                        const pesVec2f& v1 = vertices[j];
                        const float length = v0.distance(v1);
                        const float angle = atan2f(v1.y - v0.y, v1.x - v0.x) * RAD_TO_DEG;
                        const SkRect destRect = SkRect::MakeXYWH(0, threadOffsetY, length, threadWidth);
                        
                        canvas->save();
                        canvas->translate(v0.x, v0.y);
                        canvas->rotate(angle);
                        //canvas->drawImageRect(img, destRect, sampling);
                        //canvas->drawImageNine(img.get(), centerRect, destRect, SkFilterMode::kLinear);
                        if (length < minlength) {
                            canvas->drawImageRect(img, destRect, sampling);
                        }
                        else {
                            const auto destThreadBRect = SkRect::MakeXYWH(0, threadOffsetY, threadWidth, threadWidth);
                            const auto destThreadMRect = SkRect::MakeXYWH(threadWidth, threadOffsetY, length - threadWidth - threadWidth, threadWidth);
                            const auto destThreadERect = SkRect::MakeXYWH(destThreadMRect.fRight, threadOffsetY, threadWidth, threadWidth);
                            canvas->drawImageRect(img, srcThreadBRect, destThreadBRect, sampling, nullptr, SkCanvas::kFast_SrcRectConstraint);
                            canvas->drawImageRect(img, srcThreadMRect, destThreadMRect, sampling, nullptr, SkCanvas::kFast_SrcRectConstraint);
                            canvas->drawImageRect(img, srcThreadERect, destThreadERect, sampling, nullptr, SkCanvas::kFast_SrcRectConstraint);
                        }
                        canvas->restore();
                    }
                } 
                else if (drawingmode == 2) {
                    SkPaint paint;
                    paint.setAntiAlias(true);
                    paint.setStyle(SkPaint::Style::kStroke_Style);
                    paint.setStrokeCap(SkPaint::Cap::kButt_Cap);
                    paint.setStrokeJoin(SkPaint::Join::kBevel_Join);
                    paint.setStrokeWidth(0);

                    // Draw Line
                    for (int i = 0, j = 1, verticesize = (int)vertices.size(); j < verticesize;
                         i = j++) {
                        const auto type = types[j];
                        if (hideJump && (type == JUMP_STITCH || type == TRIM_STITCH)) continue;
                        const pesVec2f& v0 = vertices[i];
                        const pesVec2f& v1 = vertices[j];

                        if ((type == JUMP_STITCH || type == TRIM_STITCH)) {
                            paint.setColor(SK_ColorRED);
                        } else {
                            paint.setColor(colorReal);
                        }
                        canvas->drawLine(v0.x, v0.y, v1.x, v1.y, paint);
                    }
                }
            }
        }
        canvas->restore();
    } 
    else {
        auto bound = pesdata->getBoundingBox();
        std::vector<pesPath>& paths = pesdata->paths;

        // scales, not only x-axis
        const float pes_scale = std::min(abs(pesdata->parameter.ppefScaleX), abs(pesdata->parameter.ppefScaleY));
        const float margin = 3.0f;

        canvas->save();
        //canvas->translate(-bound.x, -bound.y);
        //canvas->translate(-bound.width / 2, -bound.height / 2);
        canvas->translate(-bound.x - SkScalarHalf(bound.width), -bound.y - SkScalarHalf(bound.height));
        
        auto sbb = pesdata->getStrokeBoundingBox();
        const auto cliprect = SkRect::MakeXYWH(
            sbb.x, 
            sbb.y, 
            sbb.width, 
            sbb.height)
            .makeOutset(margin, margin);

        //canvas->clipRect(cliprect);
        canvas->saveLayer(cliprect, nullptr);

        if (pesdata->parameter.visible) {
            SkPaint paint;
            paint.setAntiAlias(true);
            paint.setStrokeCap(SkPaint::Cap::kButt_Cap);
            paint.setStrokeJoin(SkPaint::Join::kBevel_Join);

            SkPaint strokeBlendDiff;
            strokeBlendDiff.setBlendMode(SkBlendMode::kDifference);

            for (auto& path : paths) {
                if (path.bVisible == false) {
                    continue;
                }

                SkPath skpath = toSk(path);
                if (path.isFill()) {
                    pesColor c = path.getFillColor();
                    paint.setColor(SkColorSetRGB(c.r, c.g, c.b));
                    paint.setStyle(SkPaint::Style::kFill_Style);
                    canvas->drawPath(skpath, paint);
                }
                if (path.isStroke()) {
                    if (skpath.isInverseFillType()) {
                        skpath.toggleInverseFillType();
                    }

                    auto strokewidth = abs(path.getStrokeWidth());
                    if (path.isFill() && path.getCommands().size() > 2) {
                        strokewidth = abs(strokewidth * pes_scale);
                    }
                    paint.setStrokeWidth(strokewidth);

                    pesColor c = path.getStrokeColor();
                    paint.setColor(SkColorSetRGB(c.r, c.g, c.b));
                    paint.setStyle(SkPaint::Style::kStroke_Style);
                    //canvas->saveLayer(nullptr, &strokeBlendDiff);
                    canvas->drawPath(skpath, paint);
                    //canvas->restore(); //restore saveLayer BlendDiff
                }
                skpath.reset();
            }
        }
        canvas->restore(); //restore saveLayer clip
        canvas->restore(); //restore save
    }
}

bool __showJumpStitch = true;
void pesDocument::setShowJumpStitch(bool show){
    __showJumpStitch = show;
}

bool pesDocument::isShowJumpStitch() const {
    return __showJumpStitch;
}

// deprecated, use setDrawingMode instead, remove on future
bool __drawStitchTexture = true; // NooM set default drawStitchTexture
void pesDocument::setDrawStitchTexture(bool b){
    __drawStitchTexture = b;
}

// deprecated, use getDrawingMode instead, remove on future
bool pesDocument::isDrawStitchTexture() const {
    return __drawStitchTexture; 
}

// 0: Line, 1: Texture(default), 2: Underlay
int __drawingmode = 1;
void pesDocument::setDrawingMode(int mode) {
    if (mode >= 0 && mode <= 2) {
        __drawingmode = mode;
    }
}

int pesDocument::getDrawingMode() const { 
    return __drawingmode; 
}

int pesDocument::getBackgroundWidth(){
    return pesBackgroundWidth;
}

int pesDocument::getBackgroundHeight(){
    return pesBackgroundHeight;
}

void pesDocument::setChanged(bool changed){
    pesHasChanged = changed;
}

void pesDocument::flagHasChanged(){
    setChanged(true);
}

bool pesDocument::hasChanged() const {
    return pesHasChanged;
}

SkPath pesStitchToSkPath(const pesStitchBlockList & blocks){
    SkPath skpath;
    for (auto& block : blocks) {
        auto vertices = block.polyline.getVertices();
        for (int i = 0; i < (int)vertices.size(); i++) {
            auto v = vertices[i];
            if (i == 0) {
                skpath.moveTo(v.x, v.y);
            } else {
                skpath.lineTo(v.x, v.y);
            }
        }
    }
    float s = SK_PIXELS_PER_MM / PES_PIXELS_PER_MM;
    return skpath.makeScale(s, s);
}

SkPath pesRunStitch(SkPath p, float runPitch){
    float s = PES_PIXELS_PER_MM / SK_PIXELS_PER_MM;
    SkPath path = p.makeScale(s, s);
    pesPath pes_path = toPes(path);
    pes_path.setStrokeWidth(1.0);
    pesData data;
    data.paths.push_back(pes_path);
    data.parameter.strokeType = pesData::StrokeType::STROKE_TYPE_RUNNING;
    data.parameter.strokeRunPitch = runPitch;
    data.applyStroke();
    return pesStitchToSkPath(data.strokeBlocks);
}

SkPath pesFillStitch(SkPath p, pesData::FillType fillType, float density){
    float s = PES_PIXELS_PER_MM / SK_PIXELS_PER_MM;
    SkPath path = p.makeScale(s, s);
    pesData data;
    data.paths.push_back(toPes(path));
    data.parameter.fillType = fillType;
    data.parameter.fill.density = density;
    data.parameter.patternFill.density = density;
    data.applyFill();
    return pesStitchToSkPath(data.fillBlocks);
}

SkPath pesSatinStitch(SkPath p, float width, float density){
    float s = PES_PIXELS_PER_MM / SK_PIXELS_PER_MM;
    SkPath path = p.makeScale(s, s);
    pesPath pes_path = toPes(path);
    pes_path.setStrokeWidth(1.0);
    pesData data;
    data.paths.push_back(pes_path);
    data.parameter.strokeType = pesData::StrokeType::STROKE_TYPE_SATIN;
    data.parameter.strokeWidth = width;
    data.parameter.strokeDensity = density;
    data.applyStroke();
    return pesStitchToSkPath(data.strokeBlocks);
}

void pesDocument::updateInfo() const {
    char info[256];
    info[0] = 0;

    if (__documentInstance.get() != this) {
        return;
    } 

    long totalStitches = 0;
    if (!__pesDataList.empty()) {
        //int newMinX = INT_MAX;
        //int newMinY = INT_MAX;
        //int newMaxX = INT_MIN;
        //int newMaxY = INT_MIN;
        
        for (auto it = __pesDataList.begin(); it != __pesDataList.end(); it++) {
            //if ((*it)->minx < newMinX)
            //    newMinX = (*it)->minx;
            //
            //if ((*it)->miny < newMinY)
            //    newMinY = (*it)->miny;
            //
            //if ((*it)->maxx > newMaxX)
            //    newMaxX = (*it)->maxx;
            //
            //if ((*it)->maxy > newMaxY)
            //    newMaxY = (*it)->maxy;

            if ((*it)->parameter.visible) {
                totalStitches += (*it)->getTotalStitches();
            }
        }

        //float width = (float)(newMaxX - newMinX);
        //float height = (float)(newMaxY - newMinY);

        auto bound = getBoundingBox();

        sprintf(info, "Size: %.1f x %.1f mm.  Total stitches: %li",
                bound.width / 10,
                bound.height / 10,
                totalStitches);
    }
    else {
        sprintf( info, "No stitch." );
    }
#ifdef __EMSCRIPTEN__
        char szx[512];
        sprintf(szx, "if( window.pes5StatusMessage ) { pes5StatusMessage('%s'); }", info);
        emscripten_run_script(szx);
#else
    SkDebugf( "Update PES: %s\n", info );
#endif
}



namespace embeded_pattern {
  const char    net1[] = "@VERSION 10000\r\n\r\n@SYSENV 0\r\n@TILE 15\r\n@TILELINE 0 2 768 1024 1024 768\r\n@TILELINE 1 2 512 1024 1024 512\r\n@TILELINE 2 2 512 768 1024 256\r\n@TILELINE 3 2 512 512 1024 0\r\n@TILELINE 4 2 512 256 768 0\r\n@TILELINE 5 2 512 0 640 128\r\n@TILELINE 6 2 640 384 256 0\r\n@TILELINE 7 2 640 640 0 0\r\n@TILELINE 8 2 640 896 0 256\r\n@TILELINE 9 2 512 1024 0 512\r\n@TILELINE 10 2 0 768 256 1024\r\n@TILELINE 11 2 0 1024 128 896\r\n@TILELINE 12 2 0 768 128 640\r\n@TILELINE 13 2 0 512 128 384\r\n@TILELINE 14 2 0 256 128 128\r\n";
  const char    net2[] = "@VERSION 10000\r\n\r\n@SYSENV 2\r\n@TILE 6\r\n@TILELINE 0 2 320 0 320 704\r\n@TILELINE 1 2 1024 320 320 320\r\n@TILELINE 2 2 704 1024 704 320\r\n@TILELINE 3 2 0 704 704 704\r\n@TILELINE 4 2 0 0 0 1024\r\n@TILELINE 5 2 0 0 1024 0\r\n";
  const char    net3[] = "@VERSION 10000\r\n\r\n@SYSENV 1\r\n@TILE 10\r\n@TILELINE 0 2 0 256 512 768\r\n@TILELINE 1 2 256 512 768 0\r\n@TILELINE 2 2 512 256 1024 768\r\n@TILELINE 3 2 1024 768 1024 768\r\n@TILELINE 4 2 768 512 256 1024\r\n@TILELINE 5 2 768 0 1024 256\r\n@TILELINE 6 2 256 1024 0 768\r\n@TILELINE 7 2 0 768 0 768\r\n@TILELINE 8 2 0 256 256 0\r\n@TILELINE 9 2 1024 768 768 1024\r\n";
  const char    net4[] = "@VERSION 10000\r\n\r\n@SYSENV 2\r\n@TILE 14\r\n@TILELINE 0 2 0 320 512 832\r\n@TILELINE 1 2 192 512 704 0\r\n@TILELINE 2 2 512 192 1024 704\r\n@TILELINE 3 2 1024 768 1024 768\r\n@TILELINE 4 2 832 512 320 1024\r\n@TILELINE 5 2 704 0 1024 320\r\n@TILELINE 6 2 320 1024 0 704\r\n@TILELINE 7 2 0 768 0 768\r\n@TILELINE 8 2 0 320 320 0\r\n@TILELINE 9 2 1024 704 704 1024\r\n@TILELINE 10 2 96 416 512 0\r\n@TILELINE 11 2 608 96 1024 512\r\n@TILELINE 12 2 928 608 512 1024\r\n@TILELINE 13 2 416 928 0 512\r\n";
  const char   pat01[] = "@VERSION 10000\r\n\r\n@SYSENV 1\r\n@TILE 23\r\n@TILELINE 0 5 512 384 384 512 512 640 640 512 512 384\r\n@TILELINE 1 5 640 768 768 640 896 768 768 896 640 768\r\n@TILELINE 2 5 384 768 256 640 128 768 256 896 384 768\r\n@TILELINE 3 3 384 1024 512 896 640 1024\r\n@TILELINE 4 2 896 1024 1024 896\r\n@TILELINE 5 5 256 384 384 256 256 128 128 256 256 384\r\n@TILELINE 6 2 128 0 0 128\r\n@TILELINE 7 3 384 0 512 128 640 0\r\n@TILELINE 8 5 640 256 768 128 896 256 768 384 640 256\r\n@TILELINE 9 3 1024 384 896 512 1024 640\r\n@TILELINE 10 2 896 0 1024 128\r\n@TILELINE 11 2 0 896 128 1024\r\n@TILELINE 12 3 0 384 128 512 0 640\r\n@TILELINE 13 2 512 640 512 896\r\n@TILELINE 14 2 768 384 768 640\r\n@TILELINE 15 2 768 896 768 1024\r\n@TILELINE 16 2 768 128 768 0\r\n@TILELINE 17 2 512 128 512 384\r\n@TILELINE 18 2 256 384 256 640\r\n@TILELINE 19 2 256 128 256 0\r\n@TILELINE 20 2 256 896 256 1024\r\n@TILELINE 21 2 0 640 0 896\r\n@TILELINE 22 2 0 128 0 384\r\n";
  const char   pat02[] = "@VERSION 10000\r\n\r\n@SYSENV 1\r\n@TILE 16\r\n@TILELINE 0 5 512 0 0 512 512 1024 1024 512 512 0\r\n@TILELINE 1 2 512 128 512 896\r\n@TILELINE 2 2 384 256 384 768\r\n@TILELINE 3 2 256 384 256 640\r\n@TILELINE 4 2 640 256 640 768\r\n@TILELINE 5 2 768 384 768 640\r\n@TILELINE 6 2 256 128 256 0\r\n@TILELINE 7 2 128 256 128 0\r\n@TILELINE 8 2 0 384 0 0\r\n@TILELINE 9 2 0 640 0 1024\r\n@TILELINE 10 2 128 768 128 1024\r\n@TILELINE 11 2 256 896 256 1024\r\n@TILELINE 12 2 896 768 896 1024\r\n@TILELINE 13 2 768 896 768 1024\r\n@TILELINE 14 2 768 128 768 0\r\n@TILELINE 15 2 896 256 896 0\r\n";
  const char   pat03[] = "@VERSION 10000\r\n\r\n@SYSENV 0\r\n@TILE 2\r\n@TILELINE 0 3 0 0 512 1024 1024 0\r\n@TILELINE 1 3 0 1024 512 0 1024 1024\r\n";
  const char   pat04[] = "@VERSION 10000\r\n\r\n@SYSENV 2\r\n@TILE 8\r\n@TILELINE 0 9 0 512 64 704 224 896 384 992 512 1024 640 992 800 896 960 704 1024 512\r\n@TILELINE 1 5 512 0 448 192 288 384 128 480 0 512\r\n@TILELINE 2 5 512 0 576 192 736 384 896 480 1024 512\r\n@TILELINE 3 7 224 416 288 608 384 736 512 800 640 736 736 608 800 416\r\n@TILELINE 4 4 0 288 128 224 224 96 256 0\r\n@TILELINE 5 2 256 1024 288 928\r\n@TILELINE 6 2 768 1024 736 928\r\n@TILELINE 7 4 768 0 800 96 896 224 1024 288\r\n";
  const char   pat05[] = "@VERSION 10000\r\n\r\n@SYSENV 2\r\n@TILE 18\r\n@TILELINE 0 9 0 512 64 704 224 896 384 992 512 1024 640 992 800 896 960 704 1024 512\r\n@TILELINE 1 5 512 0 448 192 288 384 128 480 0 512\r\n@TILELINE 2 5 512 0 576 192 736 384 896 480 1024 512\r\n@TILELINE 3 7 224 416 288 608 384 736 512 800 640 736 736 608 800 416\r\n@TILELINE 4 4 0 288 128 224 224 96 256 0\r\n@TILELINE 5 2 256 1024 288 928\r\n@TILELINE 6 2 768 1024 736 928\r\n@TILELINE 7 4 768 0 800 96 896 224 1024 288\r\n@TILELINE 8 3 352 704 384 512 416 256\r\n@TILELINE 9 2 640 736 640 736\r\n@TILELINE 10 2 128 0 160 192\r\n@TILELINE 11 2 896 0 864 192\r\n@TILELINE 12 2 128 1024 96 768\r\n@TILELINE 13 2 896 1024 928 768\r\n@TILELINE 14 3 608 256 640 512 672 704\r\n@TILELINE 15 2 512 0 512 800\r\n@TILELINE 16 2 0 512 0 1024\r\n@TILELINE 17 2 0 0 0 288\r\n";
  const char   pat06[] = "@VERSION 10000\r\n\r\n@SYSENV 1\r\n@TILE 32\r\n@TILELINE 0 2 0 128 128 0\r\n@TILELINE 1 2 128 128 256 256\r\n@TILELINE 2 2 256 384 384 256\r\n@TILELINE 3 2 384 384 512 512\r\n@TILELINE 4 2 256 128 384 0\r\n@TILELINE 5 2 512 384 640 256\r\n@TILELINE 6 2 384 128 512 256\r\n@TILELINE 7 2 512 128 640 0\r\n@TILELINE 8 2 128 512 0 640\r\n@TILELINE 9 2 128 384 256 512\r\n@TILELINE 10 2 128 256 0 384\r\n@TILELINE 11 2 640 384 768 512\r\n@TILELINE 12 2 640 128 768 256\r\n@TILELINE 13 2 768 128 896 0\r\n@TILELINE 14 2 896 128 1024 256\r\n@TILELINE 15 2 896 256 768 384\r\n@TILELINE 16 2 896 384 1024 512\r\n@TILELINE 17 2 128 640 256 768\r\n@TILELINE 18 2 128 896 256 1024\r\n@TILELINE 19 2 384 512 256 640\r\n@TILELINE 20 2 384 768 256 896\r\n@TILELINE 21 2 384 640 512 768\r\n@TILELINE 22 2 384 896 512 1024\r\n@TILELINE 23 2 640 512 512 640\r\n@TILELINE 24 2 640 768 512 896\r\n@TILELINE 25 2 640 896 768 1024\r\n@TILELINE 26 2 640 640 768 768\r\n@TILELINE 27 2 768 640 896 512\r\n@TILELINE 28 2 896 640 1024 768\r\n@TILELINE 29 2 896 768 768 896\r\n@TILELINE 30 2 896 896 1024 1024\r\n@TILELINE 31 2 128 768 0 896\r\n";
  const char   pat07[] = "@VERSION 10000\r\n\r\n@SYSENV 2\r\n@TILE 12\r\n@TILELINE 0 2 256 0 0 160\r\n@TILELINE 1 2 256 0 512 160\r\n@TILELINE 2 2 512 160 768 0\r\n@TILELINE 3 2 768 0 1024 160\r\n@TILELINE 4 2 512 160 512 512\r\n@TILELINE 5 2 512 512 768 672\r\n@TILELINE 6 2 768 672 1024 512\r\n@TILELINE 7 2 512 512 256 672\r\n@TILELINE 8 2 256 672 0 512\r\n@TILELINE 9 2 256 672 256 1024\r\n@TILELINE 10 2 768 672 768 1024\r\n@TILELINE 11 2 0 160 0 512\r\n";
  const char   pat08[] = "@VERSION 10000\r\n\r\n@SYSENV 2\r\n@TILE 23\r\n@TILELINE 0 2 0 512 256 352\r\n@TILELINE 1 2 256 352 512 512\r\n@TILELINE 2 2 256 352 256 0\r\n@TILELINE 3 2 0 512 256 672\r\n@TILELINE 4 2 256 672 256 1024\r\n@TILELINE 5 2 256 672 512 512\r\n@TILELINE 6 2 512 512 768 352\r\n@TILELINE 7 2 768 352 1024 512\r\n@TILELINE 8 2 1024 512 768 672\r\n@TILELINE 9 2 768 672 512 512\r\n@TILELINE 10 2 768 672 768 1024\r\n@TILELINE 11 2 768 352 768 0\r\n@TILELINE 12 2 768 0 512 160\r\n@TILELINE 13 2 512 160 256 0\r\n@TILELINE 14 2 512 160 512 512\r\n@TILELINE 15 2 256 1024 512 864\r\n@TILELINE 16 2 512 864 768 1024\r\n@TILELINE 17 2 512 864 512 512\r\n@TILELINE 18 2 768 0 1024 160\r\n@TILELINE 19 2 256 0 0 160\r\n@TILELINE 20 2 256 1024 0 864\r\n@TILELINE 21 2 768 1024 1024 864\r\n@TILELINE 22 2 1024 160 1024 864\r\n";
  const char   pat09[] = "@VERSION 10000\r\n\r\n@SYSENV 2\r\n@TILE 5\r\n@TILELINE 0 29 512 0 544 0 672 32 800 96 928 224 992 352 1024 480 1024 512 1024 544 992 672 928 800 800 928 672 992\\\r\n 544 1024 512 1024 480 1024 352 992 224 928 96 800 32 672 0 544 0 512 0 480 32 352 96 224 224 96 352 32 480 0 512\\\r\n 0\r\n@TILELINE 1 8 0 512 32 512 160 480 288 416 416 288 480 160 512 32 512 0\r\n@TILELINE 2 8 0 512 32 512 160 544 288 608 416 736 480 864 512 992 512 1024\r\n@TILELINE 3 8 1024 512 992 512 864 480 736 416 608 288 544 160 512 32 512 0\r\n@TILELINE 4 8 1024 512 992 512 864 544 736 608 608 736 544 864 512 992 512 1024\r\n";
  const char   pat10[] = "@VERSION 10000\r\n\r\n@SYSENV 2\r\n@TILE 8\r\n@TILELINE 0 2 512 0 512 1024\r\n@TILELINE 1 2 256 1024 256 0\r\n@TILELINE 2 2 768 1024 768 0\r\n@TILELINE 3 2 768 0 512 384\r\n@TILELINE 4 2 512 384 256 0\r\n@TILELINE 5 2 256 0 0 384\r\n@TILELINE 6 2 768 0 1024 384\r\n@TILELINE 7 4 1024 0 1024 1024 1024 1024 1024 1024\r\n";
  const char   pat11[] = "@VERSION 10000\r\n\r\n@SYSENV 1\r\n@TILE 7\r\n@TILELINE 0 5 128 128 896 128 896 896 128 896 128 128\r\n@TILELINE 1 2 128 320 896 320\r\n@TILELINE 2 2 128 512 896 512\r\n@TILELINE 3 2 128 704 896 704\r\n@TILELINE 4 2 320 128 320 896\r\n@TILELINE 5 2 512 128 512 896\r\n@TILELINE 6 2 704 128 704 896\r\n";
  const char   pat12[] = "@VERSION 10000\r\n\r\n@SYSENV 1\r\n@TILE 1\r\n@TILELINE 0 15 512 256 640 128 768 64 896 128 960 256 960 448 832 640 512 960 192 640 64 448 64 256 128 128 256 64\\\r\n 384 128 512 256\r\n";
  const char   pat13[] = "@VERSION 10000\r\n\r\n@SYSENV 1\r\n@TILE 2\r\n@TILELINE 0 15 512 256 640 128 768 64 896 128 960 256 960 448 832 640 512 960 192 640 64 448 64 256 128 128 256 64\\\r\n 384 128 512 256\r\n@TILELINE 1 13 512 448 640 320 768 256 832 320 832 384 704 576 512 768 320 576 192 384 192 320 256 256 384 320 512\\\r\n 448\r\n";
  const char   pat14[] = "@VERSION 10000\r\n\r\n@SYSENV 1\r\n@TILE 1\r\n@TILELINE 0 17 320 1024 192 832 64 576 0 256 64 128 192 0 320 0 448 192 512 320 576 192 704 0 832 0 960 128 1024 256\\\r\n 960 576 832 832 704 1024\r\n";
  const char   pat15[] = "@VERSION 10000\r\n\r\n@SYSENV 1\r\n@TILE 2\r\n@TILELINE 0 17 320 1024 192 832 64 576 0 256 64 128 192 0 320 0 448 192 512 320 576 192 704 0 832 0 960 128 1024 256\\\r\n 960 576 832 832 704 1024\r\n@TILELINE 1 13 512 512 640 320 768 256 832 320 832 512 704 768 512 1024 320 768 192 512 192 320 256 256 384 320 512\\\r\n 512\r\n";
  const char   pat16[] = "@VERSION 10000\r\n\r\n@SYSENV 2\r\n@TILE 1\r\n@TILELINE 0 25 512 0 544 0 736 64 864 160 960 288 1024 448 1024 512 1024 576 960 736 864 864 736 960 544 1024 512\\\r\n 1024 480 1024 288 960 160 864 64 736 0 576 0 512 0 448 64 288 160 160 288 64 480 0 512 0\r\n";
  const char   pat17[] = "@VERSION 10000\r\n\r\n@SYSENV 2\r\n@TILE 2\r\n@TILELINE 0 25 512 0 544 0 736 64 864 160 960 288 1024 448 1024 512 1024 576 960 736 864 864 736 960 544 1024 512\\\r\n 1024 480 1024 288 960 160 864 64 736 0 576 0 512 0 448 64 288 160 160 288 64 480 0 512 0\r\n@TILELINE 1 9 512 256 704 320 768 512 704 704 512 768 320 704 256 512 320 320 512 256\r\n";
  const char  stamp1[] = "@VERSION 20000\r\n\r\n@SYSENV 1\r\n@TILE 2\r\n@TILELINE 0 15 512 256 640 128 768 64 896 128 960 256 960 448 832 640 512 960 192 640 64 448 64 256 128 128 256 64\\\r\n 384 128 512 256\r\n@TILEAREA 1 1 322 406 1 181 255 66 241 72 229 79 215 85 203 92 189 98 177 105 163 111 151 118 137 124 127 132 121\\\r\n 145 114 157 108 171 101 183 95 197 88 209 82 223 75 235 69 249 65 255 65 281 65 307 65 333 65 359 65 385 65 411 65\\\r\n 437 65 448 73 459 80 470 88 481 95 493 103 504 110 515 118 526 125 538 133 549 140 560 148 571 155 583 163 594 170\\\r\n 605 178 616 185 628 196 643 209 656 222 669 235 682 248 695 261 708 274 721 287 734 300 747 313 760 326 773 339 786\\\r\n 352 799 365 812 378 825 391 838 404 851 417 864 430 877 443 890 456 903 469 916 482 929 495 942 508 955 513 958 526\\\r\n 945 539 932 552 919 565 906 578 893 591 880 604 867 617 854 630 841 643 828 656 815 669 802 682 789 695 776 708 763\\\r\n 721 750 734 737 747 724 760 711 773 698 786 685 799 672 812 659 825 646 834 636 841 624 849 613 856 602 864 591 871\\\r\n 579 879 568 886 557 894 546 901 534 909 523 916 512 924 501 931 489 939 478 946 467 954 456 959 443 959 417 959 391\\\r\n 959 365 959 339 959 313 959 287 959 261 957 252 950 240 944 226 937 214 931 200 924 188 918 174 911 162 905 148 898\\\r\n 136 895 128 881 122 869 115 855 109 843 102 829 96 817 89 803 83 791 76 777 70 769 65 755 71 743 78 729 84 717 91\\\r\n 703 97 691 104 677 110 665 117 651 123 636 134 623 147 610 160 597 173 584 186 571 199 558 212 545 225 532 238 519\\\r\n 251 513 257 503 249 490 236 477 223 464 210 451 197 438 184 425 171 412 158 399 145 386 132 375 124 361 118 349 111\\\r\n 335 105 323 98 309 92 297 85 283 79 271 72 257 65\r\n";
  const char  stamp2[] = "@VERSION 20000\r\n\r\n@SYSENV 2\r\n@TILE 2\r\n@TILELINE 0 38 512 32 416 192 320 320 288 352 224 416 128 512 96 576 96 640 128 704 160 736 224 800 256 800 352 800\\\r\n 416 768 480 704 448 800 407 864 320 896 160 960 864 960 704 896 640 864 576 800 544 704 608 768 672 800 768 800 800\\\r\n 800 864 736 896 704 928 640 928 576 896 512 800 416 736 352 704 320 608 192 512 32\r\n@TILEAREA 1 1 419 479 1 236 512 34 505 46 498 57 491 70 483 82 476 95 468 107 461 120 453 132 446 145 438 157 431\\\r\n 170 423 182 416 194 407 205 400 216 391 226 384 237 374 249 367 260 358 270 351 281 341 293 334 304 325 314 317 324\\\r\n 304 337 291 350 278 363 265 376 252 389 239 402 226 415 213 428 200 441 187 454 174 467 161 480 148 493 135 506 127\\\r\n 516 121 529 114 541 108 555 101 567 97 581 97 607 97 633 98 643 105 655 111 669 118 681 124 695 135 710 148 723 161\\\r\n 736 174 749 187 762 200 775 213 788 223 798 240 799 266 799 292 799 318 799 344 799 354 798 366 791 380 785 392 778\\\r\n 406 772 421 761 434 748 447 735 460 722 473 709 479 703 479 709 473 729 466 748 460 768 453 787 449 801 441 812 434\\\r\n 824 426 836 419 848 411 859 407 865 388 872 369 879 354 884 341 889 327 894 314 900 299 905 287 911 269 917 257 923\\\r\n 239 929 227 935 209 941 197 947 179 953 167 959 164 959 190 959 216 959 242 959 268 959 294 959 320 959 346 959 372\\\r\n 959 398 959 424 959 450 959 476 959 502 959 528 959 554 959 580 959 606 959 632 959 658 959 684 959 710 959 736 959\\\r\n 762 959 788 959 814 959 840 959 858 959 854 957 837 951 825 945 807 939 795 933 777 927 765 921 747 915 735 909 717\\\r\n 903 705 897 691 891 679 884 665 878 653 871 640 865 627 852 614 839 601 826 588 813 576 802 569 783 563 763 556 744\\\r\n 550 724 543 706 543 703 553 711 566 724 579 737 592 750 605 763 616 771 630 777 642 784 656 790 668 797 684 799 710\\\r\n 799 736 799 762 799 788 799 799 799 812 786 825 773 838 760 851 747 864 734 877 721 890 708 898 698 904 684 911 672\\\r\n 917 658 924 646 927 631 927 605 927 579 922 566 915 554 909 540 902 528 896 514 883 501 870 488 857 475 844 462 831\\\r\n 449 818 436 805 423 792 410 779 397 766 384 753 371 740 358 727 345 714 332 702 320 695 309 686 299 679 288 669 276\\\r\n 662 265 653 255 646 244 636 232 629 221 620 211 613 200 605 190 599 178 591 167 585 155 577 143 570 130 562 118 555\\\r\n 105 547 93 540 80 532 68 525 55 517 43 512 35\r\n";
  const char  stamp3[] = "@VERSION 20000\r\n\r\n@SYSENV 1\r\n@TILE 2\r\n@TILELINE 0 5 512 64 192 512 512 960 832 512 512 64\r\n@TILEAREA 1 1 369 439 1 173 512 66 504 76 497 87 489 97 482 108 474 118 467 129 459 139 452 150 444 160 437 171 429\\\r\n 181 422 192 414 202 407 213 399 223 392 234 384 244 377 255 369 265 362 276 354 286 347 297 339 307 332 318 324 328\\\r\n 317 339 309 349 302 360 294 370 287 381 279 391 272 402 264 412 257 423 249 433 242 444 234 454 227 465 219 475 212\\\r\n 486 204 496 197 507 194 512 201 523 209 533 216 544 224 554 231 565 239 575 246 586 254 596 261 607 269 617 276 628\\\r\n 284 638 291 649 299 659 306 670 314 680 321 691 329 701 336 712 344 722 351 733 359 743 366 754 374 764 381 775 389\\\r\n 785 396 796 404 806 411 817 419 827 426 838 434 848 441 859 449 869 456 880 464 890 471 901 479 911 486 922 494 932\\\r\n 501 943 509 953 512 959 519 948 527 938 534 927 542 917 549 906 557 896 564 885 572 875 579 864 587 854 594 843 602\\\r\n 833 609 822 617 812 624 801 632 791 639 780 647 770 654 759 662 749 669 738 677 728 684 717 692 707 699 696 707 686\\\r\n 714 675 722 665 729 654 737 644 744 633 752 623 759 612 767 602 774 591 782 581 789 570 797 560 804 549 812 539 819\\\r\n 528 827 518 831 512 823 502 816 491 808 481 801 470 793 460 786 449 778 439 771 428 763 418 756 407 748 397 741 386\\\r\n 733 376 726 365 718 355 711 344 703 334 696 323 688 313 681 302 673 292 666 281 658 271 651 260 643 250 636 239 628\\\r\n 229 621 218 613 208 606 197 598 187 591 176 583 166 576 155 568 145 561 134 553 124 546 113 538 103 531 92 523 82\\\r\n 516 71 512 67\r\n";
  const char  stamp4[] = "@VERSION 20000\r\n\r\n@SYSENV 2\r\n@TILE 2\r\n@TILELINE 0 62 512 32 448 32 384 64 320 96 256 192 256 256 288 320 320 384 384 448 288 384 224 384 192 384 128 416\\\r\n 96 448 64 512 64 576 64 640 96 704 128 736 160 768 192 800 256 832 384 800 416 768 448 736 480 704 480 736 448 800\\\r\n 384 864 320 896 160 960 864 960 704 896 640 864 576 800 544 736 544 704 576 736 608 768 640 800 768 832 832 800 864\\\r\n 768 896 736 928 704 960 640 960 576 960 512 928 448 896 416 832 384 800 384 736 384 640 448 704 384 736 320 768 256\\\r\n 768 192 704 96 640 64 576 32 512 32\r\n@TILEAREA 1 1 372 542 1 293 447 34 433 40 421 47 407 53 395 60 381 66 369 73 355 79 343 86 329 92 318 100 311 112\\\r\n 303 123 296 134 288 145 281 157 273 168 266 179 258 190 257 207 257 233 257 254 262 267 269 279 275 293 282 305 288\\\r\n 319 295 331 301 345 308 357 314 371 324 387 337 400 350 413 363 426 376 439 385 447 380 446 368 439 357 431 346 424\\\r\n 335 416 323 409 312 401 301 394 290 386 273 385 247 385 221 385 195 385 187 387 175 394 161 400 149 407 135 413 121\\\r\n 425 108 438 97 448 91 461 84 473 78 487 71 499 65 511 65 537 65 563 65 589 65 615 65 638 70 651 77 663 83 677 90\\\r\n 689 96 702 109 715 122 728 135 741 148 754 161 767 174 780 187 793 197 801 210 807 222 814 236 820 248 827 261 830\\\r\n 277 825 292 822 309 817 324 814 341 809 356 806 373 801 383 799 396 786 409 773 422 760 435 747 448 734 461 721 474\\\r\n 708 479 703 481 711 481 734 476 747 469 759 463 773 456 785 450 799 437 812 424 825 411 838 398 851 386 864 373 870\\\r\n 361 877 347 883 335 890 321 896 309 902 294 907 282 913 264 919 252 925 234 931 222 937 204 943 192 949 174 955 166\\\r\n 959 172 959 198 959 224 959 250 959 276 959 302 959 328 959 354 959 380 959 406 959 432 959 458 959 484 959 510 959\\\r\n 536 959 562 959 588 959 614 959 640 959 666 959 692 959 718 959 744 959 770 959 796 959 822 959 848 959 858 959 854\\\r\n 957 837 951 825 945 807 939 795 933 777 927 765 921 747 915 735 909 717 903 705 897 691 891 679 884 665 878 653 871\\\r\n 640 865 627 852 614 839 601 826 588 813 575 801 569 788 562 776 556 762 549 750 543 738 543 712 543 704 552 711 565\\\r\n 724 578 737 591 750 604 763 617 776 630 789 639 798 655 803 670 806 687 811 702 814 719 819 734 822 751 827 766 830\\\r\n 778 826 790 819 804 813 816 806 830 800 843 787 856 774 869 761 882 748 895 735 908 722 921 709 929 699 935 686 942\\\r\n 674 948 660 955 648 959 634 959 608 959 582 959 556 959 530 959 512 952 500 946 486 939 474 933 460 923 444 910 431\\\r\n 897 418 885 411 871 405 859 398 845 392 833 386 816 385 790 385 764 385 739 385 727 391 716 399 705 406 694 414 682\\\r\n 421 671 429 660 436 649 444 641 449 642 444 655 431 668 418 681 405 694 392 703 383 709 370 716 358 722 344 729 332\\\r\n 735 318 742 306 748 292 755 280 761 266 767 256 767 230 767 204 767 192 759 181 752 170 744 159 737 147 729 136 722\\\r\n 125 714 114 707 102 697 93 683 87 671 80 657 74 645 67 631 61 619 54 605 48 593 41 579 35 571 33 545 33 519 33 493\\\r\n 33 467 33 449 33\r\n";
  const char  stamp5[] = "@VERSION 20000\r\n\r\n@SYSENV 2\r\n@TILE 2\r\n@TILELINE 0 11 512 0 384 384 0 384 320 640 192 1024 512 768 832 1024 704 640 1024 384 640 384 512 0\r\n@TILEAREA 1 1 445 524 1 229 512 3 506 23 499 42 493 62 486 81 480 101 473 120 467 140 460 159 454 179 447 198 441\\\r\n 218 434 237 428 257 421 276 415 296 408 315 402 335 395 354 389 374 385 384 368 385 342 385 316 385 290 385 264 385\\\r\n 238 385 212 385 186 385 160 385 134 385 108 385 82 385 56 385 30 385 5 385 2 385 13 393 26 404 37 412 47 421 58 429\\\r\n 71 440 82 448 92 457 103 465 116 476 127 484 137 493 148 501 161 512 172 520 182 529 193 537 206 548 217 556 227\\\r\n 565 238 573 251 584 262 592 272 601 283 609 296 620 307 628 317 637 321 639 315 659 308 678 302 698 295 717 289 737\\\r\n 282 756 276 776 269 795 263 815 256 834 250 854 243 873 237 893 230 912 224 932 217 951 211 971 204 990 198 1010\\\r\n 195 1020 198 1017 209 1009 222 998 233 990 243 981 254 973 267 962 278 954 288 945 299 937 312 926 323 918 333 909\\\r\n 344 901 357 890 368 882 378 873 389 865 402 854 413 846 423 837 434 829 447 818 458 810 468 801 479 793 492 782 503\\\r\n 774 511 767 522 774 533 784 544 792 554 801 565 809 578 820 589 828 599 837 610 845 623 856 634 864 644 873 655 881\\\r\n 668 892 679 900 689 909 700 917 713 928 724 936 734 945 745 953 758 964 769 972 779 981 790 989 803 1000 814 1008\\\r\n 824 1017 829 1021 826 1008 819 989 813 969 806 950 800 930 793 911 787 891 780 872 774 852 767 833 761 813 754 794\\\r\n 748 774 741 755 735 735 728 716 722 696 715 677 709 657 703 642 709 634 720 626 730 617 741 609 754 598 765 590 775\\\r\n 581 786 573 799 562 810 554 820 545 831 537 844 526 855 518 865 509 876 501 889 490 900 482 910 473 921 465 934 454\\\r\n 945 446 955 437 966 429 979 418 990 410 1000 401 1011 393 1020 385 1014 385 988 385 962 385 936 385 910 385 884 385\\\r\n 858 385 832 385 806 385 780 385 754 385 728 385 702 385 676 385 650 385 640 385 635 372 628 353 622 333 615 314 609\\\r\n 294 602 275 596 255 589 236 583 216 576 197 570 177 563 158 557 138 550 119 544 99 537 80 531 60 524 41 518 21 512\\\r\n 4\r\n";
  const char  stamp6[] = "@VERSION 20000\r\n\r\n@SYSENV 2\r\n@TILE 2\r\n@TILELINE 0 25 512 0 544 0 736 64 864 160 960 288 1024 448 1024 512 1024 576 960 736 864 864 736 960 544 1024 512\\\r\n 1024 480 1024 288 960 160 864 64 736 0 576 0 512 0 448 64 288 160 160 288 64 480 0 512 0\r\n@TILEAREA 1 1 266 472 1 196 479 2 459 8 440 15 420 21 401 28 381 34 362 41 342 47 323 54 303 60 288 65 277 74 266\\\r\n 81 256 90 245 97 233 107 222 114 212 123 201 130 189 140 178 147 168 156 160 161 153 172 144 182 137 193 127 205\\\r\n 120 216 111 226 104 237 94 249 87 260 78 270 71 281 64 292 59 305 53 317 47 335 41 347 35 365 29 377 23 395 17 407\\\r\n 11 425 5 437 1 451 1 477 1 503 1 529 1 555 1 574 7 592 13 604 19 622 25 634 31 652 37 664 43 682 49 694 55 712 61\\\r\n 724 66 737 74 747 81 758 90 768 97 779 107 791 114 802 123 812 130 823 140 835 147 846 156 856 160 863 171 870 181\\\r\n 879 192 886 204 896 215 903 225 912 236 919 248 929 259 936 269 945 280 952 290 960 310 966 329 973 349 979 368 986\\\r\n 388 992 407 999 427 1005 446 1012 466 1018 479 1023 505 1023 531 1023 548 1022 567 1015 587 1009 606 1002 626 996\\\r\n 645 989 665 983 684 976 704 970 723 963 737 958 748 949 759 942 769 933 780 926 792 916 803 909 813 900 824 893 836\\\r\n 883 847 876 857 867 864 863 871 852 881 840 888 829 897 819 904 808 914 796 921 785 930 775 937 764 947 752 954 741\\\r\n 961 730 967 714 973 702 979 684 985 672 991 654 997 642 1003 624 1009 612 1015 594 1021 582 1023 566 1023 540 1023\\\r\n 514 1023 488 1023 462 1023 447 1017 435 1011 417 1005 405 999 387 993 375 987 357 981 345 975 327 969 315 963 297\\\r\n 955 284 948 273 939 263 932 252 922 240 915 229 906 219 899 208 889 196 882 185 873 175 866 164 855 154 844 147 834\\\r\n 138 823 131 811 121 800 114 790 105 779 98 767 88 756 81 746 72 729 63 710 56 690 50 671 43 651 37 632 30 612 24\\\r\n 593 17 573 11 554 4 539 1 513 1 487 1 481 1\r\n";
  const char  stamp7[] = "@VERSION 20000\r\n\r\n@SYSENV 2\r\n@TILE 10\r\n@TILELINE 0 29 512 0 544 0 672 32 800 96 928 224 992 352 1024 480 1024 512 1024 544 992 672 928 800 800 928 672 992\\\r\n 544 1024 512 1024 480 1024 352 992 224 928 96 800 32 672 0 544 0 512 0 480 32 352 96 224 224 96 352 32 480 0 512\\\r\n 0\r\n@TILELINE 1 8 0 512 32 512 160 480 288 416 416 288 480 160 512 32 512 0\r\n@TILELINE 2 8 0 512 32 512 160 544 288 608 416 736 480 864 512 992 512 1024\r\n@TILELINE 3 8 1024 512 992 512 864 480 736 416 608 288 544 160 512 32 512 0\r\n@TILELINE 4 8 1024 512 992 512 864 544 736 608 608 736 544 864 512 992 512 1024\r\n@TILEAREA 5 1 500 487 1 188 512 36 508 50 505 65 500 82 497 97 492 114 489 129 484 146 481 161 474 173 468 187 461\\\r\n 199 455 213 448 225 442 239 435 251 429 265 422 277 416 289 403 302 390 315 377 328 364 341 351 354 338 367 325 380\\\r\n 312 393 299 406 290 416 277 422 265 429 251 435 239 442 225 448 213 455 199 461 187 468 173 474 161 480 146 485 131\\\r\n 488 114 493 99 496 82 501 67 504 50 509 37 512 42 513 59 518 74 521 91 526 106 529 123 534 138 537 155 542 164 544\\\r\n 176 551 190 557 202 564 216 570 228 577 242 583 254 590 268 596 280 603 291 610 304 623 317 636 330 649 343 662 356\\\r\n 675 369 688 382 701 395 714 408 727 417 736 423 749 430 761 436 775 443 787 449 801 456 813 462 827 469 839 475 853\\\r\n 481 865 486 882 489 897 494 914 497 929 502 946 505 961 510 978 512 987 513 982 518 965 521 950 526 933 529 918 534\\\r\n 901 537 886 542 869 544 860 551 848 557 834 564 822 570 808 577 796 583 782 590 770 596 756 603 744 610 733 623 720\\\r\n 636 707 649 694 662 681 675 668 688 655 701 642 714 629 727 616 736 606 750 600 762 593 776 587 788 580 802 574 814\\\r\n 567 828 561 840 554 854 548 865 542 880 539 897 534 912 531 929 526 944 523 961 518 976 515 987 512 982 511 966 506\\\r\n 951 503 934 498 919 495 902 490 887 487 870 482 861 480 849 473 835 467 823 460 809 454 797 447 783 441 771 434 757\\\r\n 428 745 421 733 415 720 402 707 389 694 376 681 363 668 350 655 337 642 324 629 311 616 298 607 289 601 276 594 264\\\r\n 588 250 581 238 575 224 568 212 562 198 555 186 549 172 543 160 538 143 535 128 530 111 527 96 522 79 519 64 514\\\r\n 47 512 37\r\n@TILEAREA 6 0 207 333 1 101 478 2 463 5 446 10 431 13 414 18 399 21 382 26 367 29 351 33 339 40 325 46 313 53 299\\\r\n 59 287 66 273 72 261 79 247 85 235 92 223 98 210 111 197 124 184 137 171 150 158 163 145 176 132 189 119 202 106\\\r\n 215 97 224 91 237 84 249 78 263 71 275 65 289 58 301 52 315 45 327 39 341 33 353 28 370 25 385 20 402 17 417 12 434\\\r\n 9 449 4 466 1 489 1 508 1 511 27 511 42 508 57 504 72 501 89 496 104 493 121 488 136 485 153 480 162 478 174 471\\\r\n 188 465 200 458 214 452 226 445 240 439 252 432 266 426 278 419 290 413 303 400 316 387 329 374 342 361 355 348 368\\\r\n 335 381 322 394 309 407 296 416 286 422 272 429 260 435 246 442 234 448 220 455 208 461 194 468 182 474 168 480 157\\\r\n 483 142 488 125 491 110 496 93 499 78 504 61 507 46 511 32 511 6 511 2 494 1 480 1\r\n@TILEAREA 7 0 712 288 1 100 513 2 513 28 516 43 520 58 523 73 528 90 531 105 536 122 539 137 544 154 546 163 553 175\\\r\n 559 189 566 201 572 215 579 227 585 241 592 253 598 267 605 279 612 290 625 303 638 316 651 329 664 342 677 355 690\\\r\n 368 703 381 716 394 729 407 738 416 752 422 764 429 778 435 790 442 804 448 816 455 830 461 842 468 856 474 867 480\\\r\n 882 483 899 488 914 491 931 496 946 499 963 504 978 507 992 511 1018 511 1022 511 1023 494 1022 477 1018 463 1015\\\r\n 448 1010 431 1007 416 1002 399 999 384 994 367 991 352 984 340 978 326 971 314 965 300 958 288 952 274 945 262 939\\\r\n 248 932 236 926 224 913 211 900 198 887 185 874 172 861 159 848 146 835 133 822 120 809 107 800 97 787 91 775 84\\\r\n 761 78 749 71 735 65 723 58 709 52 697 45 683 39 671 33 654 28 639 25 622 20 607 17 590 12 575 9 558 4 535 1 515\\\r\n 1\r\n@TILEAREA 8 0 759 681 1 101 990 514 975 517 958 522 943 525 926 530 911 533 894 538 879 541 863 545 851 552 837 558\\\r\n 825 565 811 571 799 578 785 584 773 591 759 597 747 604 735 610 722 623 709 636 696 649 683 662 670 675 657 688 644\\\r\n 701 631 714 618 727 609 736 603 749 596 761 590 775 583 787 577 801 570 813 564 827 557 839 551 853 545 865 540 882\\\r\n 537 897 532 914 529 929 524 946 521 961 516 978 513 1001 513 1020 513 1023 539 1023 554 1020 569 1016 584 1013 601\\\r\n 1008 616 1005 633 1000 648 997 665 992 674 990 686 983 700 977 712 970 726 964 738 957 752 951 764 944 778 938 790\\\r\n 931 802 925 815 912 828 899 841 886 854 873 867 860 880 847 893 834 906 821 919 808 928 798 934 784 941 772 947 758\\\r\n 954 746 960 732 967 720 973 706 980 694 986 680 992 669 995 654 1000 637 1003 622 1008 605 1011 590 1016 573 1019\\\r\n 558 1023 544 1023 518 1023 514 1006 513 992 513\r\n@TILEAREA 9 0 247 688 1 100 1 514 1 540 4 555 8 570 11 585 16 602 19 617 24 634 27 649 32 666 34 675 41 687 47 701\\\r\n 54 713 60 727 67 739 73 753 80 765 86 779 93 791 100 802 113 815 126 828 139 841 152 854 165 867 178 880 191 893\\\r\n 204 906 217 919 226 928 240 934 252 941 266 947 278 954 292 960 304 967 318 973 330 980 344 986 355 992 370 995 387\\\r\n 1000 402 1003 419 1008 434 1011 451 1016 466 1019 480 1023 506 1023 510 1023 511 1006 510 989 506 975 503 960 498\\\r\n 943 495 928 490 911 487 896 482 879 479 864 472 852 466 838 459 826 453 812 446 800 440 786 433 774 427 760 420 748\\\r\n 414 736 401 723 388 710 375 697 362 684 349 671 336 658 323 645 310 632 297 619 288 609 275 603 263 596 249 590 237\\\r\n 583 223 577 211 570 197 564 185 557 171 551 159 545 142 540 127 537 110 532 95 529 78 524 63 521 46 516 23 513 3\\\r\n 513\r\n";
  const char  stampA[] = "@VERSION 20000\r\n\r\n@SYSENV 2\r\n@TILE 3\r\n@TILELINE 0 9 416 32 640 32 960 992 736 992 672 800 352 800 288 992 64 992 416 32\r\n@TILELINE 1 5 512 224 384 640 640 640 608 544 512 224\r\n@TILEAREA 2 0 435 341 2 178 417 34 410 53 405 67 400 80 394 97 389 110 383 127 378 140 372 157 367 170 361 187 356\\\r\n 200 350 217 345 230 339 247 334 260 328 277 323 290 317 307 312 320 306 337 301 350 295 367 290 380 284 397 279 410\\\r\n 273 427 268 440 262 457 257 470 251 487 246 500 240 517 235 530 229 547 224 560 218 577 213 590 207 607 202 620 196\\\r\n 637 191 650 185 667 180 680 174 697 169 710 163 727 158 740 152 757 147 770 141 787 136 800 130 817 125 830 119 847\\\r\n 114 860 108 877 103 890 97 907 92 920 86 937 81 950 75 967 70 980 66 990 73 991 99 991 125 991 151 991 177 991 203\\\r\n 991 229 991 255 991 281 991 287 991 294 972 300 952 307 933 313 913 320 894 326 874 333 855 339 835 346 816 350 801\\\r\n 358 799 384 799 410 799 436 799 462 799 488 799 514 799 540 799 566 799 592 799 618 799 644 799 670 799 675 804 681\\\r\n 824 688 843 694 863 701 882 707 902 714 921 720 941 727 960 733 980 736 990 753 991 779 991 805 991 831 991 857 991\\\r\n 883 991 909 991 935 991 956 991 959 991 952 972 946 952 939 933 933 913 926 894 920 874 913 855 907 835 900 816 894\\\r\n 796 887 777 881 757 874 738 868 718 861 699 855 679 848 660 842 640 835 621 829 601 822 582 816 562 809 543 803 523\\\r\n 796 504 790 484 783 465 777 445 770 426 764 406 757 387 751 367 744 348 738 328 731 309 725 289 718 270 712 250 705\\\r\n 231 699 211 692 192 686 172 679 153 673 133 666 114 660 94 653 75 647 55 640 36 639 33 613 33 587 33 561 33 535 33\\\r\n 509 33 483 33 457 33 431 33 419 33 59 513 225 519 245 525 265 531 285 537 305 543 325 549 345 555 365 561 385 567\\\r\n 405 573 425 579 445 585 465 591 485 597 505 603 525 609 545 616 564 622 584 629 603 635 623 641 638 641 641 615 641\\\r\n 589 641 563 641 537 641 511 641 485 641 459 641 433 641 407 641 386 641 383 641 389 622 393 608 399 588 405 570 409\\\r\n 556 415 536 421 518 425 504 431 484 437 466 441 452 447 432 453 414 457 400 463 380 469 362 473 348 479 328 485 310\\\r\n 489 296 495 276 501 258 505 244 510 225 512 223\r\n";
  const char  stampB[] = "@VERSION 20000\r\n\r\n@SYSENV 2\r\n@TILE 4\r\n@TILELINE 0 21 128 32 128 992 604 991 717 964 800 918 864 843 895 751 891 660 871 592 825 522 776 483 728 465 785\\\r\n 427 829 370 862 318 862 223 836 148 770 87 684 43 587 32 128 32\r\n@TILELINE 1 10 323 187 323 417 569 417 637 387 676 340 677 270 651 215 598 187 514 187 323 187\r\n@TILELINE 2 10 320 557 320 835 595 835 662 802 697 738 707 662 673 602 607 556 503 556 320 557\r\n@TILEAREA 3 0 206 368 3 169 129 34 129 60 129 86 129 112 129 138 129 164 129 190 129 216 129 242 129 268 129 294 129\\\r\n 320 129 346 129 372 129 398 129 424 129 450 129 476 129 502 129 528 129 554 129 580 129 606 129 632 129 658 129 684\\\r\n 129 710 129 736 129 762 129 788 129 814 129 840 129 866 129 892 129 918 129 944 129 970 129 988 129 991 155 991 181\\\r\n 991 207 991 233 991 259 991 285 991 311 991 337 991 362 991 379 990 405 990 431 990 457 990 483 990 509 990 535 990\\\r\n 561 990 587 990 605 990 622 985 637 982 652 978 667 975 681 971 696 968 710 964 719 962 730 955 742 949 753 942 766\\\r\n 936 777 929 789 923 801 916 809 906 818 896 826 886 836 875 844 865 853 855 861 845 866 832 873 813 878 796 883 783\\\r\n 890 764 894 750 893 725 892 700 891 675 889 659 885 645 879 625 875 611 871 594 863 583 856 571 848 560 841 548 833\\\r\n 537 826 525 814 514 803 506 790 495 779 487 767 481 750 475 737 470 728 466 731 461 743 454 754 446 765 439 776 431\\\r\n 785 426 792 415 802 404 809 393 819 382 826 371 834 361 841 349 849 337 856 325 861 318 861 292 861 266 861 240 861\\\r\n 222 854 203 849 187 844 174 837 155 831 144 821 136 808 123 794 111 781 98 770 88 757 82 745 75 731 69 720 62 706\\\r\n 56 694 49 679 43 662 42 644 39 627 38 609 35 592 34 575 33 549 33 523 33 497 33 471 33 445 33 419 33 393 33 367 33\\\r\n 341 33 315 33 289 33 263 33 237 33 211 33 185 33 159 33 133 33 131 33 55 600 187 613 193 624 200 637 206 649 213\\\r\n 654 218 661 235 668 247 673 260 678 268 678 294 677 319 677 338 670 349 661 359 653 369 641 383 636 389 620 395 608\\\r\n 401 593 407 581 413 568 418 542 418 516 418 490 418 464 418 438 418 412 418 386 418 360 418 334 418 324 418 322 410\\\r\n 322 384 322 358 322 332 322 306 322 280 322 254 322 228 322 202 322 188 330 186 356 186 382 186 408 186 434 186 460\\\r\n 186 486 186 512 186 538 186 564 186 590 186 598 186 64 609 556 620 564 631 571 641 579 652 586 663 594 674 601 680\\\r\n 613 687 624 693 636 700 647 706 659 707 666 705 686 702 704 700 724 697 741 690 752 684 765 677 776 671 789 664 800\\\r\n 659 805 645 811 633 818 619 824 607 831 594 836 568 836 542 836 516 836 490 836 464 836 438 836 412 836 386 836 360\\\r\n 836 334 836 321 836 319 828 319 802 319 776 319 750 319 724 319 698 319 672 319 646 319 620 319 594 319 568 319 558\\\r\n 327 556 353 556 379 556 405 556 430 555 456 555 482 555 508 555 534 555 560 555 586 555 605 555 607 555\r\n";
  const char  stampC[] = "@VERSION 20000\r\n\r\n@SYSENV 2\r\n@TILE 2\r\n@TILELINE 0 36 928 320 928 256 864 160 768 64 608 32 448 32 288 128 192 224 128 416 128 608 192 800 288 896 448 992\\\r\n 608 992 736 960 832 864 928 736 928 672 736 672 736 736 672 800 608 832 512 832 448 800 384 736 320 640 288 544 288\\\r\n 480 320 352 384 256 480 192 576 192 640 192 736 256 736 320 928 320\r\n@TILEAREA 1 0 215 350 1 248 448 34 436 40 424 48 411 55 399 63 386 70 374 78 361 85 349 93 336 100 324 108 311 115\\\r\n 299 123 288 130 275 143 262 156 249 169 236 182 223 195 210 208 197 221 190 232 184 252 177 271 171 291 164 310 158\\\r\n 330 151 349 145 369 138 388 132 408 129 423 129 449 129 475 129 501 129 527 129 553 129 579 129 605 133 619 139 639\\\r\n 146 658 152 678 159 697 165 717 172 736 178 756 185 775 191 795 199 805 212 818 225 831 238 844 251 857 264 870 277\\\r\n 883 288 895 301 902 313 910 326 917 338 925 351 932 363 940 376 947 388 955 401 962 413 970 426 977 438 985 450 991\\\r\n 476 991 502 991 528 991 554 991 580 991 606 991 621 987 636 984 653 979 668 976 685 971 700 968 717 963 732 960 742\\\r\n 952 755 939 768 926 781 913 794 900 807 887 820 874 831 864 838 853 847 843 854 832 864 820 871 809 880 799 887 788\\\r\n 897 776 904 765 913 755 920 744 927 736 927 710 927 684 927 674 910 673 884 673 858 673 832 673 806 673 780 673 754\\\r\n 673 739 673 737 681 737 707 737 733 734 740 721 753 708 766 695 779 682 792 673 801 659 807 647 814 633 820 621 827\\\r\n 609 833 583 833 557 833 531 833 513 833 499 827 487 820 473 814 461 807 448 801 435 788 422 775 409 762 396 749 383\\\r\n 737 376 726 368 715 361 703 353 692 346 681 338 670 331 658 323 647 315 629 309 609 302 590 296 570 289 551 287 535\\\r\n 287 509 287 483 290 468 294 453 297 438 302 421 305 406 310 389 313 374 318 357 320 349 328 339 335 327 343 316 350\\\r\n 305 358 294 365 282 373 271 380 260 389 251 400 244 411 236 423 229 434 221 445 214 456 206 468 199 479 192 496 191\\\r\n 522 191 548 191 574 191 600 191 626 191 641 191 652 199 663 206 674 214 686 221 697 229 708 236 719 244 731 251 737\\\r\n 255 737 281 737 307 737 317 745 319 771 319 797 319 823 319 849 319 875 319 901 319 924 319 927 319 927 293 927 267\\\r\n 927 256 919 245 912 234 904 223 897 211 889 200 882 189 874 178 867 166 855 152 842 139 829 126 816 113 803 100 790\\\r\n 87 777 74 768 65 750 61 733 58 715 54 698 51 680 47 663 44 645 40 628 37 610 33 584 33 558 33 532 33 506 33 480 33\\\r\n 454 33 450 33\r\n";
  const char  stampD[] = "@VERSION 20000\r\n\r\n@SYSENV 2\r\n@TILE 3\r\n@TILELINE 0 17 160 32 160 992 608 992 704 960 800 896 864 800 896 704 928 608 928 480 928 416 896 320 864 256 800\\\r\n 160 736 96 640 64 544 32 160 32\r\n@TILELINE 1 12 352 224 352 832 576 832 640 800 704 736 736 640 736 480 704 352 640 288 608 256 512 224 352 224\r\n@TILEAREA 2 0 288 380 2 146 161 34 161 60 161 86 161 112 161 138 161 164 161 190 161 216 161 242 161 268 161 294 161\\\r\n 320 161 346 161 372 161 398 161 424 161 450 161 476 161 502 161 528 161 554 161 580 161 606 161 632 161 658 161 684\\\r\n 161 710 161 736 161 762 161 788 161 814 161 840 161 866 161 892 161 918 161 944 161 970 161 988 161 991 187 991 213\\\r\n 991 239 991 265 991 291 991 317 991 343 991 369 991 395 991 421 991 447 991 473 991 499 991 525 991 551 991 577 991\\\r\n 603 991 618 988 637 981 657 975 676 968 696 962 708 956 719 948 731 941 742 933 753 926 764 918 776 911 787 903 798\\\r\n 896 806 886 813 874 821 863 828 852 836 841 843 829 851 818 858 807 864 795 871 776 877 756 884 737 890 717 897 698\\\r\n 903 678 910 659 916 639 923 620 927 606 927 580 927 554 927 528 927 502 927 476 927 450 927 424 924 409 918 389 911\\\r\n 370 905 350 898 331 893 316 886 304 880 290 873 278 867 264 860 253 853 241 845 230 838 219 830 208 823 196 815 185\\\r\n 808 174 800 163 792 153 779 140 766 127 753 114 740 101 728 95 709 88 689 82 670 75 650 69 631 62 611 56 592 49 572\\\r\n 43 553 36 538 33 512 33 486 33 460 33 434 33 408 33 382 33 356 33 330 33 304 33 278 33 252 33 226 33 200 33 174 33\\\r\n 163 33 85 515 224 535 230 554 237 574 243 593 250 607 254 620 267 633 280 646 293 659 306 672 319 685 332 698 345\\\r\n 706 354 709 369 714 386 717 401 722 418 725 433 730 450 733 465 737 479 737 505 737 531 737 557 737 583 737 609 737\\\r\n 635 734 650 727 669 721 689 714 708 708 728 702 740 689 753 676 766 663 779 650 792 641 801 627 807 615 814 601 820\\\r\n 589 827 577 833 551 833 525 833 499 833 473 833 447 833 421 833 395 833 369 833 353 833 351 825 351 799 351 773 351\\\r\n 747 351 721 351 695 351 669 351 643 351 617 351 591 351 565 351 539 351 513 351 487 351 461 351 435 351 409 351 383\\\r\n 351 357 351 331 351 305 351 279 351 253 351 227 351 224 368 223 394 223 420 223 446 223 472 223 498 223 513 223\r\n";
  const char  stampE[] = "@VERSION 20000\r\n\r\n@SYSENV 2\r\n@TILE 2\r\n@TILELINE 0 13 160 32 160 992 832 992 832 832 352 832 352 576 736 576 736 416 352 416 352 192 832 192 832 32 160 32\r\n@TILEAREA 1 0 256 494 1 204 161 34 161 60 161 86 161 112 161 138 161 164 161 190 161 216 161 242 161 268 161 294 161\\\r\n 320 161 346 161 372 161 398 161 424 161 450 161 476 161 502 161 528 161 554 161 580 161 606 161 632 161 658 161 684\\\r\n 161 710 161 736 161 762 161 788 161 814 161 840 161 866 161 892 161 918 161 944 161 970 161 988 161 991 187 991 213\\\r\n 991 239 991 265 991 291 991 317 991 343 991 369 991 395 991 421 991 447 991 473 991 499 991 525 991 551 991 577 991\\\r\n 603 991 629 991 655 991 681 991 707 991 733 991 759 991 785 991 811 991 829 991 831 983 831 957 831 931 831 905 831\\\r\n 879 831 853 831 835 823 833 797 833 771 833 745 833 719 833 693 833 667 833 641 833 615 833 589 833 563 833 537 833\\\r\n 511 833 485 833 459 833 433 833 407 833 381 833 355 833 352 833 351 816 351 790 351 764 351 738 351 712 351 686 351\\\r\n 660 351 634 351 608 351 582 351 576 368 575 394 575 420 575 446 575 472 575 498 575 524 575 550 575 576 575 602 575\\\r\n 628 575 654 575 680 575 706 575 732 575 735 575 735 549 735 523 735 497 735 471 735 445 735 420 735 417 709 417 683\\\r\n 417 657 417 631 417 605 417 579 417 553 417 527 417 501 417 475 417 449 417 423 417 397 417 371 417 353 417 351 409\\\r\n 351 383 351 357 351 331 351 305 351 279 351 253 351 227 351 201 351 192 368 191 394 191 420 191 446 191 472 191 498\\\r\n 191 524 191 550 191 576 191 602 191 628 191 654 191 680 191 706 191 732 191 758 191 784 191 810 191 828 191 831 191\\\r\n 831 165 831 139 831 113 831 87 831 61 831 36 831 33 805 33 779 33 753 33 727 33 701 33 675 33 649 33 623 33 597 33\\\r\n 571 33 545 33 519 33 493 33 467 33 441 33 415 33 389 33 363 33 337 33 311 33 285 33 259 33 233 33 207 33 181 33 163\\\r\n 33\r\n";
  const char  stampF[] = "@VERSION 20000\r\n\r\n@SYSENV 2\r\n@TILE 2\r\n@TILELINE 0 11 192 32 192 992 384 992 384 608 768 608 768 448 384 448 384 224 832 224 832 32 192 32\r\n@TILEAREA 1 0 256 494 1 163 193 34 193 60 193 86 193 112 193 138 193 164 193 190 193 216 193 242 193 268 193 294 193\\\r\n 320 193 346 193 372 193 398 193 424 193 450 193 476 193 502 193 528 193 554 193 580 193 606 193 632 193 658 193 684\\\r\n 193 710 193 736 193 762 193 788 193 814 193 840 193 866 193 892 193 918 193 944 193 970 193 988 193 991 219 991 245\\\r\n 991 271 991 297 991 323 991 349 991 375 991 382 991 383 974 383 948 383 922 383 896 383 870 383 844 383 818 383 792\\\r\n 383 766 383 740 383 714 383 688 383 662 383 636 383 610 383 607 409 607 435 607 461 607 487 607 513 607 539 607 565\\\r\n 607 591 607 617 607 643 607 669 607 695 607 721 607 747 607 765 607 767 599 767 573 767 547 767 521 767 495 767 469\\\r\n 767 451 759 449 733 449 707 449 681 449 655 449 629 449 603 449 577 449 551 449 525 449 499 449 473 449 447 449 421\\\r\n 449 395 449 385 449 383 441 383 415 383 389 383 363 383 337 383 311 383 285 383 259 383 233 383 224 400 223 426 223\\\r\n 452 223 478 223 504 223 530 223 556 223 582 223 608 223 634 223 660 223 686 223 712 223 738 223 764 223 790 223 816\\\r\n 223 829 223 831 215 831 189 831 163 831 137 831 111 831 85 831 59 831 36 831 33 805 33 779 33 753 33 727 33 701 33\\\r\n 675 33 649 33 623 33 597 33 571 33 545 33 519 33 493 33 467 33 441 33 415 33 389 33 363 33 337 33 311 33 285 33 259\\\r\n 33 233 33 207 33 195 33\r\n";
  const char  stampG[] = "@VERSION 20000\r\n\r\n@SYSENV 3\r\n@TILE 2\r\n@TILELINE 0 47 914 327 870 215 799 114 691 50 597 30 518 21 412 38 288 100 206 179 144 277 112 371 103 494 103 596\\\r\n 144 730 206 853 350 956 452 986 536 997 618 983 693 927 762 865 762 936 915 936 915 483 556 483 556 627 762 627 733\\\r\n 697 671 780 597 833 527 842 444 833 383 791 309 688 280 597 268 483 277 391 308 323 350 261 433 194 494 174 577 174\\\r\n 647 203 700 247 721 297 730 327 914 327\r\n@TILEAREA 1 0 218 459 1 318 515 23 499 25 484 28 468 30 453 33 437 35 422 38 411 39 399 46 385 52 373 59 359 65 347\\\r\n 72 333 78 321 85 307 91 295 98 284 105 271 118 258 131 249 140 239 148 226 161 213 174 204 183 198 195 190 206 183\\\r\n 218 175 229 168 242 160 253 153 265 146 276 139 295 132 314 126 334 119 353 113 370 112 389 110 406 109 430 107 447\\\r\n 106 471 104 488 104 514 104 540 104 566 104 592 107 607 113 623 117 637 123 657 128 672 132 686 138 706 143 721 145\\\r\n 730 152 742 158 756 165 768 171 782 178 794 184 808 191 820 197 834 204 846 207 853 218 860 228 868 239 875 249 883\\\r\n 260 890 270 898 281 905 291 913 302 920 312 928 323 935 333 943 344 950 352 956 366 960 386 966 400 970 420 976 434\\\r\n 980 451 985 469 987 487 990 507 992 525 995 541 995 556 992 572 990 587 987 603 985 618 982 629 973 640 966 650 957\\\r\n 661 950 673 940 684 933 695 923 705 915 715 905 725 897 735 887 745 879 755 869 761 864 763 872 763 898 763 924 763\\\r\n 934 780 935 806 935 832 935 858 935 884 935 910 935 913 935 914 918 914 892 914 866 914 840 914 814 914 788 914 762\\\r\n 914 736 914 710 914 684 914 658 914 632 914 606 914 580 914 554 914 528 914 502 914 486 906 484 880 484 854 484 828\\\r\n 484 802 484 776 484 750 484 724 484 698 484 672 484 646 484 620 484 594 484 568 484 558 484 557 501 557 527 557 553\\\r\n 557 579 557 605 557 623 557 626 583 626 609 626 635 626 661 626 687 626 713 626 739 626 760 626 763 626 758 641 752\\\r\n 653 746 670 740 682 735 696 727 706 720 717 710 729 703 740 694 750 687 761 677 773 672 781 661 788 651 796 640 803\\\r\n 629 812 618 819 608 827 594 835 573 837 555 840 534 842 518 842 497 840 481 838 460 836 443 834 433 826 422 819 411\\\r\n 811 400 804 389 796 382 792 375 781 366 770 359 759 351 749 344 738 336 728 329 717 320 706 313 695 306 684 300 664\\\r\n 294 644 288 624 281 605 278 590 276 567 274 551 272 529 270 513 268 491 268 475 269 458 271 442 273 419 275 403 276\\\r\n 389 282 377 288 363 294 351 300 337 306 325 313 313 321 303 328 291 336 280 343 269 351 258 362 250 372 241 383 233\\\r\n 393 224 404 216 418 204 429 196 441 190 461 184 480 177 494 173 520 173 546 173 572 173 584 175 596 181 613 187 625\\\r\n 193 639 198 654 208 664 216 678 228 688 236 700 245 706 260 712 272 717 286 723 298 727 312 730 325 747 326 773 326\\\r\n 799 326 825 326 851 326 877 326 903 326 912 326 908 314 902 298 896 286 891 270 885 258 880 242 874 230 869 215 861\\\r\n 205 854 194 845 182 838 171 830 161 823 150 814 138 807 127 799 117 794 112 782 106 771 98 759 92 748 85 736 78 725\\\r\n 71 713 65 701 57 688 50 672 47 655 43 639 40 622 36 607 33 592 30 575 29 557 26 540 25 522 22 517 22\r\n";
  const char  stampH[] = "@VERSION 20000\r\n\r\n@SYSENV 2\r\n@TILE 2\r\n@TILELINE 0 13 128 32 128 992 320 992 320 576 736 576 736 992 928 992 928 32 736 32 736 416 320 416 320 32 128 32\r\n@TILEAREA 1 0 247 397 1 209 129 34 129 60 129 86 129 112 129 138 129 164 129 190 129 216 129 242 129 268 129 294 129\\\r\n 320 129 346 129 372 129 398 129 424 129 450 129 476 129 502 129 528 129 554 129 580 129 606 129 632 129 658 129 684\\\r\n 129 710 129 736 129 762 129 788 129 814 129 840 129 866 129 892 129 918 129 944 129 970 129 988 129 991 155 991 181\\\r\n 991 207 991 233 991 259 991 285 991 311 991 318 991 319 974 319 948 319 922 319 896 319 870 319 844 319 818 319 792\\\r\n 319 766 319 740 319 714 319 688 319 662 319 636 319 610 319 584 319 576 336 575 362 575 388 575 414 575 440 575 466\\\r\n 575 492 575 518 575 544 575 570 575 596 575 622 575 648 575 674 575 700 575 726 575 736 575 737 592 737 618 737 644\\\r\n 737 670 737 696 737 722 737 748 737 774 737 800 737 826 737 852 737 878 737 904 737 930 737 956 737 982 737 990 754\\\r\n 991 780 991 806 991 832 991 858 991 884 991 910 991 925 991 927 983 927 957 927 931 927 905 927 879 927 853 927 827\\\r\n 927 801 927 775 927 749 927 723 927 697 927 671 927 645 927 619 927 593 927 567 927 541 927 515 927 489 927 463 927\\\r\n 437 927 411 927 385 927 359 927 333 927 307 927 281 927 255 927 229 927 203 927 177 927 151 927 125 927 99 927 73\\\r\n 927 47 927 35 919 33 893 33 867 33 841 33 815 33 789 33 763 33 740 33 737 33 737 59 737 85 737 111 737 137 737 163\\\r\n 737 189 737 215 737 241 737 267 737 293 737 319 737 345 737 371 737 397 737 415 729 417 703 417 677 417 651 417 625\\\r\n 417 599 417 573 417 547 417 521 417 495 417 469 417 443 417 417 417 391 417 365 417 339 417 321 417 319 409 319 383\\\r\n 319 357 319 331 319 305 319 279 319 253 319 227 319 201 319 175 319 149 319 123 319 97 319 71 319 45 319 35 311 33\\\r\n 285 33 259 33 233 33 207 33 181 33 155 33 131 33\r\n";
  const char  stampI[] = "@VERSION 20000\r\n\r\n@SYSENV 2\r\n@TILE 2\r\n@TILELINE 0 5 416 32 416 992 608 992 608 32 416 32\r\n@TILEAREA 1 0 462 427 1 93 417 34 417 60 417 86 417 112 417 138 417 164 417 190 417 216 417 242 417 268 417 294 417\\\r\n 320 417 346 417 372 417 398 417 424 417 450 417 476 417 502 417 528 417 554 417 580 417 606 417 632 417 658 417 684\\\r\n 417 710 417 736 417 762 417 788 417 814 417 840 417 866 417 892 417 918 417 944 417 970 417 988 417 991 443 991 469\\\r\n 991 495 991 521 991 547 991 573 991 599 991 606 991 607 974 607 948 607 922 607 896 607 870 607 844 607 818 607 792\\\r\n 607 766 607 740 607 714 607 688 607 662 607 636 607 610 607 584 607 558 607 532 607 506 607 480 607 454 607 428 607\\\r\n 402 607 376 607 350 607 324 607 298 607 272 607 246 607 220 607 194 607 168 607 142 607 116 607 90 607 64 607 38\\\r\n 607 34 590 33 564 33 538 33 512 33 486 33 460 33 434 33 419 33\r\n";
  const char  stampJ[] = "@VERSION 20000\r\n\r\n@SYSENV 3\r\n@TILE 2\r\n@TILELINE 0 19 608 32 608 768 583 814 544 832 448 832 384 800 352 732 352 640 192 640 192 774 224 864 343 965 448\\\r\n 992 576 992 672 954 734 894 800 786 800 32 608 32\r\n@TILEAREA 1 0 680 838 1 157 609 34 609 60 609 86 609 112 609 138 609 164 609 190 609 216 609 242 609 268 609 294 609\\\r\n 320 609 346 609 372 609 398 609 424 609 450 609 476 609 502 609 528 609 554 609 580 609 606 609 632 609 658 609 684\\\r\n 609 710 609 736 609 762 608 771 601 782 595 795 588 806 585 814 569 821 557 827 545 833 519 833 493 833 467 833 449\\\r\n 833 435 827 423 820 409 814 397 807 385 801 378 790 372 776 366 764 359 748 352 736 351 719 351 693 351 667 351 644\\\r\n 351 641 325 641 299 641 273 641 247 641 221 641 196 641 193 641 193 667 193 693 193 719 193 745 193 771 195 781 200\\\r\n 794 205 809 210 822 216 840 221 853 226 865 236 873 246 882 256 890 266 899 276 907 286 916 296 924 306 933 316 941\\\r\n 326 950 336 958 345 965 360 968 380 974 395 977 415 983 430 986 447 991 473 991 499 991 525 991 551 991 574 991 592\\\r\n 985 605 979 620 974 632 968 651 961 668 955 682 943 695 930 708 917 717 908 727 900 736 889 743 877 750 866 757 854\\\r\n 764 843 771 831 778 820 785 808 792 797 799 786 799 760 799 734 799 708 799 682 799 656 799 630 799 604 799 578 799\\\r\n 552 799 526 799 500 799 474 799 448 799 422 799 396 799 370 799 344 799 318 799 292 799 266 799 240 799 214 799 188\\\r\n 799 162 799 136 799 110 799 84 799 58 799 36 799 33 773 33 747 33 721 33 695 33 669 33 643 33 617 33 611 33\r\n";
  const char  stampK[] = "@VERSION 20000\r\n\r\n@SYSENV 3\r\n@TILE 2\r\n@TILELINE 0 13 96 32 96 992 288 992 288 672 403 557 704 992 928 992 528 426 896 32 672 32 288 416 288 32 96 32\r\n@TILEAREA 1 0 206 427 1 270 97 34 97 60 97 86 97 112 97 138 97 164 97 190 97 216 97 242 97 268 97 294 97 320 97 346\\\r\n 97 372 97 398 97 424 97 450 97 476 97 502 97 528 97 554 97 580 97 606 97 632 97 658 97 684 97 710 97 736 97 762 97\\\r\n 788 97 814 97 840 97 866 97 892 97 918 97 944 97 970 97 988 97 991 123 991 149 991 175 991 201 991 227 991 253 991\\\r\n 279 991 286 991 287 974 287 948 287 922 287 896 287 870 287 844 287 818 287 792 287 766 287 740 287 714 287 688 287\\\r\n 672 300 659 313 646 326 633 339 620 352 607 365 594 378 581 391 568 401 557 404 556 411 567 419 577 426 588 434 599\\\r\n 441 610 449 621 456 632 464 642 471 653 479 664 486 675 494 686 501 697 510 709 517 720 525 731 532 742 540 752 547\\\r\n 763 555 774 562 785 570 796 577 807 585 817 592 828 600 839 607 850 615 861 622 872 630 882 637 893 645 904 652 915\\\r\n 660 926 667 937 675 947 682 958 690 969 697 980 704 991 730 991 756 991 782 991 808 991 834 991 860 991 886 991 912\\\r\n 991 924 991 923 986 915 976 908 965 900 955 893 944 885 934 878 923 870 913 863 902 855 891 848 880 840 870 833 859\\\r\n 825 849 818 838 810 828 803 817 794 805 787 794 779 784 772 773 764 763 757 752 748 740 741 729 733 719 726 708 717\\\r\n 696 710 685 702 675 695 664 687 654 680 643 671 631 664 620 656 610 649 599 641 589 634 578 626 567 619 556 611 546\\\r\n 604 535 596 525 589 514 581 504 574 493 565 481 558 470 550 460 543 449 535 439 528 427 534 417 547 405 555 395 568\\\r\n 382 577 373 585 363 598 350 610 336 623 323 632 314 640 304 653 291 665 277 675 268 683 258 696 245 708 231 718 222\\\r\n 726 212 739 199 751 185 761 176 769 166 782 153 794 139 807 126 816 117 824 107 837 94 849 80 859 71 867 61 880 48\\\r\n 892 34 894 33 868 33 842 33 816 33 790 33 764 33 738 33 712 33 686 33 673 33 660 46 647 59 634 72 621 85 608 98 595\\\r\n 111 582 124 569 137 556 150 543 163 530 176 517 189 504 202 491 215 478 228 465 241 452 254 439 267 426 280 413 293\\\r\n 400 306 387 319 374 332 361 345 348 358 335 371 322 384 309 397 296 410 289 417 287 409 287 383 287 357 287 331 287\\\r\n 305 287 279 287 253 287 227 287 201 287 175 287 149 287 123 287 97 287 71 287 45 287 35 279 33 253 33 227 33 201\\\r\n 33 175 33 149 33 123 33 99 33\r\n";
  const char  stampL[] = "@VERSION 20000\r\n\r\n@SYSENV 2\r\n@TILE 2\r\n@TILELINE 0 7 192 32 192 992 832 992 832 800 384 800 384 32 192 32\r\n@TILEAREA 1 0 262 438 1 130 193 34 193 60 193 86 193 112 193 138 193 164 193 190 193 216 193 242 193 268 193 294 193\\\r\n 320 193 346 193 372 193 398 193 424 193 450 193 476 193 502 193 528 193 554 193 580 193 606 193 632 193 658 193 684\\\r\n 193 710 193 736 193 762 193 788 193 814 193 840 193 866 193 892 193 918 193 944 193 970 193 988 193 991 219 991 245\\\r\n 991 271 991 297 991 323 991 349 991 375 991 401 991 427 991 453 991 479 991 505 991 531 991 557 991 583 991 609 991\\\r\n 635 991 661 991 687 991 713 991 739 991 765 991 791 991 817 991 829 991 831 983 831 957 831 931 831 905 831 879 831\\\r\n 853 831 827 831 804 831 801 805 801 779 801 753 801 727 801 701 801 675 801 649 801 623 801 597 801 571 801 545 801\\\r\n 519 801 493 801 467 801 441 801 415 801 389 801 384 801 383 784 383 758 383 732 383 706 383 680 383 654 383 628 383\\\r\n 602 383 576 383 550 383 524 383 498 383 472 383 446 383 420 383 394 383 368 383 342 383 316 383 290 383 264 383 238\\\r\n 383 212 383 186 383 160 383 134 383 108 383 82 383 56 383 36 383 33 357 33 331 33 305 33 279 33 253 33 227 33 201\\\r\n 33 195 33\r\n";
  const char  stampM[] = "@VERSION 20000\r\n\r\n@SYSENV 2\r\n@TILE 2\r\n@TILELINE 0 14 32 32 32 992 224 992 224 384 384 992 576 992 800 320 800 992 992 992 992 32 704 32 500 704 320 32 32\\\r\n 32\r\n@TILEAREA 1 0 150 459 1 338 33 34 33 60 33 86 33 112 33 138 33 164 33 190 33 216 33 242 33 268 33 294 33 320 33 346\\\r\n 33 372 33 398 33 424 33 450 33 476 33 502 33 528 33 554 33 580 33 606 33 632 33 658 33 684 33 710 33 736 33 762 33\\\r\n 788 33 814 33 840 33 866 33 892 33 918 33 944 33 970 33 988 33 991 59 991 85 991 111 991 137 991 163 991 189 991\\\r\n 215 991 222 991 223 974 223 948 223 922 223 896 223 870 223 844 223 818 223 792 223 766 223 740 223 714 223 688 223\\\r\n 662 223 636 223 610 223 584 223 558 223 532 223 506 223 480 223 454 223 428 223 402 223 385 225 383 229 397 232 412\\\r\n 237 427 240 442 246 461 249 476 254 492 257 507 262 522 265 537 271 556 274 571 279 587 282 602 287 617 290 632 296\\\r\n 651 299 666 304 682 307 697 312 712 315 727 321 746 324 761 329 777 332 792 337 807 340 822 346 841 349 856 354 872\\\r\n 357 887 362 902 365 917 371 936 374 951 379 967 382 982 384 990 401 991 427 991 453 991 479 991 505 991 531 991 557\\\r\n 991 574 991 579 978 586 959 592 939 599 920 605 900 612 881 618 861 625 842 631 822 638 803 644 783 651 764 657 744\\\r\n 664 725 670 705 677 686 683 666 690 647 696 627 703 608 709 588 716 569 722 549 729 530 735 510 742 491 748 471 755\\\r\n 452 761 432 768 413 774 393 781 374 787 354 794 335 798 321 800 319 801 336 801 362 801 388 801 414 801 440 801 466\\\r\n 801 492 801 518 801 544 801 570 801 596 801 622 801 648 801 674 801 700 801 726 801 752 801 778 801 804 801 830 801\\\r\n 856 801 882 801 908 801 934 801 960 801 986 801 990 818 991 844 991 870 991 896 991 922 991 948 991 974 991 989 991\\\r\n 991 983 991 957 991 931 991 905 991 879 991 853 991 827 991 801 991 775 991 749 991 723 991 697 991 671 991 645 991\\\r\n 619 991 593 991 567 991 541 991 515 991 489 991 463 991 437 991 411 991 385 991 359 991 333 991 307 991 281 991 255\\\r\n 991 229 991 203 991 177 991 151 991 125 991 99 991 73 991 47 991 35 983 33 957 33 931 33 905 33 879 33 853 33 827\\\r\n 33 801 33 775 33 749 33 723 33 706 33 701 46 696 60 692 74 686 93 682 107 676 126 672 140 666 159 662 173 656 192\\\r\n 652 206 646 226 640 246 635 261 631 275 625 294 621 308 615 327 611 341 605 360 601 374 595 394 589 414 584 429 580\\\r\n 443 574 462 570 476 564 495 560 509 554 528 550 542 544 562 538 582 533 597 529 611 523 630 519 644 513 663 509 677\\\r\n 503 696 501 704 498 703 495 688 490 673 487 658 482 643 479 628 474 613 471 598 466 583 463 568 457 550 454 535 449\\\r\n 520 446 505 441 490 438 475 433 460 430 445 425 430 422 415 416 397 413 382 408 367 405 352 400 337 397 322 392 307\\\r\n 389 292 384 277 381 262 376 247 373 232 367 214 364 199 359 184 356 169 351 154 348 139 343 124 340 109 335 94 332\\\r\n 79 326 61 323 46 320 34 303 33 277 33 251 33 225 33 199 33 173 33 147 33 121 33 95 33 69 33 43 33 35 33\r\n";
  const char  stampN[] = "@VERSION 20000\r\n\r\n@SYSENV 2\r\n@TILE 2\r\n@TILELINE 0 11 96 32 96 992 288 992 288 320 704 992 928 992 928 32 736 32 736 704 320 32 96 32\r\n@TILEAREA 1 0 150 406 1 282 97 34 97 60 97 86 97 112 97 138 97 164 97 190 97 216 97 242 97 268 97 294 97 320 97 346\\\r\n 97 372 97 398 97 424 97 450 97 476 97 502 97 528 97 554 97 580 97 606 97 632 97 658 97 684 97 710 97 736 97 762 97\\\r\n 788 97 814 97 840 97 866 97 892 97 918 97 944 97 970 97 988 97 991 123 991 149 991 175 991 201 991 227 991 253 991\\\r\n 279 991 286 991 287 974 287 948 287 922 287 896 287 870 287 844 287 818 287 792 287 766 287 740 287 714 287 688 287\\\r\n 662 287 636 287 610 287 584 287 558 287 532 287 506 287 480 287 454 287 428 287 402 287 376 287 350 287 324 287 320\\\r\n 292 324 299 336 307 348 314 360 321 371 328 383 336 395 343 407 351 419 358 431 365 442 372 454 380 466 387 478 394\\\r\n 489 401 501 409 513 416 525 424 537 431 549 438 560 445 572 453 584 460 596 468 608 475 620 482 631 489 643 497 655\\\r\n 504 667 511 678 518 690 526 702 533 714 541 726 548 738 555 749 562 761 570 773 577 785 585 797 592 809 599 820 606\\\r\n 832 614 844 621 856 628 867 635 879 643 891 650 903 658 915 665 927 672 938 679 950 687 962 694 974 702 986 704 991\\\r\n 730 991 756 991 782 991 808 991 834 991 860 991 886 991 912 991 925 991 927 983 927 957 927 931 927 905 927 879 927\\\r\n 853 927 827 927 801 927 775 927 749 927 723 927 697 927 671 927 645 927 619 927 593 927 567 927 541 927 515 927 489\\\r\n 927 463 927 437 927 411 927 385 927 359 927 333 927 307 927 281 927 255 927 229 927 203 927 177 927 151 927 125 927\\\r\n 99 927 73 927 47 927 35 919 33 893 33 867 33 841 33 815 33 789 33 763 33 740 33 737 33 737 59 737 85 737 111 737\\\r\n 137 737 163 737 189 737 215 737 241 737 267 737 293 737 319 737 345 737 371 737 397 737 423 737 449 737 475 737 501\\\r\n 737 527 737 553 737 579 737 605 737 631 737 657 737 683 737 702 737 705 732 700 725 688 717 676 710 664 703 653 696\\\r\n 641 688 629 681 617 673 605 666 593 659 582 652 570 644 558 637 546 630 535 623 523 615 511 608 499 600 487 593 475\\\r\n 586 464 579 452 571 440 564 428 556 416 549 404 542 393 535 381 527 369 520 357 513 346 506 334 498 322 491 310 483\\\r\n 298 476 286 469 275 462 263 454 251 447 239 439 227 432 215 425 204 418 192 410 180 403 168 396 157 389 145 381 133\\\r\n 374 121 366 109 359 97 352 86 345 74 337 62 330 50 322 38 320 33 294 33 268 33 242 33 216 33 190 33 164 33 138 33\\\r\n 112 33 99 33\r\n";
  const char  stampO[] = "@VERSION 20000\r\n\r\n@SYSENV 3\r\n@TILE 3\r\n@TILELINE 0 26 512 32 480 32 352 64 224 160 160 256 128 320 96 448 96 576 128 672 160 768 224 864 384 960 480 992\\\r\n 544 992 640 960 800 864 864 768 896 672 928 576 928 448 896 320 864 256 800 160 672 64 544 32 512 32\r\n@TILELINE 1 24 512 192 480 192 416 212 352 256 312 320 288 384 273 480 273 544 288 672 320 736 384 800 480 832 544\\\r\n 832 640 800 704 736 736 672 751 544 751 480 736 384 712 320 672 256 608 212 544 192 512 192\r\n@TILEAREA 2 0 222 310 2 174 478 34 463 37 446 42 431 45 414 50 399 53 382 58 367 61 352 65 341 74 330 81 320 90 309\\\r\n 97 297 107 286 114 276 123 265 130 253 140 242 147 232 156 225 160 218 171 210 182 203 194 195 205 188 216 180 227\\\r\n 173 239 165 250 158 263 151 275 145 289 138 301 132 315 128 322 125 337 120 354 117 369 112 386 109 401 104 418 101\\\r\n 433 97 447 97 473 97 499 97 525 97 551 97 574 103 594 110 613 116 633 123 652 129 672 136 691 142 711 149 730 155\\\r\n 750 160 766 168 777 175 789 183 800 190 811 198 822 205 834 213 845 220 856 229 866 242 873 254 881 267 888 279 896\\\r\n 292 903 304 911 317 918 329 926 342 933 354 941 367 948 379 956 398 963 417 970 437 976 456 983 476 989 492 991 518\\\r\n 991 542 991 562 985 581 978 601 972 620 965 638 960 649 953 662 946 674 938 687 931 699 923 712 916 724 908 737 901\\\r\n 749 893 762 886 774 878 787 871 799 864 806 853 814 842 821 830 829 819 836 808 844 797 851 785 859 774 867 756 873\\\r\n 736 880 717 886 697 893 678 899 658 906 639 912 619 919 600 925 580 927 564 927 538 927 512 927 486 927 460 926 443\\\r\n 921 427 918 412 913 395 910 380 905 363 902 348 897 331 895 320 888 308 882 294 875 282 869 268 862 256 855 244 847\\\r\n 233 840 222 832 211 825 199 817 188 810 177 802 166 798 159 787 152 775 142 764 135 754 126 743 119 731 109 720 102\\\r\n 710 93 699 86 687 76 676 69 670 64 655 61 638 56 623 53 606 48 591 45 574 40 559 37 545 33 519 33 493 33 480 33 109\\\r\n 547 192 565 198 579 202 597 208 607 210 618 218 629 225 640 233 651 240 662 248 673 255 680 267 688 279 695 291 703\\\r\n 303 710 315 716 327 723 346 729 364 734 377 739 393 741 409 744 425 746 441 749 457 751 473 752 490 752 516 752 542\\\r\n 750 563 748 579 746 597 744 613 742 631 740 647 738 665 734 679 727 691 721 705 714 717 708 731 696 745 683 758 670\\\r\n 771 657 784 644 797 633 804 613 810 594 817 574 823 555 830 540 833 514 833 488 833 472 831 453 824 433 818 414 811\\\r\n 394 805 382 799 369 786 356 773 343 760 330 747 319 737 313 724 306 712 300 698 293 686 287 674 285 654 283 638 281\\\r\n 620 279 604 277 586 275 570 273 552 272 535 272 509 272 483 274 466 277 451 279 434 282 419 284 402 287 387 291 373\\\r\n 297 355 302 342 309 323 316 312 323 300 331 288 338 276 346 264 351 255 362 248 374 239 385 232 396 224 407 217 418\\\r\n 210 438 204 455 198 469 194 484 191 510 191 536 191 545 191\r\n";
  const char  stampP[] = "@VERSION 20000\r\n\r\n@SYSENV 2\r\n@TILE 4\r\n@TILELINE 0 15 160 32 160 992 352 992 352 672 608 672 704 640 768 608 832 544 864 448 864 288 832 192 768 96 704 64\\\r\n 576 32 160 32\r\n@TILELINE 1 8 352 512 608 512 672 448 704 384 704 320 672 256 608 192 352 192\r\n@TILELINE 2 2 352 192 352 512\r\n@TILEAREA 3 0 230 396 2 141 161 34 161 60 161 86 161 112 161 138 161 164 161 190 161 216 161 242 161 268 161 294 161\\\r\n 320 161 346 161 372 161 398 161 424 161 450 161 476 161 502 161 528 161 554 161 580 161 606 161 632 161 658 161 684\\\r\n 161 710 161 736 161 762 161 788 161 814 161 840 161 866 161 892 161 918 161 944 161 970 161 988 161 991 187 991 213\\\r\n 991 239 991 265 991 291 991 317 991 343 991 350 991 351 974 351 948 351 922 351 896 351 870 351 844 351 818 351 792\\\r\n 351 766 351 740 351 714 351 688 351 673 359 671 385 671 411 671 437 671 463 671 489 671 515 671 541 671 567 671 593\\\r\n 671 610 670 630 664 649 657 669 651 688 644 702 640 714 633 728 627 740 620 754 614 766 607 776 599 789 586 802 573\\\r\n 815 560 828 547 834 535 840 515 847 496 853 476 860 457 863 442 863 416 863 390 863 364 863 338 863 312 863 290 857\\\r\n 270 850 251 844 231 837 212 832 194 824 183 817 171 809 160 802 149 794 138 787 126 779 115 772 104 763 94 749 88\\\r\n 737 81 723 75 711 68 698 63 683 60 666 55 651 52 634 47 619 44 602 39 587 36 572 33 546 33 520 33 494 33 468 33 442\\\r\n 33 416 33 390 33 364 33 338 33 312 33 286 33 260 33 234 33 208 33 182 33 163 33 59 610 192 623 205 636 218 649 231\\\r\n 662 244 672 254 678 267 685 279 691 293 698 305 705 319 705 345 705 371 705 385 698 397 692 411 685 423 679 437 669\\\r\n 453 656 466 643 479 630 492 617 505 609 513 583 513 557 513 531 513 505 513 479 513 453 513 427 513 401 513 375 513\\\r\n 354 513 351 513 351 487 351 461 351 435 351 409 351 383 351 357 351 331 351 305 351 279 351 253 351 227 351 201 351\\\r\n 192 368 191 394 191 420 191 446 191 472 191 498 191 524 191 550 191 576 191 602 191 608 191\r\n";
  const char  stampQ[] = "@VERSION 20000\r\n\r\n@SYSENV 3\r\n@TILE 3\r\n@TILELINE 0 27 440 32 352 64 288 96 201 164 160 224 96 352 75 480 75 544 96 704 160 832 256 928 416 992 576 992 672\\\r\n 960 736 928 820 1009 916 913 832 832 896 736 928 544 928 480 896 320 832 192 768 128 672 64 576 32 440 32\r\n@TILELINE 1 26 480 192 384 224 320 288 288 320 256 406 246 480 246 544 256 618 288 704 320 768 384 800 448 832 544\\\r\n 832 586 810 530 756 626 660 704 736 736 672 768 582 776 512 768 448 736 352 704 288 640 224 544 192 480 192\r\n@TILEAREA 2 0 150 367 2 188 439 34 425 38 412 43 393 50 374 57 359 62 347 68 333 74 321 81 307 87 295 94 283 101 273\\\r\n 110 262 117 252 126 241 134 229 144 218 152 206 162 200 167 193 178 185 189 178 200 170 211 163 222 156 233 150 247\\\r\n 143 259 137 273 130 285 124 299 117 311 111 325 104 337 97 351 95 367 92 382 90 398 86 416 84 432 81 447 79 463 76\\\r\n 478 76 504 76 530 77 552 80 570 82 590 85 608 87 628 90 646 92 666 95 684 97 702 102 715 109 727 115 741 122 753\\\r\n 128 767 135 779 141 793 148 805 154 819 164 835 177 848 190 861 203 874 216 887 229 900 242 913 255 926 268 931 280\\\r\n 937 298 943 310 949 328 955 340 961 358 967 370 973 388 979 400 985 415 991 441 991 467 991 493 991 519 991 545 991\\\r\n 571 991 586 988 605 981 625 975 644 968 664 962 678 956 690 949 704 943 716 936 730 930 737 927 748 939 758 947 771\\\r\n 960 784 973 797 986 811 998 820 1007 828 999 841 986 854 973 867 960 880 947 893 934 906 921 914 913 906 905 893\\\r\n 892 880 879 866 867 853 854 840 841 831 833 838 821 846 810 853 798 861 787 868 776 876 765 883 753 891 742 897 726\\\r\n 899 710 902 695 904 679 907 664 909 648 913 630 915 614 918 599 920 583 923 568 925 552 927 536 927 510 927 484 925\\\r\n 468 921 453 918 436 914 418 911 401 907 383 904 366 900 348 897 331 893 316 886 304 880 290 873 278 867 264 860 252\\\r\n 854 238 847 226 841 212 834 200 827 189 814 176 801 163 788 150 775 137 765 127 754 120 743 112 731 105 720 97 709\\\r\n 90 698 82 686 75 675 67 656 60 637 53 617 47 598 40 578 34 561 33 535 33 509 33 483 33 457 33 441 33 123 547 192\\\r\n 567 198 586 205 606 211 625 218 639 222 652 235 665 248 678 261 691 274 704 286 710 299 717 311 723 325 730 337 736\\\r\n 351 741 364 748 383 754 403 761 422 767 442 771 460 773 481 776 500 777 517 774 534 773 551 770 569 768 585 762 604\\\r\n 757 617 751 635 746 648 741 663 735 675 729 689 722 701 716 715 709 727 705 736 696 729 686 719 676 711 663 698 650\\\r\n 685 637 672 627 663 623 665 610 678 597 691 584 704 571 717 558 730 545 743 533 755 535 759 548 772 561 785 570 794\\\r\n 580 802 587 809 582 814 568 820 557 827 545 833 519 833 493 833 467 833 449 833 435 827 423 820 409 814 397 807 383\\\r\n 801 371 794 357 788 345 781 331 775 319 769 313 756 306 744 300 730 293 718 286 700 281 687 274 668 269 655 264 641\\\r\n 259 628 254 615 252 596 249 578 247 559 245 543 245 517 245 491 246 471 249 454 251 434 254 417 256 401 261 388 268\\\r\n 369 273 356 278 342 283 329 289 317 302 304 315 291 328 278 341 265 354 252 367 239 380 226 392 220 412 214 431 207\\\r\n 451 201 470 194 485 191 511 191 537 191 545 191\r\n";
  const char  stampR[] = "@VERSION 20000\r\n\r\n@SYSENV 3\r\n@TILE 3\r\n@TILELINE 0 22 128 32 128 992 320 992 320 608 544 608 608 640 672 736 704 992 896 992 864 736 832 640 768 544 768\\\r\n 512 842 448 891 352 896 288 875 192 832 128 768 81 704 51 608 32 128 32\r\n@TILELINE 1 10 320 192 320 480 576 480 640 448 700 384 704 320 686 257 648 211 576 192 320 192\r\n@TILEAREA 2 0 230 382 2 203 129 34 129 60 129 86 129 112 129 138 129 164 129 190 129 216 129 242 129 268 129 294 129\\\r\n 320 129 346 129 372 129 398 129 424 129 450 129 476 129 502 129 528 129 554 129 580 129 606 129 632 129 658 129 684\\\r\n 129 710 129 736 129 762 129 788 129 814 129 840 129 866 129 892 129 918 129 944 129 970 129 988 129 991 155 991 181\\\r\n 991 207 991 233 991 259 991 285 991 311 991 318 991 319 974 319 948 319 922 319 896 319 870 319 844 319 818 319 792\\\r\n 319 766 319 740 319 714 319 688 319 662 319 636 319 610 319 607 345 607 371 607 397 607 423 607 449 607 475 607 501\\\r\n 607 527 607 545 607 558 613 570 620 584 626 596 633 608 639 616 649 623 661 631 672 638 683 646 694 653 706 661 717\\\r\n 668 728 673 735 675 753 678 772 680 793 683 812 685 833 688 852 690 873 693 892 695 913 698 932 700 953 703 972 705\\\r\n 988 705 991 731 991 757 991 783 991 809 991 835 991 861 991 887 991 894 991 893 976 890 957 888 936 885 917 883 896\\\r\n 880 877 878 856 875 837 873 816 870 797 868 776 865 757 863 738 857 718 850 699 844 679 837 660 832 642 824 631 817\\\r\n 619 809 608 802 597 794 586 787 574 779 563 772 552 767 545 767 519 768 510 778 501 788 493 799 483 809 475 821 464\\\r\n 831 456 841 448 847 435 854 423 860 409 867 398 873 384 880 372 886 358 890 352 891 335 893 319 894 297 893 282 890\\\r\n 266 886 250 883 234 879 218 876 202 871 188 863 177 856 165 848 155 841 143 833 132 828 126 817 119 805 109 794 102\\\r\n 783 93 772 86 761 79 747 73 735 67 722 61 710 55 696 50 679 47 661 43 645 41 630 37 613 34 596 33 570 33 544 33 518\\\r\n 33 492 33 466 33 440 33 414 33 388 33 362 33 336 33 310 33 284 33 258 33 232 33 206 33 180 33 154 33 131 33 61 579\\\r\n 192 597 197 612 200 631 206 646 209 655 217 663 227 674 240 682 250 688 258 692 273 698 293 702 308 705 323 704 341\\\r\n 702 360 701 382 693 393 680 406 668 420 655 433 643 447 631 454 617 460 605 467 591 473 579 480 562 481 536 481 510\\\r\n 481 484 481 458 481 432 481 406 481 380 481 354 481 328 481 320 481 319 464 319 438 319 412 319 386 319 360 319 334\\\r\n 319 308 319 282 319 256 319 230 319 204 319 193 327 191 353 191 379 191 405 191 431 191 457 191 483 191 509 191 535\\\r\n 191 561 191 577 191\r\n";
  const char  stampS[] = "@VERSION 20000\r\n\r\n@SYSENV 3\r\n@TILE 2\r\n@TILELINE 0 46 128 704 160 832 256 928 384 992 608 992 704 960 800 896 864 800 887 704 864 576 800 512 704 448 576\\\r\n 416 448 384 352 352 328 274 357 197 448 160 544 160 640 192 672 242 672 288 849 288 832 192 800 128 704 64 576 32\\\r\n 416 32 288 64 224 128 160 224 140 305 160 384 224 480 352 544 512 576 608 608 682 640 704 704 687 768 606 832 416\\\r\n 832 352 800 318 764 312 704 128 704\r\n@TILEAREA 1 0 259 339 1 295 414 34 399 37 382 42 367 45 350 50 335 53 318 58 303 61 289 65 276 78 263 91 250 104 237\\\r\n 117 225 128 218 139 210 150 203 162 195 173 188 184 180 195 173 207 165 218 159 231 156 246 151 263 148 278 143 295\\\r\n 142 310 147 326 150 341 155 358 158 373 163 387 171 398 178 409 186 420 193 432 201 443 208 454 216 465 223 477 234\\\r\n 484 248 490 260 497 274 503 286 510 300 516 312 523 326 529 338 536 355 544 372 547 390 551 407 554 425 558 442 561\\\r\n 460 565 477 568 495 572 511 575 531 581 550 588 570 594 589 601 609 607 624 613 636 619 649 624 661 630 677 636 685\\\r\n 646 692 665 698 685 705 702 702 717 696 736 693 751 689 767 678 777 667 785 654 796 643 804 630 815 619 823 608 832\\\r\n 591 833 565 833 539 833 513 833 487 833 461 833 435 833 417 833 403 827 391 820 377 814 365 807 352 801 342 792 334\\\r\n 782 321 769 316 760 315 743 313 727 312 710 311 705 285 705 259 705 233 705 207 705 181 705 155 705 132 705 129 705\\\r\n 134 722 137 737 142 754 145 769 150 786 153 801 158 818 160 830 173 843 186 856 199 869 212 882 225 895 238 908 251\\\r\n 921 261 929 274 935 286 942 300 948 312 955 326 961 338 968 352 974 364 981 378 987 384 991 410 991 436 991 462 991\\\r\n 488 991 514 991 540 991 566 991 592 991 609 990 629 984 648 977 668 971 687 964 702 960 713 952 725 945 736 937 747\\\r\n 930 758 922 770 915 781 907 792 900 802 892 809 880 817 869 824 858 832 847 839 835 847 824 854 813 862 802 867 785\\\r\n 870 770 874 756 877 741 881 726 884 711 885 696 881 679 879 663 875 646 873 630 869 613 867 597 863 577 850 564 837\\\r\n 551 824 538 811 525 800 513 789 506 778 498 766 491 755 483 744 476 733 468 721 461 710 453 698 447 683 444 666 439\\\r\n 651 436 634 431 619 428 602 423 587 420 570 415 555 412 538 407 523 404 506 399 491 396 474 391 459 388 439 382 419\\\r\n 376 400 369 380 363 361 356 351 353 345 335 341 321 335 301 329 283 327 272 332 259 339 240 344 227 351 208 357 196\\\r\n 370 191 382 185 400 179 412 173 427 168 439 162 454 159 480 159 506 159 532 159 549 160 568 167 588 173 607 180 627\\\r\n 186 640 191 648 202 655 214 663 225 670 237 673 244 673 270 673 285 681 287 707 287 733 287 759 287 785 287 811 287\\\r\n 837 287 847 287 845 273 843 257 839 241 837 225 833 207 831 192 824 180 818 166 811 154 805 140 799 128 787 121 776\\\r\n 113 765 106 754 98 742 91 731 83 720 76 709 68 694 62 679 59 662 54 647 51 630 46 615 43 598 38 583 35 567 33 541\\\r\n 33 515 33 489 33 463 33 437 33 416 33\r\n";
  const char  stampT[] = "@VERSION 20000\r\n\r\n@SYSENV 2\r\n@TILE 2\r\n@TILELINE 0 9 128 32 128 210 416 210 416 992 608 992 608 210 896 210 896 32 128 32\r\n@TILEAREA 1 0 451 396 1 141 129 34 129 60 129 86 129 112 129 138 129 164 129 190 129 207 137 209 163 209 189 209 215\\\r\n 209 241 209 267 209 293 209 319 209 345 209 371 209 397 209 415 209 417 217 417 243 417 269 417 295 417 321 417 347\\\r\n 417 373 417 399 417 425 417 451 417 477 417 503 417 529 417 555 417 581 417 607 417 633 417 659 417 685 417 711 417\\\r\n 737 417 763 417 789 417 815 417 841 417 867 417 893 417 919 417 945 417 971 417 989 425 991 451 991 477 991 503 991\\\r\n 529 991 555 991 581 991 604 991 607 991 607 965 607 939 607 913 607 887 607 861 607 835 607 809 607 783 607 757 607\\\r\n 731 607 705 607 679 607 653 607 627 607 601 607 575 607 549 607 523 607 497 607 471 607 445 607 419 607 393 607 367\\\r\n 607 341 607 315 607 289 607 263 607 237 607 212 607 209 633 209 659 209 685 209 711 209 737 209 763 209 789 209 815\\\r\n 209 841 209 867 209 892 209 895 209 895 183 895 157 895 131 895 105 895 79 895 53 895 35 887 33 861 33 835 33 809\\\r\n 33 783 33 757 33 731 33 705 33 679 33 653 33 627 33 601 33 575 33 549 33 523 33 497 33 471 33 445 33 419 33 393 33\\\r\n 367 33 341 33 315 33 289 33 263 33 237 33 211 33 185 33 159 33 133 33 131 33\r\n";
  const char  stampU[] = "@VERSION 20000\r\n\r\n@SYSENV 2\r\n@TILE 2\r\n@TILELINE 0 21 128 32 128 704 160 800 256 928 416 992 608 992 768 928 864 800 896 704 896 32 704 32 704 672 672 736\\\r\n 608 800 544 832 480 832 416 800 352 736 320 672 320 32 128 32\r\n@TILEAREA 1 0 216 246 1 217 129 34 129 60 129 86 129 112 129 138 129 164 129 190 129 216 129 242 129 268 129 294 129\\\r\n 320 129 346 129 372 129 398 129 424 129 450 129 476 129 502 129 528 129 554 129 580 129 606 129 632 129 658 129 684\\\r\n 129 702 135 722 142 741 148 761 155 780 160 798 168 808 175 819 185 831 192 842 201 852 208 863 218 875 225 886 234\\\r\n 896 241 907 251 919 255 926 268 931 280 937 298 943 310 949 328 955 340 961 358 967 370 973 388 979 400 985 415 991\\\r\n 441 991 467 991 493 991 519 991 545 991 571 991 597 991 609 991 621 985 639 979 651 973 669 967 681 961 699 955 711\\\r\n 949 729 943 741 937 759 931 770 924 777 913 786 903 793 892 803 880 810 869 819 859 826 848 836 836 843 825 852 815\\\r\n 859 804 865 792 872 773 878 753 885 734 891 714 895 700 895 674 895 648 895 622 895 596 895 570 895 544 895 518 895\\\r\n 492 895 466 895 440 895 414 895 388 895 362 895 336 895 310 895 284 895 258 895 232 895 206 895 180 895 154 895 128\\\r\n 895 102 895 76 895 50 895 35 887 33 861 33 835 33 809 33 783 33 757 33 731 33 708 33 705 33 705 59 705 85 705 111\\\r\n 705 137 705 163 705 189 705 215 705 241 705 267 705 293 705 319 705 345 705 371 705 397 705 423 705 449 705 475 705\\\r\n 501 705 527 705 553 705 579 705 605 705 631 705 657 705 673 698 685 692 699 685 711 679 725 669 741 656 754 643 767\\\r\n 630 780 617 793 607 802 593 808 581 815 567 821 555 828 542 833 516 833 490 833 479 833 467 826 453 820 441 813 427\\\r\n 807 411 797 398 784 385 771 372 758 359 745 350 736 344 722 337 710 331 696 324 684 319 671 319 645 319 619 319 593\\\r\n 319 567 319 541 319 515 319 489 319 463 319 437 319 411 319 385 319 359 319 333 319 307 319 281 319 255 319 229 319\\\r\n 203 319 177 319 151 319 125 319 99 319 73 319 47 319 35 311 33 285 33 259 33 233 33 207 33 181 33 155 33 131 33\r\n";
  const char  stampV[] = "@VERSION 20000\r\n\r\n@SYSENV 2\r\n@TILE 2\r\n@TILELINE 0 8 96 32 416 992 608 992 928 32 736 32 512 736 288 32 96 32\r\n@TILEAREA 1 0 294 353 1 206 98 33 104 53 111 72 117 92 124 111 130 131 137 150 143 170 150 189 156 209 163 228 169\\\r\n 248 176 267 182 287 189 306 195 326 202 345 208 365 215 384 221 404 228 423 234 443 241 462 247 482 254 501 260 521\\\r\n 267 540 273 560 280 579 286 599 293 618 299 638 306 657 312 677 319 696 325 716 332 735 338 755 345 774 351 794 358\\\r\n 813 364 833 371 852 377 872 384 891 390 911 397 930 403 950 410 969 416 989 424 991 450 991 476 991 502 991 528 991\\\r\n 554 991 580 991 604 991 609 986 615 966 622 947 628 927 635 908 641 888 648 869 654 849 661 830 667 810 674 791 680\\\r\n 771 687 752 693 732 700 713 706 693 713 674 719 654 726 635 732 615 739 596 745 576 752 557 758 537 765 518 771 498\\\r\n 778 479 784 459 791 440 797 420 804 401 810 381 817 362 823 342 830 323 836 303 843 284 849 264 856 245 862 225 869\\\r\n 206 875 186 882 167 888 147 895 128 901 108 908 89 914 69 921 50 926 35 927 33 901 33 875 33 849 33 823 33 797 33\\\r\n 771 33 745 33 737 33 731 53 724 72 719 87 715 101 709 121 703 141 696 160 691 175 687 189 681 209 675 229 668 248\\\r\n 663 263 659 277 653 297 647 317 640 336 635 351 631 365 625 385 619 405 612 424 607 439 603 453 597 473 591 493 584\\\r\n 512 579 527 575 541 569 561 563 581 556 600 551 615 547 629 541 649 535 669 528 688 523 703 519 717 514 735 511 737\\\r\n 505 717 500 704 496 690 490 670 484 650 477 631 472 616 468 602 462 582 456 562 449 543 444 528 440 514 434 494 428\\\r\n 474 421 455 416 440 412 426 406 406 400 386 393 367 388 352 384 338 378 318 372 298 365 279 360 264 356 250 350 230\\\r\n 344 210 337 191 332 176 328 162 322 142 316 122 309 103 304 88 300 74 294 54 288 35 280 33 254 33 228 33 202 33 176\\\r\n 33 150 33 124 33 99 33\r\n";
  const char  stampW[] = "@VERSION 20000\r\n\r\n@SYSENV 2\r\n@TILE 2\r\n@TILELINE 0 14 0 128 224 928 384 928 512 416 640 928 800 928 1024 128 864 128 704 672 576 128 448 128 320 672 160\\\r\n 128 0 128\r\n@TILEAREA 1 0 158 347 1 345 2 129 5 144 11 161 14 176 19 190 23 205 28 222 32 237 37 254 40 269 46 286 49 301 54 315\\\r\n 58 330 63 347 67 362 72 379 75 394 81 411 84 426 89 440 93 455 98 472 102 487 107 504 110 519 116 536 119 551 124\\\r\n 565 128 580 133 597 137 612 142 629 145 644 151 661 154 676 159 690 163 705 168 722 172 737 177 754 180 769 186 786\\\r\n 189 801 194 815 198 830 203 847 207 862 212 879 215 894 221 911 224 925 232 927 258 927 284 927 310 927 336 927 362\\\r\n 927 380 927 384 925 387 910 392 893 395 878 400 861 403 846 408 829 411 814 416 797 419 782 424 765 427 750 432 733\\\r\n 435 718 440 701 443 686 448 669 451 654 456 637 459 622 464 605 467 590 472 573 475 558 480 541 483 526 488 509 491\\\r\n 494 496 477 499 462 504 445 507 430 511 416 514 418 517 433 522 450 525 465 530 482 533 497 538 514 541 529 546 546\\\r\n 549 561 554 578 557 593 562 610 565 625 570 642 573 657 578 674 581 689 586 706 589 721 594 738 597 753 602 770 605\\\r\n 785 610 802 613 817 618 834 621 849 626 866 629 881 634 898 637 913 640 926 657 927 683 927 709 927 735 927 761 927\\\r\n 787 927 798 927 802 919 805 904 810 890 814 875 819 858 823 843 828 826 831 811 837 794 840 779 845 765 849 750 854\\\r\n 733 858 718 863 701 866 686 872 669 875 654 880 640 884 625 889 608 893 593 898 576 901 561 907 544 910 529 915 515\\\r\n 919 500 924 483 928 468 933 451 936 436 942 419 945 404 950 390 954 375 959 358 963 343 968 326 971 311 977 294 980\\\r\n 279 985 265 989 250 994 233 998 218 1003 201 1006 186 1012 169 1015 154 1020 140 1022 130 1015 129 989 129 963 129\\\r\n 937 129 911 129 885 129 867 129 864 130 860 144 854 164 850 178 844 198 840 212 834 232 830 246 824 266 820 280 814\\\r\n 300 810 314 804 334 800 348 794 368 790 382 784 402 780 416 774 436 770 450 764 470 760 484 754 504 750 518 744 538\\\r\n 740 552 734 572 730 586 724 606 720 620 714 640 710 654 706 671 704 673 699 658 696 643 692 628 689 613 685 598 682\\\r\n 583 678 568 675 553 671 539 668 524 664 509 661 494 657 479 654 464 650 449 647 434 643 420 640 405 636 390 633 375\\\r\n 629 360 626 345 622 330 619 315 615 301 612 286 608 271 605 256 601 241 598 226 594 211 591 196 587 182 584 167 580\\\r\n 152 577 137 575 129 549 129 523 129 497 129 471 129 452 129 448 131 445 146 441 160 438 175 434 190 431 205 427 220\\\r\n 424 235 420 250 417 265 413 279 410 294 406 309 403 324 399 339 396 354 392 369 389 384 385 398 382 413 378 428 375\\\r\n 443 371 458 368 473 364 488 361 503 357 517 354 532 350 547 347 562 343 577 340 592 336 607 333 622 329 636 326 651\\\r\n 322 666 321 672 318 671 314 657 308 637 304 623 298 603 294 589 288 569 284 555 278 535 274 521 268 501 264 487 258\\\r\n 467 254 453 248 433 244 419 238 399 234 385 228 365 224 351 218 331 214 317 208 297 204 283 198 263 194 249 188 229\\\r\n 184 215 178 195 174 181 168 161 164 147 160 131 152 129 126 129 100 129 74 129 48 129 22 129 3 129\r\n";
  const char  stampX[] = "@VERSION 20000\r\n\r\n@SYSENV 2\r\n@TILE 2\r\n@TILELINE 0 15 128 32 384 495 384 528 96 992 320 992 512 672 704 992 928 992 640 528 640 495 896 32 672 32 512 320\\\r\n 352 32 128 32\r\n@TILEAREA 1 0 460 339 1 303 130 34 137 45 143 58 150 69 156 81 163 92 169 105 176 116 182 128 189 139 195 152 202\\\r\n 163 208 175 215 186 221 199 228 210 234 222 241 233 247 246 254 257 260 269 267 280 273 293 280 304 286 316 293 327\\\r\n 299 340 306 351 312 363 319 374 325 387 332 398 338 410 345 421 351 434 358 445 364 457 371 468 377 481 384 492 385\\\r\n 509 385 527 379 539 371 550 365 562 357 573 350 585 343 596 336 608 328 620 321 632 313 644 306 656 298 668 291 680\\\r\n 284 691 277 703 269 715 262 727 254 739 247 751 240 762 233 774 225 785 219 797 211 808 204 821 196 832 189 845 181\\\r\n 856 175 868 167 879 160 891 153 902 146 914 138 926 131 938 123 950 116 962 109 973 102 985 98 990 115 991 141 991\\\r\n 167 991 193 991 219 991 245 991 271 991 297 991 317 991 323 986 329 974 337 962 344 949 352 937 359 924 367 912 374\\\r\n 899 382 887 389 874 397 862 404 849 412 837 419 824 427 812 434 799 442 787 449 774 457 762 464 749 472 737 479 724\\\r\n 487 712 494 699 502 687 509 674 513 671 520 684 528 696 535 709 543 721 550 734 558 746 565 759 573 771 580 784 588\\\r\n 796 595 809 603 821 610 834 618 846 625 859 633 871 640 884 648 896 655 909 663 921 670 934 678 946 685 959 693 971\\\r\n 700 984 704 991 730 991 756 991 782 991 808 991 834 991 860 991 886 991 912 991 924 991 923 986 916 975 909 963 901\\\r\n 951 894 939 886 927 879 915 872 904 865 892 857 881 851 869 843 858 836 845 828 834 821 821 813 810 807 798 799 787\\\r\n 792 775 785 764 778 752 770 740 763 728 755 716 748 704 741 693 734 681 726 669 719 657 711 645 704 633 696 621 689\\\r\n 609 682 598 675 586 667 574 660 562 652 550 645 538 639 529 639 503 640 492 647 481 653 468 660 457 666 445 673 434\\\r\n 679 421 686 410 692 398 699 387 705 374 712 363 718 351 725 340 731 327 738 316 744 304 751 293 757 280 764 269 770\\\r\n 257 777 246 783 233 790 222 796 210 803 199 809 186 816 175 822 163 829 152 835 139 842 128 848 116 855 105 861 92\\\r\n 868 81 874 69 881 58 887 45 893 34 886 33 860 33 834 33 808 33 782 33 756 33 730 33 704 33 678 33 672 33 666 46 659\\\r\n 57 653 69 646 80 640 93 633 104 627 116 620 127 614 139 607 150 601 163 594 174 588 186 581 197 575 210 568 221 562\\\r\n 233 555 244 549 256 542 267 536 280 529 291 523 303 516 314 513 321 508 316 502 304 495 293 489 280 482 269 476 257\\\r\n 469 246 463 233 456 222 450 210 443 199 437 187 430 176 424 163 417 152 411 140 404 129 398 116 391 105 385 93 378\\\r\n 82 372 70 365 59 359 46 352 34 335 33 309 33 283 33 257 33 231 33 205 33 179 33 153 33 132 33\r\n";
  const char  stampY[] = "@VERSION 20000\r\n\r\n@SYSENV 2\r\n@TILE 2\r\n@TILELINE 0 10 96 32 416 640 416 992 608 992 608 640 928 32 736 32 512 448 288 32 96 32\r\n@TILEAREA 1 0 337 318 1 223 98 34 105 46 111 59 118 70 124 84 131 95 137 108 144 120 150 133 157 145 163 158 170 169\\\r\n 176 182 183 194 189 207 196 219 202 232 209 243 215 257 222 268 228 281 235 293 241 306 248 317 254 331 261 342 267\\\r\n 355 274 367 280 380 287 392 293 405 300 416 306 429 313 441 319 454 326 466 332 479 339 490 345 504 352 515 358 528\\\r\n 365 540 371 553 378 564 384 578 391 589 397 602 404 614 410 627 416 639 417 656 417 682 417 708 417 734 417 760 417\\\r\n 786 417 812 417 838 417 864 417 890 417 916 417 942 417 968 417 988 417 991 443 991 469 991 495 991 521 991 547 991\\\r\n 573 991 599 991 606 991 607 974 607 948 607 922 607 896 607 870 607 844 607 818 607 792 607 766 607 740 607 714 607\\\r\n 688 607 662 607 642 612 629 619 618 625 604 632 593 638 580 645 568 651 555 658 544 664 530 671 519 677 506 684 494\\\r\n 690 481 697 469 703 456 710 445 716 432 723 420 729 407 736 395 742 382 749 371 755 357 762 346 768 333 775 321 781\\\r\n 308 788 297 794 283 801 272 807 259 814 247 820 234 827 222 833 209 840 198 846 185 853 173 859 160 866 148 872 135\\\r\n 879 124 885 110 892 99 898 86 905 74 911 61 918 50 924 36 926 34 909 33 883 33 857 33 831 33 805 33 779 33 753 33\\\r\n 738 33 730 45 724 57 717 69 711 82 704 93 698 106 691 117 685 130 678 141 672 154 665 165 659 178 652 189 646 202\\\r\n 639 214 633 226 626 238 620 251 613 262 607 275 600 286 594 299 587 310 581 323 574 334 568 347 561 358 555 371 548\\\r\n 383 542 395 535 407 529 420 522 431 516 444 511 449 505 437 498 425 492 412 485 401 479 388 472 377 466 364 459 353\\\r\n 453 340 446 329 440 316 433 305 427 292 420 280 414 268 407 256 401 243 394 232 388 219 381 208 375 195 368 184 362\\\r\n 171 355 160 349 147 342 136 336 123 329 111 323 99 316 87 310 74 303 63 297 50 290 39 288 33 262 33 236 33 210 33\\\r\n 184 33 158 33 132 33 106 33 100 33\r\n";
  const char  stampZ[] = "@VERSION 20000\r\n\r\n@SYSENV 2\r\n@TILE 2\r\n@TILELINE 0 11 160 32 160 192 640 192 128 832 128 992 896 992 896 832 352 832 864 192 864 32 160 32\r\n@TILEAREA 1 0 380 117 1 243 161 34 161 60 161 86 161 112 161 138 161 164 161 188 161 191 187 191 213 191 239 191 265\\\r\n 191 291 191 317 191 343 191 369 191 395 191 421 191 447 191 473 191 499 191 525 191 551 191 577 191 603 191 629 191\\\r\n 639 191 638 196 628 208 620 219 611 229 603 240 592 253 584 264 575 274 567 285 556 298 548 309 539 319 531 330 520\\\r\n 343 512 354 503 364 495 375 484 388 476 399 467 409 459 420 448 433 440 444 431 454 423 465 412 478 404 489 395 499\\\r\n 387 510 376 523 368 534 359 544 351 555 340 568 332 579 323 589 315 600 304 613 296 624 287 634 279 645 268 658 260\\\r\n 669 251 679 243 690 232 703 224 714 215 724 207 735 196 748 188 759 179 769 171 780 160 793 152 804 143 814 135 825\\\r\n 129 832 129 858 129 884 129 910 129 936 129 962 129 988 129 991 155 991 181 991 207 991 233 991 259 991 285 991 311\\\r\n 991 337 991 363 991 389 991 415 991 441 991 467 991 493 991 519 991 545 991 571 991 597 991 623 991 649 991 675 991\\\r\n 701 991 727 991 753 991 779 991 805 991 831 991 857 991 883 991 893 991 895 983 895 957 895 931 895 905 895 879 895\\\r\n 853 895 835 887 833 861 833 835 833 809 833 783 833 757 833 731 833 705 833 679 833 653 833 627 833 601 833 575 833\\\r\n 549 833 523 833 497 833 471 833 445 833 419 833 393 833 367 833 353 833 354 828 364 816 372 805 381 795 389 784 400\\\r\n 771 408 760 417 750 425 739 436 726 444 715 453 705 461 694 472 681 480 670 489 660 497 649 508 636 516 625 525 615\\\r\n 533 604 544 591 552 580 561 570 569 559 580 546 588 535 597 525 605 514 616 501 624 490 633 480 641 469 652 456 660\\\r\n 445 669 435 677 424 688 411 696 400 705 390 713 379 724 366 732 355 741 345 749 334 760 321 768 310 777 300 785 289\\\r\n 796 276 804 265 813 255 821 244 832 231 840 220 849 210 857 199 863 192 863 166 863 140 863 114 863 88 863 62 863\\\r\n 36 863 33 837 33 811 33 785 33 759 33 733 33 707 33 681 33 655 33 629 33 603 33 577 33 551 33 525 33 499 33 473 33\\\r\n 447 33 421 33 395 33 369 33 343 33 317 33 291 33 265 33 239 33 213 33 187 33 163 33\r\n";
  const char tatami1[] = "@VERSION 10000\r\n\r\n@SYSENV 1\r\n@TILE 40\r\n@TILELINE 0 2 0 0 0 128\r\n@TILELINE 1 2 64 128 64 256\r\n@TILELINE 2 2 128 256 128 384\r\n@TILELINE 3 2 192 384 192 512\r\n@TILELINE 4 2 256 512 256 640\r\n@TILELINE 5 2 320 640 320 768\r\n@TILELINE 6 2 384 768 384 896\r\n@TILELINE 7 2 448 896 448 1024\r\n@TILELINE 8 2 256 0 256 128\r\n@TILELINE 9 2 320 128 320 256\r\n@TILELINE 10 2 384 256 384 384\r\n@TILELINE 11 2 448 384 448 512\r\n@TILELINE 12 2 512 512 512 640\r\n@TILELINE 13 2 576 640 576 768\r\n@TILELINE 14 2 640 768 640 896\r\n@TILELINE 15 2 704 896 704 1024\r\n@TILELINE 16 2 512 0 512 128\r\n@TILELINE 17 2 576 128 576 256\r\n@TILELINE 18 2 640 256 640 384\r\n@TILELINE 19 2 704 384 704 512\r\n@TILELINE 20 2 768 512 768 640\r\n@TILELINE 21 2 832 640 832 768\r\n@TILELINE 22 2 896 768 896 896\r\n@TILELINE 23 2 960 896 960 1024\r\n@TILELINE 24 2 768 0 768 128\r\n@TILELINE 25 2 832 128 832 256\r\n@TILELINE 26 2 896 256 896 384\r\n@TILELINE 27 2 960 384 960 512\r\n@TILELINE 28 2 1024 512 1024 640\r\n@TILELINE 29 2 192 1024 192 896\r\n@TILELINE 30 2 128 896 128 768\r\n@TILELINE 31 2 64 768 64 640\r\n@TILELINE 32 2 0 128 1024 128\r\n@TILELINE 33 2 0 256 1024 256\r\n@TILELINE 34 2 0 384 1024 384\r\n@TILELINE 35 2 0 512 1024 512\r\n@TILELINE 36 2 0 640 1024 640\r\n@TILELINE 37 2 0 768 1024 768\r\n@TILELINE 38 2 0 896 1024 896\r\n@TILELINE 39 2 0 1024 1024 1024\r\n";
  const char tatami2[] = "@VERSION 10000\r\n\r\n@SYSENV 1\r\n@TILE 32\r\n@TILELINE 0 2 64 0 64 128\r\n@TILELINE 1 2 320 0 320 128\r\n@TILELINE 2 2 576 0 576 128\r\n@TILELINE 3 2 832 0 832 128\r\n@TILELINE 4 2 192 128 192 256\r\n@TILELINE 5 2 448 128 448 256\r\n@TILELINE 6 2 704 128 704 256\r\n@TILELINE 7 2 960 128 960 256\r\n@TILELINE 8 2 64 256 64 384\r\n@TILELINE 9 2 320 256 320 384\r\n@TILELINE 10 2 576 256 576 384\r\n@TILELINE 11 2 832 256 832 384\r\n@TILELINE 12 2 192 384 192 512\r\n@TILELINE 13 2 448 384 448 512\r\n@TILELINE 14 2 704 384 704 512\r\n@TILELINE 15 2 960 384 960 512\r\n@TILELINE 16 2 64 512 64 640\r\n@TILELINE 17 2 320 512 320 640\r\n@TILELINE 18 2 576 512 576 640\r\n@TILELINE 19 2 832 512 832 640\r\n@TILELINE 20 2 192 640 192 768\r\n@TILELINE 21 2 448 640 448 768\r\n@TILELINE 22 2 704 640 704 768\r\n@TILELINE 23 2 960 640 960 768\r\n@TILELINE 24 2 64 768 64 896\r\n@TILELINE 25 2 320 768 320 896\r\n@TILELINE 26 2 576 768 576 896\r\n@TILELINE 27 2 832 768 832 896\r\n@TILELINE 28 2 192 896 192 1024\r\n@TILELINE 29 2 448 896 448 1024\r\n@TILELINE 30 2 704 896 704 1024\r\n@TILELINE 31 2 960 896 960 1024\r\n";
  const char tatami3[] = "@VERSION 10000\r\n\r\n@SYSENV 1\r\n@TILE 12\r\n@TILELINE 0 2 128 0 128 1024\r\n@TILELINE 1 2 256 0 256 256\r\n@TILELINE 2 2 384 0 384 1024\r\n@TILELINE 3 2 640 0 640 1024\r\n@TILELINE 4 2 768 256 768 0\r\n@TILELINE 5 2 896 0 896 1024\r\n@TILELINE 6 2 0 256 0 512\r\n@TILELINE 7 2 0 768 0 1024\r\n@TILELINE 8 2 256 512 256 768\r\n@TILELINE 9 2 512 256 512 512\r\n@TILELINE 10 2 512 768 512 1024\r\n@TILELINE 11 2 768 512 768 768\r\n";
  const char tatami4[] = "@VERSION 10000\r\n\r\n@SYSENV 2\r\n@TILE 14\r\n@TILELINE 0 2 256 0 256 128\r\n@TILELINE 1 2 768 160 768 0\r\n@TILELINE 2 2 0 128 0 384\r\n@TILELINE 3 2 0 640 0 896\r\n@TILELINE 4 2 256 384 256 640\r\n@TILELINE 5 2 512 160 512 384\r\n@TILELINE 6 2 512 640 512 896\r\n@TILELINE 7 2 768 384 768 640\r\n@TILELINE 8 5 64 0 192 256 64 512 192 768 64 1024\r\n@TILELINE 9 5 448 0 320 256 448 512 320 768 448 1024\r\n@TILELINE 10 5 576 0 704 256 576 512 704 768 576 1024\r\n@TILELINE 11 5 960 1024 832 768 960 512 832 256 960 0\r\n@TILELINE 12 2 768 896 768 1024\r\n@TILELINE 13 2 256 896 256 1024\r\n";
  const char tatami5[] = "@VERSION 10000\r\n\r\n@SYSENV 1\r\n@TILE 40\r\n@TILELINE 0 2 64 0 64 128\r\n@TILELINE 1 2 320 0 320 128\r\n@TILELINE 2 2 576 0 576 128\r\n@TILELINE 3 2 832 0 832 128\r\n@TILELINE 4 2 192 128 192 256\r\n@TILELINE 5 2 448 128 448 256\r\n@TILELINE 6 2 704 128 704 256\r\n@TILELINE 7 2 960 128 960 256\r\n@TILELINE 8 2 64 256 64 384\r\n@TILELINE 9 2 320 256 320 384\r\n@TILELINE 10 2 576 256 576 384\r\n@TILELINE 11 2 832 256 832 384\r\n@TILELINE 12 2 192 384 192 512\r\n@TILELINE 13 2 448 384 448 512\r\n@TILELINE 14 2 704 384 704 512\r\n@TILELINE 15 2 960 384 960 512\r\n@TILELINE 16 2 64 512 64 640\r\n@TILELINE 17 2 320 512 320 640\r\n@TILELINE 18 2 576 512 576 640\r\n@TILELINE 19 2 832 512 832 640\r\n@TILELINE 20 2 192 640 192 768\r\n@TILELINE 21 2 448 640 448 768\r\n@TILELINE 22 2 704 640 704 768\r\n@TILELINE 23 2 960 640 960 768\r\n@TILELINE 24 2 64 768 64 896\r\n@TILELINE 25 2 320 768 320 896\r\n@TILELINE 26 2 576 768 576 896\r\n@TILELINE 27 2 832 768 832 896\r\n@TILELINE 28 2 192 896 192 1024\r\n@TILELINE 29 2 448 896 448 1024\r\n@TILELINE 30 2 704 896 704 1024\r\n@TILELINE 31 2 960 896 960 1024\r\n@TILELINE 32 2 0 128 1024 128\r\n@TILELINE 33 2 0 256 1024 256\r\n@TILELINE 34 2 0 384 1024 384\r\n@TILELINE 35 2 0 512 1024 512\r\n@TILELINE 36 2 0 640 1024 640\r\n@TILELINE 37 2 0 768 1024 768\r\n@TILELINE 38 2 0 896 1024 896\r\n@TILELINE 39 2 0 1024 1024 1024\r\n";
  const char   wave1[] = "@VERSION 10000\r\n\r\n@SYSENV 1\r\n@TILE 5\r\n@TILELINE 0 5 256 0 0 256 256 512 0 768 256 1024\r\n@TILELINE 1 5 512 0 256 256 512 512 256 768 512 1024\r\n@TILELINE 2 4 768 0 512 256 768 512 512 768\r\n@TILELINE 3 2 512 768 768 1024\r\n@TILELINE 4 5 1024 0 768 256 1024 512 768 768 1024 1024\r\n";
  const char   wave2[] = "@VERSION 10000\r\n\r\n@SYSENV 2\r\n@TILE 5\r\n@TILELINE 0 10 256 0 160 128 128 224 128 288 160 384 352 640 384 736 384 800 352 896 256 1024\r\n@TILELINE 1 10 512 0 416 128 384 224 384 288 416 384 608 640 640 736 640 800 608 896 512 1024\r\n@TILELINE 2 10 768 0 672 128 640 224 640 288 672 384 864 640 896 736 896 800 864 896 768 1024\r\n@TILELINE 3 6 0 1024 96 896 128 800 128 736 96 640 0 512\r\n@TILELINE 4 6 1024 0 928 128 896 224 896 288 928 384 1024 512\r\n";
  const char   wave3[] = "@VERSION 10000\r\n\r\n@SYSENV 2\r\n@TILE 11\r\n@TILELINE 0 10 128 0 160 128 128 256 128 320 96 448 96 576 128 736 160 832 160 928 128 1024\r\n@TILELINE 1 8 384 0 384 96 352 160 384 352 352 576 416 704 384 832 384 1024\r\n@TILELINE 2 8 640 0 672 224 640 448 608 512 640 608 672 832 672 928 640 1024\r\n@TILELINE 3 7 896 0 896 160 864 288 896 576 864 704 896 864 896 1024\r\n@TILELINE 4 3 160 128 256 448 352 576\r\n@TILELINE 5 2 640 0 384 352\r\n@TILELINE 6 3 640 608 544 864 384 1024\r\n@TILELINE 7 3 864 288 960 416 1024 736\r\n@TILELINE 8 2 0 736 128 1024\r\n@TILELINE 9 2 864 704 640 1024\r\n@TILELINE 10 2 672 224 896 576\r\n";
  const char   wave4[] = "@VERSION 10000\r\n\r\n@SYSENV 2\r\n@TILE 5\r\n@TILELINE 0 10 128 0 64 96 32 224 32 288 64 416 192 608 224 736 224 800 192 928 128 1024\r\n@TILELINE 1 6 384 512 320 608 288 736 288 800 320 928 384 1024\r\n@TILELINE 2 6 384 512 448 416 480 288 480 224 448 96 384 0\r\n@TILELINE 3 10 640 0 576 128 544 224 544 288 576 384 704 608 736 736 736 800 704 928 640 1024\r\n@TILELINE 4 11 896 0 960 96 992 224 992 288 960 416 896 512 832 608 800 736 800 800 832 928 896 1024\r\n";
  
  const char* names[] = {
    "net1",    "net2",    "net3",    "net4",   "pat01",  "pat02",   "pat03",
    "pat04",   "pat05",   "pat06",   "pat07",  "pat08",  "pat09",   "pat10",
    "pat11",   "pat12",   "pat13",   "pat14",  "pat15",  "pat16",   "pat17",
    "stamp1",  "stamp2",  "stamp3",  "stamp4", "stamp5", "stamp6",  "stamp7",
    "stampA",  "stampB",  "stampC",  "stampD", "stampE", "stampF",  "stampG",
    "stampH",  "stampI",  "stampJ",  "stampK", "stampL", "stampM",  "stampN",
    "stampO",  "stampP",  "stampQ",  "stampR", "stampS", "stampT",  "stampU",
    "stampV",  "stampW",  "stampX",  "stampY", "stampZ", "tatami1", "tatami2",
    "tatami3", "tatami4", "tatami5", "wave1",  "wave2",  "wave3",   "wave4"
  };
  
  const char* datas[] = {
    net1,    net2,    net3,    net4,    pat01,  pat02,  pat03,  pat04,  pat05,  pat06,  pat07,
    pat08,   pat09,   pat10,   pat11,   pat12,  pat13,  pat14,  pat15,  pat16,  pat17,  stamp1,
    stamp2,  stamp3,  stamp4,  stamp5,  stamp6, stamp7, stampA, stampB, stampC, stampD, stampE,
    stampF,  stampG,  stampH,  stampI,  stampJ, stampK, stampL, stampM, stampN, stampO, stampP,
    stampQ,  stampR,  stampS,  stampT,  stampU, stampV, stampW, stampX, stampY, stampZ, tatami1,
    tatami2, tatami3, tatami4, tatami5, wave1,  wave2,  wave3,  wave4
  };

  const std::size_t sizes[] = {
    sizeof(net1),    sizeof(net2),    sizeof(net3),    sizeof(net4),    sizeof(pat01),
    sizeof(pat02),   sizeof(pat03),   sizeof(pat04),   sizeof(pat05),   sizeof(pat06),
    sizeof(pat07),   sizeof(pat08),   sizeof(pat09),   sizeof(pat10),   sizeof(pat11),
    sizeof(pat12),   sizeof(pat13),   sizeof(pat14),   sizeof(pat15),   sizeof(pat16),
    sizeof(pat17),   sizeof(stamp1),  sizeof(stamp2),  sizeof(stamp3),  sizeof(stamp4),
    sizeof(stamp5),  sizeof(stamp6),  sizeof(stamp7),  sizeof(stampA),  sizeof(stampB),
    sizeof(stampC),  sizeof(stampD),  sizeof(stampE),  sizeof(stampF),  sizeof(stampG),
    sizeof(stampH),  sizeof(stampI),  sizeof(stampJ),  sizeof(stampK),  sizeof(stampL),
    sizeof(stampM),  sizeof(stampN),  sizeof(stampO),  sizeof(stampP),  sizeof(stampQ),
    sizeof(stampR),  sizeof(stampS),  sizeof(stampT),  sizeof(stampU),  sizeof(stampV),
    sizeof(stampW),  sizeof(stampX),  sizeof(stampY),  sizeof(stampZ),  sizeof(tatami1),
    sizeof(tatami2), sizeof(tatami3), sizeof(tatami4), sizeof(tatami5), sizeof(wave1),
    sizeof(wave2),   sizeof(wave3),   sizeof(wave4)
  };
  const std::size_t size = sizeof(datas);
  const char* getdata(std::string& name) {
    for (int i = 0, len = (int)size; i < len; i++) {
      if (name == names[i]) {
        return datas[i];  
      }
    }
    return nullptr; 
  }

  pesBuffer getBuffer(std::string& name) { 
    for (int i = 0, len = (int)size; i < len; i++) {
      if (name == names[i]) {
        return pesBuffer(datas[i], sizes[i]);
      }
    }
    return pesBuffer();
  }

}  // namespace embeded_pattern

namespace embeded_motif {
  const char    motif001[] = "@VERSION 30000\r\n\r\n@SYSENV 1230\r\n 2\r\n@LINE 35 0 512 512 512 352 544 224 672 160 832 160 960 320 928 448 800 480 672 512 512 352 448 224 352 160 192 160\\\r\n 64 320 96 448 224 480 352 512 512 544 352 576 224 704 96 864 64 864 192 800 352 672 448 512 512 544 672 576 800 704\\\r\n 928 864 960 864 832 800 672 672 576 512 512 1024 512\r\n";
  const char    motif002[] = "@VERSION 30000\r\n\r\n@SYSENV 1170\r\n 3\r\n@LINE 42 0 539 208 538 472 539 371 384 256 288 96 288 171 416 289 494 469 537 525 383 562 240 461 97 356 243 394 385\\\r\n 469 539 618 494 733 414 805 282 644 283 549 383 472 537 383 685 269 779 107 779 164 662 290 578 471 537 530 674 568\\\r\n 823 479 957 372 829 406 679 472 539 621 570 752 617 827 759 651 758 550 677 472 539 656 538 837 538 1024 539\r\n";
  const char    motif003[] = "@VERSION 30000\r\n\r\n@SYSENV 1100\r\n 3\r\n@LINE 37 0 953 520 879 420 728 299 668 149 647 207 788 331 855 521 879 696 851 801 780 876 643 704 665 585 740 520\\\r\n 877 500 599 383 443 396 241 497 96 601 240 587 444 501 599 316 583 168 461 103 303 248 324 383 444 501 599 681 581\\\r\n 831 460 897 323 715 349 588 437 503 601 521 879 680 896 843 919 1024 953\r\n";
  const char    motif004[] = "@VERSION 30000\r\n\r\n@SYSENV 1850\r\n 3\r\n@LINE 36 0 725 96 671 183 668 348 828 589 827 627 728 608 625 579 527 568 723 481 783 293 671 289 579 471 481 292\\\r\n 484 257 371 399 177 487 176 561 373 557 279 569 191 628 148 819 244 827 327 688 433 866 433 895 519 761 729 684 728\\\r\n 629 632 579 524 607 625 627 725 587 827 719 828 852 829 1024 725\r\n";
  const char    motif005[] = "@VERSION 30000\r\n\r\n@SYSENV 1200\r\n 2\r\n@LINE 49 0 512 192 512 96 544 96 704 256 704 192 768 192 896 320 896 531 584 320 896 384 992 448 992 512 960 576 992\\\r\n 640 960 672 896 640 608 672 896 736 928 864 928 960 832 992 736 960 608 896 512 576 512 352 480 192 512 224 448 160\\\r\n 352 160 192 256 128 352 160 416 288 352 160 416 32 512 32 576 128 608 384 576 128 672 32 768 64 832 192 736 352 832\\\r\n 192 960 192 992 320 960 448 896 512 1024 512\r\n";
  const char    motif006[] = "@VERSION 30000\r\n\r\n@SYSENV 1200\r\n 2\r\n@LINE 52 0 512 320 512 96 416 160 352 64 320 0 224 128 224 160 96 256 160 288 96 352 192 416 416 448 160 512 96 576\\\r\n 160 672 96 672 224 800 256 768 320 672 352 736 416 480 512 704 608 672 672 768 736 672 800 640 928 576 864 512 928\\\r\n 448 800 416 608 352 832 288 928 224 864 160 928 128 832 32 800 64 704 160 672 96 640 224 576 320 512 384 512 192\\\r\n 256 384 512 192 768 416 512 608 768 416 512 576 256 448 512 992 512\r\n";
  const char    motif007[] = "@VERSION 30000\r\n\r\n@SYSENV 1200\r\n 2\r\n@LINE 45 0 512 114 512 151 272 288 64 448 32 417 272 380 392 266 512 380 632 417 752 448 992 288 960 151 752 114 512\\\r\n 266 512 417 512 493 272 569 152 672 128 640 352 531 512 640 672 672 896 569 872 493 752 417 512 531 512 683 512 736\\\r\n 352 844 320 832 448 758 512 834 572 832 704 736 672 683 512 758 512 910 512 960 384 1024 416 986 512 1024 608 960\\\r\n 640 910 512 986 512\r\n";
  const char    motif008[] = "@VERSION 30000\r\n\r\n@SYSENV 950\r\n 2\r\n@LINE 25 0 512 64 512 32 384 96 224 224 96 320 32 416 32 448 128 384 288 288 384 224 448 64 512 224 512 192 672 288\\\r\n 832 384 928 512 992 608 992 608 864 576 736 512 640 384 544 224 512 416 512 640 512\r\n";
  const char    motif009[] = "@VERSION 30000\r\n\r\n@SYSENV 1200\r\n 2\r\n@LINE 27 0 512 96 288 224 96 320 96 448 288 480 416 352 448 192 672 224 896 384 896 480 736 512 512 672 512 832 352\\\r\n 832 96 672 96 544 224 480 416 512 448 704 256 320 640 512 448 614 715 736 896 832 896 928 704 1024 512\r\n";
  const char    motif010[] = "@VERSION 30000\r\n\r\n@SYSENV 1200\r\n 2\r\n@LINE 27 0 512 160 256 320 256 480 512 352 640 224 640 192 544 224 448 352 448 416 576 480 512 640 512 768 448 800\\\r\n 320 704 288 608 352 704 480 640 512 480 512 704 768 832 768 960 640 992 576 832 544 864 480 992 576 1024 512\r\n";
  const char    motif011[] = "@VERSION 30000\r\n\r\n@SYSENV 1200\r\n 3\r\n@LINE 24 115 639 5 761 116 640 241 457 383 321 548 259 708 239 865 240 1017 279 923 476 827 639 673 741 528 777 373\\\r\n 800 219 800 0 759 184 735 359 657 503 561 660 460 824 356 1013 280 920 479 827 639\r\n";
  const char    motif012[] = "@VERSION 30000\r\n\r\n@SYSENV 1200\r\n 2\r\n@LINE 30 0 608 64 672 192 672 256 416 352 256 448 352 576 192 672 288 768 160 832 224 992 160 960 320 896 416 928\\\r\n 544 864 640 768 608 736 768 576 640 576 832 416 736 352 832 192 672 416 544 608 448 768 448 896 320 960 320 896 416\\\r\n 928 544 1024 608\r\n";
  const char    motif013[] = "@VERSION 30000\r\n\r\n@SYSENV 1300\r\n 2\r\n@LINE 22 32 512 96 704 256 832 480 864 352 704 512 736 640 736 576 672 704 672 864 512 704 352 576 352 640 288 512\\\r\n 288 352 320 480 160 256 192 96 320 32 512 384 512 864 512 992 512\r\n";
  const char    motif014[] = "@VERSION 30000\r\n\r\n@SYSENV 1200\r\n 2\r\n@LINE 17 0 1024 512 1024 512 800 896 800 672 512 800 512 608 288 736 288 512 0 288 288 416 288 224 512 352 512 128\\\r\n 800 512 800 512 1024 1024 1024\r\n";
  const char    motif015[] = "@VERSION 30000\r\n\r\n@SYSENV 1600\r\n 2\r\n@LINE 25 0 576 96 704 320 832 512 832 320 768 192 608 192 416 256 320 320 288 384 320 416 512 448 320 512 288 576\\\r\n 320 608 512 640 320 704 288 768 320 832 416 832 608 704 768 512 832 704 832 928 704 1024 576\r\n";
  const char    motif016[] = "@VERSION 30000\r\n\r\n@SYSENV 850\r\n 3\r\n@LINE 45 0 853 111 850 226 850 333 838 416 787 482 689 526 589 581 508 653 428 728 367 821 340 896 325 885 439 832\\\r\n 538 769 609 677 661 588 688 482 693 418 604 324 537 221 508 112 522 0 564 59 647 134 703 226 744 320 744 407 720\\\r\n 480 692 544 593 591 476 583 340 541 237 460 165 381 250 343 349 341 486 399 612 482 692 544 731 619 773 706 811 803\\\r\n 835 916 850 1024 853\r\n";
  const char    motif017[] = "@VERSION 30000\r\n\r\n@SYSENV 1750\r\n 3\r\n@LINE 27 1 133 521 131 523 300 659 237 647 303 913 301 837 380 984 488 852 485 851 607 612 448 683 693 561 697 521\\\r\n 867 477 700 353 699 433 449 184 613 185 492 65 487 204 379 125 304 397 301 372 235 524 301 520 132 1024 133\r\n";
  const char    motif018[] = "@VERSION 30000\r\n\r\n@SYSENV 1000\r\n 1\r\n@LINE 13 0 512 512 512 192 320 128 128 256 0 384 0 512 128 640 0 768 0 896 128 832 320 512 512 1024 512\r\n";
  const char    motif019[] = "@VERSION 30000\r\n\r\n@SYSENV 1000\r\n 1\r\n@LINE 23 0 896 128 896 64 640 64 512 192 512 192 384 256 384 256 128 384 128 384 384 448 384 448 256 704 256 704 384\\\r\n 832 384 832 512 960 512 960 640 64 640 960 640 768 896 128 896 1024 896\r\n";
  const char    motif020[] = "@VERSION 30000\r\n\r\n@SYSENV 1000\r\n 1\r\n@LINE 28 0 896 192 896 128 768 64 640 576 640 640 576 640 512 640 448 576 384 512 384 384 384 320 448 256 448 256\\\r\n 640 256 448 192 448 128 320 128 192 256 64 832 64 896 256 960 256 960 448 896 448 896 832 832 896 192 896 1024 896\r\n";
  const char    motif021[] = "@VERSION 30000\r\n\r\n@SYSENV 1000\r\n 2\r\n@LINE 22 0 960 96 640 256 64 352 640 96 640 352 640 416 960 544 960 576 512 608 64 832 64 896 160 896 448 832 512\\\r\n 576 512 832 512 896 608 896 896 768 960 544 960 768 960 1024 960\r\n";
  const char    motif022[] = "@VERSION 30000\r\n\r\n@SYSENV 1000\r\n 2\r\n@LINE 37 0 512 128 512 288 736 512 896 288 736 128 512 128 512 128 288 128 512 128 288 224 128 128 288 224 128 384\\\r\n 128 224 128 384 128 512 288 384 128 512 288 640 128 512 288 640 128 800 128 640 128 800 128 896 288 800 128 896 288\\\r\n 896 512 896 512 896 288 896 512 736 736 512 896 736 736 896 512 1024 512\r\n";
  const char    motif023[] = "@VERSION 10000\r\n\r\n@SYSENV 1\r\n@LINE 5 0 512 576 512 1 512 576 512 1024 512\r\n";
  const char    motif024[] = "@VERSION 10000\r\n\r\n@SYSENV 2\r\n@LINE 6 0 480 512 480 0 480 512 480 512 1024 512 480\r\n";
  const char    motif025[] = "@VERSION 10000\r\n\r\n@SYSENV 2\r\n@LINE 4 0 480 512 480 512 1024 512 480\r\n";
  const char    motif026[] = "@VERSION 30000\r\n\r\n@SYSENV 1170\r\n 2\r\n@LINE 19 0 960 170 512 0 960 170 512 342 512 170 512 342 512 512 64 342 512 512 64 682 512 512 64 682 512 854 512\\\r\n 682 512 854 512 1024 960 854 512 1024 960\r\n";
  const char    motif027[] = "@VERSION 10000\r\n\r\n@SYSENV 2\r\n@LINE 8 0 896 0 128 512 128 0 128 0 896 512 896 0 896 512 896\r\n";
  const char    motif028[] = "@VERSION 10000\r\n\r\n@SYSENV 2\r\n@LINE 11 0 512 512 32 0 512 352 512 0 512 352 512 864 992 352 512 864 512 352 512 864 512\r\n";
  const char    motif029[] = "@VERSION 10000\r\n\r\n@SYSENV 2\r\n@LINE 6 128 704 512 1024 128 704 896 0 512 352 896 704\r\n";
  const char    motif030[] = "@VERSION 10000\r\n\r\n@SYSENV 1\r\n@LINE 13 0 512 256 512 512 1024 256 512 512 0 256 512 512 512 768 512 1024 1024 768 512 1024 0 768 512 1024 512\r\n";
  const char    motif031[] = "@VERSION 10000\r\n\r\n@SYSENV 1\r\n@LINE 7 0 512 512 0 0 0 512 512 1024 1024 512 1024 1024 512\r\n";
  const char    motif032[] = "@VERSION 30000\r\n\r\n@SYSENV 550\r\n 2\r\n@LINE 12 0 1024 448 320 640 0 448 320 1 1024 448 320 736 704 992 1024 736 704 224 0 736 704 544 1024\r\n";
  const char    motif033[] = "@VERSION 10000\r\n\r\n@SYSENV 2\r\n@LINE 17 0 512 0 0 0 512 0 1024 0 512 256 512 256 0 256 512 512 512 512 0 512 512 512 1024 512 512 768 512 768 1024\\\r\n 768 512 1024 512\r\n";
  const char    motif034[] = "@VERSION 10000\r\n\r\n@SYSENV 1\r\n@LINE 14 0 1024 0 768 0 704 256 320 256 0 256 320 512 704 512 1024 512 704 768 320 768 0 768 320 1024 704 1024 1024\r\n";
  const char    motif035[] = "@VERSION 30000\r\n\r\n@SYSENV 720\r\n 1\r\n@LINE 15 64 256 384 768 192 768 384 768 576 768 384 768 384 1024 384 768 704 256 512 256 704 256 896 256 704 256 704\\\r\n 0 704 256\r\n";
  const char    motif036[] = "@VERSION 30000\r\n\r\n@SYSENV 800\r\n 1\r\n@LINE 19 128 256 384 768 512 1024 384 768 256 1024 384 768 192 768 384 768 576 768 384 768 640 256 768 0 640 256 512\\\r\n 0 640 256 448 256 640 256 832 256 640 256\r\n";
  const char    motif037[] = "@VERSION 30000\r\n\r\n@SYSENV 800\r\n 2\r\n@LINE 15 0 640 320 1024 0 640 320 768 0 640 320 576 0 640 352 320 704 0 352 320 704 192 352 320 704 384 352 320 736\\\r\n 640\r\n";
  const char    motif038[] = "@VERSION 30000\r\n\r\n@SYSENV 1130\r\n 3\r\n@LINE 37 89 371 160 501 236 657 179 796 136 905 88 1024 137 904 180 794 237 656 293 775 349 883 416 1021 347 880 293\\\r\n 775 236 657 308 503 376 364 436 464 511 581 584 473 659 365 612 252 569 139 512 0 571 140 611 253 660 365 716 253\\\r\n 777 128 843 0 776 129 716 252 659 367 720 465 795 582 869 472 933 371\r\n";
  const char    motif039[] = "@VERSION 30000\r\n\r\n@SYSENV 1250\r\n 2\r\n@LINE 29 0 160 224 512 0 160 224 512 448 864 224 512 448 864 448 416 320 288 448 416 576 288 448 416 448 160 448 416\\\r\n 448 864 672 512 448 864 672 512 896 160 672 512 896 160 896 608 768 768 896 608 1024 768 896 608 896 864 896 608\\\r\n 896 160\r\n";
  const char    motif040[] = "@VERSION 30000\r\n\r\n@SYSENV 1200\r\n 2\r\n@LINE 15 0 768 224 992 0 768 288 864 0 768 288 672 0 768 480 256 704 32 480 256 768 160 480 256 768 320 480 256 960\\\r\n 768\r\n";
  const char    motif041[] = "@VERSION 30000\r\n\r\n@SYSENV 700\r\n 3\r\n@LINE 24 280 499 403 792 272 789 405 791 532 788 404 789 404 992 404 791 517 937 401 788 288 940 407 788 643 220 768\\\r\n 216 637 217 501 220 643 221 639 23 639 217 752 72 636 224 527 64 640 219 753 499\r\n";
  const char    motif042[] = "@VERSION 30000\r\n\r\n@SYSENV 1200\r\n 3\r\n@LINE 39 14 584 95 673 173 769 249 847 163 845 76 844 164 845 248 847 329 844 416 845 328 844 246 847 247 728 241\\\r\n 621 245 729 247 848 309 768 385 665 451 576 515 485 584 399 655 309 731 216 644 216 543 216 644 216 731 217 817 217\\\r\n 901 220 817 219 731 219 727 340 727 444 727 338 732 216 812 316 872 393 937 480 1017 584\r\n";
  const char    motif043[] = "@VERSION 30000\r\n\r\n@SYSENV 1130\r\n 1\r\n@LINE 12 0 512 512 512 704 320 320 704 512 512 320 320 704 704 512 512 512 320 512 704 512 512 1024 512\r\n";
  const char    motif044[] = "@VERSION 30000\r\n\r\n@SYSENV 680\r\n 1\r\n@LINE 17 0 768 512 768 512 512 768 512 512 512 256 512 512 512 768 256 256 768 512 512 768 768 256 256 512 512 512\\\r\n 256 512 768 768 768 1024 768\r\n";
  const char    motif045[] = "@VERSION 30000\r\n\r\n@SYSENV 1070\r\n 2\r\n@LINE 47 0 512 192 512 0 512 192 512 416 512 192 512 416 512 576 672 736 832 576 672 416 512 416 288 416 96 416 288\\\r\n 416 512 256 672 96 832 256 672 416 512 256 352 96 192 256 352 416 512 416 736 416 928 416 736 416 512 576 352 736\\\r\n 192 576 352 416 512 640 512 416 512 640 512 832 512 640 512 832 512 928 640 1024 512 928 384 832 512 928 640 1024\\\r\n 512 928 384 832 512 928 640 1024 512\r\n";
  const char    motif046[] = "@VERSION 30000\r\n\r\n@SYSENV 1600\r\n 2\r\n@LINE 45 0 512 256 512 96 352 256 512 96 672 256 512 512 512 352 288 192 96 352 288 128 256 352 288 384 64 352 288\\\r\n 512 512 352 736 128 768 352 736 192 928 352 736 384 960 352 736 512 512 672 288 896 256 672 288 832 96 672 288 640\\\r\n 64 672 288 512 512 672 736 896 768 672 736 832 928 672 736 640 960 672 736 512 512 768 512 928 672 768 512 928 352\\\r\n 768 512 1024 512\r\n";
  const char    motif047[] = "@VERSION 10000\r\n\r\n@SYSENV 1\r\n@LINE 5 0 512 1024 1024 1024 0 0 512 640 512\r\n";
  const char    motif048[] = "@VERSION 10000\r\n\r\n@SYSENV 1\r\n@LINE 6 256 512 1024 512 0 0 256 512 0 1024 1024 512\r\n";
  const char    motif049[] = "@VERSION 10000\r\n\r\n@SYSENV 1\r\n@LINE 9 0 896 640 896 320 640 0 384 320 128 960 128 640 384 320 640 640 896\r\n";
  const char    motif050[] = "@VERSION 30000\r\n\r\n@SYSENV 720\r\n 3\r\n@LINE 15 14 623 256 916 480 916 704 916 480 916 256 916 18 625 251 393 480 135 256 135 15 135 256 135 480 135 253\\\r\n 391 493 623\r\n";
  const char    motif051[] = "@VERSION 10000\r\n\r\n@SYSENV 2\r\n@LINE 13 0 512 256 0 0 512 256 1024 256 512 0 512 256 512 512 512 256 512 256 0 512 512 256 1024 512 512\r\n";
  const char    motif052[] = "@VERSION 10000\r\n\r\n@SYSENV 1\r\n@LINE 11 0 1024 512 1024 512 512 0 512 0 0 512 0 1024 0 1024 512 512 512 512 1024 1024 1024\r\n";
  const char    motif053[] = "@VERSION 30000\r\n\r\n@SYSENV 720\r\n 2\r\n@LINE 11 256 1024 512 864 512 608 256 512 256 192 512 64 736 192 736 512 512 608 512 864 736 1024\r\n";
  const char    motif054[] = "@VERSION 10000\r\n\r\n@SYSENV 2\r\n@LINE 12 0 512 0 928 800 928 800 288 416 288 416 512 608 512 608 736 224 736 224 96 1024 96 1024 512\r\n";
  const char    motif055[] = "@VERSION 30000\r\n\r\n@SYSENV 950\r\n 2\r\n@LINE 47 0 992 160 832 0 992 160 832 320 672 160 832 320 672 512 512 320 672 512 832 704 672 512 832 320 672 128 512\\\r\n 320 352 128 512 320 672 512 512 320 352 160 192 0 32 160 192 320 352 512 512 704 352 512 192 320 352 512 192 704\\\r\n 352 864 192 1024 32 864 192 704 352 896 512 704 672 896 512 704 352 512 512 704 672 512 512 704 672 864 832 704 672\\\r\n 864 832 1024 992 864 832 1024 992\r\n";
  const char    motif056[] = "@VERSION 30000\r\n\r\n@SYSENV 1550\r\n 2\r\n@LINE 43 0 928 0 768 0 640 0 480 0 352 128 352 288 352 416 352 608 352 608 448 608 576 608 672 448 672 288 672 288\\\r\n 512 128 512 128 640 128 768 128 864 288 864 448 864 448 736 448 608 448 512 288 512 288 384 288 256 288 128 416 128\\\r\n 576 128 704 128 864 128 864 256 864 384 864 544 864 672 736 672 608 672 608 800 608 928 736 928 864 928 1024 928\r\n";
  const char    motif057[] = "@VERSION 30000\r\n\r\n@SYSENV 1200\r\n 3\r\n@LINE 23 0 493 33 369 96 283 184 200 283 161 392 161 489 205 585 269 652 372 672 492 508 493 319 495 344 600 434 722\\\r\n 523 788 622 824 728 824 828 783 923 699 990 594 1012 493 849 493 672 493\r\n";
  const char    motif058[] = "@VERSION 10000\r\n\r\n@SYSENV 1\r\n@LINE 7 0 320 1024 320 192 1024 512 0 832 1024 0 320 1024 320\r\n";
  const char    motif059[] = "@VERSION 10000\r\n\r\n@SYSENV 2\r\n@LINE 20 0 512 256 512 928 704 160 192 704 928 512 64 320 928 832 192 96 672 960 512 96 320 832 832 320 96 512 960\\\r\n 704 96 192 832 928 352 64 512 928 608 1024 512\r\n";
  const char    motif060[] = "@VERSION 10000\r\n\r\n@SYSENV 2\r\n@LINE 20 0 512 208 524 754 680 130 264 572 862 416 160 260 862 676 264 78 654 780 524 78 368 676 784 260 186 416 888\\\r\n 572 186 156 784 754 394 52 524 736 544 1024 512\r\n";
  const char    motif061[] = "@VERSION 30000\r\n\r\n@SYSENV 750\r\n 2\r\n@LINE 56 0 512 832 64 800 192 768 0 736 224 704 0 672 224 640 0 608 224 576 32 544 224 256 512 544 896 832 896 800\\\r\n 992 768 864 736 1024 704 832 672 1024 640 832 608 1024 576 864 64 672 96 320 128 832 160 192 192 896 224 160 256\\\r\n 928 288 128 320 960 352 128 384 928 416 192 448 832 480 288 512 672 928 736 896 608 864 768 832 576 800 768 768 576\\\r\n 736 768 704 608 672 736 672 448 704 320 736 512 768 288 800 512 832 288 864 480 896 320 928 448 1024 512\r\n";
  const char    motif062[] = "@VERSION 30000\r\n\r\n@SYSENV 650\r\n 3\r\n@LINE 42 0 513 417 513 114 134 265 134 114 286 341 210 152 399 417 286 265 437 455 399 417 513 114 930 265 930 76\\\r\n 816 341 854 76 703 417 778 152 627 455 665 265 551 417 513 834 134 682 96 834 248 607 172 796 361 531 248 720 437\\\r\n 493 361 569 513 417 513 834 892 682 892 834 778 569 816 796 665 531 740 720 627 531 665 607 589 417 513 1024 513\r\n";
  const char    motif063[] = "@VERSION 30000\r\n\r\n@SYSENV 550\r\n 2\r\n@LINE 22 288 64 640 64 288 64 640 64 640 640 448 832 512 960 512 704 576 992 576 640 640 1024 832 800 800 928 800\\\r\n 704 736 992 736 640 672 1024 672 640 608 1024 640 640 640 64 832 64\r\n";
  const char    motif064[] = "@VERSION 30000\r\n\r\n@SYSENV 1000\r\n 3\r\n@LINE 20 0 512 256 512 281 660 352 791 515 900 721 895 887 800 979 661 1024 512 960 352 887 232 721 124 512 124 380\\\r\n 216 296 356 256 512 480 512 640 512 800 512 1024 512\r\n";
  const char    motif065[] = "@VERSION 10000\r\n\r\n@SYSENV 2\r\n@LINE 23 0 512 0 416 64 256 160 128 320 32 448 0 576 0 704 32 832 96 896 160 992 320 1024 448 1024 608 992 768 928\\\r\n 896 832 992 704 1024 576 1024 448 960 352 864 288 736 256 608 256 512\r\n";
  const char    motif066[] = "@VERSION 10000\r\n\r\n@SYSENV 2\r\n@LINE 20 0 1024 384 960 672 832 928 640 1024 352 864 96 512 0 192 96 0 352 192 96 512 0 864 96 1024 352 928 640 672\\\r\n 832 384 960 0 1024 352 1024 704 1024 1024 1024\r\n";
  const char    motif067[] = "@VERSION 10000\r\n\r\n@SYSENV 1\r\n@LINE 9 0 1024 384 0 192 1024 448 0 256 1024 704 0 512 1024 768 0 576 1024\r\n";
  const char    motif068[] = "@VERSION 10000\r\n\r\n@SYSENV 1\r\n@LINE 9 0 1024 512 0 192 1024 576 0 256 1024 832 0 512 1024 896 0 576 1024\r\n";
  const char    motif069[] = "@VERSION 10000\r\n\r\n@SYSENV 1\r\n@LINE 9 0 1024 576 0 256 1024 640 0 320 1024 960 0 640 1024 1024 0 704 1024\r\n";
  const char    motif070[] = "@VERSION 10000\r\n\r\n@SYSENV 1\r\n@LINE 5 0 1024 896 0 320 1024 960 0 384 1024\r\n";
  const char    motif071[] = "@VERSION 30000\r\n\r\n@SYSENV 750\r\n 2\r\n@LINE 69 0 0 576 32 0 64 576 96 0 128 576 160 0 192 576 224 0 256 576 288 0 320 576 352 0 384 576 416 0 448 576 480\\\r\n 0 512 32 1024 64 448 96 1024 128 448 160 1024 192 448 224 1024 256 448 288 1024 320 448 352 1024 384 448 416 1024\\\r\n 448 448 480 1024 512 448 544 1024 1024 992 448 960 1024 928 448 896 1024 864 448 832 1024 800 448 768 1024 736 448\\\r\n 704 1024 672 448 640 1024 608 448 576 1024 544 448 512 448 0 480 576 512 0 544 576 576 0 608 576 640 0 672 576 704\\\r\n 0 736 576 768 0 800 576 832 0 864 576 896 0 928 576 960 0 992 576 1024 0\r\n";
  const char    motif072[] = "@VERSION 30000\r\n\r\n@SYSENV 750\r\n 2\r\n@LINE 21 0 512 64 864 96 384 160 832 192 256 256 800 288 160 352 768 384 96 448 736 480 64 544 736 576 96 640 736\\\r\n 672 160 736 768 768 256 832 800 864 352 928 864 960 512\r\n";
  const char    motif073[] = "@VERSION 30000\r\n\r\n@SYSENV 1280\r\n 3\r\n@LINE 31 4 681 157 551 3 676 157 551 269 677 155 549 319 427 153 552 321 427 425 556 319 427 496 279 317 428 495 276\\\r\n 643 156 496 279 648 425 492 272 648 424 791 300 649 427 779 551 648 425 777 549 911 445 777 552 893 679 1020 805\\\r\n 895 679 776 549 893 681\r\n";
  const char    motif074[] = "@VERSION 30000\r\n\r\n@SYSENV 1000\r\n 3\r\n@LINE 89 15 572 11 516 44 472 95 449 143 464 181 512 169 568 180 513 145 467 96 448 43 472 12 516 12 572 21 617 45\\\r\n 661 84 699 136 715 183 726 246 732 309 733 364 716 425 683 475 637 504 591 565 551 615 513 668 475 737 424 693 401\\\r\n 727 483 663 428 683 505 621 459 655 540 576 483 611 565 536 516 569 597 503 593 501 537 500 472 501 415 503 351 463\\\r\n 348 541 375 464 397 545 427 463 453 536 488 476 517 501 589 443 551 389 509 336 475 275 425 323 396 272 484 359 429\\\r\n 319 508 393 459 349 535 425 483 384 567 460 509 429 591 503 592 525 644 568 688 627 716 693 732 767 733 831 732 890\\\r\n 725 937 700 976 668 1001 621 1011 572 1009 515 988 472 933 449 876 461 839 511 846 567 839 508 877 461 935 449 988\\\r\n 471 1009 513 1013 572\r\n";
  const char    motif075[] = "@VERSION 30000\r\n\r\n@SYSENV 2000\r\n 3\r\n@LINE 82 0 759 23 813 0 761 29 724 75 711 120 645 191 627 118 646 75 711 85 776 143 816 207 819 272 788 311 723 355\\\r\n 659 417 631 493 655 522 711 567 788 657 816 565 788 524 712 524 585 524 473 407 473 344 503 408 473 343 441 406 473\\\r\n 458 475 355 569 247 472 359 365 457 473 524 473 524 391 524 332 480 273 524 331 573 276 524 331 524 389 413 283 525\\\r\n 181 629 285 524 390 524 472 592 472 645 472 711 448 644 472 712 503 644 472 591 472 703 369 801 473 691 569 592 472\\\r\n 524 472 524 585 524 713 485 787 405 816 485 785 524 712 555 652 623 628 689 667 724 735 760 787 825 811 887 812 931\\\r\n 771 947 705 900 636 831 625 898 636 947 704 992 709 1020 758 993 813 1020 759\r\n";
  const char    motif076[] = "@VERSION 30000\r\n\r\n@SYSENV 3900\r\n 3\r\n@LINE 136 0 415 45 447 107 447 141 413 177 380 231 328 289 327 354 327 412 327 476 380 515 415 475 380 411 328 356\\\r\n 327 289 328 229 327 177 379 142 415 105 447 47 448 0 416 45 384 1 416 45 384 105 384 44 385 105 384 141 415 107 384\\\r\n 140 415 188 463 141 415 187 463 231 502 188 464 231 501 294 503 229 501 296 503 360 504 296 501 360 502 417 503 359\\\r\n 502 416 501 476 459 416 501 477 459 517 416 477 459 437 415 403 387 361 388 336 415 363 447 400 445 439 415 475 380\\\r\n 521 332 540 300 547 260 520 237 499 261 496 304 523 333 563 375 605 416 633 445 668 445 697 416 669 381 635 385 562\\\r\n 461 521 509 496 537 495 575 520 593 548 572 548 540 521 508 475 458 517 416 475 458 516 413 475 379 515 413 560 459\\\r\n 611 501 676 504 737 503 797 504 836 460 876 416 835 457 799 504 735 504 677 504 609 503 563 462 517 417 563 375 515\\\r\n 417 563 374 615 325 563 373 615 324 679 327 609 324 679 325 740 328 676 327 741 327 796 328 740 328 796 325 839 377\\\r\n 795 323 837 377 877 417 835 377 877 419 912 385 972 388 1004 415 973 388 909 387 877 419 913 447 876 417 915 445\\\r\n 968 447 911 445 968 445 1004 416 968 448 1008 415\r\n";
  const char    motif077[] = "@VERSION 30000\r\n\r\n@SYSENV 1800\r\n 3\r\n@LINE 40 0 491 127 520 260 547 384 564 449 619 503 708 383 689 319 629 259 545 144 556 72 617 40 719 128 691 187 636\\\r\n 261 545 327 672 263 785 195 673 257 547 433 509 577 481 720 449 848 428 924 369 965 280 864 299 796 357 720 449 603\\\r\n 420 537 357 500 275 600 293 657 356 720 451 803 324 741 196 661 325 719 448 875 469 1024 491\r\n";
  const char    motif078[] = "@VERSION 30000\r\n\r\n@SYSENV 2800\r\n 3\r\n@LINE 82 0 929 57 917 123 897 181 883 232 865 297 837 359 793 407 753 463 711 523 667 583 621 629 575 635 536 591\\\r\n 512 540 489 455 489 427 516 421 581 437 632 473 700 437 632 420 579 427 515 420 487 389 488 357 523 317 577 328 643\\\r\n 404 672 483 703 399 669 331 645 293 660 248 679 224 720 208 768 228 837 299 839 285 889 347 923 416 923 451 876 480\\\r\n 839 505 804 487 739 475 700 525 665 584 621 625 577 685 548 745 515 827 549 836 641 789 700 728 727 675 736 601 736\\\r\n 540 723 473 699 539 721 603 739 677 737 731 728 789 701 815 744 796 799 752 812 695 829 613 828 545 801 508 744 472\\\r\n 700 511 745 544 801 611 829 667 867 716 885 777 909 814 918 852 924 888 929 911 929\r\n";
  const char    motif079[] = "@VERSION 30000\r\n\r\n@SYSENV 1700\r\n 3\r\n@LINE 49 0 547 27 439 69 327 129 225 211 184 313 187 388 228 444 341 469 447 485 559 572 459 688 361 573 457 485 559\\\r\n 589 555 689 544 776 496 811 421 828 303 836 184 735 185 639 225 549 303 507 413 487 557 403 653 316 749 401 653 484\\\r\n 557 399 553 303 571 227 611 191 707 177 812 173 927 279 923 373 884 449 803 476 699 485 557 528 697 577 805 640 880\\\r\n 725 928 801 925 884 881 953 772 1000 657 1012 547\r\n";
  const char    motif080[] = "@VERSION 30000\r\n\r\n@SYSENV 1900\r\n 3\r\n@LINE 25 0 512 108 507 232 496 409 451 551 375 687 240 733 157 621 155 471 173 357 215 267 265 187 332 127 411 107\\\r\n 507 376 508 583 533 780 608 928 727 997 844 849 849 680 813 524 727 437 653 376 508 1024 512\r\n";
  const char    motif081[] = "@VERSION 30000\r\n\r\n@SYSENV 1950\r\n 3\r\n@LINE 38 12 639 97 561 175 479 272 476 364 489 457 551 521 652 545 772 436 773 347 745 259 675 204 584 175 476 240\\\r\n 407 293 351 359 281 424 215 357 280 293 355 393 419 500 472 603 473 701 443 779 381 835 289 859 180 764 177 664 208\\\r\n 572 267 515 377 500 472 587 520 679 575 785 639 892 705 1004 768 893 707 783 639\r\n";
  const char    motif082[] = "@VERSION 30000\r\n\r\n@SYSENV 1900\r\n 3\r\n@LINE 39 41 544 103 629 200 729 299 797 383 835 463 855 553 857 519 759 459 684 543 739 633 767 728 789 816 791 784\\\r\n 728 711 663 796 661 873 639 941 609 1015 547 953 476 876 441 797 421 711 417 773 367 824 288 735 292 632 313 556\\\r\n 343 481 371 532 291 559 199 468 200 365 237 277 272 188 339 104 439 41 543 443 543 1008 544\r\n";
  const char    motif083[] = "@VERSION 30000\r\n\r\n@SYSENV 2500\r\n 3\r\n@LINE 41 19 581 145 624 271 619 396 400 501 460 604 312 692 393 769 276 844 352 941 319 1015 279 895 477 969 583 748\\\r\n 591 755 675 689 656 624 617 617 708 557 664 500 623 488 685 468 744 407 715 337 667 267 619 500 560 537 463 503 559\\\r\n 579 603 504 559 667 503 703 420 665 501 732 560 667 503 820 460 909 367 819 460 867 501 919 543 968 581\r\n";
  const char    motif084[] = "@VERSION 30000\r\n\r\n@SYSENV 1800\r\n 3\r\n@LINE 25 7 376 76 484 192 567 316 593 445 612 565 611 669 561 732 464 768 375 771 279 747 199 613 325 493 161 375\\\r\n 323 233 197 223 296 228 388 272 484 341 561 443 611 565 612 700 595 815 567 927 483 1007 376\r\n";
  const char    motif085[] = "@VERSION 30000\r\n\r\n@SYSENV 1900\r\n 3\r\n@LINE 31 184 723 185 485 88 459 29 396 16 299 112 320 183 485 261 457 321 409 356 300 243 323 181 484 108 319 184\\\r\n 224 241 321 184 488 184 725 381 724 381 473 381 724 584 723 580 469 527 384 576 304 629 388 581 472 581 724 776 720\\\r\n 776 469 776 724 1003 723\r\n";
  const char    motif086[] = "@VERSION 30000\r\n\r\n@SYSENV 2070\r\n 3\r\n@LINE 49 116 416 176 488 235 548 313 625 373 692 487 813 404 903 439 972 509 917 487 811 536 916 639 951 601 836 483\\\r\n 811 549 817 612 823 679 743 588 719 533 767 484 812 433 752 369 693 312 623 371 559 433 493 507 416 575 355 633 303\\\r\n 696 232 717 115 784 101 837 80 812 184 747 209 693 227 768 212 823 199 905 255 812 297 751 267 693 229 701 148 704\\\r\n 79 619 36 604 139 696 232 633 303 576 353 505 416\r\n";
  const char    motif087[] = "@VERSION 30000\r\n\r\n@SYSENV 1900\r\n 3\r\n@LINE 50 77 874 185 848 281 801 363 757 432 676 461 595 533 497 627 441 743 424 697 528 629 628 529 681 432 677 393\\\r\n 599 321 499 236 440 128 429 167 537 225 624 336 683 433 678 435 543 432 420 431 317 428 217 431 317 430 421 503 343\\\r\n 573 261 483 180 428 73 345 188 269 259 355 340 430 423 435 543 433 678 513 757 591 808 675 849 752 863 848 875 899\\\r\n 777 969 705 896 629 837 516 773 635 696 707 783 781 847 874\r\n";
  const char    motif088[] = "@VERSION 30000\r\n\r\n@SYSENV 2100\r\n 3\r\n@LINE 85 225 780 221 667 177 609 115 598 69 649 76 718 144 750 176 701 144 751 76 715 68 649 115 595 176 610 221 666\\\r\n 263 609 328 598 372 646 355 718 292 750 259 699 292 753 355 718 375 647 325 595 259 610 220 669 223 780 287 842 367\\\r\n 858 444 848 507 822 551 753 551 694 520 631 405 511 307 372 272 312 275 235 317 178 385 147 455 140 536 163 592 200\\\r\n 593 320 543 360 481 372 437 319 449 250 503 214 537 261 506 215 449 250 437 319 481 372 544 360 589 322 649 360 715\\\r\n 371 765 320 751 248 700 211 661 261 700 209 752 248 761 320 713 371 647 362 592 322 593 200 653 160 728 140 804 147\\\r\n 868 182 899 235 893 312 857 382 759 511 653 626 628 694 632 753 673 813 736 849 813 861 896 838 964 780\r\n";
  const char    motif089[] = "@VERSION 30000\r\n\r\n@SYSENV 2490\r\n 3\r\n@LINE 65 0 653 77 597 14 543 5 479 43 423 109 422 149 481 140 551 77 594 136 665 185 713 248 730 326 728 389 698 447\\\r\n 635 492 580 537 520 581 484 635 464 693 464 753 483 801 517 755 562 708 580 631 581 588 557 539 520 493 485 429 464\\\r\n 369 464 324 473 276 511 313 558 360 575 423 577 481 557 539 520 572 475 595 411 596 294 573 240 536 189 499 240 471\\\r\n 294 469 411 492 469 536 519 575 557 600 623 599 726 580 792 548 826 511 788 487 744 484 631 504 569 541 519 595 588\\\r\n 649 658 700 709 761 734 837 735 897 730 961 703 1022 653\r\n";
  const char    motif090[] = "@VERSION 30000\r\n\r\n@SYSENV 1230\r\n 2\r\n@LINE 5 0 1024 1024 0 512 512 0 0 1024 1024\r\n\r\n";
  const char crossstitch[] = "@VERSION 30000\r\n\r\n@SYSENV 1230\r\n 2\r\n@LINE 5 0 0 1024 1024 512 512 0 1024 1024 0\r\n";
  
  const char* names[] = {
    "motif001", "motif002", "motif003", "motif004", "motif005", "motif006", "motif007",
    "motif008", "motif009", "motif010", "motif011", "motif012", "motif013", "motif014",
    "motif015", "motif016", "motif017", "motif018", "motif019", "motif020", "motif021",
    "motif022", "motif023", "motif024", "motif025", "motif026", "motif027", "motif028",
    "motif029", "motif030", "motif031", "motif032", "motif033", "motif034", "motif035",
    "motif036", "motif037", "motif038", "motif039", "motif040", "motif041", "motif042",
    "motif043", "motif044", "motif045", "motif046", "motif047", "motif048", "motif049",
    "motif050", "motif051", "motif052", "motif053", "motif054", "motif055", "motif056",
    "motif057", "motif058", "motif059", "motif060", "motif061", "motif062", "motif063",
    "motif064", "motif065", "motif066", "motif067", "motif068", "motif069", "motif070",
    "motif071", "motif072", "motif073", "motif074", "motif075", "motif076", "motif077",
    "motif078", "motif079", "motif080", "motif081", "motif082", "motif083", "motif084",
    "motif085", "motif086", "motif087", "motif088", "motif089", "motif090", "crossstitch"
  };
  
  const char* datas[] = {
    motif001, motif002, motif003, motif004, motif005, motif006, motif007, motif008, motif009,
    motif010, motif011, motif012, motif013, motif014, motif015, motif016, motif017, motif018,
    motif019, motif020, motif021, motif022, motif023, motif024, motif025, motif026, motif027,
    motif028, motif029, motif030, motif031, motif032, motif033, motif034, motif035, motif036,
    motif037, motif038, motif039, motif040, motif041, motif042, motif043, motif044, motif045,
    motif046, motif047, motif048, motif049, motif050, motif051, motif052, motif053, motif054,
    motif055, motif056, motif057, motif058, motif059, motif060, motif061, motif062, motif063,
    motif064, motif065, motif066, motif067, motif068, motif069, motif070, motif071, motif072,
    motif073, motif074, motif075, motif076, motif077, motif078, motif079, motif080, motif081,
    motif082, motif083, motif084, motif085, motif086, motif087, motif088, motif089, motif090,
    crossstitch
  };

  const std::size_t sizes[] = {
    sizeof(motif001), sizeof(motif002), sizeof(motif003), sizeof(motif004), sizeof(motif005),
    sizeof(motif006), sizeof(motif007), sizeof(motif008), sizeof(motif009), sizeof(motif010),
    sizeof(motif011), sizeof(motif012), sizeof(motif013), sizeof(motif014), sizeof(motif015),
    sizeof(motif016), sizeof(motif017), sizeof(motif018), sizeof(motif019), sizeof(motif020),
    sizeof(motif021), sizeof(motif022), sizeof(motif023), sizeof(motif024), sizeof(motif025),
    sizeof(motif026), sizeof(motif027), sizeof(motif028), sizeof(motif029), sizeof(motif030),
    sizeof(motif031), sizeof(motif032), sizeof(motif033), sizeof(motif034), sizeof(motif035),
    sizeof(motif036), sizeof(motif037), sizeof(motif038), sizeof(motif039), sizeof(motif040),
    sizeof(motif041), sizeof(motif042), sizeof(motif043), sizeof(motif044), sizeof(motif045),
    sizeof(motif046), sizeof(motif047), sizeof(motif048), sizeof(motif049), sizeof(motif050),
    sizeof(motif051), sizeof(motif052), sizeof(motif053), sizeof(motif054), sizeof(motif055),
    sizeof(motif056), sizeof(motif057), sizeof(motif058), sizeof(motif059), sizeof(motif060),
    sizeof(motif061), sizeof(motif062), sizeof(motif063), sizeof(motif064), sizeof(motif065),
    sizeof(motif066), sizeof(motif067), sizeof(motif068), sizeof(motif069), sizeof(motif070),
    sizeof(motif071), sizeof(motif072), sizeof(motif073), sizeof(motif074), sizeof(motif075),
    sizeof(motif076), sizeof(motif077), sizeof(motif078), sizeof(motif079), sizeof(motif080),
    sizeof(motif081), sizeof(motif082), sizeof(motif083), sizeof(motif084), sizeof(motif085),
    sizeof(motif086), sizeof(motif087), sizeof(motif088), sizeof(motif089), sizeof(motif090),
    sizeof(crossstitch)
  };

  const std::size_t size = sizeof(datas);
  const char* getdata(std::string& name) { 
    return nullptr; 
  }

  pesBuffer getBuffer(std::string& name) { 
    for (int i = 0, len = (int)size; i < len; i++) {
      if (name == names[i]) {
        return pesBuffer(datas[i], sizes[i]);
      }
    }
    return pesBuffer();
  }
} // namespace embeded_motif
