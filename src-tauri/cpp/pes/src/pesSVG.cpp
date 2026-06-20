//
//  pesSVG.cpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 12/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//
#include "pesSVG.hpp"
#include <assert.h>
#include "pesUtility.hpp"
#include "pugixml.hpp"
#include <cstring>

extern void SkDebugf(const char* format, ...);

using namespace std;

//#ifdef _MSC_VER
// long lroundf (float x) {
//    long xl = (long) x;
//    return x - xl > .5 ? xl + 1 : xl;
//}
//#endif

#if defined(__linux__) || defined(_WIN32)
#include <stddef.h>

//extern size_t	strlen (const char*);
//extern PTR	malloc (size_t);
//extern PTR	memcpy (PTR, const PTR, size_t);

char *
strndup (const char *s, size_t n)
{
    char *result;
    size_t len = strlen (s);

    if (n < len)
        len = n;

    result = (char *) malloc (len + 1);
    if (!result)
        return 0;

    result[len] = '\0';
    return (char *) memcpy (result, s, len);
}
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define KAPPA 0.5522847498

//#define     pesToString      pesToString

#if defined(WIN32)
char* strndup(const char* s, size_t n) {
    size_t len = strnlen(s, n);
    char* buffer = (char*)malloc(len + 1);

    if (buffer) {
        buffer[len] = '\0';
        return (char*)memcpy(buffer, s, len);
    } else {
        return NULL;
    }
}
#endif

pesSVG::pesSVG() {
    pesPtr<pessvgNode> rootnode(new pessvgNode());
    rootnode->type = PES_SVG_TAG_TYPE_DOCUMENT;
    info.rootnode = rootnode;
    x = 0;
    y = 0;
    // set default values
    setViewbox(0, 0, 100, 100);
    // this is important on iOS...maybe higher on OSX
    setComplexityThreshold(5000);
}

pesSVG::~pesSVG() {
    paths.clear();
    info.rootnode.reset();
    for (int i = 0; i < (int)info.flatlist.size(); i++) {
        info.flatlist[i].reset();
    }
    info.flatlist.clear();
}

// void pesSVG::load(string path){
//    GUI_Log("pesSVG::load from file\n");
//    path = jsToDataPath(path);
//
//    if(path.compare("") == 0){
//        stringstream ss;
//        ss << path << " does not exist " << endl;
//        GUI_LogError(ss.str());
//        return;
//    }
//
//    jsBuffer buffer = jsBufferFromFile(path);
//    load(buffer);
//}

void pesSVG::loadFromBuffer(const pesBuffer& buffer) {
    // GUI_Log("pesSVG::load from buffer\n");
    parseXML(buffer.getText());
}

// MARK: port svgTiny(POCO_XML) => pugixml
float pessvg_parse_length(const char* s, int viewport_size, const struct pessvg_parse_state state);
void pessvg_parse_color(const char* s, pessvg_colour* c, struct pessvg_parse_state* state);
void pessvg_parse_transform(char* s, float* ma, float* mb, float* mc, float* md, float* me,
                            float* mf);
struct pessvg_shape* pessvg_add_shape(struct pessvg_parse_state* state);
void pessvg_transform_path(float* p, unsigned int n, struct pessvg_parse_state* state);

static pessvg_code pessvg_parse_svg(pessvgInfo& info, pugi::xml_node svg,
                                    struct pessvg_parse_state state, pesPtr<pessvgNode> currnode);
static pessvg_code pessvg_parse_svg(pugi::xml_node svg, struct pessvg_parse_state state);
static pessvg_code pessvg_parse_style(pessvgInfo& info, pugi::xml_node style,
                                      struct pessvg_parse_state state);
static pessvg_code pessvg_parse_path(pugi::xml_node path, struct pessvg_parse_state state);
static pessvg_code pessvg_parse_rect(pugi::xml_node rect, struct pessvg_parse_state state);
static pessvg_code pessvg_parse_circle(pugi::xml_node circle, struct pessvg_parse_state state);
static pessvg_code pessvg_parse_ellipse(pugi::xml_node ellipse, struct pessvg_parse_state state);
static pessvg_code pessvg_parse_line(pugi::xml_node line, struct pessvg_parse_state state);
static pessvg_code pessvg_parse_poly(pugi::xml_node poly, struct pessvg_parse_state state,
                                     bool polygon);
// static pessvg_code pessvg_parse_text(pugi::xml_node text, struct pessvg_parse_state state);
static void pessvg_parse_position_attributes(const pugi::xml_node node,
                                             const struct pessvg_parse_state state, float* x,
                                             float* y, float* w, float* h);
static void pessvg_parse_paint_attributes(const pugi::xml_node node,
                                          struct pessvg_parse_state* state);
static void pessvg_parse_transform_attributes(pugi::xml_node node,
                                              struct pessvg_parse_state* state);
static pessvg_code pessvg_add_path(float* p, unsigned int n, struct pessvg_parse_state* state);

struct pessvg_diagram* pessvg_create(void) {
    struct pessvg_diagram* diagram;

    diagram = (pessvg_diagram*)malloc(sizeof *diagram);
    if (!diagram) return 0;

    diagram->shape = 0;
    diagram->shape_count = 0;
    diagram->error_line = 0;
    diagram->error_message = 0;

    return diagram;
}

void pessvg_free(struct pessvg_diagram* svg) {
    unsigned int i;
    assert(svg);

    for (i = 0; i != svg->shape_count; i++) {
        free(svg->shape[i].path);
        free(svg->shape[i].text);
    }

    free(svg->shape);

    free(svg);
}

struct pessvg_parse_state {
    struct pessvg_diagram* diagram;
    std::shared_ptr<pugi::xml_document> document;

    float viewport_width;
    float viewport_height;

    // borg
    pesPtr<pessvgNode> currNode;  // temp holder
    pessvgInfo* info;

    /* current transformation matrix */
    struct {
        float a, b, c, d, e, f;
    } ctm;

    /*struct css_style style;*/

    /* paint attributes */
    pessvg_colour fill;
    pessvg_colour stroke;
    //    int stroke_width;
    float stroke_width;
    float fill_opacity;

    bool visibility;        // Jimmy - add visibility to path
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

    std::string path_id;
    std::string group_id;

    //    /* gradients */
    //    unsigned int linear_gradient_stop_count;
    //    const char *gradient_x1, *gradient_y1, *gradient_x2, *gradient_y2;
    //    struct pessvg_gradient_stop gradient_stop[svgtiny_MAX_STOPS];
    //    bool gradient_user_space_on_use;
    //    struct {
    //        float a, b, c, d, e, f;
    //    } gradient_transform;
};

pessvg_code pessvg_parse(pessvgInfo& info, struct pessvg_diagram* diagram, const char* buffer,
                         int viewport_width, int viewport_height) {
    assert(diagram);
    assert(buffer);

    struct pessvg_parse_state state;
    float x, y, width, height;
    pessvg_code code = pessvg_OK;
    string rawXMLstring(buffer);

    auto auxDoc = std::make_shared<pugi::xml_document>();
    bool rt = auxDoc->load_string(rawXMLstring.c_str());
    if (!rt) {
        code = pessvg_NOT_SVG;
        return code;
    }

    std::shared_ptr<pugi::xml_document> document = auxDoc;
    pugi::xml_node svg = document->document_element();
    if (strcmp(svg.name(), "svg") != 0) {
        code = pessvg_NOT_SVG;
        return code;
    } else {
        // get graphic dimensions
        state.diagram = diagram;
        state.document = document;
        state.viewport_width = (float)viewport_width;
        state.viewport_height = (float)viewport_height;

        // store root svg info
        info.width = svg.attribute("width").as_string();
        info.height = svg.attribute("height").as_string();
        info.x = svg.attribute("x").as_string();
        info.y = svg.attribute("y").as_string();
        info.viewbox = svg.attribute("viewBox").as_string();
        info.id = svg.attribute("id").as_string();
        info.xmlns = svg.attribute("xmlns").as_string();
        info.xmlns_xlink = svg.attribute("xmlns:xlink").as_string();
        info.version = svg.attribute("version").as_string();
        info.preserveAspectRatio = svg.attribute("preserveAspectRatio").as_string();

        for (const auto& ch : info.width) {
            if (ch == '%') {
                info.width = "";
                break;
            }
        }
        for (const auto& ch : info.height) {
            if (ch == '%') {
                info.height = "";
                break;
            }
        }

        // illustrator check
        if (!(pesToInt(info.width) > 0) && info.viewbox != "") {
            bool hascomma = false;
            for (const auto& ch : info.viewbox) {
                if (ch == ',') {
                    hascomma = true;
                    break;
                }
            }

            vector<string> dims = pesSplitString(info.viewbox, hascomma ? "," : " ");

            if (dims.size() == 4) {
                // guess we got lucky
                // eg.  viewBox="0 0 5000 5000"
                // eg.  viewBox="0,0,5000,5000"

                info.x = dims[0];
                info.y = dims[1];
                info.width = dims[2];
                info.height = dims[3];
                diagram->width = pesToInt(dims[2]);
                ;
                diagram->height = pesToInt(dims[3]);

                state.viewport_width = width = (float)diagram->width;
                state.viewport_height = height = (float)diagram->height;

                //                GUI_LogError()<<"Fixing Illustrator missing width/height
                //                "<<info.width<<" "<<info.height<<"\n";

            } else {
                //                GUI_LogError()<<"Failed to fix Illustrator missing width/height.
                //                Also missing viewBox.\n";
            }
        }

        pessvg_parse_position_attributes(svg, state, &x, &y, &width, &height);

        diagram->width = (int)width;
        diagram->height = (int)height;

        //        state.viewport_width = (width * 254.0 / 72.0);
        //        state.viewport_height = (height * 254.0 / 72.0);
        state.viewport_width = width;
        state.viewport_height = height;
        state.ctm.a = 1; /*(float) viewport_width / (float) width;*/
        state.ctm.b = 0;
        state.ctm.c = 0;
        state.ctm.d = 1; /*(float) viewport_height / (float) height;*/
        state.ctm.e = 0; /*x;*/
        state.ctm.f = 0; /*y;*/
        /*state.style = css_base_style;
         state.style.font_size.value.length.value = option_font_size * 0.1;*/
        state.fill = 0x000000;
        state.stroke = pessvg_TRANSPARENT;
        state.stroke_width = 1;
        state.fill_opacity = 1.0;
        state.visibility = true;        // Jimmy - add visibility to path

        state.role_type = 0;
        state.role_offset = 0;

        state.fill_type = 0;
        state.fill_subtype = 0;
        state.fill_angle = -1;
        state.fill_size = 0;
        state.fill_density = 0;
        state.fill_spacing = 0;
        state.fill_pitch = 0;
        state.fill_pull_compensate = 0;
        
        state.stroke_type = 0;
        state.stroke_subtype = 0;
        state.stroke_offset = 0;
        state.stroke_size = 0;
        state.stroke_density = 0;
        state.stroke_spacing = 0;
        state.stroke_pitch = 0;
        state.stroke_pull_compensate = 0;

        //        state.linear_gradient_stop_count = 0;

        state.info = &info;

        // parse tree
        pesPtr<pessvgNode> rootnode(new pessvgNode());
        rootnode->type = PES_SVG_TAG_TYPE_DOCUMENT;
        info.rootnode = rootnode;

        code = pessvg_parse_svg(info, svg, state, rootnode);
    }

    return code;
}

