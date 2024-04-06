#include "AssetLoader.h"
#include "JobSystem.h"
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


template class Asset<Blob>;
template class Asset<Image>;
template class Asset<nvrhi::ShaderHandle>;
template class Asset<nvrhi::TextureHandle>;


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

struct ReadRequest {
    std::string path;
    Ref<BaseAsset> asset;
};

static nvrhi::IDevice *device;
static thread_local nvrhi::CommandListHandle commandList;
static ConcurrentQueue<ReadRequest> readRequestQueue;
static std::vector<std::thread> readerThreads;



template <typename T>
class AssetImpl : public Asset<T> {
    std::string type;
    std::string path;
    std::mutex mutex;
    std::vector<JobScope *> waitingScopes;

public:
    AssetImpl(const char *type, const std::string &path) : type(type), path(path) { }
    ~AssetImpl() { logger->debug("Destroying %s asset: %s", type.c_str(), path.c_str()); }

    const std::string &getPath() const { return path; }

    void addWaitingScope(JobScope *scope) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!this->loaded) {
            scope->addPendingCount(1);
            waitingScopes.push_back(scope);
        }
    }

    void loadIfNotLoaded() override {
        if (this->loaded) {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex);
        if (!this->loaded) {
            logger->debug("Loading %s asset: %s", type.c_str(), path.c_str());
            this->load();
            this->loaded = true;
            for (auto scope : waitingScopes) {
                scope->addPendingCount(-1);
            }
            waitingScopes.clear();
        }
    }

    const T &get() {
        loadIfNotLoaded();
        return this->asset;
    }
};


class BlobAssetImpl : public AssetImpl<Blob> {
public:
    BlobAssetImpl(const std::string &path) : AssetImpl("Blob", path) { }

protected:
    void load() override {
        FILE *fp = fopen(getPath().c_str(), "rb");
        assert(fp);
        fseek(fp, 0, SEEK_END);
        asset.size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        asset.data = (unsigned char *)malloc(asset.size);
        size_t n = fread(asset.data, 1, asset.size, fp);
        assert(n == asset.size);
        fclose(fp);
    }
};


class ImageAssetImpl : public AssetImpl<Image> {
public:
    ImageAssetImpl(const std::string &path) : AssetImpl("Image", path) { }

protected:
    void load() override {
        auto blobAsset = AssetLoader::getBlob(getPath());
        auto &blob = blobAsset->get();

        int width, height, comp;
        int ok = stbi_info_from_memory(blob.data, blob.size, &width, &height, &comp);
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

        asset.data = stbi_load_from_memory(blob.data, blob.size, &width, &height, &comp, reqComp);
        assert(asset.data);
        asset.format = format;
        asset.width = width;
        asset.height = height;
        asset.pitch = width*std::max(comp, reqComp);
    }
};


class ShaderAssetImpl : public AssetImpl<nvrhi::ShaderHandle> {
    nvrhi::ShaderType shaderType;

public:
    ShaderAssetImpl(const std::string &path, nvrhi::ShaderType shaderType) : AssetImpl("Shader", path), shaderType(shaderType) { }

protected:
    void load() override {
        auto blobAsset = AssetLoader::getBlob(getPath());
        auto &blob = blobAsset->get();
        asset = device->createShader(nvrhi::ShaderDesc(shaderType), blob.data, blob.size);
        assert(asset);
    }
};


class TextureAssetImpl : public AssetImpl<nvrhi::TextureHandle> {
    nvrhi::TextureDimension dimension;

    static const char *getDimensionName(nvrhi::TextureDimension dimension) {
        switch (dimension) {
            case nvrhi::TextureDimension::TextureCube: return "TextureCube";
            default: return "Texture2D";
        }
    }

public:
    TextureAssetImpl(const std::string &path, nvrhi::TextureDimension dimension) : AssetImpl(getDimensionName(dimension), path), dimension(dimension) { }

protected:
    void load() override {
        auto imageAsset = AssetLoader::getImage(getPath());
        auto& image = imageAsset->get();
        int height = image.height;

        auto textureDesc = nvrhi::TextureDesc()
            .setDimension(dimension)
            .setWidth(image.width)
            .setHeight(height)
            .setFormat(image.format)
            .setInitialState(nvrhi::ResourceStates::ShaderResource)
            .setKeepInitialState(true)
            .setDebugName(getPath());
        if (dimension == nvrhi::TextureDimension::TextureCube) {
            assert(image.width == height / 6);
            height /= 6;
            textureDesc.setArraySize(6);
            textureDesc.setHeight(height);
        }
        nvrhi::TextureHandle texture = device->createTexture(textureDesc);
        assert(texture);

        commandList->open();
        if (dimension == nvrhi::TextureDimension::TextureCube) {
            for (int i = 0; i < 6; ++i) {
                commandList->writeTexture(texture, /* arraySlice = */ i, /* mipLevel = */ 0, image.data + i*image.pitch*height, image.pitch);
            }
        } else {
            commandList->writeTexture(texture, /* arraySlice = */ 0, /* mipLevel = */ 0, image.data, image.pitch);
        }
        commandList->setPermanentTextureState(texture, nvrhi::ResourceStates::ShaderResource);
        commandList->commitBarriers();
        commandList->close();
        device->executeCommandList(commandList);

        asset = texture;
    }
};





