//
// Created by torapture on 17-11-12.
//

#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <sys/syscall.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/daily_file_sink.h>

#include "Config.h"

Config * Config::instance = new Config("config.ini");

const int Config::CPP_LANG = 1;
const int Config::CPP11_LANG = 2;
const int Config::JAVA_LANG = 3;
const int Config::PY2_LANG = 4;
const int Config::PY3_LANG = 5;

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

    } catch (const spdlog::spdlog_ex& ex) {
        std::cout << "log initialization failed: " << ex.what() << std::endl;
        exit(1);
    }
}


Config::Config(std::string config_file) {
	
    init_logger();

	print_logo();

	std::ifstream file(config_file.c_str());

	if (!file) {
		SPDLOG_ERROR("config file: [{:s}] does not exist", config_file);
		exit(1);
	} else {

		restricted_call[CPP_LANG] = restricted_call[CPP11_LANG] = restricted_call[PY2_LANG] = restricted_call[PY3_LANG] = {
			SYS__sysctl,
			SYS_chdir,
			SYS_chmod,
			SYS_chown,
			SYS_chroot,
			SYS_clone,
			SYS_creat,
			SYS_create_module,
			SYS_delete_module,
			SYS_execve,
			SYS_fork,
			SYS_getpgrp,
			SYS_kill,
			SYS_mkdir,
			SYS_mknod,
			SYS_mount,
			SYS_rmdir,
			SYS_ptrace,
			SYS_reboot,
			SYS_rename,
			SYS_restart_syscall,
			SYS_select,
			SYS_setgid,
			SYS_setitimer,
			SYS_setgroups,
			SYS_sethostname,
			SYS_setrlimit,
			SYS_setuid,
			SYS_settimeofday,
			SYS_tkill,
			SYS_vfork,
			SYS_vhangup,
			SYS_vserver,
			SYS_wait4,
			SYS_clock_nanosleep,
			SYS_nanosleep,
			SYS_pause,
#ifndef __i386__
			SYS_accept,
			SYS_bind,
			SYS_connect,
			SYS_listen,
			SYS_socket
#else
			SYS_signal,
			SYS_waitpid,
			SYS_nice,
			SYS_waitpid,
			SYS_umount,
			SYS_socketcall
#endif
		}; // end of restricted_call

		restricted_call[JAVA_LANG] = {
			SYS__sysctl,
			SYS_chdir,
			SYS_chmod,
			SYS_chown,
			SYS_chroot,
			SYS_creat,
			SYS_create_module,
			SYS_delete_module,
			SYS_execve,
			SYS_fork,
			SYS_getpgrp,
			SYS_kill,
			SYS_mkdir,
			SYS_mknod,
			SYS_mount,
			SYS_rmdir,
			SYS_ptrace,
			SYS_reboot,
			SYS_rename,
			SYS_restart_syscall,
			SYS_select,
			SYS_setgid,
			SYS_setitimer,
			SYS_setgroups,
			SYS_sethostname,
			SYS_setrlimit,
			SYS_setuid,
			SYS_settimeofday,
			SYS_tkill,
			SYS_vfork,
			SYS_vhangup,
			SYS_vserver,
			SYS_wait4,
			SYS_clock_nanosleep,
			SYS_nanosleep,
			SYS_pause,
#ifndef __i386__
			SYS_accept,
			SYS_bind,
			SYS_connect,
			SYS_listen,
			SYS_socket
#else
			SYS_signal,
			SYS_waitpid,
			SYS_nice,
			SYS_waitpid,
			SYS_umount,
			SYS_socketcall
#endif
		}; // end of restricted_call

		src_extension[CPP_LANG] = ".cpp";
		src_extension[CPP11_LANG] = ".cpp";
		src_extension[JAVA_LANG] = ".java";
		src_extension[PY2_LANG] = ".py";
		src_extension[PY3_LANG] = ".py";

		exc_extension[CPP_LANG] = ".out";
		exc_extension[CPP11_LANG] = ".out";
		exc_extension[JAVA_LANG] = ".class";
		exc_extension[PY2_LANG] = ".pyc";
		exc_extension[PY3_LANG] = ".pyc";


		std::string key, eq, value;
		while (file >> key >> eq >> value) {
			config_map.insert({key, value});
		}

		std::vector<std::string> check_list = {
			"listen_port",
			"db_ip",
			"db_port",
			"db_name",
			"db_user",
			"db_password",
			"low_privilege_uid",
			"compile_time_ms",
			"stack_limit",
			"spj_run_time_ms",
			"spj_memory_kb",
			"source_file",
			"binary_file",
			"output_file",
			"ce_info_file",
			"temp_path",
			"max_output_limit",
			"test_files_path",
			"spj_files_path",
			"stderr_file",
			"vm_multiplier",
			"extra_runtime",
		    "connect_string",
		};

		for (const auto &key : check_list) {
			if (config_map.find(key) == config_map.end()) {
				SPDLOG_ERROR("need key: [{:s}] in config.ini", key);
				exit(1);
			}
		}

		listen_port = atoi(config_map["listen_port"].c_str());
		db_ip = config_map["db_ip"];
		db_port = atoi(config_map["db_port"].c_str());
		db_name = config_map["db_name"];
		db_user = config_map["db_user"];
		db_password = config_map["db_password"];
		low_privilege_uid = atoi(config_map["low_privilege_uid"].c_str());
		compile_time_ms = atoi(config_map["compile_time_ms"].c_str());
		stack_limit_kb = atoi(config_map["stack_limit"].c_str());
		spj_run_time_ms = atoi(config_map["spj_run_time_ms"].c_str());
		spj_memory_kb = atoi(config_map["spj_memory_kb"].c_str());
		source_file = config_map["source_file"];
		binary_file = config_map["binary_file"];
		output_file = config_map["output_file"];
		ce_info_file = config_map["ce_info_file"];
		temp_path = config_map["temp_path"];
		max_output_limit = atoi(config_map["max_output_limit"].c_str());
		vm_multiplier = atoi(config_map["vm_multiplier"].c_str());
		extra_runtime = atoi(config_map["extra_runtime"].c_str());
		test_files_path = config_map["test_files_path"];
		spj_files_path = config_map["spj_files_path"];
		stderr_file = config_map["stderr_file"];
		connect_string = config_map["connect_string"];
		SPDLOG_INFO("configuration is finished");
	}
}
