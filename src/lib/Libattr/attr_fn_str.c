/*
 * Copyright (C) 1994-2016 Altair Engineering, Inc.
 * For more information, contact Altair at www.altair.com.
 *  
 * This file is part of the PBS Professional ("PBS Pro") software.
 * 
 * Open Source License Information:
 *  
 * PBS Pro is free software. You can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free 
 * Software Foundation, either version 3 of the License, or (at your option) any 
 * later version.
 *  
 * PBS Pro is distributed in the hope that it will be useful, but WITHOUT ANY 
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU Affero General Public License for more details.
 *  
 * You should have received a copy of the GNU Affero General Public License along 
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *  
 * Commercial License Information: 
 * 
 * The PBS Pro software is licensed under the terms of the GNU Affero General 
 * Public License agreement ("AGPL"), except where a separate commercial license 
 * agreement for PBS Pro version 14 or later has been executed in writing with Altair.
 *  
 * Altair’s dual-license business model allows companies, individuals, and 
 * organizations to create proprietary derivative works of PBS Pro and distribute 
 * them - whether embedded or bundled with other software - under a commercial 
 * license agreement.
 * 
 * Use of Altair’s trademarks, including but not limited to "PBS™", 
 * "PBS Professional®", and "PBS Pro™" and Altair’s logos is subject to Altair's 
 * trademark licensing policies.
 *
 */
#include <pbs_config.h>   /* the master config generated by configure */

#include <assert.h>
#include <ctype.h>
#include <memory.h>
#ifndef NDEBUG
#include <stdio.h>
#endif
#include <stdlib.h>
#include <string.h>
#include "pbs_ifl.h"
#include "list_link.h"
#include "attribute.h"
#include "pbs_error.h"


/**
 * @file	attr_fn_str.c
 * @brief
 * 	This file contains functions for manipulating attributes of type string
 *
 * 	Then there are a set of functions for each type of attribute:
 *	string
 * @details
 * Each set has functions for:
 *	Decoding the value string to the internal representation.
 *	Encoding the internal attribute form to external form
 *	Setting the value by =, + or - operators.
 *	Comparing a (decoded) value with the attribute value.
 *
 * Some or all of the functions for an attribute type may be shared with
 * other attribute types.
 *
 * The prototypes are declared in "attribute.h"
 *
 * -------------------------------------------------
 * Set of general attribute functions for attributes
 * with value type "string"
 * -------------------------------------------------
 */

/**
 * @brief
 * 	decode_str - decode string into string attribute
 *
 * @param[in] patr - ptr to attribute to decode
 * @param[in] name - attribute name
 * @param[in] rescn - resource name or null
 * @param[out] val - string holding values for attribute structure
 *
 * @retval      int
 * @retval      0       if ok
 * @retval      >0      error number1 if error,
 * @retval      *patr   members set
 *
 */

int
decode_str(struct attribute *patr, char *name, char *rescn, char *val)
{
	size_t len;

	if ((patr->at_flags & ATR_VFLAG_SET) && (patr->at_val.at_str))
		(void)free(patr->at_val.at_str);

	if ((val != (char *)0) && ((len = strlen(val) + 1) > 1)) {
		patr->at_val.at_str = malloc((unsigned) len);
		if (patr->at_val.at_str == (char *)0)
			return (PBSE_SYSTEM);
		(void)strcpy(patr->at_val.at_str, val);
		patr->at_flags |= ATR_VFLAG_SET | ATR_VFLAG_MODIFY | ATR_VFLAG_MODCACHE;
	} else {
		patr->at_flags = (patr->at_flags & ~ATR_VFLAG_SET) |
			(ATR_VFLAG_MODIFY | ATR_VFLAG_MODCACHE);
		patr->at_val.at_str = (char *)0;
	}
	return (0);
}

/**
 * @brief
 * 	encode_str - encode attribute of type ATR_TYPE_STR into attr_extern
 *
 * @param[in] attr - ptr to attribute to encode
 * @param[in] phead - ptr to head of attrlist list
 * @param[in] atname - attribute name
 * @param[in] rsname - resource name or null
 * @param[in] mode - encode mode
 * @param[out] rtnl - ptr to svrattrl
 *
 * @retval      int
 * @retval      >0      if ok, entry created and linked into list
 * @retval      =0      no value to encode, entry not created
 * @retval      -1      if error
 *
 */

/*ARGSUSED*/

int
encode_str(attribute *attr, pbs_list_head *phead, char *atname, char *rsname, int mode, svrattrl **rtnl)

