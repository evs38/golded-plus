
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
//  Menus.
//  ------------------------------------------------------------------

#include <golded.h>
#include <grecode.h>
#include <gutf8.h>

//  ------------------------------------------------------------------

GMsg* MenuMsgPtr;


//  ------------------------------------------------------------------

void DispHeadAttrs(GMsg* msg)
{

    CREATEBUFFER(char, atrs, MAXCOL+1);
    MakeAttrStr(atrs, sizeof(atrs), &msg->attr);
    strsetsz(atrs, MAXCOL-CFG->disphdrnodeset.pos);

    HeaderView->window.prints(1, CFG->disphdrnodeset.pos, HeaderView->window_color, atrs);
    HeaderView->window.prints(4, 0, HeaderView->window_color, (msg->attr.att() or msg->attr.frq() or msg->attr.urq()) ? LNG->File : LNG->Subj);
}


//  ------------------------------------------------------------------

static void DispHeadAttrs()
{

    DispHeadAttrs(MenuMsgPtr);
}


//  ------------------------------------------------------------------

static void toggle_pvt()
{

    MenuMsgPtr->attr.pvtX();
    DispHeadAttrs();
}


//  ------------------------------------------------------------------

static void toggle_received()
{

    MenuMsgPtr->attr.rcvX();
    DispHeadAttrs();
}


//  ------------------------------------------------------------------

static void toggle_crash()
{

    MenuMsgPtr->attr.craX();
    if(MenuMsgPtr->attr.cra())
        MenuMsgPtr->attr.hld0();
    DispHeadAttrs();
}


//  ------------------------------------------------------------------

static void toggle_hold()
{

    MenuMsgPtr->attr.hldX();
    if(MenuMsgPtr->attr.hld())
    {
        MenuMsgPtr->attr.cra0();
        MenuMsgPtr->attr.imm0();
    }
    DispHeadAttrs();
}


//  ------------------------------------------------------------------

static void toggle_file()
{

    MenuMsgPtr->attr.attX();
    if(MenuMsgPtr->attr.att())
    {
        AttrAdd(&MenuMsgPtr->attr, &CFG->attribsattach);
        MenuMsgPtr->attr.urq0();
        MenuMsgPtr->attr.frq0();
    }
    DispHeadAttrs();
}


//  ------------------------------------------------------------------

static void toggle_freq()
{

    MenuMsgPtr->attr.frqX();
    if(MenuMsgPtr->attr.frq())
    {
        MenuMsgPtr->attr.att0();
        MenuMsgPtr->attr.urq0();
    }
    DispHeadAttrs();
}


//  ------------------------------------------------------------------

static void toggle_updreq()
{

    MenuMsgPtr->attr.urqX();
    if(MenuMsgPtr->attr.urq())
    {
        MenuMsgPtr->attr.att0();
        MenuMsgPtr->attr.frq0();
    }
    DispHeadAttrs();
}


//  ------------------------------------------------------------------

static void toggle_kill()
{

    MenuMsgPtr->attr.k_sX();
    if(MenuMsgPtr->attr.k_s())
        MenuMsgPtr->attr.a_s0();
    DispHeadAttrs();
}


//  ------------------------------------------------------------------

static void toggle_sent()
{

    MenuMsgPtr->attr.sntX();
    if(MenuMsgPtr->attr.snt())
    {
        MenuMsgPtr->attr.uns0();
        MenuMsgPtr->attr.scn1();
    }
    else
    {
        MenuMsgPtr->attr.uns1();
        MenuMsgPtr->attr.scn0();
        MenuMsgPtr->attr.loc1();
    }

    DispHeadAttrs();
}


//  ------------------------------------------------------------------

static void toggle_trunc()
{

    MenuMsgPtr->attr.tfsX();
    if(MenuMsgPtr->attr.tfs())
        MenuMsgPtr->attr.kfs0();
    DispHeadAttrs();
}


//  ------------------------------------------------------------------

static void toggle_delsent()
{

    MenuMsgPtr->attr.kfsX();
    if(MenuMsgPtr->attr.kfs())
        MenuMsgPtr->attr.tfs0();
    DispHeadAttrs();
}


//  ------------------------------------------------------------------

static void toggle_direct()
{

    MenuMsgPtr->attr.dirX();
    if(MenuMsgPtr->attr.dir())
    {
        MenuMsgPtr->attr.zon0();
        MenuMsgPtr->attr.hub0();
    }
    DispHeadAttrs();
}


//  ------------------------------------------------------------------

static void toggle_imm()
{

    MenuMsgPtr->attr.immX();
    if(MenuMsgPtr->attr.imm())
        MenuMsgPtr->attr.hld0();
    DispHeadAttrs();
}


//  ------------------------------------------------------------------

static void toggle_locked()
{

    MenuMsgPtr->attr.lokX();
    DispHeadAttrs();
}


//  ------------------------------------------------------------------

