## Judger

# 依赖
1. glog https://github.com/google/glog
2. Mysql Connector/C https://dev.mysql.com/downloads/connector/c/6.1.html

# 编译
在`judger/`文件夹下执行`cmake . && make`命令

# 支持语言
status.language / problem.is_special_judge | language
---- | ---
1 | GNU C++
2 | GNU C++ 11
3 | Java
4 | Python2
5 | Python3

# 配置

## 配置文件
配置文件名为`config.ini`，其中的任何字段都不可删除。
## 用户
1. 新建低权限用户`judger`，`sudo useradd judger`
2. 给`judger`指定密码，`sudo passwd judger`

## 测试数据
测试数据文件夹由`config.ini`中的`test_files_path`字段指定，该文件夹与其中的内容的属主应为非`judger`用户。
测试输入数据文件为`{test_files_path}/{problem_id}/input`，
测试输出数据文件为`{test_files_path}/{problem_id}/output`，
即使输入数据或输出数据为空也应该创建该文件，此时该文件为空文件。
若无输入数据文件或输出数据文件会返回`Judger Error`。

## 临时文件夹
临时文件夹由`config.ini`中的`temp_path`字段制定，该文件夹创建后使用如下命令修改权限`sudo chown -R judger:{another_user} {temp_path}/`

## Special Judge
Special Judge所需的文件夹由`config.ini`中的`spj_files_path`字段指定，该文件夹与其中的内容的属主应为非`judger`用户。
若题目的`is_special_judge`字段的值为`0`则表明该题目非Special Judge，
否则Special Judge程序的语言由`is_special_judge`的值决定。
格式为`{spj_files_path}/{problem_id}/spj.{ext}`。
例：题目1000, Special Judge语言为C++，则格式为`{spj_files_path}/1000/spj.cpp`。
### 使用方法
SPJ程序的标准输入会有三行被写入，分别是测试数据输入文件名，测试数据输出文件名和用户输出数据文件名，你需要从这三个文件中来读取数据。
若SPJ程序的exit_code为0则表示AC，否则表示WA。

## Ballon
### 运行Redis
`redis.conf`
```
port 8080
bind 0.0.0.0
appendonly yes
maxmemory-policy noeviction
```
`$ redis-server ./redis.conf`

### 修改Judger
修改`src/Submit.cpp`，在
```cpp
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
```
中`else`的分支加上
```cpp
char cmd[256];
sprintf(cmd, "redis-cli -h 127.0.0.1 -p 8080 rpush ballon_%d %d_%d_%d", contest_id, uid, pid, runid);
system(cmd);
```

# 运行
把编译后生成的`Judger`和`config.ini`与各个所需文件夹放在同一个目录下，由非`judger`用户`sudo ./Judger`运行。
日志信息全部会输出到`stderr`。
