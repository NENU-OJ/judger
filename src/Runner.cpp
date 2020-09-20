//
// Created by torapture on 17-11-12.
//

#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <spdlog/spdlog.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <syscall.h>

#include "Runner.h"
#include "Config.h"
#include "Utils.h"


Runner::Runner() {
    this->time_limit_ms = DEFAULT_TIME_LIMIT_MS;
    this->memory_limit_kb = DEFAULT_MEMORY_LIMIT_KB;

    this->language = Config::CPP_LANG;
    this->src = "main(){}";

    this->input_file = "";
    this->src_file_name =
            Config::get_instance()->get_temp_path() +
            Config::get_instance()->get_source_file() +
            Config::get_instance()->get_src_extention(language);
    this->exc_file_name =
            Config::get_instance()->get_temp_path() +
            Config::get_instance()->get_binary_file() +
            Config::get_instance()->get_exc_extentino(language);
}

Runner::Runner(int time_limit_ms, int memory_limit_kb,
               int language, const std::string &src) {

    this->time_limit_ms = time_limit_ms;
    this->memory_limit_kb = memory_limit_kb;

    this->language = language;
    this->src = src;

    this->input_file = "";
    this->src_file_name =
            Config::get_instance()->get_temp_path() +
            Config::get_instance()->get_source_file() +
            Config::get_instance()->get_src_extention(language);
    this->exc_file_name =
            Config::get_instance()->get_temp_path() +
            Config::get_instance()->get_binary_file() +
            Config::get_instance()->get_exc_extentino(language);
}

void Runner::child_compile() {

    /// set time limit
    itimerval itv;
    itv.it_value.tv_sec = Config::get_instance()->get_compile_time_ms() / 1000;
    itv.it_value.tv_usec = Config::get_instance()->get_compile_time_ms() % 1000 * 1000;
    itv.it_interval.tv_sec = 0;
    itv.it_interval.tv_usec = 0;
    setitimer(ITIMER_REAL, &itv, NULL);

    /// redirect stderr stream
    std::string ce_info_file = Config::get_instance()->get_temp_path() + Config::get_instance()->get_ce_info_file();
    freopen(ce_info_file.c_str(), "w", stderr);

    /// set uid
    if (setuid(Config::get_instance()->get_low_privilege_uid()) < 0) {
        fputs("setuid before compile fail.", stderr);
        SPDLOG_ERROR("can't setuid, maybe you should sudo or choose a right uid");
        exit(1);
    }

    /// compile
    if (Config::CPP_LANG == language) {
        execl("/usr/bin/g++", "g++", src_file_name.c_str(), "-o",
              exc_file_name.c_str(), "-DONLINE_JUDGE", "-fmax-errors=64", "-O2", "-fno-asm", "-Wall", "-lm", NULL);
    } else if (Config::CPP11_LANG == language) {
        execl("/usr/bin/g++", "g++", "-std=c++11", src_file_name.c_str(), "-o",
              exc_file_name.c_str(), "-DONLINE_JUDGE", "-fmax-errors=64", "-O2", "-fno-asm", "-Wall", "-lm", NULL);
    } else if (Config::JAVA_LANG == language) {
        execl("/usr/bin/javac", "javac", "-g:none", "-Xlint",
              src_file_name.c_str(), NULL);
    } else if (Config::PY2_LANG == language) {
        execl("/usr/bin/python2", "python2", "-c",
              ((std::string) "import py_compile; py_compile.compile('" +
               src_file_name + "')").c_str(),
              NULL);
    } else if (Config::PY3_LANG == language) {
        execl("/usr/bin/python3", "python3", "-c",
              ((std::string) "import py_compile; py_compile.compile('" +
               src_file_name + "', cfile='" + exc_file_name + "')").c_str(),
              NULL);
    }
    /// Not suppose to get here if compile success
    fputs("Fucking Error.", stderr);
    exit(1);
}

