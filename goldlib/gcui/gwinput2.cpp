//  This may look like C code, but it is really -*- C++ -*-

//  ------------------------------------------------------------------
//  The Goldware Library
//  Copyright (C) 1990-1999 Odinn Sorensen
//  Copyright (C) 1999-2000 Alexander S. Aganichev
//  ------------------------------------------------------------------
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Library General Public
//  License as published by the Free Software Foundation; either
//  version 2 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Library General Public License for more details.
//
//  You should have received a copy of the GNU Library General Public
//  License along with this program; if not, write to the Free
//  Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
//  MA 02111-1307, USA
//  ------------------------------------------------------------------
//  $Id$
//  ------------------------------------------------------------------
//  GCUI: Golded+ Character-oriented User Interface.
//  Class gwinput: Input form and field editing.
//  ------------------------------------------------------------------

#include <cstring>
#include <gkbdcode.h>
#include <gmemdbg.h>
#include <gstrall.h>
#include <gutf8.h>
#include <gwinall.h>
#include <gwinhelp.h>
#include <gwinput.h>
#include <gutlclip.h>


//  ------------------------------------------------------------------

gwinput::gwinput(gwindow &w) : window(w)
{

    first_field = current = NULL;
    fill_acs = false;
    idle_attr = active_attr = edit_attr = LGREY_|_BLACK;
    idle_fill = active_fill = edit_fill = ' ';
    insert_mode = true;
    done = dropped = false;
    start_id = 0;
}


//  ------------------------------------------------------------------

gwinput::~gwinput()
{

    field* current = first_field;
    if(current)
    {
        do
        {
            field* junk = current;
            current = current->next;
            delete junk;
        }
        while(current);
    }
}


//  ------------------------------------------------------------------

void gwinput::setup(vattr i_attr, vattr a_attr, vattr e_attr, vchar fill, bool f_acs)
{

    idle_attr = i_attr;
    active_attr = a_attr;
    edit_attr = e_attr;
    fill_acs = f_acs;
    active_fill = edit_fill = fill;
}


//  ------------------------------------------------------------------

bool gwinput::validate()
{

    return true;
}


//  ------------------------------------------------------------------

void gwinput::before()
{

    current->activate();
}


//  ------------------------------------------------------------------

void gwinput::after()
{

    current->deactivate();
}


//  ------------------------------------------------------------------

void gwinput::add_field(int idnum, int wrow, int wcol, int field_width, std::string& dest, int dest_size, int cvt, int mode)
{

    field* fld = new field(this, idnum, wrow, wcol, field_width, dest, dest_size, cvt, mode);
    throw_new(fld);
    if(current)
    {
        current->next = fld;
        fld->prev = current;
        current = fld;
    }
    else
    {
        first_field = current = fld;
    }
}


//  ------------------------------------------------------------------

bool gwinput::move_to(int wrow, int wcol)
{

    field* f = field_at(wrow, wcol);
    if(f)
    {
        after();
        current = f;
        before();
        return true;
    }
    return false;
}


//  ------------------------------------------------------------------

gwinput::field* gwinput::field_at(int wrow, int wcol)
{

    field* here = first_field;
    do
    {
        if(here->row == wrow)
            if(in_range(wcol, here->column, here->max_column))
                return here;
        here = here->next;
    }
    while(here);
    return NULL;
}


//  ------------------------------------------------------------------

gwinput::field* gwinput::get_field(int id)
{

    field* here = first_field;
    do
    {
        if(here->id == id)
            return here;
        here = here->next;
    }
    while(here);
    return NULL;
}


//  ------------------------------------------------------------------

void gwinput::draw_all()
{

    first();
    do
    {
        current->draw();
    }
    while(next());
}


//  ------------------------------------------------------------------

void gwinput::reload_all()
{

    first();
    do
    {
        if(current->entry == gwinput::entry_new)
            *current->buf = NUL;
        else
            strxcpy(current->buf, current->destination.c_str(), current->buf_len+1);
        current->convert();
        current->buf_end_pos = strlen(current->buf);
        current->draw();
    }
    while(next());
}


