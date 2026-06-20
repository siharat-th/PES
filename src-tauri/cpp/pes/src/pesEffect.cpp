//
//  pesEffect.cpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 1/10/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#include "pesEffect.hpp"
#include "pesSatinOutline.hpp"
#include "pesCubicSuperPath.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace std;

// MARK: - pesEffect
pesRectangle pesEffect::getBoundingBox(const vector<pesPath> & paths){
    pesRectangle rect;
    bool first = true;
    for(pesPath s : paths){
        if(first){
            rect = s.getBoundingBox();
            first = false;
        }
        else{
            rect.growToInclude(s.getBoundingBox());
        }
    }
    return rect;
}

pesPath pesEffect::createBorderPaths(const pesPolyline &outline){
    //const float padding = 50;
    const float borderWidth = 75;
    //const float inset = padding + borderWidth/2;
    
    pesSatinOutline result;
//    pesSatinOutlineGenerate(const_cast<pesPolyline&>(outline), borderWidth, inset, result);
    pesSatinOutlineGenerate(const_cast<pesPolyline&>(outline), borderWidth, 0, result);
    
    pesPath inner, outer;
    for(int i=0; i<result.length; i++){
        if(i==0){
            inner.moveTo(result.side1[i].x, result.side1[i].y);
            outer.moveTo(result.side2[i].x, result.side2[i].y);
        }
        else{
            inner.lineTo(result.side1[i].x, result.side1[i].y);
            outer.lineTo(result.side2[i].x, result.side2[i].y);
        }
    }
    
    pesPath border;
    border.append(outer);
    border.append(inner);
    return border;
}

vector<pesVec2f> pesEffect::createBorderStitches(const pesPolyline  &outline){
    //const float padding = 50;
    const float borderWidth = 75;
    //const float inset = padding + borderWidth/2;
    
    pesSatinOutline result;
//    pesSatinOutlineGenerate(outline, borderWidth, 0, result);
    pesSatinOutlineGenerate(const_cast<pesPolyline&>(outline), borderWidth, 0, result);
    return pesSatinOutlineRenderStitches(result, 10/2.5);
}

void pesEffect::applyPaths(vector<pesPath> & paths){
    if(applyPaths_cb!=nullptr)
        applyPaths_cb(paths);
}
void pesEffect::applyStitches(shared_ptr<pesData> data){
    if(applyStitches_cb!=nullptr)
        applyStitches_cb(data);
}

pesEffect pesEffect::CreateEffect(pesData::Parameter parameter){
    float angle = parameter.angleValue;
    float radius = parameter.radiusValue;
    int baselineIndex = parameter.shapeIndex;
    bool bItalic = parameter.italic;
    bool bCreateBorder = parameter.border;
    pesEffect effect;
    
      if(baselineIndex==0){
          effect = pesEffectNormal();
          effect.borderGap = 100 + parameter.borderGap;
          effect.borderGapY = 100 + parameter.borderGapY;
      }
      else if(baselineIndex==1){
          effect = pesEffectArchTop();
          ((pesEffectArchTop*)&effect)->angle = angle;
          ((pesEffectArchTop*)&effect)->radius = radius;
          effect.borderGap = 100 + parameter.borderGap;
          effect.borderGapY = 100 + parameter.borderGapY;
      }
      else if(baselineIndex==2){
          effect = pesEffectArchBottom();
          ((pesEffectArchBottom*)&effect)->angle = angle;
          ((pesEffectArchBottom*)&effect)->radius = radius;
          effect.borderGap = 100 + parameter.borderGap;
          effect.borderGapY = 100 + parameter.borderGapY;
      }
      else if(baselineIndex==3){
          effect = pesEffectCircle();
          effect.borderGap = 100 + parameter.borderGap;
          effect.borderGapY = 100 + parameter.borderGapY;
      }
      else if(baselineIndex==4){
          effect = pesEffectSineWave();
          ((pesEffectSineWave*)&effect)->magnitude = parameter.waveMagnitude;
          effect.borderGap = 100 + parameter.borderGap;
          effect.borderGapY = 100 + parameter.borderGapY;
      }
      else if(baselineIndex==5){
          bool bChevronUp = true;
          effect = pesEffectChevron(bChevronUp);
          effect.borderGap = 100 + parameter.borderGap;
          effect.borderGapY = 100 + parameter.borderGapY;
      }
      else if(baselineIndex==6){
          bool bChevronUp = false;
          effect = pesEffectChevron(bChevronUp);
          effect.borderGap = 100 + parameter.borderGap;
          effect.borderGapY = 100 + parameter.borderGapY;
      }
      else if(baselineIndex==7){
          bool bSlantUp = true;
          effect = pesEffectSlant(bSlantUp);
          ((pesEffectSlant*)&effect)->angle = parameter.slantUpAngle;
      }
      else if(baselineIndex==8){
          bool bSlantUp = false;
          effect = pesEffectSlant(bSlantUp);
          ((pesEffectSlant*)&effect)->angle = parameter.slantDownAngle;
      }
      else if(baselineIndex==9){
          effect= pesEffectTriangleUp();
      }
      else if(baselineIndex==10){
          effect= pesEffectTriangleDown();
      }
      else if(baselineIndex==11){
          effect = pesEffectFadeRight();
      }
      else if(baselineIndex==12){
          effect= pesEffectFadeLeft();
      }
      else if(baselineIndex==13){
          effect = pesEffectFadeUp();
          ((pesEffectFadeUp*)&effect)->slantFactor = parameter.fadeUpSlant;
      }
      else if(baselineIndex==14){
          effect = pesEffectFadeDown();
          ((pesEffectFadeDown*)&effect)->slantFactor = parameter.fadeDownSlant;
      }
      else if(baselineIndex==15){
          effect = pesEffectInflate();
      }

    effect.bItalic = bItalic;
    effect.bCreateBorder = bCreateBorder;
    
    return effect;
}



