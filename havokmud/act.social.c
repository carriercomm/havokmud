/*
  DaleMUD v2.0	Released 2/1994
  See license.doc for distribution terms.   DaleMUD is based on DIKUMUD
*/

#include <stdio.h>
#include <string.h>

#include "protos.h"

/* extern variables */

extern struct descriptor_data *descriptor_list;


int rand(void);
void *realloc(void *ptr, size_t size);
struct social_messg
{
  int act_nr;
  int hide;
  int min_victim_position; /* Position of victim */

  /* No argument was supplied */
  char *char_no_arg;
  char *others_no_arg;
  
  /* An argument was there, and a victim was found */
  char *char_found;		/* if NULL, read no further, ignore args */
  char *others_found;
  char *vict_found;
  
  /* An argument was there, but no victim was found */
  char *not_found;
  
  /* The victim turned out to be the character */
  char *char_auto;
  char *others_auto;
  
  /* For objects */
  char *obj_you;
  char *obj_other;
  
} *soc_mess_list = 0;



struct pose_type
{
	int level;          /* minimum level for poser */
	char *poser_msg[4];  /* message to poser        */
	char *room_msg[4];   /* message to room         */
} pose_messages[MAX_MESSAGES];

static int list_top = -1;




char *fread_action(FILE *fl)
{
  char buf[MAX_STRING_LENGTH], *rslt;
  
  for (;;)	{
    fgets(buf, MAX_STRING_LENGTH, fl);
    if (feof(fl))		{
      log("Fread_action - unexpected EOF.");
      exit(0);
    }
    
    if (*buf == '#')
      return(0);
    else {
      *(buf + strlen(buf) - 1) = '\0';
      CREATE(rslt, char, strlen(buf) + 1);
      strcpy(rslt, buf);
      return(rslt);
    }
  }
  return(0);
}
	



void boot_social_messages()
{
  FILE *fl;
  int tmp, hide, min_pos;
  
  if (!(fl = fopen(SOCMESS_FILE, "r"))) {
    perror("boot_social_messages");
    assert(0);
  }
  
  for (;;)    {
      fscanf(fl, " %d ", &tmp);
      if (tmp < 0)
	break;
      fscanf(fl, " %d ", &hide);
      fscanf(fl, " %d \n", &min_pos);
      
      /* alloc a new cell */
      if (!soc_mess_list) {
	CREATE(soc_mess_list, struct social_messg, 1);
	list_top = 0;
      } else
	if (!(soc_mess_list = (struct social_messg *)
	      realloc(soc_mess_list, sizeof (struct social_messg) *
		      (++list_top + 1))))  {
	    perror("boot_social_messages. realloc");
	    exit(0);
	  }
      
      /* read the stuff */
      soc_mess_list[list_top].act_nr = tmp;
      soc_mess_list[list_top].hide = hide;
      soc_mess_list[list_top].min_victim_position = min_pos;
      
      soc_mess_list[list_top].char_no_arg = fread_action(fl);
      soc_mess_list[list_top].others_no_arg = fread_action(fl);
      
      soc_mess_list[list_top].char_found = fread_action(fl);
      /*     
	     if(!soc_mess_list[list_top].char_found) {
	     soc_mess_list[list_top].obj_you = fread_action(fl);
	     
	     soc_mess_list[list_top].obj_other = fread_action(fl);
	     
	     }
      */
      
      /* if no char_found, the rest is to be ignored */
      if (!soc_mess_list[list_top].char_found)
	continue;
      
      soc_mess_list[list_top].others_found = fread_action(fl);	
      soc_mess_list[list_top].vict_found = fread_action(fl);
      
      soc_mess_list[list_top].not_found = fread_action(fl);
      
      soc_mess_list[list_top].char_auto = fread_action(fl);
      
      soc_mess_list[list_top].others_auto = fread_action(fl);
      
      soc_mess_list[list_top].obj_you = fread_action(fl);
      
      soc_mess_list[list_top].obj_other = fread_action(fl);
    
  }
  fclose(fl);
}




int find_action(int cmd)
{
  int bot, top, mid;
  
  bot = 0;
  top = list_top;
  
  if (top < 0)
    return(-1);
  
  for(;;)    {
      mid = (bot + top) / 2;
      
      if (soc_mess_list[mid].act_nr == cmd)
	return(mid);
      if (bot >= top)
	return(-1);
      
      if (soc_mess_list[mid].act_nr > cmd)
	top = --mid;
      else
	bot = ++mid;
    }
}





