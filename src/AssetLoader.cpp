#include "AssetLoader.h"
#include "Logger.h"
#include "JobSystem/JobSystem.h"

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
    std::unordered_map<std::string, nvrhi::RefCountPtr<Asset<T>>> map;

public:
    template <typename Handler>
    nvrhi::RefCountPtr<Asset<T>> getOrCreateAsset(const std::string &path, Handler &&handler) {
        nvrhi::RefCountPtr<Asset<T>> asset;
        std::lock_guard<std::mutex> lock(mutex);
        auto it = map.find(path);
        if (it != map.end()) {
            asset = it->second.Get();
        } else {
            asset = new Asset<T>(path);
            map.insert({path, asset});
            asset->Release(); // ref count starts at 1, so Release after we are storing a reference
            ReadRequest request;
            request.path = path;
            request.handler = [asset, handler = std::move(handler)] (unsigned char *buffer, size_t size) {
                Job::enqueue([asset, buffer, size, handler = std::move(handler)] {
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
static AssetMap<nvrhi::TextureHandle> textureAssets;


static nvrhi::CommandListHandle getOrCreateCommandList() {
    nvrhi::CommandListHandle commandList;
    {
        std::lock_guard<std::mutex> lock(commandListsMutex);
        if (!commandLists.empty()) {
            commandList = commandLists.back();
            commandLists.pop_back();
        } else {
            commandList = device->createCommandList();
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
    textureAssets.clear();
    std::lock_guard<std::mutex> lock(commandListsMutex);
    commandLists.clear();
    device = nullptr;
}

ShaderAssetHandle AssetLoader::getShader(const std::string &path, nvrhi::ShaderType type) {
    return shaderAssets.getOrCreateAsset(path, [type] (const std::string &path, unsigned char *buffer, size_t size) {
        return device->createShader(nvrhi::ShaderDesc(type), buffer, size);
    });
}

TextureAssetHandle AssetLoader::getTexture(const std::string &path) {
    return textureAssets.getOrCreateAsset(path, [] (const std::string &path, unsigned char *buffer, size_t size) {
        int width, height, comp;
        int ok = stbi_info_from_memory(buffer, size, &width, &height, &comp);
        assert(ok);

        int req_comp = 0;
        nvrhi::Format format;
        switch (comp) {
            case 1: format = nvrhi::Format::R8_UNORM; break;
            case 2: format = nvrhi::Format::RG8_UNORM; break;
            case 3:
            case 4: format = nvrhi::Format::SRGBA8_UNORM; req_comp = 4; break;
            default: assert(false);
        }

        stbi_uc *decodedBuf = stbi_load_from_memory(buffer, size, &width, &height, &comp, req_comp);
        assert(decodedBuf);

        auto textureDesc = nvrhi::TextureDesc()
            .setDimension(nvrhi::TextureDimension::Texture2D)
            .setWidth(width)
            .setHeight(height)
            .setFormat(format)
            .setInitialState(nvrhi::ResourceStates::ShaderResource)
            .setKeepInitialState(true)
            .setDebugName(path);
        nvrhi::TextureHandle texture = device->createTexture(textureDesc);
        assert(texture);
        logger->debug("Loaded texture %s (%d x %d x %d/%d)", path.c_str(), width, height, comp, req_comp);

        auto commandList = getOrCreateCommandList();
        commandList->writeTexture(texture, /* arraySlice = */ 0, /* mipLevel = */ 0, decodedBuf, width*std::max(comp, req_comp));
        finishCommandList(commandList);
        free(decodedBuf);

        return texture;
    });
}
