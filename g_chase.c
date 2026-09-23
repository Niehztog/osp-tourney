// g_chase.c -- vanilla's chase camera, with the mod's three observer entry
// points at the top: OSP_ChaseCam, OSP_startObserve and OSP_removeChaseCam.
// They share the join/leave sequence with p_camera.c's CameraCmd almost line
// for line.  One object, not two: real's ELF has an `as` nop (89 f6) between
// OSP_removeChaseCam and UpdateChaseCam and no .rodata realignment before
// UpdateChaseCam's strings.

#include "g_local.h"

// gamex86.dll: 1001C0E0..1001C72D
// gamei386.so: 00054420..00054969
void OSP_ChaseCam (edict_t *ent)
{
	// Two separate edict pointers, not one reused across both loops.
	gclient_t	*clp;
	edict_t		*p;
	edict_t		*ep;
	int			t;

	clp = ent->client;

	if (level.intermissiontime != 0)
		return;

	if (match_paused && m_mode > 1 && clp->resp.entered != ENTERED_ENTERED)
	{
		gi.cprintf (ent, PRINT_HIGH,
			"Sorry, cannot join teams during a paused match.\n");
		return;
	}

	if (clp->resp.entered == ENTERED_ENTERED &&
		ent->health < 100 && ent->health > 0)
	{
		gi.cprintf (ent, PRINT_HIGH,
			"Cannot go to spectator mode while injured.\n");
		return;
	}

	// Already chasing -> this is the "rejoin the game" half, the same sequence
	// CameraCmd runs when it leaves camera mode.
	if (clp->chase_target)
	{
		if (m_mode == 3 && !OSP_1v1AllowJoin (ent))
			return;

		if (!clp->resp.osp_r030 || m_mode == 3)
		{
			if (m_mode > 1 && !OSP_addTeamMember (ent, 2))
				return;

			clp->resp.osp_r030 = 1;
			clp->resp.enterframe = level.framenum;
		}
		else
		{
			if (m_mode > 1 && !OSP_readdTeamMember (ent))
				return;

			if (clp->resp.osp_r030)
				clp->resp.enterframe = level.framenum - clp->resp.osp_r2d4;
		}

		clp->chase_target = NULL;
		clp->update_chase = false;
		clp->osp_t03c = NULL;
		clp->resp.entered = ENTERED_ENTERED;
		clp->resp.osp_r240 = 0;
		clp->resp.score = clp->resp.osp_r248;
		clp->resp.osp_r0a0--;
		clp->resp.osp_r09c--;
		active_clients++;

		if (m_mode > 0 && sync_stat < 4)
		{
			clp->resp.osp_r010 -= 2;
			OSP_notready_cmd (ent, true);
		}

		gi.bprintf (PRINT_HIGH, "%s entered the game (clients = %d)\n",
			clp->pers.netname, active_clients);
		EntityListAdd (ent);
		OSP_DoRankSort ();
		q2log_playerEntered (ent);
		return;
	}

	if (clp->resp.entered == ENTERED_ENTERED && !clp->resp.osp_r240)
		return;

	for (t = 1; t <= maxclients->value; t++)
	{
		p = g_edicts + t;

		if (p->inuse && p->solid && p != ent && p->client &&
			p->client->resp.entered == ENTERED_ENTERED)
		{
			if (rune_stat)
				OSP_deadDropRune (ent);

			VectorSet (ent->movedir, 0, 0, 0);
			ent->speed = camera_depth->value;
			clp->osp_t018 = 0;
			clp->chase_target = p;
			p->client->resp.osp_r000++;
			clp->update_chase = true;
			clp->osp_t03c = NULL;
			ent->waterlevel = 0;
			ent->watertype = 0;
			ent->svflags |= SVF_NOCLIENT;
			ent->solid = SOLID_NOT;
			ent->movetype = MOVETYPE_FLYMISSILE;
			clp->osp_t040 = 0;
			clp->ps.gunindex = 0;

			if (clp->resp.osp_r030 && clp->resp.entered == ENTERED_ENTERED)
			{
				clp->resp.osp_r248 = clp->resp.score;

				if (m_mode > 1 && clp->resp.team != 2)
					OSP_removeTeamMember (ent, false);
			}

			ent->deadflag = DEAD_NO;
			clp->resp.osp_r2dc = 0;
			clp->resp.score = -100;
			clp->resp.osp_r0a0--;
			clp->resp.osp_r09c--;
			clp->resp.osp_r000 = 0;
			clp->resp.osp_r2d4 = level.framenum - clp->resp.enterframe;

			if (sync_stat < 4 && clp->resp.entered == ENTERED_ENTERED)
				OSP_notready_cmd (ent, true);

			if (clp->resp.entered == ENTERED_ENTERED)
			{
				active_clients--;
				EntityListRemove (ent);

				if (m_mode == 3)
					OSP_1v1Remove (ent, false);
			}

			clp->resp.entered = 4;
			clp->resp.osp_r240 = 0;
			clp->menu = NULL;
			clp->inmenu = false;

			if (m_mode > 1)
				OSP_checkHalt (clp->resp.osp_r2cc);
			else if (m_mode == 1)
				OSP_checkHalt (2);

			OSP_DoRankSort ();
			break;
		}
	}

	if (!clp->chase_target)
	{
		gi.cprintf (ent, PRINT_HIGH, "No clients to chase.\n");
		return;
	}

	for (t = 1; t <= game.maxclients; t++)
	{
		ep = g_edicts + t;

		if (!ep->inuse || !ep->client ||
			ep->client->chase_target != ent)
			continue;

		gi.cprintf (ep, PRINT_HIGH, "Target switched to chasecam mode.\n");
		OSP_removeChaseCam (ep);
	}

	OSP_observerTeamFrags (ent);
	q2log_playerMode (ent, "Chasecam");
}

