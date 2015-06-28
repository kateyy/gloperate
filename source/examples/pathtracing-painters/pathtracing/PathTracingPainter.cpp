
#include "PathTracingPainter.h"

#include <gloperate/painter/ViewportCapability.h>
#include <gloperate/painter/TargetFramebufferCapability.h>
#include <gloperate/painter/CameraCapability.h>
#include <gloperate/painter/PerspectiveProjectionCapability.h>
#include <gloperate/painter/TypedRenderTargetCapability.h>


PathTracingPainter::PathTracingPainter(gloperate::ResourceManager & resourceManager)
:   PipelinePainter(resourceManager, m_pipeline)
{
    auto targetFBO = addCapability(new gloperate::TargetFramebufferCapability());
    auto viewport = addCapability(new gloperate::ViewportCapability());
    m_camera = addCapability(new gloperate::CameraCapability());
    m_projection = addCapability(new gloperate::PerspectiveProjectionCapability(viewport));
    auto renderTargets = addCapability(new gloperate::TypedRenderTargetCapability());

    m_pipeline.targetFBO.setData(targetFBO);
    m_pipeline.viewport.setData(viewport);
    m_pipeline.camera.setData(m_camera);
    m_pipeline.projection.setData(m_projection);
    m_pipeline.renderTargets.setData(renderTargets);

    targetFBO->changed.connect([this]() { m_pipeline.targetFBO.invalidate(); });
    viewport->changed.connect([this]() { m_pipeline.viewport.invalidate(); });
}

void PathTracingPainter::onInitialize()
{
    PipelinePainter::onInitialize();

    auto cornellCenter = glm::vec3(278.0f, 274.4f, 279.6f);

    m_camera->setCenter(cornellCenter);
    m_camera->setEye(glm::vec3(cornellCenter.x, cornellCenter.y, -cornellCenter.z));
    m_camera->setUp(glm::vec3(0.0f, 1.0, 0.0f));
    m_projection->setZNear(0.01f);
    m_projection->setZFar(2000.0f);

}
