#pragma once

#include <nvrhi/nvrhi.h>
#include <mutex>
#include "Logger.h"
#include "JobSystem.h"
#include "RefCounted.h"

template <typename T>
class Asset : public RefCounted {
    template <typename U>
    friend class AssetMap;
    
    std::string path;
    std::mutex mutex;
    std::atomic<bool> loaded;
    std::vector<JobScope *> waitingScopes;
    T asset;

    void addWaitingScope(JobScope *scope) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!loaded) {
            scope->addPendingCount(1);
            waitingScopes.push_back(scope);
        }
    }

    void setLoadedAsset(T &&asset) {
        std::lock_guard<std::mutex> lock(mutex);
        assert(!loaded);
        this->asset = asset;
        loaded = true;
        for (auto scope : waitingScopes) {
            scope->addPendingCount(-1);
        }
        waitingScopes.clear();
    }

    Asset(const std::string &path) {
        this->path = path;
    }

public:
    ~Asset() { logger->debug("Destroying asset: %s", path.c_str()); }

    const std::string &getPath() const { return path; }

    bool isLoaded() const { return loaded; }

    const T &get() const {
        assert(loaded);
        return asset;
    }
};

typedef Ref<Asset<nvrhi::ShaderHandle>> ShaderAssetHandle;
typedef Ref<Asset<nvrhi::TextureHandle>> TextureAssetHandle;

class AssetLoader {
public:
    static void initialize(nvrhi::IDevice *dev);
    static void cleanup();
    static ShaderAssetHandle getShader(const std::string &path, nvrhi::ShaderType type);
    static TextureAssetHandle getTexture(const std::string &path, nvrhi::TextureDimension dimension = nvrhi::TextureDimension::Texture2D);
};
