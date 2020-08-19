#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <queue>
#include <sys/socket.h>
#include <netinet/in.h>
#include <wait.h>

#include "Runner.h"
#include "Config.h"
#include "Utils.h"
#include "Submit.h"
#include "DatabaseHandler.h"
#include "Exception.h"

using namespace std;

static queue<Submit *> judge_queue;
static int sockfd;
static pthread_mutex_t queue_mtx = PTHREAD_MUTEX_INITIALIZER;

void print_logo() {
    const char *logo = "\n"
                       " __   __     ______     __   __     __  __     ______       __    \n"
                       "/\\ \"-.\\ \\   /\\  ___\\   /\\ \"-.\\ \\   /\\ \\/\\ \\   /\\  __ \\     /\\ \\   \n"
                       "\\ \\ \\-.  \\  \\ \\  __\\   \\ \\ \\-.  \\  \\ \\ \\_\\ \\  \\ \\ \\/\\ \\   _\\_\\ \\  \n"
                       " \\ \\_\\\\\"\\_\\  \\ \\_____\\  \\ \\_\\\\\"\\_\\  \\ \\_____\\  \\ \\_____\\ /\\_____\\ \n"
                       "  \\/_/ \\/_/   \\/_____/   \\/_/ \\/_/   \\/_____/   \\/_____/ \\/_____/ \n"
                       "                                                                  \n";
    fputs(logo, stderr);
}

void init_queue() {
    try {
        SPDLOG_INFO("init queue");
        DatabaseHandler db;
        auto unfinished_runs = db.get_unfinished_results();
        for (auto &run : unfinished_runs) {
            int runid = atoi(run["id"].c_str());
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

            judge_queue.push(submit);
            db.change_run_result(runid, RunResult::QUEUEING);
            SPDLOG_INFO("init enqueue runid: {:d}", runid);
        }
        SPDLOG_INFO("init queue finished");
    } catch (Exception &e) {
        SPDLOG_ERROR("{:s}", e.what());
        exit(1);
    }
}

void init_socket() {
    SPDLOG_INFO("init socket");

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        SPDLOG_ERROR("socket error: {:s}", strerror(errno));
        exit(1);
    }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(Config::get_instance()->get_listen_port());
    addr.sin_addr.s_addr = INADDR_ANY;
    bzero(&(addr.sin_zero), 8);

    SPDLOG_INFO("bind socket");
    if (bind(sockfd, (struct sockaddr *) &addr, sizeof(struct sockaddr)) == -1) {
        SPDLOG_ERROR("bind error: {:s}", strerror(errno));
        exit(1);
    }

    SPDLOG_INFO("start listen");
    if (listen(sockfd, 5) == -1) {
        SPDLOG_ERROR("listen error: {:s}", strerror(errno));
        exit(1);
    }

    SPDLOG_INFO("init socket finished");
}

int next_runid() {
    int cfd = accept(sockfd, NULL, NULL);
    if (cfd == -1) {
        SPDLOG_ERROR("accept error: {:s}", strerror(errno));
        throw Exception("accept returned -1");
    }
    SPDLOG_INFO("accepted connection fd: {:d}", cfd);

    // TODO: parse data more elegantly
    static char buf[128];
    int num_read = 0;
    int tries = 5;
    while (num_read == 0 && tries--) {
        num_read += read(cfd, buf, sizeof(buf));
    }
    buf[num_read] = '\0';
    close(cfd);

    std::vector<std::string> split_list = Utils::split(buf);
    if (split_list.size() < 2) {
        SPDLOG_ERROR("split_list.size: {:d}, buf: {:s}", split_list.size(), buf);
        throw Exception("Error runid request from web");
    }

    if (split_list[0] != Config::get_instance()->get_connect_string())
        throw Exception("Wrong connect_string");
    int runid = atoi(split_list[1].c_str()); // TODO: use safe method instead of dangerous atoi
    return runid;
}