// gamex86.dll: 1001C72D..1001CA58
// gamei386.so: 0005496C..00054BFD
void OSP_startObserve (edict_t *ent)
{
	gclient_t	*cl;

	cl = ent->client;

	if (level.intermissiontime != 0)
		return;

	// Two independent top-level tests, each re-testing `entered`, not one
	// outer `if` with two arms.  Identical to CameraCmd's join sequence.
	if (cl->resp.entered == ENTERED_ENTERED && !cl->resp.osp_r240)
		return;

	if (cl->resp.entered == ENTERED_ENTERED && ent->health < 100 &&
		ent->health > 0 && sync_stat > 2)
	{
		gi.cprintf (ent, PRINT_HIGH,
			"Cannot go to spectator mode while injured.\n");
		return;
	}

	if (match_paused && m_mode > 1 && cl->resp.entered != ENTERED_ENTERED)
	{
		gi.cprintf (ent, PRINT_HIGH,
			"Sorry, cannot join teams during a paused match.\n");
		return;
	}

	if (cl->resp.entered == 2)
	{
		if (m_mode == 3 && !OSP_1v1AllowJoin (ent))
			return;

		if (!cl->resp.osp_r030 || m_mode == 3)
		{
			if (m_mode > 1 && !OSP_addTeamMember (ent, 2))
				return;

			cl->resp.osp_r030 = 1;
			cl->resp.enterframe = level.framenum;
		}
		else
		{
			if (m_mode > 1 && !OSP_readdTeamMember (ent))
				return;

			if (cl->resp.osp_r030)
				cl->resp.enterframe = level.framenum - cl->resp.osp_r2d4;
		}

		ent->deadflag = DEAD_NO;
		cl->chase_target = NULL;
		cl->update_chase = false;
		cl->osp_t03c = NULL;
		cl->resp.entered = ENTERED_ENTERED;
		cl->resp.osp_r240 = 0;
		cl->resp.osp_r2dc = 0;
		cl->resp.score = cl->resp.osp_r248;
		cl->resp.osp_r0a0--;
		cl->resp.osp_r09c--;
		active_clients++;

		if (m_mode > 0 && sync_stat < 4)
		{
			cl->resp.osp_r010 -= 2;
			OSP_notready_cmd (ent, true);
		}

		gi.bprintf (PRINT_HIGH, "%s entered the game (clients = %d)\n",
			cl->pers.netname, active_clients);
		EntityListAdd (ent);
		OSP_DoRankSort ();
		q2log_playerEntered (ent);
	}
	else
	{
		if (sync_stat < 4)
		{
			OSP_notready_cmd (ent, true);
			OSP_CheckReady ();
		}

		if (rune_stat)
			OSP_deadDropRune (ent);

		OSP_observerTeamFrags (ent);
		cl->resp.osp_r2d4 = level.framenum - cl->resp.enterframe;
		cl->resp.osp_r000 = 0;
		cl->menu = NULL;
		cl->inmenu = false;
		OSP_removeChaseCam (ent);
	}
}

