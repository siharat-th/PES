//
//  pesSVGTypes.h
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 12/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#ifndef pesSVGTypes_h
#define pesSVGTypes_h

#include <stdio.h>
#include <string>
#include <cstdlib>
#include <algorithm>
#include <vector>
#include <map>
#include<memory>

using namespace std;

typedef int pessvg_colour;
#define pessvg_TRANSPARENT 0x1000000
#ifdef __riscos__
#define pessvg_RGB(r, g, b) ((b) << 16 | (g) << 8 | (r))
#define pessvg_RED(c) ((c) & 0xff)
#define pessvg_GREEN(c) (((c) >> 8) & 0xff)
#define pessvg_BLUE(c) (((c) >> 16) & 0xff)
#else
#define pessvg_RGB(r, g, b) ((r) << 16 | (g) << 8 | (b))
#define pessvg_RED(c) (((c) >> 16) & 0xff)
#define pessvg_GREEN(c) (((c) >> 8) & 0xff)
#define pessvg_BLUE(c) ((c) & 0xff)
#endif

struct pessvg_shape {
    float *path;
    unsigned int path_length;
    char *text;
    float text_x, text_y;
    pessvg_colour fill;
    pessvg_colour stroke;
    int stroke_width;
    bool visibility;    // Jimmy - add visibility to path
    
    int role_type;
    float role_offset;
    
    int fill_type;
    int fill_subtype;
    float fill_angle;
    float fill_size;
    float fill_density;
    float fill_spacing;
    float fill_pitch;
    float fill_pull_compensate;

    int stroke_type;
    int stroke_subtype;
    float stroke_offset;
    float stroke_size;
    float stroke_density;
    float stroke_spacing;
    float stroke_pitch; 
    float stroke_pull_compensate;   
    
    char path_id[128];
    char group_id[128];
};

struct pessvg_diagram {
    int width, height;

    struct pessvg_shape *shape;
    unsigned int shape_count;

    unsigned short error_line;
    const char *error_message;
};

typedef enum {
    pessvg_OK,
    pessvg_OUT_OF_MEMORY,
    pessvg_LIBDOM_ERROR,
    pessvg_NOT_SVG,
    pessvg_SVG_ERROR
} pessvg_code;

enum {
    pessvg_PATH_MOVE,
    pessvg_PATH_CLOSE,
    pessvg_PATH_LINE,
    pessvg_PATH_BEZIER
};

struct pessvg_named_color {
    const char *name;
    pessvg_colour color;
};

template <typename T>
using pesPtr = std::shared_ptr<T>;

//struct pessvgNode;
typedef  enum PES_SVG_TAG_TYPE{
    PES_SVG_TAG_TYPE_CIRCLE,
    PES_SVG_TAG_TYPE_RECT,
    PES_SVG_TAG_TYPE_PATH,
    PES_SVG_TAG_TYPE_IMAGE,
    PES_SVG_TAG_TYPE_GROUP,
    PES_SVG_TAG_TYPE_SVG,
    PES_SVG_TAG_TYPE_DOCUMENT,
    PES_SVG_TAG_TYPE_STYLE,
    PES_SVG_TAG_TYPE_LINE,
}PES_SVG_TAG_TYPE;

/*
 TODO: COMPLETE THESE FOR ALL FORMS CIRCLE, ELLIPSE etc....

 */
typedef struct svgCircleDef_{
    string fill;
    string stroke;
    string fill_opacity;//float 0-1
    string stroke_opacity;//float 0-1
    string stroke_width;
    string cx;
    string cy;
    string r;
    string group_id;
}pessvgCircleDef;


typedef struct svgPathDef_{
    string fill;
    string stroke;
    string fill_opacity;//float 0-1
    string stroke_opacity;//float 0-1
    string stroke_width;
    string stroke_miterlimit;
    string d;
    string visibility;  // Jimmy - add visibility to path
    string fill_type;   
    string fill_subtype;   
    string fill_angle;   
    string fill_density;   
    string fill_size;   
    string fill_spacing;   
    string fill_pitch;   
    string fill_pull_compensate;
    string stroke_type;   
    string stroke_subtype;   
    string stroke_offset;   
    string stroke_size;   
    string stroke_density;   
    string stroke_spacing;   
    string stroke_pitch;  
    string stroke_pull_compensate; 
    string path_id;
    string group_id;
    string role_type;   
    string role_offset;   
}pessvgPathDef;

typedef struct svgRectDef_{
    string fill;
    string stroke;
    string fill_opacity;//float 0-1
    string stroke_opacity;//float 0-1
    string stroke_width;
    string stroke_miterlimit;
    string x;
    string y;
    string width;
    string height;
    string group_id;
}pessvgRectDef;

typedef struct svgLineDef_{
    string stroke;
    string stroke_opacity;//float 0-1
    string stroke_width;
    string x1;
    string y1;
    string x2;
    string y2;
    string group_id;
}pessvgLineDef;

typedef struct svgGroupDef_ {
    string transform;
    string fill;
    string stroke;
    string stroke_width;
    string stroke_miterlimit;
    string fill_opacity;//float 0-1
    string stroke_opacity;//float 0-1
    string group_id;
    // string rawXML;
}pessvgGroupDef;

typedef struct svgImageDef_ {
    string x;
    string y;
    string width;
    string height;
    string base64ImgData;
}pessvgImageDef;

typedef struct svgDef_ {
    string x;
    string y;
    string width;
    string height;
    string viewbox;
    string enable_background;
    //string rawXML;
}pessvgDef;

typedef struct svgNode_{
    pessvgDef svg;
    pessvgGroupDef group;
    pessvgPathDef path;
    pessvgRectDef rect;
    pessvgCircleDef circle;
    pessvgLineDef line;
    pessvgImageDef image;
    vector<pesPtr<svgNode_> >children;//
    PES_SVG_TAG_TYPE type;
}pessvgNode;

typedef struct svgInfo_ {
    pesPtr<pessvgNode> rootnode;//nested nodes reflecting original doc structure, used to save out mod doc
    vector<pesPtr<pessvgNode> >flatlist;//flatlist that corresponds to ofPath ids, used to update nodes when paths changed
    string version;
    string id;
    string xmlns;
    //   string xmlns_ns1;
    string xmlns_xlink;
    string x;
    string y;
    string width;
    string height;
    string viewbox;
    string enable_background;
    string xml_space;
    string preserveAspectRatio;
    string rawXML;

    map<string, string> styles;
}pessvgInfo;

#endif /* pesSVGTypes_h */