static void toggle_reserved()
{

    MenuMsgPtr->attr.rsvX();
    DispHeadAttrs();
}


//  ------------------------------------------------------------------

static void toggle_groupmsg()
{

    MenuMsgPtr->attr.grpX();
    DispHeadAttrs();
}


//  ------------------------------------------------------------------

static void toggle_arcsent()
{

    MenuMsgPtr->attr.a_sX();
    if(MenuMsgPtr->attr.a_s())
        MenuMsgPtr->attr.k_s0();
    DispHeadAttrs();
}


//  ------------------------------------------------------------------

static void toggle_zonegate()
{

    MenuMsgPtr->attr.zonX();
    if(MenuMsgPtr->attr.zon())
    {
        MenuMsgPtr->attr.dir0();
        MenuMsgPtr->attr.hub0();
    }
    DispHeadAttrs();
}


//  ------------------------------------------------------------------

static void toggle_transit()
{

    MenuMsgPtr->attr.trsX();
    DispHeadAttrs();
}


//  ------------------------------------------------------------------

static void toggle_retrecreq()
{

    MenuMsgPtr->attr.rrqX();
    DispHeadAttrs();
}


//  ------------------------------------------------------------------

static void toggle_retrec()
{

    MenuMsgPtr->attr.rrcX();
    DispHeadAttrs();
}


//  ------------------------------------------------------------------

static void toggle_orphan()
{

    MenuMsgPtr->attr.orpX();
    DispHeadAttrs();
}


//  ------------------------------------------------------------------

static void toggle_audit()
{

    MenuMsgPtr->attr.arqX();
    DispHeadAttrs();
}


//  ------------------------------------------------------------------

static void toggle_hubhost()
{

    MenuMsgPtr->attr.hubX();
    if(MenuMsgPtr->attr.hub())
    {
        MenuMsgPtr->attr.dir0();
        MenuMsgPtr->attr.zon0();
    }
    DispHeadAttrs();
}


//  ------------------------------------------------------------------

static void toggle_local()
{

    MenuMsgPtr->attr.locX();
    if(MenuMsgPtr->attr.loc())
        MenuMsgPtr->attr.trs0();
    DispHeadAttrs();
}


//  ------------------------------------------------------------------

static void toggle_xmail()
{

    MenuMsgPtr->attr.xmaX();
    DispHeadAttrs();
}


//  ------------------------------------------------------------------

static void toggle_cfmrecreq()
{

    MenuMsgPtr->attr.cfmX();
    DispHeadAttrs();
}


//  ------------------------------------------------------------------

static void toggle_scanned()
{

    MenuMsgPtr->attr.scnX();
    if(MenuMsgPtr->attr.scn())
    {
        MenuMsgPtr->attr.snt1();
        MenuMsgPtr->attr.uns0();
    }
    else
    {
        MenuMsgPtr->attr.snt0();
        MenuMsgPtr->attr.uns1();
        MenuMsgPtr->attr.loc1();
    }
    DispHeadAttrs();
}


//  ------------------------------------------------------------------

static void clear_attrib()
{

    MenuMsgPtr->attr.reset();
    DispHeadAttrs();
}


//  ------------------------------------------------------------------

static void DispAttrWindow(int show=-1)
{

    static int wh_background = -1;
    static int wh_attributes = -1;

    if(show == -1)
        show = wh_attributes == -1;

    if(show)
    {
        wh_background = whandle();
        size_t wide = MaxV(g_utf8_width(LNG->AttrTitle)+2, g_utf8_width(LNG->AttrPvt)+2);
        wide = MinV(wide, MAXCOL-4);
        wh_attributes = wopen_(6, 0, 17, wide, W_BMENU, C_MENUB, C_MENUW);
        set_title(LNG->AttrTitle, TCENTER, C_MENUT);
        if(*LNG->AttrTurnOff)
            wtitle(LNG->AttrTurnOff, TCENTER|TBOTTOM, C_MENUT);
        title_shadow();
        int n = 0;
        wide -= 2;
        wprintns(n++, 0, C_MENUW, LNG->AttrPvt, wide, ' ', C_MENUW);
        wprintns(n++, 0, C_MENUW, LNG->AttrRcv, wide, ' ', C_MENUW);
        wprintns(n++, 0, C_MENUW, LNG->AttrSnt, wide, ' ', C_MENUW);
        wprintns(n++, 0, C_MENUW, LNG->AttrCrs, wide, ' ', C_MENUW);
        wprintns(n++, 0, C_MENUW, LNG->AttrHld, wide, ' ', C_MENUW);
        wprintns(n++, 0, C_MENUW, LNG->AttrFil, wide, ' ', C_MENUW);
        wprintns(n++, 0, C_MENUW, LNG->AttrFrq, wide, ' ', C_MENUW);
        wprintns(n++, 0, C_MENUW, LNG->AttrUpd, wide, ' ', C_MENUW);
        wprintns(n++, 0, C_MENUW, LNG->AttrKS, wide, ' ', C_MENUW);
        wprintns(n++, 0, C_MENUW, LNG->AttrKfs, wide, ' ', C_MENUW);
        wprintns(n++, 0, C_MENUW, LNG->AttrTfs, wide, ' ', C_MENUW);
        wprintns(n++, 0, C_MENUW, LNG->AttrDir, wide, ' ', C_MENUW);
        wprintns(n++, 0, C_MENUW, LNG->AttrImm, wide, ' ', C_MENUW);
        wprintns(n++, 0, C_MENUW, LNG->AttrLok, wide, ' ', C_MENUW);
        wprintns(n++, 0, C_MENUW, LNG->AttrZap, wide, ' ', C_MENUW);
        wactiv_(wh_background);
    }
    else if(wh_attributes != -1)
    {
        wactiv_(wh_attributes);
        wclose();
        wactiv_(wh_background);
        wh_attributes = -1;
    }
}