pesEffectNormal::pesEffectNormal()
{
    applyPaths_cb = [=](vector<pesPath> & shapes){
        if (bItalic) {
            pesEffectItalic().applyPaths_cb(shapes);
        }        
        if(bCreateBorder){
            pesRectangle bound = getBoundingBox(shapes);
            
            bound.x -= this->borderGap;
            bound.y -= this->borderGapY;
            bound.width += this->borderGap*2;
            bound.height += this->borderGapY*2;
            
            // rectangle contour
            pesPath contour;
            contour.setMode(pesPath::POLYLINES);
            contour.moveTo(bound.getMaxX(), bound.getMinY()); // start from top-right corner
            contour.lineTo(bound.getMaxX(), bound.getMaxY());
            contour.lineTo(bound.getMinX(), bound.getMaxY());
            contour.lineTo(bound.getMinX(), bound.getMinY());
            contour.close();
            
            shapes.push_back( createBorderPaths(contour.getOutline().front()));
        }

    };
    
    applyStitches_cb = [=](shared_ptr<pesData> data){
        
        if(bCreateBorder){
            pesRectangle bound = data->getBoundingBox();
            
            bound.x -= this->borderGap;
            bound.y -= this->borderGapY;
            bound.width += this->borderGap*2;
            bound.height += this->borderGapY*2;
            
            pesPolyline polyline = pesPolyline::fromRect(bound);
            vector<pesVec2f> vertices = createBorderStitches(polyline);
            pesStitchBlock block;
            for(auto &v:vertices){
                if(block.size()==0){
                    block.addJump(v);
                }
                else{
                    block.addStitch(v);
                }
            }
            
            block.setColorFromIndex(data->fillBlocks.front().colorIndex);
            data->fillBlocks.push_back(block);
            data->recalculate();
        }
        
        if(bItalic){
            pesEffectItalic().applyStitches_cb(data);
            data->recalculate();
        }
        
        
    };
}



pesEffectArchTop::pesEffectArchTop()
{
    borderGap = borderGapY = 0;
    
    applyPaths_cb = [=](vector<pesPath> & shapes){
        if (bItalic) {
            pesEffectItalic().applyPaths_cb(shapes);
        }

        float angle = this->angle;
        if(angle==-1)
            angle = 60;
        this->angle = angle;
        
        pesRectangle rect = getBoundingBox(shapes);
        if(bCreateBorder){
            rect.x -= borderGap;
            rect.y -= borderGapY;
            rect.width += borderGap*2;
            rect.height += borderGapY*2;
        }
        
        float minX = rect.getMinX();
        float maxX = rect.getMaxX();
        float width = rect.width;
        float radius = this->radius;
        if(radius==-1){
            radius = width * 3.0 / TWO_PI;
        }
        this->radius = radius;
        pesVec2f pivot(0, radius);
        
        for(pesPath & s : shapes){
            float cx = s.getBoundingBox().getCenter().x;
            float tx = -cx;
            float ty = 0;
            
            s.translate(tx, ty);
            float degree = pesMap(cx, minX, maxX, -angle, angle);
            s.rotateAround(degree, pivot);
        }
        
        if(bCreateBorder){
            pesVec3f points[4];
            points[0].set(rect.getMaxX(), rect.getMinY());
            points[1].set(rect.getMaxX(), rect.getMaxY());
            points[2].set(rect.getMinX(), rect.getMaxY());
            points[3].set(rect.getMinX(), rect.getMinY());
            
            pesVec3f axis(0,0,1);
            for(int i=0; i<4; i++){
                float cx = points[i].x;
                float tx = -cx;
                float ty = 0;
                
                points[i]+=pesVec2f(tx, ty);
                float degree = pesMap(cx, minX, maxX, -angle, angle);
                points[i].rotate(degree, pivot, axis);
            }
            
            float topRadius = pesDist(pivot.x, pivot.y, 0, rect.getMinY());
            float bottomRadius = pesDist(pivot.x, pivot.y, 0, rect.getMaxY());
            
            // border contour
            pesPath contour;
            contour.setMode(pesPath::POLYLINES);
            contour.setCircleResolution(angle);
            contour.moveTo(points[0].x, points[0].y); // start from top-right corner
            contour.arcNegative(pivot, bottomRadius, bottomRadius, 270+angle, 270-angle);
            contour.arc(pivot, topRadius, topRadius, 270-angle, 270+angle);
            contour.close();
            
            shapes.push_back( createBorderPaths(contour.getOutline().front()));
        }
        
        //if(bItalic){
        //    pesEffectItalic().applyPaths_cb(shapes); 
        //}
    };  
    
    applyStitches_cb = [=](shared_ptr<pesData> data){
        float angle = this->angle;
        if(angle==-1)
            angle = 60;
        this->angle = angle;
        
        pesRectangle rect = data->getBoundingBox();
        if(bCreateBorder){
            rect.x -= borderGap;
            rect.y -= borderGapY;
            rect.width += borderGap*2;
            rect.height += borderGapY*2;
        }
        
        float minX = rect.getMinX();
        float maxX = rect.getMaxX();
        float width = rect.width;
        float radius = this->radius;
        if(radius==-1){
            radius = width * 3.0 / TWO_PI;
        }
        this->radius = radius;
        pesVec2f pivot(0, radius);
        
        for(auto & s : data->fillBlocks.front().polyline.getVertices()){
            float cx = s.x;
            float tx = -cx;
            float ty = 0;
            
            s.x+=tx;
            s.y+=ty;
            float degree = pesMap(cx, minX, maxX, -angle, angle);
            s.rotate(degree, pivot);
        }
        
        if(bCreateBorder){
            pesVec2f points[4];
            points[0].set(rect.getMaxX(), rect.getMinY());
            points[1].set(rect.getMaxX(), rect.getMaxY());
            points[2].set(rect.getMinX(), rect.getMaxY());
            points[3].set(rect.getMinX(), rect.getMinY());
            
            pesVec3f axis(0,0,1);
            for(int i=0; i<4; i++){
                float cx = points[i].x;
                float tx = -cx;
                float ty = 0;
                
                points[i]+=pesVec2f(tx, ty);
                float degree = pesMap(cx, minX, maxX, -angle, angle);
                points[i].rotate(degree, pivot);
            }
            
            float topRadius = pesDist(pivot.x, pivot.y, 0, rect.getMinY());
            float bottomRadius = pesDist(pivot.x, pivot.y, 0, rect.getMaxY());
            
            // border contour
            pesPath polyline;
            polyline.setMode(pesPath::POLYLINES);
            polyline.setCircleResolution(angle);
            polyline.moveTo(points[0].x, points[0].y); // start from top-right corner
            polyline.arcNegative(pivot, bottomRadius, bottomRadius, 270+angle, 270-angle);
            polyline.arc(pivot, topRadius, topRadius, 270-angle, 270+angle);
            polyline.close();
            
            vector<pesVec2f> vertices = createBorderStitches(polyline.getOutline().front());
            pesStitchBlock block;
            for(auto &v:vertices){
                if(block.size()==0){
                    block.addJump(v);
                }
                else{
                    block.addStitch(v);
                }
            }
            
            block.setColorFromIndex(data->fillBlocks.front().colorIndex);
            data->fillBlocks.push_back(block);
            data->recalculate();
        }
        
        if(bItalic){
            pesEffectItalic().applyStitches_cb(data);
            data->recalculate();
        }
    };
}



