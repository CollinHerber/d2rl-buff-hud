#include "core/services.hpp"
#include "systems/buff_hud/skill_icon_resolver.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <array>
#include <cstdio>
#include <cstring>
#include <iostream>

namespace {
namespace D = D2RL::DataTables;
namespace H = BuffPanel::Systems::BuffHud::Internal;
std::array<std::array<unsigned char, 748>, 525> Skills{};
std::array<std::array<unsigned char, 300>, 295> Descriptions{};
D2RL::PluginContext Context{};
D2RL::DataTableService Tables{};
D2RL::LocalizationService Localization{};
BuffPanel::Core::ServiceRegistry Registry{};

void Word(auto& row, std::size_t offset, unsigned short value) {
    std::memcpy(row.data() + offset, &value, sizeof(value));
}

D::Result __cdecl GetTable(const D2RL::PluginContext*, D::Bank, D::TableId id, D::TableView* out) noexcept {
    if (id == D::TableId::Skills) {
        out->rows = Skills.data(); out->rowSize = 748; out->rowCount = 525;
    } else if (id == D::TableId::SkillDesc) {
        out->rows = Descriptions.data(); out->rowSize = 300; out->rowCount = 295;
    } else return D::Result::NotFound;
    return D::Result::Success;
}

D::Result __cdecl FindSkill(const D2RL::PluginContext*, D::Bank, D::TableId, unsigned id, D::RowView* out) noexcept {
    if (id >= Skills.size()) return D::Result::NotFound;
    out->row = Skills[id].data(); out->rowSize = 748; out->rowIndex = id;
    return D::Result::Success;
}

D2RL::Localization::Result __cdecl GetString(const D2RL::PluginContext*, unsigned id,
    char* out, unsigned capacity, unsigned* required) noexcept {
    if (id < 1000 || id >= 1525) return D2RL::Localization::Result::NotFound;
    *required = static_cast<unsigned>(std::snprintf(out, capacity, "Skill %u", id - 1000)) + 1;
    return D2RL::Localization::Result::Success;
}

void Fixture(bool reordered) {
    Skills = {}; Descriptions = {};
    // The old Amazon-only probe sees TWO valid links: id at +0 and
    // descriptor at +0x23c. The latter diverges after the monster-skill gap.
    const unsigned ids[]{6,7,8,9,10,11,36,66,68,96,126,221,235,251,387,149,155,510};
    const unsigned descs[]{6,7,8,9,10,11,36,66,68,96,126,160,174,190,247,149,155,294};
    const unsigned char classes[]{0,0,0,0,0,0,1,2,2,3,4,5,5,6,7,4,4,2};
    const unsigned char icons[]{0,2,4,6,8,10,0,0,4,0,0,0,40,0,22,46,58,12};
    for (std::size_t i = 0; i < std::size(ids); ++i) {
        auto desc = reordered ? (descs[i] + 23) % Descriptions.size() : descs[i];
        Word(Skills[ids[i]], 0, static_cast<unsigned short>(ids[i]));
        Word(Skills[ids[i]], 0x23c, static_cast<unsigned short>(desc));
        Skills[ids[i]][0x0c] = classes[i];
        Descriptions[desc][6] = icons[i];
        Word(Descriptions[desc], 8, static_cast<unsigned short>(1000 + ids[i]));
    }
}

void CheckIcons() {
    assert(H::RebuildSkillIconCache(42));
    const auto status = H::SkillIconStatus();
    assert(status.ready && status.namesReady && status.linkCandidateCount == 1);
    assert(status.skillDescLinkOffset == 0x23c && status.iconCelOffset == 6);
    H::SkillIconDescriptor icon{};
    assert(H::TryResolveSkillIcon(68, icon));
    assert(icon.atlas == BuffPanel::Core::BuffIconAtlas::Necromancer && icon.frame == 4);
    assert(H::TryResolveSkillIcon(235, icon));
    assert(icon.atlas == BuffPanel::Core::BuffIconAtlas::Druid && icon.frame == 40);
    assert(H::TryResolveSkillIcon(387, icon));
    assert(icon.atlas == BuffPanel::Core::BuffIconAtlas::Warlock && icon.frame == 22);
    assert(H::TryResolveSkillIcon(149, icon) && icon.atlas == BuffPanel::Core::BuffIconAtlas::Barbarian && icon.frame == 46);
    assert(H::TryResolveSkillIcon(155, icon) && icon.atlas == BuffPanel::Core::BuffIconAtlas::Barbarian && icon.frame == 58);
    assert(H::TryResolveSkillIcon(510, icon) && icon.atlas == BuffPanel::Core::BuffIconAtlas::Necromancer && icon.frame == 12);
    char name[80]{};
    assert(H::TryResolveSkillName(510, name, sizeof(name)) && std::strcmp(name, "Skill 510") == 0);
    assert(H::TryResolveSkillName(68, name, sizeof(name)) && std::strcmp(name, "Skill 68") == 0);
}
}

namespace BuffPanel::Core {
const ServiceRegistry& Services() noexcept { return Registry; }
const D2RL::DataTableService* DataTableService() noexcept { return &Tables; }
const D2RL::LocalizationService* LocalizationService() noexcept { return &Localization; }
}

int main() {
    Tables.getTable = GetTable; Tables.findRowById = FindSkill;
    Localization.getStringById = GetString;
    Registry.context = &Context; Registry.dataTables = &Tables; Registry.localization = &Localization;
    Fixture(false); CheckIcons();
    Fixture(true); CheckIcons();
    // Truly ambiguous layouts must still fail closed, never choose the first.
    for (auto& row : Skills) std::memcpy(row.data() + 0x240, row.data() + 0x23c, 2);
    assert(!H::RebuildSkillIconCache(43));
    assert(H::SkillIconStatus().linkCandidateCount == 2);
    H::SkillIconDescriptor icon{};
    assert(!H::TryResolveSkillIcon(68, icon));
    // A changed witness must also reject the layout and drop the previous cache.
    Fixture(false); Descriptions[174][6] = 42;
    assert(!H::RebuildSkillIconCache(44));
    assert(!H::TryResolveSkillIcon(68, icon));
    std::cout << "PASS: skill-id collision, reordered descriptors, buff icons/names, ambiguous and unsupported layouts\n";
}
