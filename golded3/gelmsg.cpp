
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
//  Load message or header. Save header.
//  ------------------------------------------------------------------

#include <golded.h>

//  ------------------------------------------------------------------

int Area::LoadHdr(GMsg* msg, uint32_t msgno, bool enable_recode)
{
    if( msg == NULL )
    {
        LOG.printf("! Area::LoadHdr() is called with NULL pointer to msg." );
        return false;
    }

    msg->Reset();
    msg->msgno = msgno;
    int retval = area->load_hdr(msg);

    if (isecho())
    {
        if (CFG->akamatchfromto && msg->dest.invalid())
            msg->dest = Aka().addr;
        else if (CFG->akamatchfromto == ALWAYS)
            msg->dest = Aka().addr;
    }

    // Don't translate charsets if we don't know charset
    // Currently, it only mime-decodes, so it's okay.
    if(retval and enable_recode)
    {
        // Use default translation by default
        int table = GetCurrentTable();

        //  A CHRS kludge normally lives in the message text, and
        //  reading every message just to build a list would be far
        //  too slow. Some bases keep the kludges in the header
        //  instead - there the driver has already handed us the
        //  charset for nothing, and the list can decode the header
        //  fields exactly as the reader does. Where it has not, the
        //  area's own import charset is all we know.
        //
        //  Not when the user has fixed the charset by hand: the reader
        //  ignores the kludges then, and the list has to agree with
        //  it. That is decided on the option alone - with a recoder in
        //  force there is no table, so its index says nothing.
        msg->charsetlevel = 0;
        if(*msg->hdrchrs and not CFG->ignorecharset)
            msg->charsetlevel = LoadCharset(msg->hdrchrs, CFG->xlatlocalset);
        if(not msg->charsetlevel)
        {
            if((table == -1) or not CFG->ignorecharset)
                msg->charsetlevel = LoadCharset(AA->Xlatimport(), CFG->xlatlocalset);
            else
                msg->charsetlevel = LoadCharset(table);
        }

        // Charset translate header fields
        strxmimecpy(msg->realby, msg->realby, msg->charsetlevel, sizeof(INam), true);
        strxmimecpy(msg->realto, msg->realto, msg->charsetlevel, sizeof(INam), true);
        strxmimecpy(msg->by, msg->by, msg->charsetlevel, sizeof(INam), true);
        strxmimecpy(msg->to, msg->to, msg->charsetlevel, sizeof(INam), true);

        if(not (msg->attr.frq() or msg->attr.att() or msg->attr.urq()))
            strxmimecpy(msg->re, msg->re, msg->charsetlevel, sizeof(ISub), true);

        //  The FSP-1030 fields, where the driver found them in the
        //  header: the list shows the same name the reader will.
        ApplyUcsHeaders(msg);
    }
    return retval;
}


//  ------------------------------------------------------------------

int Area::LoadMsg(GMsg* msg, uint32_t msgno, int margin, int mode)
{
    if( msg == NULL )
    {
        LOG.printf("! Area::LoadMsg() is called with NULL pointer to msg." );
        return false;
    }

    msg->Reset();
    msg->msgno = msgno;
    if(msgno and area->load_msg(msg))
    {

        if (isecho())
        {
            if (CFG->akamatchfromto && msg->dest.invalid())
                msg->dest = Aka().addr;
            else if (CFG->akamatchfromto == ALWAYS)
                msg->dest = Aka().addr;
        }

        if(mode & (GMSG_COPY|GMSG_MOVE))
        {
            //  The header fields stay as the base holds them, and
            //  nothing downstream converts them - so the driver that
            //  writes the copy has to be told here whether a cut may
            //  fall inside a character. The header's own charset
            //  kludge is all that is known about them.
            msg->hdrutf8 = *msg->hdrchrs and GRecoder::is_utf8(msg->hdrchrs);
            if(not ((mode & GMSG_MOVE) and (mode & GMSG_UNS_NOT_RCV)))
                return true;
            if(not (msg->attr.uns() and not msg->attr.rcv()))
                return true;
        }

        msg->TextToLines(margin);

        return true;
    }
    return false;
}


//  ------------------------------------------------------------------

void Area::SaveHdr(int mode, GMsg* msg)
{
    if( msg == NULL )
    {
        LOG.printf("! Area::LoadMsg() is called with NULL pointer to msg." );
        PointerErrorExit();
    }

    // Translate softcr to configured char
    //  Not in a UTF-8 session: 0x8D is a continuation byte there - the
    //  second byte of the Cyrillic э - and swapping it would break the
    //  character.
    if (adat->usesoftcrxlat && EDIT->SoftCrXlat() && !g_utf8_mode())
    {
        strchg(msg->by, SOFTCR, EDIT->SoftCrXlat());
        strchg(msg->to, SOFTCR, EDIT->SoftCrXlat());
        strchg(msg->realby, SOFTCR, EDIT->SoftCrXlat());
        strchg(msg->realto, SOFTCR, EDIT->SoftCrXlat());
        if(not (msg->attr.frq() or msg->attr.att() or msg->attr.urq()))
            strchg(msg->re, SOFTCR, EDIT->SoftCrXlat());
    }
    area->save_hdr(mode, msg);
    UpdateAreadata();
}

//  ------------------------------------------------------------------
