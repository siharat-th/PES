//
//  pesDocument.hpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 12/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#ifndef pesDocument_hpp
#define pesDocument_hpp

// make sure redidect print debug for pes lib
// see: SkLoadUserConfig.h, SkUserConfig.h, SkDebug.h
#ifdef NDEBUG
#define SkDebugf(...)
#endif

#include <stdio.h>
#include "pesBuffer.hpp"
#include "pesData.hpp"

#include "include/core/SkCanvas.h"
#include "include/core/SkTypes.h"

using namespace std;

const int PES_MIN_LIMIT_OBJECT_COUNT = 16;
const int PES_MAX_LIMIT_OBJECT_COUNT = 1024;

// A layer group: folder-like organizational metadata layered over the flat
// object list. A group is NOT a pesData object; membership is per-object via
// pesData::Parameter::groupId == pesGroup::id. Scalability is derived from
// members (a group with any non-scalable object cannot scale), never stored.
struct pesGroup {
    int id = 0;        // stable, 1-based; 0 means "no group"
    int parentId = 0;  // reserved for nesting; always 0 for now
    std::string name;
    bool collapsed = false;
    int order = 0;     // sibling order in the layer panel
};

class pesDocument{
public:
    ~pesDocument();
    
    static pesDocument * getInstance(); // single skeleton
    static pesDocument * createInstance();
    
    std::string getAppName();
    void setAppName(const std::string strMax32);

    void newDocument();
    bool loadPPESFromMemory(uintptr_t /* uint8_t*  */ iptr, size_t length);
//    bool loadPPESFromBuffer(const pesBuffer & ppesBuff) const;
    bool loadPPESFromBuffer(const pesBuffer & ppesBuff);
    bool loadJSONFromBuffer(const pesBuffer & jsonBuff, const pesBuffer & jbinBuff);
    int addObject(pesData & data, bool nolimit = false); // return data object count
    int addObjectToXY(pesData & data, float x, float y, bool nolimit = false); // return data object count
    int classifyObject(pesData & data); // return data object count
    void clipPathForObject(int idx);
    
    bool moveObjectBack(int idx);
    bool moveObjectFront(int idx);
    bool mergeObject(int idx);
    bool duplicateObject(int idx);
    bool deleteObject(int idx);

    // --- Layer groups (organizational metadata; see pesGroup) ---
    const std::vector<pesGroup> & getGroups() const;
    int createGroup(const std::string & name, int parentId = 0); // returns new id
    bool renameGroup(int id, const std::string & name);
    bool deleteGroup(int id);                 // ungroup: clear members, drop entry
    bool setGroupCollapsed(int id, bool collapsed);
    bool setGroupOrder(int id, int order);
    void setObjectGroup(int idx, int groupId); // 0 = remove from any group
    bool isGroupScalable(int id) const;        // false if any member non-scalable
    
    const std::vector<std::shared_ptr<pesData>> & getDataObjects() const;
    std::vector<std::shared_ptr<pesData>> & getDataObjects();
    std::shared_ptr<pesData> getDataObject(int idx) const;
    int getObjectCount() const;
    
    pesData::Parameter& getDataParameter(int idx) const;
    
    void setHoopSizeInMM(float w, float h);
    void setHoopSizeInMM(const pesVec2f & size);
    pesVec2f getHoopSizeInMM() const;
    
    void setTrimAtLength(float mm); // unit mm(default = 8mm)
    float getTrimAtLength(); // unit mm
    
