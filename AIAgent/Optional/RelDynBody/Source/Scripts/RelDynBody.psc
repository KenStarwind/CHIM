Scriptname RelDynBody
{RelDyn body bridge (local CHIM change for the RelDyn extension of HerikaServer; branch reldyn-blush).

The server queues ExtCmdRelDynBody_<Action> in CHIM's command channel (a responselog row whose action is
"command|ExtCmdRelDynBody_<Action>@<parameter>"). The plugin's ExtCmd branch (Plugin/Commands.cpp) takes the text between "ExtCmd"
and the first "_" as a script name and calls <script>.DispatchExternalCommand(npcName, command, parameter) when that script exists, so
this script is all the game side there is: no DLL change.

  ExtCmdRelDynBody_Blush@<seconds>@<token>   put a blush on the NPC through OBlush, take it off after <seconds> real seconds; a newer
                                             token while it is on extends it (the older wait gives way), it does not blush twice
  ExtCmdRelDynBody_BlushOff@<token>          take the blush off now (the server's safeguard when the game never said it ended); does
                                             nothing when the NPC is not blushing or has a newer blush than <token>

Everything that touches OBlush lives in RelDynBodyOBlush, which is only called when OBlush.esp is loaded, so a load order without
OBlush runs this script and does nothing. Any other command is ignored (logged).}

Function DispatchExternalCommand(String npcName, String command, String parameter) global
	if command == "ExtCmdRelDynBody_Blush"
		int sep = StringUtil.Find(parameter, "@")
		float seconds = 30.0
		int token = 0
		if sep >= 0
			seconds = StringUtil.Substring(parameter, 0, sep) as float
			token = StringUtil.Substring(parameter, sep + 1) as int
		else
			seconds = parameter as float
		endif
		if seconds <= 0.0
			seconds = 30.0
		endif
		Actor npc = FindNpc(npcName)
		if !npc
			Debug.Trace("[RelDyn] blush for " + npcName + ": not nearby, dropped")
			return
		endif
		if !Game.IsPluginInstalled("OBlush.esp")
			Debug.Trace("[RelDyn] blush for " + npcName + ": OBlush.esp is not loaded, dropped")
			return
		endif
		RelDynBodyOBlush.Apply(npc, seconds, token)
	elseif command == "ExtCmdRelDynBody_BlushOff"
		Actor npc2 = FindNpc(npcName)
		if !npc2
			return
		endif
		if !Game.IsPluginInstalled("OBlush.esp")
			return
		endif
		RelDynBodyOBlush.ClearIfNotNewer(npc2, parameter as int)
	else
		Debug.Trace("[RelDyn] unknown body command " + command)
	endif
EndFunction

; The NPC by the name the server knows it by (its display name), among the agents CHIM manages near the player.
Actor Function FindNpc(String npcName) global
	Actor[] agents = AIAgentFunctions.findAllNearbyAgents()
	if !agents
		return None
	endif
	int i = 0
	while i < agents.Length
		if agents[i] && agents[i].GetDisplayName() == npcName
			return agents[i]
		endif
		i += 1
	endWhile
	return None
EndFunction