//  ------------------------------------------------------------------

bool gwinput::first_visible()
{

    if(first())
    {
        if(not current->visible())
            if(next_visible())
                return true;
    }
    return false;
}


//  ------------------------------------------------------------------

bool gwinput::next_visible()
{

    field* here = current;
    while(here->next)
    {
        here = here->next;
        if(here->visible())
        {
            current = here;
            return true;
        }
    }
    return false;
}


//  ------------------------------------------------------------------

bool gwinput::previous_visible()
{

    field* here = current;
    while(here->prev)
    {
        here = here->prev;
        if(here->visible())
        {
            current = here;
            return true;
        }
    }
    return false;
}


//  ------------------------------------------------------------------

bool gwinput::last_visible()
{

    if(last())
    {
        if(not current->visible())
            if(previous_visible())
                return true;
    }
    return false;
}


//  ------------------------------------------------------------------

bool gwinput::first(int id)
{

    if(first_field)
    {
        current = first_field;
        if(id)
            while(current->id != id)
                next();
        return true;
    }
    return false;
}


//  ------------------------------------------------------------------

bool gwinput::next()
{

    if(current->next)
    {
        current = current->next;
        return true;
    }
    return false;
}


//  ------------------------------------------------------------------

bool gwinput::previous()
{

    if(current->prev)
    {
        current = current->prev;
        return true;
    }
    return false;
}


//  ------------------------------------------------------------------

bool gwinput::last()
{

    if(first_field)
    {
        current = first_field;
        while(next())
            ;
        return true;
    }
    return false;
}


//  ------------------------------------------------------------------

void gwinput::show_cursor()
{

    if(insert_mode)
        vcursmall();
    else
        vcurlarge();
}


//  ------------------------------------------------------------------

void gwinput::drop_form()
{

    after();
    done = true;
    dropped = true;
}


//  ------------------------------------------------------------------

void gwinput::form_complete()
{

    after();
    done = true;
}


//  ------------------------------------------------------------------

void gwinput::field_complete()
{

    if(validate())
    {
        after();
        if(not next_visible())
        {
            done = true;
            return;
        }
        before();
    }
}


//  ------------------------------------------------------------------

void gwinput::go_next_field()
{

    after();
    if(not next_visible())
        first_visible();
    before();
}


//  ------------------------------------------------------------------

void gwinput::go_previous_field()
{

    after();
    if(not previous_visible())
        last_visible();
    before();
}


//  ------------------------------------------------------------------

void gwinput::go_up()
{

    int min_column = current->column;
    int max_column = current->max_column;
    int check_row = current->row - 1;
    int check_column = current->column + current->pos;
    for(int r=check_row; r>=0; r--)
    {
        int c;
        for(c=check_column; c<=max_column; c++)
        {
            if(move_to(r, c))
                return;
        }
        for(c=check_column-1; c>=min_column; c--)
        {
            if(move_to(r, c))
                return;
        }
    }
}


//  ------------------------------------------------------------------

void gwinput::go_down()
{

    int max_row = window.height() - (window.has_border() ? 2 : 0) - 1;
    int min_column = current->column;
    int max_column = current->max_column;
    int check_row = current->row + 1;
    int check_column = current->column + current->pos;
    for(int r=check_row; r<=max_row; r++)
    {
        int c;
        for(c=check_column; c<=max_column; c++)
        {
            if(move_to(r, c))
                return;
        }
        for(c=check_column-1; c>=min_column; c--)
        {
            if(move_to(r, c))
                return;
        }
    }
}


//  ------------------------------------------------------------------

void gwinput::go_left()
{

    if(not current->left())
        go_previous_field();
}


//  ------------------------------------------------------------------

void gwinput::go_right()
{

    if(not current->right())
        go_next_field();
}


