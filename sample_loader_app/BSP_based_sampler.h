#pragma once

#include <memory>
#include <unordered_map>
#include <variant>
#include <vector>

#include <cstdint>
#include <stack>

#include "svm.h"


std::vector<float> gen_hammersley(uint32_t cnt, float radius);

template<typename TexType>
class BSPBasedSampler
{
  public:
  struct Config
  {
    uint32_t width, height;
    uint32_t additionalSamplesCnt ;
    float radius;
  } config;
  private:

  std::vector<TexType> singleRayData;
  std::vector<float> hammSamples;
  using Line = std::array<float, 3>;

  // Left means that dot(point, split) >= 0 for the point in left child
  struct BSPNode
  {
    uint32_t isleftLeaf:1;
    uint32_t leftIdx:31;
    uint32_t isRightLeaf:1;
    uint32_t rightIdx:31;
    Line split;
  };
  std::vector<BSPNode> nodesCollection;
  std::vector<TexType> subtexelsCollection;
  std::unordered_map<uint32_t, uint32_t> specialTexels;

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
      root.isleftLeaf = 1;
      root.leftIdx = leftLabels[0] + subtexelsCollection.size();
    }
    else
    {
      root.isleftLeaf = 0;
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

  BSPBasedSampler(uint32_t a_width, uint32_t a_height)
  {
    config.width          = a_width;
    config.height         = a_height;
    config.radius         = 0.5f;
    config.additionalSamplesCnt = 64;
    singleRayData.resize(config.width * config.height);
    hammSamples = gen_hammersley(config.additionalSamplesCnt, config.radius);
  }
  BSPBasedSampler(const Config &conf) : config(conf) 
  {
    singleRayData.resize(config.width * config.height);
    hammSamples = gen_hammersley(config.additionalSamplesCnt, config.radius);
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
    std::vector<uint32_t> suspiciosTexelIds;
    for (int y1 = 0; y1 < int(config.height); ++y1) {
      for (int x1 = 0; x1 < int(config.width); ++x1) {

        bool needResample = false;
        const TexType texel = singleRayData[y1 * config.width + x1];
        
        for (int x = std::max(x1 - 1, 0); x <= std::min(x1 + 1, (int)config.width - 1) && !needResample; ++x)
          for (int y = std::max(y1 - 1, 0); y <= std::min(y1 + 1, (int)config.height - 1) && !needResample; ++y)
            needResample = !close_tex_data(singleRayData[y * config.width + x], texel);
      
        if (needResample)
          suspiciosTexelIds.push_back(y1 * config.width + x1);
      }
    }

    const uint32_t samplesCount = config.additionalSamplesCnt; 

    // Process suspicious texels
    std::vector<TexType> samples;
    samples.reserve(samplesCount);
    for (uint32_t texel_idx : suspiciosTexelIds)
    {
      samples.clear();
      samples.push_back(singleRayData[texel_idx]);
      const uint32_t texel_x = texel_idx % config.width;
      const uint32_t texel_y = texel_idx / config.width;
      // Make new samples
      for (uint32_t i = 0; i < config.additionalSamplesCnt; ++i)
      {
        samples.push_back(sampler.sample((texel_x + hammSamples[2 * i]) / config.width, (texel_y + hammSamples[2 * i + 1]) / config.height));
      }

      // Make labels for samples
      std::vector<TexType> referenceSamples;
      std::vector<uint32_t> labels;
      for (uint32_t i = 0; i < samplesCount; ++i)
      {
        bool refFound = false;
        for (uint32_t j = 0, je = referenceSamples.size(); j < je; ++j)
        {
          if (close_tex_data(referenceSamples[j], samples[i]))
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

      if (referenceSamples.size() == 1)
      {
        continue;
      }

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
      }

      std::vector<float> samplesPositions(2, 0);
      samplesPositions.insert(samplesPositions.end(), hammSamples.begin(), hammSamples.end());
      // Remove lines which don't split anything
      for (int i = lines.size() - 1; i >= 0; --i)
      {
        uint32_t leftCnt = 0;
        uint32_t rightCnt = 0;
        for (uint32_t j = 0; j < samples.size(); j += 2)
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
      if (!lines.empty())
      {
        specialTexels[texel_idx] = construct_bsp(lines, labels, samplesPositions, referenceSamples);
        subtexelsCollection.insert(subtexelsCollection.end(), referenceSamples.begin(), referenceSamples.end());
      }
    }
  }

  TexType sample(float x, float y)
  {
    const uint32_t x_texel  = uint32_t( x * float(config.width)  );
    const uint32_t y_texel  = uint32_t( y * float(config.height) );
    const uint32_t texel_id = y_texel * config.width + x_texel;
    if (specialTexels.count(texel_id) == 0)
      return singleRayData[texel_id];

    //return TexType(); // #VF: for debug

    const float x_local = ((x * config.width - x_texel) - 0.5f) * 2.f * config.radius;
    const float y_local = ((y * config.height - y_texel) - 0.5f) * 2.f * config.radius;

    uint32_t currentNode = specialTexels[texel_id];
    while (true)
    {
      const BSPNode &node = nodesCollection[currentNode];
      if (node.split[0] * x_local + node.split[1] * y_local + node.split[2] >= 0.f)
      {
        if (node.isleftLeaf)
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

  TexType& access(float x, float y)
  {
    const uint32_t x_texel  = uint32_t( x * float(config.width)  );
    const uint32_t y_texel  = uint32_t( y * float(config.height) );
    
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
        if (node.isleftLeaf)
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

  void clear(TexType a_val)
  {
    for(uint32_t y = 0; y < config.height; y++) 
    {
      for(uint32_t x = 0; x < config.width; x++) 
      {

        const uint32_t texel_id = y*config.width + x;
        if (specialTexels.count(texel_id) == 0)
          singleRayData[texel_id] = a_val;
        else
        {
          std::stack<uint32_t> nodesToProcess;
          uint32_t currentNode = specialTexels[texel_id];
          
          do 
          {
            BSPNode &node = nodesCollection[currentNode];
            if (!node.isleftLeaf)
              nodesToProcess.push(node.leftIdx);
            else
              subtexelsCollection[node.leftIdx] = a_val;

            if (!node.isRightLeaf)
              nodesToProcess.push(node.rightIdx);
            else
              subtexelsCollection[node.rightIdx] = a_val;
            
            if(nodesToProcess.empty())
              currentNode = 0xFFFFFFFF;
            else
            {
              currentNode = nodesToProcess.top(); 
              nodesToProcess.pop();   
            }
          } 
          while(currentNode != 0xFFFFFFFF);
        }

      }
    }
  }
};
