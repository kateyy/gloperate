#include <gloperate/rendering/GenericPathTracingStage.h>

#include <algorithm>
#include <chrono>
#include <cassert>
#include <fstream>
#include <random>

#include <glm/gtc/random.hpp>
#include <glm/vec2.hpp>

#include <glbinding/gl/bitfield.h>
#include <glbinding/gl/boolean.h>
#include <glbinding/gl/enum.h>
#include <glbinding/gl/functions.h>

#include <globjects/Shader.h>
#include <globjects/NamedString.h>
#include <gloperate/base/directorytraversal.h>
#include <globjects/base/File.h>

#include <widgetzeug/make_unique.hpp>

#include <gloperate/painter/AbstractViewportCapability.h>
#include <gloperate/painter/AbstractPerspectiveProjectionCapability.h>
#include <gloperate/primitives/UniformGroup.h>


using namespace gl;
using widgetzeug::make_unique;


namespace
{
const std::string s_shader_path = "data/pathtracing/generic/";
const std::string s_ext_namespace = "/PATHTRACING_EXTENSIONS/";
const std::vector<std::string> s_extensions{
    "rayCastingOutput", "materials",
    "geometryTraversal", "secondOrderRayGeneration",
    "shadowRayCast" };

const GLuint s_maxRayDepthDefault = 7;
const int s_coarseSamplingWindowSizeDefault = 1;
const int s_coarseEndDefault = -1;

struct SecondOrderRay
{
    glm::ivec2 pixel;
    glm::vec2 padding1;
    glm::vec3 origin;
    int padding2;
    glm::vec3 direction;
    int padding3;
    glm::vec4 color;    // pass color from first to second order, for testing
};

struct PathStackData
{
    int objectID;
    int materialID;
    int aux0;
    int aux1;
    glm::vec4 lightColor;
};

template<typename T>
T nextMultiple(T n, T k)
{
    return n + (k - n%k) % k;
}

template<typename T>
T gcd(T a, T b)
{
    for (;;)
    {
        if (a == 0) return b;
        b %= a;
        if (b == 0) return a;
        a %= b;
    }
}

template<typename T>
T lcm(T a, T b)
{
    T _gcd = gcd(a, b);

    return _gcd ? (a / _gcd * b) : 0;
}

}

