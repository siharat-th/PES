//
//  pesData.hpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 3/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#ifndef pesData_hpp
#define pesData_hpp

#include <stdio.h>
#include "pesRectangle.hpp"
#include "pesColor.hpp"
#include "pesPolyline.hpp"
#include "pesPath.hpp"
#include "pesStitchBlock.hpp"
#include "pesClipper.hpp"
#include "pesBuffer.hpp"
//#include "pesMath.hpp"

#include "include/core/SkCanvas.h"
#include "include/core/SkTypes.h"
#include "include/core/SkData.h"
#include "include/core/SkScalar.h"
#include "include/core/SkSurface.h"
#include "include/core/SkVertices.h"
#include "include/core/SkPath.h"

class pesDocument;

class pesData{
public:
    enum ObjectType {
        OBJECT_TYPE_PES2_TEXT = 0,
        OBJECT_TYPE_PES,
        OBJECT_TYPE_SHAPE,
        OBJECT_TYPE_BACKGROUND,
        OBJECT_TYPE_SCALABLE_SVG_FILE,
        OBJECT_TYPE_SCALABLE_PPEF_TEXT,
        OBJECT_TYPE_SCALABLE_TTF_TEXT,
        OBJECT_TYPE_SCALABLE_SATINCOLUMN,
        OBJECT_TYPE_SCALABLE_PPEF_TEXT_V2, // Monogram Font
                                           // PPES File Version > 503
        OBJECT_TYPE_SCALABLE_CONTAINER,    // Object's container
                                           // Main Purpose: (like as SVG-G Tag)
                                           //   For transformting partial or multiple objects at once 
                                           //   including a translate, rotate and scaling them at the same time.
                                           //
    };
    
    enum StrokeType {
        STROKE_TYPE_NONE = 0,
        STROKE_TYPE_RUNNING,
        STROKE_TYPE_RUNNING_DOUBLE,
        STROKE_TYPE_RUNNING_TRIPLE,
        STROKE_TYPE_RUNNING_BEAN,
        STROKE_TYPE_SATIN,
        STROKE_TYPE_SATIN_OUTER,
        STROKE_TYPE_SATIN_INNER,
        STROKE_TYPE_MOTIF,
        STROKE_TYPE_DYNAMIC = 99
    };
    
    enum FillType {
        FILL_TYPE_NONE = 0,
        FILL_TYPE_NORMAL,
        FILL_TYPE_PATTERN,
        FILL_TYPE_MOTIF,
        FILL_TYPE_SATIN_COLUMN,
        FILL_TYPE_COLOR,
        FILL_TYPE_DISABLE,
        FILL_TYPE_CROSS_STITCH,
        FILL_TYPE_NORMAL2,
        FILL_TYPE_DYNAMIC2 = 98,
        FILL_TYPE_DYNAMIC = 99
    };
    
    enum JoinType{
        JOINT_TYPE_SQUARE = ClipperLib::jtSquare,
        JOINT_TYPE_ROUND = ClipperLib::jtRound,
        JOINT_TYPE_MITER = ClipperLib::jtMiter
    };

    
    class Parameter {
//    struct Parameter {
    public:
        Parameter() {
            type = OBJECT_TYPE_PES2_TEXT;
            text = "Text";
            typeLabel = "Font Stitch";
            tag = "";
            groupId = 0;
            fontIndex = fontSizeIndex = 0;
            colorIndex = 11;
            borderColorIndex = 10;
            baselineIndex = 0;
            shapeIndex = 0;
            radiusValue = 0.5f;
            angleValue = 0.1f;
            rotateValue = 0.5f;
            
            // v 1.00
            extraLetterSpace = 0;
            extraSpace = 0;
            
            // v 1.90
            locked = false;
            visible = true;
            backgroundBuffer.clear();
            //background = "";
            //background_loaded = false;
            convertToStitch = false;
            strokeType = STROKE_TYPE_NONE;
            strokeTypeIndex = 0; // index=0
            strokeRunPitch = 1.0; // running 1mm.
            strokeWidth = 1.0; // 1mm.
            strokeDensity = 2.5; // zigzag frequency
            strokeRunningInset = 0.0;
            isSatinColumnPath = false;
            isPPEFPath = false;
            isTTFPath = false;
            
            strokeJoinType = JOINT_TYPE_SQUARE;
            strokeJoinTypeIndex = 0;
            bound.set(0, 0, 0, 0);
            
            italic = false;
            border = false;
            
            pullCompensate = 0.0;
            fontWeight = 100;
            waveMagnitude = 150;
            slantUpAngle = slantDownAngle = 30;
            fadeUpSlant = fadeDownSlant = 10;
            ppefScaleX = ppefScaleY = 1.0;
            lastFontSizeIndex = 0;
            borderGap = 0;
            borderGapY = 0;
            ppefReverseChar = false;
            ppefReverseSegment = false;
            rotateDegree = 0;
            colors.clear();
            
            fillFading = false;
        }
        
