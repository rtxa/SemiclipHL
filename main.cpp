#include "main.h"
#include "mem_parse.h"

static edict_t *maxEdict;
static edict_t *nullEdict;
static edict_t *startEdict;

bool g_bNotActive = false;
char **global_host_client;
const char *g_szTeamList;

patch_t patchData[] =
{
	{ (void *)NULL, {}, 0 },
	{ (void *)NULL, {}, 0 },
	{ (void *)NULL, {}, 0 }
};

semiclipData_t semiclipData;
semiclip_t g_pSemiclip[MAX_CLIENTS + 1];

void OnMetaDetach()
{
	for (int j = 0; j < 3; j++)
	{
		if (patchData[j].addr)
			mem_memcpy(patchData[j].addr, patchData[j].bytes, patchData[j].nSize);
	}
}
int OnMetaAttach()
{
	load_config();

	if (load_parse())
	{
		REG_SVR_COMMAND("semiclip_option", SVR_SemiclipOption);
		return 1;
	}
	return 0;
}

// m_szTeamName 16
// m_szTeamList 512
int GetTeamId(edict_t *pEntity)
{
	char *infobuffer = (*g_engfuncs.pfnGetInfoKeyBuffer)(pEntity);

	char model[16];

	strcpy(model, (g_engfuncs.pfnInfoKeyValue(infobuffer, "model")));

	const char *pos = strstr(g_szTeamList, model);
	char *sofar = (char*)g_szTeamList;

	int team = 1;

	if (sofar == NULL)
		team = -1;
	else
	{
		// count ";"s
		while (sofar < pos)
		{
			if (*sofar == ';')
				team++;

			sofar = sofar + 1;
		}
	}

	return team;
}

