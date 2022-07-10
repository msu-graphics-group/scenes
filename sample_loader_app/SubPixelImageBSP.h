#pragma once

#include <bitset>
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
#include "render/raytracing.h"

void PlaneHammersley(float *result, int n);

//#define STORE_LABELS
//typedef SurfaceInfo TexType;

extern int epoches_to_improve;

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

  Line NormalizeLine(const Line& a_rhs) 
  {
    Line res;
    if(a_rhs[2] == 0.0f)
    {
      if(std::abs(a_rhs[0]) > std::abs(a_rhs[1]))
      {
        res[0] = 1.0f;
        res[1] = a_rhs[1]/a_rhs[0];
      }
      else
      {
        res[0] = a_rhs[0]/a_rhs[1];
        res[1] = 1.0f;
      }
      res[2] = 0.0f;
    }
    else
    {
      res[0] = a_rhs[0]/a_rhs[2];
      res[1] = a_rhs[1]/a_rhs[2];
      res[2] = 1.0f;
    }
    return res;
  }

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
  std::vector<uint32_t> geomIdsPerPixel;
  std::unordered_map<uint32_t,  std::vector<uint32_t> > specialLabels;
  #endif

  uint32_t BadSplits(const std::vector<Line>& lines, const std::vector<uint32_t>& labels, const std::vector<float>& samples)
  {
    uint32_t bestLineScore = config.additionalSamplesCnt + 1; // Amount of wrong classified samples (wrong mean that major amount of samples with the same label are on another side of the line)
    std::vector<uint32_t> leftLabelsTemp;
    std::vector<uint32_t> rightLabelsTemp;
    for (uint32_t lineId = 0; lineId < lines.size() && bestLineScore > 0; ++lineId)
    {
      leftLabelsTemp.clear();
      rightLabelsTemp.clear();
      const Line &line = lines[lineId];
      for (uint32_t sampleId = 0, sampleEnd = labels.size(); sampleId < sampleEnd; sampleId++)
      {
        const auto currLabel   = labels[sampleId];
        const float signedDist = line[0] * samples[sampleId * 2] + line[1] * samples[sampleId * 2 + 1] + line[2];
        if (signedDist >= 0.f)
        {
          if (leftLabelsTemp.size() <= currLabel)
            leftLabelsTemp.resize(currLabel + 1, 0);
          leftLabelsTemp[currLabel]++;
        }
        else
        {
          if (rightLabelsTemp.size() <= currLabel)
            rightLabelsTemp.resize(currLabel + 1, 0);
          rightLabelsTemp[currLabel]++;
        }
      }
      uint32_t score = 0;
      for (uint32_t i = 0, ie = std::min(leftLabelsTemp.size(), rightLabelsTemp.size()); i < ie; ++i)
      {
        score += std::min(leftLabelsTemp[i], rightLabelsTemp[i]);
      }
      if (score < bestLineScore)
        bestLineScore = score;
    }

    return bestLineScore;
  }

  static const uint32_t MAX_SAMPLES_COUNT = 256;

  struct BSPBuildContext
  {
    std::vector<std::bitset<MAX_SAMPLES_COUNT>> lineSampleCache;
    const std::vector<Line> &lines;
    const std::vector<uint32_t> &labels;
    const std::vector<float> &samples;
    const std::vector<TexType> &texData;
    std::vector<std::bitset<MAX_SAMPLES_COUNT>> labelsPerSet;
    BSPBuildContext(const std::vector<Line> &lines, const std::vector<uint32_t> &labels, const std::vector<float> &samples, const std::vector<TexType> &texData) :
    lines(lines), labels(labels), samples(samples), texData(texData)
    {
      lineSampleCache.resize(lines.size());
      for (uint32_t i = 0; i < lines.size(); ++i)
      {
        for (uint32_t j = 0; j < labels.size(); ++j)
        {
          const float signedDist = lines[i][0] * samples[j * 2] + lines[i][1] * samples[j * 2 + 1] + lines[i][2];
          lineSampleCache[i].set(j, signedDist >= 0.0f);
        }
      }

      for (uint32_t i = 0; i < labels.size(); ++i)
      {
        if (labels[i] >= labelsPerSet.size())
          labelsPerSet.resize(labels[i] + 1);
        labelsPerSet[labels[i]].set(i);
      }
    }
  };

  uint32_t construct_bsp(const std::vector<Line> &lines, const std::vector<uint32_t> &labels, const std::vector<float> &samples, const std::vector<TexType> &texData)
  {
    BSPBuildContext context(lines, labels, samples, texData);
    std::bitset<MAX_SAMPLES_COUNT> samplesIndices;
    samplesIndices.flip();
    std::vector<uint32_t> linesIndices(lines.size());
    for (uint32_t i = 0; i < linesIndices.size(); ++i)
    {
      linesIndices[i] = i;
    }
    return construct_bsp(context, samplesIndices, linesIndices);
  }

  uint32_t construct_bsp(BSPBuildContext &context, const std::bitset<MAX_SAMPLES_COUNT> &samples_indices, const std::vector<uint32_t> &lines_indices)
  {
    uint32_t bestLineIdx = 0;
    uint32_t bestLineScore = config.additionalSamplesCnt + 1; // Amount of wrong classified samples (wrong mean that major amount of samples with the same label are on another side of the line)
    for (uint32_t lineId : lines_indices)
    {
      uint32_t score = 0;
      for (uint32_t i = 0; i < context.labelsPerSet.size(); ++i)
      {
        const auto currentSet = context.labelsPerSet[i] & samples_indices;
        score += std::min(
          (currentSet & context.lineSampleCache[lineId]).count(),
          (currentSet & (~context.lineSampleCache[lineId])).count()
        );
      }
      if (score < bestLineScore)
      {
        bestLineScore = score;
        bestLineIdx = lineId;
      }
      if (bestLineScore == 0)
        break;
    }

    // Prepare subnodes data

    const Line &line = context.lines[bestLineIdx];
    bool leftIsLeaf = false;
    bool rightIsLeaf = false;

    std::bitset<MAX_SAMPLES_COUNT> leftSampleIndices = context.lineSampleCache[bestLineIdx] & samples_indices;
    std::bitset<MAX_SAMPLES_COUNT> rightSampleIndices = (~context.lineSampleCache[bestLineIdx]) & samples_indices;
    for (uint32_t i = 0; i < context.labelsPerSet.size(); ++i)
    {
      const auto currentLeftSet = context.labelsPerSet[i] & leftSampleIndices;
      if (currentLeftSet.count() == leftSampleIndices.count())
      {
        leftIsLeaf = true;
      }
      const auto currentRightSet = context.labelsPerSet[i] & rightSampleIndices;
      if (currentRightSet.count() == rightSampleIndices.count())
      {
        rightIsLeaf = true;
      }
    }

    std::vector<uint32_t> leftLines  = RemoveBadLines(context, leftSampleIndices, lines_indices);
    std::vector<uint32_t> rightLines = RemoveBadLines(context, rightSampleIndices, lines_indices);

    uint32_t nodeIdx = nodesCollection.size();
    nodesCollection.resize(nodesCollection.size() + 1);
    size_t rootOffset = nodesCollection.size()-1;      // previously: "auto& root = nodesCollection.back(); 
                                                       // This is not valid because reference may become broken after next nodesCollection.resize() in subsequent recursive call of 'construct_bsp'! 
    nodesCollection[rootOffset].split = line;
    if (leftIsLeaf || leftLines.empty())
    {
      nodesCollection[rootOffset].isLeftLeaf = 1;
      uint32_t begin = 0;
      while (!leftSampleIndices.test(begin))
        begin++;
      nodesCollection[rootOffset].leftIdx = context.labels[begin] + subtexelsCollection.size();
    }
    else
    {
      nodesCollection[rootOffset].isLeftLeaf = 0;
      nodesCollection[rootOffset].leftIdx = construct_bsp(context, leftSampleIndices, leftLines);
    }

    if (rightIsLeaf || rightLines.empty())
    {
      nodesCollection[rootOffset].isRightLeaf = 1;
      uint32_t begin = 0;
      while (!rightSampleIndices.test(begin))
        begin++;
      nodesCollection[rootOffset].rightIdx = context.labels[begin] + subtexelsCollection.size();
    }
    else
    {
      nodesCollection[rootOffset].isRightLeaf = 0;
      nodesCollection[rootOffset].rightIdx = construct_bsp(context, rightSampleIndices, rightLines);
    }
    return nodeIdx;
  }

  void init()
  {
    singleRayData.resize(config.width * config.height);
    hammSamples.resize(config.additionalSamplesCnt*2);
    PlaneHammersley(hammSamples.data(), config.additionalSamplesCnt);
    config.additionalSamplesCnt += 5;
    hammSamples.push_back(0.5f);
    hammSamples.push_back(0.5f);
    hammSamples.push_back(0.0f);
    hammSamples.push_back(0.0f);
    hammSamples.push_back(0.0f);
    hammSamples.push_back(1.0f);
    hammSamples.push_back(1.0f);
    hammSamples.push_back(1.0f);
    hammSamples.push_back(1.0f);
    hammSamples.push_back(0.0f);

    hammSamples.push_back(0.2f);
    hammSamples.push_back(1.0f);
    hammSamples.push_back(0.2f);
    hammSamples.push_back(0.0f);
    hammSamples.push_back(1.0f);
    hammSamples.push_back(0.2f);
    hammSamples.push_back(0.0f);
    hammSamples.push_back(0.2f);

    hammSamples.push_back(0.4f);
    hammSamples.push_back(1.0f);
    hammSamples.push_back(0.4f);
    hammSamples.push_back(0.0f);
    hammSamples.push_back(1.0f);
    hammSamples.push_back(0.4f);
    hammSamples.push_back(0.0f);
    hammSamples.push_back(0.4f);

    hammSamples.push_back(0.6f);
    hammSamples.push_back(1.0f);
    hammSamples.push_back(0.6f);
    hammSamples.push_back(0.0f);
    hammSamples.push_back(1.0f);
    hammSamples.push_back(0.6f);
    hammSamples.push_back(0.0f);
    hammSamples.push_back(0.6f);

    hammSamples.push_back(0.8f);
    hammSamples.push_back(1.0f);
    hammSamples.push_back(0.8f);
    hammSamples.push_back(0.0f);
    hammSamples.push_back(1.0f);
    hammSamples.push_back(0.8f);
    hammSamples.push_back(0.0f);
    hammSamples.push_back(0.8f);

    for (float& v : hammSamples)                                       // [0,1] ==> [-0.5,0.5]
      v = (v - 0.5f) * 2.0f * config.radius;
    #ifdef STORE_LABELS
    geomIdsPerPixel.resize(config.height * config.width);
    #endif
  }