pesEffectArchBottom::pesEffectArchBottom()
{
    borderGap = borderGapY = 0;
    
    applyPaths_cb = [=](vector<pesPath> & shapes){
        if (bItalic) {
            pesEffectItalic().applyPaths_cb(shapes);
        }

        float angle = this->angle;
        if(angle==-1)
            angle = 60;
        this->angle = angle;
        
        pesRectangle rect = getBoundingBox(shapes);
        if(bCreateBorder){
            rect.x -= borderGap;
            rect.y -= borderGapY;
            rect.width += borderGap*2;
            rect.height += borderGapY*2;
        }
        float minX = rect.getMinX();
        float maxX = rect.getMaxX();
        float width = rect.width;
        float radius = this->radius;
        if(radius==-1){
            radius = width * 3.0 / TWO_PI;
        }
        this->radius = radius;
        pesVec2f pivot(0, -radius);
        
        for(pesPath & s : shapes){
            float cx = s.getBoundingBox().getCenter().x;
            float tx = -cx;
            float ty = 300;
            
            s.translate(tx, ty);
            float degree = pesMap(cx, minX, maxX, angle, -angle);
            s.rotateAround(degree, pivot);
        }
        
        if(bCreateBorder){
            pesVec3f points[4];
            points[0].set(rect.getMaxX(), rect.getMinY());
            points[1].set(rect.getMaxX(), rect.getMaxY());
            points[2].set(rect.getMinX(), rect.getMaxY());
            points[3].set(rect.getMinX(), rect.getMinY());
            
            pesVec3f axis(0,0,1);
            for(int i=0; i<4; i++){
                float cx = points[i].x;
                float tx = -cx;
                float ty = 300;
                
                points[i]+=pesVec2f(tx, ty);
                float degree = pesMap(cx, minX, maxX, angle, -angle);
                points[i].rotate(degree, pivot, axis);
            }
            
            float topRadius = pesDist(pivot.x, pivot.y, 0, rect.getMinY()+300);
            float bottomRadius = pesDist(pivot.x, pivot.y, 0, rect.getMaxY()+300);
            
            // border contour
            pesPath contour;
            contour.setMode(pesPath::POLYLINES);
            contour.setCircleResolution(angle);
            contour.moveTo(points[0].x, points[0].y); // start from top-right corner
            contour.arc(pivot, bottomRadius, bottomRadius, 90-angle, 90+angle);
            contour.arcNegative(pivot, topRadius, topRadius, 90+angle, 90-angle);
            contour.close();
            
            shapes.push_back( createBorderPaths(contour.getOutline().front()));
        }
        
        //if(bItalic){
        //    pesEffectItalic().applyPaths_cb(shapes);
        //}
    };
    
    applyStitches_cb = [=](shared_ptr<pesData> data){
        float angle = this->angle;
        if(angle==-1)
            angle = 60;
        this->angle = angle;
        
        pesRectangle rect = data->getBoundingBox();
        if(bCreateBorder){
            rect.x -= borderGap;
            rect.y -= borderGapY;
            rect.width += borderGap*2;
            rect.height += borderGapY*2;
        }
        float minX = rect.getMinX();
        float maxX = rect.getMaxX();
        float width = rect.width;
        float radius = this->radius;
        if(radius==-1){
            radius = width * 3.0 / TWO_PI;
        }
        this->radius = radius;
        pesVec2f pivot(0, -radius);
        
        for(auto &block:data->fillBlocks){
            for(auto &s:block.polyline){
                float cx = s.x;
                float tx = -cx;
                float ty = 300;
                
                s+=pesVec2f(tx, ty);
                float degree = pesMap(cx, minX, maxX, angle, -angle);
                s.rotate(degree, pivot);
            }
        }
        
        if(bCreateBorder){
            pesVec2f points[4];
            points[0].set(rect.getMaxX(), rect.getMinY());
            points[1].set(rect.getMaxX(), rect.getMaxY());
            points[2].set(rect.getMinX(), rect.getMaxY());
            points[3].set(rect.getMinX(), rect.getMinY());
            
            for(int i=0; i<4; i++){
                float cx = points[i].x;
                float tx = -cx;
                float ty = 300;
                
                points[i]+=pesVec2f(tx, ty);
                float degree = pesMap(cx, minX, maxX, angle, -angle);
                points[i].rotate(degree, pivot);
            }
            
            float topRadius = pesDist(pivot.x, pivot.y, 0, rect.getMinY()+300);
            float bottomRadius = pesDist(pivot.x, pivot.y, 0, rect.getMaxY()+300);
            
            // border contour
            pesPath contour;
            contour.setMode(pesPath::POLYLINES);
            contour.setCircleResolution(angle);
            contour.moveTo(points[0].x, points[0].y); // start from top-right corner
            contour.arc(pivot, bottomRadius, bottomRadius, 90-angle, 90+angle);
            contour.arcNegative(pivot, topRadius, topRadius, 90+angle, 90-angle);
            contour.close();
            
            //            shapes.push_back( createBorder(contour.getOutline().front()));
            vector<pesVec2f> vertices =createBorderStitches(contour.getOutline().front());
            pesStitchBlock block;
            for(auto &v:vertices){
                if(block.size()==0)
                    block.addJump(v);
                else
                    block.addStitch(v);
            }
            block.setColorFromIndex(data->fillBlocks.back().colorIndex);
            data->fillBlocks.push_back(block);
            data->recalculate();
        }
        
        if(bItalic){
            pesEffectItalic().applyStitches_cb(data);
            data->recalculate();
        }
    };
}
//}


pesEffectItalic::pesEffectItalic(){
    applyPaths_cb = [=](vector<pesPath> & shapes){
        // v.2
        const float fontHeight = 300; // default x-height of ppef font
        const float slant = 20 * DEG_TO_RAD;
        const float maxOffsetX = tan(slant) * fontHeight;
        
        for(auto & s:shapes){
            auto & commands = s.getCommands();
            for(auto & c:commands){
                c.to.x+=pesMap(c.to.y, -fontHeight, 0, maxOffsetX, 0);
                if(c.type==pesPath::Command::Type::_bezierTo || c.type==pesPath::Command::Type::_quadBezierTo){
                    c.cp1.x+=pesMap(c.cp1.y, -fontHeight, 0, maxOffsetX, 0);
                    c.cp2.x+=pesMap(c.cp2.y, -fontHeight, 0, maxOffsetX, 0);
                }
            }
        }
    };
    
    applyStitches_cb = [=](shared_ptr<pesData> data){
        const float fontHeight =  data->getBoundingBox().height; //300; // default x-height of ppef font
        const float slant = 20 * DEG_TO_RAD;
        const float maxOffsetX = tan(slant) * fontHeight;
        
        for(auto & block:data->fillBlocks){
            for(auto & v:block.polyline.getVertices()){
                v.x+=pesMap(v.y, -fontHeight, 0, maxOffsetX, 0);
            }
        }
    };
}