//  ------------------------------------------------------------------

void gwinput::delete_left()
{

    current->delete_left();
}


//  ------------------------------------------------------------------

void gwinput::delete_char()
{

    current->delete_char();
}


//  ------------------------------------------------------------------

void gwinput::go_field_begin()
{

    current->home();
}


//  ------------------------------------------------------------------

void gwinput::go_field_end()
{

    current->end();
}


//  ------------------------------------------------------------------

void gwinput::go_form_begin()
{

    after();
    first_visible();
    before();
    current->home();
}


//  ------------------------------------------------------------------

void gwinput::go_form_end()
{

    after();
    last_visible();
    before();
    current->end();
}


//  ------------------------------------------------------------------

void gwinput::toggle_insert()
{

    insert_mode ^= true;
    show_cursor();
}


//  ------------------------------------------------------------------

void gwinput::restore_field()
{

    current->restore();
}


//  ------------------------------------------------------------------

void gwinput::delete_left_word()
{

    current->delete_word(true);
}


//  ------------------------------------------------------------------

void gwinput::delete_right_word()
{

    current->delete_word(false);
}


//  ------------------------------------------------------------------

void gwinput::go_left_word()
{

    current->left_word();
}


//  ------------------------------------------------------------------

void gwinput::go_right_word()
{

    current->right_word();
}


//  ------------------------------------------------------------------

void gwinput::enter_char(char ch)
{

    enter_char(&ch, 1);
}


//  ------------------------------------------------------------------

void gwinput::enter_char(const char* chars, int len)
{

    if(len > 0 and chars[0])
    {
        if(insert_mode)
            current->insert_char(chars, len);
        else
            current->overwrite_char(chars, len);
    }
}


//  ------------------------------------------------------------------

void gwinput::prepare_form()
{

    cursor_was_hidden = vcurhidden();
    draw_all();
    first(start_id);
    before();
    show_cursor();
}


//  ------------------------------------------------------------------

void gwinput::finish_form()
{

    if(not dropped)
    {
        first();
        do
        {
            current->commit();
        }
        while(next());
    }

    if(cursor_was_hidden)
        vcurhide();
}


//  ------------------------------------------------------------------

void gwinput::clear_field()
{

    current->clear_field();
}


//  ------------------------------------------------------------------

void gwinput::clipboard_cut()
{

    current->clipboard_copy();
    current->clear_field();
}


//  ------------------------------------------------------------------

void gwinput::clipboard_paste()
{

    if(insert_mode)
        current->clipboard_paste();
    else
    {
        current->clear_field();
        current->clipboard_paste();
    }
}


//  ------------------------------------------------------------------

void gwinput::clipboard_copy()
{

    current->clipboard_copy();
}


//  ------------------------------------------------------------------

bool gwinput::handle_other_keys(gkey&)
{

    return false;
}


//  ------------------------------------------------------------------