void do_action(struct char_data *ch, char *argument, int cmd)
{
  int act_nr;
  char buf[MAX_INPUT_LENGTH];
  struct social_messg *action;
  struct char_data *vict;
  struct obj_data *objx = 0;

  dlog("in do_action");

  if ((act_nr = find_action(cmd)) < 0)   {
    send_to_char("That action is not supported.\n\r", ch);
    return;
    }
  
  action = &soc_mess_list[act_nr];
  
  if (action->char_found)
    only_argument(argument, buf);
  else
    *buf = '\0';
  
  if (!*buf)    {
      send_to_char(action->char_no_arg, ch);
      send_to_char("\n\r", ch);
      act(action->others_no_arg, action->hide, ch, 0, 0, TO_ROOM);
      return;
    }
  
  if (!(vict = get_char_room_vis(ch, buf)))    {
     if ((objx = get_obj_in_list_vis(ch, buf, ch->carrying))) {
       act(action->obj_you, action->hide, ch, objx, objx, TO_CHAR);
       act(action->obj_other, action->hide, ch, objx, objx, TO_ROOM);
       return;
     }
       send_to_char(action->not_found, ch);
      send_to_char("\n\r", ch);
    }  else if (vict == ch)    {
      send_to_char(action->char_auto, ch);
      send_to_char("\n\r", ch);
      act(action->others_auto, action->hide, ch, 0, 0, TO_ROOM);
    }  else    {
      if (GET_POS(vict) < action->min_victim_position) {
	act("$N is not in a proper position for that.",FALSE,ch,0,vict,TO_CHAR);
      } else {
	act(action->char_found, 0, ch, 0, vict, TO_CHAR);
	
	act(action->others_found, action->hide, ch, 0, vict, TO_NOTVICT);
	
	act(action->vict_found, action->hide, ch, 0, vict, TO_VICT);
      }
    }
}



void do_insult(struct char_data *ch, char *argument, int cmd)
{
	static char buf[100];
	static char arg[MAX_STRING_LENGTH];
	struct char_data *victim;

dlog("in do_insult");
 
 only_argument(argument, arg);
 
 if(*arg) {
   if(!(victim = get_char_room_vis(ch, arg))) {
     send_to_char("Can't hear you!\n\r", ch);
   } else {
     if(victim != ch) { 
       sprintf(buf, "You insult %s.\n\r",GET_NAME(victim) );
       send_to_char(buf,ch);
       
       switch(random()%3) {
       case 0 : {
	 if (GET_SEX(ch) == SEX_MALE) {
	   if (GET_SEX(victim) == SEX_MALE)
	     act(
		 "$n accuses you of fighting like a woman!", FALSE,
		 ch, 0, victim, TO_VICT);
	   else
	     act("$n says that women can't fight.",
		 FALSE, ch, 0, victim, TO_VICT);
	 } else { /* Ch == Woman */
	   if (GET_SEX(victim) == SEX_MALE)
	     act("$n accuses you of having the smallest.... (brain?)",
		 FALSE, ch, 0, victim, TO_VICT );
	   else
	     act("$n tells you that you'd loose a beautycontest against a troll.", FALSE, ch, 0, victim, TO_VICT );
	 }
       } break;
       case 1 : {
	 act("$n calls your mother a bitch!",
	     FALSE, ch, 0, victim, TO_VICT );
       } break;
       default : {
	 act("$n tells you to get lost!",FALSE,ch,0,victim,TO_VICT);
       } break;
       } /* end switch */
       
       act("$n insults $N.", TRUE, ch, 0, victim, TO_NOTVICT);
     } else { /* ch == victim */
       send_to_char("You feel insulted.\n\r", ch);
     }
   }
 } else send_to_char("Sure you don't want to insult everybody.\n\r", ch);
}


void boot_pose_messages()
{
  FILE *fl;
  byte counter;
  byte class;

  if (!(fl = fopen(POSEMESS_FILE, "r")))  {
    perror("boot_pose_messages");
    exit(0);
  }

  for (counter = 0;;counter++)  {
    fscanf(fl, " %d ", &pose_messages[counter].level);
    if (pose_messages[counter].level < 0)
      break;
    for (class = 0;class < 4;class++){
      pose_messages[counter].poser_msg[class] = fread_action(fl);
      pose_messages[counter].room_msg[class] = fread_action(fl);
    }
  }
  fclose(fl);
}

void do_pose(struct char_data *ch, char *argument, int cmd)
{
  byte to_pose;
  byte counter;
  int lev, class;


dlog("in do_pose");

  lev = GetMaxLevel(ch);
  if (IS_IMMORTAL(ch)) {
    lev = LOW_IMMORTAL-1;
  }
  
  if ((lev < pose_messages[0].level) || !IS_PC(ch)) {
    send_to_char("Pardon?\n\r", ch);
    return;
  }

  if (!IS_SET(ch->player.class,CLASS_MAGIC_USER|CLASS_CLERIC|CLASS_WARRIOR|CLASS_THIEF)) {
    send_to_char("Sorry.. no pose messages for you yet\n", ch);
    return;
  }

  do {
          class = number(0, OLD_MAX_CLASS-1);
  } while((lev = GET_LEVEL(ch, class)) < pose_messages[0].level);

  
  for (counter = 0; (pose_messages[counter].level < lev) && 
       (pose_messages[counter].level > 0); counter++);
  counter--;
  
  to_pose = number(0, counter);

  act(pose_messages[to_pose].poser_msg[class], 0, ch, 0, 0, TO_CHAR);
  act(pose_messages[to_pose].room_msg[class], 0, ch, 0, 0, TO_ROOM);

}
