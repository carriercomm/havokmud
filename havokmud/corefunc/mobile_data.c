/*
 *  This file is part of the havokmud package
 *  Copyright (C) 2008 Gavin Hurlbut
 *
 *  havokmud is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*HEADER---------------------------------------------------
* $Id$
*
* Copyright 2008 Gavin Hurlbut
* All rights reserved
*
* Comments :
*
*--------------------------------------------------------*/

/* INCLUDE FILES */
#include "environment.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include "structs.h"
#include "newstructs.h"
#include "protos.h"
#include "utils.h"
#include "logging.h"
#include "interthread.h"
#include "balanced_btree.h"

/* CVS generated ID string */
static char ident[] _UNUSED_ = 
    "$Id$";

/**
 * @file
 * @brief Handles loading, saving of mobiles
 */

#define MOB_DIR "mobiles"
FILE           *mob_f;           /* file containing mob prototypes */

int             top_of_mobt = 0;        /* top of mobile index table */
int             top_of_sort_mobt = 0;
int             top_of_alloc_mobt = 99999;
int             mob_tick_count = 0;
int             top_of_scripts = 0;


BalancedBTree_t *mobileMasterTree  = NULL;
BalancedBTree_t *mobileKeywordTree = NULL;
BalancedBTree_t *mobileTypeTree    = NULL;

long            mob_count = 0;


void initializeMobiles( void )
{
    mobileMasterTree = BalancedBTreeCreate( BTREE_KEY_INT );
    mobileKeywordTree = BalancedBTreeCreate( BTREE_KEY_STRING );
    mobileTypeTree = BalancedBTreeCreate( BTREE_KEY_INT );
#if 0
    db_load_mobile_tree( mobileMasterTree );
#endif
}

struct index_data *mobileIndex( int vnum )
{
    struct index_data      *index;
    BalancedBTreeItem_t    *item;

    item = BalancedBTreeFind( mobileMasterTree, &vnum, UNLOCKED, FALSE );
    if( !item ) {
        return( NULL );
    }

    index = (struct index_data *)item->item;
    return( index );
}


void mobileInsert(struct char_data *mob, long vnum)
{
    BalancedBTreeItem_t    *item;
    struct index_data      *index;

    CREATE(index, struct index_data, 1);
    CREATE(item, BalancedBTreeItem_t, 1);

    index->vnum = vnum;
    StringToKeywords( GET_NAME(mob), &index->keywords );
    index->number = 0;
    index->func = NULL;
    index->list = LinkedListCreate();

    item->key = &index->vnum;
    item->item = (void *)index;
    BalancedBTreeAdd( mobileMasterTree, item, UNLOCKED, TRUE );
}


/*
 * read a mobile from MOB_FILE
 */

typedef struct {
    int_func    func;
    long        actbit;
} CommonProcDef_t;

static CommonProcDef_t procDefs[] = {
    { NULL, 0 },                    /* PROC_NONE */
    { shopkeeper, 0 },              /* PROC_SHOPKEEPER */
    { generic_guildmaster, 0 },     /* PROC_GUILDMASTER */
    { Tyrannosaurus_swallower, 0 }, /* PROC_SWALLOWER */
    { NULL, 0 },                    /* PROC_DRAIN */
    { QuestMobProc, 0 },            /* PROC_QUEST */
    { BreathWeapon, 0 },            /* PROC_OLD_BREATH */
    { FireBreather, 0 },            /* PROC_FIRE_BREATH */
    { GasBreather, 0 },             /* PROC_GAS_BREATH */
    { FrostBreather, 0 },           /* PROC_FROST_BREATH */
    { AcidBreather, 0 },            /* PROC_ACID_BREATH */
    { LightningBreather, 0 },       /* PROC_LIGHTNING_BREATH */
    { DehydBreather, 0 },           /* PROC_DEHYDRATION_BREATH */
    { VaporBreather, 0 },           /* PROC_VAPOR_BREATH */
    { SoundBreather, 0 },           /* PROC_SOUND_BREATH */
    { ShardBreather, 0 },           /* PROC_SHARD_BREATH */
    { SleepBreather, 0 },           /* PROC_SLEEP_BREATH */
    { LightBreather, 0 },           /* PROC_LIGHT_BREATH */
    { DarkBreather, 0 },            /* PROC_DARK_BREATH */
    { receptionist, ACT_SENTINEL }, /* PROC_RECEPTIONIST */
    { RepairGuy, 0 }                /* PROC_REPAIRGUY */
};
static int procDefCount = NELEMENTS(procDefs);

struct char_data *mobileRead(int nr)
{
    struct char_data   *mob;
    struct index_data  *index;
    int                 procNum;