bool gwinput::handle_key(gkey key)
{

    switch(key)
    {
    case Key_Esc:
        drop_form();
        break;
    case Key_C_Ent:
        form_complete();
        break;
    case Key_Ent:
        field_complete();
        break;
    case Key_Tab:
        go_next_field();
        break;
    case Key_S_Tab:
        go_previous_field();
        break;
    case Key_Up:
        go_up();
        break;
    case Key_Dwn:
        go_down();
        break;
    case Key_Lft:
        go_left();
        break;
    case Key_Rgt:
        go_right();
        break;
    case Key_BS:
        delete_left();
        break;
    case Key_Del:
        delete_char();
        break;
    case Key_Home:
        go_field_begin();
        break;
    case Key_End:
        go_field_end();
        break;
    case Key_C_Home:
        go_form_begin();
        break;
    case Key_C_End:
        go_form_end();
        break;
    case Key_Ins:
        toggle_insert();
        break;
    case Key_A_BS:          // fall through
    case Key_C_R:
        restore_field();
        break;
    case Key_C_BS:
        delete_left_word();
        break;
    case Key_C_T:
        delete_right_word();
        break;
    case Key_C_Lft:
        go_left_word();
        break;
    case Key_C_Rgt:
        go_right_word();
        break;
#if !defined(__UNIX__) || defined(__USE_NCURSES__)
    case Key_S_Ins:         // fall through
#endif
    case Key_C_V:
        clipboard_paste();
        break;
#if !defined(__UNIX__) || defined(__USE_NCURSES__)
    case Key_S_Del:         // fall through
#endif
    case Key_C_X:
        clipboard_cut();
        break;
#if !defined(__UNIX__) || defined(__USE_NCURSES__)
    case Key_C_Ins:         // fall through
#endif
    case Key_C_C:
        clipboard_copy();
        break;
#if !defined(__UNIX__) || defined(__USE_NCURSES__)
    case Key_C_Del:         // fall through
#endif
    case Key_C_Y:           // fall through
    case Key_C_D:
        clear_field();
        break;
    default:
        if(not handle_other_keys(key))
            {
                int _len = 0;
                const char* _chars = gkbd_keychars(key, &_len);

                if(_chars)
                    enter_char(_chars, _len);
                else
                    enter_char(KCodAsc(key));
            }
    }

    return not done;
}


//  ------------------------------------------------------------------

gwinput::field::field(gwinput* iform, int idnum, int wrow, int wcol, int field_width, std::string& dest, int dest_size, int cvt, int mode)
    : destination(dest)
{
    prev = next = NULL;
    pos = buf_pos = buf_left_pos = 0;

    form = iform;
    id = idnum;
    row = wrow;
    column = wcol;
    max_pos = field_width - 1;
    max_column = wcol + max_pos;
    buf_len = dest_size - 1;
    buf = new char[dest_size];
    throw_new(buf);
    conversion = cvt;
    entry = entry_mode = mode;
    attr = form->idle_attr;
    fill = form->idle_fill;
    fill_acs = form->fill_acs;

    if(entry == gwinput::entry_new)
        *buf = NUL;
    else
        strxcpy(buf, dest.c_str(), dest_size);
    convert();
    buf_end_pos = strlen(buf);
}


//  ------------------------------------------------------------------

gwinput::field::~field()
{

    delete[] buf;
}


//  ------------------------------------------------------------------

bool gwinput::field::visible()
{

    return max_pos != -1;
}


//  ------------------------------------------------------------------

void gwinput::field::convert()
{

    switch(conversion)
    {
    case gwinput::cvt_lowercase:
        strlwr(buf);
        break;
    case gwinput::cvt_uppercase:
        strupr(buf);
        break;
    case gwinput::cvt_mixedcase:
        struplow(buf);
        break;
    }
}


//  ------------------------------------------------------------------

void gwinput::field::update()
{

    buf_end_pos = strlen(buf);
    end();
}


//  ------------------------------------------------------------------

void gwinput::field::activate()
{

    buf_end_pos = strlen(buf);
    entry = entry_mode;
    if(entry == gwinput::entry_conditional or entry == gwinput::entry_noedit)
    {
        attr = form->active_attr;
        fill = form->active_fill;
        int entry_bak = entry;
        entry = gwinput::entry_update;   // cheat adjust_mode() in end()
        end();
        entry = entry_bak;
    }
    else
    {
        // 0 == entry_new, 1 == entry_update
        entry ? end() : home();
    }
}


//  ------------------------------------------------------------------

void gwinput::field::deactivate()
{

    fill = form->idle_fill;
    attr = form->idle_attr;
    draw();
}


//  ------------------------------------------------------------------

void gwinput::field::restore()
{

    std::string tmp(buf);
    strxcpy(buf, destination.c_str(), buf_len+1);
    destination = tmp;
    convert();
    activate();
}


//  ------------------------------------------------------------------

void gwinput::field::commit()
{

    destination = buf;
}


//  ------------------------------------------------------------------