pessvg_code processChildren(pessvgInfo& info, pesPtr<pessvgNode> node,
                            pugi::xml_object_range<pugi::xml_node_iterator> children,
                            struct pessvg_parse_state state) {
    pessvg_code code = pessvg_OK;
    for (auto it = children.begin(); it != children.end(); ++it) {
        pesPtr<pessvgNode> childnode(new pessvgNode());
        const char* name = (*it).name();

        if (strcmp(name, "svg") == 0) {
            // nested svg
            pessvgDef svgDef;
            svgDef.width = (*it).attribute("width").value();
            svgDef.height = (*it).attribute("height").value();
            svgDef.x = (*it).attribute("x").value();
            svgDef.y = (*it).attribute("y").value();
            svgDef.viewbox = (*it).attribute("viewbox").value();

            childnode->svg = svgDef;
            childnode->type = PES_SVG_TAG_TYPE_SVG;
            node->children.push_back(childnode);

            // recursive call...it seeems svg and groups are treated the same
            code = pessvg_parse_svg(info, (*it), state, childnode);
        } else if (strcmp(name, "style") == 0) {
            code = pessvg_parse_style(info, (*it), state);
        } else if (strcmp(name, "g") == 0) {
            // group
            pessvgGroupDef gDef;
            gDef.transform = (*it).attribute("transform").value();

            gDef.fill = (*it).attribute("fill").value();
            gDef.stroke = (*it).attribute("stroke").value();
            gDef.stroke_width = (*it).attribute("stroke-width").value();
            gDef.stroke_miterlimit = (*it).attribute("stroke-miterlimit").value();
            gDef.fill_opacity = (*it).attribute("fill-opacity").value();
            gDef.stroke_opacity = (*it).attribute("stroke-opacity").value();

            gDef.group_id = (*it).attribute("id").value();
            if( gDef.group_id.length() == 0 ) {
                gDef.group_id = node->group.group_id.c_str();
            }
            

            childnode->group = gDef;
            childnode->type = PES_SVG_TAG_TYPE_GROUP;
            node->children.push_back(childnode);

            // recursive call...it seeems svg and groups are treated the same
            code = pessvg_parse_svg(info, (*it), state, childnode);
        } else if (strcmp(name, "a") == 0) {
            // a node?! who cares
            code = pessvg_parse_svg(info, (*it), state, childnode);
        } else if (strcmp(name, "path") == 0) {
            // path
            pessvgPathDef pDef;
            pDef.d = (*it).attribute("d").value();
            pDef.fill = (*it).attribute("fill").value();
            pDef.stroke = (*it).attribute("stroke").value();
            pDef.stroke_width = (*it).attribute("stroke-width").value();
            pDef.stroke_miterlimit = (*it).attribute("stroke-miterlimit").value();
            pDef.fill_opacity = (*it).attribute("fill-opacity").value();
            pDef.stroke_opacity = (*it).attribute("stroke-opacity").value();
            pDef.visibility = (*it).attribute("visibility").value();    // Jimmy - add visibility to path (0ut)
            pDef.fill_type = (*it).attribute("fill-type").value();      
            pDef.fill_subtype = (*it).attribute("fill-subtype").value();      
            pDef.fill_angle = (*it).attribute("fill-angle").value();      
            pDef.fill_density = (*it).attribute("fill-density").value();      
            pDef.fill_size = (*it).attribute("fill-size").value();      
            pDef.fill_spacing = (*it).attribute("fill-spacing").value();      
            pDef.fill_pitch = (*it).attribute("fill-pitch").value();      
            pDef.fill_pull_compensate = (*it).attribute("fill-pull-compensate").value();
            pDef.stroke_type = (*it).attribute("stroke-type").value();      
            pDef.stroke_subtype = (*it).attribute("stroke-subtype").value();      
            pDef.stroke_offset = (*it).attribute("stroke-offset").value();      
            pDef.stroke_size = (*it).attribute("stroke-size").value();      
            pDef.stroke_density = (*it).attribute("stroke-density").value();      
            pDef.stroke_spacing = (*it).attribute("stroke-spacing").value();      
            pDef.stroke_pitch = (*it).attribute("stroke-pitch").value();      
            pDef.stroke_pull_compensate = (*it).attribute("stroke-pull-compensate").value();
            pDef.role_type = (*it).attribute("role-type").value();      
            pDef.role_offset = (*it).attribute("role-offset").value();      
            pDef.path_id = (*it).attribute("id").value();
            pDef.group_id = (*it).attribute("group-id").value();

            if( node->type == PES_SVG_TAG_TYPE_GROUP ) {
                pDef.group_id = node->group.group_id;
            }

            childnode->path = pDef;
            childnode->type = PES_SVG_TAG_TYPE_PATH;
            node->children.push_back(childnode);

            state.currNode = childnode;
            code = pessvg_parse_path((*it), state);
        } else if (strcmp(name, "rect") == 0) {
            pessvgRectDef pDef;

            pDef.fill = (*it).attribute("fill").value();
            pDef.stroke = (*it).attribute("stroke").value();
            pDef.stroke_width = (*it).attribute("stroke-width").value();
            pDef.stroke_miterlimit = (*it).attribute("stroke-miterlimit").value();
            pDef.fill_opacity = (*it).attribute("fill-opacity").value();
            pDef.stroke_opacity = (*it).attribute("stroke-opacity").value();

            pDef.x = (*it).attribute("x").value();
            pDef.y = (*it).attribute("y").value();
            pDef.width = (*it).attribute("width").value();
            pDef.height = (*it).attribute("height").value();
            

            if( node->type == PES_SVG_TAG_TYPE_GROUP ) {
                pDef.group_id = node->group.group_id;
            }            

            childnode->rect = pDef;
            childnode->type = PES_SVG_TAG_TYPE_RECT;
            node->children.push_back(childnode);

            state.currNode = childnode;
            code = pessvg_parse_rect((*it), state);
        } else if (strcmp(name, "circle") == 0) {
            pessvgCircleDef pDef;

            pDef.fill = (*it).attribute("fill").value();
            pDef.stroke = (*it).attribute("stroke").value();
            pDef.stroke_width = (*it).attribute("stroke-width").value();

            pDef.fill_opacity = (*it).attribute("fill-opacity").value();
            pDef.stroke_opacity = (*it).attribute("stroke-opacity").value();

            pDef.cx = (*it).attribute("cx").value();
            pDef.cy = (*it).attribute("cy").value();
            pDef.r = (*it).attribute("r").value();

            if( node->type == PES_SVG_TAG_TYPE_GROUP ) {
                pDef.group_id = node->group.group_id;
            }               

            childnode->circle = pDef;
            childnode->type = PES_SVG_TAG_TYPE_CIRCLE;
            node->children.push_back(childnode);

            state.currNode = childnode;
            code = pessvg_parse_circle((*it), state);

        } else if (strcmp(name, "ellipse") == 0) {
            if( node->type == PES_SVG_TAG_TYPE_GROUP ) {
                state.group_id = node->group.group_id;
            }
            code = pessvg_parse_ellipse((*it), state);
        } else if (strcmp(name, "line") == 0) {
            pessvgLineDef pDef;

            pDef.stroke = (*it).attribute("stroke").value();
            pDef.stroke_width = (*it).attribute("stroke-width").value();
            pDef.stroke_opacity = (*it).attribute("stroke-opacity").value();

            pDef.x1 = (*it).attribute("x1").value();
            pDef.y1 = (*it).attribute("y1").value();
            pDef.x2 = (*it).attribute("x2").value();
            pDef.y2 = (*it).attribute("y2").value();

            if( node->type == PES_SVG_TAG_TYPE_GROUP ) {
                pDef.group_id = node->group.group_id;
            }               

            childnode->line = pDef;
            childnode->type = PES_SVG_TAG_TYPE_LINE;
            node->children.push_back(childnode);

            state.currNode = childnode;           
            code = pessvg_parse_line((*it), state);
        } else if (strcmp(name, "polyline") == 0) {
            if( node->type == PES_SVG_TAG_TYPE_GROUP ) {
                state.group_id = node->group.group_id;
            }            
            code = pessvg_parse_poly((*it), state, false);
        } else if (strcmp(name, "polygon") == 0) {
            if( node->type == PES_SVG_TAG_TYPE_GROUP ) {
                state.group_id = node->group.group_id;
            }            
            code = pessvg_parse_poly((*it), state, true);
        }

        if (code != pessvg_OK) {
            return code;
        }
    }

    return code;
}

pessvg_code pessvg_parse_svg(pessvgInfo& info, pugi::xml_node svg, struct pessvg_parse_state state,
                             pesPtr<pessvgNode> currnode) {
    float x, y, width, height;
    pugi::xml_attribute view_box;

    pessvg_parse_position_attributes(svg, state, &x, &y, &width, &height);
    pessvg_parse_paint_attributes(svg, &state);
    //    svgtiny_parse_font_attributes(svg, &state);
    view_box = svg.attribute("viewBox");
    if (view_box) {
        // const char *s = (const char *) view_box->children->content;
        const char* s = (const char*)view_box.value();  // ->getValue().c_str();
        float min_x, min_y, vwidth, vheight;
        if (sscanf(s, "%f,%f,%f,%f", &min_x, &min_y, &vwidth, &vheight) == 4 ||
            sscanf(s, "%f %f %f %f", &min_x, &min_y, &vwidth, &vheight) == 4) {
            state.ctm.a = (float)state.viewport_width / vwidth;
            state.ctm.d = (float)state.viewport_height / vheight;
            state.ctm.e += -min_x * state.ctm.a;
            state.ctm.f += -min_y * state.ctm.d;
        }
    }
    pessvg_parse_transform_attributes(svg, &state);

    // Finding Path Title (Boxy SVG)
    pugi::xml_object_range<pugi::xml_node_iterator> children = svg.children();
    for (auto it = children.begin(); it != children.end(); it++) {
        const char* name = (*it).name();
        if (strcmp(name, "title") == 0) {
            auto cv = (*it).child_value();
            if( cv ) {
                string szStyle(cv);
                //SkDebugf( "Path Title: %s\n", szStyle.c_str() );
                state.group_id = std::string( szStyle.c_str() );
            }
        }
    }      

    //pugi::xml_object_range<pugi::xml_node_iterator> children = svg.children();

    pessvg_code code = processChildren(info, currnode, children, state);

    if (code != pessvg_OK) {
        //        GUI_LogWarning("pessvg_parse_svg") << "processChildren fail";
        return code;
    }

    return pessvg_OK;
}

pessvg_code pessvg_parse_style(pessvgInfo& info, pugi::xml_node style,
                               struct pessvg_parse_state state) {
    auto cv = style.child_value();
    string szStyle(cv);
    szStyle = pesTrim(szStyle);
    vector<string> styles = pesSplitString(szStyle, "}", true, true);
    map<string, string> styleTable;
    for (int i = 0; i < styles.size(); i++) {
        vector<string> sep = pesSplitString(styles[i], "{", true, true);
        if (sep.size() == 2) {
            if (pesIsStringStartsWith(sep[0], ".")) {
                sep[0] = sep[0].substr(1);
            }
            styleTable.emplace(sep[0], sep[1]);
        }
    }
    if (styleTable.size()) {
        info.styles.clear();
        info.styles = styleTable;
    }

    return pessvg_OK;
}

void pessvg_parse_position_attributes(const pugi::xml_node node,
                                      const struct pessvg_parse_state state, float* x, float* y,
                                      float* width, float* height) {
    *x = 0;
    *y = 0;
    *width = state.viewport_width;
    *height = state.viewport_height;

    for (auto it = node.attributes_begin(); it != node.attributes_end(); ++it) {
        const char* name = (*it).name();
        const char* content = (*it).value();

        if (strcmp(name, "x") == 0)
            *x = pessvg_parse_length(content, (int)state.viewport_width, state);
        else if (strcmp(name, "y") == 0)
            *y = pessvg_parse_length(content, (int)state.viewport_height, state);
        else if (strcmp(name, "width") == 0)
            *width = pessvg_parse_length(content, (int)state.viewport_width, state);
        else if (strcmp(name, "height") == 0)
            *height = pessvg_parse_length(content, (int)state.viewport_height, state);
    }
}

