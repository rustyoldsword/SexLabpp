#pragma once

// The custom strip settings file (Stripping.yaml) as a YAML tree: { "<plugin file>": { <local form id>: <strip> } }.
// Plain functions on the tree so the file rules can be tested without the game.
namespace UserData::StripYaml
{
	inline void Write(YAML::Node& a_root, const std::string& a_file, uint32_t a_id, int32_t a_strip)
	{
		a_root[a_file][a_id] = a_strip;
	}

	// Forget one item, so the next save no longer writes it back
	inline void Erase(YAML::Node& a_root, const std::string& a_file, uint32_t a_id)
	{
		const YAML::Node& root = a_root;  // const lookups never add a missing key
		if (!root.IsMap() || !root[a_file] || !root[a_file].IsMap()) {
			return;
		}
		YAML::Node items = a_root[a_file];
		items.remove(a_id);
		if (items.size() == 0) {
			a_root.remove(a_file);
		}
	}

	// Forget every item of the plugins a_isLoaded accepts; plugins that are not loaded keep their entries
	template <class Pred>
	inline void EraseLoaded(YAML::Node& a_root, Pred a_isLoaded)
	{
		if (!a_root.IsMap()) {
			return;
		}
		std::vector<std::string> files;
		for (const auto& entry : a_root) {
			const auto file = entry.first.as<std::string>();
			if (a_isLoaded(file)) {
				files.push_back(file);
			}
		}
		for (const auto& file : files) {
			a_root.remove(file);
		}
	}
}  // namespace UserData::StripYaml
