#pragma once

#include <memory>
#include <unordered_map>
#include <variant>
#include <vector>

#include <cstdint>
#include <stack>
#include <iomanip> // for printing integer with leading zerows
#include <fstream>
#include <sstream>

#include "svm.h"

void PlaneHammersley(float *result, int n);

#define STORE_LABELS

template<typename TexType>
class SubPixelImageBSP
{
  public:
  struct Config
  {
    uint32_t width, height;
    uint32_t windowHalfSize = 1;
    uint32_t additionalSamplesCnt = 128;
    float radius = 0.5f;
  } config;
  private:

  std::vector<TexType> singleRayData;
  std::vector<float> hammSamples;
  using Line = std::array<float, 3>;

  // Left means that dot(point, split) >= 0 for the point in left child
  struct BSPNode
  {
    uint32_t isLeftLeaf:1;
    uint32_t leftIdx:31;
    uint32_t isRightLeaf:1;
    uint32_t rightIdx:31;
    Line split;
  };
  std::vector<BSPNode> nodesCollection;
  std::vector<TexType> subtexelsCollection;
  std::unordered_map<uint32_t, uint32_t> specialTexels;

  #ifdef STORE_LABELS
  std::unordered_map<uint32_t,  std::vector<uint32_t> > specialLabels;
  #endif


  uint32_t construct_bsp(std::vector<Line> lines, const std::vector<uint32_t> &labels, const std::vector<float> &samples, const std::vector<TexType> &texData)
  {
    uint32_t bestLineIdx = 0;
    uint32_t bestLineScore = config.additionalSamplesCnt + 1; // Amount of wrong classified samples (wrong mean that major amount of samples with the same label are on another side of the line)
    for (uint32_t lineId = 0; lineId < lines.size() && bestLineScore > 0; ++lineId)
    {
      const Line &line = lines[lineId];
      std::unordered_map<uint32_t, uint32_t> leftLabels;
      std::unordered_map<uint32_t, uint32_t> rightLabels;
      for (uint32_t sampleId = 0, sampleEnd = labels.size(); sampleId < sampleEnd; sampleId++)
      {
        if (line[0] * samples[sampleId * 2] + line[1] * samples[sampleId * 2 + 1] + line[2] >= 0.f)
        {
          leftLabels[labels[sampleId]]++;
        }
        else
        {
          rightLabels[labels[sampleId]]++;
        }
      }
      uint32_t score = 0;
      for (auto leftIt : leftLabels)
      {
        if (rightLabels.count(leftIt.first) != 0)
        {
          score += std::min(leftIt.second, rightLabels[leftIt.first]);
        }
      }
      if (score < bestLineScore)
      {
        bestLineScore = score;
        bestLineIdx = lineId;
      }
    }

    // Prepare subnodes data  
    std::vector<uint32_t> leftLabels;
    std::vector<uint32_t> rightLabels;
    std::vector<float> leftSamples;
    std::vector<float> rightSamples;

    const Line &line = lines[bestLineIdx];
    bool leftIsLeaf = true;
    bool rightIsLeaf = true;

    for (uint32_t i = 0, ie = labels.size(); i < ie; ++i)
    {
      if (line[0] * samples[2 * i] + line[1] * samples[2 * i + 1] + line[2] >= 0.f)
      {
        if (!leftLabels.empty())
        {
          leftIsLeaf &= leftLabels.back() == labels[i];
        }
        leftLabels.push_back(labels[i]);
        leftSamples.push_back(samples[2 * i]);
        leftSamples.push_back(samples[2 * i + 1]);
      }
      else
      {
        if (!rightLabels.empty())
        {
          rightIsLeaf &= rightLabels.back() == labels[i];
        }
        rightLabels.push_back(labels[i]);
        rightSamples.push_back(samples[2 * i]);
        rightSamples.push_back(samples[2 * i + 1]);
      }
    }

    std::vector<Line> leftLines = lines;

    for (int i = leftLines.size() - 1; i >= 0; --i)
    {
      uint32_t leftCnt = 0;
      uint32_t rightCnt = 0;
      for (uint32_t j = 0; j < leftSamples.size(); j += 2)
      {
        if (leftLines[i][0] * leftSamples[j] + leftLines[i][1] * leftSamples[j + 1] + leftLines[i][2] > 0.0f)
        {
          leftCnt++;
        }
        else
        {
          rightCnt++;
        }
      }
      if (leftCnt == 0 || rightCnt == 0)
      {
        leftLines.erase(leftLines.begin() + i);
      }
    }

    std::vector<Line> rightLines = lines;

    for (int i = rightLines.size() - 1; i >= 0; --i)
    {
      uint32_t leftCnt = 0;
      uint32_t rightCnt = 0;
      for (uint32_t j = 0; j < rightSamples.size(); j += 2)
      {
        if (rightLines[i][0] * rightSamples[j] + rightLines[i][1] * rightSamples[j + 1] + rightLines[i][2] > 0.0f)
        {
          leftCnt++;
        }
        else
        {
          rightCnt++;
        }
      }
      if (leftCnt == 0 || rightCnt == 0)
      {
        rightLines.erase(rightLines.begin() + i);
      }
    }

    uint32_t nodeIdx = nodesCollection.size();
    nodesCollection.resize(nodesCollection.size() + 1);
    BSPNode &root = nodesCollection.back();
    root.split = line;
    if (leftIsLeaf || leftLines.empty())
    {
      root.isLeftLeaf = 1;
      root.leftIdx = leftLabels[0] + subtexelsCollection.size();
    }
    else
    {
      root.isLeftLeaf = 0;
      root.leftIdx = construct_bsp(leftLines, leftLabels, leftSamples, texData);
    }

    if (rightIsLeaf || rightLines.empty())
    {
      root.isRightLeaf = 1;
      root.rightIdx = rightLabels[0] + subtexelsCollection.size();
    }
    else
    {
      root.isRightLeaf = 0;
      root.rightIdx = construct_bsp(rightLines, rightLabels, rightSamples, texData);
    }
    return nodeIdx;
  }

public:
  SubPixelImageBSP(const Config &conf) : config(conf) 
  {
    singleRayData.resize(config.width * config.height);
    hammSamples.resize(config.additionalSamplesCnt*2);
    PlaneHammersley(hammSamples.data(), config.additionalSamplesCnt);
    for (float& v : hammSamples)                                       // [0,1] ==> [-0.5,0.5]
      v = (v - 0.5f) * 2.0f * config.radius;
  }