void *listen_thread(void *arg) {
    while (true) {
        try {
            int runid = next_runid();
            Submit *submit = Submit::get_from_runid(runid);
            DatabaseHandler db;
            db.change_run_result(runid, RunResult::QUEUEING);
            pthread_mutex_lock(&queue_mtx);
            judge_queue.push(submit);
            SPDLOG_INFO("[listen thread] socket enqueue runid: {:d}", runid);
            pthread_mutex_unlock(&queue_mtx);
        } catch (Exception &e) {
            SPDLOG_ERROR("{:s}", e.what());
        }
    }
}

void *judge_thread(void *arg) {
    while (true) {
        usleep(61743);
        Submit *submit;
        bool have_run = false;
        pthread_mutex_lock(&queue_mtx);
        if (!judge_queue.empty()) {
            have_run = true;
            submit = judge_queue.front();
            judge_queue.pop();
        }
        pthread_mutex_unlock(&queue_mtx);

        if (!have_run) continue;

        try {
            SPDLOG_INFO("[judge thread] send runid: {:d} to work", submit->get_runid());
            submit->work();
            delete submit;
            submit = nullptr;
        } catch (Exception &e) {
            SPDLOG_ERROR("{:s}", e.what());
            if (submit != nullptr) {
                delete submit;
                submit = nullptr;
            }
        }
    }
}

void init_threads() {
    /// listen thread
    pthread_t tid_listen;
    if (pthread_create(&tid_listen, NULL, listen_thread, NULL) != 0) {
        SPDLOG_ERROR("cannot init listen thread!");
        exit(1);
    }
    if (pthread_detach(tid_listen) != 0) {
        SPDLOG_ERROR("cannot detach listen thread!");
        exit(1);
    }
    SPDLOG_INFO("listen thread init finished");

    /// judge_thread
    pthread_t tid_judge;
    if (pthread_create(&tid_judge, NULL, judge_thread, NULL) != 0) {
        SPDLOG_ERROR("cannot init judge thread!");
        exit(1);
    }
    if (pthread_detach(tid_judge) != 0) {
        SPDLOG_ERROR("cannot detach judge thread!");
        exit(1);
    }
    SPDLOG_INFO("judge thread init finished");
}

void test_set_uid() {
    pid_t pid;
    if ((pid = fork()) == 0) {
        if (setuid(Config::get_instance()->get_low_privilege_uid()) == -1) {
            exit(1);
        } else {
            exit(0);
        }
    } else {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) == 0) {
                SPDLOG_INFO("can set low_privilege_uid");
            } else if (WEXITSTATUS(status) == 1) {
                SPDLOG_ERROR("cannot set low_privilege_uid");
                exit(1);
            } else {
                SPDLOG_ERROR("unknown error");
                exit(1);
            }
        } else {
            SPDLOG_ERROR("unknown error");
            exit(1);
        }
    }
}

void init_logger() {
    const std::string pattern = "[%l] %Y-%m-%d %H:%M:%S,%e %P %t %s:%# %v";
    try {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::info);
        console_sink->set_pattern(pattern);

        auto file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>("logs/judger.txt", 23, 59);
        file_sink->set_level(spdlog::level::info);
        file_sink->set_pattern(pattern);

        spdlog::sinks_init_list sink_list = {file_sink, console_sink};
        spdlog::set_default_logger(std::make_shared<spdlog::logger>("multi_sink", sink_list));
        spdlog::flush_on(spdlog::level::info);

    } catch (const spdlog::spdlog_ex &ex) {
        std::cout << "log initialization failed: " << ex.what() << std::endl;
        exit(1);
    }
}

int main(int argc, const char *argv[]) {
    print_logo();
    init_logger();
    Config::instance = new Config("config.ini");

    test_set_uid();
    init_socket();
    init_queue();
    init_threads();

    while (true)
        sleep(3600);

    return 0;
}