    // MARK: Encode
    pesBuffer getPPESBuffer() const; // project file
    pesBuffer getPES1Buffer() const; // merge all stitches inside document and export .pes buffer
    pesBuffer getPES1Buffer(pesData &mergedData) const;
    pesBuffer getPES5Buffer(bool orderByColor = false, bool center = false) const;
    pesBuffer getPES5Buffer(pesData &mergedData, bool orderByColor = false, bool center = false) const;
    pesBuffer getPINNBuffer() const;
    pesBuffer getPINNBuffer(pesData& mergedData) const;
    pesBuffer getDSTBuffer() const;
    pesBuffer getDSTBuffer(pesData& mergedData) const;
    pesBuffer getEXPBuffer() const;
    pesBuffer getEXPBuffer(pesData& mergedData) const;
    pesBuffer getJEFBuffer() const;
    pesBuffer getJEFBuffer(pesData& mergedData) const;
    pesBuffer getXXXBuffer() const;
    pesBuffer getXXXBuffer(pesData& mergedData) const;
    pesBuffer getGCodeBuffer() const;
    pesBuffer getGCodeBuffer(pesData& mergedData) const;
//    pesBuffer getDXFBuffer() const;
    pesBuffer getJSONBuffer(pesBuffer& data) const;
    pesBuffer getPNGBuffer(bool dpi300 = false) const;
    pesBuffer getThumbnailPNGBuffer(int wmax400 = 200,
                                    int hmax400 = 200,
                                    const int index = -1,
                                    const float lineWidth = 6) const;
    
    pesBuffer getPreviewPNGBuffer(bool dpi300 = false) const;

    pesBuffer exportBufferAs(const std::string as, const std::string options = "") const;
    
    // for repaint cycle purpose
    bool isDirty() const;
    void setDirty(bool dirty);

    pesRectangle getBoundingBox(bool onlyVisible = true, bool onlyStitch = true) const;
    
    sk_sp<SkImage> makePesScalableImageSnapshot(int idx, std::string filter = "") const;
    sk_sp<SkImage> makePesBackgroundImageSnapshot(int idx) const;

    // TODO: should be change name to makePesImageSnapshot or makeStitchImageSnapshot
    sk_sp<SkImage> makeImageSnapshot(pesData& pesdata,
                                     float maxw = -1.0f,
                                     float maxh = -1.0f,
                                     float x = -1000000.0f,
                                     float y = -1000000.0f,
                                     bool dpi300 = false) const;

    sk_sp<SkImage> makePesImageSnapshot(int idx) const;

    void drawPesObject(SkCanvas* canvas, int idx, float scale) const;
    
    void setShowJumpStitch(bool show);
    bool isShowJumpStitch() const;

    // deprecated, use setDrawingMode instead, remove on future
    void setDrawStitchTexture(bool b);
    // deprecated, use getDrawingMode instead, remove on future
    bool isDrawStitchTexture() const;
    
    // 0: Line, 1: Texture(default), 2: Underlay
    void setDrawingMode(int mode);
    int getDrawingMode() const;
    
    int getBackgroundWidth();
    int getBackgroundHeight();
    
//    std::string getLayerTypeString( int i );
//    std::string getLayerImageString( int i );
//    void selectLayer( int i );

    void setDataParameterText(int idx, std::string str);
    void setDataParameterFont(int idx, std::string str);
    void setDataParameterFontSize(int idx, int fontSize);
    void setDataParameterTextEffect(int idx, int effectIndex);
    void setDataParameterTextEffectAngle(int idx, float value);
    void setDataParameterTextEffectRadius(int idx, float value);
    void setDataParameterFillColor( int idx, int colorIndex );
    void setDataParameterFillTypeIndex( int idx, int fillTypeIndex );
    void setDataParameterTextBorder( int idx, bool isBorder );
    void setDataParameterTextBorderGapX(int idx, int value);
    void setDataParameterTextBorderGapY(int idx, int value);
    void setDataParameterTextBorderColor(int idx, int colorIndex);
    void setDataParameterTextItalic(int idx, bool isItalic);
    void setDataParameterTextDensity( int idx, float density );
    void setDataParameterTextPullCompensate( int idx, float pullCompensate );
    void setDataParameterStrokeColor( int idx, int colorIndex );
    void setDataParameterStrokeTypeIndex( int idx, int fillTypeIndex );
    void setDataParameterVisible(int idx, bool visible);
    void setDataParameterLocked(int idx, bool locked);
    
    void setDataParameterTextExtraLetterSpace(int idx, int value);
    void setDataParameterTextExtraSpace(int idx, int value);

    void setDataParameterStrokeRunPitch(int idx, float value);
    void setDataParameterStrokeWidth(int idx, float value);
    void setDataParameterStrokeDensity(int idx, float value);
    void setDataParameterStrokeRunningInset(int idx, float value);
    
