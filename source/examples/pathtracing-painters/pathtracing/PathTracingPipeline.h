#pragma once

#include <gloperate/pipeline/AbstractPipeline.h>
#include <gloperate/pipeline/Data.h>

namespace gloperate
{

class AbstractTargetFramebufferCapability;
class AbstractViewportCapability;
class AbstractVirtualTimeCapability;
class AbstractCameraCapability;
class AbstractProjectionCapability;
class AbstractTypedRenderTargetCapability;

}

class RasterizationStage;
class PostprocessingStage;

class PathTracingPipeline : public gloperate::AbstractPipeline
{
public:
    PathTracingPipeline();
    virtual ~PathTracingPipeline();
    
    gloperate::Data<gloperate::AbstractTargetFramebufferCapability *> targetFBO;
    gloperate::Data<gloperate::AbstractViewportCapability *> viewport;
    gloperate::Data<gloperate::AbstractCameraCapability *> camera;
    gloperate::Data<gloperate::AbstractProjectionCapability *> projection;
    gloperate::Data<gloperate::AbstractTypedRenderTargetCapability *> renderTargets;
};
