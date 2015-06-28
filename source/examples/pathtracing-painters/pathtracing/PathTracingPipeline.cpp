#include "PathTracingPipeline.h"

#include <glbinding/gl/bitfield.h>
#include <glbinding/gl/enum.h>
#include <glbinding/gl/functions.h>

#include <globjects/Framebuffer.h>
#include <globjects/Texture.h>

#include <gloperate/painter/AbstractTargetFramebufferCapability.h>
#include <gloperate/primitives/ScreenAlignedQuad.h>
#include <gloperate/rendering/GenericPathTracingStage.h>


using namespace gl;

namespace
{

const char* s_blitFragmentShader = R"(
#version 140

uniform sampler2D colorTexture;
uniform sampler2D depthTexture;

out vec4 fragColor;

in vec2 v_uv;

void main()
{
    fragColor = texture(colorTexture, v_uv);
    gl_FragDepth = texture(depthTexture, v_uv).x;
}
)";

class BlitStage : public gloperate::AbstractStage
{
public:
    BlitStage()
        : AbstractStage("BlitStage")
    {
        addInput("targetFBO", targetFramebuffer);
        addInput("color", color);
        addInput("depth", depth);

        alwaysProcess(true);
    }

    ~BlitStage() override = default;

    virtual void initialize() override
    {
        globjects::ref_ptr<globjects::Shader> fragmentShader = globjects::Shader::fromString(GL_FRAGMENT_SHADER, s_blitFragmentShader);

        m_quad = globjects::make_ref<gloperate::ScreenAlignedQuad>(fragmentShader);
        m_quad->program()->setUniform("colorTexture", 0);
        m_quad->program()->setUniform("depthTexture", 1);
    }

    gloperate::InputSlot<gloperate::AbstractTargetFramebufferCapability * > targetFramebuffer;

    gloperate::InputSlot<globjects::ref_ptr<globjects::Texture>> color;
    gloperate::InputSlot<globjects::ref_ptr<globjects::Texture>> depth;

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

        color.data()->bindActive(GL_TEXTURE0);
        depth.data()->bindActive(GL_TEXTURE1);

        m_quad->draw();

        color.data()->unbindActive(GL_TEXTURE0);
        depth.data()->unbindActive(GL_TEXTURE1);

        globjects::Framebuffer::defaultFBO()->bind(GL_FRAMEBUFFER);

        glEnable(GL_DEPTH_TEST);
    }
protected:
    globjects::ref_ptr<gloperate::ScreenAlignedQuad> m_quad;
};

}

PathTracingPipeline::PathTracingPipeline()
{
    auto pathTracingStage = new gloperate::GenericPathTracingStage();
    auto blitStage = new BlitStage();

    pathTracingStage->viewport = viewport;
    pathTracingStage->camera = camera;
    pathTracingStage->projection = projection;

    blitStage->color = pathTracingStage->colorTexture;
    blitStage->depth = pathTracingStage->depthTexture;
    blitStage->targetFramebuffer = targetFBO;

    addStages(
        pathTracingStage,
        blitStage
    );
}

PathTracingPipeline::~PathTracingPipeline() = default;
