#pragma once

#include <array>
#include <map>
#include <memory>

#include <globjects/base/ref_ptr.h>

#include <globjects/Buffer.h>
#include <globjects/Program.h>
#include <globjects/Shader.h>
#include <globjects/Texture.h>

#include <gloperate/painter/AbstractCameraCapability.h>
#include <gloperate/painter/AbstractProjectionCapability.h>
#include <gloperate/painter/AbstractViewportCapability.h>
#include <gloperate/pipeline/AbstractStage.h>
#include <gloperate/pipeline/InputSlot.h>


namespace gloperate
{

class UniformGroup;


class GLOPERATE_API GenericPathTracingStage : public gloperate::AbstractStage
{
public:
    /** @param namedExtensionShaderFiles pass map of extension name -> shader file name */
    GenericPathTracingStage(const std::map<std::string, std::string> & namedExtensionShaderFiles = {});
    ~GenericPathTracingStage() override;

    virtual void initialize() override;

    // Path Tracing extension points
    static const std::vector<std::string> & extensionNames();

public:
    gloperate::InputSlot<gloperate::AbstractViewportCapability *> viewport;
    gloperate::InputSlot<gloperate::AbstractCameraCapability *> camera;

    gloperate::InputSlot<gloperate::AbstractProjectionCapability *> projection;
    gloperate::InputSlot<unsigned int> coarseSamplingWindowSize;
    gloperate::InputSlot<int> coarseEnd;
    /** ray depth of 0 results in ray casting only */
    gloperate::InputSlot<gl::GLuint> maxRayDepth;

    gloperate::Data<globjects::ref_ptr<globjects::Texture>> colorTexture;
    gloperate::Data<globjects::ref_ptr<globjects::Texture>> normalTexture;
    gloperate::Data<globjects::ref_ptr<globjects::Texture>> depthTexture;

protected:
    virtual void process() override;

    virtual void preRender();
    virtual void postRender();

    /** discards any previous path tracing results */
    void setSceneChanged();

    virtual void clearTexturesEvent(const glm::ivec2 & size);
    virtual void resizeTexturesEvent(const glm::ivec2 & size);

    template<typename T>
    void setUniform(const std::string & name, const T & value);

private:
    void render();
    gl::GLuint computeFirstOrderRays(const glm::uvec3 & workGroupCount);
    gl::GLuint computeSecondOrderRays(gl::GLuint depth, gl::GLuint numRays);
    void computeShadowRays(gl::GLuint depth, gl::GLuint numRays);
    void flattenPath(gl::GLuint numStackLayers);
    void aggregateColors();

    static glm::uvec3 getSecondOrderRaysWorkgroupCount(gl::GLuint count, gl::GLuint * stride);
    void setupPathStack(gl::GLuint stackDepth, glm::uvec2 layerSize);
    void clearPathStack();
    void bindPathStack(gl::GLuint binding, int layer = -1);

    void bindCommonObjects();
    void unbindCommonObjects();
    void bindRayOutputBuffer(gl::GLuint binding);
    void bindRayInputBuffer(gl::GLuint binding);
    void swapRaysBuffers();

    void setupRayData();
    void resetRayCounter();
    gl::GLuint getRayCounter();

    void loadShaders();
    void initializeExtensionShaders();

    void setupTextures();
    void clearTextures(const glm::ivec2 & size);
    void resizeTextures(const glm::ivec2 & size);

    void setupUniforms();
    void updateUniforms();
    void updateSampling();

    static glm::mat3 rayMatrix(
        const glm::vec3 & eye, const glm::vec3 & center, const glm::vec3 & up, 
        float fovy, float aspectRatio);

    void setupTestingData();

private:
    static const glm::uvec3 s_workGroupSize;

    const std::map<std::string, std::string> m_namedExtensionShaders;

    bool m_sceneChanged;

    globjects::ref_ptr<globjects::Program> m_firstOrderRayProgram;
    globjects::ref_ptr<globjects::Program> m_secondOrderRayProgram;
    globjects::ref_ptr<globjects::Program> m_shadowRayProgram;

    globjects::ref_ptr<globjects::Program> m_flattenPathStackProgram;
    globjects::ref_ptr<globjects::Program> m_clearPathStackProgram;

    globjects::ref_ptr<globjects::Program> m_aggregateColorsProgram;

    gl::GLuint m_frameCounter;

    std::map<std::string, std::unique_ptr<UniformGroup>> m_uniformGroups;

    std::array<globjects::ref_ptr<globjects::Buffer>, 2> m_rayBuffers;
    int m_rayOutputBufferIndex;
    globjects::ref_ptr<globjects::Buffer> m_counterBuffer;
    globjects::ref_ptr<globjects::Buffer> m_pathStackBuffer;
    unsigned int m_pathStackDepth;
    glm::uvec2 m_pathStackLayerExtent;
    gl::GLsizeiptr m_pathStackLayerDataSize;
    gl::GLuint m_pathStackLayerStride;

    // per frame rendering results, aggregated to the output colorTexture
    gloperate::Data<globjects::ref_ptr<globjects::Texture>> colorPerFrameTexture;

    globjects::ref_ptr<globjects::Texture> m_randomIndexTexture;
    globjects::ref_ptr<globjects::Texture> m_randomVectorTexture;

    globjects::ref_ptr<globjects::Buffer> m_coarsePixelOrderBuffer;

    bool m_useTestingData;
    globjects::ref_ptr<globjects::Buffer> m_testingIndices;
    globjects::ref_ptr<globjects::Buffer> m_testingVertices;
};


template<typename T>
void GenericPathTracingStage::setUniform(const std::string & name, const T & value)
{
    m_firstOrderRayProgram->setUniform(name, value);
    m_secondOrderRayProgram->setUniform(name, value);
    m_shadowRayProgram->setUniform(name, value);
}

} // namespace gloperate