{
	svrattrl *pal;

	if (!attr)
		return (-1);
	if (!(attr->at_flags & ATR_VFLAG_SET) || !attr->at_val.at_str ||
		(*attr->at_val.at_str == '\0'))
		return (0);

	pal = attrlist_create(atname, rsname, (int)strlen(attr->at_val.at_str)+1);
	if (pal == (svrattrl *)0)
		return (-1);

	(void)strcpy(pal->al_value, attr->at_val.at_str);
	pal->al_flags = attr->at_flags;
	if (phead)
		append_link(phead, &pal->al_link, pal);
	if (rtnl)
		*rtnl = pal;

	return (1);
}

/**
 * @brief
 * 	set_str - set attribute value based upon another
 *
 *	A+B --> B is concatenated to end of A
 *	A=B --> A is replaced with B
 *	A-B --> If B is a substring at the end of A, it is stripped off
 *
 * @param[in]   attr - pointer to new attribute to be set (A)
 * @param[in]   new  - pointer to attribute (B)
 * @param[in]   op   - operator
 *
 * @return      int
 * @retval      0       if ok
 * @retval     >0       if error
 *
 */

int
set_str(struct attribute *attr, struct attribute *new, enum batch_op op)
{
	char	*new_value;
	char	*p;
	size_t nsize;

	assert(attr && new && new->at_val.at_str && (new->at_flags & ATR_VFLAG_SET));
	nsize = strlen(new->at_val.at_str) + 1;	/* length of new string */
	if ((op == INCR) && !attr->at_val.at_str)
		op = SET;	/* no current string, change INCR to SET */

	switch (op) {

		case SET:	/* set is replace old string with new */

			if (attr->at_val.at_str)
				(void)free(attr->at_val.at_str);
			if ((attr->at_val.at_str = malloc(nsize)) == (char *)0)
				return (PBSE_SYSTEM);
			(void)strcpy(attr->at_val.at_str, new->at_val.at_str);
			break;

		case INCR:	/* INCR is concatenate new to old string */

			nsize += strlen(attr->at_val.at_str);
			if (attr->at_val.at_str)
				new_value = realloc(attr->at_val.at_str, nsize);
			else
				new_value = malloc(nsize);
			if (new_value == (char *)0)
				return (PBSE_SYSTEM);
			attr->at_val.at_str = new_value;
			(void)strcat(attr->at_val.at_str, new->at_val.at_str);
			break;

		case DECR:	/* DECR is remove substring if match, start at end */

			if (!attr->at_val.at_str)
				break;

			if (--nsize == 0)
				break;
			p = attr->at_val.at_str + strlen(attr->at_val.at_str) - nsize;
			while (p >= attr->at_val.at_str) {
				if (strncmp(p, new->at_val.at_str, (int)nsize) == 0) {
					do {
						*p = *(p + nsize);
					} while (*p++);
				}
				p--;
			}
			break;

		default:	return (PBSE_INTERNAL);
	}
	if ((attr->at_val.at_str != (char *)0) && (*attr->at_val.at_str !='\0'))
		attr->at_flags |= ATR_VFLAG_SET | ATR_VFLAG_MODIFY | ATR_VFLAG_MODCACHE;
	else
		attr->at_flags &= ~ATR_VFLAG_SET;

	return (0);
}

/**
 * @brief
 * 	comp_str - compare two attributes of type ATR_TYPE_STR
 *
 * @param[in] attr - pointer to attribute structure
 * @param[in] with - pointer to attribute structure
 *
 * @return      int
 * @retval      0       if the set of strings in "with" is a subset of "attr"
 * @retval      1       otherwise
 *
 */

int
comp_str(struct attribute *attr, struct attribute *with)
{
	if (!attr || !attr->at_val.at_str)
		return (-1);
	return (strcmp(attr->at_val.at_str, with->at_val.at_str));
}

/**
 * @brief
 * 	free_str - free space malloc-ed for string attribute value
 *
 * @param[in] attr - pointer to attribute structure
 *
 * @return	Void
 *
 */

void
free_str(struct attribute *attr)
{
	if ((attr->at_flags & ATR_VFLAG_SET) && (attr->at_val.at_str)) {
		(void)free(attr->at_val.at_str);
	}
	free_null(attr);
	attr->at_val.at_str = (char *)0;
}

/**
 * @brief 
 *	Special function that verifies the size of the input
 * 	for jobname before calling decode_str
 *
 * @param[in] patr - attribute structure
 * @param[in] name - attribute name
 * @param[in] rescn - resource name - unused here
 * @param[in] val - attribute value
 *
 * @return  int
 * @retval  0 if 0k
 * @retval  >0 error number if error
 *
 */

int
decode_jobname(attribute *patr, char *name, char *rescn, char *val)
{

	if (val != (char *)0) {
		if (strlen(val) > (size_t)PBS_MAXJOBNAME)
			return (PBSE_BADATVAL);
	}
	return (decode_str(patr, name, rescn, val));
}
