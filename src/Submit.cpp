//
// Created by torapture on 17-11-16.
//

#include "Submit.h"

#include <spdlog/spdlog.h>

#include <algorithm>

#include "Config.h"
#include "DatabaseHandler.h"
#include "Runner.h"
#include "Utils.h"

Submit::Submit() {
}

void Submit::set_runid(int runid) {
    this->runid = runid;
}

void Submit::set_uid(int uid) {
    this->uid = uid;
}

int Submit::get_uid() {
    return this->uid;
}

void Submit::set_pid(int pid) {
    this->pid = pid;
}

int Submit::get_pid() {
    return this->pid;
}

void Submit::set_contest_id(int contest_id) {
    this->contest_id = contest_id;
}

std::string Submit::get_contest_id_str() {
    if (this->contest_id == 0) {
        return "None";
    }
    return std::to_string(this->contest_id);
}

void Submit::set_time_limit_ms(int time_limit_ms) {
    this->time_limit_ms = time_limit_ms;
}

void Submit::set_memory_limit_kb(int memory_limit_kb) {
    this->memory_limit_kb = memory_limit_kb;
}

void Submit::set_language(int language) {
    this->language = language;
}

void Submit::set_is_spj(int is_spj) {
    this->is_spj = is_spj;
}

void Submit::set_std_input_file(const std::string &std_input_file) {
    this->std_input_file = std_input_file;
}

void Submit::set_std_output_file(const std::string &std_output_file) {
    this->std_output_file = std_output_file;
}

void Submit::set_user_output_file(const std::string &user_output_file) {
    this->user_output_file = user_output_file;
}

void Submit::set_src(const std::string &src) {
    this->src = src;
}

void Submit::work() {
    RunResult result;
    DatabaseHandler db;

    /// must have input and output file even if they are empty
    if (!Utils::check_file(std_input_file) || !Utils::check_file(std_output_file)) {
        result = RunResult::JUDGE_ERROR;
        db.change_run_result(runid, result);
        SPDLOG_ERROR("problem: {:d} does not have input_file or output_file supposed in {:s} and {:s}", pid, std_input_file, std_output_file);
        SPDLOG_INFO("result runid: {:d} {:s}", runid, result.status);
        return;
    }

    if (language != Config::CPP_LANG && language != Config::CPP11_LANG) {
        time_limit_ms *= Config::get_instance()->get_vm_multiplier();
        memory_limit_kb *= Config::get_instance()->get_vm_multiplier();
    }

    Runner run(time_limit_ms, memory_limit_kb, language, src);

    SPDLOG_INFO("compiling runid: {:d}", runid);
    db.change_run_result(runid, RunResult::COMPILING);
    result = run.compile();

    if (result == RunResult::COMPILE_SUCCESS) {
        SPDLOG_INFO("running runid: {:d}", runid);
        db.change_run_result(runid, RunResult::RUNNING);
        result = run.run(std_input_file);
        if (result == RunResult::RUN_SUCCESS) {
            if (is_spj) {
                result = spj_check().set_time_used(result.time_used_ms).set_memory_used(result.memory_used_kb);
            } else {
                result = normal_check().set_time_used(result.time_used_ms).set_memory_used(result.memory_used_kb);
            }
        }
    }
    std::string output_file = Config::get_instance()->get_temp_path() + Config::get_instance()->get_output_file();
    if (Utils::check_file(output_file)) Utils::delete_file(output_file);

    SPDLOG_INFO("↥↥↥↥↥↥↥↥↥↥RESULT runid[{:d}] status[{:s}] time[{:d}ms] memory[{:d}kb]↥↥↥↥↥↥↥↥↥↥\n", runid, result.status, result.time_used_ms,
                result.memory_used_kb);

    if (result == RunResult::ACCEPTED) {
        if (contest_id == 0) {
            if (!db.already_accepted(uid, pid)) {
                db.add_user_total_solved(uid);
            }
            db.add_user_total_accepted(uid);
        } else {
            db.add_contest_total_accepted(contest_id, pid);
        }
    }

    db.change_run_result(runid, result);

    if (contest_id == 0)
        db.add_problem_result(pid, result);
}

