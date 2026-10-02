Scriptname RelDynBodyOBlush
{The part of the RelDyn body bridge that touches OBlush (Nexus 145083). Kept apart from RelDynBody so a load order without OBlush never
loads a script that names its types: RelDynBody calls here only after Game.IsPluginInstalled("OBlush.esp").

OBlush blushes only inside OStim scenes on its own. Its OBlushHandle quest script (OBlushQuest, form 0x801 of OBlush.esp) exposes the two
functions the scene loop uses, enableBlush(Actor) -> the face overlay node it took, and disableBlush(Actor, node), over NiOverride. This
calls them for a blush the server asked for, and remembers the node and the newest token per NPC in StorageUtil (saved with the game, so
the server's BlushOff can still find a blush whose wait a reloaded save lost).

Master switch: OBlush's own OBEnable (its MCM). Its male / female toggles are for its OStim scenes and are not read: RelDyn is for every
character.}

OBlushHandle Function Handle() global
	return Game.GetFormFromFile(0x00000801, "OBlush.esp") as OBlushHandle
EndFunction

Function Apply(Actor akActor, float seconds, int token) global
	OBlushHandle h = Handle()
	if !h || !akActor
		Debug.Trace("[RelDyn] blush: OBlush's quest script is not there")
		return
	endif
	if h.OBEnable && h.OBEnable.GetValue() != 1.0
		return
	endif
	if akActor.IsDead() || !akActor.Is3DLoaded()
		return
	endif
	StorageUtil.SetIntValue(akActor, "RelDynBlush_Token", token)
	string node = StorageUtil.GetStringValue(akActor, "RelDynBlush_Node", "")
	if node == ""
		node = h.enableBlush(akActor)
		if node == ""
			Debug.Trace("[RelDyn] blush: no free face overlay slot on " + akActor.GetDisplayName())
			return
		endif
		StorageUtil.SetStringValue(akActor, "RelDynBlush_Node", node)
	endif
	Utility.Wait(seconds)
	; a newer blush (an extension) took the token over while this waited: it clears
	if StorageUtil.GetIntValue(akActor, "RelDynBlush_Token", 0) == token
		Clear(akActor)
	endif
EndFunction

Function Clear(Actor akActor) global
	OBlushHandle h = Handle()
	if !h || !akActor
		return
	endif
	string node = StorageUtil.GetStringValue(akActor, "RelDynBlush_Node", "")
	if node == ""
		return
	endif
	StorageUtil.UnsetStringValue(akActor, "RelDynBlush_Node")
	StorageUtil.UnsetIntValue(akActor, "RelDynBlush_Token")
	h.disableBlush(akActor, node)
EndFunction

Function ClearIfNotNewer(Actor akActor, int token) global
	if StorageUtil.GetIntValue(akActor, "RelDynBlush_Token", 0) > token
		return
	endif
	Clear(akActor)
EndFunction