pesEffectSineWave::pesEffectSineWave()
{
    applyPaths_cb = [=](vector<pesPath> & shapes){
        if(bItalic){
            pesEffectItalic().applyPaths_cb(shapes);
        }

        const float magnitude = this->magnitude;
        
        pesRectangle bound = getBoundingBox(shapes);
        float minX = bound.getMinX();
        float maxX = bound.getMaxX();
        float minY = bound.getMinY();
        float maxY = bound.getMaxY();
        float width = bound.width;
        float radiusX = width/4;
        //float radiusY = magnitude;
        
        for(auto & s:shapes){
            auto & commands = s.getCommands();
            for(auto & c:commands){
                c.to.y -= sin(pesMap(c.to.x,  minX, maxX, 0, TWO_PI)) * magnitude;
                if(c.type==pesPath::Command::Type::_bezierTo || c.type==pesPath::Command::Type::_quadBezierTo){
                    c.cp1.y-= sin(pesMap(c.cp1.x, minX, maxX, 0, TWO_PI)) * magnitude;
                    c.cp2.y-= sin(pesMap(c.cp2.x, minX, maxX, 0, TWO_PI)) * magnitude;
                }
            }
        }
        
        if(bCreateBorder){
            
            pesCubicSuperPath csp;
            
            csp.addCornerPoint(pesVec2f(maxX, minY)); // top-left
            
            // bottom sine-wave
            csp.addCornerPoint(pesVec2f(maxX, maxY));
            csp.addCurvePoint(pesVec2f(maxX-radiusX, maxY+magnitude));
            csp.addCornerPoint(pesVec2f(minX+width/2, maxY));
            csp.addCurvePoint(pesVec2f(minX+radiusX, maxY-magnitude));
            csp.addCornerPoint(pesVec2f(minX, maxY));
            
            // top sine-wave
            csp.addCornerPoint(pesVec2f(minX, minY));
            csp.addCurvePoint(pesVec2f(minX+radiusX, minY-magnitude));
            csp.addCornerPoint(pesVec2f(minX+width/2, minY));
            csp.addCurvePoint(pesVec2f(maxX-radiusX, minY+magnitude));
            csp.addCornerPoint(pesVec2f(maxX, minY));
            
            csp.calculateCSP();
            
            // border contour
            pesPath contour = csp.path;
            contour.close();
            contour.simplify(0.5);
            
            shapes.push_back( createBorderPaths(contour.getOutline().front()));
        }
        
        //if(bItalic){
        //    pesEffectItalic().applyPaths_cb(shapes);
        //}
        
    };
    
    applyStitches_cb = [=](shared_ptr<pesData> data){
        const float magnitude = this->magnitude;
        
        pesRectangle bound = data->getBoundingBox();
        float minX = bound.getMinX();
        float maxX = bound.getMaxX();
        float minY = bound.getMinY();
        float maxY = bound.getMaxY();
        float width = bound.width;
        float radiusX = width/4;
        //float radiusY = magnitude;
        
        for(auto & block:data->fillBlocks){
            for(auto & v:block.polyline){
                v.y -= sin(pesMap(v.x,  minX, maxX, 0, TWO_PI)) * magnitude;
            }
        }
        
        if(bCreateBorder){
            
            pesCubicSuperPath csp;
            csp.reset();
            
            csp.addCornerPoint(pesVec2f(maxX, minY)); // top-left
            
            // bottom sine-wave
            csp.addCornerPoint(pesVec2f(maxX, maxY));
            csp.addCurvePoint(pesVec2f(maxX-radiusX, maxY+magnitude));
            csp.addCornerPoint(pesVec2f(minX+width/2, maxY));
            csp.addCurvePoint(pesVec2f(minX+radiusX, maxY-magnitude));
            csp.addCornerPoint(pesVec2f(minX, maxY));
            
            // top sine-wave
            csp.addCornerPoint(pesVec2f(minX, minY));
            csp.addCurvePoint(pesVec2f(minX+radiusX, minY-magnitude));
            csp.addCornerPoint(pesVec2f(minX+width/2, minY));
            csp.addCurvePoint(pesVec2f(maxX-radiusX, minY+magnitude));
            csp.addCornerPoint(pesVec2f(maxX, minY));
            
            csp.calculateCSP();
            
            // border contour
            pesPath contour = csp.path;
            contour.close();
            contour.simplify(0.5);
            
            vector<pesVec2f> vertices =createBorderStitches(contour.getOutline().front());
            
            pesStitchBlock block;
            for(auto &v: vertices){
                if(block.size()==0){
                    block.addJump(v);
                }
                else{
                    block.addStitch(v);
                }
            }
            block.setColorFromIndex(data->fillBlocks.back().colorIndex);
            data->fillBlocks.push_back(block);
            data->recalculate();
        }
        
        if(bItalic){
            pesEffectItalic().applyStitches_cb(data);
        }
        
    };
}