float pessvg_parse_length(const char* s, int viewport_size, const struct pessvg_parse_state state) {
    int num_length = strspn(s, "0123456789+-.");
    const char* unit = s + num_length;
    float n = (float)atof((const char*)s);
    float font_size = 20; /*css_len2px(&state.style.font_size.value.length, 0);*/

    //    if (unit[0] == 0) {
    //        return n;
    //    } else if (unit[0] == '%') {
    //        return n / 100.0f * viewport_size;
    //    } else if (unit[0] == 'e' && unit[1] == 'm') {
    //        return n * font_size;
    //    } else if (unit[0] == 'e' && unit[1] == 'x') {
    //        return n / 2.0f * font_size;
    //    } else if (unit[0] == 'p' && unit[1] == 'x') {
    //        return n;
    //    } else if (unit[0] == 'p' && unit[1] == 't') {
    //        return n * 1.25f;     //--------------------------- equal dpi/72
    //    } else if (unit[0] == 'p' && unit[1] == 'c') {
    //        return n * 15.0f;     //--------------------------- equal dpi/6
    //    } else if (unit[0] == 'm' && unit[1] == 'm') {
    //        return n * 3.543307f; //--------------------------- equal dpi/25.4
    //    } else if (unit[0] == 'c' && unit[1] == 'm') {
    //        return n * 35.43307f; //--------------------------- equal dpi/2.54
    //    } else if (unit[0] == 'i' && unit[1] == 'n') {
    //        return n * 90;        //--------------------------- equal dpi actually up to
    //        application and/or hardware monitor
    //    }

    //    const float DPI = 90.0; // same of above(tinySVG default value)
    const float DPI = 254.0;  // PES Define Unit 1MM=10, 1CM=100, 1INCH = 2.54CM = 2.54*100 = 254

    if (unit[0] == 0) {
        return n;
    } else if (unit[0] == '%') {
        return n / 100.0f * viewport_size;
    } else if (unit[0] == 'e' && unit[1] == 'm') {
        return n * font_size;
    } else if (unit[0] == 'e' && unit[1] == 'x') {
        return n / 2.0f * font_size;
    } else if (unit[0] == 'p' && unit[1] == 'x') {
        return n;
    } else if (unit[0] == 'p' && unit[1] == 't') {
        float dotPerPT = DPI / 72.0;
        return n * dotPerPT;  //--------------------------- equal dpi/72
    } else if (unit[0] == 'p' && unit[1] == 'c') {
        float dotPerPC = DPI / 6.0;
        return n * dotPerPC;  //--------------------------- equal dpi/6
    } else if (unit[0] == 'm' && unit[1] == 'm') {
        float dotPerMM = DPI / 25.4;
        return n * dotPerMM;  //--------------------------- equal dpi/25.4
    } else if (unit[0] == 'c' && unit[1] == 'm') {
        float dotPerCM = DPI / 2.54;
        return n * dotPerCM;  //--------------------------- equal dpi/2.54
    } else if (unit[0] == 'i' && unit[1] == 'n') {
        return n * DPI;  //--------------------------- equal dpi, actually this value is up to
                         //application and/or monitor
    }

    return 0;
}

void pessvg_parse_paint_attributes(const pugi::xml_node node, struct pessvg_parse_state* state) {
    for (auto it = node.attributes_begin(); it != node.attributes_end(); ++it) {
        const char* name = (*it).name();
        const char* content = (*it).value();
        auto in = (*it).internal_object();
        if (strcmp(name, "visibility") == 0) {      // Jimmy - add visibility to path
            //SkDebugf( "Visibility: %s\n", content );
            if( !strcmp( content, "hidden" ) ) {
                state->visibility = false;
            }
            else {
                state->visibility = true;
            }
        }
        if (strcmp(name, "style") == 0) { // Boxy-svg use style for visibility
            std::vector<std::string> styles = pesSplitString( content, ";", true, true );
            for( std::string style: styles ) {
                std::vector<std::string> ss = pesSplitString( style, ":", true, true );
                if( ss.size() == 2 ) {
                    if( ss[0] == "visibility" ) {
                        if( ss[1] == "hidden" ) {
                            state->visibility = false;
                        }
                        else {
                            state->visibility = true;
                        }
                    }
                    else if( ss[0] == "stroke" ) {
                        pessvg_parse_color(ss[1].c_str(), &state->stroke, state);
                    }
                    else if( ss[0] == "stroke-width" ) {
                        state->stroke_width = pessvg_parse_length(ss[1].c_str(), (int)state->viewport_width, *state);
                    }
                    else if( ss[0] == "fill" ) {
                        pessvg_parse_color(ss[1].c_str(), &state->fill, state);
                    }
                }
            }
        }

        if (strcmp(name, "role-type") == 0 ) {
            state->role_type = atoi(content);
        }
        if (strcmp(name, "role-offset") == 0 ) {
            state->role_offset = atof(content);
        }
        if (strcmp(name, "fill-type") == 0 ) {
            state->fill_type = atoi(content);
        }
        if (strcmp(name, "fill-subtype") == 0 ) {
            state->fill_subtype = atoi(content);
        }
        if (strcmp(name, "fill-angle") == 0 ) {
            state->fill_angle = atof(content);
        }
        if (strcmp(name, "fill-size") == 0 ) {
            state->fill_size = atof(content);
        }
        if (strcmp(name, "fill-density") == 0 ) {
            state->fill_density = atof(content);
        }
        if (strcmp(name, "fill-spacing") == 0 ) {
            state->fill_spacing = atof(content);
        }
        if (strcmp(name, "fill-pitch") == 0 ) {
            state->fill_pitch = atof(content);
        }
        if (strcmp(name, "fill-pull-compensate") == 0 ) {
            state->fill_pull_compensate = atof(content);
        }
        if (strcmp(name, "stroke-type") == 0 ) {
            state->stroke_type = atoi(content);
        }
        if (strcmp(name, "stroke-subtype") == 0 ) {
            state->stroke_subtype = atoi(content);
        }
        if (strcmp(name, "stroke-offset") == 0 ) {
            state->stroke_offset = atof(content);
        }
        if (strcmp(name, "stroke-size") == 0 ) {
            state->stroke_size = atof(content);
        }
        if (strcmp(name, "stroke-density") == 0 ) {
            state->stroke_density = atof(content);
        }
        if (strcmp(name, "stroke-spacing") == 0 ) {
            state->stroke_spacing = atof(content);
        }
        if (strcmp(name, "stroke-pitch") == 0 ) {
            state->stroke_pitch = atof(content);
        }
        if (strcmp(name, "stroke-pull-compensate") == 0 ) {
            state->stroke_pull_compensate = atof(content);
        }

        if (strcmp(name, "id") == 0 ) {      // Jimmy - add fill-type to path
            state->path_id = std::string( content );
        }
        if (strcmp(name, "group-id") == 0 ) {      // Jimmy - add fill-type to path
            state->group_id = std::string( content );
        }
        if (strcmp(name, "fill") == 0)
            pessvg_parse_color(content, &state->fill, state);
        else if (strcmp(name, "fill-opacity") == 0) {
            state->fill_opacity = pessvg_parse_length(content, (int)state->viewport_width, *state);
        } else if (strcmp(name, "stroke") == 0)
            pessvg_parse_color(content, &state->stroke, state);
        else if (strcmp(name, "stroke-width") == 0) {
            state->stroke_width = pessvg_parse_length(content, (int)state->stroke_width, *state);
        } else if (strcmp(name, "class") == 0) {
            //            GUI_LogNotice("pessvg_parse_paint_attributes") << "class=" << content;
            // SkDebugf( "pessvg_parse_paint_attributes Class:%s\n", content);
            auto it = state->info->styles.find(content);
            if (it != state->info->styles.end()) {
                //                GUI_LogNotice("value") << style->second << endl;
                // SkDebugf( "Found class: %s\n", it->second.c_str() );
                const char* style = it->second.c_str();
                const char* s;
                char* value;
                if ((s = strstr(style, "fill:"))) {
                    s += 5;
                    while (*s == ' ') s++;
                    value = strndup(s, strcspn(s, "; "));
                    pessvg_parse_color(value, &state->fill, state);
                    free(value);
                }
                if ((s = strstr(style, "fill-opacity:"))) {
                    s += 13;
                    while (*s == ' ') s++;
                    value = strndup(s, strcspn(s, "; "));
                    state->fill_opacity =
                            (int)pessvg_parse_length(value, (int)state->viewport_width, *state);
                    free(value);
                }
                if ((s = strstr(style, "opacity:"))) {
                    s += 8;
                    while (*s == ' ') s++;
                    value = strndup(s, strcspn(s, "; "));
                    state->fill_opacity =
                            pessvg_parse_length(value, (int)state->viewport_width, *state);
                    // SkDebugf( "Opacity: %0.2f\n", state->fill_opacity );
                    free(value);
                }
                if ((s = strstr(style, "stroke:"))) {
                    s += 7;
                    while (*s == ' ') s++;
                    value = strndup(s, strcspn(s, "; "));
                    pessvg_parse_color(value, &state->stroke, state);
                    free(value);
                }
                if ((s = strstr(style, "stroke-width:"))) {
                    s += 13;
                    while (*s == ' ') s++;
                    value = strndup(s, strcspn(s, "; "));
                    state->stroke_width =
                            (int)pessvg_parse_length(value, (int)state->viewport_width, *state);
                    free(value);
                }
            }
        }
    }
}

void pessvg_parse_color(const char* s, pessvg_colour* c, struct pessvg_parse_state* state) {
    unsigned int r, g, b;
    float rf, gf, bf;
    size_t len = strlen(s);
    char *id = 0, *rparen;

    if (len == 4 && s[0] == '#') {
        if (sscanf(s + 1, "%1x%1x%1x", &r, &g, &b) == 3)
            *c = pessvg_RGB(r | r << 4, g | g << 4, b | b << 4);

    } else if (len == 7 && s[0] == '#') {
        if (sscanf(s + 1, "%2x%2x%2x", &r, &g, &b) == 3) *c = pessvg_RGB(r, g, b);

    } else if (10 <= len && s[0] == 'r' && s[1] == 'g' && s[2] == 'b' && s[3] == '(' &&
               s[len - 1] == ')') {
        if (sscanf(s + 4, "%u,%u,%u", &r, &g, &b) == 3)
            *c = pessvg_RGB(r, g, b);
        else if (sscanf(s + 4, "%f%%,%f%%,%f%%", &rf, &gf, &bf) == 3) {
            b = (unsigned int)(bf * 255 / 100);
            g = (unsigned int)(gf * 255 / 100);
            r = (unsigned int)(rf * 255 / 100);
            *c = pessvg_RGB(r, g, b);
        }

    } else if (len == 4 && strcmp(s, "none") == 0) {
        *c = pessvg_TRANSPARENT;

    } else if (5 < len && s[0] == 'u' && s[1] == 'r' && s[2] == 'l' && s[3] == '(') {
        if (s[4] == '#') {
            id = strdup(s + 5);
            if (!id) return;
            rparen = strchr(id, ')');
            if (rparen) *rparen = 0;
            //            svgtiny_find_gradient(id, state);
            free(id);
            //            fprintf(stderr, "linear_gradient_stop_count %i\n",
            //            state->linear_gradient_stop_count); if (state->linear_gradient_stop_count
            //            == 0)
            //                *c = svgtiny_TRANSPARENT;
            //            else if (state->linear_gradient_stop_count == 1)
            //                *c = state->gradient_stop[0].color;
            //            else
            //                *c = svgtiny_LINEAR_GRADIENT;
        }

    } else {
        const struct pessvg_named_color* named_color = NULL;
        //        named_color = pessvg_color_lookup(s, strlen(s));
        if (named_color)
            *c = named_color->color;
        else {
            //            GUI_LogWarning("pessvg_parse_color") << "need color lookup table";
        }
    }
}