        Parameter(const Parameter & p){
            *this = p;
        }

        //Parameter(const Parameter&) = default;
        Parameter(Parameter&&) = default;
        Parameter& operator=(const Parameter&) = default;
        Parameter& operator=(Parameter&&) = default;            
        
        void setType(ObjectType _type) {
            type = _type;
            
            if (type == OBJECT_TYPE_PES) {
                type = OBJECT_TYPE_PES;
                text = "Text";
                typeLabel = "PES Stitch";
                fontIndex = fontSizeIndex = 0;
                baselineIndex = 0;
                radiusValue = 0.5f;
                angleValue = 0.1f;
                rotateValue = 0.5f;
            } else if (type == OBJECT_TYPE_PES2_TEXT) {
                type = OBJECT_TYPE_PES2_TEXT;
                text = "Text";
                typeLabel = "Font Stitch";
                
                if (fontIndex < 0)
                    fontIndex = 0;
                
                if (fontSizeIndex < 0)
                    fontSizeIndex = 0;
                
                if (colorIndex < 0 || colorIndex>64) {
                    colorIndex = 0;
                }
                
                if (!pesInRange(baselineIndex, 0, 2))
                    baselineIndex = 0;
                
                if (!pesInRange(radiusValue, 0.0, 1.0)) {
                    radiusValue = 0.5f;
                }
                
                if (!pesInRange(angleValue, 0.0, 1.0)) {
                    //radiusValue = 0.1f;
                    angleValue = 0.1f;
                }
                
//                // v 1.00
//                extraLetterSpace = 0;
//                extraSpace = 0;
            } else if (type == OBJECT_TYPE_SHAPE) {
                type = OBJECT_TYPE_SHAPE;
                text = "Text";
                typeLabel = "Shape Stitch";
                fontIndex = fontSizeIndex = 0;
                
                // MARK: v0.8
                if (!pesInRange(baselineIndex, 0, 2))
                    baselineIndex = 0;
                
                radiusValue = 0.5f;
                angleValue = 0.1f;
                rotateValue = 0.5f;
                
                shapeIndex = 0;
                width = 5.0; // 5.0 cm => 50 mm => 500 in pes format
                height = 5.0f;
                lineWidth = 0.2f; // 0.5 cm => 5 mm => 50 in pes format.,apply to zigzag width
                density = 0.4f; // 0.25 => 25 in pes format., apply to zigzag step(zigzag frequency)
            }
            else if (type == OBJECT_TYPE_BACKGROUND) {
                text = "Real Material";
            }
            else if (type >= OBJECT_TYPE_SCALABLE_SVG_FILE && type<=OBJECT_TYPE_SCALABLE_PPEF_TEXT_V2) {
                //text = "Scalable";
                typeLabel = "Scalable Stitch";
//                strokeType = STROKE_TYPE_NONE;
//                strokeTypeIndex = 0; // index=0
//                strokeRunPitch = 1.0; // running 1mm.
//                strokeWidth = 1.0; // 1mm.
//                strokeDensity = 2.5; // zigzag frequency
//                strokeRunningInset = 0.0;
//                colorIndex = 20 - 1; // Black
                radiusValue = -1;
                angleValue = -1;
//                strokeJoinType = JOINT_TYPE_SQUARE;
//                strokeJoinTypeIndex = 0;
//                bound.set(0, 0, 0, 0);
                
                if(type==OBJECT_TYPE_SCALABLE_PPEF_TEXT || type==OBJECT_TYPE_SCALABLE_PPEF_TEXT_V2) isPPEFPath = true;
                else if(type==OBJECT_TYPE_SCALABLE_TTF_TEXT) isTTFPath = true;
                else if(type==OBJECT_TYPE_SCALABLE_SATINCOLUMN) isSatinColumnPath = true;
                
                if(type==OBJECT_TYPE_SCALABLE_SVG_FILE) useColorFromPicker = false;
            }
        }
        