void gwinput::field::move_cursor()
{

    form->window.move_cursor(row, column+pos);
}


//  ------------------------------------------------------------------
//  Byte offsets and screen columns.

int gwinput::field::next_off(int off) const
{
    if(off >= buf_end_pos)
        return buf_end_pos;
    return (int)(g_utf8_cluster_next(buf + off, buf + buf_end_pos) - buf);
}


int gwinput::field::prev_off(int off) const
{
    if(off <= 0)
        return 0;
    return (int)(g_utf8_cluster_prev(buf, buf + off) - buf);
}


int gwinput::field::col_of(int off) const
{
    if(off <= buf_left_pos)
        return 0;
    return (int)g_utf8_width(buf + buf_left_pos, off - buf_left_pos);
}


int gwinput::field::off_at_col(int col) const
{
    return buf_left_pos + (int)g_utf8_bytes_for_cols(buf + buf_left_pos, col);
}


//  ------------------------------------------------------------------
//  Put the cursor where buf_pos says, scrolling the field sideways if
//  that would otherwise be off its left or right edge.

void gwinput::field::resync()
{
    bool scrolled = false;

    if(buf_pos < buf_left_pos)
    {
        buf_left_pos = buf_pos;
        scrolled = true;
    }

    while(col_of(buf_pos) > max_pos)
    {
        buf_left_pos = next_off(buf_left_pos);
        scrolled = true;
    }

    pos = col_of(buf_pos);

    if(scrolled)
        draw();
    move_cursor();
}


//  ------------------------------------------------------------------

void gwinput::field::draw(int from_col)
{

    if(visible())
    {
        //  from_col is a screen column; the text to draw from starts at
        //  the byte that column falls on.
        int off = off_at_col(from_col);
        form->window.printns(row, column+from_col, attr, buf+off,
                             1+max_pos-from_col, fill, attr | (fill_acs ? ACSET : 0));
    }
}


//  ------------------------------------------------------------------

bool gwinput::field::adjust_mode()
{

    if(entry != gwinput::entry_update and entry != gwinput::entry_noedit)
    {
        entry = gwinput::entry_update;
        attr = form->edit_attr;
        fill = form->edit_fill;
        return true;
    }
    return false;
}


//  ------------------------------------------------------------------

void gwinput::field::conditional()
{

    if(entry == gwinput::entry_conditional)
    {
        clear_field();
    }
}


//  ------------------------------------------------------------------

void gwinput::field::move_left()
{

    buf_pos = prev_off(buf_pos);
    resync();
}


//  ------------------------------------------------------------------

void gwinput::field::move_right()
{

    buf_pos = next_off(buf_pos);
    resync();
}


//  ------------------------------------------------------------------

bool gwinput::field::left()
{

    if(adjust_mode())
        draw();

    if(entry != gwinput::entry_noedit)
    {
        if(buf_pos > 0)
        {
            move_left();
            return true;
        }
    }
    return false;
}


//  ------------------------------------------------------------------

bool gwinput::field::right()
{

    if(adjust_mode())
    {
        draw();
        return true;
    }

    if(entry != gwinput::entry_noedit)
    {
        if(buf_pos < buf_end_pos)
        {
            move_right();
            return true;
        }
    }
    return false;
}


//  ------------------------------------------------------------------

bool gwinput::field::left_word()
{

    if(adjust_mode())
        draw();

    if(entry != gwinput::entry_noedit)
    {
        if(buf_pos > 0)
        {
            move_left();
            if(not isxalnum(buf[buf_pos]))
            {
                while(not isxalnum(buf[buf_pos]) and (buf_pos > 0))
                    move_left();
                while(isxalnum(buf[buf_pos]) and (buf_pos > 0))
                    move_left();
            }
            else
            {
                while(isxalnum(buf[buf_pos]) and (buf_pos > 0))
                    move_left();
            }

            if(buf_pos != 0)
                move_right();
        }
    }
    return false;
}