void pessvg_parse_transform_attributes(pugi::xml_node node, struct pessvg_parse_state* state) {
    char* transform = (char*)node.attribute("transform").value();
    if (transform) {
        pessvg_parse_transform(transform, &state->ctm.a, &state->ctm.b, &state->ctm.c,
                               &state->ctm.d, &state->ctm.e, &state->ctm.f);
    }
}

void pessvg_parse_transform(char* s, float* ma, float* mb, float* mc, float* md, float* me,
                            float* mf) {
    float a, b, c, d, e, f;
    float za, zb, zc, zd, ze, zf;
    float angle, x, y;
    int n;
    unsigned int i;

    for (i = 0; s[i]; i++)
        if (s[i] == ',') s[i] = ' ';

    while (*s) {
        a = d = 1;
        b = c = 0;
        e = f = 0;
        if (sscanf(s, "matrix (%f %f %f %f %f %f) %n", &a, &b, &c, &d, &e, &f, &n) == 6)
            ;
        else if (sscanf(s, "translate (%f %f) %n", &e, &f, &n) == 2)
            ;
        else if (sscanf(s, "translate (%f) %n", &e, &n) == 1)
            ;
        else if (sscanf(s, "scale (%f %f) %n", &a, &d, &n) == 2)
            ;
        else if (sscanf(s, "scale (%f) %n", &a, &n) == 1)
            d = a;
        else if (sscanf(s, "rotate (%f %f %f) %n", &angle, &x, &y, &n) == 3) {
            angle = (float)(angle / 180 * M_PI);
            a = cos(angle);
            b = sin(angle);
            c = -sin(angle);
            d = cos(angle);
            e = -x * cos(angle) + y * sin(angle) + x;
            f = -x * sin(angle) - y * cos(angle) + y;
        } else if (sscanf(s, "rotate (%f) %n", &angle, &n) == 1) {
            angle = (float)(angle / 180 * M_PI);
            a = cos(angle);
            b = sin(angle);
            c = -sin(angle);
            d = cos(angle);
        } else if (sscanf(s, "skewX (%f) %n", &angle, &n) == 1) {
            angle = (float)(angle / 180 * M_PI);
            c = tan(angle);
        } else if (sscanf(s, "skewY (%f) %n", &angle, &n) == 1) {
            angle = (float)(angle / 180 * M_PI);
            b = tan(angle);
        } else
            break;
        za = *ma * a + *mc * b;
        zb = *mb * a + *md * b;
        zc = *ma * c + *mc * d;
        zd = *mb * c + *md * d;
        ze = *ma * e + *mc * f + *me;
        zf = *mb * e + *md * f + *mf;
        *ma = za;
        *mb = zb;
        *mc = zc;
        *md = zd;
        *me = ze;
        *mf = zf;
        s += n;
    }
}

pessvg_code pessvg_parse_path(pugi::xml_node path, struct pessvg_parse_state state) {
    char *s, *path_d;
    float* p;
    unsigned int i;
    float last_x = 0, last_y = 0;
    float last_cubic_x = 0, last_cubic_y = 0;
    float last_quad_x = 0, last_quad_y = 0;

    // Jimmy - if component is hide, just ignore it
    string displayState = path.attribute("display").value();
    if (displayState == "none") return pessvg_OK;
    //-- Jimmy

    pessvg_parse_paint_attributes(path, &state);
    pessvg_parse_transform_attributes(path, &state);

    if( state.currNode->path.group_id.size() ) {
        state.group_id = state.currNode->path.group_id;
    }

    s = path_d = (char*)path.attribute("d").value();
    if (!s) {
        // state.diagram->error_line = path->line;
        state.diagram->error_message = "path: missing d attribute";
        return pessvg_SVG_ERROR;
    }

    p = (float*)malloc(sizeof p[0] * strlen(s));
    if (!p) return pessvg_OUT_OF_MEMORY;

    // parse d and build path
    for (i = 0; s[i]; i++)
        if (s[i] == ',') s[i] = ' ';

    i = 0;
    while (*s) {
        char command[2];
        int plot_command;
        float x, y, x1, y1, x2, y2, rx, ry, rotation, large_arc, sweep;
        int n;

        /* moveto (M, m), lineto (L, l) (2 arguments) */
        if (sscanf(s, " %1[MmLl] %f %f %n", command, &x, &y, &n) == 3) {
            /*LOG(("moveto or lineto"));*/
            if (*command == 'M' || *command == 'm')
                plot_command = pessvg_PATH_MOVE;
            else
                plot_command = pessvg_PATH_LINE;
            do {
                p[i++] = (float)plot_command;
                if ('a' <= *command) {
                    x += last_x;
                    y += last_y;
                }
                p[i++] = last_cubic_x = last_quad_x = last_x = x;
                p[i++] = last_cubic_y = last_quad_y = last_y = y;
                s += n;
                //plot_command = pessvg_PATH_LINE;
            } while (sscanf(s, "%f %f %n", &x, &y, &n) == 2);

            /* closepath (Z, z) (no arguments) */
        } else if (sscanf(s, " %1[Zz] %n", command, &n) == 1) {
            /*LOG(("closepath"));*/
            p[i++] = pessvg_PATH_CLOSE;
            s += n;

            /* horizontal lineto (H, h) (1 argument) */
        } else if (sscanf(s, " %1[Hh] %f %n", command, &x, &n) == 2) {
            /*LOG(("horizontal lineto"));*/
            do {
                p[i++] = pessvg_PATH_LINE;
                if (*command == 'h') x += last_x;
                p[i++] = last_cubic_x = last_quad_x = last_x = x;
                p[i++] = last_cubic_y = last_quad_y = last_y;
                s += n;
            } while (sscanf(s, "%f %n", &x, &n) == 1);

            /* vertical lineto (V, v) (1 argument) */
        } else if (sscanf(s, " %1[Vv] %f %n", command, &y, &n) == 2) {
            /*LOG(("vertical lineto"));*/
            do {
                p[i++] = pessvg_PATH_LINE;
                if (*command == 'v') y += last_y;
                p[i++] = last_cubic_x = last_quad_x = last_x;
                p[i++] = last_cubic_y = last_quad_y = last_y = y;
                s += n;
            } while (sscanf(s, "%f %n", &y, &n) == 1);

            /* curveto (C, c) (6 arguments) */
        } else if (sscanf(s, " %1[Cc] %f %f %f %f %f %f %n", command, &x1, &y1, &x2, &y2, &x, &y,
                          &n) == 7) {
            /*LOG(("curveto"));*/
            do {
                p[i++] = pessvg_PATH_BEZIER;
                if (*command == 'c') {
                    x1 += last_x;
                    y1 += last_y;
                    x2 += last_x;
                    y2 += last_y;
                    x += last_x;
                    y += last_y;
                }
                p[i++] = x1;
                p[i++] = y1;
                p[i++] = last_cubic_x = x2;
                p[i++] = last_cubic_y = y2;
                p[i++] = last_quad_x = last_x = x;
                p[i++] = last_quad_y = last_y = y;
                s += n;
            } while (sscanf(s, "%f %f %f %f %f %f %n", &x1, &y1, &x2, &y2, &x, &y, &n) == 6);

            /* shorthand/smooth curveto (S, s) (4 arguments) */
        } else if (sscanf(s, " %1[Ss] %f %f %f %f %n", command, &x2, &y2, &x, &y, &n) == 5) {
            /*LOG(("shorthand/smooth curveto"));*/
            do {
                p[i++] = pessvg_PATH_BEZIER;
                x1 = last_x + (last_x - last_cubic_x);
                y1 = last_y + (last_y - last_cubic_y);
                if (*command == 's') {
                    x2 += last_x;
                    y2 += last_y;
                    x += last_x;
                    y += last_y;
                }
                p[i++] = x1;
                p[i++] = y1;
                p[i++] = last_cubic_x = x2;
                p[i++] = last_cubic_y = y2;
                p[i++] = last_quad_x = last_x = x;
                p[i++] = last_quad_y = last_y = y;
                s += n;
            } while (sscanf(s, "%f %f %f %f %n", &x2, &y2, &x, &y, &n) == 4);

            /* quadratic Bezier curveto (Q, q) (4 arguments) */
        } else if (sscanf(s, " %1[Qq] %f %f %f %f %n", command, &x1, &y1, &x, &y, &n) == 5) {
            /*LOG(("quadratic Bezier curveto"));*/
            do {
                p[i++] = pessvg_PATH_BEZIER;
                last_quad_x = x1;
                last_quad_y = y1;
                if (*command == 'q') {
                    x1 += last_x;
                    y1 += last_y;
                    x += last_x;
                    y += last_y;
                }
                p[i++] = (float)(1. / 3 * last_x + 2. / 3 * x1);
                p[i++] = (float)(1. / 3 * last_y + 2. / 3 * y1);
                p[i++] = (float)(2. / 3 * x1 + 1. / 3 * x);
                p[i++] = (float)(2. / 3 * y1 + 1. / 3 * y);
                p[i++] = last_cubic_x = last_x = x;
                p[i++] = last_cubic_y = last_y = y;
                s += n;
            } while (sscanf(s, "%f %f %f %f %n", &x1, &y1, &x, &y, &n) == 4);

            /* shorthand/smooth quadratic Bezier curveto (T, t)
             (2 arguments) */
        } else if (sscanf(s, " %1[Tt] %f %f %n", command, &x, &y, &n) == 3) {
            /*LOG(("shorthand/smooth quadratic Bezier curveto"));*/
            do {
                p[i++] = pessvg_PATH_BEZIER;
                x1 = last_x + (last_x - last_quad_x);
                y1 = last_y + (last_y - last_quad_y);
                last_quad_x = x1;
                last_quad_y = y1;
                if (*command == 't') {
                    x1 += last_x;
                    y1 += last_y;
                    x += last_x;
                    y += last_y;
                }
                p[i++] = (float)(1. / 3 * last_x + 2. / 3 * x1);
                p[i++] = (float)(1. / 3 * last_y + 2. / 3 * y1);
                p[i++] = (float)(2. / 3 * x1 + 1. / 3 * x);
                p[i++] = (float)(2. / 3 * y1 + 1. / 3 * y);
                p[i++] = last_cubic_x = last_x = x;
                p[i++] = last_cubic_y = last_y = y;
                s += n;
            } while (sscanf(s, "%f %f %n", &x, &y, &n) == 2);

            /* elliptical arc (A, a) (7 arguments) */
        } else if (sscanf(s, " %1[Aa] %f %f %f %f %f %f %f %n", command, &rx, &ry, &rotation,
                          &large_arc, &sweep, &x, &y, &n) == 8) {
            do {
                // TODO: elliptical arc >>>
                p[i++] = pessvg_PATH_LINE;
                if (*command == 'a') {
                    x += last_x;
                    y += last_y;
                }
                p[i++] = last_cubic_x = last_quad_x = last_x = x;
                p[i++] = last_cubic_y = last_quad_y = last_y = y;
                // TODO: elliptical arc <<<
                s += n;
            } while (sscanf(s, "%f %f %f %f %f %f %f %n", &rx, &ry, &rotation, &large_arc, &sweep,
                            &x, &y, &n) == 7);

        } else {
            fprintf(stderr, "parse failed at \"%s\"\n", s);
            break;
        }
    }  // end while(*s)

    if (path_d) {
        // delete path_d;
    }

    if (i <= 4) {
        /* no real segments in path */
        free(p);
        return pessvg_OK;
    }

    // Finding Path Title (Boxy SVG)
    pugi::xml_object_range<pugi::xml_node_iterator> children = path.children();
    for (auto it = children.begin(); it != children.end(); it++) {
        const char* name = (*it).name();
        if (strcmp(name, "title") == 0) {
            auto cv = (*it).child_value();
            if( cv ) {
                string szStyle(cv);
                //SkDebugf( "Path Title: %s\n", szStyle.c_str() );
                state.path_id = std::string( szStyle.c_str() );
            }
        }
    }    

    return pessvg_add_path(p, i, &state);
}