RunResult Runner::compile() {
    pid_t cid;

    /// write to src file
    if (!Utils::save_to_file(src_file_name, src)) {
        SPDLOG_ERROR("Can not save source code to file: {:s}", src_file_name);
        return RunResult::JUDGE_ERROR;
    }

    /// try to compile
    if ((cid = fork()) == -1) {
        return RunResult::JUDGE_ERROR;
    } else if (cid == 0) { // child process
        child_compile();
    } else { // father process
        SPDLOG_INFO("child compiler process pid is {:d}", cid);
        int status;
        rusage run_info;

        if (wait4(cid, &status, WUNTRACED, &run_info) == -1)
            return RunResult::JUDGE_ERROR;

        int time_used_ms = get_time_ms(run_info);
        int memory_used_kb = get_memory_kb(run_info);

        std::string ce_info_file = Config::get_instance()->get_temp_path() + Config::get_instance()->get_ce_info_file();
        std::string ce_info = Utils::get_content_from_file(ce_info_file);

        if (Utils::check_file(src_file_name)) Utils::delete_file(src_file_name);
        if (Utils::check_file(ce_info_file)) Utils::delete_file(ce_info_file);

        if (((language != Config::PY2_LANG && language != Config::PY3_LANG) || ce_info.empty())
            && WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            return RunResult::COMPILE_SUCCESS.set_time_used(time_used_ms).set_memory_used(memory_used_kb);
        } else {
            if (WIFSIGNALED(status) && WTERMSIG(status) == SIGALRM) { // compile time limit exceeded
                time_used_ms = Config::get_instance()->get_compile_time_ms();
                ce_info = "Compile time limit exceeded.";
                // FIXME: deal with zombie process
                // dirty way to kill the subprocess, maybe clean it later
                // g++'s child process cc1plus will become orphan process when g++ is killed
                int code = system("pkill cc1plus");
                SPDLOG_INFO("compile time limit exceeded, run cmd[pkill cc1plus] to kill the compiler, pkill exit code: {:d}", code);
            }
            return RunResult::COMPILE_ERROR.set_time_used(time_used_ms).set_memory_used(memory_used_kb).set_ce_info(ce_info);
        }
    }
}

void Runner::child_run() {

    /// set time limit
    int total_run_time_ms = time_limit_ms + Config::get_instance()->get_extra_runtime(); // extra runtime for I/O and Context switching

    itimerval itv;
    itv.it_value.tv_sec = total_run_time_ms / 1000;
    itv.it_value.tv_usec = (total_run_time_ms % 1000) * 1000;
    itv.it_interval.tv_sec = 0;
    itv.it_interval.tv_usec = 0;
    setitimer(ITIMER_PROF, &itv, NULL); // CPU time

    /// set stack limit
    rlimit stack_limit;
    stack_limit.rlim_max = stack_limit.rlim_cur = rlim_t(Config::get_instance()->get_stack_limit_kb()) * 1024;
    setrlimit(RLIMIT_STACK, &stack_limit);

    /// set output limit
    rlimit output_limit;
    output_limit.rlim_max = output_limit.rlim_cur = rlim_t(Config::get_instance()->get_max_output_limit()) * 1024 * 1024;
    setrlimit(RLIMIT_FSIZE, &output_limit);

    /// set uid
    if (setuid(Config::get_instance()->get_low_privilege_uid()) < 0) {
        SPDLOG_ERROR("can't setuid, maybe you should sudo or choose a right uid");
        exit(1);
    }

    /// redirect stdin stream
    if (!input_file.empty())
        freopen(input_file.c_str(), "r", stdin);

    /// redirect stdout stream
    std::string output_file = Config::get_instance()->get_temp_path() + Config::get_instance()->get_output_file();
    freopen(output_file.c_str(), "w", stdout);

    /// redirect stderr stream
    std::string stderr_file = Config::get_instance()->get_temp_path() + Config::get_instance()->get_stderr_file();
    freopen(stderr_file.c_str(), "w", stderr);

    if (language == Config::JAVA_LANG) {
        chdir(Config::get_instance()->get_temp_path().c_str());
    } else {
        /// set process num limit
        rlimit nproc_limit;
        nproc_limit.rlim_max = nproc_limit.rlim_cur = 1;
        setrlimit(RLIMIT_NPROC, &nproc_limit);
    }

    /// start ptrace
    ptrace(PTRACE_TRACEME, 0, NULL, NULL);

    /// run
    if (Config::CPP_LANG == language || Config::CPP11_LANG == language) {
        execl(exc_file_name.c_str(), exc_file_name.c_str(), NULL);
    } else if (Config::JAVA_LANG == language) {
        execl("/usr/bin/java", "java", "-Djava.security.manager",
              "-Djava.security.policy=java.policy", "-client",
              Config::get_instance()->get_binary_file().c_str(), NULL);
    } else if (Config::PY2_LANG == language) {
        execl("/usr/bin/python2", "python2", exc_file_name.c_str(), NULL);
    } else if (Config::PY3_LANG == language) {
        execl("/usr/bin/python3", "python3", exc_file_name.c_str(), NULL);
    }
}

