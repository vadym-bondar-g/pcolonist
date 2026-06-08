#pragma once

#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <utility>

namespace pcolonist {

class ResourceManager {
public:
    template <typename Resource, typename Loader>
    std::shared_ptr<Resource> load(const std::string& id, Loader&& loader) {
        auto& resources = cache<Resource>().resources;
        if (const auto iterator = resources.find(id); iterator != resources.end()) {
            return iterator->second;
        }

        auto resource = std::make_shared<Resource>(std::forward<Loader>(loader)());
        resources.emplace(id, resource);
        return resource;
    }

    template <typename Resource>
    [[nodiscard]] std::shared_ptr<Resource> find(const std::string& id) const {
        const auto* typedCache = findCache<Resource>();
        if (typedCache == nullptr) {
            return {};
        }
        const auto iterator = typedCache->resources.find(id);
        return iterator == typedCache->resources.end() ? std::shared_ptr<Resource>{} : iterator->second;
    }

    template <typename Resource>
    void unload(const std::string& id) {
        if (auto* typedCache = findCache<Resource>()) {
            typedCache->resources.erase(id);
        }
    }

    void clear() {
        caches_.clear();
    }

private:
    struct ICache {
        virtual ~ICache() = default;
    };

    template <typename Resource>
    struct Cache final : ICache {
        std::unordered_map<std::string, std::shared_ptr<Resource>> resources;
    };

    template <typename Resource>
    Cache<Resource>& cache() {
        const std::type_index type = typeid(Resource);
        auto [iterator, inserted] = caches_.try_emplace(type);
        if (inserted) {
            iterator->second = std::make_unique<Cache<Resource>>();
        }
        return *static_cast<Cache<Resource>*>(iterator->second.get());
    }

    template <typename Resource>
    Cache<Resource>* findCache() {
        const auto iterator = caches_.find(typeid(Resource));
        return iterator == caches_.end() ? nullptr : static_cast<Cache<Resource>*>(iterator->second.get());
    }

    template <typename Resource>
    const Cache<Resource>* findCache() const {
        const auto iterator = caches_.find(typeid(Resource));
        return iterator == caches_.end() ? nullptr : static_cast<const Cache<Resource>*>(iterator->second.get());
    }

    std::unordered_map<std::type_index, std::unique_ptr<ICache>> caches_;
};

} // namespace pcolonist
