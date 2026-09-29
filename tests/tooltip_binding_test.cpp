// Exercise the production native binding (qualification, pointer checks,
// encoding, length updates, and exact tooltip allocation) without exporting test APIs.
#include "systems/buff_hud/buff_hud.cpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>

namespace H = BuffPanel::Systems::BuffHud;
namespace {
alignas(8) std::array<std::byte, 0x1000> Widget{};
constexpr std::size_t TextField = 0x88;

void* __fastcall FindPanel(const char*) noexcept { return Widget.data(); }
void* __fastcall FindChild(void*, const char*) noexcept { return Widget.data(); }
void Field(std::size_t offset, std::uint64_t value) { std::memcpy(Widget.data() + offset, &value, sizeof(value)); }
std::uint64_t Length() { std::uint64_t v{}; std::memcpy(&v, Widget.data() + TextField + 8, sizeof(v)); return v; }
void Bind(void* buffer, std::uint64_t length) {
    Widget = {}; H::RenderStates = {};
    Field(TextField, reinterpret_cast<std::uintptr_t>(buffer));
    Field(TextField + 8, length); Field(TextField + 16, length);
}
}

int main() {
    H::FindTopLevelPanel = &FindPanel; H::FindChildWidgetByName = &FindChild;
    SYSTEM_INFO info{}; GetSystemInfo(&info);
    auto* pages = static_cast<char*>(VirtualAlloc(nullptr, info.dwPageSize * 2,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    assert(pages); DWORD old{};
    assert(VirtualProtect(pages + info.dwPageSize, info.dwPageSize, PAGE_NOACCESS, &old));
    auto* exact = pages + info.dwPageSize - H::TooltipReserveBytes;
    for (std::size_t slot = 0; slot < H::SlotCount; ++slot) {
        char reserve[H::TooltipReserveBytes]{}; H::MakeTooltipReserve(slot, reserve);
        std::memcpy(exact, reserve, sizeof(reserve)); Bind(exact, H::TooltipReserveLength);
        assert(H::WriteTooltipText(slot, "")); assert(Length() == 0);
        assert(H::WriteTooltipText(slot, "Bone Armor"));
        assert(Length() == 10 && std::strcmp(exact, "Bone Armor") == 0);
        assert(H::RenderStates[slot].qualifiedTooltipFieldOffset == TextField);
        assert(H::WriteTooltipText(slot, "Battle Command"));
        assert(Length() == 14 && std::strcmp(exact, "Battle Command") == 0);
        H::RestoreQualifiedTooltipBuffer(slot, H::RenderStates[slot]);
        assert(Length() == H::TooltipReserveLength);
        assert(std::memcmp(exact, reserve, sizeof(reserve)) == 0);
    }
    std::array<std::uint16_t, H::TooltipReserveCodepoints + 1> wide{};
    H::MakeTooltipReserveUtf16(1, wide); Bind(wide.data(), H::TooltipReserveCodepoints);
    assert(H::WriteTooltipText(1, "Battle Orders"));
    assert(Length() == 13 && wide[0] == 'B' && wide[13] == 0);
    assert(H::RenderStates[1].qualifiedTooltipEncoding == H::TooltipStorageEncoding::BlizzardUtf16);
    char reserve[H::TooltipReserveBytes]{}; H::MakeTooltipReserve(0, reserve);
    std::memcpy(exact, reserve, sizeof(reserve)); Bind(exact, H::TooltipReserveLength);
    assert(!H::WriteTooltipText(1, "Wrong slot"));
    assert(std::memcmp(exact, reserve, sizeof(reserve)) == 0);
    std::memcpy(Widget.data() + 0x180, Widget.data() + TextField, 24);
    assert(!H::WriteTooltipText(0, "Ambiguous"));
    assert(std::memcmp(exact, reserve, sizeof(reserve)) == 0);
    VirtualFree(pages, 0, MEM_RELEASE);
    std::cout << "PASS: production tooltip binding, exact allocation, native length, reuse, restore and rejection\n";
}