//  ------------------------------------------------------------------

static void toggle_dispattrwindow()
{

    DispAttrWindow();
}


//  ------------------------------------------------------------------

bool ProcessAttrs(gkey &key)
{

    switch(key)
    {
    case KK_HeaderToggleScanned:
        toggle_scanned();
        return true;

    case KK_HeaderToggleGroupmsg:
        toggle_groupmsg();
        return true;

    case KK_HeaderToggleZonegate:
        toggle_zonegate();
        return true;

    case KK_HeaderToggleHubhost:
        toggle_hubhost();
        return true;

    case KK_HeaderToggleRetrecreq:
        toggle_retrecreq();
        return true;

    case KK_HeaderToggleCrash:
        toggle_crash();
        return true;

    case KK_HeaderToggleLocked:
        toggle_locked();
        return true;

    case KK_HeaderToggleReceived:
        toggle_received();
        return true;

    case KK_HeaderToggleLocal:
        toggle_local();
        return true;

    case KK_HeaderToggleRetrec:
        toggle_retrec();
        return true;

    case KK_HeaderToggleFreq:
        toggle_freq();
        return true;

    case KK_HeaderToggleImm:
        toggle_imm();
        return true;

    case KK_HeaderToggleAttrWin:
        toggle_dispattrwindow();
        return true;

    case KK_HeaderToggleArcsent:
        toggle_arcsent();
        return true;

    case KK_HeaderToggleHold:
        toggle_hold();
        return true;

    case KK_HeaderToggleAudit:
        toggle_audit();
        return true;

    case KK_HeaderToggleXmail:
        toggle_xmail();
        return true;

    case KK_HeaderToggleTrunc:
        toggle_trunc();
        return true;

    case KK_HeaderToggleUpdreq:
        toggle_updreq();
        return true;

    case KK_HeaderClearAttrib:
        clear_attrib();
        return true;

    case KK_HeaderToggleKill:
        toggle_kill();
        return true;

    case KK_HeaderToggleTransit:
        toggle_transit();
        return true;

    case KK_HeaderToggleCfmrecreq:
        toggle_cfmrecreq();
        return true;

    case KK_HeaderToggleOrphan:
        toggle_orphan();
        return true;

    case KK_HeaderToggleFile:
        toggle_file();
        return true;

    case KK_HeaderToggleDelsent:
        toggle_delsent();
        return true;

    case KK_HeaderToggleDirect:
        toggle_direct();
        return true;

    case KK_HeaderToggleReserved:
        toggle_reserved();
        return true;

    case KK_HeaderTogglePvt:
        toggle_pvt();
        return true;

    case KK_HeaderToggleSent:
        toggle_sent();
        return true;
    }
    return false;
}

//  ------------------------------------------------------------------

void ChgAttrs(int mode, GMsg* __msg)
{

    if(mode)
    {
        MenuMsgPtr = __msg;

        if(EDIT->HeaderAttrs() or (mode == ALWAYS))
            DispAttrWindow(true);
    }
    else
    {
        DispAttrWindow(false);
    }
}


//  ------------------------------------------------------------------

void AskAttributes(GMsg* __msg)
{

    ChgAttrs(ALWAYS, __msg);
    update_statusline(LNG->ChangeAttrs);
    whelppcat(H_Attributes);

    gkey key;
    do
    {
        gkey kk;

        key = getxch();
        if(key < KK_Commands)
        {
            key = key_tolower(key);
            kk = SearchKey(key, HeaderKey, HeaderKeys);
            if(kk)
                key = kk;
        }
    }
    while(ProcessAttrs(key) == true);

    whelpop();
    ChgAttrs(NO, __msg);
}


//  ------------------------------------------------------------------