static uint16_t FixedUnsigned16(float fValue, float fScale)
{
	int output = (int)(fValue * fScale);

	if (output < 0)
		output = 0;

	if (output > 0xFFFF)
		output = 0xFFFF;

	return (uint16_t)output;
}
void UTIL_ScreenFade(edict_t *pEdict, float flFadeTime, float flFadeHold, int iAlpha)
{
	static int gmsigScreenFade = 0;
	if (gmsigScreenFade || (gmsigScreenFade = REG_USER_MSG("ScreenFade", -1)))
	{
		gpEnginefuncInterface->pfnMessageBegin(MSG_ONE, gmsigScreenFade, NULL, pEdict);
		gpEnginefuncInterface->pfnWriteShort(FixedUnsigned16(flFadeTime, 1 << 12));
		gpEnginefuncInterface->pfnWriteShort(FixedUnsigned16(flFadeHold, 1 << 12));
		gpEnginefuncInterface->pfnWriteShort(0);
		gpEnginefuncInterface->pfnWriteByte(255);
		gpEnginefuncInterface->pfnWriteByte(255);
		gpEnginefuncInterface->pfnWriteByte(255);
		gpEnginefuncInterface->pfnWriteByte(iAlpha);
		gpEnginefuncInterface->pfnMessageEnd();
	}
}
void RadiusFlash_Handler(Vector vecStart, entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage)
{
	void *pData;

	int iAlpha;
	int m_iFlashAlpha;

	float flFadeTime;
	float flFadeHold;
	float flRadius = 1500;
	float flAdjustedDamage;
	float flCurrentHoldTime;
	float flFallDmg = flDamage / flRadius;
	float flCurrentTime = gpGlobals->time;

	float m_flFlashedAt;
	float m_flFlashHoldTime;
	float m_flFlashDuration;

	Vector vecEnd;
	Vector vecPlane;
	Vector vecTemp;

	TraceResult tr;

	edict_t *pHit;
	edict_t *pEdict;
	edict_t *pObserver;
	edict_t *pAttacker = (pevAttacker != NULL) ? pevAttacker->pContainingEntity : NULL;
	edict_t *pFlashbang = pevInflictor->pContainingEntity;

	static int iMaxClients = gpGlobals->maxClients;

	bool bInWater = (POINT_CONTENTS(vecStart) == CONTENTS_WATER);
	bool bIsValid = (semiclipData.noteamflash && pAttacker);

	vecStart.z += 1.0f;
	vecTemp = vecStart;

	for (int j = 1; j <= iMaxClients; j++)
	{
		pEdict = EDICT_NUM(j);

		if (!pEdict->pvPrivateData
			|| pEdict->v.deadflag != DEAD_NO
			/*|| pEdict->v.flags & FL_FAKECLIENT*/
			|| (bInWater ? pEdict->v.waterlevel == 0 : pEdict->v.waterlevel == 3)
			|| (bIsValid && pEdict != pAttacker && GetTeamId(pEdict) == GetTeamId(pAttacker))
			)
		{
			continue;
		}

		vecStart = vecTemp;

		pData = pEdict->pvPrivateData;
		vecEnd = pEdict->v.origin + pEdict->v.view_ofs;
		vecPlane = vecStart - vecEnd;

		if (vecPlane.Length() > flRadius)
		{
			continue;
		}

		TRACE_LINE(vecStart, vecEnd, 0, pFlashbang, &tr);

		if (semiclipData.semiclip)
		{
			for (int k = 0; k < iMaxClients; k++)
			{
				int iHit = NUM_FOR_EDICT(tr.pHit);

				if (!iHit || iHit > iMaxClients || j == iHit || !g_pSemiclip[j].solid[iHit])
				{
					break;
				}

				pHit = tr.pHit;

				vecStart = pHit->v.origin + pHit->v.view_ofs;

				TRACE_LINE(vecStart, vecEnd, 0, pHit, &tr);
			}
		}
		if (tr.flFraction == 1.0f || tr.pHit == pEdict)
		{
			flAdjustedDamage = flDamage - (vecPlane.Length() * flFallDmg);

			if (flAdjustedDamage < 0.0f)
			{
				flAdjustedDamage = 0.0f;
			}

			MAKE_VECTORS(pEdict->v.v_angle);

			if (DotProduct(vecPlane, gpGlobals->v_forward) < 0.3f)
			{
				flFadeTime = flAdjustedDamage * 1.75f;
				flFadeHold = flAdjustedDamage / 3.5f;
				iAlpha = 200;
			}
			else
			{
				flFadeTime = flAdjustedDamage * 3.0f;
				flFadeHold = flAdjustedDamage / 1.5f;
				iAlpha = 255;
			}

			m_flFlashedAt = *((float *)pData + OFFSET_FLASH_AT);
			m_flFlashHoldTime = *((float *)pData + OFFSET_FLASH_HOLD_TIME);
			m_flFlashDuration = *((float *)pData + OFFSET_FLASH_DURATION);
			m_iFlashAlpha = *((int *)pData + OFFSET_FLASH_ALPHA);

			flCurrentHoldTime = m_flFlashedAt + m_flFlashHoldTime - flCurrentTime;

			if (flCurrentHoldTime > 0.0f && iAlpha == 255.0f)
			{
				flFadeHold += flCurrentHoldTime;
			}
			if (m_flFlashedAt != 0.0f && m_flFlashDuration != 0.0f && (m_flFlashedAt + m_flFlashDuration + m_flFlashHoldTime) > flCurrentTime)
			{
				if (m_flFlashDuration > flFadeTime)
				{
					flFadeTime = m_flFlashDuration;
				}
				if (m_iFlashAlpha > iAlpha)
				{
					iAlpha = m_iFlashAlpha;
				}
			}

			UTIL_ScreenFade(pEdict, flFadeTime, flFadeHold, iAlpha);
			for (int i = 1; i <= iMaxClients; i++)
			{
				pObserver = EDICT_NUM(i);

				if (!i == j
					|| !pObserver->pvPrivateData
					|| (pObserver->v.flags == FL_DORMANT)
					|| !(pObserver->v.iuser1 == 4 && pObserver->v.iuser2 == j))
				{
					continue;
				}

				UTIL_ScreenFade(pObserver, flFadeTime, flFadeHold, iAlpha);
			}

			*((float *)pData + OFFSET_FLASH_UNTIL) = flFadeTime / 3 + flCurrentTime;
			*((float *)pData + OFFSET_FLASH_AT) = flCurrentTime;

			*((float *)pData + OFFSET_FLASH_HOLD_TIME) = flFadeHold;
			*((float *)pData + OFFSET_FLASH_DURATION) = flFadeTime;
			*((int *)pData + OFFSET_FLASH_ALPHA) = iAlpha;
		}
	}
}
// Change settings at runtime.
void SVR_SemiclipOption()
{
	if (CMD_ARGC() < 3)
	{
		print_settings();
		return;
	}

	const char *argv = CMD_ARGV(1);
	const char *value = CMD_ARGV(2);

	if (*value == '\0')
	{
		return;
	}

	// Block some settings from changing at runtime
	static const char *szRestrict[] =
	{
		"patch", "flashfix"
	};
	for (int i = 0; i < 2; i++)
	{
		if (!strcasecmp(argv, szRestrict[i]))
		{
			printf("[%s] Error: Setting \"%s\" can't be changed while server is running.\n", Plugin_info.logtag, argv);
			return;
		}
	}

	if (!parse_settings(argv, value, READ_IN_GAME))
	{
		print_settings();
	}
	else if (!semiclipData.semiclip)
	{
		g_bNotActive = true;

		g_pFunctionTable->pfnPM_Move = NULL;
		g_pEnginefuncsTable_Post->pfnAlertMessage = NULL;
	}
	else
	{
		if (!semiclipData.time)
		{
			memset(g_pSemiclip, 0, sizeof(g_pSemiclip));

			g_bNotActive = false;

			g_pFunctionTable->pfnPM_Move = PM_Move;
			g_pEnginefuncsTable_Post->pfnAlertMessage = NULL;
		}
		else
			g_pEnginefuncsTable_Post->pfnAlertMessage = AlertMessage;
	}
}
// Hook for Q_memcpy inside of the SV_WriteEntitiesToClient function.
// Mostly sets all players as non-solid and apply transparency when sending entities to the active host.
void *Q_memcpy_Handler(void *_Dst, entity_state_t *_Src, uint32_t _Size)
{
	if (g_bNotActive)
	{
		// Semiclip is off
		return memcpy(_Dst, _Src, _Size);
	}

	int j;
	int nSize;
	int hostId, ent;

	semiclip_t *hostSemiclip;
	edict_t *pHostEdict, *pEdict;
	entity_state_t *state;

	nSize = _Size / sizeof(entity_state_t);
	pHostEdict = *(edict_t **)(*global_host_client + OFFSET_EDICT_CL);

	if (pHostEdict->v.deadflag != DEAD_NO)
	{
		// Player that is getting updates is dead
		return memcpy(_Dst, _Src, _Size);
	}

	hostId = NUM_FOR_EDICT(pHostEdict);
	hostSemiclip = &g_pSemiclip[hostId];

	// Loop thru players in the update pack
	for (j = 0; j < nSize; j++)
	{
		state = _Src + j;
		ent = state->number;

		if (ent > gpGlobals->maxClients)
		{
			break;
		}

		pEdict = EDICT_NUM(ent);

		// Skip for the host player and dead players
		if (pEdict == pHostEdict || pEdict->v.deadflag != DEAD_NO)
		{
			continue;
		}

		// If player was non-solid in PM_Move, send it as non-solid
		if (hostSemiclip->solid[ent])
		{
			state->solid = SOLID_NOT;

			// Apply transparency if configured so
			if (semiclipData.transparency)
			{
				state->rendermode = kRenderTransAlpha;
				state->renderamt = semiclipData.effects ? (hostSemiclip->diff[ent] > MIN_AMOUNT) ? hostSemiclip->diff[ent] : MIN_AMOUNT : semiclipData.transparency;
			}
		}
	}

	return memcpy(_Dst, _Src, _Size);
}

