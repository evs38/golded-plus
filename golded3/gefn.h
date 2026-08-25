
//  ------------------------------------------------------------------
//  GoldED+
//  Copyright (C) 1990-1999 Odinn Sorensen
//  Copyright (C) 1999-2000 Alexander S. Aganichev
//  ------------------------------------------------------------------
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License as
//  published by the Free Software Foundation; either version 2 of the
//  License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston,
//  MA 02111-1307 USA
//  ------------------------------------------------------------------
//  $Id$
//  ------------------------------------------------------------------
//  Standart filenames.
//  ------------------------------------------------------------------


#ifndef __GEFN_H__
#define __GEFN_H__

#ifndef GEDCFG
    #define GEDCFG "golded.cfg"
#endif /* GEDCFG */

#ifndef GEDCFG2

    #if defined(__OS2__)
        #define GEDCFG2 "ged2.cfg"
    #elif defined(__WIN32__)
        #define GEDCFG2 "gedw32.cfg"
    #elif defined(__DOS__) || defined(__MSDOS__)
        #define GEDCFG2 "geddos.cfg"
    #elif defined(__UNIX__)
        #define GEDCFG2 "golded.conf"
    #endif

#endif /* GEDCFG2 */

#ifndef GOLDAREA_INC
    #define GOLDAREA_INC "goldarea.inc"
#endif

#ifndef GOLDED_MSG
    #define GOLDED_MSG "golded.msg"
#endif

#ifndef GOLDHELP_CFG
    #define GOLDHELP_CFG "goldhelp.cfg"
#endif

#ifndef GOLDKEYS_CFG
    #define GOLDKEYS_CFG "goldkeys.cfg"
#endif

#ifndef GOLDLANG_CFG
    #define GOLDLANG_CFG "goldlang.cfg"
#endif

#ifndef GOLDLAST_LST
    #define GOLDLAST_LST "goldlast.lst"
#endif

#ifndef GOLDUSER_LST
    #define GOLDUSER_LST "golduser.lst"
#endif

#ifndef GOLDED_CFM
    #define GOLDED_CFM "golded.cfm"
#endif

#ifndef GOLDXLAT
    #define GOLDXLAT "goldxlat"
#endif

#ifndef GOLDED_LOG
    #define GOLDED_LOG "golded.log"
#endif

#ifndef NAMES_FD
    #define NAMES_FD "names.fd"
#endif

#ifndef GOLDED_LST
    #define GOLDED_LST "golded.lst"
#endif

#ifndef FIDOLASTREAD
    #define FIDOLASTREAD "lastread"
#endif

/*  Where golded.cfg is looked for when the command line and the GOLDED
 *  and GED environment variables named none. geinit.cpp walks these in
 *  order, the user's own configuration ahead of the machine's; every
 *  entry ends in a slash, because every entry is a directory.
 *
 *  On unix this is the XDG Base Directory specification: the user's
 *  configuration lives under $XDG_CONFIG_HOME, which is ~/.config when
 *  that is unset - geinit.cpp reads the variable, since expanding it
 *  here would leave "/golded/" behind when it is not set. The older
 *  ~/.golded stays behind it so setups that already exist go on
 *  working, together with the two FTN layouts GoldED has always looked
 *  in. XDG's system half is /etc/xdg, and it is honoured, but
 *  /etc/golded comes first: that is where FTN software keeps this.
 *
 *  Elsewhere each system has a convention of its own. BeOS and Haiku
 *  put settings in ~/config/settings and /boot/system/settings. Windows
 *  has two halves and they are not interchangeable: %APPDATA% roams
 *  with the user and is the one meant for settings, %LOCALAPPDATA% is
 *  machine-local and meant for caches - so APPDATA leads and the other
 *  is accepted behind it. OS/2 has no convention at all, so %HOME% it
 *  is, with %ETC% - which OS/2 does define - behind it.
 *
 *  DOS has neither a home directory nor an /etc, and gets neither list:
 *  there the program's own directory is the whole answer.
 */

#ifndef GOLD_CFG_USER_DIRS
    #if defined(__BEOS__)
        #define GOLD_CFG_USER_DIRS "~/config/settings/golded/"
    #elif defined(__OS2__)
        #define GOLD_CFG_USER_DIRS "%HOME%\\GoldED\\"
    #elif defined(__WIN32__)
        #define GOLD_CFG_USER_DIRS "%APPDATA%\\GoldED\\", \
                                   "%LOCALAPPDATA%\\GoldED\\"
    #elif defined(__UNIX__)
        #define GOLD_CFG_USER_DIRS "~/.config/golded/", "~/.golded/", \
                                   "~/fido/etc/", "~/ftn/etc/"
    #endif
#endif

#ifndef GOLD_CFG_SYSTEM_DIRS
    #if defined(__BEOS__)
        #define GOLD_CFG_SYSTEM_DIRS "/boot/system/settings/golded/"
    #elif defined(__OS2__)
        #define GOLD_CFG_SYSTEM_DIRS "%ETC%\\golded\\"
    #elif defined(__UNIX__)
        #define GOLD_CFG_SYSTEM_DIRS "/etc/golded/", "/etc/xdg/golded/"
    #endif
#endif

/*  The one the build system knows and the source cannot: sysconfdir,
 *  which is <prefix>/etc/golded unless the packager said otherwise.
 *  Passed in as -DGOLD_SYSCONFDIR="..."; absent, this step is skipped.
 */

/*  CFGUSERPATH1, CFGUSERPATH2 and CFGPATH were the whole search before
 *  the lists above existed. Anyone who defines one still gets it looked
 *  in, ahead of everything else - see geinit.cpp.
 */

#endif /* __GEFN_H__ */
