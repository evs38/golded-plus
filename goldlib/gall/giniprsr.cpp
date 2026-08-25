//  This may look like C code, but it is really -*- C++ -*-

//  ------------------------------------------------------------------
//  The Goldware Library
//  Copyright (C) 2024 Vitaliy Aksyonov
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
//  INI file parser.
//  ------------------------------------------------------------------

#include "giniprsr.h"
#include <gstrall.h>
#include <fstream>

using namespace std;

namespace ini
{

//  Open Watcom's C++ library has no std::getline() for strings, and its
//  <cstdio> drops the POSIX getline() into namespace std, which makes
//  that look like an overload mismatch rather than an absence. Reading
//  the line here costs nothing on a file this size and needs no #ifdef.
static bool gini_getline(std::istream& is, std::string& line)
{
    strerase(line);

    char c;
    bool got = false;

    while(is.get(c))
    {
        got = true;
        if(c == '\n')
            return true;
        line += c;
    }

    return got;
}


bool ReadIniFile(const char* fileName, Sections& sections)
{
    ifstream file(fileName);

    if (!file)
        return false;

    string currentSection;
    for (string line; gini_getline(file, line);)
    {
        strltrim(strtrim(line));
        // Skip comments and empty lines
        if (line.empty() || line[0] == ';')
            continue;

        // New section
        if (line[0] == '[')
        {
            // Skip invalid section header
            size_t sz = line.size();
            size_t pos = line.find(']');
            if (pos == string::npos || pos != (sz - 1))
                continue;

            currentSection = line.substr(1, sz - 2);
        }
        else
        {
            size_t pos = line.find('=');
            if (pos != string::npos)
            {
                string key = line.substr(0, pos);
                strtrim(key);
                string value = line.substr(pos + 1);
                strltrim(strtrim(value));
                Variables& vars = sections[currentSection];
                // Skip if variable already exist in section.
                Variables::iterator it = vars.find(key);
                if (it == vars.end())
                    vars[key] = value;
            }
        }
    }
    return true;
}

} // namespace ini