  SubPixelImageBSP(uint32_t a_width, uint32_t a_height, float a_radius = 0.5f)
  {
    config.width          = a_width;
    config.height         = a_height;
    config.radius         = a_radius;
    config.additionalSamplesCnt = 64;
    singleRayData.resize(config.width * config.height);
    hammSamples.resize(config.additionalSamplesCnt*2);
    PlaneHammersley(hammSamples.data(), config.additionalSamplesCnt);
    for (float& v : hammSamples)                                      // [0,1] ==> [-0.5,0.5]
      v = (v - 0.5f) * 2.0f * config.radius;
  }

  struct uint2 {
    uint2() : x(0), y(0) {}
    uint2(uint32_t a_x, uint32_t a_y) : x(a_x), y(a_y) {}
    uint32_t x, y;
  };

  std::vector<TexType> RemoveDuplicatesAndMakeSVMLabels(const std::vector<TexType>& samples, std::vector<uint32_t>& labels)
  {
    std::vector<TexType> referenceSamples; 
    referenceSamples.reserve(samples.size());
    
    labels.reserve(samples.size());
    labels.clear();

    for (uint32_t i = 0; i < samples.size(); ++i)
    {
      bool refFound = false;
      for (uint32_t j = 0, je = referenceSamples.size(); j < je; ++j)
      {
        if (referenceSamples[j] == samples[i])
        {
          labels.push_back(j);
          refFound = true;
          break;
        }
      }

      if (!refFound)
      {
        labels.push_back(referenceSamples.size());
        referenceSamples.push_back(samples[i]);
      }
    }

    return referenceSamples;
  }

  std::vector<Line> GetLinesSVM(const std::vector<TexType>& referenceSamples, const std::vector<uint32_t>& labels)
  {
    std::vector<Line> lines; 
    const uint32_t splitLinesCnt = referenceSamples.size() * (referenceSamples.size() - 1) / 2;
    for (uint32_t lineId = 0, cluster1Id = 0, cluster2Id = referenceSamples.size() - 1; lineId < splitLinesCnt; ++lineId)
    {
      // Prepare data to train SVM
      std::vector<float> localSamples;
      std::vector<int> localLabels;
      for (uint32_t i = 0; i < labels.size(); ++i)
      {
        if (labels[i] == cluster1Id || labels[i] == cluster2Id)
        {
          localLabels.push_back(labels[i] == cluster1Id ? 1 : -1);
          if (i == 0)
          {
            localSamples.resize(2, 0.f);
          }
          else
          {
            localSamples.push_back(hammSamples[i * 2 - 2]);
            localSamples.push_back(hammSamples[i * 2 - 1]);
          }
        }
      }

      // Train SVM
      SVM svm;
      svm.fit(localSamples, localLabels, localSamples, localLabels);
      lines.push_back(svm.get_weights());
      // Update cluster ids
      cluster1Id++;
      if (cluster1Id == cluster2Id)
      {
        cluster1Id = 0;
        cluster2Id--;
      }

    } // end of 'for (uint32_t lineId = 0, cluster1Id = 0, cluster2Id = referenceSamples.size() - 1; lineId < splitLinesCnt; ++lineId)'
    return lines;
  }

