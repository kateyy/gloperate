#ifndef SAMPLING
#define SAMPLING


uniform uint coarseSamplingWindowSize;
uint numCoarseSubSamples = coarseSamplingWindowSize * coarseSamplingWindowSize;

const uint antiAliasingKernelSize = 4;
uint numAntiAliasingFrames = antiAliasingKernelSize * antiAliasingKernelSize;


struct CoarseSamplingWindow
{
	uvec2 origin;
	uvec2 windowSize;
};

layout (std430, binding = 0) readonly buffer CoarseSamplingOrder
{
	CoarseSamplingWindow coarseSamplingWindows[];
};

// coarse sampling and anti-aliasing:
//  (1) render all pixel once (coarseSamplingInit)
//  (2) render each pixel 16x (anti-aliasing)


bool getCoarseSamplingInit(uint frameCounter)
{
    return frameCounter < numCoarseSubSamples;
}

uint getCoarseSubSampleId(const in uint frameCounter, out bool coarseSamplingInit)
{
    coarseSamplingInit = getCoarseSamplingInit(frameCounter);

    return coarseSamplingInit
        ? frameCounter
        : (frameCounter / numAntiAliasingFrames) % numCoarseSubSamples;
}

uint getCoarseSubSampleId(uint frameCounter)
{
    bool notUsed;

    return getCoarseSubSampleId(frameCounter, notUsed);
}


#endif