pessvg_code pessvg_parse_rect(pugi::xml_node rect, struct pessvg_parse_state state) {
    float x, y, width, height;
    float* p;

    pessvg_parse_position_attributes(rect, state, &x, &y, &width, &height);
    pessvg_parse_paint_attributes(rect, &state);
    pessvg_parse_transform_attributes(rect, &state);

    if( state.currNode->rect.group_id.size() ) {
        state.group_id = state.currNode->rect.group_id;
    }    

    p = (float*)malloc(13 * sizeof p[0]);
    if (!p) return pessvg_OUT_OF_MEMORY;

    p[0] = pessvg_PATH_MOVE;
    p[1] = x;
    p[2] = y;
    p[3] = pessvg_PATH_LINE;
    p[4] = x + width;
    p[5] = y;
    p[6] = pessvg_PATH_LINE;
    p[7] = x + width;
    p[8] = y + height;
    p[9] = pessvg_PATH_LINE;
    p[10] = x;
    p[11] = y + height;
    p[12] = pessvg_PATH_CLOSE;

    return pessvg_add_path(p, 13, &state);
}

pessvg_code pessvg_parse_circle(pugi::xml_node circle, struct pessvg_parse_state state) {
    float x = 0, y = 0, r = -1;
    float* p;

    auto attrib = circle.attributes();
    for (auto it = attrib.begin(); it != attrib.end(); it++) {
        const char* name = (const char*)it->name();
        const char* content = (const char*)it->value();

        if (strcmp(name, "cx") == 0)
            x = pessvg_parse_length(content, (int)state.viewport_width, state);
        else if (strcmp(name, "cy") == 0)
            y = pessvg_parse_length(content, (int)state.viewport_height, state);
        else if (strcmp(name, "r") == 0)
            r = pessvg_parse_length(content, (int)state.viewport_width, state);
    }
    pessvg_parse_paint_attributes(circle, &state);
    pessvg_parse_transform_attributes(circle, &state);

    if( state.currNode->circle.group_id.size() ) {
        state.group_id = state.currNode->circle.group_id;
    }    

    if (r < 0) {
        // how to figure?
        // state.diagram->error_line = circle->line;

        state.diagram->error_message = "circle: r missing or negative";
        return pessvg_SVG_ERROR;
    }
    if (r == 0) return pessvg_OK;

    p = (float*)malloc(32 * sizeof p[0]);
    if (!p) return pessvg_OUT_OF_MEMORY;

    p[0] = pessvg_PATH_MOVE;
    p[1] = x + r;
    p[2] = y;
    p[3] = pessvg_PATH_BEZIER;
    p[4] = x + r;
    p[5] = (float)(y + r * KAPPA);
    p[6] = (float)(x + r * KAPPA);
    p[7] = y + r;
    p[8] = x;
    p[9] = y + r;
    p[10] = pessvg_PATH_BEZIER;
    p[11] = (float)(x - r * KAPPA);
    p[12] = y + r;
    p[13] = x - r;
    p[14] = (float)(y + r * KAPPA);
    p[15] = x - r;
    p[16] = y;
    p[17] = pessvg_PATH_BEZIER;
    p[18] = x - r;
    p[19] = (float)(y - r * KAPPA);
    p[20] = (float)(x - r * KAPPA);
    p[21] = y - r;
    p[22] = x;
    p[23] = y - r;
    p[24] = pessvg_PATH_BEZIER;
    p[25] = (float)(x + r * KAPPA);
    p[26] = y - r;
    p[27] = x + r;
    p[28] = (float)(y - r * KAPPA);
    p[29] = x + r;
    p[30] = y;
    p[31] = pessvg_PATH_CLOSE;

    return pessvg_add_path(p, 32, &state);
}

pessvg_code pessvg_parse_ellipse(pugi::xml_node ellipse, struct pessvg_parse_state state) {
    float x = 0, y = 0, rx = -1, ry = -1;
    float* p;

    auto attrib = ellipse.attributes();
    for (auto it = attrib.begin(); it != attrib.end(); it++) {
        const char* name = (const char*)it->name();
        const char* content = (const char*)it->value();

        if (strcmp(name, "cx") == 0)
            x = pessvg_parse_length(content, (int)state.viewport_width, state);
        else if (strcmp(name, "cy") == 0)
            y = pessvg_parse_length(content, (int)state.viewport_height, state);
        else if (strcmp(name, "rx") == 0)
            rx = pessvg_parse_length(content, (int)state.viewport_width, state);
        else if (strcmp(name, "ry") == 0)
            ry = pessvg_parse_length(content, (int)state.viewport_width, state);
    }
    pessvg_parse_paint_attributes(ellipse, &state);
    pessvg_parse_transform_attributes(ellipse, &state);

    if( state.currNode->group.group_id.size() ) {
        state.group_id = state.currNode->group.group_id;
    }

    if (rx < 0 || ry < 0) {
        // how to figure?
        // state.diagram->error_line = circle->line;

        state.diagram->error_message = "circle: r missing or negative";
        return pessvg_SVG_ERROR;
    }
    if (rx == 0 || ry == 0) return pessvg_OK;

    p = (float*)malloc(32 * sizeof p[0]);
    if (!p) return pessvg_OUT_OF_MEMORY;

    p[0] = pessvg_PATH_MOVE;
    p[1] = x + rx;
    p[2] = y;
    p[3] = pessvg_PATH_BEZIER;
    p[4] = x + rx;
    p[5] = (float)(y + ry * KAPPA);
    p[6] = (float)(x + rx * KAPPA);
    p[7] = y + ry;
    p[8] = x;
    p[9] = y + ry;
    p[10] = pessvg_PATH_BEZIER;
    p[11] = (float)(x - rx * KAPPA);
    p[12] = y + ry;
    p[13] = x - rx;
    p[14] = (float)(y + ry * KAPPA);
    p[15] = x - rx;
    p[16] = y;
    p[17] = pessvg_PATH_BEZIER;
    p[18] = x - rx;
    p[19] = (float)(y - ry * KAPPA);
    p[20] = (float)(x - rx * KAPPA);
    p[21] = y - ry;
    p[22] = x;
    p[23] = y - ry;
    p[24] = pessvg_PATH_BEZIER;
    p[25] = (float)(x + rx * KAPPA);
    p[26] = y - ry;
    p[27] = x + rx;
    p[28] = (float)(y - ry * KAPPA);
    p[29] = x + rx;
    p[30] = y;
    p[31] = pessvg_PATH_CLOSE;

    return pessvg_add_path(p, 32, &state);
}

pessvg_code pessvg_parse_line(pugi::xml_node line, struct pessvg_parse_state state) {
    float x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    float* p;

    auto attrib = line.attributes();
    for (auto it = attrib.begin(); it != attrib.end(); it++) {
        const char* name = (const char*)it->name();
        const char* content = (const char*)it->value();

        if (strcmp(name, "x1") == 0)
            x1 = pessvg_parse_length(content, (int)state.viewport_width, state);
        else if (strcmp(name, "y1") == 0)
            y1 = pessvg_parse_length(content, (int)state.viewport_height, state);
        else if (strcmp(name, "x2") == 0)
            x2 = pessvg_parse_length(content, (int)state.viewport_width, state);
        else if (strcmp(name, "y2") == 0)
            y2 = pessvg_parse_length(content, (int)state.viewport_height, state);
    }
    pessvg_parse_paint_attributes(line, &state);
    pessvg_parse_transform_attributes(line, &state);

    if( state.currNode->line.group_id.size() ) {
        state.group_id = state.currNode->line.group_id;
    }     

    p = (float*)malloc(7 * sizeof p[0]);
    if (!p) return pessvg_OUT_OF_MEMORY;

    p[0] = pessvg_PATH_MOVE;
    p[1] = x1;
    p[2] = y1;
    p[3] = pessvg_PATH_LINE;
    p[4] = x2;
    p[5] = y2;
    //p[6] = pessvg_PATH_CLOSE; // Jimmy - we should not close line

    return pessvg_add_path(p, 6, &state);
}

pessvg_code pessvg_parse_poly(pugi::xml_node poly, struct pessvg_parse_state state, bool polygon) {
    char *s, *points;
    float* p;
    unsigned int i;

    pessvg_parse_paint_attributes(poly, &state);
    pessvg_parse_transform_attributes(poly, &state);

    if( state.currNode && state.currNode->group.group_id.size() ) {
        state.group_id = state.currNode->group.group_id;
    }  

    s = points = (char*)poly.attribute("points").value();

    if (!s) {
        state.diagram->error_message = "polyline/polygon: missing points attribute";
        return pessvg_SVG_ERROR;
    }

    // allocate space for path: it will never have more elements than s
    p = (float*)malloc(sizeof p[0] * strlen(s));
    if (!p) {
        return pessvg_OUT_OF_MEMORY;
    }

    // parse s and build path
    for (i = 0; s[i]; i++)
        if (s[i] == ',') s[i] = ' ';
    i = 0;
    while (*s) {
        float x, y;
        int n;

        if (sscanf(s, "%f %f %n", &x, &y, &n) == 2) {
            if (i == 0)
                p[i++] = pessvg_PATH_MOVE;
            else
                p[i++] = pessvg_PATH_LINE;
            p[i++] = x;
            p[i++] = y;
            s += n;
        } else {
            break;
        }
    }
    if (polygon) p[i++] = pessvg_PATH_CLOSE;

    return pessvg_add_path(p, i, &state);
}

// pessvg_code pessvg_parse_text(pugi::xml_node text, struct pessvg_parse_state state){
//
//}

pessvg_code pessvg_add_path(float* p, unsigned int n, struct pessvg_parse_state* state) {
    struct pessvg_shape* shape;

    pessvg_transform_path(p, n, state);

    shape = pessvg_add_shape(state);
    if (!shape) {
        free(p);
        return pessvg_OUT_OF_MEMORY;
    }
    shape->path = p;
    shape->path_length = n;
    state->diagram->shape_count++;

    return pessvg_OK;
}

struct pessvg_shape* pessvg_add_shape(struct pessvg_parse_state* state) {
    struct pessvg_shape* shape = (pessvg_shape*)realloc(
            state->diagram->shape,
            (state->diagram->shape_count + 1) * sizeof(state->diagram->shape[0]));
    if (!shape) return 0;
    state->diagram->shape = shape;

    shape += state->diagram->shape_count;
    shape->path = 0;
    shape->visibility = state->visibility;  // Jimmy - add visibility to path
    
    shape->role_type = state->role_type;
    shape->role_offset = state->role_offset; 
    shape->fill_type = state->fill_type;
    shape->fill_subtype = state->fill_subtype;
    shape->fill_angle = state->fill_angle;
    shape->fill_size = state->fill_size;
    shape->fill_density = state->fill_density;
    shape->fill_spacing = state->fill_spacing;
    shape->fill_pitch = state->fill_pitch;
    shape->fill_pull_compensate = state->fill_pull_compensate;
    shape->stroke_type = state->stroke_type;
    shape->stroke_subtype = state->stroke_subtype;
    shape->stroke_offset = state->stroke_offset;
    shape->stroke_size = state->stroke_size;
    shape->stroke_density = state->stroke_density;
    shape->stroke_spacing = state->stroke_spacing;
    shape->stroke_pitch = state->stroke_pitch;
    shape->stroke_pull_compensate = state->stroke_pull_compensate;