    index = mobileIndex( nr );
    if( !index ) {
        Log("Mobile (V) %d does not exist in database.", nr);
        return( NULL );
    }

    CREATE(mob, struct char_data, 1);
    if (!mob) {
        Log("Cannot create mob?!");
        return( NULL );
    }

    clear_char(mob);

#if 0
    if( !db_read_mobile(mob, nr) ) {
        free_char( mob );
        return( NULL );
    }
#endif

    /*
     * assign common proc flags
     */
    procNum = mob->specials.proc;
    if( procNum >= PROC_NONE && procNum < procDefCount ) {
        if( procDefs[procNum].func ) {
            index->func = procDefs[procNum].func;
        }
        SET_BIT( mob->specials.act, procDefs[procNum].actbit );
    }

    mob_count++;

    return (mob);
}


/*
 * ---------- Start of write_mob_to_file ----------
 */
/*
 * write a mobile to a file
 */

 /*
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                                          *
  *  Redone By Lennya, 20030802                                              *
  *  Mob files will now have this format:                                    *
  *                                                                          *
  *  #vnum                                                                   *
  *  name~                                                                   *
  *  short description~                                                      *
  *  long description~                                                       *
  *  description                                                             *
  *  ~                                                                       *
  *  local sound                                                             *
  *  ~                                                                       *
  *  distant sound                                                           *
  *  ~                                                                       *
  *  ActionFlags  AffectFlags  Alignment  NumAttacks                         *
  *  Level  HitRoll  ArmorClass/10  TotalHp  Damage(Example: 1d8+2)          *
  *  -1  Gold  ExpFlag  Race                                                 *
  *  Position  DefaultPosition  Sex  Immunities  Resistance  Susceptibilies  *
  *  CommonProcedure                                                         *
  *  talk string                                                             *
  *  ~                                                                       *
  *  quest solved string                                                     *
  *  ~                                                                       *
  *  wrong quest item string                                                 *
  *  ~                                                                       *
  *                                                                          *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  */

void mobileWriteToFile(struct char_data *mob, void * mob_fi)
{
    int             i,
                    xpflag;
    long            tmp;
    float           tmpexp;
    char            buff[MAX_STRING_LENGTH];

    /*
     *** String data ***
     */
    fwrite_string(mob_fi, mob->player.name);
    fwrite_string(mob_fi, mob->player.short_descr);
    remove_cr(buff, mob->player.long_descr);
    fwrite_string(mob_fi, buff);
    remove_cr(buff, mob->player.description);
    fwrite_string(mob_fi, buff);
    if (mob->player.sounds) {
        remove_cr(buff, mob->player.sounds);
        fwrite_string(mob_fi, buff);
    } else {
        fwrite_string(mob_fi, "");
    }
    if (mob->player.distant_snds) {
        remove_cr(buff, mob->player.distant_snds);
        fwrite_string(mob_fi, buff);
    } else {
        fwrite_string(mob_fi, "");
    }

    /*
     *** Numeric data ***
     */
    tmp = mob->specials.act;
    REMOVE_BIT(tmp, ACT_ISNPC);
    REMOVE_BIT(tmp, ACT_HATEFUL);
    REMOVE_BIT(tmp, ACT_GUARDIAN);
    REMOVE_BIT(tmp, ACT_HUNTING);
    REMOVE_BIT(tmp, ACT_AFRAID);

    fprintf(mob_fi, "%ld ", tmp);
    fprintf(mob_fi, " %ld ", mob->specials.affected_by);
    fprintf(mob_fi, " %d ", mob->specials.alignment);
    fprintf(mob_fi, " %.1f \n", mob->mult_att);

    fprintf(mob_fi, " %d ", GET_LEVEL(mob, WARRIOR_LEVEL_IND));
    fprintf(mob_fi, " %d ", (mob->points.hitroll - 20) * -1);
    fprintf(mob_fi, " %d ", mob->points.armor / 10);
    fprintf(mob_fi, " %d ", mob->points.max_hit);
    fprintf(mob_fi, " %dd%d+%d \n", mob->specials.damnodice,
            mob->specials.damsizedice, mob->points.damroll);

    tmpexp = GET_EXP(mob);

    /*
     * revert exp for agressive mobs (lower)
     */
    if (IS_SET(mob->specials.act, ACT_AGGRESSIVE)) {
        /*
         * big bonus for fully aggressive mobs for now
         */
        if (!IS_SET(mob->specials.act, ACT_WIMPY) ||
            IS_SET(mob->specials.act, ACT_META_AGG)) {
            tmpexp = tmpexp / 1.5;
        }
        tmpexp = tmpexp / 1.1;
    }
    /*
     * revert exp for wimpy mobs (higher)
     */
    if (IS_SET(mob->specials.act, ACT_WIMPY)) {
        tmpexp = tmpexp / 0.9;
    }
    i = (int) tmpexp;

    fprintf(mob_fi, " %d ", -1);
    fprintf(mob_fi, " %d ", mob->points.gold);

#if 0
    xpflag = GetExpFlags(mob, i);
#else
    xpflag = 0;
#endif
    if (xpflag < 1) {
        xpflag = 1;
    }
    if (xpflag > 31) {
        xpflag = 31;
    }
    fprintf(mob_fi, " %d ", xpflag);
    fprintf(mob_fi, " %d \n", GET_RACE(mob));

    fprintf(mob_fi, " %d ", mob->specials.position);
    fprintf(mob_fi, " %d ", mob->specials.default_pos);

    fprintf(mob_fi, " %d ", mob->player.sex);
    fprintf(mob_fi, " %ld ", mob->immune);
    fprintf(mob_fi, " %ld ", mob->M_immune);
    fprintf(mob_fi, " %ld \n", mob->susc);

    fprintf(mob_fi, " %d \n", mob->specials.proc);

    if (mob->specials.talks) {
        remove_cr(buff, mob->specials.talks);
        fwrite_string(mob_fi, buff);
    } else {
        fwrite_string(mob_fi, "");
    }
    if (mob->specials.quest_yes) {
        remove_cr(buff, mob->specials.quest_yes);
        fwrite_string(mob_fi, buff);
    } else {
        fwrite_string(mob_fi, "");
    }
    if (mob->specials.quest_no) {
        remove_cr(buff, mob->specials.quest_no);
        fwrite_string(mob_fi, buff);
    } else {
        fwrite_string(mob_fi, "");
    }
}