int SelectFromFile(const char* file, char* selection, const char* title, const char* nolines)
{
    char buf[256];
    bool retval=false;
    int lines = 0;

    gfile fp(AddPath(CFG->goldpath, file), "rt", CFG->sharemode);
    if (fp.isopen())
    {
        while (fp.Fgets(buf, sizeof(buf)))
            lines++;
    }

    if (lines)
    {
        gstrarray Listi;
        fp.Rewind();

        size_t n;
        for (n = 0; n < lines; n++)
        {
            fp.Fgets(buf, sizeof(buf)-2);
            //  A tagline or origin file is one of GoldED+'s own, in
            //  the charset XLATCONFIGSET names; unconverted, its lines
            //  showed as mojibake and the picked one went into the
            //  message that way. GetRandomLine() already converts -
            //  the picker read the same file raw.
            XlatCfgLine(buf, sizeof(buf)-2);
            strtrim(buf);
            strins(" ", buf, 0);
            strcat(buf, " ");
            //  The list is as wide as the screen, so cut by columns.
            //  Cutting at a byte offset shortened multibyte text several
            //  times too much and could split a character in half.
            if(g_utf8_width(buf) > (size_t)(MAXCOL-2-2))
                strxcpy(buf, g_utf8_truncate(buf, (size_t)(MAXCOL-2-2)).c_str(),
                        sizeof(buf));
            Listi.push_back(buf);
        }

        n = MinV(n, (MAXROW-10));
        set_title(title, TCENTER, C_ASKT);
        n = wpickstr(6, 0, 6+n+1, -1, W_BASK, C_ASKB, C_ASKW, C_ASKS, Listi, 0, title_shadow);

        if (n != -1)
        {
            //  Both callers hand a 1024-byte buffer; the line itself
            //  is at most a screenful.
            strxcpy_utf8(selection, Listi[n].c_str(), 256*4);
            strtrim(selection);
            strltrim(selection);
            retval = true;
        }
    }
    else
    {
        w_info(nolines);
        waitkeyt(10000);
        w_info(NULL);
    }

    return retval;
}


//  ------------------------------------------------------------------

int ChangeTagline()
{
    bool retval = false;

    if (not CFG->tagline.empty())
    {
        char buf[256*4];
        gstrarray Listi;

        gstrarray::iterator it = CFG->tagline.begin();
        gstrarray::iterator end = CFG->tagline.end();

        for (; it != end; it++)
        {
            if((*it)[0] == '@')
                strxmerge(buf, sizeof(buf), " [", CleanFilename(it->c_str() + 1), "] ", NULL);
            else
                strxmerge(buf, sizeof(buf), " ", it->c_str(), " ", NULL);

            //  What fits on the line is a number of columns; strxmerge
            //  counts bytes, and bounding it by MAXCOL cut a Russian
            //  tagline to half the width the menu had for it.
            buf[g_utf8_bytes_for_cols(buf, (size_t)(MAXCOL-2-2))] = NUL;

            Listi.push_back(buf);
        }

        size_t n = MinV(Listi.size(), (MAXROW-10));
        set_title(LNG->Taglines, TCENTER, C_ASKT);
        update_statusline(LNG->ChangeTagline);
        whelppcat(H_ChangeTagline);
        n = wpickstr(6, 0, 6+n+1, -1, W_BASK, C_ASKB, C_ASKW, C_ASKS, Listi, CFG->taglineno, title_shadow);

        if (n != -1)
        {
            const char *tagl = CFG->tagline[n].c_str();
            if(tagl[0] == '@')
            {
                strxmerge(buf, sizeof(buf), LNG->Taglines, " [", CleanFilename(tagl+1), "] ", NULL);
                buf[g_utf8_bytes_for_cols(buf, (size_t)(MAXCOL-2-2))] = NUL;
                if(SelectFromFile(tagl+1, buf, LNG->Taglines, LNG->NoTagline))
                {
                    AA->SetTagline(buf);
                    retval = true;
                }
            }
            else
            {
                CFG->taglineno = n;
                AA->SetTagline(CFG->tagline[n].c_str());
                retval = true;
            }
        }

        whelpop();
    }
    else
    {
        w_info(LNG->NoTagline);
        waitkeyt(10000);
        w_info(NULL);
    }

    return retval;
}


//  ------------------------------------------------------------------