//  ------------------------------------------------------------------

bool gwinput::field::right_word()
{

    if(adjust_mode())
        draw();

    if(entry != gwinput::entry_noedit)
    {
        if(buf_pos < buf_end_pos)
        {
            move_right();
            if(not isxalnum(buf[buf_pos]))
            {
                while(not isxalnum(buf[buf_pos]) and ((buf_pos+1) <= buf_end_pos))
                    move_right();
            }
            else
            {
                while(isxalnum(buf[buf_pos]) and ((buf_pos+1) <= buf_end_pos))
                    move_right();
                while(not isxalnum(buf[buf_pos]) and ((buf_pos+1) <= buf_end_pos))
                    move_right();
            }
            return true;
        }
    }
    return false;
}


//  ------------------------------------------------------------------

bool gwinput::field::delete_left()
{

    if(adjust_mode())
        draw();

    if(entry != gwinput::entry_noedit)
    {
        if(buf_pos > 0)
        {
            left();
            return delete_char();
        }
    }
    return false;
}


//  ------------------------------------------------------------------

bool gwinput::field::delete_char()
{

    if(adjust_mode())
        draw();

    if(entry != gwinput::entry_noedit)
    {
        if(buf_pos < buf_end_pos)
        {
            //  Remove the whole character, however many bytes it is.
            int nxt = next_off(buf_pos);
            int gone = nxt - buf_pos;
            memmove(buf+buf_pos, buf+nxt, buf_len-nxt+1);
            buf_end_pos -= gone;
            buf[buf_end_pos] = NUL;
            draw(pos);
            move_cursor();
            return true;
        }
    }
    return false;
}


//  ------------------------------------------------------------------

bool gwinput::field::delete_word(bool left)
{

    if(adjust_mode())
        draw();

    if(entry != gwinput::entry_noedit)
    {

        bool state = make_bool(isspace(buf[buf_pos-((int) left)]));

        while(left ? buf_pos > 0 : buf_pos < buf_end_pos)
        {
            left ? delete_left() : delete_char();

            if(make_bool(isspace(buf[buf_pos-((int) left)])) != state)
                break;
        }
        return true;

    }
    return false;
}


//  ------------------------------------------------------------------

bool gwinput::field::insert_char(const char* chars, int len)
{

    if(entry != gwinput::entry_noedit)
    {

        conditional();

        if(buf_end_pos + len <= buf_len)
        {
            int tail = buf_end_pos - buf_pos;
            memmove(buf+buf_pos+len, buf+buf_pos, tail+1);
            buf_end_pos += len;
            //  The room is made; overwrite_char() now just fills it in,
            //  and must not push the end out a second time.
            int saved_end = buf_end_pos;
            bool rc = overwrite_char(chars, len);
            buf_end_pos = saved_end;
            buf[buf_end_pos] = NUL;
            return rc;
        }
    }
    return false;
}


//  ------------------------------------------------------------------

bool gwinput::field::overwrite_char(const char* chars, int len)
{

    if(entry != gwinput::entry_noedit)
    {

        conditional();

        //  Case conversion is a property of the character, so it is done
        //  on the codepoint rather than on each byte.
        std::string text(chars, len);

        if(conversion == gwinput::cvt_lowercase or conversion == gwinput::cvt_uppercase)
        {
            if(g_utf8_mode())
            {
                int used = 1;
                uint32_t cp = g_utf8_decode(text.c_str(), &used);
                cp = (conversion == gwinput::cvt_lowercase) ? g_cp_tolower(cp)
                                                            : g_cp_toupper(cp);
                text = g_utf8_encode(cp);
            }
            else
            {
                text[0] = (char)((conversion == gwinput::cvt_lowercase)
                                 ? g_tolower(text[0]) : g_toupper(text[0]));
            }
        }

        //  A replacement of a different length has to move the tail.
        int old = (buf_pos < buf_end_pos) ? next_off(buf_pos) - buf_pos : 0;
        int now = (int)text.length();

        if(buf_end_pos - old + now > buf_len)
            return false;

        if(old != now)
        {
            memmove(buf+buf_pos+now, buf+buf_pos+old, buf_end_pos-buf_pos-old+1);
            buf_end_pos += now - old;
        }

        memcpy(buf+buf_pos, text.data(), now);

        if(buf_pos + now > buf_end_pos)
            buf_end_pos = buf_pos + now;
        buf[buf_end_pos] = NUL;

        if(conversion == gwinput::cvt_mixedcase)
        {
            struplow(buf);
            draw();
        }
        else
        {
            draw(pos);
        }

        right();
    }

    return true;
}


