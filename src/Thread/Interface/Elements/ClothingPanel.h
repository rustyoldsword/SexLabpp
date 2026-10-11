#pragma once
#include "Registry/Util/Wardrobe.h"
#include "Thread/Interface/SceneHUD.h"

namespace Thread::Interface
{
    // Per-group clothing rules for the actors of the running scene (see Registry::Wardrobe)
    class ClothingPanel final
    {
      public:
        void Open(SceneHUD& a_hud);
        void Close();
        void Render(SceneHUD& a_hud);

      private:
        void OnSetRule(SceneHUD& a_hud, RE::Actor* a_actor, Registry::WardrobeGroup a_group, const Registry::GroupRule& a_rule);
        void OnUndressAll(RE::Actor* a_actor);
        void OnDressAll(RE::Actor* a_actor);
        void RenderGroupRow(SceneHUD& a_hud, RE::Actor* a_actor, Registry::WardrobeGroup a_group);

        size_t _actorIndex{ 0 };
        Registry::Scope _scope{ Registry::Scope::Scene };
        int _row{ 0 };  // zebra rows of the group table
    };
}