RunResult Runner::father_run(pid_t cid) {
    SPDLOG_INFO("child runner process pid is {:d}", cid);

    RunResult result = RunResult::RUN_SUCCESS;
    int status;
    bool first_exec = false;
    bool first_wait = true;
    do {
        rusage run_info;
        if (wait4(cid, &status, 0, &run_info) == -1) {
            SPDLOG_ERROR("wait4 error: {:s}", strerror(errno));
            return RunResult::JUDGE_ERROR;
        }
        result = result.set_time_used(get_time_ms(run_info)).set_memory_used(get_memory_kb(run_info));

        if (first_wait) {
            if (ptrace(PTRACE_SETOPTIONS, cid, 0, PTRACE_O_TRACESYSGOOD) == -1) {
                SPDLOG_ERROR("PTRACE_SETOPTIONS error: {:s}", strerror(errno));
            }
            first_wait = false;
        }

        // stopped by system call
        if (WIFSTOPPED(status) && (WSTOPSIG(status) == (SIGTRAP | 0x80))) {
            unsigned long long syscall = get_syscall(cid);
            if (called_restricted_function(language, syscall, first_exec)) {
                result.status = RunResult::RESTRICTED_FUNCTION.status;
                SPDLOG_INFO("child[{:d}] called restricted function: {:d}", cid, syscall);
                if (kill(cid, SIGKILL) == -1) {
                    SPDLOG_ERROR("kill cid[{:d}] error: {:s}", cid, strerror(errno));
                }
                continue;
            }

            if (syscall == SYS_execve) {
                first_exec = false;
            }
            if (ptrace(PTRACE_SYSCALL, cid, NULL, NULL) == -1) {
                SPDLOG_ERROR("PTRACE_SYSCALL error: {:s}", strerror(errno));
            }
            continue;
        }


        if (WIFEXITED(status)) {
            SPDLOG_INFO("cid[{:d}] exited, status: {:d}", cid, WEXITSTATUS(status));
            if (WEXITSTATUS(status) != 0) {
                result.status = RunResult::RUNTIME_ERROR.status;
                SPDLOG_INFO("runtime error because exit status: {:d} is not zero", WEXITSTATUS(status));
            } else if (result.time_used_ms > time_limit_ms) {
                result.status = RunResult::TIME_LIMIT_EXCEEDED.status;
                SPDLOG_INFO("time limit exceeded after run successfully, time used: {:d}ms", result.time_used_ms);
            } else if (result.memory_used_kb > memory_limit_kb) {
                result.status = RunResult::MEMORY_LIMIT_EXCEEDED.status;
                SPDLOG_INFO("memory limit exceeded after run successfully, memory used: {:d}kb", result.memory_used_kb);
            }
        } else if (WIFSIGNALED(status) || WIFSTOPPED(status)) {
            int sig;
            if (WIFSIGNALED(status)) {
                sig = WTERMSIG(status);
                SPDLOG_INFO("killed by signal: {:d} {:s}", sig, strsignal(sig));
            } else {
                sig = WSTOPSIG(status);
                SPDLOG_INFO("stopped by signal: {:d} {:s}", sig, strsignal(sig));

                if (sig == SIGTRAP) {
                    SPDLOG_INFO("continue running because stopped by SIGTRAP");
                    if (ptrace(PTRACE_SYSCALL, cid, NULL, NULL) == -1) {
                        SPDLOG_ERROR("PTRACE_SYSCALL error: {:s}", strerror(errno));
                    }
                    continue;
                }
            }

            if (sig == SIGPROF) {
                result.status = RunResult::TIME_LIMIT_EXCEEDED.status;
                SPDLOG_INFO("time limit exceeded by signal: {:d} {:s}", sig, strsignal(sig));
            } else if (sig == SIGXFSZ) {
                result.status = RunResult::OUTPUT_LIMIT_EXCEEDED.status;
                SPDLOG_INFO("output limit exceeded by signal: {:d} {:s}", sig, strsignal(sig));
            } else {
                if (result == RunResult::RUN_SUCCESS) {
                    result.status = RunResult::RUNTIME_ERROR.status;
                    SPDLOG_INFO("runtime error by signal: {:d} {:s}", sig, strsignal(sig));
                }
            }
            if (WIFSTOPPED(status)) {
                SPDLOG_INFO("kill cid[{:d}] because stopped by signal", cid);
                if (kill(cid, SIGKILL) == -1) {
                    SPDLOG_ERROR("kill cid[{:d}] error: {:s}", cid, strerror(errno));
                }
            }
        } else { // continued
            result.status = RunResult::RUNTIME_ERROR.status;
            SPDLOG_INFO("runtime error because continued");

            SPDLOG_INFO("kill cid[{:d}] because continued", cid);
            if (kill(cid, SIGKILL) == -1) {
                SPDLOG_ERROR("kill cid[{:d}] error: {:s}", cid, strerror(errno));
            }
        }
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));

    return result;
}

