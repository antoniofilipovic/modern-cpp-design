

#include <cstdint>
#include <string>
#include <unordered_map>

constexpr std::string_view PREFIX{"PTR"};


class PointerTracker {
public:
    PointerTracker() = default;

    std::string_view getPtrHumaneReadableName(void *chunk) {
        if (chunk == nullptr) {
            return "EMPTY";
        }

        const auto ptr = reinterpret_cast<uintptr_t>(chunk);
        const auto iter = name_map_.find(ptr);
        assert(iter != name_map_.end());
        return iter->second;
    }


    std::string createPtrHumaneReadableName(const std::size_t id, void *chunk) {
        const auto ptr = reinterpret_cast<uintptr_t>(chunk);
        auto iter = name_map_.find(ptr);
        if(iter != name_map_.end()) {
            return iter->second;
        }


        auto [insert_iter, ok ] = name_map_.emplace(ptr,
            std::format("{}_{}_{}", PREFIX, id_map_[id], name_counter_++));
        return insert_iter->second;
    }

    void updateNameMap(std::size_t id, std::string_view name) {
        id_map_.emplace(id, name);
    }


private:
    std::unordered_map<uintptr_t, std::string> name_map_{};
    std::unordered_map<std::size_t, std::string> id_map_{};
    std::size_t name_counter_{0};


};
