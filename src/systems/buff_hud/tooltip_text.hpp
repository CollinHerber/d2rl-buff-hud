#pragma once

#include <cstddef>
#include <cstring>
#include <string_view>

namespace BuffPanel::Systems::BuffHud::Internal {
inline constexpr std::size_t TooltipReserveLength = 126;
// Native capacity excludes ONE terminator. The byte after it is not ours.
inline constexpr std::size_t TooltipReserveBytes = TooltipReserveLength + 1;

[[nodiscard]] inline bool MatchesTooltipReserve(const void* buffer, const char* reserve) noexcept {
    return std::memcmp(buffer, reserve, TooltipReserveBytes) == 0;
}

inline bool StoreTooltipText(void* buffer, std::string_view text) noexcept {
    if (text.size() > TooltipReserveLength) return false;
    std::memset(buffer, 0, TooltipReserveBytes);
    if (!text.empty()) std::memcpy(buffer, text.data(), text.size());
    return true;
}
} // namespace BuffPanel::Systems::BuffHud::Internal