// before calling this function, must call function compile first
RunResult Runner::run(const std::string &input_file) {
    this->input_file = input_file;

    pid_t cid = fork();

    if (cid == -1) {
        SPDLOG_ERROR("fork error: {:s}", strerror(errno));
        return RunResult::JUDGE_ERROR;
    } else if (cid == 0) {
        child_run();
    }

    RunResult result = father_run(cid);
    SPDLOG_INFO("result: {:s}, time used: {:d}ms, memory used: {:d}kb", result.status, result.time_used_ms, result.memory_used_kb);
    if (result == RunResult::TIME_LIMIT_EXCEEDED) {
        result.time_used_ms = std::max(result.time_used_ms, time_limit_ms);
    }

    std::string stderr_file = Config::get_instance()->get_temp_path() + Config::get_instance()->get_stderr_file();
    if (Utils::check_file(exc_file_name)) Utils::delete_file(exc_file_name);
    if (Utils::check_file(stderr_file)) Utils::delete_file(stderr_file);

    return result;
}

RunResult Runner::run() {
    return run("");
}

int Runner::get_time_ms(const rusage &run_info) {
    return run_info.ru_utime.tv_sec * 1000 + run_info.ru_utime.tv_usec / 1000 +
           run_info.ru_stime.tv_sec * 1000 + run_info.ru_stime.tv_usec / 1000;
}

int Runner::get_memory_kb(const rusage &run_info) {
    return run_info.ru_minflt * (getpagesize() / 1024);
}

unsigned long long Runner::get_syscall(pid_t pid) {
    user_regs_struct reg;
    if (ptrace(PTRACE_GETREGS, pid, NULL, &reg) == -1) {
        SPDLOG_ERROR("PTRACE_GETREGS error: {:s}", strerror(errno));
        return -1;
    }
    return reg.orig_rax;
}

bool Runner::called_restricted_function(int language, unsigned long long syscall, bool first_exec) {
    if (syscall < 0)
        return false;

    if (syscall == SYS_execve && first_exec)
        return false;

    return Config::get_instance()->is_restricted_call(language, syscall);
}