int ChangeOrigin()
{
    bool retval = false;

    if (not CFG->origin.empty())
    {
        char buf[256*4];
        gstrarray Listi;

        gstrarray::iterator it = CFG->origin.begin();
        gstrarray::iterator end = CFG->origin.end();

        for (; it !=end; it++)
        {
            if ((*it)[0] == '@')
                strxmerge(buf, sizeof(buf), " [", CleanFilename(it->c_str() + 1), "] ", NULL);
            else
                strxmerge(buf, sizeof(buf), " ", it->c_str(), " ", NULL);

            //  Columns, not bytes, like the tagline picker above.
            buf[g_utf8_bytes_for_cols(buf, (size_t)(MAXCOL-2-2))] = NUL;

            Listi.push_back(buf);
        }

        size_t n = MinV(Listi.size(), (MAXROW-10));
        set_title(LNG->Origins, TCENTER, C_ASKT);
        update_statusline(LNG->ChangeOrigin);
        whelppcat(H_ChangeOrigin);
        n = wpickstr(6, 0, 6+n+1, -1, W_BASK, C_ASKB, C_ASKW, C_ASKS, Listi, CFG->originno, title_shadow);

        if (n != -1)
        {
            const char *orig = CFG->origin[n].c_str();
            if(orig[0] == '@')
            {
                strxmerge(buf, sizeof(buf), LNG->Origins, " [", CleanFilename(orig+1), "] ", NULL);
                buf[g_utf8_bytes_for_cols(buf, (size_t)(MAXCOL-2-2))] = NUL;
                if(SelectFromFile(orig+1, buf, LNG->Origins, LNG->NoOrigDefined))
                {
                    AA->SetOrigin(buf);
                    retval = true;
                }
            }
            else
            {
                CFG->originno = n;
                AA->SetOrigin(orig);
                retval = true;
            }
        }

        whelpop();
    }
    else
    {
        w_info(LNG->NoOrigDefined);
        waitkeyt(10000);
        w_info(NULL);
    }

    return retval;
}


//  ------------------------------------------------------------------

int ChangeUsername()
{
    if(not CFG->username.empty())
    {
        char buf[256];
        char adrs[40];
        gstrarray Listi;

        std::vector<Node>::iterator it = CFG->username.begin();
        std::vector<Node>::iterator end = CFG->username.end();

        for (; it != end; it++)
        {
            it->addr.make_string(adrs);
            gsprintf(PRINTF_DECLARE_BUFFER(buf), " %s %s ", g_utf8_fit(it->name, 35).c_str(), adrs);
            Listi.push_back(buf);
        }

        size_t n = MinV(Listi.size(), (MAXROW-10));
        set_title(LNG->Usernames, TCENTER, C_ASKT);
        update_statusline(LNG->ChangeUsername);
        whelppcat(H_ChangeUsername);
        n = wpickstr(6, 0, 6+n+1, -1, W_BASK, C_ASKB, C_ASKW, C_ASKS, Listi, CFG->usernameno, title_shadow);

        if (n != -1)
        {
            CFG->usernameno = n;
            AA->SetUsername(CFG->username[n]);
            for(std::vector<gaka>::iterator a = CFG->aka.begin(); a != CFG->aka.end(); a++)
            {
                if(AA->Username().addr.match(a->addr))
                {
                    AA->SetAka(a->addr);
                    break;
                }
            }
        }

        whelpop();
    }
    else
    {
        w_info(LNG->NoUserDefined);
        waitkeyt(10000);
        w_info(NULL);
    }

    return YES;
}


//  ------------------------------------------------------------------

int ChangeTemplate()
{
    if (not CFG->tpl.empty())
    {
        char buf[256];
        char adrs[40];
        gstrarray Listi;

        std::vector<Tpl>::iterator it = CFG->tpl.begin();
        std::vector<Tpl>::iterator end = CFG->tpl.end();

        for (; it != end; it++)
        {
            it->match.make_string(adrs);
            gsprintf(PRINTF_DECLARE_BUFFER(buf), " %s %s ", g_utf8_fit(it->name, 45).c_str(), adrs);
            Listi.push_back(buf);
        }

        size_t n = MinV(Listi.size(), (MAXROW-10));
        set_title(LNG->Templates, TCENTER, C_ASKT);
        update_statusline(LNG->ChangeTemplate);
        whelppcat(H_ChangeTemplate);
        n = wpickstr(6, 0, 6+n+1, -1, W_BASK, C_ASKB, C_ASKW, C_ASKS, Listi, CFG->tplno, title_shadow);
        whelpop();

        if (n != -1)
        {
            AA->SetTpl(CFG->tpl[n].file);
            CFG->tplno = n;
        }

        return n;
    }
    else
    {
        w_info(LNG->NoTplDefined);
        waitkeyt(10000);
        w_info(NULL);
    }

    return -1;
}


//  ------------------------------------------------------------------

int ChangeAka()
{
    if (CFG->aka.size() > 1)
    {
        size_t startat = 0;
        char addr[100];
        char buf[100];
        gstrarray Listi;

        std::vector<gaka>::iterator it = CFG->aka.begin();
        std::vector<gaka>::iterator end = CFG->aka.end();

        for (; it != end; it++)
        {
            it->addr.make_string(addr, it->domain);
            gsprintf(PRINTF_DECLARE_BUFFER(buf), " %s ", addr);
            Listi.push_back(buf);

            if (AA->Aka().addr.equals(it->addr))
                startat = Listi.size() - 1;
        }

        size_t n = MinV(Listi.size(), (MAXROW-10));
        set_title(LNG->Akas, TCENTER, C_ASKT);
        update_statusline(LNG->ChangeAka);
        whelppcat(H_ChangeAka);
        n = wpickstr(6, 0, 6+n+1, -1, W_BASK, C_ASKB, C_ASKW, C_ASKS, Listi, startat, title_shadow);
        whelpop();

        if (n != -1)
            AA->SetAka(CFG->aka[n].addr);
    }
    else
    {
        w_info(LNG->NoAkaDefined);
        waitkeyt(10000);
        w_info(NULL);
    }
    return(YES);
}


