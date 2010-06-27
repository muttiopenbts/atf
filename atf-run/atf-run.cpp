//
// Automated Testing Framework (atf)
//
// Copyright (c) 2007, 2008, 2009, 2010 The NetBSD Foundation, Inc.
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

#if defined(HAVE_CONFIG_H)
#include "bconfig.h"
#endif

extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
}

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

#include "atf-c++/application.hpp"
#include "atf-c++/config.hpp"
#include "atf-c++/env.hpp"
#include "atf-c++/exceptions.hpp"
#include "atf-c++/fs.hpp"
#include "atf-c++/io.hpp"
#include "atf-c++/parser.hpp"
#include "atf-c++/process.hpp"
#include "atf-c++/sanity.hpp"
#include "atf-c++/signals.hpp"
#include "atf-c++/tests.hpp"
#include "atf-c++/text.hpp"
#include "atf-c++/user.hpp"

#include "atffile.hpp"
#include "config.hpp"
#include "fs.hpp"
#include "requirements.hpp"
#include "test-program.hpp"

namespace impl = atf::atf_run;

class atf_run : public atf::application::app {
    static const char* m_description;

    atf::tests::vars_map m_cmdline_vars;

    static atf::tests::vars_map::value_type parse_var(const std::string&);

    void process_option(int, const char*);
    std::string specific_args(void) const;
    options_set specific_options(void) const;

    void parse_vflag(const std::string&);

    std::vector< std::string > conf_args(void) const;

    size_t count_tps(std::vector< std::string >) const;

    int run_test(const atf::fs::path&,
                 impl::atf_tps_writer&,
                 const atf::tests::vars_map&,
                 const atf::fs::path&);
    int run_test_directory(const atf::fs::path&,
                           impl::atf_tps_writer&,
                           const atf::fs::path&);
    int run_test_program(const atf::fs::path&, impl::atf_tps_writer&,
                         const atf::tests::vars_map&,
                         const atf::fs::path&);

    impl::test_case_result get_test_case_result(const std::string&,
        const atf::process::status&, const atf::fs::path&) const;

public:
    atf_run(void);

    int main(void);
};

const char* atf_run::m_description =
    "atf-run is a tool that runs tests programs and collects their "
    "results.";

atf_run::atf_run(void) :
    app(m_description, "atf-run(1)", "atf(7)")
{
}

void
atf_run::process_option(int ch, const char* arg)
{
    switch (ch) {
    case 'v':
        parse_vflag(arg);
        break;

    default:
        UNREACHABLE;
    }
}

std::string
atf_run::specific_args(void)
    const
{
    return "[test-program1 .. test-programN]";
}

atf_run::options_set
atf_run::specific_options(void)
    const
{
    using atf::application::option;
    options_set opts;
    opts.insert(option('v', "var=value", "Sets the configuration variable "
                                         "`var' to `value'; overrides "
                                         "values in configuration files"));
    return opts;
}

void
atf_run::parse_vflag(const std::string& str)
{
    if (str.empty())
        throw std::runtime_error("-v requires a non-empty argument");

    std::vector< std::string > ws = atf::text::split(str, "=");
    if (ws.size() == 1 && str[str.length() - 1] == '=') {
        m_cmdline_vars[ws[0]] = "";
    } else {
        if (ws.size() != 2)
            throw std::runtime_error("-v requires an argument of the form "
                                     "var=value");

        m_cmdline_vars[ws[0]] = ws[1];
    }
}

int
atf_run::run_test(const atf::fs::path& tp,
                  impl::atf_tps_writer& w,
                  const atf::tests::vars_map& config,
                  const atf::fs::path& ro_workdir)
{
    atf::fs::file_info fi(tp);

    int errcode;
    if (fi.get_type() == atf::fs::file_info::dir_type)
        errcode = run_test_directory(tp, w, ro_workdir);
    else {
        const atf::tests::vars_map effective_config =
            impl::merge_configs(config, m_cmdline_vars);

        errcode = run_test_program(tp, w, effective_config, ro_workdir);
    }
    return errcode;
}