pesEffectChevron::pesEffectChevron(bool bChevronTop){
    applyPaths_cb = [=](vector<pesPath> & shapes){
        if (bItalic) {
            pesEffectItalic().applyPaths_cb(shapes);
        }
        pesRectangle bound = getBoundingBox(shapes);
        float minX = bound.getMinX();
        float maxX = bound.getMaxX();
        float minY = bound.getMinY();
        float maxY = bound.getMaxY();
        float width = bound.width;
        float cx = bound.getCenter().x;
        
        const float slant = 15 * DEG_TO_RAD;
        const float yh = tan(slant) * (width/2);
        const float maxOffsetY = bChevronTop ? -yh : yh;
        
        for(auto & s:shapes){
            auto & commands = s.getCommands();
            for(auto & c:commands){
                if(c.to.x<=cx)
                    c.to.y += pesMap(c.to.x,  minX, cx, 0, maxOffsetY);
                else
                    c.to.y += pesMap(c.to.x,  cx, maxX, maxOffsetY, 0);
                
                if(c.type==pesPath::Command::Type::_bezierTo || c.type==pesPath::Command::Type::_quadBezierTo){
                    if(c.cp1.x<=cx)
                        c.cp1.y += pesMap(c.cp1.x,  minX, cx, 0, maxOffsetY);
                    else
                        c.cp1.y += pesMap(c.cp1.x,  cx, maxX, maxOffsetY, 0);
                    
                    if(c.cp2.x<=cx)
                        c.cp2.y += pesMap(c.cp2.x,  minX, cx, 0, maxOffsetY);
                    else
                        c.cp2.y += pesMap(c.cp2.x,  cx, maxX, maxOffsetY, 0);
                }
            }
        }
        
        if(bCreateBorder){
            pesPath contour;
            contour.moveTo(maxX, minY);
            contour.lineTo(maxX, maxY);
            contour.lineTo(bound.getCenter().x, maxY+maxOffsetY);
            contour.lineTo(minX, maxY);
            contour.lineTo(minX, minY);
            contour.lineTo(bound.getCenter().x, minY+maxOffsetY);
            contour.close();
            
            shapes.push_back( createBorderPaths(contour.getOutline().front()));
        }
        
        //if(bItalic){
        //    pesEffectItalic().applyPaths_cb(shapes);
        //}
    };
    
    applyStitches_cb = [=](shared_ptr<pesData> data){
        pesRectangle bound = data->getBoundingBox();
        float minX = bound.getMinX();
        float maxX = bound.getMaxX();
        float minY = bound.getMinY();
        float maxY = bound.getMaxY();
        float width = bound.width;
        float cx = bound.getCenter().x;
        
        const float slant = 15 * DEG_TO_RAD;
        const float yh = tan(slant) * (width/2);
        const float maxOffsetY = bChevronTop ? -yh : yh;
        
        for(auto & block:data->fillBlocks){
            for(auto & v:block.polyline){
                if(v.x<=cx)
                    v.y += pesMap(v.x,  minX, cx, 0, maxOffsetY);
                else
                    v.y += pesMap(v.x,  cx, maxX, maxOffsetY, 0);
            }
        }
        
        if(bCreateBorder){
            pesPath contour;
            contour.moveTo(maxX, minY);
            contour.lineTo(maxX, maxY);
            contour.lineTo(bound.getCenter().x, maxY+maxOffsetY);
            contour.lineTo(minX, maxY);
            contour.lineTo(minX, minY);
            contour.lineTo(bound.getCenter().x, minY+maxOffsetY);
            contour.close();
            
            vector<pesVec2f> vertices =createBorderStitches(contour.getOutline().front());
            
            pesStitchBlock block;
            for(auto &v: vertices){
                if(block.size()==0){
                    block.addJump(v);
                }
                else{
                    block.addStitch(v);
                }
            }
            block.setColorFromIndex(data->fillBlocks.back().colorIndex);
            data->fillBlocks.push_back(block);
            data->recalculate();
        }
        
        if(bItalic){
            pesEffectItalic().applyStitches_cb(data);
        }
        
    };
}

pesEffectCircle::pesEffectCircle(){
    
    applyPaths_cb = [=](vector<pesPath> & shapes){
        if (bItalic) {
            pesEffectItalic().applyPaths_cb(shapes);
        }
        const float startAngle = -90;
        const float endAngle = 260; //267.5;
        
        pesRectangle rect = getBoundingBox(shapes);
        float minX = rect.getMinX();
        float maxX = rect.getMaxX();
        float width = rect.width;
        float radius = width / TWO_PI;
        pesVec2f pivot(0, radius);
        
        for(pesPath & s : shapes){
            float cx = s.getBoundingBox().getCenter().x;
            float tx = -cx;
            float ty = 0;
            
            s.translate(tx, ty);
            float degree = pesMap(cx, minX, maxX, startAngle, endAngle);
            s.rotateAround(degree, pivot);
        }
        
        if(bCreateBorder){
            float topRadius = pesDist(pivot.x, pivot.y, 0, rect.getMinY());
            float bottomRadius = pesDist(pivot.x, pivot.y, 0, rect.getMaxY());
            
            // border contour
            pesPath contour0;
            contour0.setMode(pesPath::POLYLINES);
            contour0.setCircleResolution(60);
            contour0.arcNegative(pivot, bottomRadius, bottomRadius, 0, 360);
            shapes.push_back( createBorderPaths(contour0.getOutline().front()));
            
            pesPath contour;
            contour.setMode(pesPath::POLYLINES);
            contour.setCircleResolution(60);
            contour.circle(pivot, topRadius);
            shapes.push_back( createBorderPaths(contour.getOutline().front()));
        }
        
        //if(bItalic){
        //    pesEffectItalic().applyPaths_cb(shapes);
        //}
    };
    
    applyStitches_cb = [=](shared_ptr<pesData> data){
        const float startAngle = -90;
        const float endAngle = 260; //267.5;
        
        pesRectangle rect = data->getBoundingBox();
        float minX = rect.getMinX();
        float maxX = rect.getMaxX();
        float width = rect.width;
        float radius = width / TWO_PI;
        pesVec2f pivot(0, radius);
        
        for(auto &block : data->fillBlocks){
            for(auto &s : block.polyline){
                float cx = s.x;
                float tx = -cx;
                float ty = 0;
                
                s.translate(tx, ty);
                float degree = pesMap(cx, minX, maxX, startAngle, endAngle);
                s.rotate(degree, pivot);
            }
        }
        
        if(bCreateBorder){
            float topRadius = pesDist(pivot.x, pivot.y, 0, rect.getMinY());
            float bottomRadius = pesDist(pivot.x, pivot.y, 0, rect.getMaxY());
            
            // border contour
            pesPath path0;
            path0.setMode(pesPath::POLYLINES);
            path0.setCircleResolution(60);
            path0.arcNegative(pivot, bottomRadius, bottomRadius, 0, 360);
            vector<pesVec2f> vertices0 =  createBorderStitches(path0.getOutline().front());
            pesStitchBlock block;
            for(auto v:vertices0){
                if(block.size()==0){
                    block.addJump(v);
                }
                else{
                    block.addStitch(v);
                }
            }
            
            pesPath path;
            path.setMode(pesPath::POLYLINES);
            path.setCircleResolution(60);
            path.circle(pivot.x, pivot.y, topRadius);
            vector<pesVec2f> vertices =  createBorderStitches(path.getOutline().front());
            bool first = true;
            for(auto v:vertices){
                if(first){
                    block.addJump(v);
                    first = false;
                }
                else{
                    block.addStitch(v);
                }
            }
            
            block.setColorFromIndex(data->fillBlocks.back().colorIndex);
            data->fillBlocks.push_back(block);
            data->recalculate();
        }
        
        if(bItalic){
            pesEffectItalic().applyStitches_cb(data);
        }
    };
}



pesEffectSlant::pesEffectSlant(bool bSlantUp){
    applyPaths_cb = [=](vector<pesPath> & shapes){
        pesRectangle rect = getBoundingBox(shapes);
        float minX = rect.getMinX();
        float maxX = rect.getMaxX();
        float width = rect.width;
        
        const float slant = this->angle * DEG_TO_RAD;
        const float yh = tan(slant) * width;
        const float maxOffsetY = bSlantUp ? -yh : yh;
        
        for(auto & s:shapes){
            auto & commands = s.getCommands();
            for(auto & c:commands){
                c.to.y += pesMap(c.to.x,  minX, maxX, 0, maxOffsetY);
                if(c.type==pesPath::Command::Type::_bezierTo || c.type==pesPath::Command::Type::_quadBezierTo){
                    c.cp1.y += pesMap(c.cp1.x,  minX, maxX, 0, maxOffsetY);
                    c.cp2.y += pesMap(c.cp2.x,  minX, maxX, 0, maxOffsetY);
                }
            }
        }
    };
    
    applyStitches_cb = [=](shared_ptr<pesData> data){
        pesRectangle rect = data->getBoundingBox();
        float minX = rect.getMinX();
        float maxX = rect.getMaxX();
        float width = rect.width;
        
        const float slant = this->angle * DEG_TO_RAD;
        const float yh = tan(slant) * width;
        const float maxOffsetY = bSlantUp ? -yh : yh;
        
        for(auto & block:data->fillBlocks){
            for(auto & c:block.polyline){
                c.y += pesMap(c.x,  minX, maxX, 0, maxOffsetY);
            }
        }
        data->recalculate();
    };
}


