#include "systems/buff_hud/tooltip_text.hpp"
#include <Windows.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <array>
#include <iostream>
#include <string>

using namespace BuffPanel::Systems::BuffHud::Internal;

int main() {
    std::array<char, TooltipReserveBytes> reserve{};
    for (std::size_t n = 0; n < 42; ++n) std::memcpy(reserve.data() + n * 3, "\xE2\x80\x8B", 3);
    // Live native buffer: 126 bytes + NUL, followed by an unrelated 0xFF byte.
    std::array<unsigned char, TooltipReserveBytes + 1> live{};
    std::memcpy(live.data(), reserve.data(), reserve.size());
    live.back() = 0xFF;
    assert(MatchesTooltipReserve(live.data(), reserve.data()));
    assert(StoreTooltipText(live.data(), "Bone Armor"));
    assert(std::strcmp(reinterpret_cast<char*>(live.data()), "Bone Armor") == 0);
    assert(live.back() == 0xFF);
    assert(!MatchesTooltipReserve(live.data(), reserve.data()));
    assert(!StoreTooltipText(live.data(), std::string(127, 'x')));
    assert(live.back() == 0xFF);

    // An exact-size allocation touching a guard page catches even a one-byte
    // out-of-bounds comparison or clear, as in the former 128-byte operation.
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    auto* pages = static_cast<char*>(VirtualAlloc(nullptr, info.dwPageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    assert(pages);
    DWORD old{};
    assert(VirtualProtect(pages + info.dwPageSize, info.dwPageSize, PAGE_NOACCESS, &old));
    auto* exact = pages + info.dwPageSize - TooltipReserveBytes;
    assert(StoreTooltipText(exact, std::string_view(reserve.data(), TooltipReserveLength)));
    assert(MatchesTooltipReserve(exact, reserve.data()));
    assert(StoreTooltipText(exact, "Battle Orders"));
    assert(std::strcmp(exact, "Battle Orders") == 0);
    assert(StoreTooltipText(exact, ""));
    assert(exact[0] == 0);
    VirtualFree(pages, 0, MEM_RELEASE);
    std::cout << "PASS: native tooltip sentinel, exact-capacity guard page, restore/write/clear\n";
}