bool IsUserSpectator(entvars_t& ent) {
	return ent.iuser1 || ent.iuser2;
}

bool Semiclip_IsTeamAllowed(int playerTeamId, int otherTeamId)
{
	switch (semiclipData.team)
	{
		case SemiclipTeam::ALL: return true;
		case SemiclipTeam::ONLY_BLUE: return playerTeamId == TeamId::BLUE_TEAM && otherTeamId == TeamId::BLUE_TEAM;
		case SemiclipTeam::ONLY_RED: return playerTeamId == TeamId::RED_TEAM && otherTeamId == TeamId::RED_TEAM;
		case SemiclipTeam::ONLY_TEAMMATES: return playerTeamId == otherTeamId;
	}

	return false;
}

static bool allowDontSolid(playermove_t *pmove, edict_t *pHost, int host, int j)
{
	int ent = pmove->physents[j].player;

	semiclip_t* hostSemiclip = &(g_pSemiclip[host]);
	semiclip_t* otherSemiclip = &(g_pSemiclip[ent]);

	if (hostSemiclip->dont)
	{
		return hostSemiclip->solid[ent] = false;
	}

	edict_t *pEntity;

	pHost = EDICT_NUM(host);
	pEntity = EDICT_NUM(ent);
	
	entvars_t* pevHost = &(pHost->v);
	entvars_t* pevEnt = &(pEntity->v);

	Vector hostOrigin = pevHost->origin;
	Vector entOrigin = pevEnt->origin;

	int hostTeamId = GetTeamId(pHost);
	int entTeamId = GetTeamId(pEntity);

	hostSemiclip->diff[ent] = GET_DISTANCE(hostOrigin, entOrigin);

	bool isHostSpec = IsUserSpectator(*pevHost);
	bool isEntSpec = IsUserSpectator(*pevEnt);
	bool isDistanceAllowed = hostSemiclip->diff[ent] < semiclipData.distance;

	hostSemiclip->solid[ent] = isHostSpec || (semiclipData.effects || isDistanceAllowed) && 
		!isEntSpec &&
		Semiclip_IsTeamAllowed(hostTeamId, entTeamId) &&
		!otherSemiclip->dont;

	if (semiclipData.crouch && hostSemiclip->solid[ent])
	{
		float fDiff = abs(hostOrigin.z - entOrigin.z);

		if (fDiff < FLOAT_CROUCH && hostSemiclip->crouch[ent] && otherSemiclip->crouch[host])
		{
			hostSemiclip->crouch[ent] = false;
			otherSemiclip->crouch[host] = false;
		}

		if (!hostSemiclip->crouch[ent] && (pevEnt->button & IN_DUCK || pevEnt->flags & FL_DUCKING) && pevHost->button & IN_DUCK && fDiff >= FLOAT_CROUCH)
		{
			hostSemiclip->crouch[ent] = true;
			otherSemiclip->crouch[host] = true;

			return hostSemiclip->solid[ent] = false;
		}
		if ((pevHost->groundentity == pEntity || pevEnt->groundentity == pHost || fDiff >= FLOAT_CROUCH) && (hostSemiclip->crouch[ent] || otherSemiclip->crouch[host]))
		{
			return hostSemiclip->solid[ent] = false;
		}
	}

	return hostSemiclip->solid[ent];
}
void PM_Move(playermove_t *pmove, int server)
{
	// Skip for spectators and dead players
	if (pmove->spectator || pmove->dead || pmove->deadflag != DEAD_NO)
	{
		RETURN_META(MRES_IGNORED);
	}

	int j;
	int hostId;
	edict_t *pHostEdict;
	int numPlayers = 0;
	int numphysent = -1;

	hostId = pmove->player_index + 1;
	pHostEdict = EDICT_NUM(hostId);

	// Skip to first player
	for (j = 0; j < pmove->numphysent; j++)
	{
		++numPlayers;
		if (pmove->physents[numphysent].player && ++numphysent)
		{
			break;
		}
	}

	// No players around?
	if (!numPlayers)
	{
		memset(&g_pSemiclip[hostId], 0, sizeof(semiclip_t));
	}

	for (j = numphysent; j < pmove->numphysent; j++)
	{
		if (!pmove->physents[j].player || !allowDontSolid(pmove, pHostEdict, hostId, j))
		{
			// Copy entity into new list, so it will participate in calculations
			pmove->physents[numphysent++] = pmove->physents[j];
		}
	}

	// If the time from the beginning of the round was passed
	if (semiclipData.time && gpGlobals->time > semiclipData.count)
	{
		bool bCollide = false;
		bool needSolid = false;
		int hostTeamId;

		entvars_t *e;
		edict_t *pEntity;
		Vector hostOrigin;
		
		hostOrigin = pHostEdict->v.origin;
		hostTeamId = GetTeamId(pHostEdict);

		for (pEntity = startEdict, j = 1; pEntity <= maxEdict; pEntity++, j++)
		{
			e = &(pEntity->v);

			if (g_pSemiclip[j].dont || !pEntity->pvPrivateData || e->deadflag != DEAD_NO || e->health <= 0.0)
			{
				continue;
			}

			if (!bCollide && j != hostId
				&& ((semiclipData.team == 0) ? 1 : ((semiclipData.team == 3) ? (hostTeamId == GetTeamId(pEntity)) : (hostTeamId == semiclipData.team && GetTeamId(pEntity) == semiclipData.team)))
				&& GET_COLLIDE(hostOrigin, e->origin))
				bCollide = true;

			needSolid = true;

			if (bCollide && needSolid)
				break;
		}

		// the last player should always have g_pSemiclip[host].dont = true
		if (!numPlayers || !bCollide)
			g_pSemiclip[hostId].dont = true;

		// if no players uses semiclip, so put callback NULL
		if (!needSolid)
		{
			g_bNotActive = true;
			g_pFunctionTable->pfnPM_Move = NULL;
		}
	}

	pmove->numphysent = numphysent;
	RETURN_META(MRES_IGNORED);
}
void ServerActivate_Post(edict_t *pEdictList, int edictCount, int clientMax)
{
	load_config_maps();

	nullEdict = pEdictList;
	startEdict = pEdictList + 1;
	maxEdict = pEdictList + clientMax;

	memset(g_pSemiclip, 0, sizeof(g_pSemiclip));
	g_szTeamList = CVAR_GET_STRING("mp_teamlist");

	RETURN_META(MRES_IGNORED);
}
void ClientPutInServer_Post(edict_t *pEdict)
{
	if (!pEdict->pvPrivateData)
	{
		RETURN_META(MRES_IGNORED);
	}

	int host = NUM_FOR_EDICT(pEdict);

	memset(&g_pSemiclip[host], 0, sizeof(semiclip_t));

	RETURN_META(MRES_IGNORED);
}
void AlertMessage(ALERT_TYPE atype, char *fmt, ...)
{
	if (atype != at_logged)
	{
		RETURN_META(MRES_IGNORED);
	}

	char buf[0x100U];

	va_list	ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
	va_end(ap);

	if (!strcmp(buf, "World triggered \"Round_Start\"\n"))
	{
		g_bNotActive = false;
		g_pFunctionTable->pfnPM_Move = PM_Move;
		semiclipData.count = gpGlobals->time + semiclipData.time;

		memset(g_pSemiclip, 0, sizeof(g_pSemiclip));
	}
	RETURN_META(MRES_IGNORED);
}