pesEffectFadeRight::pesEffectFadeRight(){
    applyPaths_cb = [=](vector<pesPath> & shapes){
        
        pesRectangle rect = getBoundingBox(shapes);
        float minX = rect.getMinX();
        float maxX = rect.getMaxX();
        float minY = rect.getMinY();
        float maxY = rect.getMaxY();
        //float width = rect.width;
        float height = rect.height;
        
        float fadeHeight = height/3;
        float fadeMinY = rect.getCenter().y - fadeHeight/2;
        float fadeMaxY = rect.getCenter().y + fadeHeight/2;
        
        for(auto & s:shapes){
            auto & commands = s.getCommands();
            for(auto & c:commands){
                float curMinY, curMaxY;
                pesVec2f cur;
                
                cur.set(c.to.x, c.to.y);
                curMinY = pesMap(cur.x, minX, maxX, minY, fadeMinY);
                curMaxY = pesMap(cur.x, minX, maxX, maxY, fadeMaxY);
                c.to.y = pesMap(cur.y, minY, maxY, curMinY, curMaxY);
                
                if(c.type==pesPath::Command::Type::_bezierTo || c.type==pesPath::Command::Type::_quadBezierTo){
                    cur.set(c.cp1.x, c.cp1.y);
                    curMinY = pesMap(cur.x, minX, maxX, minY, fadeMinY);
                    curMaxY = pesMap(cur.x, minX, maxX, maxY, fadeMaxY);
                    c.cp1.y = pesMap(cur.y, minY, maxY, curMinY, curMaxY);
                    
                    cur.set(c.cp2.x, c.cp2.y);
                    curMinY = pesMap(cur.x, minX, maxX, minY, fadeMinY);
                    curMaxY = pesMap(cur.x, minX, maxX, maxY, fadeMaxY);
                    c.cp2.y = pesMap(cur.y, minY, maxY, curMinY, curMaxY);
                }
                
            }
        }
        
    };
    
    applyStitches_cb = [=](shared_ptr<pesData> data){
        pesRectangle rect = data->getBoundingBox();
        float minX = rect.getMinX();
        float maxX = rect.getMaxX();
        float minY = rect.getMinY();
        float maxY = rect.getMaxY();
        //float width = rect.width;
        float height = rect.height;
        
        float fadeHeight = height/3;
        float fadeMinY = rect.getCenter().y - fadeHeight/2;
        float fadeMaxY = rect.getCenter().y + fadeHeight/2;
        
        for(auto & block:data->fillBlocks){
            for(auto & c:block.polyline){
                float curMinY, curMaxY;
                pesVec2f cur;
                cur.set(c.x, c.y);
                curMinY = pesMap(cur.x, minX, maxX, minY, fadeMinY);
                curMaxY = pesMap(cur.x, minX, maxX, maxY, fadeMaxY);
                c.y = pesMap(cur.y, minY, maxY, curMinY, curMaxY);
            }
        }
    };
}



pesEffectFadeLeft::pesEffectFadeLeft(){
    applyPaths_cb = [=](vector<pesPath> & shapes){
        
        pesRectangle rect = getBoundingBox(shapes);
        float minX = rect.getMinX();
        float maxX = rect.getMaxX();
        float minY = rect.getMinY();
        float maxY = rect.getMaxY();
        //float width = rect.width;
        float height = rect.height;
        
        float fadeHeight = height/3;
        float fadeMinY = rect.getCenter().y - fadeHeight/2;
        float fadeMaxY = rect.getCenter().y + fadeHeight/2;
        
        for(auto & s:shapes){
            auto & commands = s.getCommands();
            for(auto & c:commands){
                float curMinY, curMaxY;
                pesVec2f cur;
                
                cur.set(c.to.x, c.to.y);
                curMinY = pesMap(cur.x, minX, maxX, fadeMinY, minY);
                curMaxY = pesMap(cur.x, minX, maxX, fadeMaxY, maxY);
                c.to.y = pesMap(cur.y, minY, maxY, curMinY, curMaxY);
                
                if(c.type==pesPath::Command::Type::_bezierTo || c.type==pesPath::Command::Type::_quadBezierTo){
                    cur.set(c.cp1.x, c.cp1.y);
                    curMinY = pesMap(cur.x, minX, maxX, fadeMinY, minY);
                    curMaxY = pesMap(cur.x, minX, maxX, fadeMaxY, maxY);
                    c.cp1.y = pesMap(cur.y, minY, maxY, curMinY, curMaxY);
                    
                    cur.set(c.cp2.x, c.cp2.y);
                    curMinY = pesMap(cur.x, minX, maxX, fadeMinY, minY);
                    curMaxY = pesMap(cur.x, minX, maxX, fadeMaxY, maxY);
                    c.cp2.y = pesMap(cur.y, minY, maxY, curMinY, curMaxY);
                }
                
            }
        }
        
    };
    
    applyStitches_cb = [=](shared_ptr<pesData> data){
        pesRectangle rect = data->getBoundingBox();
        float minX = rect.getMinX();
        float maxX = rect.getMaxX();
        float minY = rect.getMinY();
        float maxY = rect.getMaxY();
        //float width = rect.width;
        float height = rect.height;
        
        float fadeHeight = height/3;
        float fadeMinY = rect.getCenter().y - fadeHeight/2;
        float fadeMaxY = rect.getCenter().y + fadeHeight/2;
        
        for(auto & block:data->fillBlocks){
            for(auto & c:block.polyline){
                float curMinY, curMaxY;
                pesVec2f cur;
                cur.set(c.x, c.y);
                curMinY = pesMap(cur.x, minX, maxX, fadeMinY, minY);
                curMaxY = pesMap(cur.x, minX, maxX, fadeMaxY, maxY);
                c.y = pesMap(cur.y, minY, maxY, curMinY, curMaxY);
            }
        }
    };
}