        ObjectType type;
        std::string text;
        std::string typeLabel;
        std::string tag;
        int groupId; // layer-group membership; 0 = ungrouped (see pesGroup)
        int fontIndex;
        int fontSizeIndex;
        int colorIndex;
        int baselineIndex;
        float radiusValue;
        float angleValue;
        float rotateValue;
        int extraLetterSpace;
        int extraSpace;
        bool locked;
        bool visible;
        
        pesBuffer backgroundBuffer;
        
        // shape parameter
        int shapeIndex;
        float width;
        float height;
        float lineWidth;
        float density;
        
        // scalable stitch parameter
        bool convertToStitch;
        StrokeType strokeType;
        int strokeTypeIndex;
        float strokeRunPitch;   // unit mm.
        float strokeWidth;      // unit mm.
        float strokeDensity;
        float strokeRunningInset;
        JoinType strokeJoinType;
        int strokeJoinTypeIndex;
        
        bool isSatinColumnPath;
        bool isPPEFPath;
        bool isTTFPath;
        float pullCompensate; // unit mm.
        int fontWeight; // unit % (Note default=100%)
        pesRectangle bound;
        
        struct motif_t{
            float size = 2.5; // square size unit mm.
            float spacing = 0.0; // unit mm
            float runPitch = 5.0; // unit mm.
            int patternIndex = 0;
            std::string patternName = "motif001"; // fullpath = "Pattern/motif001.pmf"
            pesBuffer patternBuffer;
        }motif;
        
        
        FillType fillType = FILL_TYPE_NONE;
        int fillTypeIndex = 0;
        int fillColorIndex = 11; // Deep Gold
        
        struct fill_t{
            bool underlay = false; //true; //false;
            float density = 2.5;//4.5; // line/mm
            int sewDirection = 0;//-45; // degree
            int presetIndex = 0;
        }fill;
        
        struct patternFill_t{
            bool underlay = false;  //true; //false;
            float density = 2.5;//4.5; // line/mm
            int sewDirection = 0;//-45; // degree
            int patternIndex = 0;
            std::string patternName = "pat01"; 
            float patternSize = 8; // mm
            pesBuffer patternBuffer;
        }patternFill;
        
        
        struct motifFill_t{
            float size = 2.5; // 1.0// square size unit mm.
            float spacing = 0.0; // unit mm
            float rowSpacing = 0.0; // unit mm.
            int patternIndex = 0;
            std::string patternName = "motif001"; // fullpath = "Pattern/motif001.pmf"
            pesBuffer patternBuffer;
        }motifFill;
        
        struct underlayFill_t{
            bool underlay = false;
            float density = 0.2; // line/mm
//            int sewDirection = 90; // degree
            float shrinkSize = 2; // unit mm.
            std::vector<pesPolyline> outlines;
        }underlayFill;
        
        struct autoSatinFill_t{
            bool noneOverlap = false;
        }autoSatinFill;
        
        // v.1.91 (PukChue2)
        bool italic;
        bool border;
        int borderColorIndex;
        int waveMagnitude;
        int slantUpAngle, slantDownAngle; // unit degree
        int fadeUpSlant, fadeDownSlant; // unit percent
        float ppefScaleX=1.0;
        float ppefScaleY=1.0;
        int lastFontSizeIndex;
        int borderGap;
        int borderGapY;
        bool ppefReverseChar;
        bool ppefReverseSegment;
        float rotateDegree;
        
        // pes5
        std::string fontName = "";
        int fontSize = 8;
        int lastFontSize = 8;
        bool useColorFromPicker = true;
        // pes5-monogram
        std::vector<uint8_t> colors;
        
        sk_sp<SkImage> textureBackground;
        
        // pom - fading
        bool fillFading;
    };
    
    
public:
    pesData();
    ~pesData();
    
    void clear();
    
    bool loadPES2TextFromBuffer(const pesBuffer & fontbuff, const pesBuffer & configbuff, std::string textIn, bool center=true);
    
    bool loadPESFromBuffer(const pesBuffer & pesbuff, bool center=true);
    bool loadXXXFromBuffer(const pesBuffer & xxxbuff, bool center=true);
    bool loadDSTFromBuffer(const pesBuffer & dstbuff, bool center=true);
    
