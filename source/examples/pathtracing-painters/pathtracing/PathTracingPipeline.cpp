#include "PathTracingPipeline.h"

#include <glbinding/gl/bitfield.h>
#include <glbinding/gl/enum.h>
#include <glbinding/gl/functions.h>

#include <globjects/Framebuffer.h>

#include <gloperate/painter/AbstractTargetFramebufferCapability.h>
#include <gloperate/primitives/ScreenAlignedQuad.h>
#include <gloperate/rendering/GenericPathTracingStage.h>


using namespace gl;

class BlitStage : public gloperate::AbstractStage
{
public:
    BlitStage()
        : AbstractStage("BlitStage")
    {
        addInput("targetFBO", targetFramebuffer);
        addInput("color", color);
        //addInput("normal", normal);

        alwaysProcess(true);
    }

    ~BlitStage() override = default;

    virtual void initialize() override
    {
        m_quad = globjects::make_ref<gloperate::ScreenAlignedQuad>(color.data());
    }

    gloperate::InputSlot<gloperate::AbstractTargetFramebufferCapability * > targetFramebuffer;

    gloperate::InputSlot<globjects::ref_ptr<globjects::Texture>> color;

protected:
    virtual void process() override
    {
        glDisable(GL_DEPTH_TEST);

        globjects::Framebuffer * fbo = targetFramebuffer.data()->framebuffer() ? targetFramebuffer.data()->framebuffer() : globjects::Framebuffer::defaultFBO();

        fbo->bind(GL_FRAMEBUFFER);

        if (!fbo->isDefault())
        {
            fbo->setDrawBuffers({ GL_COLOR_ATTACHMENT0 });
        }

        m_quad->draw();

        globjects::Framebuffer::defaultFBO()->bind(GL_FRAMEBUFFER);

        glEnable(GL_DEPTH_TEST);
    }
protected:
    globjects::ref_ptr<gloperate::ScreenAlignedQuad> m_quad;
};

PathTracingPipeline::PathTracingPipeline()
{
    auto pathTracingStage = new gloperate::GenericPathTracingStage();
    auto blitStage = new BlitStage();

    pathTracingStage->viewport = viewport;
    pathTracingStage->camera = camera;
    pathTracingStage->projection = projection;

    blitStage->color = pathTracingStage->colorTexture;
    blitStage->targetFramebuffer = targetFBO;


    addStages(
        pathTracingStage,
        blitStage
    );
}

PathTracingPipeline::~PathTracingPipeline() = default;
