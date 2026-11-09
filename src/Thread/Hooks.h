#pragma once

namespace Thread::Hooks
{
    void Install();
    void SetWeaponDrawBlocked(bool a_blocked) noexcept;
    bool IsWeaponDrawBlocked() noexcept;
}