void save_new_mobile_structure(struct char_data *mob, FILE * mob_fi)
{
    int             i,
                    xpflag;
    long            tmp;
    float           tmpexp;
    char            buff[MAX_STRING_LENGTH];

    /*
     *** String data ***
     */
    fwrite_string(mob_fi, mob->player.name);
    fwrite_string(mob_fi, mob->player.short_descr);
    remove_cr(buff, mob->player.long_descr);
    fwrite_string(mob_fi, buff);
    remove_cr(buff, mob->player.description);
    fwrite_string(mob_fi, buff);
    if (mob->player.sounds) {
        remove_cr(buff, mob->player.sounds);
        fwrite_string(mob_fi, buff);
    } else {
        fwrite_string(mob_fi, "");
    }
    if (mob->player.distant_snds) {
        remove_cr(buff, mob->player.distant_snds);
        fwrite_string(mob_fi, buff);
    } else {
        fwrite_string(mob_fi, "");
    }

    /*
     *** Numeric data ***
     */
    tmp = mob->specials.act;
    REMOVE_BIT(tmp, ACT_ISNPC);
    REMOVE_BIT(tmp, ACT_HATEFUL);
    REMOVE_BIT(tmp, ACT_GUARDIAN);
    REMOVE_BIT(tmp, ACT_HUNTING);
    REMOVE_BIT(tmp, ACT_AFRAID);

    fprintf(mob_fi, "%ld ", tmp);
    fprintf(mob_fi, " %ld ", mob->specials.affected_by);
    fprintf(mob_fi, " %d ", mob->specials.alignment);
    fprintf(mob_fi, " %.1f \n", mob->mult_att);

    fprintf(mob_fi, " %d ", GET_LEVEL(mob, WARRIOR_LEVEL_IND));
    fprintf(mob_fi, " %d ", (mob->points.hitroll - 20) * -1);
    fprintf(mob_fi, " %d ", mob->points.armor / 10);
    fprintf(mob_fi, " %d ", mob->points.max_hit);
    fprintf(mob_fi, " %dd%d+%d \n", mob->specials.damnodice,
            mob->specials.damsizedice, mob->points.damroll);

    tmpexp = GET_EXP(mob);

    /*
     * revert exp for agressive mobs (lower)
     */
    if (IS_SET(mob->specials.act, ACT_AGGRESSIVE)) {
        /*
         * big bonus for fully aggressive mobs for now
         */
        if (!IS_SET(mob->specials.act, ACT_WIMPY) ||
            IS_SET(mob->specials.act, ACT_META_AGG)) {
            tmpexp = tmpexp / 1.5;
        }
        tmpexp = tmpexp / 1.1;
    }

    /*
     * revert exp for wimpy mobs (higher)
     */
    if (IS_SET(mob->specials.act, ACT_WIMPY)) {
        tmpexp = tmpexp / 0.9;
    }
    i = (int) tmpexp;
    if (mob->points.gold < 0) {
        mob->points.gold = 0;
    }
    i -= mob->points.gold;

    fprintf(mob_fi, " %d ", -1);
    fprintf(mob_fi, " %d ", mob->points.gold);

#if 0
    xpflag = GetExpFlags(mob, i);
#else
    xpflag = 0;
#endif
    if (xpflag < 1) {
        xpflag = 1;
    }
    if (xpflag > 31) {
        xpflag = 31;
    }
    fprintf(mob_fi, " %d ", xpflag);
    fprintf(mob_fi, " %d \n", GET_RACE(mob));

    fprintf(mob_fi, " %d ", mob->specials.position);
    fprintf(mob_fi, " %d ", mob->specials.default_pos);

    fprintf(mob_fi, " %d ", mob->player.sex);
    fprintf(mob_fi, " %ld ", mob->immune);
    fprintf(mob_fi, " %ld ", mob->M_immune);
    fprintf(mob_fi, " %ld \n", mob->susc);

    fprintf(mob_fi, " %d \n", mob->specials.proc);

    if (mob->specials.talks) {
        remove_cr(buff, mob->specials.talks);
        fwrite_string(mob_fi, buff);
    } else {
        fwrite_string(mob_fi, "");
    }
    if (mob->specials.quest_yes) {
        remove_cr(buff, mob->specials.quest_yes);
        fwrite_string(mob_fi, buff);
    } else {
        fwrite_string(mob_fi, "");
    }
    if (mob->specials.quest_no) {
        remove_cr(buff, mob->specials.quest_no);
        fwrite_string(mob_fi, buff);
    } else {
        fwrite_string(mob_fi, "");
    }
}

