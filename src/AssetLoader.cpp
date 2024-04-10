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

#define MAX_IO_THREADS 2


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

struct IOAction {
    bool stopThread;
    std::function<void ()> perform;
};

static nvrhi::IDevice *device;

static std::atomic<int> pendingLoads;

static ConcurrentQueue<IOAction> ioActionQueue;
static std::vector<std::thread> ioThreads;



template <typename T>
class AssetImpl : public Asset<T> {
protected:
    std::string type;
    std::string path;
    std::mutex mutex;
    std::vector<std::coroutine_handle<>> awaiters;

    void addAwaiter(std::coroutine_handle<> handle) noexcept override {
        std::lock_guard<std::mutex> lock(mutex);
        if (this->loaded) {
            Job::enqueueOnWorker([handle, thisRef = Ref(this)] () {
                handle.resume();
            });
        } else {
            awaiters.push_back(handle);
        }
    }

public:
    AssetImpl(const char *type, const std::string &path) : type(type), path(path) {
        //logger->debug("Creating %s asset: %s", type, path.c_str());
    }
    ~AssetImpl() {
        //logger->debug("Destroying %s asset: %s", type.c_str(), path.c_str());
    }

    void loadingFinished() {
        std::lock_guard<std::mutex> lock(mutex);
        this->loaded = true;
        for (auto handle : awaiters) {
            Job::enqueueOnWorker([handle, thisRef = Ref(this)] () {
                handle.resume();
            });
        }
        awaiters.clear();
        --pendingLoads;
    }
};


class BlobAssetImpl : public AssetImpl<Blob> {
public:
    BlobAssetImpl(const std::string &path) : AssetImpl("Blob", path) { }

    void load() {
        ioActionQueue.push(IOAction { false,
            [thisRef = Ref(this)] () mutable {
                FILE *fp = fopen(thisRef->path.c_str(), "rb");
                assert(fp);
                fseek(fp, 0, SEEK_END);
                size_t size = ftell(fp);
                fseek(fp, 0, SEEK_SET);
                unsigned char *data = (unsigned char *)malloc(size);
                size_t n = fread(data, 1, size, fp);
                assert(n == size);
                fclose(fp);

                thisRef->asset.data = data;
                thisRef->asset.size = size;
                thisRef->loadingFinished();
            }
        });
    }
};


class ImageAssetImpl : public AssetImpl<Image> {
public:
    ImageAssetImpl(const std::string &path) : AssetImpl("Image", path) { }

    Coroutine load() {
        auto thisRef = Ref(this);
        auto blobAsset = AssetLoader::getBlob(path);
        auto &blob = co_await *blobAsset;

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
        loadingFinished();
    }
};


class ShaderAssetImpl : public AssetImpl<nvrhi::ShaderHandle> {
    nvrhi::ShaderType shaderType;

public:
    ShaderAssetImpl(const std::string &path, nvrhi::ShaderType shaderType) : AssetImpl("Shader", path), shaderType(shaderType) { }

    Coroutine load() {
        auto thisRef = Ref(this);
        auto blobAsset = AssetLoader::getBlob(path);
        auto &blob = co_await *blobAsset;
        asset = device->createShader(nvrhi::ShaderDesc(shaderType), blob.data, blob.size);
        assert(asset);
        loadingFinished();
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

    Coroutine load() {
        auto thisRef = Ref(this);
        auto imageAsset = AssetLoader::getImage(path);
        auto &image = co_await *imageAsset;
        int height = image.height;

        auto textureDesc = nvrhi::TextureDesc()
            .setDimension(dimension)
            .setWidth(image.width)
            .setHeight(height)
            .setFormat(image.format)
            .setInitialState(nvrhi::ResourceStates::ShaderResource)
            .setKeepInitialState(true)
            .setDebugName(path);
        if (dimension == nvrhi::TextureDimension::TextureCube) {
            assert(image.width == height / 6);
            height /= 6;
            textureDesc.setArraySize(6);
            textureDesc.setHeight(height);
        }
        asset = device->createTexture(textureDesc);
        assert(asset);

        auto commandList = device->createCommandList(nvrhi::CommandListParameters().setEnableImmediateExecution(false));
        commandList->open();
        if (dimension == nvrhi::TextureDimension::TextureCube) {
            for (int i = 0; i < 6; ++i) {
                commandList->writeTexture(asset, /* arraySlice = */ i, /* mipLevel = */ 0, image.data + i*image.pitch*height, image.pitch);
            }
        } else {
            commandList->writeTexture(asset, /* arraySlice = */ 0, /* mipLevel = */ 0, image.data, image.pitch);
        }
        commandList->setPermanentTextureState(asset, nvrhi::ResourceStates::ShaderResource);
        commandList->commitBarriers();
        commandList->close();

        Job::enqueueOnMain([thisRef = Ref(this), commandList] () mutable {
            device->executeCommandList(commandList);
            thisRef->loadingFinished();
        });
    }
};





template<typename T>
class AssetMap {
    std::mutex mutex;
    std::unordered_map<std::string, Ref<T>> map;

public:
    template <typename Creator>
    Ref<T> getOrCreateAsset(const std::string &path, Creator createAsset) {
        Ref<T> asset;
        std::lock_guard<std::mutex> lock(mutex);
        auto it = map.find(path);
        if (it != map.end()) {
            asset = it->second.get();
        } else {
            asset = createAsset(path);
            map.insert({path, asset});
            ++pendingLoads;
            Job::enqueueOnWorker([asset] () mutable {
                asset->load();
            });
        }
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


void AssetLoader::initialize(nvrhi::IDevice *dev) {
    device = dev;
    for (int i = 0; i < MAX_IO_THREADS; ++i) {
        ioThreads.emplace_back([] {
            for (;;) {
                auto action = ioActionQueue.pop();
                if (action.stopThread) {
                    logger->debug("Stopping IO thread.");
                    break;
                }
                action.perform();
            }
        });
    }
}

void AssetLoader::cleanup() {
    while (pendingLoads > 0) {
        JobSystem::dispatch();
    }
    for (int i = 0; i < MAX_IO_THREADS; ++i) {
        ioActionQueue.push(IOAction { true });
    }
    for (auto &thread : ioThreads) {
        thread.join();
    }
    assert(ioActionQueue.empty());
    ioThreads.clear();
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
    auto asset = blobAssets.getOrCreateAsset(path, [] (const std::string &path) {
        return new BlobAssetImpl(path);
    });
    return asset.get();
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

    auto asset = imageAssets.getOrCreateAsset(realPath, [] (const std::string &path) {
        return new ImageAssetImpl(path);
    });
    return asset.get();
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
    
    auto asset = shaderAssets.getOrCreateAsset(realPath, [type] (const std::string &path) {
        return new ShaderAssetImpl(path, type);
    });
    return asset.get();
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

    auto asset = assets.getOrCreateAsset(realPath, [dimension] (const std::string &path) {
        return new TextureAssetImpl(path, dimension);
    });
    return asset.get();
}