template<typename T>
class AssetMap {
    std::mutex mutex;
    std::unordered_map<std::string, Ref<T>> map;

public:
    template <typename Creator>
    T *getOrCreateAsset(const std::string &path, Creator createAsset) {
        T *asset;
        std::lock_guard<std::mutex> lock(mutex);
        auto it = map.find(path);
        if (it != map.end()) {
            asset = it->second.get();
        } else {
            asset = createAsset(path);
            map.insert({path, asset});

            ReadRequest request;
            request.path = path;
            request.asset = asset;
            readRequestQueue.push(std::move(request));
        }
        asset->addWaitingScope(JobScope::getActiveScope());
        return asset;
    }

    void garbageCollect(bool incremental) {
        std::lock_guard<std::mutex> lock(mutex);
        for (auto &e : map) {
            if (e.second->getRefCount() == 1) {
                map.erase(e.first);
                if (incremental) {
                    break; // one is enough
                }
            }
        }
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex);
        map.clear();
    }
};

static AssetMap<BlobAssetImpl> blobAssets;
static AssetMap<ImageAssetImpl> imageAssets;
static AssetMap<ShaderAssetImpl> shaderAssets;
static AssetMap<TextureAssetImpl> texture2DAssets;
static AssetMap<TextureAssetImpl> textureCubeAssets;


static void readerThreadFunc() {
    commandList = device->createCommandList(nvrhi::CommandListParameters().setEnableImmediateExecution(false));
    for (;;) {
        auto request = readRequestQueue.pop();
        if (request.path.empty()) {
            logger->debug("Stopping reader thread.");
            break;
        }
        request.asset->loadIfNotLoaded();
    }
    commandList = nullptr;
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
    blobAssets.clear();
    imageAssets.clear();
    shaderAssets.clear();
    texture2DAssets.clear();
    textureCubeAssets.clear();
    device = nullptr;
}

void AssetLoader::garbageCollect(bool incremental) {
    blobAssets.garbageCollect(incremental);
    imageAssets.garbageCollect(incremental);
    //shaderAssets.garbageCollect(incremental);
    //texture2DAssets.garbageCollect(incremental);
    //textureCubeAssets.garbageCollect(incremental);
}

BlobAssetHandle AssetLoader::getBlob(const std::string &path) {
    return blobAssets.getOrCreateAsset(path, [] (const std::string &path) {
        return new BlobAssetImpl(path);
    });
}

ImageAssetHandle AssetLoader::getImage(const std::string &path) {
    static std::string shadersPrefix("assets/textures/");
    std::string realPath;
    if (!path.compare(0, shadersPrefix.size(), shadersPrefix)) {
        realPath = path;
    } else {
        realPath = shadersPrefix;
        realPath.append(path);
    }

    return imageAssets.getOrCreateAsset(realPath, [] (const std::string &path) {
        return new ImageAssetImpl(path);
    });
}

ShaderAssetHandle AssetLoader::getShader(const std::string &path, nvrhi::ShaderType type) {
    static std::string shadersPrefix("assets/shaders/");
    std::string realPath;
    if (!path.compare(0, shadersPrefix.size(), shadersPrefix)) {
        realPath = path;
    } else {
        realPath = shadersPrefix;
        realPath.append(path);
    }
    
    return shaderAssets.getOrCreateAsset(realPath, [type] (const std::string &path) {
        return new ShaderAssetImpl(path, type);
    }); 
}

TextureAssetHandle AssetLoader::getTexture(const std::string &path, nvrhi::TextureDimension dimension) {
    static std::string shadersPrefix("assets/textures/");
    std::string realPath;
    if (!path.compare(0, shadersPrefix.size(), shadersPrefix)) {
        realPath = path;
    } else {
        realPath = shadersPrefix;
        realPath.append(path);
    }

    assert(dimension == nvrhi::TextureDimension::Texture2D || dimension == nvrhi::TextureDimension::TextureCube);
    auto &assets = dimension == nvrhi::TextureDimension::Texture2D ? texture2DAssets : textureCubeAssets;

    return assets.getOrCreateAsset(realPath, [dimension] (const std::string &path) {
        return new TextureAssetImpl(path, dimension);
    });
}
