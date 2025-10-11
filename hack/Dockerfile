FROM nvidia/cuda:12.6.3-cudnn-devel-ubuntu20.04



ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    git \
    wget \
    curl \
    ca-certificates \
    build-essential \
    libssl-dev \
    rsync && \
    rm -rf /var/lib/apt/lists/* && \
    wget https://cmake.org/files/v3.22/cmake-3.22.1.tar.gz && \
    tar -xzf cmake-3.22.1.tar.gz && \
    cd cmake-3.22.1 && \
    ./bootstrap && \
    make -j$(nproc) && \
    make install -j $(nproc) && \
    cd .. && \
    rm -rf cmake-3.22.1 cmake-3.22.1.tar.gz && \
    ln -sf /usr/local/bin/cmake/bin/* /usr/bin/