//  ------------------------------------------------------------------

bool gwinput::field::home()
{

    adjust_mode();

    pos = buf_pos = buf_left_pos = 0;
    draw();
    move_cursor();
    return true;
}


//  ------------------------------------------------------------------

bool gwinput::field::end()
{

    adjust_mode();

    buf_pos = buf_end_pos;
    if(buf_pos >= buf_len)
        buf_pos = prev_off(buf_len);

    //  Show as much of the tail as the field is wide, measured in
    //  columns, and start it on a character boundary.
    buf_left_pos = 0;
    int width = (int)g_utf8_width(buf, buf_pos);
    while(width > max_pos)
    {
        buf_left_pos = next_off(buf_left_pos);
        width = (int)g_utf8_width(buf+buf_left_pos, buf_pos-buf_left_pos);
    }
    pos = width;

    draw();
    move_cursor();
    return true;
}


//  ------------------------------------------------------------------

void gwinput::field::clear_field()
{

    if(entry != gwinput::entry_noedit)
    {

        pos = buf_pos = buf_left_pos = buf_end_pos = 0;
        *buf = NUL;
        adjust_mode();
        draw();
        move_cursor();
    }
}


//  ------------------------------------------------------------------

void gwinput::field::clipboard_paste()
{

    if(entry != gwinput::entry_noedit)
    {

        conditional();

        gclipbrd clipbrd;

        if(not clipbrd.openread())
            return;

        char *clpbuf = (char *)throw_malloc(buf_len + 1);

        if(clipbrd.read(clpbuf, buf_len + 1))
        {

            size_t len = strlen(clpbuf);
            if((len != 0) and (clpbuf[len - 1] == '\n'))
            {
                clpbuf[--len] = NUL;

                switch(conversion)
                {
                case gwinput::cvt_lowercase:
                    strlwr(clpbuf);
                    break;
                case gwinput::cvt_uppercase:
                    strupr(clpbuf);
                    break;
                }
            }

            if((buf_pos == buf_end_pos) or ((buf_pos + len) >= buf_len))
            {
                strxcat(buf, clpbuf, buf_len + 1);
                buf_end_pos = strlen(buf);
                end();
            }
            else
            {
                strxcat(clpbuf, buf + buf_pos, buf_len + 1);
                buf[buf_pos] = NUL;
                strxcat(buf, clpbuf, buf_len + 1);
                buf_end_pos = strlen(buf);
                for(int i = 0; i < len; i++)
                    move_right();
            }

            if(conversion == gwinput::cvt_mixedcase)
            {
                struplow(buf);
                draw();
            }
            else
            {
                draw();
            }
        }

        throw_free(clpbuf);

        clipbrd.close();
    }
}


//  ------------------------------------------------------------------

void gwinput::field::clipboard_copy()
{

    if(entry != gwinput::entry_noedit)
    {

        gclipbrd clipbrd;

        clipbrd.writeclipbrd(buf);
    }
}


//  ------------------------------------------------------------------

bool gwinput2::run(int helpcat)
{

    prepare_form();
    whelppcat(helpcat);

    while(handle_key(getxch()));

    whelpop();
    finish_form();

    return not dropped;
}


//  ------------------------------------------------------------------

