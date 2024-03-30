#pragma once

#include <nvrhi/nvrhi.h>
#include <mutex>
#include "Logger.h"
#include "JobSystem/JobSystem.h"

class IAsset {
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

public:
    IAsset() : loaded(false) { }
    virtual ~IAsset() { logger->debug("Destroying asset: %s", path.c_str()); }
    virtual unsigned long AddRef() = 0;
    virtual unsigned long Release() = 0;

    const std::string &getPath() const { return path; }
    bool isLoaded() const { return loaded; }
};

template <typename T>
class Asset : public nvrhi::RefCounter<IAsset> {
    template <typename U>
    friend class AssetMap;
    
    T asset;

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
    const T &get() const {
        assert(loaded);
        return asset;
    }
};

typedef nvrhi::RefCountPtr<Asset<nvrhi::ShaderHandle>> ShaderAssetHandle;
typedef nvrhi::RefCountPtr<Asset<nvrhi::TextureHandle>> TextureAssetHandle;

class AssetLoader {
public:
    static void initialize(nvrhi::IDevice *dev);
    static void cleanup();
    static ShaderAssetHandle getShader(const std::string &path, nvrhi::ShaderType type);
    static TextureAssetHandle getTexture(const std::string &path);
};