pesEffectFadeUp::pesEffectFadeUp(){
    applyPaths_cb = [=](vector<pesPath> & shapes){
        
        pesRectangle rect = getBoundingBox(shapes);
        float minX = rect.getMinX();
        float maxX = rect.getMaxX();
        float minY = rect.getMinY();
        float maxY = rect.getMaxY();
        float width = rect.width;
        
        // v.1 (slant degree)
        //        const float fontHeight = 300;
        //        const float slant = 30 * DEG_TO_RAD;
        //        float dw = tan(slant) * fontHeight;
        
        // v.2 (slant with percent width)
        float percent = this->slantFactor / 100.0;
        float dw = width * percent;
        
        float fadeMinX = minX + dw;
        float fadeMaxX = maxX - dw;
        
        for(auto & s:shapes){
            auto & commands = s.getCommands();
            for(auto & c:commands){
                float curMinX, curMaxX;
                pesVec2f cur;
                
                cur.set(c.to);
                curMinX = pesMap(cur.y, minY, maxY, fadeMinX, minX);
                curMaxX = pesMap(cur.y, minY, maxY, fadeMaxX, maxX);
                c.to.x = pesMap(cur.x, minX, maxX, curMinX, curMaxX);
                
                if(c.type==pesPath::Command::Type::_bezierTo || c.type==pesPath::Command::Type::_quadBezierTo){
                    cur.set(c.cp1);
                    curMinX = pesMap(cur.y, minY, maxY, fadeMinX, minX);
                    curMaxX = pesMap(cur.y, minY, maxY, fadeMaxX, maxX);
                    c.cp1.x = pesMap(cur.x, minX, maxX, curMinX, curMaxX);
                    
                    cur.set(c.cp2);
                    curMinX = pesMap(cur.y, minY, maxY, fadeMinX, minX);
                    curMaxX = pesMap(cur.y, minY, maxY, fadeMaxX, maxX);
                    c.cp2.x = pesMap(cur.x, minX, maxX, curMinX, curMaxX);
                }
                
            }
        }
    };
    
    applyStitches_cb = [=](shared_ptr<pesData> data){
        pesRectangle rect = data->getBoundingBox();
        float minX = rect.getMinX();
        float maxX = rect.getMaxX();
        float minY = rect.getMinY();
        float maxY = rect.getMaxY();
        float width = rect.width;
        
        float percent = this->slantFactor / 100.0;
        float dw = width * percent;
        float fadeMinX = minX + dw;
        float fadeMaxX = maxX - dw;
        
        for(auto & block:data->fillBlocks){
            for(auto & c:block.polyline){
                float curMinX, curMaxX;
                pesVec2f cur;
                cur.set(c.x, c.y);
                curMinX = pesMap(cur.y, minY, maxY, fadeMinX, minX);
                curMaxX = pesMap(cur.y, minY, maxY, fadeMaxX, maxX);
                c.x = pesMap(cur.x, minX, maxX, curMinX, curMaxX);
            }
        }
    };
}



pesEffectFadeDown::pesEffectFadeDown(){
    applyPaths_cb = [=](vector<pesPath> & shapes){
        
        pesRectangle rect = getBoundingBox(shapes);
        float minX = rect.getMinX();
        float maxX = rect.getMaxX();
        float minY = rect.getMinY();
        float maxY = rect.getMaxY();
        float width = rect.width;
        
        // v.1 (slant degree)
        //        const float fontHeight = 300;
        //        const float slant = 30 * DEG_TO_RAD;
        //        float dw = tan(slant) * fontHeight;
        
        // v.2 (slant with percent width)
        float percent = this->slantFactor / 100.0;
        float dw = width * percent;
        
        float fadeMinX = minX + dw;
        float fadeMaxX = maxX - dw;
        
        for(auto & s:shapes){
            auto & commands = s.getCommands();
            for(auto & c:commands){
                float curMinX, curMaxX;
                pesVec2f cur;
                
                cur.set(c.to);
                curMinX = pesMap(cur.y, minY, maxY, minX, fadeMinX);
                curMaxX = pesMap(cur.y, minY, maxY, maxX, fadeMaxX);
                c.to.x = pesMap(cur.x, minX, maxX, curMinX, curMaxX);
                
                if(c.type==pesPath::Command::Type::_bezierTo || c.type==pesPath::Command::Type::_quadBezierTo){
                    cur.set(c.cp1);
                    curMinX = pesMap(cur.y, minY, maxY, minX, fadeMinX);
                    curMaxX = pesMap(cur.y, minY, maxY, maxX, fadeMaxX);
                    c.cp1.x = pesMap(cur.x, minX, maxX, curMinX, curMaxX);
                    
                    cur.set(c.cp2);
                    curMinX = pesMap(cur.y, minY, maxY, minX, fadeMinX);
                    curMaxX = pesMap(cur.y, minY, maxY, maxX, fadeMaxX);
                    c.cp2.x = pesMap(cur.x, minX, maxX, curMinX, curMaxX);
                }
                
            }
        }
    };
    
    applyStitches_cb = [=](shared_ptr<pesData> data){
        pesRectangle rect = data->getBoundingBox();
        float minX = rect.getMinX();
        float maxX = rect.getMaxX();
        float minY = rect.getMinY();
        float maxY = rect.getMaxY();
        float width = rect.width;
        
        float percent = this->slantFactor / 100.0;
        float dw = width * percent;
        float fadeMinX = minX + dw;
        float fadeMaxX = maxX - dw;
        
        for(auto & block:data->fillBlocks){
            for(auto & c:block.polyline){
                float curMinX, curMaxX;
                pesVec2f cur;
                cur.set(c.x, c.y);
                curMinX = pesMap(cur.y, minY, maxY, minX, fadeMinX);
                curMaxX = pesMap(cur.y, minY, maxY, maxX, fadeMaxX);
                c.x = pesMap(cur.x, minX, maxX, curMinX, curMaxX);
            }
        }
    };
}