    bool loadPINNFromBuffer(const pesBuffer & pinnbuff, bool center=true, bool isPINNFont=false);
    bool loadSVGFromBuffer(const pesBuffer & svgbuff, bool center=true);
    bool loadBackgroundFromBuffer(const pesBuffer & svgbuff, bool center=true);
    bool loadPPEFFromSVGBuffer(const pesBuffer & svgbuff, bool center=false);
    bool loadTTFFromSVGBuffer(const pesBuffer & svgbuff, bool center=false);
    bool loadSatinColumnFromSVGBuffer(const pesBuffer & svgbuff);
    
    bool isSameType(const pesData & other);
    bool canMerge(const pesData & other);
    bool merge(const pesData & newData);
    void calcBoundingBox();
    void recalculate();
    void translate(float tx, float ty);
    void scale(float sx, float sy); // Note: scale from left-top
    void scaleStitches(float sx, float sy);
    void rotate(float degree); // Note: rotate around getBoundingBox().getCenter()
    
    void applyFill();
    void applyPPEFFill();
    void applyPPEF_V2_Fill();
    void applyStroke();
    
    bool isScalable();

    int showPathBoundIndex;
    int showPathIndex;
    
    Parameter parameter;
    Parameter savedParameter;
    
    int numColorChange; // typically equal colors.size()-1 which 0 mean 1 color
    std::vector<uint8_t> colors;

    pesPath PathStrokeToFill( pesPath &spath, float scale=1.0 );
    pesPath unitePaths( pesPath &path1, pesPath &path2 );
    pesPath PathInset( pesPath &tpath, float value );
    pesPath PathOutset( pesPath &tpath, float value );
    SkPath SkiaPathStroke( SkPath &skPath, float value );
    
//    vector<pesPath *> paths;
    std::vector<pesPath> paths;
    std::vector<pesPath> savedPaths;
    pesStitchBlockList fillBlocks;
    pesStitchBlockList strokeBlocks;
    
    void getStitchBlockList(pesStitchBlockList & blocks, bool bJoinBlocksWithColorIndex = false);
    unsigned int getTotalStitches() const;
    unsigned int getTotalJump() const;
//    pesRectangle getBoundingBox() const;
    pesRectangle getBoundingBox();
    pesRectangle getStrokeBoundingBox();
    pesRectangle getScalableBoundingBox() const;
    pesRectangle getRotatedBoundingBox();
    
    pesBuffer getSVGBuffer(bool forScanNcut=false) const; // set forScanNcut=true will set size match brother scan n cut
    
    int centerx, centery, byteLength;
    int minx, miny, maxx, maxy;
    
    void orderByColor();
    // pom
    void reduceColor(int numColor);
    // pom
    void quantizeColor(int numColor);
    
    // flip
    bool canFlip();
    void horizontalFlip(); // flip along y-axis
    void verticalFlip(); // flip along x-axis
    
    void saveParameter(){
        savedParameter = parameter;
    }
    
    void savePaths();

    pesDocument * document;
    
private:
    bool readPECStitch(const unsigned char* pbyte, unsigned int* colors);
    
    void doStrokeRunning(const std::vector<pesPolyline> &stroke, float oneStitch, int strokeBlockIndex, int nRound=1, bool doBean=false);
    void doStrokeSatin(std::vector<pesPolyline> stroke,const std::vector<float> strokeWidth, float inset, float spacing, int strokeBlockIndex);
    
    void calcStitchBoudingBox();
    void calcScalableBoundingBox();
    
    unsigned int totalStitches, totalJump;
    pesRectangle bound;
    pesRectangle strokebound;
    
    // Experiment
    float perimeter_distance(pesVec2f p1, pesVec2f p2, const pesPolyline & outline);
    void connectPointsOnOutline(pesVec2f last, pesVec2f p1, pesVec2f p2, const pesPolyline & outline, int fillBlockIndex);
    void findClosestPointsBetweenOutlines(const pesPolyline & outline1, const pesPolyline & outline2, pesVec2f & closest1, pesVec2f & closest2);
    void connectPointsBetweenOutlines(pesVec2f p1, const pesPolyline & outline1, int fillBlockIndex1, pesVec2f p2, const pesPolyline & outline2, int fillBlockIndex2);
    
    
    void connectPointsOnOutline(pesVec2f last, pesVec2f p1, pesVec2f p2, const pesPolyline & outline, pesStitchBlock & block);
    void connectPointsBetweenOutlines(pesVec2f p1, const pesPolyline & outline1, pesVec2f p2, const pesPolyline & outline2, pesStitchBlock & block);
    
    
    void applyCrossStitchFill();
    void applyCrossStitchFill2();
};


#endif /* pesData_hpp */