    void setDataParameterStrokeMotifSize(int idx, float value);
    void setDataParameterStrokeMotifSpacing(int idx, float value);

    void setDataParameterFillDensity(int idx, float value);
    void setDataParameterFillDirection(int idx, float value);
    
    void setDataParameterFillPatternDensity(int idx, float value);
    void setDataParameterFillPatternDirection(int idx, float value);
    void setDataParameterFillPatternSize(int idx, float value);
    
    void setDataParameterFillMotifSize(int idx, float value);
    void setDataParameterFillMotifSpacing(int idx, float value);
    void setDataParameterFillMotifRowSpacing(int idx, float value);
    
    void setDataParameterFillUnderlay(int idx, bool value);
    void setDataParameterFillPatternUnderlay(int idx, bool value);
    void setDataParameterAutoSatinFillNoneOverlap(int idx, bool value);

    void updateInfo() const;
    
    pesData mergeAll() const;
    
    void setName(const std::string name);
    std::string getName() const;
    
    int getPesMaxObjectCount() const { 
        return pesMaxObjectCount; 
    }

    int setPesMaxObjectCount(int value) {
        if (value < PES_MIN_LIMIT_OBJECT_COUNT) {
            value = PES_MIN_LIMIT_OBJECT_COUNT;
        }
        else if (value > PES_MAX_LIMIT_OBJECT_COUNT) {
            value = PES_MAX_LIMIT_OBJECT_COUNT;
        }
        pesMaxObjectCount = value;
        return pesMaxObjectCount;
    }
    
    void setChanged(bool changed);
    void flagHasChanged();
    bool hasChanged() const;

private:
    pesDocument(); // hidden
    vector<shared_ptr<pesData>> __pesDataList;
    std::vector<pesGroup> __pesGroups; // layer-group registry
    int __nextGroupId = 1;             // monotonic; ids never reused per document
    std::string strAppName;
    std::string strName;

    int pesBackgroundWidth = 100; // default 100 mm.(1cm)
    int pesBackgroundHeight = 100; // default 100 mm.(1cm)
    
    // default
    int pesMaxObjectCount = PES_MIN_LIMIT_OBJECT_COUNT;
    bool pesHasChanged;
};

// C Wrapper
pesDocument * pesGetDocument();
bool loadAssets(bool reload = false);

const float PES_PIXELS_PER_MM = 10;
const float SK_PIXELS_PER_MM = 5;

SkPath pesStitchToSkPath(const pesStitchBlockList & blocks);
SkPath pesRunStitch(SkPath path, float runPitch=1.0); // runPitch default 1.0 mm
SkPath pesFillStitch(SkPath path, pesData::FillType fillType=pesData::FILL_TYPE_NORMAL, float density=2.5);
SkPath pesSatinStitch(SkPath path, float width=1.0, float density=2.5);
pesData pesMergeAllData(const pesDocument& doc);

sk_sp<SkData> SkImageToPngData(sk_sp<SkImage> img, const uint32_t ppm = 0);

