#pragma once

#include <gloperate/pipeline/PipelinePainter.h>

#include "PathTracingPipeline.h"


namespace gloperate
{
class AbstractCameraCapability;
class AbstractPerspectiveProjectionCapability;
}


class PathTracingPainter : public gloperate::PipelinePainter
{
public:
    PathTracingPainter(gloperate::ResourceManager & resourceManager);

protected:
    void onInitialize() override;

protected:
    PathTracingPipeline m_pipeline;

    gloperate::AbstractCameraCapability * m_camera;
    gloperate::AbstractPerspectiveProjectionCapability * m_projection;
};
