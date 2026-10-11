ScriptName SLPPatches Hidden
{RustyOldSword patches for SexLab P+ 2.19 - natives in SexLabUtil.dll.
The feature switches are ordinary SexLab settings: sslSystemConfig.GetSettingBool/SetSettingBool("bPatchPetite"),
"bPatchHeels", "bPatchClothing", "bPatchDevices", "bPatchEnjoyment" (saved in SKSE\SexLab\UserData\Settings.yaml).}

; One line per known mod: "Name|on/off|Detail"
string[] Function GetIntegrations() global native
string Function GetPatchVersion() global native