  std::vector<float> RemoveBadLines(std::vector<Line>& lines)
  {
    std::vector<float> samplesPositions(2, 0);
    samplesPositions.insert(samplesPositions.end(), hammSamples.begin(), hammSamples.end());
    // Remove lines which don't split anything
    for (int i = lines.size() - 1; i >= 0; --i)
    {
      uint32_t leftCnt = 0;
      uint32_t rightCnt = 0;
      for (uint32_t j = 0; j < samplesPositions.size(); j += 2)
      {
        if (lines[i][0] * samplesPositions[j] + lines[i][1] * samplesPositions[j + 1] + lines[i][2] > 0.0f)
        {
          leftCnt++;
        }
        else
        {
          rightCnt++;
        }
      }
      if (leftCnt == 0 || rightCnt == 0)
      {
        lines.erase(lines.begin() + i);
      }
    }
    return samplesPositions;
  }

  std::vector<uint2> GetSuspiciosTexels(const std::vector<TexType>& a_singleRayData)
  {
    std::vector<uint2> suspiciosTexelIds;
    for (int i = 0; i < int(config.width); ++i) {
      for (int j = 0; j < int(config.height); ++j) {
        
        bool needResample   = false;
        const TexType texel = a_singleRayData[i * config.width + j];
        
        for (int x = std::max(i - 1, 0); x <= std::min(i + 1, (int)config.width - 1) && !needResample; ++x)
          for (int y = std::max(j - 1, 0); y <= std::min(j + 1, (int)config.height - 1) && !needResample; ++y)
            needResample = (a_singleRayData[y * config.width + x] != texel);
        
        if (needResample)
          suspiciosTexelIds.push_back(uint2(i,j));
      }
    }
    return suspiciosTexelIds;
  }
  
  template<typename BaseSampler>
  void configure(const BaseSampler &sampler)
  {
    // Fill initial grid. One ray per texel.
    for (uint32_t y = 0; y < config.height; ++y) {
      for (uint32_t x = 0; x < config.width; ++x) {
        singleRayData[y * config.width + x] = sampler.fetch(x, y);
      }
    }

    // Collect suspicious (aliased, with high divergence in neighbourhood) texels.
    std::vector<uint2> suspiciosTexelIds = GetSuspiciosTexels(singleRayData);
   
    const uint32_t samplesCount = config.additionalSamplesCnt + 1;

    // Process suspicious texels
    std::vector<TexType> samples;
    samples.reserve(samplesCount);
    for (auto texel_idx : suspiciosTexelIds)
    {
      samples.clear();
      samples.push_back(singleRayData[texel_idx.y*config.width+texel_idx.x]);
      const uint32_t texel_x = texel_idx.x;
      const uint32_t texel_y = texel_idx.y;
      // Make new samples
      for (uint32_t i = 0; i < config.additionalSamplesCnt; ++i)
      {
        samples.push_back(sampler.sample((texel_x + 0.5f + hammSamples[2 * i + 0]) / float(config.width), 
                                         (texel_y + 0.5f + hammSamples[2 * i + 1]) / float(config.height)));
      }

      // Make labels for samples
      //
      std::vector<uint32_t> labels; 
      std::vector<TexType> referenceSamples = RemoveDuplicatesAndMakeSVMLabels(samples, labels);

      if (referenceSamples.size() == 1)
        continue;

      #ifdef STORE_LABELS
      specialLabels[texel_idx.y*config.width + texel_idx.x] = labels;
      #endif

      std::vector<Line> lines = GetLinesSVM(referenceSamples, labels);
      
      std::vector<float> samplesPositions = RemoveBadLines(lines);
      if (!lines.empty())
      {
        specialTexels[texel_idx.y*config.width + texel_idx.x] = construct_bsp(lines, labels, samplesPositions, referenceSamples);
        subtexelsCollection.insert(subtexelsCollection.end(), referenceSamples.begin(), referenceSamples.end());
      }
    }
  }