int
atf_run::run_test_directory(const atf::fs::path& tp,
                            impl::atf_tps_writer& w,
                            const atf::fs::path& ro_workdir)
{
    impl::atffile af = impl::read_atffile(tp / "Atffile");

    atf::tests::vars_map test_suite_vars;
    {
        atf::tests::vars_map::const_iterator iter =
            af.props().find("test-suite");
        INV(iter != af.props().end());
        test_suite_vars = impl::read_config_files((*iter).second);
    }

    bool ok = true;
    for (std::vector< std::string >::const_iterator iter = af.tps().begin();
         iter != af.tps().end(); iter++) {
        const bool result = run_test(tp / *iter, w,
            impl::merge_configs(af.conf(), test_suite_vars), ro_workdir);
        ok &= (result == EXIT_SUCCESS);
    }

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

impl::test_case_result
atf_run::get_test_case_result(const std::string& broken_reason,
                              const atf::process::status& s,
                              const atf::fs::path& resfile)
    const
{
    if (!broken_reason.empty())
        return impl::test_case_result("failed", broken_reason);

    if (s.exited()) {
        try {
            const impl::test_case_result tcr = impl::read_test_case_result(
                resfile);
            if (tcr.state() == "failed") {
                if (s.exitstatus() == EXIT_SUCCESS)
                    return impl::test_case_result("failed", "Test case "
                        "exited successfully but reported failure");
            } else {
                if (s.exitstatus() != EXIT_SUCCESS)
                    return impl::test_case_result("failed", "Test case "
                        "exited with error but reported success");
            }
            return tcr;
        } catch (const std::runtime_error& e) {
            return impl::test_case_result("failed", "Test case exited "
                "normally but failed to create the results file: " +
                std::string(e.what()));
        }
    } else if (s.signaled()) {
        return impl::test_case_result("failed", "Test program received "
            "signal " + atf::text::to_string(s.termsig()) +
            (s.coredump() ? " (core dumped)" : ""));
    } else {
        UNREACHABLE;
        throw std::runtime_error("Unknown exit status");
    }
}

int
atf_run::run_test_program(const atf::fs::path& tp,
                          impl::atf_tps_writer& w,
                          const atf::tests::vars_map& config,
                          const atf::fs::path& ro_workdir)
{
    int errcode = EXIT_SUCCESS;

    impl::metadata md;
    try {
        md = impl::get_metadata(tp, config);
    } catch (const atf::parser::format_error& e) {
        w.start_tp(tp.str(), 0);
        w.end_tp("Invalid format for test case list: " + std::string(e.what()));
        return EXIT_FAILURE;
    } catch (const atf::parser::parse_errors& e) {
        const std::string reason = atf::text::join(e, "; ");
        w.start_tp(tp.str(), 0);
        w.end_tp("Invalid format for test case list: " + reason);
        return EXIT_FAILURE;
    }

    impl::temp_dir resdir(atf::fs::path(atf::config::get("atf_workdir")) /
                          "atf-run.XXXXXX");

    w.start_tp(tp.str(), md.test_cases.size());
    if (md.test_cases.empty()) {
        w.end_tp("Bogus test program: reported 0 test cases");
        errcode = EXIT_FAILURE;
    } else {
        for (std::map< std::string, atf::tests::vars_map >::const_iterator iter
             = md.test_cases.begin(); iter != md.test_cases.end(); iter++) {
            const std::string& tcname = (*iter).first;
            const atf::tests::vars_map& tcmd = (*iter).second;

            w.start_tc(tcname);

            try {
                const std::string& reqfail = impl::check_requirements(
                    tcmd, config);
                if (!reqfail.empty()) {
                    w.end_tc("skipped", reqfail);
                    continue;
                }
            } catch (const std::runtime_error& e) {
                w.end_tc("failed", e.what());
                errcode = EXIT_FAILURE;
                continue;
            }

            const atf::fs::path resfile = resdir.get_path() / "tcr";
            INV(!atf::fs::exists(resfile));
            try {
                const bool has_cleanup = atf::text::to_bool(
                    (*tcmd.find("has.cleanup")).second);
                const bool use_fs = atf::text::to_bool(
                    (*tcmd.find("use.fs")).second);

                impl::test_case_result tcr("passed", "");

                if (use_fs) {
                    impl::temp_dir workdir(atf::fs::path(atf::config::get(
                        "atf_workdir")) / "atf-run.XXXXXX");

                    std::pair< std::string, const atf::process::status > s =
                        impl::run_test_case(tp, tcname, "body", tcmd, config,
                                            resfile, workdir.get_path(), w);
                    if (has_cleanup)
                        (void)impl::run_test_case(tp, tcname, "cleanup", tcmd,
                                config, resfile, workdir.get_path(), w);

                    // TODO: Force deletion of workdir.

                    tcr = get_test_case_result(s.first, s.second, resfile);
                } else {
                    std::pair< std::string, const atf::process::status > s =
                        impl::run_test_case(tp, tcname, "body", tcmd, config,
                                            resfile, ro_workdir, w);
                    if (has_cleanup)
                        (void)impl::run_test_case(tp, tcname, "cleanup", tcmd,
                            config, resfile, ro_workdir, w);

                    tcr = get_test_case_result(s.first, s.second, resfile);
                }
                w.end_tc(tcr.state(), tcr.reason());
                if (tcr.state() == "failed")
                    errcode = EXIT_FAILURE;
            } catch (...) {
                if (atf::fs::exists(resfile))
                    atf::fs::remove(resfile);
                throw;
            }
            if (atf::fs::exists(resfile))
                atf::fs::remove(resfile);

        }
        w.end_tp("");
    }

    return errcode;
}

size_t
atf_run::count_tps(std::vector< std::string > tps)
    const
{
    size_t ntps = 0;

    for (std::vector< std::string >::const_iterator iter = tps.begin();
         iter != tps.end(); iter++) {
        atf::fs::path tp(*iter);
        atf::fs::file_info fi(tp);

        if (fi.get_type() == atf::fs::file_info::dir_type) {
            impl::atffile af = impl::read_atffile(tp / "Atffile");
            std::vector< std::string > aux = af.tps();
            for (std::vector< std::string >::iterator i2 = aux.begin();
                 i2 != aux.end(); i2++)
                *i2 = (tp / *i2).str();
            ntps += count_tps(aux);
        } else
            ntps++;
    }

    return ntps;
}

static
void
call_hook(const std::string& tool, const std::string& hook)
{
    const atf::fs::path sh(atf::config::get("atf_shell"));
    const atf::fs::path hooks =
        atf::fs::path(atf::config::get("atf_pkgdatadir")) / (tool + ".hooks");

    const atf::process::status s =
        atf::process::exec(sh,
                           atf::process::argv_array(sh.c_str(), hooks.c_str(),
                                                    hook.c_str(), NULL),
                           atf::process::stream_inherit(),
                           atf::process::stream_inherit());


    if (!s.exited() || s.exitstatus() != EXIT_SUCCESS)
        throw std::runtime_error("Failed to run the '" + hook + "' hook "
                                 "for '" + tool + "'");
}

int
atf_run::main(void)
{
    impl::atffile af = impl::read_atffile(atf::fs::path("Atffile"));

    std::vector< std::string > tps;
    tps = af.tps();
    if (m_argc >= 1) {
        // TODO: Ensure that the given test names are listed in the
        // Atffile.  Take into account that the file can be using globs.
        tps.clear();
        for (int i = 0; i < m_argc; i++)
            tps.push_back(m_argv[i]);
    }

    // Read configuration data for this test suite.
    atf::tests::vars_map test_suite_vars;
    {
        atf::tests::vars_map::const_iterator iter =
            af.props().find("test-suite");
        INV(iter != af.props().end());
        test_suite_vars = impl::read_config_files((*iter).second);
    }

    impl::atf_tps_writer w(std::cout);
    call_hook("atf-run", "info_start_hook");
    w.ntps(count_tps(tps));

    impl::temp_dir ro_workdir(atf::fs::path(atf::config::get(
        "atf_workdir")) / "atf-run.XXXXXX");
    if (::chmod(ro_workdir.get_path().c_str(), S_IXUSR) == -1)
        throw std::runtime_error("Failed to create read-only work directory");
    if (!impl::set_immutable(ro_workdir.get_path(), true)) {
        // TODO: Report that use.fs may not work.  Non-fatal though.
    }

    bool ok = true;
    for (std::vector< std::string >::const_iterator iter = tps.begin();
         iter != tps.end(); iter++) {
        const bool result = run_test(atf::fs::path(*iter), w,
            impl::merge_configs(af.conf(), test_suite_vars),
                                ro_workdir.get_path());
        ok &= (result == EXIT_SUCCESS);
    }

    call_hook("atf-run", "info_end_hook");

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
main(int argc, char* const* argv)
{
    return atf_run().run(argc, argv);
}
