#include <basic-examples/SimpleTexture/SimpleTexture.h>
#include <random>
#include <glbinding/gl/gl.h>
#include <gloperate/capabilities/ViewportCapability.h>


using namespace gloperate;
using namespace gl;


SimpleTexture::SimpleTexture(ResourceManager & resourceManager)
: Painter(resourceManager)
, m_viewportCapability(new gloperate::ViewportCapability)
{
    addCapability(m_viewportCapability);
}

SimpleTexture::~SimpleTexture()
{
}

void SimpleTexture::onInitialize()
{
    gl::glClearColor(0.2f, 0.3f, 0.4f, 1.f);

    createAndSetupTexture();
    createAndSetupGeometry();
}

void SimpleTexture::onPaint()
{
    if (m_viewportCapability->hasChanged())
    {
        glViewport(m_viewportCapability->x(), m_viewportCapability->y(), m_viewportCapability->width(), m_viewportCapability->height());

        m_viewportCapability->setChanged(false);
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_quad->draw();
}

void SimpleTexture::createAndSetupTexture()
{
    static const int w(256);
    static const int h(256);
    unsigned char data[w * h * 4];

    std::random_device rd;
    std::mt19937 generator(rd());
    std::poisson_distribution<> r(0.2);

    for (int i = 0; i < w * h * 4; ++i) {
        data[i] = static_cast<unsigned char>(255 - static_cast<unsigned char>(r(generator) * 255));
    }

    m_texture = globjects::Texture::createDefault(gl::GL_TEXTURE_2D);
    m_texture->image2D(0, gl::GL_RGBA8, w, h, 0, gl::GL_RGBA, gl::GL_UNSIGNED_BYTE, data);
}

void SimpleTexture::createAndSetupGeometry()
{
    m_quad = new gloperate::ScreenAlignedQuad(m_texture);
    m_quad->setSamplerUniform(0);
}