// gamex86.dll: 1001CA58..1001CD50
// gamei386.so: 00054C00..00054E7E
void OSP_removeChaseCam (edict_t *ent)
{
	gclient_t	*client;
	edict_t		*ee;
	int			x;
	int			was;

	client = ent->client;

	if (level.intermissiontime != 0)
		return;

	gi.cprintf (ent, PRINT_HIGH, "Changing to OBSERVER mode.\n");
	client->chase_target = NULL;
	client->update_chase = false;

	if (sync_stat > 2 && m_mode < 2)
		client->ps.stats[20] = 0;

	OSP_zeroRuneStats (ent);
	ent->movetype = MOVETYPE_NOCLIP;
	ent->clipmask = 0;
	ent->solid = SOLID_NOT;
	ent->waterlevel = 0;
	ent->watertype = 0;
	ent->svflags |= SVF_NOCLIENT;
	client->resp.osp_r2bc = 1;
	client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;
	client->osp_t040 = 0;
	client->osp_t03c = NULL;
	client->latched_buttons &= ~BUTTON_ATTACK;
	client->ps.gunindex = 0;

	if (client->resp.osp_r030 && client->resp.entered == ENTERED_ENTERED)
	{
		client->resp.osp_r248 = client->resp.score;

		if (m_mode > 1 && client->resp.team != 2)
			OSP_removeTeamMember (ent, false);

		if (m_mode == 3)
			OSP_1v1Remove (ent, false);
	}

	client->resp.score = -100;
	client->resp.osp_r0a0--;
	client->resp.osp_r09c--;

	if (client->resp.entered == ENTERED_ENTERED)
	{
		active_clients--;
		EntityListRemove (ent);
	}

	was = client->resp.entered;
	client->resp.entered = 2;
	client->resp.osp_r240 = 0;
	OSP_DoRankSort ();

	if (sync_stat < 4)
	{
		client->resp.osp_r20c = 0;
		OSP_CheckReady ();
	}

	if (was == ENTERED_ENTERED)
	{
		for (x = 1; x <= game.maxclients; x++)
		{
			ee = g_edicts + x;

			if (!ee->inuse || !ee->client || ee->client->chase_target != ent)
				continue;

			gi.cprintf (ee, PRINT_HIGH, "Target switched to observer mode.\n");
			OSP_removeChaseCam (ee);
		}

		if (m_mode > 1)
			OSP_checkHalt (client->resp.osp_r2cc);
		else if (m_mode == 1)
			OSP_checkHalt (2);
	}

	q2log_playerMode (ent, "Observe");
}


