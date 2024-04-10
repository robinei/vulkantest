#pragma once

#include <nvrhi/nvrhi.h>
#include "RefCounted.h"
#include <coroutine>

class Coroutine {
public:
    struct Promise {
        Coroutine get_return_object() { return Coroutine {}; }
        void unhandled_exception() noexcept { }
        void return_void() noexcept { }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
    };
    using promise_type = Promise;
};

template <typename T>
class Asset : public RefCounted {
protected:
    std::atomic<bool> loaded = false;
    T asset;

    virtual void addAwaiter(std::coroutine_handle<> handle) noexcept = 0;

public:
    bool isLoaded() const noexcept { return loaded; }

    const T &get() const noexcept {
        assert(loaded);
        return asset;
    }

    auto operator co_await() {
        struct Awaiter {
            Asset *asset;
            bool await_ready() const noexcept { return asset->loaded; }
            void await_suspend(std::coroutine_handle<> handle) noexcept { asset->addAwaiter(handle); }
            const T &await_resume() const noexcept { return asset->get(); }
        };
        return Awaiter { this };
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
