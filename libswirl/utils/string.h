/*
	This file is part of libswirl
*/
#include "license/bsd"


#pragma once
#include "types.h"

// TODO: Improve this
static vector<string> SplitString(string s, string delim)
{
	vector<string> rv;

	auto start = 0U;
    auto end = s.find(delim);

    while (end != std::string::npos)
    {
    	rv.push_back(s.substr(start, end - start));
        start = end + delim.length();
        end = s.find(delim, start);
    }

	rv.push_back(s.substr(start, end));

    return rv;
}