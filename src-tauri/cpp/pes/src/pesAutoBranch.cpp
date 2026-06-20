//
//  pesAutoBranch.cpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 9/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//
#include "include/core/SkTypes.h"
#include "pesAutoBranch.hpp"
#include "pesMath.hpp"
#include <float.h>
#include <deque>
#include <climits>
#include <cstring>

using namespace std;

// travelling salesman problem
#define MAX_CITY    10
// Fixed by Github Copilot - GPT 4o
int tsp(const vector<vector<int>>& graph, int numCity, int city, vector<int>& results) {
    vector<int> vertex;
    for (int i = 0; i < numCity; i++) {
        if (i != city) {
            vertex.push_back(i);
        }
    }

    int min_path = INT_MAX;
    do {
        int current_pathweight = 0;
        int k = city;
        for (int i = 0; i < vertex.size(); i++) {
            current_pathweight += graph[k][vertex[i]];
            k = vertex[i];
        }
        current_pathweight += graph[k][city];
        if (current_pathweight < min_path) {
            min_path = current_pathweight;
            results = vertex;
        }
    } while (next_permutation(vertex.begin(), vertex.end())); // Generate all (numCity-1)! permutations of cities.

    return min_path;
}

void pesFindNearestEnterAndExit(vector<pesPolyline> & patches, vector<pesNearestBranchingResult> & results,
                             bool bKeepOrder, bool bAccurateClosestPoint){
    pesVec2f last(0, 0);
    // int graph[MAX_CITY][MAX_CITY];
    // memset(graph, 0, MAX_CITY*MAX_CITY*sizeof(int));
    // Fixed by Github Copilot - GPT 4o
    int numCity = static_cast<int>(patches.size());
    vector<vector<int>> graph(numCity, vector<int>(numCity, 0));
    vector<int> route;
    
    if(bKeepOrder==false){
        int numCity = (int)patches.size();
//        GUI_LogNotice("tsp", "max_city=%i num_city=%i", MAX_CITY, numCity);
        if(numCity<=MAX_CITY){
            for(int r=0; r<numCity; r++){
                for(int c=0; c<numCity; c++){
                    if(r!=c){
                        if(bAccurateClosestPoint){
                            float dist = INFINITY;
                            for(int first=0; first<patches[r].size(); first++){
                                for(int second=0; second<patches[c].size(); second++){
                                    float d = patches[r][first].distance(patches[c][second]);
                                    if(d<dist){
                                        dist = d;
                                    }
                                }
                            }
                            graph[r][c] = dist;
                        }
                        else{
                            graph[r][c] = patches[r].getClosestPoint(patches[c][0]).distance(patches[c][0]);
                        }
                    }
                }
            }
//            GUI_LogNotice("travellingSalesmanProblem") << tsp(graph, numCity, city, route);
            
            const int firstCity = 0;
            tsp(graph, numCity, firstCity, route);
            auto bk = patches;
            patches.clear();
            patches.push_back(bk[firstCity]);
            for(int i=0; i<route.size(); i++){
                patches.push_back(bk[route[i]]);
            }
        }
        else{
            std::deque<pesPolyline> qp;
            qp.insert(qp.end(), patches.begin(), patches.end());
            patches.clear();
            patches.push_back( qp.front());
            qp.pop_front();
            last = patches.back().getVertices().back();
            
            float minDist = FLT_MAX;
            while (qp.size()) {
                int minIdx=0;
                minDist = FLT_MAX;
                for(int i=0; i<qp.size(); i++){
                    auto p = qp.at(i);
                    unsigned int nearestIndex;
                    pesVec2f nearestPoint = p.getClosestPoint(last, &nearestIndex);
                    float dist = nearestPoint.distance(last);
                    if(dist<minDist){
                        minDist = dist;
                        minIdx = i;
                    }
                }
                
                patches.push_back(qp.at(minIdx));
                qp.erase(qp.begin() + minIdx);
                last = patches.back().getVertices().back();
            }
        }
    }
    
    last.set(0, 0);
    for(int i=0; i<patches.size(); i++){
        int enterIndex = 0;
        pesVec2f enterPoint;
        if(i==0){
            enterIndex = 0;
            enterPoint = patches[i].getVertices().front();
        }
        else{
            unsigned int nearestIndex;
            patches[i].getClosestPoint(last, &nearestIndex);
            
            enterPoint = patches[i][nearestIndex];
            enterIndex = nearestIndex;
        }
        
        
        // find exitPoint
        int exitIndex;
        pesVec2f exitPoint;
        if(i==patches.size()-1){
            exitIndex = (int)patches[i].size() - 1;
            exitPoint = patches[i].getVertices().back();
        }
        else{
            exitIndex = (int)patches[i].size() - 1;
            exitPoint = patches[i].getVertices().back();
            
            // float dist = INFINITY;
            // Fixed by Github Copilot - GPT 4o
            float dist = std::numeric_limits<float>::infinity();
            for(int first=0; first<patches[i].size(); first++){
                for(int second=0; second<patches[i+1].size(); second++){
                    float d = patches[i][first].distance(patches[i+1][second]);
                    if(d<dist){
                        dist = d;
                        exitIndex = first;
                    }
                }
            }
        }
        
        pesNearestBranchingResult n;
        n.enterIndex=enterIndex;
        n.exitIndex=exitIndex;
        n.patchesIndex=i;
        results.push_back(n);
        last.set(patches[i][exitIndex].x, patches[i][exitIndex].y);
    }
}
