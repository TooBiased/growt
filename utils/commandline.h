/*******************************************************************************
 * utils/commandline.h
 *
 * Really simple command line parser for our tests/benchmarks
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#ifndef COMMANDLINE_H
#define COMMANDLINE_H

#include <string>
#include <vector>
#include <tuple>

class CommandLine
{
public:
    CommandLine(int argn, char** argc)
    {
        for (size_t i = 0; i < size_t(argn); ++i)
        {
            params.emplace_back(argc[i]);
            flags .push_back   (ParamCodes::unused);
        }
    }

    std::string strArg(const std::string& name, const std::string def = "")
    {
        auto ind = findName(name);
        if (ind+1 < params.size())
        {
            flags[ind+1] = ParamCodes::used;
            return params[ind+1];
        }
        else if (ind < params.size())
        {
            flags[ind] = ParamCodes::error;
            std::cout << "found argument \"" << name << "\" without following integer!"
                      << std::endl;
        }
        return def;
    }

    int intArg(const std::string& name, int def = 0)
    {
        auto ind = findName(name);
        if (ind+1 < params.size())
        {
            flags[ind+1] = ParamCodes::used;
            int  r = 0;
            try
            {   r = std::stoi(params[ind+1]);   }
            catch (std::invalid_argument& e)
            {
                flags[ind+1] = ParamCodes::error;
                r = def;
                std::cout << "error reading int argument \"" << name
                          << "\" from console, got \"invalid_argument exception\""
                          << std::endl;
            }
            return r;
        }
        else if (ind < params.size())
        {
            flags[ind] = ParamCodes::error;
            std::cout << "found argument \"" << name << "\" without following integer!"
                      << std::endl;
        }
        return def;

    }

    double doubleArg(const std::string& name, double def = 0.)
    {
        auto ind = findName(name);
        if (ind+1 < params.size())
        {
            flags[ind+1] = ParamCodes::used;
            double  r = 0;
            try
            {   r = std::stod(params[ind+1]);   }
            catch (std::invalid_argument& e)
            {
                flags[ind+1] = ParamCodes::error;
                r = def;
                std::cout << "error reading double argument \"" << name
                          << "\" from console, got \"invalid-argument exception\"!"
                          << std::endl;
            }
            return r;
        }
        else if (ind < params.size())
        {
            flags[ind] = ParamCodes::error;
            std::cout << "found argument \"" << name << "\" without following double!"
                      << std::endl;
        }
        return def;
    }

    bool boolArg(const std::string& name)
    {
        return (findName(name)) < params.size();
    }

    bool report()
    {
        bool un = true;
        for (size_t i = 1; i < params.size(); ++i)
        {
            if (flags[i] != ParamCodes::used)
            {
                if (flags[i] == ParamCodes::unused)
                {
                    std::cout << "parameter " << i << " = \"" << params[i]
                              << "\" was unused!" << std::endl;
                }
                else if ( flags[i] == ParamCodes::error)
                {
                    std::cout << "error reading parameter " << i
                              << " = \"" << params[i] << "\"" << std::endl;
                }
                un = false;
            }
        }
        return un;
    }

private:
    enum class ParamCodes
    {
        unused,
        used,
        error
    };

    std::vector<std::string> params;
    std::vector<ParamCodes>  flags;

    size_t findName(const std::string& name)
    {
        for (size_t i = 0; i < params.size(); ++i)
        {
            if (params[i] == name)
            {
                flags[i] = ParamCodes::used;
                return i;
            }
        }
        return params.size();
    }

};


#endif // COMMANDLINE_H
