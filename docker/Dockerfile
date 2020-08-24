FROM ubuntu

# edit sources.list according to https://www.alibabacloud.com/help/zh/doc-detail/120851.htm
RUN echo 'deb http://mirrors.aliyun.com/ubuntu/ focal main restricted universe multiverse\n\
deb-src http://mirrors.aliyun.com/ubuntu/ focal main restricted universe multiverse\n\
\n\
deb http://mirrors.aliyun.com/ubuntu/ focal-security main restricted universe multiverse\n\
deb-src http://mirrors.aliyun.com/ubuntu/ focal-security main restricted universe multiverse\n\
\n\
deb http://mirrors.aliyun.com/ubuntu/ focal-updates main restricted universe multiverse\n\
deb-src http://mirrors.aliyun.com/ubuntu/ focal-updates main restricted universe multiverse\n\
\n\
deb http://mirrors.aliyun.com/ubuntu/ focal-proposed main restricted universe multiverse\n\
deb-src http://mirrors.aliyun.com/ubuntu/ focal-proposed main restricted universe multiverse\n\
\n\
deb http://mirrors.aliyun.com/ubuntu/ focal-backports main restricted universe multiverse\n\
deb-src http://mirrors.aliyun.com/ubuntu/ focal-backports main restricted universe multiverse\n' > /etc/apt/sources.list

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y \
        vim \
        less \
        build-essential \
        git \
        cmake \
        autoconf \
        python2 \
        python3 \
        default-jdk \
        libmysqlclient-dev

RUN useradd judger --uid 1000

COPY / /judger-build
WORKDIR /judger-build
RUN cmake . && make

WORKDIR /
RUN mkdir judger
RUN cp judger-build/judger judger/
RUN rm -rf judger-build

WORKDIR judger
RUN mkdir test_files spj_files temp_path && chown judger:judger temp_path

CMD ["./judger"]