//  ------------------------------------------------------------------

//  ------------------------------------------------------------------
//  Build the charset list for the "change charset" menus out of what
//  the recoder can do, for the case where no .chs tables are configured
//  - which is the normal state of affairs once iconv is doing the work.
//
//  The entries have the same shape as the table-derived ones, because
//  the caller picks the charset back out with tokenize(): the name it
//  wants is the first word of the line either way. Reading, that is the
//  charset a message is in; writing, the one it is to be written in -
//  so the list runs the other way round and the name stays in front.

//  Whether a charset is worth putting in the menu at all.
//
//  Naming one is not the same as being able to convert it: iconv, the
//  Win32 codepage API and OS/2's ULS each know a different set, and a
//  build with none of them has only the compiled-in tables. Offering a
//  charset the session cannot convert would give the user an entry that
//  quietly does nothing, so each one is tried before it is listed.

static bool XlatOffered(const char* name, bool importing)
{
    //  The session's own charset belongs in the list like any other,
    //  even though converting it to itself does nothing. Reading, it is
    //  how one says "this message is in my charset, leave the bytes
    //  alone" - and without it there is no way back from a charset
    //  chosen by hand, since Auto returns to what the message's own
    //  CHRS says, the very thing being overridden. Writing, it is how
    //  one says "send it as it stands".
    GRecoder probe;
    return importing ? probe.open(name, CFG->xlatlocalset)
                     : probe.open(CFG->xlatlocalset, name);
}


static void BuiltinXlatList(gstrarray& list, bool importing)
{
    char buf[100];
    char ftn[64];
    char local[64];

    //  The names shown are the ones FTS-5003 uses, which is what a CHRS
    //  kludge carries and therefore what the reader recognises: LATIN-1
    //  rather than the ISO-8859-1 this program calls it by, ASCII
    //  rather than US-ASCII, CP10000 rather than MACINTOSH. What the
    //  picked line puts into XLATIMPORT is the same word, and the
    //  recoder resolves it back, so nothing downstream needs to know.
    g_charset_ftn(CFG->xlatlocalset, local, sizeof(local), NULL);

    size_t width = 0;
    size_t n;                   // one declaration: Visual C++ 6.0 lets a
                                // for-scoped variable outlive its loop
    for (n = 0; n < g_charset_count(); n++)
    {
        if (XlatOffered(g_charset_name(n), importing))
        {
            g_charset_ftn(g_charset_name(n), ftn, sizeof(ftn), NULL);
            width = MaxV(width, strlen(ftn));
        }
    }

    for (n = 0; n < g_charset_count(); n++)
    {
        const char* name = g_charset_name(n);
        if (not XlatOffered(name, importing))
            continue;

        g_charset_ftn(name, ftn, sizeof(ftn), NULL);

        if (importing)
            gsprintf(PRINTF_DECLARE_BUFFER(buf), " %*s -> %s ",
                     (int)width, ftn, local);
        else
            gsprintf(PRINTF_DECLARE_BUFFER(buf), " %*s <- %s ",
                     (int)width, ftn, local);
        list.push_back(buf);
    }
}


//  ------------------------------------------------------------------

