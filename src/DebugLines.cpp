#include "DebugLines.h"
#include "AssetLoader.h"
#include <nvrhi/nvrhi.h>

struct LineVertex {
    glm::vec3 position;
    glm::vec4 color;
};

struct Line {
    LineVertex a, b;
};

#define MAX_LINES 2048

static Line lines[MAX_LINES];
static std::atomic<int> lineCount;

static ShaderAssetHandle vertShader;
static ShaderAssetHandle fragShader;

static nvrhi::BufferHandle lineVertexBuffer;
static nvrhi::GraphicsPipelineHandle lineGraphicsPipeline;
static nvrhi::BindingSetHandle lineBindingSet;

void initDebugLines() {
    vertShader = AssetLoader::getShader("trivial_color.vert.spv", nvrhi::ShaderType::Vertex);
    fragShader = AssetLoader::getShader("trivial_color.frag.spv", nvrhi::ShaderType::Pixel);
}

static void doInit(RenderContext &context) {
    lineVertexBuffer = context.device->createBuffer(nvrhi::BufferDesc()
        .setByteSize(sizeof(lines))
        .setIsVertexBuffer(true)
        .setInitialState(nvrhi::ResourceStates::VertexBuffer)
        .setKeepInitialState(true));

    auto layoutDesc = nvrhi::BindingLayoutDesc()
        .setVisibility(nvrhi::ShaderType::All)
        .addItem(nvrhi::BindingLayoutItem::PushConstants(0, sizeof(float)*16));
    nvrhi::BindingLayoutHandle bindingLayout = context.device->createBindingLayout(layoutDesc);
    
    lineBindingSet = context.device->createBindingSet(nvrhi::BindingSetDesc()
        .addItem(nvrhi::BindingSetItem::PushConstants(0, sizeof(float)*16)), bindingLayout);

    nvrhi::VertexAttributeDesc attributes[] = {
        nvrhi::VertexAttributeDesc()
            .setName("POSITION")
            .setFormat(nvrhi::Format::RGB32_FLOAT)
            .setOffset(offsetof(LineVertex, position))
            .setElementStride(sizeof(LineVertex)),
        nvrhi::VertexAttributeDesc()
            .setName("COLOR")
            .setFormat(nvrhi::Format::RGBA32_FLOAT)
            .setOffset(offsetof(LineVertex, color))
            .setElementStride(sizeof(LineVertex)),
    };
    nvrhi::InputLayoutHandle inputLayout = context.device->createInputLayout(attributes, 2, vertShader->get());

    auto pipelineDesc = nvrhi::GraphicsPipelineDesc()
        .setPrimType(nvrhi::PrimitiveType::LineList)
        .setInputLayout(inputLayout)
        .setVertexShader(vertShader->get())
        .setPixelShader(fragShader->get())
        .addBindingLayout(bindingLayout);
    pipelineDesc.renderState.rasterState.setCullNone();
    pipelineDesc.renderState.depthStencilState.setDepthTestEnable(false);
    lineGraphicsPipeline = context.device->createGraphicsPipeline(pipelineDesc, context.framebuffer);
    assert(lineGraphicsPipeline);
}

void deinitDebugLines() {
    vertShader = nullptr;
    fragShader = nullptr;
    lineBindingSet = nullptr;
    lineVertexBuffer = nullptr;
    lineGraphicsPipeline = nullptr;
}

void clearDebugLines() {
    lineCount = 0;
}

void drawDebugLine(const glm::vec3 &a, const glm::vec3 &b, const glm::vec4 &color) {
    int index = lineCount++;
    assert(index < MAX_LINES);
    Line *line = lines + index;
    line->a.position = a;
    line->a.color = color;
    line->b.position = b;
    line->b.color = color;
}

void updateDebugLines(RenderContext &context) {
    if (!lineGraphicsPipeline) {
        if (!vertShader->isLoaded() || !fragShader->isLoaded()) {
            return;
        }
        doInit(context);
    }
}

void renderDebugLines(RenderContext &context) {
    if (!lineGraphicsPipeline) {
        return;
    }
    context.commandList->writeBuffer(lineVertexBuffer, lines, sizeof(Line)*lineCount);
    auto graphicsState = nvrhi::GraphicsState()
        .setPipeline(lineGraphicsPipeline)
        .setFramebuffer(context.framebuffer)
        .setViewport(nvrhi::ViewportState().addViewportAndScissorRect(context.viewport))
        .addBindingSet(lineBindingSet)
        .addVertexBuffer(nvrhi::VertexBufferBinding().setSlot(0).setOffset(0).setBuffer(lineVertexBuffer));
    context.commandList->setGraphicsState(graphicsState);
    glm::mat4 pvm = context.camera->getProjectionMatrix() * context.camera->getViewMatrix();
    context.commandList->setPushConstants(&pvm, sizeof(pvm));
    context.commandList->draw(nvrhi::DrawArguments().setVertexCount(lineCount * 2));
}
