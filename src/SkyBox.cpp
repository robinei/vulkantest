#include "SkyBox.h"
#include "AssetLoader.h"

struct SkyboxVertex {
    float position[3];
};

static const SkyboxVertex skyboxVertices[] = {
    { -1.0f,  1.0f, -1.0f },
    {  1.0f,  1.0f, -1.0f },
    { -1.0f, -1.0f, -1.0f },
    {  1.0f, -1.0f, -1.0f },
    { -1.0f,  1.0f,  1.0f },
    {  1.0f,  1.0f,  1.0f },
    { -1.0f, -1.0f,  1.0f },
    {  1.0f, -1.0f,  1.0f },
};

static unsigned short skyboxIndices[] = {
    0, 1, 2,    // side 1
    2, 1, 3,
    4, 0, 6,    // side 2
    6, 0, 2,
    7, 5, 6,    // side 3
    6, 5, 4,
    3, 1, 7,    // side 4
    7, 1, 5,
    4, 5, 0,    // side 5
    0, 5, 1,
    3, 7, 2,    // side 6
    2, 7, 6,
};

static ShaderAssetHandle vertShader;
static ShaderAssetHandle fragShader;
static TextureAssetHandle cubemap;

static nvrhi::BufferHandle skyboxVertexBuffer;
static nvrhi::BufferHandle skyboxIndexBuffer;
static nvrhi::GraphicsPipelineHandle skyboxPipeline;
static nvrhi::SamplerHandle linearClampSampler;
static nvrhi::BindingLayoutHandle bindingLayout;
static nvrhi::BindingSetHandle skyboxBindings;

void initSkyBox() {
    vertShader = AssetLoader::getShader("skybox.vert.spv", nvrhi::ShaderType::Vertex);
    fragShader = AssetLoader::getShader("skybox.frag.spv", nvrhi::ShaderType::Pixel);
}

static void doInit(RenderContext &context) {
    auto samplerDesc = nvrhi::SamplerDesc()
        .setAllFilters(true)
        .setAllAddressModes(nvrhi::SamplerAddressMode::Clamp);
    samplerDesc.setAllFilters(true);
    linearClampSampler = context.device->createSampler(samplerDesc);

    skyboxVertexBuffer = context.device->createBuffer(nvrhi::BufferDesc()
        .setByteSize(sizeof(skyboxVertices))
        .setIsVertexBuffer(true)
        .setInitialState(nvrhi::ResourceStates::VertexBuffer)
        .setKeepInitialState(true) // enable fully automatic state tracking
        .setDebugName("Vertex Buffer"));
    skyboxIndexBuffer = context.device->createBuffer(nvrhi::BufferDesc()
        .setByteSize(sizeof(skyboxIndices))
        .setIsIndexBuffer(true)
        .setInitialState(nvrhi::ResourceStates::IndexBuffer)
        .setKeepInitialState(true) // enable fully automatic state tracking
        .setDebugName("Index Buffer"));
    context.commandList->open();
    context.commandList->writeBuffer(skyboxVertexBuffer, skyboxVertices, sizeof(skyboxVertices));
    context.commandList->writeBuffer(skyboxIndexBuffer, skyboxIndices, sizeof(skyboxIndices));
    context.commandList->close();
    context.device->executeCommandList(context.commandList);

    auto layoutDesc = nvrhi::BindingLayoutDesc()
        .setVisibility(nvrhi::ShaderType::All)
        .addItem(nvrhi::BindingLayoutItem::PushConstants(0, sizeof(float)*16))
        .addItem(nvrhi::BindingLayoutItem::Sampler(0))
        .addItem(nvrhi::BindingLayoutItem::Texture_SRV(1));
    layoutDesc.bindingOffsets.setSamplerOffset(0);
    bindingLayout = context.device->createBindingLayout(layoutDesc);

    nvrhi::VertexAttributeDesc attributes[] = {
        nvrhi::VertexAttributeDesc()
            .setName("POSITION")
            .setFormat(nvrhi::Format::RGB32_FLOAT)
            .setOffset(offsetof(SkyboxVertex, position))
            .setElementStride(sizeof(SkyboxVertex)),
    };
    nvrhi::InputLayoutHandle inputLayout = context.device->createInputLayout(attributes, 1, vertShader->get());

    auto pipelineDesc = nvrhi::GraphicsPipelineDesc()
        .setInputLayout(inputLayout)
        .setVertexShader(vertShader->get())
        .setPixelShader(fragShader->get())
        .addBindingLayout(bindingLayout);
    pipelineDesc.renderState.rasterState.setCullNone();
    pipelineDesc.renderState.depthStencilState.setDepthTestEnable(false);
    pipelineDesc.renderState.depthStencilState.setDepthWriteEnable(false);
    skyboxPipeline = context.device->createGraphicsPipeline(pipelineDesc, context.framebuffer);
    assert(skyboxPipeline);
}

void deinitSkyBox() {
    vertShader = nullptr;
    fragShader = nullptr;
    cubemap = nullptr;
    skyboxVertexBuffer = nullptr;
    skyboxIndexBuffer = nullptr;
    skyboxPipeline = nullptr;
    linearClampSampler = nullptr;
    bindingLayout = nullptr;
    skyboxBindings = nullptr;
}

void setSkyBoxTexture(const std::string &path) {
    cubemap = AssetLoader::getTexture(path, nvrhi::TextureDimension::TextureCube);
    skyboxBindings = nullptr;
}

void updateSkyBox(RenderContext &context) {
    if (!skyboxPipeline) {
        if (!vertShader->isLoaded() || !fragShader->isLoaded()) {
            return;
        }
        doInit(context);
    }
    if (!skyboxBindings) {
        if (!cubemap->isLoaded()) {
            return;
        }
        skyboxBindings = context.device->createBindingSet(nvrhi::BindingSetDesc()
            .addItem(nvrhi::BindingSetItem::PushConstants(0, sizeof(float)*16))
            .addItem(nvrhi::BindingSetItem::Sampler(0, linearClampSampler))
            .addItem(nvrhi::BindingSetItem::Texture_SRV(1, cubemap->get())), bindingLayout);
    }
}

void renderSkyBox(RenderContext &context) {
    if (!skyboxPipeline || !skyboxBindings) {
        return;
    }
    auto graphicsState = nvrhi::GraphicsState()
        .setPipeline(skyboxPipeline)
        .setFramebuffer(context.framebuffer)
        .setViewport(nvrhi::ViewportState().addViewportAndScissorRect(context.viewport))
        .addBindingSet(skyboxBindings)
        .setIndexBuffer(nvrhi::IndexBufferBinding().setFormat(nvrhi::Format::R16_UINT).setBuffer(skyboxIndexBuffer))
        .addVertexBuffer(nvrhi::VertexBufferBinding().setSlot(0).setOffset(0).setBuffer(skyboxVertexBuffer));
    context.commandList->setGraphicsState(graphicsState);
    glm::mat4 vm = context.camera->getViewMatrix();
    vm[3].x = 0;
    vm[3].y = 0;
    vm[3].z = 0;
    glm::mat4 pvm = context.camera->getPerspectiveMatrix() * vm;
    context.commandList->setPushConstants(&pvm, sizeof(pvm));
    context.commandList->drawIndexed(nvrhi::DrawArguments().setVertexCount(36));
}