    strcpy( shape->path_id, state->path_id.c_str() );
    strcpy( shape->group_id, state->group_id.c_str() );
    shape->path_length = 0;
    shape->text = 0;
    shape->fill = state->fill + ((int)(state->fill_opacity * 255) << 24);
    shape->stroke = state->stroke;
    shape->stroke_width =
            lroundf((float)(state->stroke_width * (state->ctm.a + state->ctm.d) / 2.0));
    if (0 < state->stroke_width && shape->stroke_width == 0) shape->stroke_width = 1;

    return shape;
}

void pessvg_transform_path(float* p, unsigned int n, struct pessvg_parse_state* state) {
    unsigned int j;

    for (j = 0; j != n;) {
        unsigned int points = 0;
        unsigned int k;
        switch ((int)p[j]) {
            case pessvg_PATH_MOVE:
            case pessvg_PATH_LINE:
                points = 1;
                break;
            case pessvg_PATH_CLOSE:
                points = 0;
                break;
            case pessvg_PATH_BEZIER:
                points = 3;
                break;
            default:
                assert(0);
        }
        j++;
        for (k = 0; k != points; k++) {
            float x0 = p[j], y0 = p[j + 1];
            float x = state->ctm.a * x0 + state->ctm.c * y0 + state->ctm.e;
            float y = state->ctm.b * x0 + state->ctm.d * y0 + state->ctm.f;
            p[j] = x;
            p[j + 1] = y;
            j += 2;
        }
    }
}

void pesSVG::parseXML(string xml) {
    // GUI_Log("pesSVG::parseXML\n");
    if (info.rootnode) {
        info.rootnode.reset();
    }

    info.flatlist.clear();

    pesPtr<pessvgNode> rootnode(new pessvgNode());
    rootnode->type = PES_SVG_TAG_TYPE_DOCUMENT;
    info.rootnode = rootnode;
    x = 0;
    y = 0;
    if (paths.size() > 0) {
        paths.clear();
    }

    struct pessvg_diagram* diagram = pessvg_create();  // = svgtiny_create();
    pessvg_code code = pessvg_parse(info, diagram, xml.c_str(), 0,
                                    0);  // = svgtiny_parse(info,diagram, xml.c_str(), 0, 0);

    if (code != pessvg_OK) {
        string msg;
        switch (code) {
            case pessvg_OUT_OF_MEMORY:
                msg = "svgtiny_OUT_OF_MEMORY";
                break;

            case pessvg_LIBDOM_ERROR:
                msg = "svgtiny_LIBXML_ERROR";
                break;

            case pessvg_NOT_SVG:
                msg = "svgtiny_NOT_SVG";
                break;

            case pessvg_SVG_ERROR:
                msg = "svgtiny_SVG_ERROR: line " + pesToString(diagram->error_line) + ": " +
                      diagram->error_message;
                break;

            default:
                msg = "unknown svgtiny_code " + pesToString(code);
                break;
        }
        //        GUI_LogError("guiSVG") << "load(jsBuffer): couldn't parse " << msg;
    }

    setupDiagram(diagram);
    pessvg_free(diagram);

    // Check if path to complex. Simplify if so.
    for (int i = 0; i < (int)paths.size(); i++) {
        if ((int)paths[i].getCommands().size() > complexityThreshold) {
            paths[i].simplify(0.3f);
            //            GUI_LogWarning()<<"ofxEditableSVG too complex for tesselator, it was
            //            simplified. Cmd size: "<<paths[i]->getCommands().size()<<endl;
        }
    }
}

void pesSVG::setupDiagram(struct pessvg_diagram* diagram) {
    // GUI_Log("pesSVG::setupDiagram\n");
    width = (float)diagram->width;
    height = (float)diagram->height;

    for (int i = 0; i < (int)diagram->shape_count; i++) {
        if (diagram->shape[i].path) {
            setupShape(&diagram->shape[i]);
        } else if (diagram->shape[i].text) {
            printf("text: not implemented yet\n");
        }
    }
}

void pesSVG::setupShape(struct pessvg_shape* shape) {
    //SkDebugf("pesSVG::setupShape %08X %08X\n", shape->stroke, shape->fill );
    float* p = shape->path;

    //    pesPath * path = new pesPath();
    //    paths.push_back(ofPathRef(path));

    //    pesPath *path = new pesPath();
    pesPath path;
    path.setFilled(false);

    path.bVisible = shape->visibility;      // Jimmy - add visibility to path

    path.roleType = shape->role_type;
    path.roleOffset = shape->role_offset; 
    path.fillType = shape->fill_type;
    path.fillSubtype = shape->fill_subtype;
    path.fillAngle = shape->fill_angle;
    path.fillSize = shape->fill_size;
    path.fillDensity = shape->fill_density;
    path.fillSpacing = shape->fill_spacing;
    path.fillPitch = shape->fill_pitch;
    path.fillPullCompensate = shape->fill_pull_compensate;
    path.strokeType = shape->stroke_type;
    path.strokeSubtype = shape->stroke_subtype;
    path.strokeOffset = shape->stroke_offset;
    path.strokeSize = shape->stroke_size;
    path.strokeDensity = shape->stroke_density;
    path.strokeSpacing = shape->stroke_spacing;
    path.strokePitch = shape->stroke_pitch;
    path.strokePullCompensate = shape->stroke_pull_compensate;

    path.path_id = std::string(shape->path_id);
    path.group_id = std::string(shape->group_id);
    if (shape->fill != pessvg_TRANSPARENT && shape->fill != 0) {
        path.setFilled(true);
        path.setFillHexColor(shape->fill);
        //        path->setPolyWindingMode(PolyWindingMode::POLY_WINDING_NONZERO);
    } else {
        pesColor ocol = {0, 0, 0, 0};
        path.setFilled(false);
        path.setFillColor(ocol);
    }

    if (shape->stroke != pessvg_TRANSPARENT) {
        path.setStrokeWidth((float)shape->stroke_width);
        path.setStrokeHexColor(shape->stroke);
    } else {
        pesColor ocol = {0, 0, 0, 0};
        path.setStrokeColor(ocol);
    }

    for (int i = 0; i < (int)shape->path_length;) {
        if (p[i] == pessvg_PATH_MOVE) {
            path.moveTo(p[i + 1], p[i + 2]);
            i += 3;
        } else if (p[i] == pessvg_PATH_CLOSE) {
            path.close();

            i += 1;
        } else if (p[i] == pessvg_PATH_LINE) {
            path.lineTo(p[i + 1], p[i + 2]);
            i += 3;
        } else if (p[i] == pessvg_PATH_BEZIER) {
            path.bezierTo(pesVec2f(p[i + 1], p[i + 2]),
                          pesVec2f(p[i + 3], p[i + 4]),
                          pesVec2f(p[i + 5], p[i + 6]));
            i += 7;
        } else {
            // cout << "error\n" << endl;
            //            GUI_LogError("pesSVG::setupShape") << "SVG parse error";
            i += 1;
        }
    }

    if (path.getCommands().size()) {
        paths.push_back(path);
    } else {
        //        GUI_LogWarning("pesSVG::setupShape") << "SVG parse path is empty";
    }
}

const vector<pesPath>& pesSVG::getPaths() const { return paths; }

// void pesSVG::draw(){
//    for(int i = 0; i < (int)paths.size(); i++){
//        paths[i]->draw();
//    }
//}

void pesSVG::setFilled(bool t) {
    for (int i = 0; i < (int)paths.size(); i++) {
        paths[i].setFilled(t);
        if (t) {
            setSVGattribute(info.flatlist[i], "fill_opacity", "1.0");
        } else {
            setSVGattribute(info.flatlist[i], "fill_opacity", "0.0");
        }
    }
    isFilled = t;
};

void pesSVG::setFilled(bool t, int path) {
    if (path > (int)info.flatlist.size() - 1) {
        //        GUI_LogWarning()<<"WARNING: Tried to modify ofxEditableSVG path out of index - "<<
        //        path<<" of info.flatlist.size() "<<info.flatlist.size()<<endl;
        return;
    }
    paths[path].setFilled(t);

    if (t) {
        setSVGattribute(info.flatlist[path], "fill_opacity", "1.0");
    } else {
        setSVGattribute(info.flatlist[path], "fill_opacity", "0.0");
    }
}

void pesSVG::setFillColor(pesColor col) {
    // ofLog()<<"col.r "<<ofToHex(col.r)<<endl;
    for (int i = 0; i < (int)paths.size(); i++) {
        // col.set(255,255,255);
        paths[i].setFillHexColor(col.getHex());
        // borg...update DOM ...
        setSVGattribute(info.flatlist[i], "fill", hexToWeb(col));
        setSVGattribute(info.flatlist[i], "fill_opacity", pesToString((float)col.a / 255.0));

        // info.flatlist[i]->path.fill = hexToWeb(col);
        // info.flatlist[i]->path.fill_opacity = pesToString((float) col.a/255.0);
    }
    fill = col;
}

void pesSVG::setFillColor(pesColor col, int path) {
    if (path > (int)info.flatlist.size() - 1) {
        //        GUI_LogWarning()<<"WARNING: Tried to modify ofxEditableSVG path out of index - "<<
        //        path<<" of info.flatlist.size() "<<info.flatlist.size()<<endl;
        return;
    }

    paths[path].setFillHexColor(col.getHex());

    setSVGattribute(info.flatlist[path], "fill", pesToString(hexToWeb(col)));
    setSVGattribute(info.flatlist[path], "fill_opacity", pesToString((float)col.a / 255.0));

    // info.flatlist[path]->path.fill = hexToWeb(col);
    // info.flatlist[path]->path.fill_opacity = pesToString((float) col.a/255.0);
}

void pesSVG::setStrokeWidth(float f) {
    // ofLog()<<"setStrokeWidth info.flatlist.size "<< info.flatlist.size()<<" paths.size()
    // "<<paths.size()<<endl;

    for (int i = 0; i < (int)paths.size(); i++) {
        paths[i].setStrokeWidth(f);
        // borg...update DOM...make sure you cast correctly as flatlist is using pesPtr...and xcode
        // doesn't tell you float is not string

        // info.flatlist[i]->path.stroke_width = pesToString(f);
        setSVGattribute(info.flatlist[i], "stroke_width", pesToString(f));
    }

    strokeWidth = f;
}

void pesSVG::setStrokeColor(pesColor col) {
    for (int i = 0; i < (int)paths.size(); i++) {
        paths[i].setStrokeColor(col);
        // borg...update DOM
        setSVGattribute(info.flatlist[i], "stroke", hexToWeb(col));
        setSVGattribute(info.flatlist[i], "stroke_opacity", pesToString((float)col.a / 255.0));
        // info.flatlist[i]->path.stroke = pesToString(hexToWeb(col));
        // info.flatlist[i]->path.stroke_opacity = pesToString((float) col.a/255.0);
    }
    stroke = col;
}

