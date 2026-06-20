//
//  pesSVG.hpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 12/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#ifndef pesSVG_hpp
#define pesSVG_hpp

#include <stdio.h>
#include "pesSVGTypes.h"
#include "pugixml.hpp"
#include "pesPath.hpp"
#include "pesBuffer.hpp"
#include "pesUtility.hpp"

class pesSVG {
public:
    
    pesSVG();
    ~pesSVG();
    
    
    float getWidth() const {
        return width;
    }
    float getHeight() const {
        return height;
    }
//    void load(string path);
    void loadFromBuffer(const pesBuffer & buff);
//    void draw();
    
    int getNumPath(){
        return (int)paths.size();
    }
    
    pesPath & getPathAt(int n){
        return paths[n];
    }
    
//    const vector <pesPath *> & getPaths() const;
    const vector <pesPath> & getPaths() const;
    
    void setFilled(bool t);
    void setFillColor(pesColor col);
    void setStrokeWidth(float f);
    void setStrokeColor(pesColor col);//use to set alpha too
    bool getFilled();
    pesColor getFillColor();
    float getStrokeWidth();
    pesColor getStrokeColor();
    
    //applies to specific path
    void setFilled(bool t,int path);
    void setFillColor(pesColor col,int path);//use to set alpha too
    void setStrokeWidth(float f,int path);
    void setStrokeColor(pesColor col,int path);//use to set alpha too
    bool getFilled(int path);
    pesColor getFillColor(int path);
    float getStrokeWidth(int path);
    pesColor getStrokeColor(int path);
    
    string hexToWeb(pesColor col)
    {
        return "#" +pesToHex(col.r)+pesToHex(col.g)+pesToHex(col.b);
    }
    
    //Functions to build new SVGs
    //reparse is required to update doc after changes, but when adding many
    //paths better to do after all is said and done
    //    void addPath(GUI_Path &path,bool inNewGroup=false);
//    void addPaths(vector<pesPath *> &paths,bool inNewGroup=false);
    void addPaths(const vector<pesPath> &paths,bool inNewGroup=false);
    
    //    void merge(ofxEditableSVG & svg);//add to current
    void save(string file);
    string toString();//return the whole svg as a xml document string
    
    void parseXML(string xml);
    
    void setSize(float w, float h, string unit="px");
    void setPos(pesVec2f pt);
    void setPos(int x, int y);
    int getX();
    int getY();
    int getWidth();
    int getHeight();
    void setViewbox(int x, int y,int w, int h);
    
    //Tesselator crashes when too many commands, test for more than 10000
    void setComplexityThreshold(int i){
        complexityThreshold = i;
    }
    int getComplexityThreshold(){
        return complexityThreshold;
    }
    
private:
    float width, height;
    float x,y;
    
//    vector <pesPath *> paths;
    vector <pesPath> paths;
    
    void setupDiagram(struct pessvg_diagram * diagram);
    void setupShape(struct pessvg_shape * shape);
    
    int complexityThreshold;//to avoid tesselator crash
    bool isFilled;
    pesColor fill;
    pesColor stroke;
    float strokeWidth;
    
    void parseHeader(std::shared_ptr<pugi::xml_document>  doc);
    
    void xmlCreateSVG(pugi::xml_node & extNode, pesPtr<pessvgNode> intNode);
    
    pugi::xml_node parseNode(pugi::xml_node & root, pesPtr<pessvgNode> node);
    
    void setSVGattribute(pesPtr<pessvgNode>  node,string attribute, string value);
    
    pugi::xml_node parsePath(pugi::xml_node & root, const pesPath & path);
    
    pessvgInfo info;
};

#endif /* pesSVG_hpp */