namespace gloperate
{

const glm::uvec3 GenericPathTracingStage::s_workGroupSize{ 16, 16, 1 };


GenericPathTracingStage::GenericPathTracingStage(const std::map<std::string, std::string> & namedExtensionShaderFiles)
    : gloperate::AbstractStage()
    , m_namedExtensionShaders(namedExtensionShaderFiles)
    , m_sceneChanged(true)
    , m_frameCounter(0)
    , m_rayOutputBufferIndex(0)
    , m_pathStackDepth(0)
    , m_pathStackLayerExtent(0, 0)
    , m_pathStackLayerDataSize(0)
    , m_pathStackLayerStride(0)
    , m_useTestingData(true)
{
    addInput("viewport", viewport);
    addInput("camera", camera);
    addInput("projection", projection);
    addOptionalInput("coarseSamplingWindowSize", coarseSamplingWindowSize);
    addOptionalInput("coarseEnd", coarseEnd);
    addOptionalInput("maxRayDepth", maxRayDepth);

    addOutput("colorTexture", colorTexture);
    addOutput("normalTexture", normalTexture);
    addOutput("depthTexture", depthTexture);
}

GenericPathTracingStage::~GenericPathTracingStage() = default;

void GenericPathTracingStage::initialize()
{
    gloperate::scanDirectory(s_shader_path.substr(0, s_shader_path.length() - 1), "glsl", true);

    initializeExtensionShaders();
    loadShaders();

    setupTextures();
    setupUniforms();
    setupRayData();

    setupTestingData();
}

const std::vector<std::string> & GenericPathTracingStage::extensionNames()
{
    return s_extensions;
}

void GenericPathTracingStage::process()
{
    m_sceneChanged = false;

    const auto & vp = *viewport.data();
    
    if (viewport.hasChanged())
    {
        resizeTextures({ vp.width(), vp.height() });
        setSceneChanged();

        glViewport(vp.x(), vp.y(), vp.width(), vp.height());
    }

    if (camera.hasChanged() || projection.hasChanged() || coarseSamplingWindowSize.hasChanged() || coarseEnd.hasChanged())
    {
        setSceneChanged();
    }

    updateSampling();

    // allow implementors to setup their extensions and invalidate the scene if needed
    preRender();

    if (m_sceneChanged)
        m_frameCounter = 0;
    else
        ++m_frameCounter;

    auto coarseEndValue = coarseEnd.data(s_coarseEndDefault);
    if (coarseEndValue >= 0 && int(m_frameCounter) >= coarseEndValue)
        return;

    updateUniforms();

    render();

    postRender();

    m_processScheduled = true;  // always repaint for now, to accumulate

    invalidateOutputs();
}

void GenericPathTracingStage::preRender()
{
}

void GenericPathTracingStage::postRender()
{
}

void GenericPathTracingStage::setSceneChanged()
{
    m_sceneChanged = true;
}

void GenericPathTracingStage::clearTexturesEvent(const glm::ivec2 & /*size*/)
{
}

void GenericPathTracingStage::resizeTexturesEvent(const glm::ivec2 & /*size*/)
{
}

void GenericPathTracingStage::render()
{
    const auto & vp = *viewport.data();
    if (m_sceneChanged || viewport.hasChanged())
    {
        clearTextures({ vp.width(), vp.height() });

        GLsizeiptr bufferSize = static_cast<GLsizeiptr>(sizeof(SecondOrderRay) * vp.width() * vp.height());

        m_rayBuffers[0]->setData(bufferSize, nullptr, GL_DYNAMIC_READ);
        m_rayBuffers[1]->setData(bufferSize, nullptr, GL_DYNAMIC_READ);
    }

    const GLuint depth = maxRayDepth.data(s_maxRayDepthDefault);
    if (viewport.hasChanged() || maxRayDepth.hasChanged())
    {
        setupPathStack(depth + 1, { vp.width(), vp.height() });
    }

    clearPathStack();


    bindCommonObjects();

    glm::vec2 coarseFragmentCount = glm::vec2(viewport.data()->width(), viewport.data()->height()) / float(coarseSamplingWindowSize.data(s_coarseSamplingWindowSizeDefault));

    glm::uvec3 workGroupCount(glm::ceil(glm::vec3(coarseFragmentCount, 1) / glm::vec3(s_workGroupSize)));


    GLuint numRays = computeFirstOrderRays(workGroupCount);
    computeShadowRays(0, numRays);

    GLuint currentDepth = 1;
    for (; currentDepth <= depth; ++currentDepth)
    {
        if (numRays == 0)
            break;
        
        numRays = computeSecondOrderRays(currentDepth, numRays);
        computeShadowRays(currentDepth, numRays);
    }

    flattenPath(currentDepth);

    aggregateColors();

    unbindCommonObjects();
}

GLuint GenericPathTracingStage::computeFirstOrderRays(const glm::uvec3 & workGroupCount)
{
    resetRayCounter();

    // raycast depths and normals
    normalTexture.data()->bindImageTexture(0, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
    depthTexture.data()->bindImageTexture(1, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F);

    bindPathStack(1, 0);
    bindRayOutputBuffer(2);  // for second order rays

    m_firstOrderRayProgram->dispatchCompute(workGroupCount);

    globjects::Texture::unbindImageTexture(0);
    globjects::Texture::unbindImageTexture(1);

    globjects::Buffer::unbind(GL_SHADER_STORAGE_BUFFER, 1);
    globjects::Buffer::unbind(GL_SHADER_STORAGE_BUFFER, 2);

    swapRaysBuffers();

    return getRayCounter();
}

GLuint GenericPathTracingStage::computeSecondOrderRays(GLuint depth, GLuint numRays)
{
    resetRayCounter();

    GLuint stride;
    glm::uvec3 workGroupCount = getSecondOrderRaysWorkgroupCount(numRays, &stride);
    m_secondOrderRayProgram->setUniform("numRays", numRays);
    m_secondOrderRayProgram->setUniform("stride", stride);
    m_secondOrderRayProgram->setUniform("depth", depth);

    bindPathStack(1, depth);
    bindRayInputBuffer(2);
    bindRayOutputBuffer(3);

    m_secondOrderRayProgram->dispatchCompute(workGroupCount);

    globjects::Buffer::unbind(GL_SHADER_STORAGE_BUFFER, 1);
    globjects::Buffer::unbind(GL_SHADER_STORAGE_BUFFER, 2);
    globjects::Buffer::unbind(GL_SHADER_STORAGE_BUFFER, 3);

    swapRaysBuffers();

    return getRayCounter();
}

void GenericPathTracingStage::computeShadowRays(gl::GLuint depth, gl::GLuint numRays)
{
    GLuint stride;
    glm::uvec3 workGroupCount = getSecondOrderRaysWorkgroupCount(numRays, &stride);

    m_shadowRayProgram->setUniform("numRays", numRays);
    m_shadowRayProgram->setUniform("stride", stride);

    bindPathStack(1, depth);
    bindRayInputBuffer(2);  // resulting rays from previous tracing

    m_shadowRayProgram->dispatchCompute(workGroupCount);

    globjects::Buffer::unbind(GL_SHADER_STORAGE_BUFFER, 1);
    globjects::Buffer::unbind(GL_SHADER_STORAGE_BUFFER, 2);
}

void GenericPathTracingStage::flattenPath(gl::GLuint numStackLayers)
{
    glm::uvec3 workGroupCount(glm::ceil(glm::vec3(m_pathStackLayerExtent, 1) / glm::vec3(s_workGroupSize)));

    colorPerFrameTexture.data()->bindImageTexture(0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

    bindPathStack(1);

    m_flattenPathStackProgram->setUniform("numStackLayers", numStackLayers);
    m_flattenPathStackProgram->setUniform("layerSize", m_pathStackLayerExtent);
    m_flattenPathStackProgram->setUniform("layerStride", m_pathStackLayerStride);

    m_flattenPathStackProgram->dispatchCompute(workGroupCount);

    globjects::Buffer::unbind(GL_SHADER_STORAGE_BUFFER, 1);

    globjects::Texture::unbindImageTexture(0);
}

void GenericPathTracingStage::aggregateColors()
{
    glm::uvec3 workGroupCount(glm::ceil(glm::vec3(m_pathStackLayerExtent, 1) / glm::vec3(s_workGroupSize)));

    colorPerFrameTexture.data()->bindImageTexture(0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
    colorTexture.data()->bindImageTexture(1, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

    m_aggregateColorsProgram->setUniform("layerSize", m_pathStackLayerExtent);

    m_aggregateColorsProgram->dispatchCompute(workGroupCount);

    globjects::Texture::unbindImageTexture(0);
    globjects::Texture::unbindImageTexture(1);
}

glm::uvec3 GenericPathTracingStage::getSecondOrderRaysWorkgroupCount(GLuint numRays, GLuint * stride)
{
    if (numRays == 0)
        return glm::uvec3(0);

    int strideX = nextMultiple(static_cast<int>(glm::ceil(
        glm::sqrt(static_cast<float>(numRays)))), int(s_workGroupSize.x));
    int strideY = nextMultiple(static_cast<int>(glm::ceil(static_cast<float>(numRays) / static_cast<float>(strideX))), int(s_workGroupSize.y));

    int x = strideX / s_workGroupSize.x;
    int y = strideY / s_workGroupSize.y;

    *stride = strideX;

    return glm::uvec3(x, y, 1); 
}

void GenericPathTracingStage::setupPathStack(GLuint stackDepth, glm::uvec2 layerSize)
{
    if (m_pathStackDepth == stackDepth && m_pathStackLayerExtent == layerSize)
        return;

    static GLint64 pathStackAlignment = -1;
    if (pathStackAlignment < 1)
    {
        GLint64 ssboAlignment;
        // https://www.opengl.org/registry/specs/ARB/shader_storage_buffer_object.txt
        glGetInteger64v(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, &ssboAlignment);

        pathStackAlignment = lcm(ssboAlignment, static_cast<GLint64>(sizeof(PathStackData)));
    }
    
    GLint64 layerDataSize = static_cast<GLint64>(layerSize.x * layerSize.y * sizeof(PathStackData));
    m_pathStackLayerDataSize = nextMultiple(layerDataSize, pathStackAlignment);
    m_pathStackLayerStride = static_cast<GLuint>((m_pathStackLayerDataSize - layerDataSize) / sizeof(PathStackData));
    assert(m_pathStackLayerDataSize == static_cast<gl::GLsizeiptr>(layerDataSize + sizeof(PathStackData) * m_pathStackLayerStride));

    m_pathStackBuffer->setData(
        static_cast<GLsizeiptr>(stackDepth * m_pathStackLayerDataSize),
        nullptr, GL_DYNAMIC_DRAW);

    m_pathStackDepth = stackDepth;
    m_pathStackLayerExtent = layerSize;
}

void GenericPathTracingStage::clearPathStack()
{
    glm::uvec3 workGroupCount(glm::ceil(glm::vec3(m_pathStackLayerExtent, 1) / glm::vec3(s_workGroupSize)));

    bindPathStack(1);

    m_clearPathStackProgram->setUniform("numStackLayers", m_pathStackDepth);
    m_clearPathStackProgram->setUniform("layerSize", m_pathStackLayerExtent);
    m_clearPathStackProgram->setUniform("layerStride", m_pathStackLayerStride);

    m_clearPathStackProgram->dispatchCompute(workGroupCount);

    globjects::Buffer::unbind(GL_SHADER_STORAGE_BUFFER, 1);
}

void GenericPathTracingStage::bindPathStack(gl::GLuint binding, int layer)
{
    if (layer < 0)
    {
        m_pathStackBuffer->bindBase(GL_SHADER_STORAGE_BUFFER, binding);
        return;
    }

    m_pathStackBuffer->bindRange(GL_SHADER_STORAGE_BUFFER, binding, m_pathStackLayerDataSize * layer, m_pathStackLayerDataSize);
}

void GenericPathTracingStage::bindCommonObjects()
{
    if (m_useTestingData)
    {
        m_testingIndices->bindBase(GL_SHADER_STORAGE_BUFFER, 6);
        m_testingVertices->bindBase(GL_SHADER_STORAGE_BUFFER, 7);
    }

    m_randomIndexTexture->bindActive(GL_TEXTURE2);
    m_randomVectorTexture->bindActive(GL_TEXTURE3);

    m_counterBuffer->bindBase(GL_ATOMIC_COUNTER_BUFFER, 0);
    m_coarsePixelOrderBuffer->bindBase(GL_SHADER_STORAGE_BUFFER, 0);
}

void GenericPathTracingStage::unbindCommonObjects()
{
    if (m_useTestingData)
    {
        globjects::Buffer::unbind(GL_SHADER_STORAGE_BUFFER, 6);
        globjects::Buffer::unbind(GL_SHADER_STORAGE_BUFFER, 7);
    }

    globjects::Texture::unbindImageTexture(2);
    globjects::Texture::unbindImageTexture(3);

    globjects::Buffer::unbind(GL_ATOMIC_COUNTER_BUFFER, 0);
    globjects::Buffer::unbind(GL_SHADER_STORAGE_BUFFER, 0);
}

void GenericPathTracingStage::bindRayOutputBuffer(gl::GLuint binding)
{
    m_rayBuffers[m_rayOutputBufferIndex]->bindBase(GL_SHADER_STORAGE_BUFFER, binding);
}

void GenericPathTracingStage::bindRayInputBuffer(gl::GLuint binding)
{
    m_rayBuffers[(m_rayOutputBufferIndex + 1) % 2]
        ->bindBase(GL_SHADER_STORAGE_BUFFER, binding);
}

void GenericPathTracingStage::swapRaysBuffers()
{
    m_rayOutputBufferIndex = (m_rayOutputBufferIndex + 1) % 2;
}

void GenericPathTracingStage::setupRayData()
{
    m_rayBuffers[0] = new globjects::Buffer();
    m_rayBuffers[1] = new globjects::Buffer();
    m_counterBuffer = new globjects::Buffer();
    m_counterBuffer->setData<GLuint, 1>({ { 0 } }, GL_DYNAMIC_DRAW);
    m_pathStackBuffer = new globjects::Buffer();

    m_coarsePixelOrderBuffer = new globjects::Buffer();
}

void GenericPathTracingStage::resetRayCounter()
{
    m_counterBuffer->setSubData<GLuint, 1>({{ 0 }});
}

GLuint GenericPathTracingStage::getRayCounter()
{
    return m_counterBuffer->getSubData<GLuint, 1>()[0];
}

void GenericPathTracingStage::loadShaders()
{
    m_firstOrderRayProgram = new globjects::Program();
    m_firstOrderRayProgram->setName("First Order Ray Program");
    m_firstOrderRayProgram->attach(globjects::Shader::fromFile(
        GL_COMPUTE_SHADER, s_shader_path + "firstOrderRays.comp"));

    m_firstOrderRayProgram->link();

    m_secondOrderRayProgram = new globjects::Program();
    m_secondOrderRayProgram->setName("Second Order Ray Program");
    m_secondOrderRayProgram->attach(globjects::Shader::fromFile(
        GL_COMPUTE_SHADER, s_shader_path + "secondOrderRays.comp"));

    m_secondOrderRayProgram->link();

    m_shadowRayProgram = new globjects::Program();
    m_shadowRayProgram->setName("Shadow Ray Program");
    m_shadowRayProgram->attach(globjects::Shader::fromFile(
        GL_COMPUTE_SHADER, s_shader_path + "shadowRays.comp"));

    m_shadowRayProgram->link();


    m_clearPathStackProgram = new globjects::Program();
    m_clearPathStackProgram->setName("Clear Path Stack Program");
    m_clearPathStackProgram->attach(globjects::Shader::fromFile(
        GL_COMPUTE_SHADER, s_shader_path + "clearPathStack.comp"));

    m_clearPathStackProgram->link();


    m_flattenPathStackProgram = new globjects::Program();
    m_flattenPathStackProgram->setName("Flatten Path Stack Program");
    m_flattenPathStackProgram->attach(globjects::Shader::fromFile(
        GL_COMPUTE_SHADER, s_shader_path + "flattenPathStack.comp"));

    m_flattenPathStackProgram->link();

    m_aggregateColorsProgram = new globjects::Program();
    m_aggregateColorsProgram->setName("Color Aggregation Program");
    m_aggregateColorsProgram->attach(globjects::Shader::fromFile(
        GL_COMPUTE_SHADER, s_shader_path + "aggregateColors.comp"));

}

void GenericPathTracingStage::initializeExtensionShaders()
{
    std::string extPath{ "/PATHTRACING_EXTENSIONS/" };

    for (const auto & ext : m_namedExtensionShaders)
    {
        const std::string & extName = ext.first;
        const std::string & extFileName = ext.second;

        globjects::NamedString::create(extPath + extName, new globjects::File(extFileName));
    }

    // render testing data only if geometry not defined by the user
    m_useTestingData = m_namedExtensionShaders.find("geometryTraversal") == m_namedExtensionShaders.end();

    // load templates for not implemented extensions
    for (const std::string & ext : extensionNames())
    {
        std::string fileName = s_shader_path + "extension_templates/" + ext + ".glsl";
        assert(std::ifstream(fileName).good());
        if (!globjects::NamedString::isNamedString(s_ext_namespace + ext))
            globjects::NamedString::create(s_ext_namespace + ext,
            new globjects::File(fileName));
    }
}

void GenericPathTracingStage::setupTextures()
{
    colorTexture = globjects::Texture::createDefault();
    colorTexture.data()->setName("Color Texture");

    colorPerFrameTexture = globjects::Texture::createDefault();
    colorPerFrameTexture.data()->setName("Color Per Frame Texture");

    normalTexture = globjects::Texture::createDefault();
    normalTexture.data()->setName("Normal Texture");

    depthTexture = globjects::Texture::createDefault();
    depthTexture.data()->setName("Depth Texture");


    m_randomIndexTexture = globjects::Texture::createDefault(GL_TEXTURE_1D);
    m_randomIndexTexture->setName("Random Index Texture");
    m_randomIndexTexture->setParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    m_randomIndexTexture->setParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    m_randomIndexTexture->setParameter(GL_TEXTURE_WRAP_S, GL_REPEAT);
    m_randomIndexTexture->setParameter(GL_TEXTURE_WRAP_T, GL_REPEAT);
    m_randomIndexTexture->setParameter(GL_TEXTURE_WRAP_R, GL_REPEAT);

    m_randomVectorTexture = new globjects::Texture(GL_TEXTURE_2D);
    m_randomVectorTexture->setName("Random Vector Texture");
    m_randomVectorTexture->setParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    m_randomVectorTexture->setParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    m_randomVectorTexture->setParameter(GL_TEXTURE_WRAP_S, GL_REPEAT);
    m_randomVectorTexture->setParameter(GL_TEXTURE_WRAP_T, GL_REPEAT);
    m_randomVectorTexture->setParameter(GL_TEXTURE_WRAP_R, GL_REPEAT);

    GLsizei numIndices = 1024;
    GLsizei vectorTexWidth = 100;
    std::vector<GLint> indices(numIndices);
    std::vector<glm::vec3> vectors(vectorTexWidth * vectorTexWidth);

    std::mt19937 engine;
    std::uniform_int_distribution<GLint> distribution;

    for (size_t i = 0; i < indices.size(); ++i)
        indices[i] = distribution(engine);

    for (size_t i = 0; i < vectors.size(); ++i)
        vectors[i] = glm::sphericalRand<float>(1.f);

    m_randomIndexTexture->image1D(0, GL_R32I, numIndices, 0, GL_RED_INTEGER, GL_INT, indices.data());
    m_randomVectorTexture->image2D(0, GL_RGB32F, vectorTexWidth, vectorTexWidth, 0, GL_RGB, GL_FLOAT, vectors.data());
}

void GenericPathTracingStage::clearTextures(const glm::ivec2 & size)
{
    glm::vec4 f32White{ 1.0f, 1.0f, 1.0f, 1.0f };
    colorTexture.data()->clearImage(0, GL_RGBA, GL_FLOAT, &f32White);
    colorPerFrameTexture.data()->clearImage(0, GL_RGBA, GL_FLOAT, &f32White);
    depthTexture.data()->clearImage(0, GL_RED, GL_FLOAT, nullptr);
    normalTexture.data()->clearImage(0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    clearTexturesEvent(size);
}

void GenericPathTracingStage::resizeTextures(const glm::ivec2 & size)
{
    colorTexture.data()->image2D(0, GL_RGBA32F, size, 0, GL_RGBA, GL_FLOAT, nullptr);
    colorPerFrameTexture.data()->image2D(0, GL_RGBA32F, size, 0, GL_RGBA, GL_FLOAT, nullptr);
    depthTexture.data()->image2D(0, GL_R32F, size, 0, GL_RED, GL_FLOAT, nullptr);
    normalTexture.data()->image2D(0, GL_RGBA8, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    resizeTexturesEvent(size);
}

void GenericPathTracingStage::setupUniforms()
{
    auto camera = make_unique<UniformGroup>();

    camera->addUniform(globjects::make_ref<globjects::Uniform<glm::vec3>>("eye"));
    camera->addUniform(globjects::make_ref<globjects::Uniform<glm::vec3>>("center"));
    camera->addUniform(globjects::make_ref<globjects::Uniform<glm::vec3>>("up"));
    camera->addUniform(globjects::make_ref<globjects::Uniform<float>>("fov"));
    camera->addUniform(globjects::make_ref<globjects::Uniform<float>>("aspect"));
    camera->addUniform(globjects::make_ref<globjects::Uniform<float>>("zNear"));
    camera->addUniform(globjects::make_ref<globjects::Uniform<float>>("zFar"));
    camera->addUniform(globjects::make_ref<globjects::Uniform<glm::mat3>>("rayMatrix"));

    camera->addToProgram(m_firstOrderRayProgram);
    camera->addToProgram(m_shadowRayProgram);

    m_uniformGroups.emplace("camera", std::move(camera));

    auto sampling = make_unique<UniformGroup>();
    
    sampling->addUniform(globjects::make_ref<globjects::Uniform<GLuint>>("coarseSamplingWindowSize"));
    sampling->addUniform(globjects::make_ref<globjects::Uniform<GLuint>>("frameCounter"));
    sampling->addUniform(globjects::make_ref<globjects::Uniform<GLuint>>("time"));

    sampling->addToProgram(m_firstOrderRayProgram);
    sampling->addToProgram(m_secondOrderRayProgram);
    sampling->addToProgram(m_shadowRayProgram);
    sampling->addToProgram(m_flattenPathStackProgram);
    sampling->addToProgram(m_aggregateColorsProgram);

    m_uniformGroups.emplace("sampling", std::move(sampling));
}

void GenericPathTracingStage::updateUniforms()
{
    if (camera.hasChanged() || projection.hasChanged())
    {
        auto perspective = dynamic_cast<gloperate::AbstractPerspectiveProjectionCapability *>(projection.data());
        assert(perspective);

        const auto & cameraGroup = m_uniformGroups["camera"];

        cameraGroup->uniform<glm::vec3>("eye")->set(camera.data()->eye());
        cameraGroup->uniform<glm::vec3>("center")->set(camera.data()->center());
        cameraGroup->uniform<glm::vec3>("up")->set(camera.data()->up());
        cameraGroup->uniform<float>("fovy")->set(perspective->fovy());
        cameraGroup->uniform<float>("aspect")->set(perspective->aspectRatio());
        cameraGroup->uniform<float>("zNear")->set(perspective->zNear());
        cameraGroup->uniform<float>("zFar")->set(perspective->zFar());

        cameraGroup->uniform<glm::mat3>("rayMatrix")->set(rayMatrix(
            camera.data()->eye(), camera.data()->center(), camera.data()->up(),
            perspective->fovy(), perspective->aspectRatio()));
    }

    const auto & samplingGroup = m_uniformGroups["sampling"];

    if (coarseSamplingWindowSize.hasChanged())
    {
        samplingGroup->uniform<GLuint>("coarseSamplingWindowSize")->set(coarseSamplingWindowSize.data(s_coarseSamplingWindowSizeDefault));
    }

    samplingGroup->uniform<GLuint>("frameCounter")->set(m_frameCounter);
    auto randomTime = static_cast<GLuint>(std::chrono::steady_clock::now().time_since_epoch().count() & 0xFFFFFFFF);
    samplingGroup->uniform<GLuint>("time")->set(randomTime);

    setUniform("viewportSize", glm::ivec2( viewport.data()->width(), viewport.data()->height() ));

    m_firstOrderRayProgram->setUniform(10, // randomOffset
        glm::vec2(glm::linearRand(-1.0f, 1.0f), glm::linearRand(-1.0f, 1.0f)));

    m_secondOrderRayProgram->setUniform(10, // randomOffset
        glm::vec2(glm::linearRand(-1.0f, 1.0f), glm::linearRand(-1.0f, 1.0f)));

    m_shadowRayProgram->setUniform(10, // randomOffset
        glm::vec2(glm::linearRand(-1.0f, 1.0f), glm::linearRand(-1.0f, 1.0f)));
}

void GenericPathTracingStage::updateSampling()
{
    if (!coarseSamplingWindowSize.hasChanged())
        return;

    // find the next power of two coarse sampling window size
    GLuint powerOf2WindowSize = 1;
    while (powerOf2WindowSize < coarseSamplingWindowSize.data(s_coarseSamplingWindowSizeDefault))
        powerOf2WindowSize *= 2;
    
    /*
    * trace one pixel per sampling window (origin, minimum coordinate)
    * set whole window color to this pixel
    * split window equally -> four sub-windows
    * for each sub-window, trace one pixel, set window color, split into four
            0 2     <-- window call order
            3 1
    * discard all calls for virtual pixels, that are not part of the actual window
    * (start with power of 2 window size, two simplify splits)
    */
    struct CoarseSamplingWindow
    {
        glm::uvec2 origin;      // minimal coordinate of the window
        glm::uvec2 windowSize;  // window size to fill with one traced color
    };

    std::map<int, std::vector<CoarseSamplingWindow>> windowLayers;

    GLuint actualWindowSize = coarseSamplingWindowSize.data(s_coarseSamplingWindowSizeDefault);
    std::function<void(GLuint, const glm::uvec2 &, int)> addCoarseSampleWindow =
        [&windowLayers, &addCoarseSampleWindow, &actualWindowSize](
        GLuint subWindowSize, const glm::uvec2 & origin, int depth)
    {
        // ignore power of 2 sub-windows which don't match into the actual window size
        if (origin.x >= actualWindowSize || origin.y >= actualWindowSize)
            return;

        glm::uvec2 actualSubWindowSize =
            glm::min(glm::uvec2(actualWindowSize), origin + glm::uvec2(subWindowSize)) // actual maximal coordinate
            - origin;

        windowLayers[depth].push_back({origin, actualSubWindowSize});

        GLuint nextWindowSize = subWindowSize /= 2; // next lower power of two window size
        if (nextWindowSize == 0)
            return;

        addCoarseSampleWindow(nextWindowSize, origin, depth + 1);
        addCoarseSampleWindow(nextWindowSize, origin + nextWindowSize, depth + 1);
        addCoarseSampleWindow(nextWindowSize, origin + glm::uvec2(nextWindowSize, 0), depth + 1);
        addCoarseSampleWindow(nextWindowSize, origin + glm::uvec2(0, nextWindowSize), depth + 1);
    };

    addCoarseSampleWindow(powerOf2WindowSize, {0, 0}, 0);

    std::vector<CoarseSamplingWindow> coarseSamplingWindows;
    std::vector<std::vector<bool>> addedPixels(actualWindowSize, std::vector<bool>(actualWindowSize, false));
    int depth = 0;
    std::map<int, std::vector<CoarseSamplingWindow>>::iterator layer;
    for (int depth = 0; (layer = windowLayers.find(depth)) != windowLayers.end(); ++depth)
    {
        for (auto & window : layer->second)
        {
            const glm::uvec2 & tracedPixel = window.origin;
            // if we traced the pixel on a higher resolution, don't trace it again now
            if (addedPixels[tracedPixel.x][tracedPixel.y])
                continue;

            addedPixels[tracedPixel.x][tracedPixel.y] = true;

            coarseSamplingWindows.push_back(window);
        }
    }

    assert(coarseSamplingWindows.size() == actualWindowSize * actualWindowSize);

    m_coarsePixelOrderBuffer->setData(coarseSamplingWindows, GL_DYNAMIC_READ);
}

glm::mat3 GenericPathTracingStage::rayMatrix(
    const glm::vec3 & eye, const glm::vec3 & center, const glm::vec3 & up,
    float fovy, float aspectRatio)
{
    glm::vec3 eyeDir = glm::normalize(center - eye);
    glm::vec3 side = glm::normalize(glm::cross(eyeDir, up));
    glm::vec3 newUp = glm::normalize(glm::cross(side, eyeDir));

    float l = 1.f;

    float h = 2.f*l*glm::tan(fovy / 2.f);
    float w = h*aspectRatio;

    return glm::mat3(side * w / 2.f, newUp * h / 2.f, eyeDir * l);
}

void GenericPathTracingStage::setupTestingData()
{
    // Cornell Box Data: http://www.graphics.cornell.edu/online/box/data.html
    // Code from https://github.com/cgcostume/pathgl/blob/master/pathgl.cpp

    std::vector<glm::vec4> vertices;
    // lights
    vertices.push_back(glm::vec4(343.0, 448.8, 227.0, 0.0)); // 00
    vertices.push_back(glm::vec4(343.0, 548.8, 332.0, 0.0)); // 01
    vertices.push_back(glm::vec4(213.0, 548.8, 332.0, 0.0)); // 02
    vertices.push_back(glm::vec4(213.0, 548.8, 227.0, 0.0)); // 03
    // room
    vertices.push_back(glm::vec4(0.0, 0.0, 0.0, 0.0)); // 04 
    vertices.push_back(glm::vec4(0.0, 0.0, 559.2, 0.0)); // 05 
    vertices.push_back(glm::vec4(0.0, 548.8, 0.0, 0.0)); // 06
    vertices.push_back(glm::vec4(0.0, 548.8, 559.2, 0.0)); // 07
    vertices.push_back(glm::vec4(552.8, 0.0, 0.0, 0.0)); // 08 
    vertices.push_back(glm::vec4(549.6, 0.0, 559.2, 0.0)); // 09
    vertices.push_back(glm::vec4(556.0, 548.8, 0.0, 0.0)); // 10
    vertices.push_back(glm::vec4(556.0, 548.8, 559.2, 0.0)); // 11
    // short block
    vertices.push_back(glm::vec4(290.0, 0.0, 114.0, 0.0)); // 12
    vertices.push_back(glm::vec4(290.0, 165.0, 114.0, 0.0)); // 13
    vertices.push_back(glm::vec4(240.0, 0.0, 272.0, 0.0)); // 14
    vertices.push_back(glm::vec4(240.0, 165.0, 272.0, 0.0)); // 15
    vertices.push_back(glm::vec4(82.0, 0.0, 225.0, 0.0)); // 16
    vertices.push_back(glm::vec4(82.0, 165.0, 225.0, 0.0)); // 17
    vertices.push_back(glm::vec4(130.0, 0.0, 65.0, 0.0)); // 18
    vertices.push_back(glm::vec4(130.0, 165.0, 65.0, 0.0)); // 19
    // tall block
    vertices.push_back(glm::vec4(423.0, 0.0, 247.0, 0.0)); // 20
    vertices.push_back(glm::vec4(423.0, 330.0, 247.0, 0.0)); // 21
    vertices.push_back(glm::vec4(472.0, 0.0, 406.0, 0.0)); // 22
    vertices.push_back(glm::vec4(472.0, 330.0, 406.0, 0.0)); // 23
    vertices.push_back(glm::vec4(314.0, 0.0, 456.0, 0.0)); // 24
    vertices.push_back(glm::vec4(314.0, 330.0, 456.0, 0.0)); // 25
    vertices.push_back(glm::vec4(265.0, 0.0, 296.0, 0.0)); // 26
    vertices.push_back(glm::vec4(265.0, 330.0, 296.0, 0.0)); // 27

    std::vector<glm::uvec4> indices;
    // light
    indices.push_back(glm::uvec4(0, 1, 2, 0));
    indices.push_back(glm::uvec4(0, 2, 3, 0));
    // room ceiling
    indices.push_back(glm::uvec4(10, 11, 7, 1));
    indices.push_back(glm::uvec4(10, 7, 6, 1));
    // room floor
    indices.push_back(glm::uvec4(8, 4, 5, 1));
    indices.push_back(glm::uvec4(8, 5, 9, 1));
    // room front wall
    indices.push_back(glm::uvec4(10, 6, 4, 1));
    indices.push_back(glm::uvec4(10, 4, 8, 1));
    // room back wall
    indices.push_back(glm::uvec4(9, 5, 7, 1));
    indices.push_back(glm::uvec4(9, 7, 11, 1));
    // room right wall
    indices.push_back(glm::uvec4(5, 4, 6, 3));
    indices.push_back(glm::uvec4(5, 6, 7, 3));
    // room left wall
    indices.push_back(glm::uvec4(8, 9, 11, 2));
    indices.push_back(glm::uvec4(8, 11, 10, 2));
    // short block
    indices.push_back(glm::uvec4(19, 17, 15, 1));
    indices.push_back(glm::uvec4(19, 15, 13, 1));
    indices.push_back(glm::uvec4(12, 13, 15, 1));
    indices.push_back(glm::uvec4(12, 15, 14, 1));
    indices.push_back(glm::uvec4(18, 19, 13, 1));
    indices.push_back(glm::uvec4(18, 13, 12, 1));
    indices.push_back(glm::uvec4(16, 17, 19, 1));
    indices.push_back(glm::uvec4(16, 19, 18, 1));
    indices.push_back(glm::uvec4(14, 15, 17, 1));
    indices.push_back(glm::uvec4(14, 17, 16, 1));
    // tall block
    indices.push_back(glm::uvec4(27, 25, 23, 1));
    indices.push_back(glm::uvec4(27, 23, 21, 1));
    indices.push_back(glm::uvec4(20, 21, 23, 1));
    indices.push_back(glm::uvec4(20, 23, 22, 1));
    indices.push_back(glm::uvec4(26, 27, 21, 1));
    indices.push_back(glm::uvec4(26, 21, 20, 1));
    indices.push_back(glm::uvec4(24, 25, 27, 1));
    indices.push_back(glm::uvec4(24, 27, 26, 1));
    indices.push_back(glm::uvec4(22, 23, 25, 1));
    indices.push_back(glm::uvec4(22, 25, 24, 1));

    m_testingIndices = new globjects::Buffer();
    m_testingVertices = new globjects::Buffer();
    m_testingIndices->setData(indices, GL_STATIC_READ);
    m_testingVertices->setData(vertices, GL_STATIC_READ);
}

} // namespace gloperate