void pesSVG::setSVGattribute(pesPtr<pessvgNode> node, string attribute, string value) {
    switch (node->type) {
        case PES_SVG_TAG_TYPE_CIRCLE:
            if (attribute == "cx") {
                node->circle.cx = value;
            }
            if (attribute == "cy") {
                node->circle.cy = value;
            }
            if (attribute == "fill") {
                node->circle.fill = value;
            }
            if (attribute == "stroke_width") {
                node->circle.stroke_width = value;
            }
            if (attribute == "stroke") {
                node->circle.stroke = value;
            }
            if (attribute == "r") {
                node->circle.r = value;
            }

            if (attribute == "stroke_width") {
                node->circle.stroke_width = value;
            }
            if (attribute == "stroke_opacity") {
                node->circle.stroke_opacity = value;
            }
            if (attribute == "fill_opacity") {
                node->circle.fill_opacity = value;
            }
            break;

        case PES_SVG_TAG_TYPE_RECT:
            if (attribute == "x") {
                node->rect.x = value;
            }
            if (attribute == "y") {
                node->rect.y = value;
            }
            if (attribute == "fill") {
                node->rect.fill = value;
            }
            if (attribute == "stroke_width") {
                node->rect.stroke_width = value;
            }
            if (attribute == "stroke") {
                node->rect.stroke = value;
            }
            if (attribute == "width") {
                node->rect.width = value;
            }
            if (attribute == "height") {
                node->rect.height = value;
            }
            if (attribute == "stroke_miterlimit") {
                node->rect.stroke_miterlimit = value;
            }
            if (attribute == "stroke_width") {
                node->rect.stroke_width = value;
            }
            if (attribute == "stroke_opacity") {
                node->rect.stroke_opacity = value;
            }
            if (attribute == "fill_opacity") {
                node->rect.fill_opacity = value;
            }
            break;

        case PES_SVG_TAG_TYPE_PATH:
            // ofLog()<<"SVG_TAG_TYPE_PATH"<<endl;
            if (attribute == "fill") {
                node->path.fill = value;
            }
            if (attribute == "stroke_width") {
                node->path.stroke_width = value;
            }
            if (attribute == "stroke") {
                node->path.stroke = value;
            }
            if (attribute == "stroke_miterlimit") {
                node->path.stroke_miterlimit = value;
            }
            if (attribute == "stroke_width") {
                node->path.stroke_width = value;
            }
            if (attribute == "stroke_opacity") {
                node->path.stroke_opacity = value;
            }
            if (attribute == "fill_opacity") {
                node->path.fill_opacity = value;
            }
            break;

        case PES_SVG_TAG_TYPE_GROUP:
            // ofLog()<<"SVG_TAG_TYPE_GROUP"<<endl;
            if (attribute == "transform") {
                node->group.transform = value;
            }
            if (attribute == "fill") {
                node->group.fill = value;
            }
            if (attribute == "stroke_width") {
                node->group.stroke_width = value;
            }
            if (attribute == "stroke") {
                node->group.stroke = value;
            }
            if (attribute == "stroke_miterlimit") {
                node->group.stroke_miterlimit = value;
            }
            if (attribute == "stroke_width") {
                node->group.stroke_width = value;
            }
            if (attribute == "stroke_opacity") {
                node->group.stroke_opacity = value;
            }
            if (attribute == "fill_opacity") {
                node->group.fill_opacity = value;
            }
            break;
        case PES_SVG_TAG_TYPE_SVG:
            break;
        case PES_SVG_TAG_TYPE_DOCUMENT:
            break;

        default:
            break;
    }
}

bool pesSVG::getFilled() { return isFilled; }

pesColor pesSVG::getFillColor() { return fill; }

float pesSVG::getStrokeWidth() { return strokeWidth; }

pesColor pesSVG::getStrokeColor() { return stroke; }

int pesSVG::getX() { return (int)x; }

int pesSVG::getY() { return (int)y; }

int pesSVG::getWidth() { return (int)width; }

int pesSVG::getHeight() { return (int)height; }

void pesSVG::setPos(int _x, int _y) {
    x = (float)_x;
    y = (float)_y;
}

void pesSVG::setPos(pesVec2f pt) {
    x = pt.x;
    y = pt.y;
}

void pesSVG::setViewbox(int x, int y, int w, int h) {
    info.viewbox =
            pesToString(x) + " " + pesToString(y) + " " + pesToString(w) + " " + pesToString(h);
    info.y = pesToString(y) + "px";
    info.x = pesToString(x) + "px";
    info.width = pesToString(w) + "px";
    info.height = pesToString(h) + "px";
}

void pesSVG::setSize(float w, float h, string unit) {
    width = w;
    height = h;
    info.width = pesToString(w) + unit;
    info.height = pesToString(h) + unit;
}

// MARK: string stream & writer
string pesSVG::toString() {
    std::shared_ptr<pugi::xml_document> doc = std::make_shared<pugi::xml_document>();
    parseHeader(doc);
    //    auto children = doc->children();

    ostringstream stream;
    doc->print(stream);

    return stream.str();
}

void pesSVG::parseHeader(std::shared_ptr<pugi::xml_document> doc) {
    //    GUI_Log("pesSVG::parseHeader\n");
    pugi::xml_node svg = doc->root().append_child("svg");

    if (!info.x.empty()) {
        svg.append_attribute("x") = info.x.c_str();
    }
    if (!info.y.empty()) {
        svg.append_attribute("y") = info.y.c_str();
    }
    if (!info.width.empty()) {
        svg.append_attribute("width") = info.width.c_str();
    }
    if (!info.height.empty()) {
        svg.append_attribute("height") = info.height.c_str();
    }
    if (!info.enable_background.empty()) {
        svg.append_attribute("enable-background") = info.enable_background.c_str();
    }
    if (!info.viewbox.empty()) {
        svg.append_attribute("viewBox") = info.viewbox.c_str();
    }
    if (!info.version.empty()) {
        svg.append_attribute("version") = info.version.c_str();
    }
    if (!info.id.empty()) {
        svg.append_attribute("id") = info.id.c_str();
    }
    if (!info.preserveAspectRatio.empty()) {
        svg.append_attribute("preserveAspectRatio") = info.preserveAspectRatio.c_str();
    }
    if (!info.xmlns.empty()) {
        svg.append_attribute("xmlns") = info.xmlns.c_str();
    } else {
        svg.append_attribute("xmlns") = "http://www.w3.org/2000/svg";
    }

    if (!info.xmlns_xlink.empty()) {
        svg.append_attribute("xmlns:xlink") = info.xmlns_xlink.c_str();
    } else {
        svg.append_attribute("xmlns:xlink") = "http://www.w3.org/1999/xlink";
    }

    if (info.rootnode) {
        xmlCreateSVG(svg, info.rootnode);
    } else {
        //        GUI_LogNotice("parseHeader") << "root node empty";
    }
}

void pesSVG::xmlCreateSVG(pugi::xml_node& extNode, pesPtr<pessvgNode> intNode) {
    //    GUI_Log("pesSVG::xmlCreateSVG\n");
    pugi::xml_node newNode = parseNode(extNode, intNode);
    const char* value = newNode.value();
    if (newNode.empty() == false) {
        //        extNode.append_copy(newNode);
        // recursive call
        for (int i = 0; i < (int)intNode->children.size(); i++) {
            xmlCreateSVG(newNode, intNode->children[i]);
        }
    } else {
        // recursive call
        for (int i = 0; i < (int)intNode->children.size(); i++) {
            xmlCreateSVG(extNode, intNode->children[i]);
        }
    }
}

pugi::xml_node pesSVG::parseNode(pugi::xml_node& root, pesPtr<pessvgNode> node) {
    if (!node) {
        //        GUI_LogWarning("pugiSVG", "parseNode") << "Why is node null?";
        return pugi::xml_node();
    }

    pugi::xml_node newNode;
    switch (node->type) {
        case PES_SVG_TAG_TYPE_PATH:
            newNode = root.append_child("path");
            newNode.append_attribute("d") = node->path.d.c_str();
            if (!node->path.fill.empty()) {
                newNode.append_attribute("fill") = node->path.fill.c_str();
            }
            if (!node->path.stroke.empty()) {
                newNode.append_attribute("stroke") = node->path.stroke.c_str();
            }
            if (!node->path.stroke_miterlimit.empty()) {
                newNode.append_attribute("stroke-miterlimit") =
                        node->path.stroke_miterlimit.c_str();
            }
            if (!node->path.stroke_width.empty()) {
                newNode.append_attribute("stroke-width") = node->path.stroke_width.c_str();
            }
            if (!node->path.fill_opacity.empty()) {
                newNode.append_attribute("fill-opacity") = node->path.fill_opacity.c_str();
            }
            if (!node->path.stroke_opacity.empty()) {
                newNode.append_attribute("stroke-opacity") = node->path.stroke_opacity.c_str();
            }
            if (!node->path.visibility.empty()) {   // Jimmy - add visibility to path
                newNode.append_attribute("visibility") = node->path.visibility.c_str();
            }
            if (!node->path.fill_type.empty()) {   // Jimmy - add fill-type to path (out)
                newNode.append_attribute("fill-type") = node->path.fill_type.c_str();
            }
            if (!node->path.fill_subtype.empty()) {   // Jimmy - add fill-type to path (out)
                newNode.append_attribute("fill-subtype") = node->path.fill_subtype.c_str();
            }
            if (!node->path.fill_angle.empty()) {   // Jimmy - add fill-type to path (out)
                newNode.append_attribute("fill-angle") = node->path.fill_angle.c_str();
            }
            if (!node->path.fill_density.empty()) {   // Jimmy - add fill-type to path (out)
                newNode.append_attribute("fill-density") = node->path.fill_density.c_str();
            }
            if (!node->path.fill_size.empty()) {   // Jimmy - add fill-type to path (out)
                newNode.append_attribute("fill-size") = node->path.fill_size.c_str();
            }
            if (!node->path.fill_spacing.empty()) {   // Jimmy - add fill-type to path (out)
                newNode.append_attribute("fill-spacing") = node->path.fill_spacing.c_str();
            }
            if (!node->path.fill_pitch.empty()) {   // Jimmy - add fill-type to path (out)
                newNode.append_attribute("fill-pitch") = node->path.fill_pitch.c_str();
            }
            if (!node->path.fill_pull_compensate.empty()) {   // Jimmy - add fill-type to path (out)
                newNode.append_attribute("fill-pull-compensate") = node->path.fill_pull_compensate.c_str();
            }
            if (!node->path.stroke_type.empty()) {   // Jimmy - add fill-type to path (out)
                newNode.append_attribute("stroke-type") = node->path.stroke_type.c_str();
            }
            if (!node->path.stroke_subtype.empty()) {   // Jimmy - add fill-type to path (out)
                newNode.append_attribute("stroke-subtype") = node->path.stroke_subtype.c_str();
            }
            if (!node->path.stroke_offset.empty()) {   // Jimmy - add fill-type to path (out)
                newNode.append_attribute("stroke-offset") = node->path.stroke_offset.c_str();
            }
            if (!node->path.stroke_size.empty()) {   // Jimmy - add fill-type to path (out)
                newNode.append_attribute("stroke-size") = node->path.stroke_size.c_str();
            }
            if (!node->path.stroke_density.empty()) {   // Jimmy - add fill-type to path (out)
                newNode.append_attribute("stroke-density") = node->path.stroke_density.c_str();
            }
            if (!node->path.stroke_spacing.empty()) {   // Jimmy - add fill-type to path (out)
                newNode.append_attribute("stroke-spacing") = node->path.stroke_spacing.c_str();
            }
            if (!node->path.stroke_pitch.empty()) {   // Jimmy - add fill-type to path (out)
                newNode.append_attribute("stroke-pitch") = node->path.stroke_pitch.c_str();
            }
            if (!node->path.stroke_pull_compensate.empty()) {   // Jimmy - add fill-type to path (out)
                newNode.append_attribute("stroke-pull-compensate") = node->path.stroke_pull_compensate.c_str();
            }
            if (!node->path.role_type.empty()) {   // Jimmy - add fill-type to path (out)
                newNode.append_attribute("role-type") = node->path.role_type.c_str();
            }
            if (!node->path.role_offset.empty()) {   // Jimmy - add fill-type to path (out)
                newNode.append_attribute("role-offset") = node->path.role_offset.c_str();
            }
            if (!node->path.group_id.empty()) {   
                newNode.append_attribute("group-id") = node->path.group_id.c_str();
            }
            if (!node->path.path_id.empty()) {   
                newNode.append_attribute("id") = node->path.path_id.c_str();
            }
            break;

        case PES_SVG_TAG_TYPE_GROUP:
            newNode = root.append_child("g");
            if (!node->group.transform.empty()) {
                newNode.append_attribute("transform") = node->group.transform.c_str();
            }
            if (!node->group.fill.empty()) {
                newNode.append_attribute("fill") = node->group.fill.c_str();
            }
            if (!node->group.stroke.empty()) {
                newNode.append_attribute("stroke") = node->group.stroke.c_str();
            }
            if (!node->group.stroke_miterlimit.empty()) {
                newNode.append_attribute("stroke-miterlimit") =
                        node->group.stroke_miterlimit.c_str();
            }
            if (!node->group.stroke_width.empty()) {
                newNode.append_attribute("stroke-width") = node->group.stroke_width.c_str();
            }
            if (!node->group.fill_opacity.empty()) {
                newNode.append_attribute("fill-opacity") = node->group.fill_opacity.c_str();
            }
            if (!node->group.stroke_opacity.empty()) {
                newNode.append_attribute("stroke-opacity") = node->group.stroke_opacity.c_str();
            }
            break;

        case PES_SVG_TAG_TYPE_SVG:
            newNode = root.append_child("svg");
            newNode.append_attribute("x") = node->svg.x.c_str();
            newNode.append_attribute("y") = node->svg.y.c_str();
            newNode.append_attribute("width") = node->svg.width.c_str();
            newNode.append_attribute("height") = node->svg.height.c_str();
            if (!node->svg.enable_background.empty()) {
                newNode.append_attribute("enable-background") = node->svg.enable_background.c_str();
            }
            // newNode->setAttribute("viewBox",node->svg.viewbox);
            break;

        case PES_SVG_TAG_TYPE_DOCUMENT:
            return pugi::xml_node();
            break;

        default:
            break;
    }
    return newNode;
}