// gamex86.dll: 1001CD50..1001D49B
// gamei386.so: 00054E80..0005550C
void UpdateChaseCam(edict_t *ent)
{
	vec3_t o, ownerv, goal;
	edict_t *targ;
	vec3_t forward, right;
	trace_t trace;
	int i;
	vec3_t oldgoal;
	vec3_t angles;
	vec3_t vangles;			// <INVENTED NAME>

	targ = ent->client->chase_target;

	VectorCopy(targ->s.origin, ownerv);
	VectorCopy(ent->s.origin, oldgoal);

	ownerv[2] += targ->viewheight;

	VectorCopy(targ->client->v_angle, angles);
	VectorCopy(targ->client->v_angle, vangles);

	if (ent->client->resp.entered == 4)
	{
		if (angles[PITCH] > 56)
			angles[PITCH] = 56;
	}
	else
	{
		if (angles[PITCH] > 1)
			angles[PITCH] = 1;
	}

	// chained chasecam: ent's own velocity/avelocity are unused while it is
	// frozen for chasecam, so they are repurposed as the previous frame's
	// cmd_angles, giving the frame-to-frame delta in avelocity.
	VectorSubtract(ent->velocity, ent->client->resp.cmd_angles, ent->avelocity);
	VectorCopy(ent->client->resp.cmd_angles, ent->velocity);

	// ent's own movedir/speed are likewise unused, and hold the free-look
	// pitch/yaw offset and zoom distance while chasecamming (entered==4);
	// in in-eyes mode (entered==8) there is no free-look and no zoom.
	if (ent->client->resp.entered == 4)
		ent->movedir[0] = camera_pitch->value;
	else
		ent->movedir[0] = 0;

	if (ent->client->resp.entered == 4)
		ent->movedir[1] = *(float *)&ent->client->osp_t018;
	else
	{
		ent->movedir[1] = 0;
		ent->speed = -12;
	}

	angles[PITCH] += ent->movedir[0];
	if (angles[PITCH] > 90)
		angles[PITCH] = 90;
	if (angles[PITCH] < -90)
		angles[PITCH] = -90;

	angles[YAW] += ent->movedir[1];

	vangles[PITCH] += ent->movedir[0];
	if (vangles[PITCH] > 90)
		vangles[PITCH] = 90;
	if (vangles[PITCH] < -90)
		vangles[PITCH] = -90;

	vangles[YAW] += ent->movedir[1];

	AngleVectors (angles, forward, right, NULL);
	VectorNormalize(forward);
	VectorMA(ownerv, -ent->speed, forward, o);

	if (ent->client->resp.entered == 4)
	{
		if (o[2] < targ->s.origin[2] + 30)
			o[2] = targ->s.origin[2] + 30;
	}

	// jump animation lifts
	if (!targ->groundentity)
		o[2] += 16;

	trace = gi.trace(ownerv, vec3_origin, vec3_origin, o, targ, MASK_SOLID);

	VectorCopy(trace.endpos, goal);

	VectorMA(goal, 2, forward, goal);

	// pad for floors and ceilings
	VectorCopy(goal, o);
	o[2] += 6;
	trace = gi.trace(goal, vec3_origin, vec3_origin, o, targ, MASK_SOLID);
	if (trace.fraction < 1) {
		VectorCopy(trace.endpos, goal);
		goal[2] -= 6;
	}

	VectorCopy(goal, o);
	o[2] -= 6;
	trace = gi.trace(goal, vec3_origin, vec3_origin, o, targ, MASK_SOLID);
	if (trace.fraction < 1) {
		VectorCopy(trace.endpos, goal);
		goal[2] += 6;
	}

	ent->client->ps.pmove.pm_type = PM_FREEZE;

	VectorCopy(goal, ent->s.origin);
	for (i=0 ; i<3 ; i++)
		ent->client->ps.pmove.delta_angles[i] = ANGLE2SHORT(targ->client->v_angle[i] - ent->client->resp.cmd_angles[i]);

	VectorCopy(vangles, ent->client->ps.viewangles);
	VectorCopy(vangles, ent->client->v_angle);

	ent->viewheight = 0;
	ent->client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;
	gi.linkentity(ent);

	if ((!ent->client->showscores && !ent->client->showinventory &&
		 !ent->client->showhelp && !(level.framenum & 0x1f)) ||
		ent->client->update_chase)
	{
		char string[1024];			// <INVENTED SIZE>

		ent->client->update_chase = false;

		if (m_mode != 2)
		{
			sprintf (string, "xv 44 yb -59 string \"Chasing `%s'\"",
				targ->client->pers.netname);
		}
		else if (sync_stat > 2)
		{
			sprintf (string, "xv 44 yb -59 string \"Chasing `%s' [%d] (%s)\"",
				targ->client->pers.netname, targ->client->resp.score,
				teams[targ->client->resp.team].netname);
		}
		else
		{
			sprintf (string, "xv 44 yb -59 string \"Chasing `%s' (%s)\"",
				targ->client->pers.netname, teams[targ->client->resp.team].netname);
		}

		gi.WriteByte (svc_layout);
		gi.WriteString (string);
		gi.unicast (ent, false);
	}
}

// gamex86.dll: 1001D49B..1001D5C4
// gamei386.so: 0005550C..0005562E
void ChaseNext(edict_t *ent)
{
	int i;
	edict_t *e;

	if (!ent->client->chase_target)
		return;

	VectorSet (ent->movedir, 0, 0, 0);
	ent->speed = camera_depth->value;
	ent->client->osp_t018 = 0;

	i = ent->client->chase_target - g_edicts;
	do {
		i++;
		if (i > maxclients->value)
			i = 1;
		e = g_edicts + i;
		if (!e->inuse)
			continue;
		if (e->solid)
			break;
	} while (e != ent->client->chase_target);

	ent->client->chase_target = e;
	e->client->resp.osp_r000++;
	ent->client->update_chase = true;
	UpdateChaseCam (ent);
}

// gamex86.dll: 1001D5C4..1001D6F0
// gamei386.so: 00055630..0005574B
void ChasePrev(edict_t *ent)
{
	int i;
	edict_t *e;

	if (!ent->client->chase_target)
		return;

	VectorSet (ent->movedir, 0, 0, 0);
	ent->speed = camera_depth->value;
	ent->client->osp_t018 = 0;

	i = ent->client->chase_target - g_edicts;
	do {
		i--;
		if (i < 1)
			i = maxclients->value;
		e = g_edicts + i;
		if (!e->inuse)
			continue;
		if (e->solid)
			break;
	} while (e != ent->client->chase_target);

	ent->client->chase_target = e;
	e->client->resp.osp_r000++;
	ent->client->update_chase = true;
	UpdateChaseCam (ent);
}
