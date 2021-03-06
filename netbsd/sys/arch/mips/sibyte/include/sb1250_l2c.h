/*  *********************************************************************
    *  SB1250 Board Support Package
    *
    *  L2 Cache constants and macros		File: sb1250_l2c.h
    *
    *  This module contains constants useful for manipulating the
    *  level 2 cache.
    *
    *  SB1250 specification level:  0.2
    *
    *  Author:  Mitch Lichtenberg (mitch@sibyte.com)
    *
    *********************************************************************
    *
    *  Copyright 2000,2001
    *  Broadcom Corporation. All rights reserved.
    *
    *  This software is furnished under license and may be used and
    *  copied only in accordance with the following terms and
    *  conditions.  Subject to these conditions, you may download,
    *  copy, install, use, modify and distribute modified or unmodified
    *  copies of this software in source and/or binary form.  No title
    *  or ownership is transferred hereby.
    *
    *  1) Any source code used, modified or distributed must reproduce
    *     and retain this copyright notice and list of conditions as
    *     they appear in the source file.
    *
    *  2) No right is granted to use any trade name, trademark, or
    *     logo of Broadcom Corporation. Neither the "Broadcom
    *     Corporation" name nor any trademark or logo of Broadcom
    *     Corporation may be used to endorse or promote products
    *     derived from this software without the prior written
    *     permission of Broadcom Corporation.
    *
    *  3) THIS SOFTWARE IS PROVIDED "AS-IS" AND ANY EXPRESS OR
    *     IMPLIED WARRANTIES, INCLUDING BUT NOT LIMITED TO, ANY IMPLIED
    *     WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
    *     PURPOSE, OR NON-INFRINGEMENT ARE DISCLAIMED. IN NO EVENT
    *     SHALL BROADCOM BE LIABLE FOR ANY DAMAGES WHATSOEVER, AND IN
    *     PARTICULAR, BROADCOM SHALL NOT BE LIABLE FOR DIRECT, INDIRECT,
    *     INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    *     (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
    *     GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
    *     BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
    *     OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
    *     TORT (INCLUDING NEGLIGENCE OR OTHERWISE), EVEN IF ADVISED OF
    *     THE POSSIBILITY OF SUCH DAMAGE.
    ********************************************************************* */


#ifndef _SB1250_L2C_H
#define	_SB1250_L2C_H

#include "sb1250_defs.h"

/*
 * Level 2 Cache Tag register (Table 5-3)
 */

#define	S_L2C_TAG_MBZ               0
#define	M_L2C_TAG_MBZ               _SB_MAKEMASK(5,S_L2C_TAG_MBZ)

#define	S_L2C_TAG_INDEX             5
#define	M_L2C_TAG_INDEX             _SB_MAKEMASK(12,S_L2C_TAG_INDEX)
#define	V_L2C_TAG_INDEX(x)          _SB_MAKEVALUE(x,S_L2C_TAG_INDEX)
#define	G_L2C_TAG_INDEX(x)          _SB_GETVALUE(x,S_L2C_TAG_INDEX,M_L2C_TAG_INDEX)

#define	S_L2C_TAG_TAG               17
#define	M_L2C_TAG_TAG               _SB_MAKEMASK(23,S_L2C_TAG_TAG)
#define	V_L2C_TAG_TAG(x)            _SB_MAKEVALUE(x,S_L2C_TAG_TAG)
#define	G_L2C_TAG_TAG(x)            _SB_GETVALUE(x,S_L2C_TAG_TAG,M_L2C_TAG_TAG)

#define	S_L2C_TAG_ECC               40
#define	M_L2C_TAG_ECC               _SB_MAKEMASK(6,S_L2C_TAG_ECC)
#define	V_L2C_TAG_ECC(x)            _SB_MAKEVALUE(x,S_L2C_TAG_ECC)
#define	G_L2C_TAG_ECC(x)            _SB_GETVALUE(x,S_L2C_TAG_ECC,M_L2C_TAG_ECC)

#define	S_L2C_TAG_WAY               46
#define	M_L2C_TAG_WAY               _SB_MAKEMASK(2,S_L2C_TAG_WAY)
#define	V_L2C_TAG_WAY(x)            _SB_MAKEVALUE(x,S_L2C_TAG_WAY)
#define	G_L2C_TAG_WAY(x)            _SB_GETVALUE(x,S_L2C_TAG_WAY,M_L2C_TAG_WAY)

#define	M_L2C_TAG_DIRTY             _SB_MAKEMASK1(48)
#define	M_L2C_TAG_VALID             _SB_MAKEMASK1(49)

/*
 * Format of level 2 cache management address (table 5-2)
 */

#define	S_L2C_MGMT_INDEX            5
#define	M_L2C_MGMT_INDEX            _SB_MAKEMASK(12,S_L2C_MGMT_INDEX)
#define	V_L2C_MGMT_INDEX(x)         _SB_MAKEVALUE(x,S_L2C_MGMT_INDEX)
#define	G_L2C_MGMT_INDEX(x)         _SB_GETVALUE(x,S_L2C_MGMT_INDEX,M_L2C_MGMT_INDEX)

#define	S_L2C_MGMT_WAY              17
#define	M_L2C_MGMT_WAY              _SB_MAKEMASK(2,S_L2C_MGMT_WAY)
#define	V_L2C_MGMT_WAY(x)           _SB_MAKEVALUE(x,S_L2C_MGMT_WAY)
#define	G_L2C_MGMT_WAY(x)           _SB_GETVALUE(x,S_L2C_MGMT_WAY,M_L2C_MGMT_WAY)

#define	S_L2C_MGMT_TAG              21
#define	M_L2C_MGMT_TAG              _SB_MAKEMASK(6,S_L2C_MGMT_TAG)
#define	V_L2C_MGMT_TAG(x)           _SB_MAKEVALUE(x,S_L2C_MGMT_TAG)
#define	G_L2C_MGMT_TAG(x)           _SB_GETVALUE(x,S_L2C_MGMT_TAG,M_L2C_MGMT_TAG)

#define	M_L2C_MGMT_DIRTY            _SB_MAKEMASK1(19)
#define	M_L2C_MGMT_VALID            _SB_MAKEMASK1(20)

#define	A_L2C_MGMT_TAG_BASE         0x00D0000000

#define	L2C_ENTRIES_PER_WAY       4096
#define	L2C_NUM_WAYS              4

#endif