// string pugiGetTimestampString(const string& timestampFormat){
//    std::stringstream str;
//    auto now = std::chrono::system_clock::now();
//    auto t = std::chrono::system_clock::to_time_t(now);    std::chrono::duration<double> s = now -
//    std::chrono::system_clock::from_time_t(t); int ms = s.count() * 1000; auto tm =
//    *std::localtime(&t); constexpr int bufsize = 256; char buf[bufsize];
//
//    // Beware! an invalid timestamp string crashes windows apps.
//    // so we have to filter out %i (which is not supported by vs)
//    // earlier.
//    auto tmpTimestampFormat = timestampFormat;
//    guiStringReplace(tmpTimestampFormat, "%i", pesToString(ms, 3, '0'));
//
//    if (strftime(buf,bufsize, tmpTimestampFormat.c_str(),&tm) != 0){
//        str << buf;
//    }
//    auto ret = str.str();
//
//
//    return ret;
//}
//
// string pugiGetTimestampString(){
//    string timeFormat = "%Y-%m-%d-%H-%M-%S-%i";
//
//    return pugiGetTimestampString(timeFormat);
//}

void pesSVG::addPaths(const vector<pesPath>& paths, bool inNewGroup) {
    //    GUI_Log("pesSVG::addPaths\n");
    string rawXMLstring = toString();
    pugi::xml_document doc;
    doc.load_string(rawXMLstring.c_str());

    pugi::xml_node svg = doc.first_child();
    pugi::xml_node appendNode;
    if (inNewGroup) {
        appendNode = svg.append_child("g");
        //        appendNode.append_attribute("id") = pugiGetTimestampString().c_str();
    } else {
        appendNode = svg;
    }

    for (int i = 0; i < (int)paths.size(); i++) {
        // Jimmy - svg export with hidden
        //if( paths[i].bVisible == false ) 
        //    continue;


        //        Element *pathElement = parsePath(paths[i],document);
        //        appendElement->appendChild(pathElement);
        parsePath(appendNode, paths[i]);
    }

    ostringstream xmlstream;
    doc.print(xmlstream);

    parseXML(xmlstream.str());
}

pugi::xml_node pesSVG::parsePath(pugi::xml_node& root, const pesPath& path) {
    //    GUI_Log("pesSVG::parsePath\n");
    pugi::xml_node pnode;
    string d = "";
    for (pesPath::Command com : path.getCommands()) {
        //        int t = com.type;
        switch (com.type) {
            case pesPath::Command::Type::_moveTo:
                d += " M" + pesToString(com.to.x) + "," + pesToString(com.to.y);
                //                pnode = root.append_child("path");
                //                pnode.append_attribute("d") = d.c_str();
                break;

            case pesPath::Command::Type::_lineTo:
                d += " L" + pesToString(com.to.x) + "," + pesToString(com.to.y);
                //                pnode = root.append_child("path");
                //                pnode.append_attribute("d") = d.c_str();
                break;

            case pesPath::Command::Type::_curveTo:
                //                GUI_LogWarning()<<"curveTo to SVG not properly implemented";
                d += " Q" + pesToString(com.cp1.x) + "," + pesToString(com.cp1.y) + " " +
                     pesToString(com.to.x) + "," + pesToString(com.to.y);
                //                pnode = root.append_child("path");
                //                pnode.append_attribute("d") = d.c_str();
                break;

            case pesPath::Command::Type::_bezierTo:
                d += " C" + pesToString(com.cp1.x) + "," + pesToString(com.cp1.y) + " " +
                     pesToString(com.cp2.x) + "," + pesToString(com.cp2.y) + " " +
                     pesToString(com.to.x) + "," + pesToString(com.to.y);
                //                pnode = root.append_child("path");
                //                pnode.append_attribute("d") = d.c_str();
                break;

            case pesPath::Command::Type::_quadBezierTo:
                // SVG Q is quadratic bezier
                d += " Q" + pesToString(com.cp2.x) + "," + pesToString(com.cp2.y) + " " +
                     pesToString(com.to.x) + "," + pesToString(com.to.y);
                //                pnode = root.append_child("path");
                //                pnode.append_attribute("d") = d.c_str();
                break;

            case pesPath::Command::Type::_arc:

                if ((com.angleEnd - com.angleBegin) < 360) {
                    //                    GUI_LogWarning()<<"arc to SVG not properly implemented";
                    d += " A" + pesToString(com.radiusX) + "," + pesToString(com.radiusY) + " " +
                         pesToString(com.angleBegin) + " 1,0 " + pesToString(com.to.x) + "," +
                         pesToString(com.to.y);
                    //                    pnode = root.append_child("path");
                    //                    pnode.append_attribute("d") = d.c_str();
                } else {
                    //                    pnode = root.append_child("circle");
                    //                    pnode.append_attribute("cx") =
                    //                    pesToString(com.to.x).c_str();
                    //                    pnode.append_attribute("cy") =
                    //                    pesToString(com.to.y).c_str(); pnode.append_attribute("r")
                    //                    = pesToString(com.radiusX).c_str();
                }
                break;

            case pesPath::Command::Type::_arcNegative: {
                // arcNegative
                //                GUI_LogWarning()<<"arcNegative to SVG not implemented";
                break;
            }

            case pesPath::Command::Type::_close: {
                // close
                d += " Z ";
                //                pnode = root.append_child("path");
                //                pnode.append_attribute("d") = d.c_str();
                break;
            }
        }
    }  // end for

    pnode = root.append_child("path");
    pnode.append_attribute("d") = d.c_str();

    //    if(isFilled){
    //    // force export outline(non-fill) svg for scan n cut
    //    if(0){
    //        pnode.append_attribute("fill") = hexToWeb(getFillColor()).c_str();
    //        pnode.append_attribute("fill-opacity") =
    //        pesToString(path.getFillColor().a/255.0f).c_str();
    //    }
    //    else{
    //        pnode.append_attribute("fill") = "none";
    //        pnode.append_attribute("stroke") = hexToWeb(ofColor()).c_str();
    //        pnode.append_attribute("stroke-width") = "1.0";
    //        pnode.append_attribute("stroke-opacity") = "1.0";
    //    }

    if (path.isFill()) {
        pnode.append_attribute("fill") = hexToWeb(path.getFillColor()).c_str();
        pnode.append_attribute("fill-opacity") =
                pesToString(path.getFillColor().a / 255.0f).c_str();
    }
    else {
        pnode.append_attribute("fill") = "none";
    }
    if (path.isStroke()) {
        pnode.append_attribute("stroke") = hexToWeb(path.getStrokeColor()).c_str();
        pnode.append_attribute("stroke-width") = pesToString(path.getStrokeWidth()).c_str();
        pnode.append_attribute("stroke-opacity") =
                pesToString(path.getStrokeColor().a / 255.0f).c_str();
    }
    if (!path.isStroke() && !path.isFill() ) {
        pnode.append_attribute("fill") = "none";
        pnode.append_attribute("stroke") = hexToWeb(pesColor()).c_str();
        pnode.append_attribute("stroke-width") = "1.0";
        pnode.append_attribute("stroke-opacity") = "1.0";
    }
    if (path.bVisible ) { // Jimmy - add visibility to path
        pnode.append_attribute("visibility") = "visible";
    }
    else {
        pnode.append_attribute("visibility") = "hidden";
    }
    pnode.append_attribute("fill-type") = pesToString( path.fillType ).c_str(); // Jimmy - add fill-type to path
    pnode.append_attribute("fill-subtype") = pesToString( path.fillSubtype ).c_str(); 
    pnode.append_attribute("fill-angle") = pesToString( path.fillAngle ).c_str(); 
    pnode.append_attribute("fill-density") = pesToString( path.fillDensity ).c_str(); 
    pnode.append_attribute("fill-size") = pesToString( path.fillSize ).c_str(); 
    pnode.append_attribute("fill-spacing") = pesToString( path.fillSpacing ).c_str(); 
    pnode.append_attribute("fill-pitch") = pesToString( path.fillPitch ).c_str(); 
    pnode.append_attribute("fill-pull-compensate") = pesToString( path.fillPullCompensate ).c_str(); 

    pnode.append_attribute("stroke-type") = pesToString( path.strokeType ).c_str(); 
    pnode.append_attribute("stroke-subtype") = pesToString( path.strokeSubtype ).c_str(); 
    pnode.append_attribute("stroke-offset") = pesToString( path.strokeOffset ).c_str(); 
    pnode.append_attribute("stroke-size") = pesToString( path.strokeSize ).c_str(); 
    pnode.append_attribute("stroke-density") = pesToString( path.strokeDensity ).c_str(); 
    pnode.append_attribute("stroke-spacing") = pesToString( path.strokeSpacing ).c_str(); 
    pnode.append_attribute("stroke-pitch") = pesToString( path.strokePitch ).c_str(); 
    pnode.append_attribute("stroke-pull-compensate") = pesToString( path.strokePullCompensate ).c_str(); 

    pnode.append_attribute("id") = path.path_id.c_str(); 
    pnode.append_attribute("group-id") = path.group_id.c_str(); 
    pnode.append_attribute("role-type") = pesToString( path.roleType ).c_str(); 
    pnode.append_attribute("role-offset") = pesToString(path.roleOffset).c_str(); 

    return pnode;
}

void pesSVG::save(string file) {
    std::shared_ptr<pugi::xml_document> doc = std::make_shared<pugi::xml_document>();
    parseHeader(doc);
    //    string fullXmlFile = jsToDataPath(file);
    string fullXmlFile(file);

    doc->save_file(fullXmlFile.c_str());
    //    GUI_LogNotice()<<"Saved "<<fullXmlFile;
}