RunResult Submit::spj_check() {
    SPDLOG_INFO("spj check runid: {:d}", runid);

    std::string spj_src_file = Config::get_instance()->get_spj_files_path() + std::to_string(pid) +
                               "/spj" + Config::get_instance()->get_src_extention(is_spj);

    if (!Utils::check_file(spj_src_file)) {
        SPDLOG_ERROR("spj for problem: {:d} need {:s}", pid, spj_src_file);
        return RunResult::JUDGE_ERROR;
    }

    std::string spj_src = Utils::get_content_from_file(spj_src_file);

    Runner spj(Config::get_instance()->get_spj_run_time_ms(), Config::get_instance()->get_spj_memory_kb(),
               is_spj, spj_src);

    std::string spj_std_input = Config::get_instance()->get_temp_path() + "spj_std_input";
    std::string spj_std_output = Config::get_instance()->get_temp_path() + "spj_std_output";
    std::string spj_user_output = Config::get_instance()->get_temp_path() + "spj_user_output";

    system(("cp " + std_input_file + " " + spj_std_input).c_str());
    system(("cp " + std_output_file + " " + spj_std_output).c_str());
    system(("cp " + user_output_file + " " + spj_user_output).c_str());
    if (chown(spj_std_input.c_str(), Config::get_instance()->get_low_privilege_uid(), Config::get_instance()->get_low_privilege_uid()) == -1) {
        SPDLOG_ERROR("chown {:s} failed: {:s}", spj_std_input, strerror(errno));
        return RunResult::JUDGE_ERROR;
    }
    if (chown(spj_std_output.c_str(), Config::get_instance()->get_low_privilege_uid(), Config::get_instance()->get_low_privilege_uid()) == -1) {
        SPDLOG_ERROR("chown {:s} failed: {:s}", spj_std_output, strerror(errno));
        return RunResult::JUDGE_ERROR;
    }
    if (chown(spj_user_output.c_str(), Config::get_instance()->get_low_privilege_uid(), Config::get_instance()->get_low_privilege_uid()) == -1) {
        SPDLOG_ERROR("chown {:s} failed: {:s}", spj_user_output, strerror(errno));
        return RunResult::JUDGE_ERROR;
    }

    std::string spj_info = spj_std_input + "\n" + spj_std_output + "\n" + spj_user_output + "\n";

    std::string spj_info_file = Config::get_instance()->get_temp_path() + "spj_info";

    Utils::save_to_file(spj_info_file, spj_info);

    SPDLOG_INFO("spj compiling runid: {:d}", runid);

    RunResult result = spj.compile();

    if (result == RunResult::COMPILE_ERROR) {
        SPDLOG_ERROR("spj program for problem: {:d} in {:s} have Compile Error", pid, spj_src_file);
        return RunResult::JUDGE_ERROR;
    }

    SPDLOG_INFO("spj running runid: {:d}", runid);
    result = spj.run(spj_info_file);

    SPDLOG_INFO("spj result runid: {:d} {:s}", runid, result.status);

    if (Utils::check_file(spj_info_file)) Utils::delete_file(spj_info_file);
    if (Utils::check_file(spj_std_input)) Utils::delete_file(spj_std_input);
    if (Utils::check_file(spj_std_output)) Utils::delete_file(spj_std_output);
    if (Utils::check_file(spj_user_output)) Utils::delete_file(spj_user_output);

    if (result == RunResult::RUN_SUCCESS)
        return RunResult::ACCEPTED;
    else
        return RunResult::WRONG_ANSWER;
}

RunResult Submit::normal_check() {
    SPDLOG_INFO("normal check runid: {:d}", runid);

    bool aced = true, peed = true;
    FILE *program_out, *standard_out;
    int eofp = EOF, eofs = EOF;
    program_out = fopen(user_output_file.c_str(), "r");
    standard_out = fopen(std_output_file.c_str(), "r");

    char po_char, so_char;
    while (1) {
        while ((eofs = fscanf(standard_out, "%c", &so_char)) != EOF &&
               so_char == '\r')
            ;
        while ((eofp = fscanf(program_out, "%c", &po_char)) != EOF &&
               po_char == '\r')
            ;
        if (eofs == EOF || eofp == EOF) break;
        if (so_char != po_char) {
            aced = false;
            break;
        }
    }
    while ((so_char == '\n' || so_char == '\r') &&
           ((eofs = fscanf(standard_out, "%c", &so_char)) != EOF))
        ;
    while ((po_char == '\n' || po_char == '\r') &&
           ((eofp = fscanf(program_out, "%c", &po_char)) != EOF))
        ;
    if (eofp != eofs) aced = false;
    fclose(program_out);
    fclose(standard_out);
    if (!aced) {
        program_out = fopen(user_output_file.c_str(), "r");
        standard_out = fopen(std_output_file.c_str(), "r");
        while (1) {
            while ((eofs = fscanf(standard_out, "%c", &so_char)) != EOF &&
                   (so_char == ' ' || so_char == '\n' || so_char == '\r'))
                ;
            while ((eofp = fscanf(program_out, "%c", &po_char)) != EOF &&
                   (po_char == ' ' || po_char == '\n' || po_char == '\r'))
                ;
            if (eofs == EOF || eofp == EOF) break;
            if (so_char != po_char) {
                peed = false;
                break;
            }
        }
        while ((so_char == ' ' || so_char == '\n' || so_char == '\r') &&
               ((eofs = fscanf(standard_out, "%c", &so_char)) != EOF))
            ;
        while ((po_char == ' ' || po_char == '\n' || po_char == '\r') &&
               ((eofp = fscanf(program_out, "%c", &po_char)) != EOF))
            ;
        if (eofp != eofs) {
            peed = false;
        }
        fclose(program_out);
        fclose(standard_out);
    }
    if (aced)
        return RunResult::ACCEPTED;
    else if (peed)
        return RunResult::PRESENTATION_ERROR;
    else
        return RunResult::WRONG_ANSWER;
}

Submit *Submit::get_from_runid(int runid) {
    DatabaseHandler db;
    auto run = db.get_run_stat(runid);
    int pid = atoi(run["problem_id"].c_str());
    int uid = atoi(run["user_id"].c_str());
    int contest_id = atoi(run["contest_id"].c_str());
    auto problem_info = db.get_problem_description(pid);
    Submit *submit = new Submit();
    submit->set_runid(runid);
    submit->set_pid(pid);
    submit->set_uid(uid);
    submit->set_contest_id(contest_id);
    submit->set_time_limit_ms(atoi(problem_info["time_limit"].c_str()));
    submit->set_memory_limit_kb(atoi(problem_info["memory_limit"].c_str()));
    submit->set_language(atoi(run["language_id"].c_str()));
    submit->set_is_spj(atoi(problem_info["is_special_judge"].c_str()));
    submit->set_std_input_file(Utils::get_input_file(pid));
    submit->set_std_output_file(Utils::get_output_file(pid));
    submit->set_user_output_file(Utils::get_user_output_file());
    submit->set_src(run["source"]);
    return submit;
}
