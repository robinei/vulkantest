#include "AssetLoader.h"
#include "Logger.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <cassert>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <deque>
#include <vector>
#include <unordered_map>


#define MAX_READER_THREADS 2

struct ReadRequest {
    std::string path;
    std::function<void (unsigned char *, size_t)> handler;
};


template<typename T>
class ConcurrentQueue {
    std::deque<T> queue;
    std::mutex mutex;
    std::condition_variable cond;

public:
    void push(T &&value) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            queue.push_back(value);
        }
        cond.notify_one();
    }

    T pop() {
        std::unique_lock lock(mutex);
        cond.wait(lock, [this] { return !queue.empty(); });
        assert(!queue.empty());
        T result = queue.front();
        queue.pop_front();
        return result;
    }

    bool empty() {
        std::lock_guard<std::mutex> lock(mutex);
        return queue.empty();
    }
};

static nvrhi::IDevice *device;

static std::mutex commandListsMutex;
static std::vector<nvrhi::CommandListHandle> commandLists;

static ConcurrentQueue<ReadRequest> readRequestQueue;

static std::vector<std::thread> readerThreads;


template<typename T>
class AssetMap {
    std::mutex mutex;
    std::unordered_map<std::string, Ref<Asset<T>>> map;

public:
    template <typename Handler>
    Ref<Asset<T>> getOrCreateAsset(const std::string &path, Handler &&handler) {
        Ref<Asset<T>> asset;
        std::lock_guard<std::mutex> lock(mutex);
        auto it = map.find(path);
        if (it != map.end()) {
            asset = it->second;
        } else {
            asset = new Asset<T>(path);
            map.insert({path, asset});
            asset->release(); // ref count starts at 1, so release after we are storing a reference
            ReadRequest request;
            request.path = path;
            request.handler = [asset, handler = std::move(handler)] (unsigned char *buffer, size_t size) mutable {
                Job::enqueue([asset, buffer, size, handler = std::move(handler)] () mutable {
                    asset->setLoadedAsset(std::move(handler(asset->getPath(), buffer, size)));
                    free(buffer);
                }, JobType::BACKGROUND);
            };
            readRequestQueue.push(std::move(request));
        }
        asset->addWaitingScope(JobScope::getActiveScope());
        return asset;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex);
        map.clear();
    }
};


static AssetMap<nvrhi::ShaderHandle> shaderAssets;
static AssetMap<nvrhi::TextureHandle> texture2DAssets;
static AssetMap<nvrhi::TextureHandle> textureCubeAssets;


static nvrhi::CommandListHandle getOrCreateCommandList() {
    nvrhi::CommandListHandle commandList;
    {
        std::lock_guard<std::mutex> lock(commandListsMutex);
        if (!commandLists.empty()) {
            commandList = commandLists.back();
            commandLists.pop_back();
        } else {
            commandList = device->createCommandList(nvrhi::CommandListParameters().setEnableImmediateExecution(false));
        }
    }
    commandList->open();
    return commandList;
}

static void finishCommandList(nvrhi::CommandListHandle commandList) {
    commandList->close();
    device->executeCommandList(commandList);
    std::lock_guard<std::mutex> lock(commandListsMutex);
    commandLists.push_back(commandList);
}


static unsigned char *readFile(const char *path, size_t &size) {
    FILE *fp = fopen(path, "rb");
    assert(fp);
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    unsigned char *buf = (unsigned char *)malloc(size + 1);
    size_t n = fread(buf, 1, size, fp);
    assert(n == size);
    fclose(fp);
    buf[size] = '\0';
    return buf;
}

static void readerThreadFunc() {
    for (;;) {
        auto request = readRequestQueue.pop();
        if (request.path.empty()) {
            logger->debug("Stopping reader thread.");
            break;
        }
        size_t size = 0;
        unsigned char *buffer = readFile(request.path.c_str(), size);
        assert(buffer);
        request.handler(buffer, size);
    }
}


