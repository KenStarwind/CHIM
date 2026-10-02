ScriptName NiOverride Hidden

; Trimmed stub for compile-time signature resolution only (RaceMenu/SKEE64 provides the
; real implementation at runtime via SKSE plugin). Confirmed against the decompiled
; nioverride.pex shipped in RaceMenu.bsa.

Float Function GetBodyMorph(ObjectReference ref, string morphName, string keyName) Global Native
Function SetBodyMorph(ObjectReference ref, string morphName, string keyName, float value) Global Native
Function ClearBodyMorph(ObjectReference ref, string morphName, string keyName) Global Native
Function UpdateModelWeight(ObjectReference ref) Global Native
; 2026-08-02: experimental additions for face-refresh investigation
Function ApplyOverrides(ObjectReference ref) Global Native
Function ApplySkinOverrides(ObjectReference ref) Global Native
Function ApplyNodeOverrides(ObjectReference ref) Global Native
string[] Function GetCachedMorphNames() Global Native

; --- added for this compile only (real SKEE64 signatures; OBlushHandle.psc and OStim's OUtils.psc call these) ---
bool Function HasNodeTransformPosition(ObjectReference ref, bool firstPerson, bool isFemale, string node, string key) Global Native
float[] Function GetNodeTransformPosition(ObjectReference ref, bool firstPerson, bool isFemale, string node, string key) Global Native
Function AddNodeTransformPosition(ObjectReference ref, bool firstPerson, bool isFemale, string node, string key, float[] position) Global Native
Function UpdateNodeTransform(ObjectReference ref, bool firstPerson, bool isFemale, string node) Global Native
int Function GetNumBodyOverlays() Global Native
string Function GetNodeOverrideString(ObjectReference ref, bool isFemale, string node, int key, int index) Global Native
Function AddNodeOverrideFloat(ObjectReference ref, bool isFemale, string node, int key, int index, float value, bool persist) Global Native
Function AddNodeOverrideString(ObjectReference ref, bool isFemale, string node, int key, int index, string value, bool persist) Global Native
Function AddNodeOverrideInt(ObjectReference ref, bool isFemale, string node, int key, int index, int value, bool persist) Global Native
Function AddOverlays(ObjectReference ref) Global Native
Function RemoveNodeOverride(ObjectReference ref, bool isFemale, string node, int key, int index) Global Native
