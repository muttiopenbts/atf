//
// Automated Testing Framework (atf)
//
// Copyright (c) 2007 The NetBSD Foundation, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 3. All advertising materials mentioning features or use of this
//    software must display the following acknowledgement:
//        This product includes software developed by the NetBSD
//        Foundation, Inc. and its contributors.
// 4. Neither the name of The NetBSD Foundation nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
// CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
// IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
// IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include <cstdlib>
#include <iostream>
#include <map>
#include <string>

#include "atf/application.hpp"
#include "atf/config.hpp"
#include "atf/sanity.hpp"

class atf_config : public atf::application {
    static const char* m_description;

    bool m_tflag;

    void process_option(int, const char*);
    std::string specific_args(void) const;
    options_set specific_options(void) const;

    std::string format_var(const std::string&, const std::string&);

public:
    atf_config(void);

    int main(void);
};

const char* atf_config::m_description =
    "atf-config is a tool that queries the value of several "
    "installation-specific configuration values of the atf.  "
    "It can be used by external tools to discover where specific "
    "internal atf files are installed.";

atf_config::atf_config(void) :
    application(m_description, "atf-config(1)"),
    m_tflag(false)
{
}

void
atf_config::process_option(int ch, const char* arg)
{
    switch (ch) {
    case 't':
        m_tflag = true;
        break;

    default:
        UNREACHABLE;
    }
}

std::string
atf_config::specific_args(void)
    const
{
    return "[var1 [.. varN]]";
}

atf_config::options_set
atf_config::specific_options(void)
    const
{
    options_set opts;
    opts.insert(option('t', "", "Terse output: show values only"));
    return opts;
}

std::string
atf_config::format_var(const std::string& name, const std::string& val)
{
    std::string str;

    if (m_tflag)
        str = val;
    else
        str = name + " : " + val;

    return str;
}

int
atf_config::main(void)
{
    if (m_argc < 1) {
        std::map< std::string, std::string > cv = atf::config::get_all();

        for (std::map< std::string, std::string >::const_iterator iter =
             cv.begin(); iter != cv.end(); iter++)
            std::cout << format_var((*iter).first, (*iter).second)
                      << std::endl;
    } else {
        for (int i = 0; i < m_argc; i++) {
            if (!atf::config::has(m_argv[i]))
                throw std::runtime_error(std::string("Unknown variable `") +
                                         m_argv[i] + "'");
        }

        for (int i = 0; i < m_argc; i++) {
            std::cout << format_var(m_argv[i], atf::config::get(m_argv[i]))
                      << std::endl;
        }
    }

    return EXIT_SUCCESS;
}

int
main(int argc, char* const* argv)
{
    return atf_config().run(argc, argv);
}