pesEffectTriangleUp::pesEffectTriangleUp(){
    applyPaths_cb = [=](vector<pesPath> & shapes){
        pesRectangle rect = getBoundingBox(shapes);
        float minX = rect.getMinX();
        float maxX = rect.getMaxX();
        float minY = rect.getMinY();
        float maxY = rect.getMaxY();
        //float width = rect.width;
        float cx = rect.getCenter().x;
        float cy = rect.getCenter().y;
        
        for(auto & s:shapes){
            auto & commands = s.getCommands();
            for(auto & c:commands){
                float curMinY;
                pesVec2f cur;
                
                cur.set(c.to);
                if(cur.x<=cx) curMinY = pesMap(cur.x,  minX, cx, cy, minY);
                else curMinY = pesMap(cur.x, cx, maxX, minY, cy);
                c.to.y = pesMap(cur.y, minY, maxY, curMinY, maxY);
                
                if(c.type==pesPath::Command::Type::_bezierTo || c.type==pesPath::Command::Type::_quadBezierTo){
                    cur.set(c.cp1);
                    if(cur.x<=cx) curMinY = pesMap(cur.x,  minX, cx, cy, minY);
                    else curMinY = pesMap(cur.x, cx, maxX, minY, cy);
                    c.cp1.y = pesMap(cur.y, minY, maxY, curMinY, maxY);
                    
                    cur.set(c.cp2);
                    if(cur.x<=cx) curMinY = pesMap(cur.x,  minX, cx, cy, minY);
                    else curMinY = pesMap(cur.x, cx, maxX, minY, cy);
                    c.cp2.y = pesMap(cur.y, minY, maxY, curMinY, maxY);
                }
            }
        }
    };
    
    applyStitches_cb = [=](shared_ptr<pesData> data){
        pesRectangle rect = data->getBoundingBox();
        float minX = rect.getMinX();
        float maxX = rect.getMaxX();
        float minY = rect.getMinY();
        float maxY = rect.getMaxY();
        //float width = rect.width;
        float cx = rect.getCenter().x;
        float cy = rect.getCenter().y;
        
        for(auto & block:data->fillBlocks){
            for(auto & c:block.polyline){
                float curMinY;
                pesVec2f cur;
                cur.set(c.x, c.y);
                if(cur.x<=cx) curMinY = pesMap(cur.x,  minX, cx, cy, minY);
                else curMinY = pesMap(cur.x, cx, maxX, minY, cy);
                c.y = pesMap(cur.y, minY, maxY, curMinY, maxY);
            }
        }
    };
}

pesEffectTriangleDown::pesEffectTriangleDown(){
    applyPaths_cb = [=](vector<pesPath> & shapes){
        pesRectangle rect = getBoundingBox(shapes);
        float minX = rect.getMinX();
        float maxX = rect.getMaxX();
        float minY = rect.getMinY();
        float maxY = rect.getMaxY();
        //float width = rect.width;
        float cx = rect.getCenter().x;
        float cy = rect.getCenter().y;
        
        for(auto & s:shapes){
            auto & commands = s.getCommands();
            for(auto & c:commands){
                float curMaxY;
                pesVec2f cur;
                
                cur.set(c.to);
                if(cur.x<=cx) curMaxY = pesMap(cur.x,  minX, cx, cy, maxY);
                else curMaxY = pesMap(cur.x, cx, maxX, maxY, cy);
                c.to.y = pesMap(cur.y, minY, maxY, minY, curMaxY);
                
                if(c.type==pesPath::Command::Type::_bezierTo || c.type==pesPath::Command::Type::_quadBezierTo){
                    cur.set(c.cp1);
                    if(cur.x<=cx) curMaxY = pesMap(cur.x,  minX, cx, cy, maxY);
                    else curMaxY = pesMap(cur.x, cx, maxX, maxY, cy);
                    c.cp1.y = pesMap(cur.y, minY, maxY, minY, curMaxY);
                    
                    cur.set(c.cp2);
                    if(cur.x<=cx) curMaxY = pesMap(cur.x,  minX, cx, cy, maxY);
                    else curMaxY = pesMap(cur.x, cx, maxX, maxY, cy);
                    c.cp2.y = pesMap(cur.y, minY, maxY, minY, curMaxY);
                }
            }
        }
    };
    
    applyStitches_cb = [=](shared_ptr<pesData> data){
        pesRectangle rect = data->getBoundingBox();
        float minX = rect.getMinX();
        float maxX = rect.getMaxX();
        float minY = rect.getMinY();
        float maxY = rect.getMaxY();
        //float width = rect.width;
        float cx = rect.getCenter().x;
        float cy = rect.getCenter().y;
        
        for(auto & block:data->fillBlocks){
            for(auto & c:block.polyline){
                float curMaxY;
                pesVec2f cur;
                cur.set(c.x, c.y);
                if(cur.x<=cx) curMaxY = pesMap(cur.x,  minX, cx, cy, maxY);
                else curMaxY = pesMap(cur.x, cx, maxX, maxY, cy);
                c.y = pesMap(cur.y, minY, maxY, minY, curMaxY);
            }
        }
    };
}

pesEffectInflate::pesEffectInflate(){
    applyPaths_cb = [=](vector<pesPath> & shapes){
        pesRectangle rect = getBoundingBox(shapes);
        float minX = rect.getMinX();
        float maxX = rect.getMaxX();
        float minY = rect.getMinY();
        float maxY = rect.getMaxY();
        //float width = rect.width;
        
        const float amplitude = 150;
        
        for(auto & s:shapes){
            auto & commands = s.getCommands();
            for(auto & c:commands){
                pesVec2f cur;
                float curMinY, curMaxY, curAngle;
                
                cur.set(c.to);
                curAngle = pesMap(cur.x, minX, maxX, 0, M_PI);
                curMinY = minY - sin(curAngle) * amplitude;
                curMaxY = maxY + sin(curAngle) * amplitude;
                c.to.y = pesMap(cur.y, minY, maxY, curMinY, curMaxY);
                
                if(c.type==pesPath::Command::Type::_bezierTo || c.type==pesPath::Command::Type::_quadBezierTo){
                    cur.set(c.cp1);
                    curAngle = pesMap(cur.x, minX, maxX, 0, M_PI);
                    curMinY = minY - sin(curAngle) * amplitude;
                    curMaxY = maxY + sin(curAngle) * amplitude;
                    c.cp1.y = pesMap(cur.y, minY, maxY, curMinY, curMaxY);
                    
                    cur.set(c.cp2);
                    curAngle = pesMap(cur.x, minX, maxX, 0, M_PI);
                    curMinY = minY - sin(curAngle) * amplitude;
                    curMaxY = maxY + sin(curAngle) * amplitude;
                    c.cp2.y = pesMap(cur.y, minY, maxY, curMinY, curMaxY);
                }
            }
        }
    };
    
    applyStitches_cb = [=](shared_ptr<pesData> data){
        pesRectangle rect = data->getBoundingBox();
        float minX = rect.getMinX();
        float maxX = rect.getMaxX();
        float minY = rect.getMinY();
        float maxY = rect.getMaxY();
        //float width = rect.width;
        
        const float amplitude = rect.height/2; // 150
        
        for(auto & block : data->fillBlocks){
            for(auto & c:block.polyline){
                pesVec2f cur;
                float curMinY, curMaxY, curAngle;
                
                cur.set(c.x, c.y);
                curAngle = pesMap(cur.x, minX, maxX, 0, M_PI);
                curMinY = minY - sin(curAngle) * amplitude;
                curMaxY = maxY + sin(curAngle) * amplitude;
                c.y = pesMap(cur.y, minY, maxY, curMinY, curMaxY);
            }
        }
    };
}

