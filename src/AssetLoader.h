#pragma once

#include <nvrhi/nvrhi.h>
#include <mutex>
#include <utility>
#include <functional>
#include "Logger.h"
#include "JobSystem.h"
#include "RefCounted.h"

class BaseAsset : public RefCounted {
    template <typename U>
    friend class AssetMap;

    void addWaitingScope(JobScope *scope) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!loaded) {
            scope->addPendingCount(1);
            waitingScopes.push_back(scope);
        }
    }

protected:
    std::string path;
    std::mutex mutex;
    std::atomic<bool> loaded;
    std::vector<JobScope *> waitingScopes;

    BaseAsset(const std::string &path) : path(path), loaded(false) { }

public:
    ~BaseAsset() { logger->debug("Destroying asset: %s", path.c_str()); }

    const std::string &getPath() const { return path; }

    bool isLoaded() const { return loaded; }

    virtual void loadIfNotLoaded() = 0;
};

template <typename T>
class Asset : public BaseAsset {
    template <typename U>
    friend class AssetMap;
    
    std::function<T (const std::string &)> loader;
    T asset;

public:
    Asset(const std::string &path, std::function<T (const std::string &)> &&loader) : BaseAsset(path), loader(loader) { }

    void loadIfNotLoaded() override {
        if (loaded) {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex);
        if (!loaded) {
            asset = std::move(loader(path));
            loaded = true;
            for (auto scope : waitingScopes) {
                scope->addPendingCount(-1);
            }
            waitingScopes.clear();
        }
    }

    const T &get() {
        loadIfNotLoaded();
        return asset;
    }
};

typedef Ref<BaseAsset> AssetHandle;

typedef std::vector<unsigned char> Blob;
typedef Asset<Blob> BlobAsset;
typedef Ref<BlobAsset> BlobAssetHandle;

typedef Asset<nvrhi::ShaderHandle> ShaderAsset;
typedef Ref<ShaderAsset> ShaderAssetHandle;

typedef Asset<nvrhi::TextureHandle> TextureAsset;
typedef Ref<TextureAsset> TextureAssetHandle;

class AssetLoader {
public:
    static void initialize(nvrhi::IDevice *dev);
    static void cleanup();
    static BlobAssetHandle getBlob(const std::string &path);
    static ShaderAssetHandle getShader(const std::string &path, nvrhi::ShaderType type);
    static TextureAssetHandle getTexture(const std::string &path, nvrhi::TextureDimension dimension = nvrhi::TextureDimension::Texture2D);
};
