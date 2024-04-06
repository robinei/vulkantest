#pragma once

#include <nvrhi/nvrhi.h>
#include "RefCounted.h"

class BaseAsset : public RefCounted {
protected:
    std::atomic<bool> loaded = false;

public:
    virtual void loadIfNotLoaded() = 0;
    bool isLoaded() const { return loaded; }
};

template <typename T>
class Asset : public BaseAsset {
protected:
    T asset;
    virtual void load() = 0;

public:
    const T &get() {
        loadIfNotLoaded();
        return asset;
    }
};

struct Blob {
    size_t size = 0;
    unsigned char *data = nullptr;

    ~Blob() { free(data); }
    Blob() = default;
    Blob(const Blob &) = delete;
    Blob &operator=(const Blob &) = delete;
};

struct Image {
    nvrhi::Format format = nvrhi::Format::UNKNOWN;
    int width = 0;
    int height = 0;
    int pitch = 0;
    unsigned char *data = nullptr;

    ~Image() { free(data); }
    Image() = default;
    Image(const Image &) = delete;
    Image &operator=(const Image &) = delete;
};

extern template class Asset<Blob>;
extern template class Asset<Image>;
extern template class Asset<nvrhi::ShaderHandle>;
extern template class Asset<nvrhi::TextureHandle>;

typedef Ref<Asset<Blob>> BlobAssetHandle;
typedef Ref<Asset<Image>> ImageAssetHandle;
typedef Ref<Asset<nvrhi::ShaderHandle>> ShaderAssetHandle;
typedef Ref<Asset<nvrhi::TextureHandle>> TextureAssetHandle;

class AssetLoader {
public:
    static void initialize(nvrhi::IDevice *dev);
    static void cleanup();
    static void garbageCollect(bool incremental = false);
    static BlobAssetHandle getBlob(const std::string &path);
    static ImageAssetHandle getImage(const std::string &path);
    static ShaderAssetHandle getShader(const std::string &path, nvrhi::ShaderType type);
    static TextureAssetHandle getTexture(const std::string &path, nvrhi::TextureDimension dimension = nvrhi::TextureDimension::Texture2D);
};