public:
  SubPixelImageBSP(const Config &conf) : config(conf) 
  {
    init();
  }

  SubPixelImageBSP(uint32_t a_width, uint32_t a_height, float a_radius = 0.5f)
  {
    config.width          = a_width;
    config.height         = a_height;
    config.radius         = a_radius;
    config.additionalSamplesCnt = 64;
    init();
  }

  struct uint2 {
    uint2() : x(0), y(0) {}
    uint2(uint32_t a_x, uint32_t a_y) : x(a_x), y(a_y) {}
    uint32_t x, y;
  };

  void RemoveDuplicatesAndMakeLabels(const std::vector<TexType>& samples, std::vector<uint32_t>& labels, std::vector<TexType> &reference_samples)
  {
    reference_samples.clear();
    labels.clear();

    for (uint32_t i = 0; i < samples.size(); ++i)
    {
      bool refFound = false;
      for (uint32_t j = 0, je = reference_samples.size(); j < je; ++j)
      {
        if (reference_samples[j] == samples[i])
        {
          labels.push_back(j);
          refFound = true;
          break;
        }
      }

      if (!refFound)
      {
        labels.push_back(reference_samples.size());
        reference_samples.push_back(samples[i]);
      }
    }
  }

  std::vector<TexType> RemoveDuplicatesForTList(const std::vector<TexType>& samples)
  {
    std::vector<TexType> referenceSamples; 
    referenceSamples.reserve(samples.size());
    
    for (uint32_t i = 0; i < samples.size(); ++i)
    {
      bool refFound = false;
      for (uint32_t j = 0, je = referenceSamples.size(); j < je; ++j)
      {
        if (referenceSamples[j].instId == samples[i].instId && 
            referenceSamples[j].geomId == samples[i].geomId && 
            referenceSamples[j].primId == samples[i].primId)
        {
          refFound = true;
          break;
        }
      }

      if (!refFound)
        referenceSamples.push_back(samples[i]);
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
          localSamples.push_back(hammSamples[i * 2]);
          localSamples.push_back(hammSamples[i * 2 + 1]);
        }
      }

      // Train SVM
      SVM svm;
      svm.fit(localSamples, localLabels);
      lines.push_back(NormalizeLine(svm.get_weights()));
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
  
  template<typename BaseSampler>
  std::vector<Line> GetLinesFromTriangles(const std::vector<TexType>& referenceSamples, const BaseSampler &sampler, uint32_t px, uint32_t py)
  {
    // (1) get vertices in NDC
    //
    auto tverts = sampler.GetAllTriangleVerts2D(referenceSamples.data(), referenceSamples.size());

    // (2) transform vertices from NDC to pixel-scale coordinates
    //
    const float absPX = (float(px) + 0.5f)/float(config.width);  
    const float absPY = (float(py) + 0.5f)/float(config.height); 
    
    const float absPX2 = absPX*2.0f - 1.0f; // [0,1] ==> [-1,1]
    const float absPY2 = absPY*2.0f - 1.0f; // [0,1] ==> [-1,1]

    const float pixelSSX = float(config.width);  // NDC scale => pixel scale   
    const float pixelSSY = float(config.height); // NDC scale => pixel scale

    for(auto& v : tverts) 
    {
      v.x = config.radius*pixelSSX*(v.x - absPX2);
      v.y = config.radius*pixelSSY*(v.y - absPY2);
    }
    
    // (3) calc line equation
    //
    std::vector<Line> lines(tverts.size());
    for(size_t i=0;i<lines.size();i+=3)
    { 
      auto v0 = tverts[i+0];
      auto v1 = tverts[i+1];
      auto v2 = tverts[i+2];

      // v0-->v1
      lines[i+0][0] = v0.y - v1.y; // A = (y1 - y2);
      lines[i+0][1] = v1.x - v0.x; // B = (x2 - x1);
      lines[i+0][2] = -lines[i+0][0]*v0.x - lines[i+0][1]*v0.y; // C = -A*x1 - B*y1;
      
      // v1-->v2
      lines[i+1][0] = v1.y - v2.y; // A = (y1 - y2);
      lines[i+1][1] = v2.x - v1.x; // B = (x2 - x1);
      lines[i+1][2] = -lines[i+1][0]*v1.x - lines[i+1][1]*v1.y;  // C = -A*x1 - B*y1;
      
      // v2-->v0
      lines[i+2][0] = v2.y - v0.y; // A = (y1 - y2);
      lines[i+2][1] = v0.x - v2.x; // B = (x2 - x1);
      lines[i+2][2] = -lines[i+2][0]*v2.x - lines[i+2][1]*v2.y; // C = -A*x1 - B*y1;

      lines[i+0] = NormalizeLine(lines[i+0]);
      lines[i+1] = NormalizeLine(lines[i+1]);
      lines[i+2] = NormalizeLine(lines[i+2]);
    }

    // (4) exclude lines which don't have intersection with pixel bouding box using half-space equetions
    //
    const LiteMath::float2 c0 = LiteMath::float2(-1,-1)*config.radius;
    const LiteMath::float2 c1 = LiteMath::float2(-1,+1)*config.radius;
    const LiteMath::float2 c2 = LiteMath::float2(+1,+1)*config.radius;
    const LiteMath::float2 c3 = LiteMath::float2(+1,-1)*config.radius;

    std::vector<Line> linesInPixel; linesInPixel.reserve(lines.size());
    for(auto& line : lines)
    {
      const float halfDist0 = line[0]*c0.x + line[1]*c0.y + line[2];
      const float halfDist1 = line[0]*c1.x + line[1]*c1.y + line[2];
      const float halfDist2 = line[0]*c2.x + line[1]*c2.y + line[2];
      const float halfDist3 = line[0]*c3.x + line[1]*c3.y + line[2];

      const bool s0 = std::signbit(halfDist0);
      const bool s1 = std::signbit(halfDist1);
      const bool s2 = std::signbit(halfDist2);
      const bool s3 = std::signbit(halfDist3);

      if(s0 == s1 && s1 == s2 && s2 == s3)
        continue;
      
      linesInPixel.push_back(line);
    }

    return linesInPixel; 
  }

  std::vector<uint32_t> RemoveBadLines(
    const BSPBuildContext& context,
    const std::bitset<MAX_SAMPLES_COUNT> &sample_indices,
    const std::vector<uint32_t> &lines_indices)
  {
    std::vector<uint32_t> linesTemp;
    linesTemp.reserve(lines_indices.size());
    
    // Remove lines which don't split anything
    for (const uint32_t lineId : lines_indices)
    {
      uint32_t leftCnt = (sample_indices & context.lineSampleCache[lineId]).count();
      uint32_t rightCnt = (sample_indices & (~context.lineSampleCache[lineId])).count();

      if (leftCnt != 0 && rightCnt != 0)
        linesTemp.push_back(lineId);
    }

    return linesTemp;
  }

  std::vector<Line> RemoveBadLines(const std::vector<Line>& lines, const std::vector<float>& samplesPositions)
  {
    std::vector<Line> linesTemp;
    linesTemp.reserve(lines.size());
    
    // Remove lines which don't split anything
    for (const auto& line : lines)
    {
      uint32_t leftCnt = 0;
      uint32_t rightCnt = 0;
      for (uint32_t j = 0; j < samplesPositions.size(); j += 2)
      {
        if (line[0] * samplesPositions[j] + line[1] * samplesPositions[j + 1] + line[2] >= 0.0f)
          leftCnt++;
        else
          rightCnt++;
      }

      if (leftCnt != 0 && rightCnt != 0)
        linesTemp.push_back(line);
    }

    return linesTemp;
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
    #pragma omp parallel for
    for (int y = 0; y < config.height; ++y) {
      for (uint32_t x = 0; x < config.width; ++x) {
        singleRayData[y * config.width + x] = sampler.fetch(x, y);
        #ifdef STORE_LABELS
        geomIdsPerPixel[y * config.width + x] = singleRayData[y * config.width + x].geomId;
        #endif
      }
    }

    // Collect suspicious (aliased, with high divergence in neighbourhood) texels.
    std::vector<uint2> suspiciosTexelIds = GetSuspiciosTexels(singleRayData);
   
    const uint32_t samplesCount = hammSamples.size() / 2;
    uint32_t badSplits = 0;
    uint32_t borderPixels = 0;
    
    std::cout << "suspiciosTexelIds.size() = " << suspiciosTexelIds.size() << std::endl;
    size_t texleId = 0;
    // Process suspicious texels
    std::vector<TexType> samples;
    samples.reserve(samplesCount);
    std::vector<uint32_t> labels;
    labels.reserve(samplesCount);
    std::vector<TexType> referenceSamples;
    referenceSamples.reserve(samplesCount);
    for (auto texel_idx : suspiciosTexelIds)
    {
      const uint32_t texel_x = texel_idx.x;
      const uint32_t texel_y = texel_idx.y;

      // Make new samples
      samples.clear();
      samples.resize(samplesCount);
      #pragma omp parallel for
      for (int i = 0; i < samplesCount; ++i)
      {
        samples[i] = sampler.sample((texel_x + 0.5f + hammSamples[2 * i + 0]) / float(config.width), 
                                         (texel_y + 0.5f + hammSamples[2 * i + 1]) / float(config.height));
      }

      // Make labels for samples
      //
      RemoveDuplicatesAndMakeLabels(samples, labels, referenceSamples);

      texleId++;
      if(texleId % 100 == 0)
      {
        std::cout << "progress = " << int( 100.0f*float(texleId)/float(suspiciosTexelIds.size()) ) << "% \r";
        std::cout.flush();
      }

      if (referenceSamples.size() == 1)
        continue;

      #ifdef STORE_LABELS
      specialLabels[texel_idx.y*config.width + texel_idx.x] = labels;
      for (size_t i = 0; i < samples.size(); ++i)
        specialLabels[texel_idx.y*config.width + texel_idx.x][i] = samples[i].geomId;
      #endif

      std::vector<Line> lines = RemoveBadLines(GetLinesFromTriangles(RemoveDuplicatesForTList(samples), sampler, texel_x, texel_y), hammSamples);   

      uint32_t tScores = BadSplits(lines, labels, hammSamples);
      if(tScores > 0) 
      {
        auto lines2 = RemoveBadLines(GetLinesSVM(referenceSamples, labels), hammSamples);
        badSplits++;
        uint32_t sScores = BadSplits(lines2, labels, hammSamples);
        if(sScores < tScores)
          lines = lines2;
      }

      if (!lines.empty())
      {
        const uint32_t offset = construct_bsp(lines, labels, hammSamples, referenceSamples);
        specialTexels[texel_idx.y*config.width + texel_idx.x] = offset;
        subtexelsCollection.insert(subtexelsCollection.end(), referenceSamples.begin(), referenceSamples.end());
        borderPixels++;
      }
    }

    std::cout << "Enough epoches: " << epoches_to_improve << std::endl;
    
    std::cout << std::endl;
    auto oldPrec = std::cout.precision(2);
    std::cout << "[BspImage]: borderPixels = " << borderPixels           << std::endl;
    std::cout << "[BspImage]: totalNodes   = " << nodesCollection.size() << std::endl;
    std::cout << "[BspImage]: BadSplitsNum = " << badSplits              << ", which is " << 100.0f*float(badSplits)/float(borderPixels) << "% of border pixels" << std::endl;
    std::cout.precision(oldPrec);
  }

  TexType& access(float x, float y)
  {
    const uint32_t x_texel = static_cast<uint32_t>(x * config.width);
    const uint32_t y_texel = static_cast<uint32_t>(y * config.height);
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
    int add_samples = int(hammSamples.size()/2);
    std::ofstream out(ss.str(), std::ios::binary | std::ios::out);
    out.write((char*)&add_samples, sizeof(add_samples));
    out.write((char*)hammSamples.data(), sizeof(float) * hammSamples.size());
    #ifdef STORE_LABELS
    out.write((char*)&config.height, sizeof(config.height));
    out.write((char*)&config.width, sizeof(config.width));
    for (uint32_t i = 0; i < config.height; ++i)
      for (uint32_t j = 0; j < config.width; ++j)
        out.write((char*)&geomIdsPerPixel[(config.height - i - 1) * config.width + j], sizeof(geomIdsPerPixel[i * config.width + j]));
    #endif
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