  TexType& access(float x, float y)
  {
    const uint32_t x_texel = x * config.width;
    const uint32_t y_texel = y * config.height;
    const uint32_t texel_id = y_texel * config.width + x_texel;
    if (specialTexels.count(texel_id) == 0)
      return singleRayData[texel_id];
    
    const float x_local = ((x * config.width - x_texel) - 0.5f) * 2.f * config.radius;
    const float y_local = ((y * config.height - y_texel) - 0.5f) * 2.f * config.radius;

    uint32_t currentNode = specialTexels[texel_id];
    while (true)
    {
      const BSPNode &node = nodesCollection[currentNode];
      if (node.split[0] * x_local + node.split[1] * y_local + node.split[2] >= 0.f)
      {
        if (node.isLeftLeaf)
        {
          return subtexelsCollection[node.leftIdx];
        }
        currentNode = node.leftIdx;
      }
      else
      {
        if (node.isRightLeaf)
        {
          return subtexelsCollection[node.rightIdx];
        }
        currentNode = node.rightIdx;
      }
    }
  }
  
  TexType sample(float x, float y) { return access(x,y); }

  //TexType sample(float x, float y) // for debug, draw black pixels on boundaries
  //{
  //  const uint32_t x_texel = x * config.width;
  //  const uint32_t y_texel = y * config.height;
  //  const uint32_t texel_id = y_texel * config.width + x_texel;
  //  if (specialTexels.count(texel_id) == 0)
  //    return singleRayData[texel_id];
  //  
  //  return TexType();
  //}

  void dumpSamples(const char* a_path)
  {
    std::stringstream ss;
    ss << a_path << "a_samples.bin";
    int add_samples = int(hammSamples.size()/2) + 1;
    std::ofstream out(ss.str(), std::ios::binary | std::ios::out);
    out.write((char*)&add_samples, sizeof(add_samples));
    float center[2] = {0.f, 0.f};
    out.write((char*)center, sizeof(center));
    out.write((char*)hammSamples.data(), sizeof(float) * hammSamples.size());
    out.close();
  }

  void dumpPixel(const char* a_path, uint32_t x_texel, uint32_t y_texel)
  {
    const uint32_t texel_id = y_texel * config.width + x_texel;
    if (specialTexels.count(texel_id) == 0)
      return;
    
    std::stack<uint32_t> nodesToProcess;
    std::vector<BSPNode> nodesToPrint; 
    uint32_t currentNode = specialTexels[texel_id];

    while(true)
    {
      const BSPNode &node = nodesCollection[currentNode];
      nodesToPrint.push_back(node);
      
      if (!node.isLeftLeaf && !node.isRightLeaf)
      {
        currentNode = node.leftIdx;
        nodesToProcess.push(node.rightIdx);
      }
      else if(!node.isLeftLeaf)
        currentNode = node.leftIdx;
      else if(!node.isRightLeaf)
        currentNode = node.rightIdx;
      else
      {
        if(nodesToProcess.empty())
          break;
        else
        {
          currentNode = nodesToProcess.top();
          nodesToProcess.pop();
        }
      }  
    }  
     
    std::string fileName = std::string(a_path) + "pix_";
    std::stringstream fNameStream;
    fNameStream << a_path << "pix_" << std::setfill('0') << std::setw(4) << x_texel << "_" << std::setfill('0') << std::setw(4) << config.height-y_texel-1 << ".bin";
    
    std::ofstream fout(fNameStream.str(), std::ios::out | std::ios::binary);
    int add_samples = int(nodesToPrint.size());
    fout.write((char*)&add_samples, sizeof(add_samples));
    for(auto node : nodesToPrint)
      fout.write((char*)&node.split[0], sizeof(float)*3);
    
    #ifdef STORE_LABELS
    const auto& labels = specialLabels[texel_id];
    for(uint32_t label : labels)
      fout.write((char*)&label, sizeof(uint32_t));
    #endif
    fout.close();
  }

  // std::vector<float> hammSamples(add_samples * 2);
  // std::vector<int> labels(ids.size());
  // auto weights = svm.get_weights();
  // const float a = weights[0];
  // const float b = weights[1];
  // const float c = weights[2];

  //std::ofstream out(ss.str(), std::ios::binary | std::ios::out);
  //out.write((char*)&add_samples, sizeof(add_samples));
  //out.write((char*)hammSamples.data(), sizeof(float) * hammSamples.size());
  //out.write((char*)labels.data(), sizeof(labels[0]) * labels.size());
  //out.write((char*)weights.data(), sizeof(float) * weights.size());
  //out.close();
  
};