void InitScripts(void)
{
    char            buf[255],
                    buf2[255];
    FILE           *f1,
                   *f2;
    int             i,
                    count;
    struct char_data *mob;

    if (!script_data) {
        top_of_scripts = 0;
    }
    return;
    /*
     * DISABLED
     */

    /*
     * what is ths for? turn off all the scripts ???
     * -yes, just in case the script file was removed, saves pointer probs
     */

    for (mob = character_list; mob; mob = mob->next) {
        if (IS_MOB(mob) && IS_SET(mob->specials.act, ACT_SCRIPT)) {
            mob->commandp = 0;
            REMOVE_BIT(mob->specials.act, ACT_SCRIPT);
        }
    }
    if (!(f1 = fopen("scripts.dat", "r"))) {
        Log("Unable to open file \"scripts.dat\".");
        return;
    }

    if (script_data) {
        for (; i < top_of_scripts; i++) {
            if (script_data[i].script) {
                free(script_data[i].script);
            }
            if (script_data[i].filename) {
                free(script_data[i].filename);
            }
        }
        if (script_data) {
            free(script_data);
        }
        top_of_scripts = 0;
    }

    script_data = NULL;
    script_data = (struct scripts *) malloc(sizeof(struct scripts));

    while (1) {
        if (fgets(buf, 254, f1) == NULL) {
            break;
        }
        if (buf[strlen(buf) - 1] == '\n') {
            buf[strlen(buf) - 1] = '\0';
        }
        sscanf(buf, "%s %d", buf2, &i);

        sprintf(buf, "scripts/%s", buf2);
        if (!(f2 = fopen(buf, "r"))) {
            Log("Unable to open script \"%s\" for reading.", buf2);
        } else {
            script_data = (struct scripts *) realloc(script_data,
                               (top_of_scripts + 1) * sizeof(struct scripts));

            count = 0;
            while (!feof(f2)) {
                fgets(buf, 254, f2);
                if (buf[strlen(buf) - 1] == '\n') {
                    buf[strlen(buf) - 1] = '\0';
                }
                /*
                 * you really don't want to do a lot of reallocs all at
                 * once
                 */
                if (count == 0) {
                    script_data[top_of_scripts].script =
                        (struct foo_data *)malloc(sizeof(struct foo_data));
                } else {
                    script_data[top_of_scripts].script = (struct foo_data *)
                        realloc(script_data[top_of_scripts].script,
                                sizeof(struct foo_data) * (count + 1));
                }
                script_data[top_of_scripts].script[count].line =
                    (char *) malloc(sizeof(char) * (strlen(buf) + 1));

                strcpy(script_data[top_of_scripts].script[count].line, buf);

                count++;
            }

            script_data[top_of_scripts].vnum = i;
            script_data[top_of_scripts].filename =
                (char *) malloc((strlen(buf2) + 1) * sizeof(char));
            strcpy(script_data[top_of_scripts].filename, buf2);
            Log("Script %s assigned to mobile %d.", buf2, i);
            top_of_scripts++;
            fclose(f2);
        }
    }

    if (top_of_scripts) {
        Log("%d scripts assigned.", top_of_scripts);
    } else {
        Log("No scripts found to assign.");
    }

    fclose(f1);
}



/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
