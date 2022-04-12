#include "pch.h"
#include "globals.h"
#include "engine.h"

#include "config.h"
#include "offsets.h"

namespace eng
{
	void CG_ParseReinforcementTimes(const char *pszReinfSeedString)
	{
		const char *tmp = pszReinfSeedString, *tmp2;
		unsigned int i, j, dwDummy, dwOffset[TEAM_NUM_TEAMS];

		#define GETVAL(x,y) if((tmp = strchr(tmp, ' ')) == NULL) return; x = atoi(++tmp)/y;

		dwOffset[TEAM_ALLIES] = atoi(pszReinfSeedString) >> REINF_BLUEDELT;
		GETVAL(dwOffset[TEAM_AXIS], (1 << REINF_REDDELT));
		tmp2 = tmp;

		for(i = TEAM_AXIS; i <= TEAM_ALLIES; i++)
		{
			tmp = tmp2;

			for(j = 0; j<MAX_REINFSEEDS; j++)
			{
				if(j == dwOffset[i])
				{
					GETVAL(cgs_aReinfOffset[i], aReinfSeeds[j]);
					cgs_aReinfOffset[i] *= 1000;
					break;
				}

				GETVAL(dwDummy, 1);
			}
		}

		#undef GETVAL
	}
	int CG_CalculateReinfTime(team_t team)
	{
		vmCvar_t redlimbotime, bluelimbotime;
		DoSyscall(CG_CVAR_REGISTER, &redlimbotime, XorString("g_redlimbotime"), XorString("30000"), 0);
		DoSyscall(CG_CVAR_REGISTER, &bluelimbotime, XorString("g_bluelimbotime"), XorString("30000"), 0);

		int dwDeployTime = (team == TEAM_AXIS) ? redlimbotime.integer : bluelimbotime.integer;
		if (dwDeployTime == 0)
			return 0;

		return (1 + (dwDeployTime - ((cgs_aReinfOffset[team] + cg_time - cgs_levelStartTime) % dwDeployTime)) * 0.001f);
	}
	void CG_RailTrail(const vec3_t from, const vec3_t to, const vec4_t col, int renderfx)
	{
		refEntity_t ent = {};
		VectorCopy(from, ent.origin);
		VectorCopy(to, ent.oldorigin);

		ent.reType = RT_RAIL_CORE;
		ent.customShader = media.railCoreShader;

		ent.shaderRGBA[0] = col[0] * 255;
		ent.shaderRGBA[1] = col[1] * 255;
		ent.shaderRGBA[2] = col[2] * 255;
		ent.shaderRGBA[3] = col[3] * 255;

		ent.renderfx = RF_NOSHADOW | renderfx;

		DoSyscall(CG_R_ADDREFENTITYTOSCENE, &ent);
	}
	void CG_Trace(trace_t *result, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int skipNumber, int mask)
	{
		//CG_BuildSolidList();

		DoSyscall(CG_CM_BOXTRACE, result, start, end, mins, maxs, 0, mask);

		result->entityNum = result->fraction != 1.0 ? ENTITYNUM_WORLD : ENTITYNUM_NONE;

		//CG_ClipMoveToEntities(start, mins, maxs, end, skipNumber, mask, qfalse, result);
	}
	bool IsPointVisible(const vec3_t start, const vec3_t pt)
	{
		trace_t t;
		eng::CG_Trace(&t, start, NULL, NULL, pt, cg_snapshot.ps.clientNum, MASK_SHOT);

		return (t.fraction == 1.f);
	}
	bool IsBoxVisible(const vec3_t start, const vec3_t mins, const vec3_t maxs, float step, vec3_t visOut)
	{
		// Trivial case: Middle is visible

		VectorAdd(mins, maxs, visOut);
		VectorScale(visOut, 0.5f, visOut);

		if (IsPointVisible(start, visOut))
			return true;


		// Middle wasn't visible, trace the whole box.
		// Interpolate between different box sizes up to 99%.
		// Always start with the smallest box and the middle of each edge.

		const vec3_t boxSize = 
		{
			abs(maxs[0] - mins[0]),
			abs(maxs[1] - mins[1]),
			abs(maxs[2] - mins[2])
		};

		for (float sd = 0.1f; sd < 0.99f; sd += 0.1f)
		{
			vec3_t scaledMins, scaledMaxs;
			VectorScale(boxSize, -sd/2.0f, scaledMins);
			VectorScale(boxSize, +sd/2.0f, scaledMaxs);
			VectorAdd(scaledMins, visOut, scaledMins);
			VectorAdd(scaledMaxs, visOut, scaledMaxs);

			const vec3_t boxCorner[] =
			{
				{ scaledMaxs[0], scaledMaxs[1], scaledMaxs[2] },
				{ scaledMaxs[0], scaledMaxs[1], scaledMins[2] },
				{ scaledMins[0], scaledMaxs[1], scaledMins[2] },
				{ scaledMins[0], scaledMaxs[1], scaledMaxs[2] },
				{ scaledMaxs[0], scaledMins[1], scaledMaxs[2] },
				{ scaledMaxs[0], scaledMins[1], scaledMins[2] },
				{ scaledMins[0], scaledMins[1], scaledMins[2] },
				{ scaledMins[0], scaledMins[1], scaledMaxs[2] }
			};
		
			// Try all mid values first
			for (size_t i = 0; i < std::size(boxCorner) - 1; i++)
			{
				vec3_t mid;
				VectorAdd(boxCorner[i], boxCorner[i + 1], mid);
				VectorScale(mid, 0.5f, mid);

				if (IsPointVisible(start, mid))
				{
					VectorCopy(mid, visOut);
					return true;
				}
			}

			// Try all corners last
			for (size_t i = 0; i < std::size(boxCorner); i++)
			{
				if (IsPointVisible(start, boxCorner[i]))
				{
					VectorCopy(boxCorner[i], visOut);
					return true;
				}
			}
		}

		return false;
	}
	bool AimAtTarget(const vec3_t target)
	{
		auto &localClient = cgs_clientinfo[cg_snapshot.ps.clientNum];

		vec3_t predictVieworg;
		VectorMA(cg_refdef.vieworg, cg_frametime / 1000.0f, localClient.velocity, predictVieworg);

		vec3_t dir, ang;
		VectorSubtract(target, predictVieworg, dir);
		vectoangles(dir, ang);

		vec3_t refdefViewAngles;
		vectoangles(cg_refdef.viewaxis[0], refdefViewAngles);

		float yawOffset = AngleNormalize180(ang[YAW] - refdefViewAngles[YAW]);
		float pitchOffset = AngleNormalize180(ang[PITCH] - refdefViewAngles[PITCH]);

		vec_t *angles = off::cur.viewangles();

		if (cfg.aimbotHumanAim)
		{
			const float targetDist = VectorDistance(predictVieworg, target);

			float targetScreenSizeDegX = cfg.aimbotHumanFovX / targetDist * 180.0f;
			float targetScreenSizeDegY = cfg.aimbotHumanFovY / targetDist * 180.0f;

			targetScreenSizeDegX = min(targetScreenSizeDegX, cfg.aimbotHumanFovMaxX);
			targetScreenSizeDegY = min(targetScreenSizeDegY, cfg.aimbotHumanFovMaxY);

			if (abs(yawOffset)   < targetScreenSizeDegX && 
				abs(pitchOffset) < targetScreenSizeDegY)
			{
				angles[YAW] += yawOffset * cfg.aimbotHumanSpeed;
				angles[PITCH] += pitchOffset * cfg.aimbotHumanSpeed;

				return true;
			}
		}
		else
		{
			angles[YAW] += yawOffset;
			angles[PITCH] += pitchOffset;

			return true;
		}

		return false;
	}
	bool IsKeyActionActive(const char *action)
	{
		int key1, key2;
		DoSyscall(CG_KEY_BINDINGTOKEYS, action, &key1, &key2);
	
		return DoSyscall(CG_KEY_ISDOWN, key1) || DoSyscall(CG_KEY_ISDOWN, key2);
	}
	hitbox_t GetHeadHitbox(const SClientInfo &ci)
	{
		const int modIndex = static_cast<int>(currentMod);
		auto &hitbox = head_hitboxes[modIndex];

		if (ci.flags & (EF_PRONE | EF_PRONE_MOVING))
			return { hitbox.prone_offset, hitbox.size };

		bool isMoving = !!VectorCompare(ci.velocity, vec3_origin);

		if (ci.flags & EF_CROUCHING)
			return { isMoving ? hitbox.crouch_offset_moving : hitbox.crouch_offset, hitbox.size };

		return { isMoving ? hitbox.stand_offset_moving : hitbox.stand_offset, hitbox.size };
	}
	bool IsEntityArmed(const entityState_t* entState)
	{
		if (currentMod == EMod::Legacy)
			return entState->effect1Time != 0;

		return entState->teamNum == TEAM_AXIS || entState->teamNum == TEAM_ALLIES;
	}
	bool IsValidTeam(int team)
	{
		return team == TEAM_AXIS || team == TEAM_ALLIES;
	}
}