int ChangeXlatImport()
{
    if (CFG->xlatcharsets.empty())
    {
        //  No translation tables: offer what the recoder knows instead
        //  of telling the user there is nothing to choose from.
        gstrarray Listi;
        Listi.push_back(LNG->CharsetAuto);
        BuiltinXlatList(Listi, true);

        size_t startat = 0;
        size_t n;               // see BuiltinXlatList() above
        for (n = 1; n < Listi.size(); n++)
        {
            gstrarray parts;
            tokenize(parts, Listi[n].c_str());
            if (not parts.empty() and GRecoder::same(parts[0].c_str(), AA->Xlatimport()))
            {
                startat = n;
                break;
            }
        }

        n = MinV(Listi.size(), (size_t)(MAXROW-10));
        set_title(LNG->Charsets, TCENTER, C_ASKT);
        update_statusline(LNG->ChangeXlatImp);
        whelppcat(H_ChangeXlatImport);
        n = wpickstr(6, 0, 6+n+1, -1, W_BASK, C_ASKB, C_ASKW, C_ASKS, Listi, startat, title_shadow);
        whelpop();

        if (n == 0)
        {
            CFG->ignorecharset = false;
        }
        else if (n != (size_t)-1)
        {
            CFG->ignorecharset = true;
            gstrarray xlat;
            tokenize(xlat, Listi[n].c_str());
            if (not xlat.empty())
                AA->SetXlatimport(xlat[0].c_str());
        }

        LoadCharset(AA->Xlatimport(), CFG->xlatlocalset);
        return true;
    }

    if (not CFG->xlatcharsets.empty())
    {
        size_t startat = 0;
        int maximport = 0;
        int maxexport = 0;

        char buf[100];
        gstrarray Listi;

        ChrsMap::iterator xlt = CFG->xlatcharsets.begin();
        ChrsMap::iterator end = CFG->xlatcharsets.end();

        for (size_t xlatimports = 1; xlt != end; xlt++)
        {
            if (strieql((*xlt).first.second.c_str(), CFG->xlatlocalset))
            {
                maximport = MaxV(maximport, (int)(*xlt).first.first.size());
                maxexport = MaxV(maxexport, (int)(*xlt).first.second.size());
                if ((CFG->ignorecharset == true) and strieql((*xlt).first.first.c_str(), AA->Xlatimport()))
                    startat = xlatimports;
                xlatimports++;
            }
        }

        //  Is one of the tables the session's own charset read as
        //  itself? If not, the entry is added at the end: see
        //  XlatOffered() above for why the list needs it. The widths
        //  have to allow for the name, since the format truncates.
        bool haslocal = false;
        for (xlt = CFG->xlatcharsets.begin(); xlt != end; ++xlt)
        {
            if (strieql((*xlt).first.first.c_str(), CFG->xlatlocalset)
                and strieql((*xlt).first.second.c_str(), CFG->xlatlocalset))
                haslocal = true;
        }
        if (not haslocal)
        {
            maximport = MaxV(maximport, (int)strlen(CFG->xlatlocalset));
            maxexport = MaxV(maxexport, (int)strlen(CFG->xlatlocalset));
        }

        Listi.push_back(LNG->CharsetAuto);

        for (xlt = CFG->xlatcharsets.begin(); xlt != end; ++xlt)
        {
            if (strieql((*xlt).first.second.c_str(), CFG->xlatlocalset))
            {
                gsprintf(PRINTF_DECLARE_BUFFER(buf), " %*.*s -> %-*.*s ",
                         maximport, maximport, (*xlt).first.first.c_str(), maxexport, maxexport, (*xlt).first.second.c_str());
                Listi.push_back(buf);
            }
        }

        if (not haslocal)
        {
            gsprintf(PRINTF_DECLARE_BUFFER(buf), " %*.*s -> %-*.*s ",
                     maximport, maximport, CFG->xlatlocalset, maxexport, maxexport, CFG->xlatlocalset);
            Listi.push_back(buf);
            if (CFG->ignorecharset and strieql(AA->Xlatimport(), CFG->xlatlocalset))
                startat = Listi.size() - 1;
        }

        size_t n = MinV(Listi.size(), (MAXROW-10));
        set_title(LNG->Charsets, TCENTER, C_ASKT);
        update_statusline(LNG->ChangeXlatImp);
        whelppcat(H_ChangeXlatImport);
        n = wpickstr(6, 0, 6+n+1, -1, W_BASK, C_ASKB, C_ASKW, C_ASKS, Listi, startat, title_shadow);
        whelpop();

        if (n == 0)
        {
            CFG->ignorecharset = false;
        }
        else if (n != -1)
        {
            CFG->ignorecharset = true;
            gstrarray xlat;
            tokenize(xlat, Listi[n].c_str());
            AA->SetXlatimport(xlat[0].c_str());
        }

        LoadCharset(AA->Xlatimport(), CFG->xlatlocalset);
    }
    //  (The no-tables answer now lives in the picker above, built from
    //  the recoder's own list; the old message became unreachable.)

    return true;
}


//  ------------------------------------------------------------------
//  The export charset chosen by hand, and the area it was chosen in.
//
//  It belongs to the echo in front of the reader, not to the session.
//  SetActiveAreaNo() drops it on the way out, so a charset picked for
//  one letter does not silently follow into the next echo - and a
//  crosspost, which is written after the area has changed, takes the
//  charset of the area it lands in for the same reason.

static Area*    xlatexport_area = NULL;
static XlatName xlatexport_pick;


//  The charset of the message being answered, for as long as an
//  answer is being written. XLATREPLYORIGINAL turns it into the
//  charset the answer goes out in - for the area the answer is
//  written into, which is not always the one it is answered from.

static XlatName xlatreply_orig;


Area* XlatexportArea()
{
    return xlatexport_area;
}


void ResetXlatexport()
{
    xlatexport_area = NULL;
    *xlatexport_pick = NUL;
}


void SetXlatReplyOriginal(const char* charset)
{
    *xlatreply_orig = NUL;
    if(charset and *charset)
        strxcpy(xlatreply_orig, charset, sizeof(XlatName));
}


