FROM alpine:latest

# 安装必要的工具
RUN apk add --no-cache \
    gcc \
    musl-dev \
    make \
    linux-headers \
    perf \
    strace \
    bash \
    util-linux

WORKDIR /workspace

# 复制源码和脚本
COPY . /workspace/

# 编译程序
RUN make clean && make all

CMD ["/bin/bash"]