namespace embeded_pattern {
  extern const char    net1[];
  extern const char    net2[];
  extern const char    net3[];
  extern const char    net4[];
  extern const char   pat01[];
  extern const char   pat02[];
  extern const char   pat03[];
  extern const char   pat04[];
  extern const char   pat05[];
  extern const char   pat06[];
  extern const char   pat07[];
  extern const char   pat08[];
  extern const char   pat09[];
  extern const char   pat10[];
  extern const char   pat11[];
  extern const char   pat12[];
  extern const char   pat13[];
  extern const char   pat14[];
  extern const char   pat15[];
  extern const char   pat16[];
  extern const char   pat17[];
  extern const char  stamp1[];
  extern const char  stamp2[];
  extern const char  stamp3[];
  extern const char  stamp4[];
  extern const char  stamp5[];
  extern const char  stamp6[];
  extern const char  stamp7[];
  extern const char  stampA[];
  extern const char  stampB[];
  extern const char  stampC[];
  extern const char  stampD[];
  extern const char  stampE[];
  extern const char  stampF[];
  extern const char  stampG[];
  extern const char  stampH[];
  extern const char  stampI[];
  extern const char  stampJ[];
  extern const char  stampK[];
  extern const char  stampL[];
  extern const char  stampM[];
  extern const char  stampN[];
  extern const char  stampO[];
  extern const char  stampP[];
  extern const char  stampQ[];
  extern const char  stampR[];
  extern const char  stampS[];
  extern const char  stampT[];
  extern const char  stampU[];
  extern const char  stampV[];
  extern const char  stampW[];
  extern const char  stampX[];
  extern const char  stampY[];
  extern const char  stampZ[];
  extern const char tatami1[];
  extern const char tatami2[];
  extern const char tatami3[];
  extern const char tatami4[];
  extern const char tatami5[];
  extern const char   wave1[];
  extern const char   wave2[];
  extern const char   wave3[];
  extern const char   wave4[];
  extern const char* names[];
  extern const char* datas[];
  extern const std::size_t sizes[];
  extern const std::size_t size;
  const char* getdata(std::string &name);
  pesBuffer getBuffer(std::string& name);
}

namespace embeded_motif {
  extern const char    motif001[];
  extern const char    motif002[];
  extern const char    motif003[];
  extern const char    motif004[];
  extern const char    motif005[];
  extern const char    motif006[];
  extern const char    motif007[];
  extern const char    motif008[];
  extern const char    motif009[];
  extern const char    motif010[];
  extern const char    motif011[];
  extern const char    motif012[];
  extern const char    motif013[];
  extern const char    motif014[];
  extern const char    motif015[];
  extern const char    motif016[];
  extern const char    motif017[];
  extern const char    motif018[];
  extern const char    motif019[];
  extern const char    motif020[];
  extern const char    motif021[];
  extern const char    motif022[];
  extern const char    motif023[];
  extern const char    motif024[];
  extern const char    motif025[];
  extern const char    motif026[];
  extern const char    motif027[];
  extern const char    motif028[];
  extern const char    motif029[];
  extern const char    motif030[];
  extern const char    motif031[];
  extern const char    motif032[];
  extern const char    motif033[];
  extern const char    motif034[];
  extern const char    motif035[];
  extern const char    motif036[];
  extern const char    motif037[];
  extern const char    motif038[];
  extern const char    motif039[];
  extern const char    motif040[];
  extern const char    motif041[];
  extern const char    motif042[];
  extern const char    motif043[];
  extern const char    motif044[];
  extern const char    motif045[];
  extern const char    motif046[];
  extern const char    motif047[];
  extern const char    motif048[];
  extern const char    motif049[];
  extern const char    motif050[];
  extern const char    motif051[];
  extern const char    motif052[];
  extern const char    motif053[];
  extern const char    motif054[];
  extern const char    motif055[];
  extern const char    motif056[];
  extern const char    motif057[];
  extern const char    motif058[];
  extern const char    motif059[];
  extern const char    motif060[];
  extern const char    motif061[];
  extern const char    motif062[];
  extern const char    motif063[];
  extern const char    motif064[];
  extern const char    motif065[];
  extern const char    motif066[];
  extern const char    motif067[];
  extern const char    motif068[];
  extern const char    motif069[];
  extern const char    motif070[];
  extern const char    motif071[];
  extern const char    motif072[];
  extern const char    motif073[];
  extern const char    motif074[];
  extern const char    motif075[];
  extern const char    motif076[];
  extern const char    motif077[];
  extern const char    motif078[];
  extern const char    motif079[];
  extern const char    motif080[];
  extern const char    motif081[];
  extern const char    motif082[];
  extern const char    motif083[];
  extern const char    motif084[];
  extern const char    motif085[];
  extern const char    motif086[];
  extern const char    motif087[];
  extern const char    motif088[];
  extern const char    motif089[];
  extern const char    motif090[];
  extern const char crossstitch[];
  extern const char* names[];
  extern const char* datas[];
  extern const std::size_t sizes[];
  extern const std::size_t size;
  const char* getdata(std::string& name);
  pesBuffer getBuffer(std::string& name);
}

#endif /* pesDocument_hpp */