void ClearXlatReplyOriginal()
{
    *xlatreply_orig = NUL;
}


//  What an area's messages are to be written in: what was picked by
//  hand, if the pick belongs to this area, and what the configuration
//  says if it does not.

const char* AreaXlatexport(Area* area)
{
    if(area == NULL)
        return CFG->xlatexport;

    //  A charset chosen by hand wins: it is the latest thing said.
    //  Starting an answer drops an older pick, so what is left here
    //  can only have been chosen while writing this one.
    if((area == xlatexport_area) and *xlatexport_pick)
        return xlatexport_pick;

    //  Then the charset of the message being answered, where this
    //  area asked to answer in it.
    if(*xlatreply_orig and area->Xlatreplyoriginal())
        return xlatreply_orig;

    return area->Xlatexport();
}


//  ------------------------------------------------------------------
//  The charset the next message written here goes out in.
//
//  The mirror of ChangeXlatImport() above, and it shares the list -
//  the same charsets, tried the other way round. Auto means the same
//  thing at this end as it does at that one: stop overriding and let
//  the configuration decide, which here is XLATEXPORT. It sits first
//  and is never tokenized.
//
//  The cursor starts on the charset actually in force - the one picked
//  by hand where there is one, and the one XLATEXPORT resolves to
//  where there is not - never on Auto itself. What the message will be
//  written in is a charset either way, and that is what the reader
//  came to see.

int ChangeXlatExport()
{
    gstrarray Listi;
    char      buf[100];

    Listi.push_back(LNG->CharsetAuto);

    if (CFG->xlatcharsets.empty())
    {
        BuiltinXlatList(Listi, false);
    }
    else
    {
        //  Tables: those that start from the charset the session runs
        //  in, since that is what the text is held in on its way out.
        int maxexp = (int)strlen(CFG->xlatlocalset);
        int maxloc = maxexp;

        ChrsMap::iterator xlt = CFG->xlatcharsets.begin();
        ChrsMap::iterator end = CFG->xlatcharsets.end();

        for (; xlt != end; ++xlt)
        {
            if (strieql((*xlt).first.first.c_str(), CFG->xlatlocalset))
            {
                maxexp = MaxV(maxexp, (int)(*xlt).first.second.size());
                maxloc = MaxV(maxloc, (int)(*xlt).first.first.size());
            }
        }

        //  Writing in the session's own charset needs no table at all -
        //  the text is already in it - so that entry is always there.
        gsprintf(PRINTF_DECLARE_BUFFER(buf), " %*.*s <- %-*.*s ",
                 maxexp, maxexp, CFG->xlatlocalset,
                 maxloc, maxloc, CFG->xlatlocalset);
        Listi.push_back(buf);

        for (xlt = CFG->xlatcharsets.begin(); xlt != end; ++xlt)
        {
            if (strieql((*xlt).first.first.c_str(), CFG->xlatlocalset)
                and not strieql((*xlt).first.second.c_str(), CFG->xlatlocalset))
            {
                gsprintf(PRINTF_DECLARE_BUFFER(buf), " %*.*s <- %-*.*s ",
                         maxexp, maxexp, (*xlt).first.second.c_str(),
                         maxloc, maxloc, (*xlt).first.first.c_str());
                Listi.push_back(buf);
            }
        }
    }

    if (Listi.size() < 2)
        return false;           // nothing but Auto: nothing to choose

    const char* current = AreaXlatexport(AA);
    if ((current == NULL) or (*current == NUL))
        current = CFG->xlatlocalset;

    size_t startat = 0;
    size_t n;                   // one declaration: Visual C++ 6.0 lets
                                // a for-scoped variable outlive its loop
    for (n = 1; n < Listi.size(); n++)
    {
        gstrarray parts;
        tokenize(parts, Listi[n].c_str());
        if (not parts.empty() and GRecoder::same(parts[0].c_str(), current))
        {
            startat = n;
            break;
        }
    }

    n = MinV(Listi.size(), (size_t)(MAXROW-10));
    set_title(LNG->Charsets, TCENTER, C_ASKT);
    update_statusline(LNG->ChangeXlatExp);
    whelppcat(H_ChangeXlatExport);
    n = wpickstr(6, 0, 6+n+1, -1, W_BASK, C_ASKB, C_ASKW, C_ASKS, Listi, startat, title_shadow);
    whelpop();

    if (n == (size_t)-1)
        return false;

    if (n == 0)
    {
        //  Auto: back to what XLATEXPORT says for this area.
        ResetXlatexport();
        return true;
    }

    gstrarray xlat;
    tokenize(xlat, Listi[n].c_str());
    if (xlat.empty())
        return false;

    xlatexport_area = AA;
    strxcpy(xlatexport_pick, xlat[0].c_str(), sizeof(XlatName));

    return true;
}


//  ------------------------------------------------------------------