void AssetLoader::initialize(nvrhi::IDevice *dev) {
    device = dev;
    for (int i = 0; i < MAX_READER_THREADS; ++i) {
        readerThreads.emplace_back([] { readerThreadFunc(); });
    }
}

void AssetLoader::cleanup() {
    for (int i = 0; i < MAX_READER_THREADS; ++i) {
        readRequestQueue.push(ReadRequest());
    }
    for (auto &thread : readerThreads) {
        thread.join();
    }
    assert(readRequestQueue.empty());
    readerThreads.clear();
    shaderAssets.clear();
    texture2DAssets.clear();
    textureCubeAssets.clear();
    std::lock_guard<std::mutex> lock(commandListsMutex);
    commandLists.clear();
    device = nullptr;
}

ShaderAssetHandle AssetLoader::getShader(const std::string &path, nvrhi::ShaderType type) {
    std::string realPath("assets/shaders/");
    realPath.append(path);
    return shaderAssets.getOrCreateAsset(realPath, [type] (const std::string &path, unsigned char *buffer, size_t size) {
        return device->createShader(nvrhi::ShaderDesc(type), buffer, size);
    });
}

TextureAssetHandle AssetLoader::getTexture(const std::string &path, nvrhi::TextureDimension dimension) {
    assert(dimension == nvrhi::TextureDimension::Texture2D || dimension == nvrhi::TextureDimension::TextureCube);
    std::string realPath("assets/textures/");
    realPath.append(path);

    auto &assets =
        dimension == nvrhi::TextureDimension::Texture2D ? texture2DAssets : textureCubeAssets;

    return assets.getOrCreateAsset(realPath, [dimension] (const std::string &path, unsigned char *buffer, size_t size) {
        int width, height, comp;
        int ok = stbi_info_from_memory(buffer, size, &width, &height, &comp);
        assert(ok);

        int reqComp = 0;
        nvrhi::Format format;
        switch (comp) {
            case 1: format = nvrhi::Format::R8_UNORM; break;
            case 2: format = nvrhi::Format::RG8_UNORM; break;
            case 3:
            case 4: format = nvrhi::Format::SRGBA8_UNORM; reqComp = 4; break;
            default: assert(false);
        }

        stbi_uc *decodedBuf = stbi_load_from_memory(buffer, size, &width, &height, &comp, reqComp);
        assert(decodedBuf);

        auto textureDesc = nvrhi::TextureDesc()
            .setDimension(dimension)
            .setWidth(width)
            .setHeight(height)
            .setFormat(format)
            .setInitialState(nvrhi::ResourceStates::ShaderResource)
            .setKeepInitialState(true)
            .setDebugName(path);
        if (dimension == nvrhi::TextureDimension::TextureCube) {
            assert(width == height / 6);
            height /= 6;
            textureDesc.setArraySize(6);
            textureDesc.setHeight(height);
        }
        nvrhi::TextureHandle texture = device->createTexture(textureDesc);
        assert(texture);
        logger->debug("Loaded texture %s (%d x %d x %d/%d)", path.c_str(), width, height, comp, reqComp);

        size_t rowPitch = width*std::max(comp, reqComp);
        auto commandList = getOrCreateCommandList();
        if (dimension == nvrhi::TextureDimension::TextureCube) {
            for (int i = 0; i < 6; ++i) {
                commandList->writeTexture(texture, /* arraySlice = */ i, /* mipLevel = */ 0, decodedBuf + i*rowPitch*height, rowPitch);
            }
        } else {
            commandList->writeTexture(texture, /* arraySlice = */ 0, /* mipLevel = */ 0, decodedBuf, rowPitch);
        }
        commandList->setPermanentTextureState(texture, nvrhi::ResourceStates::ShaderResource);
        commandList->commitBarriers();
        finishCommandList(commandList);
        free(decodedBuf);

        return texture;
    });
}
