FROM ubuntu:trusty

# setup source
RUN mkdir -p /home/sloopy/llvm
RUN apt-get update && apt-get install -y curl git
RUN curl http://releases.llvm.org/3.4.2/llvm-3.4.2.src.tar.gz | tar xz -C /home/sloopy/llvm --strip-components 1
RUN curl https://raw.githubusercontent.com/thpani/llvm/3097779e869ec81361413f04104b97c20d0d0182/include/llvm/Analysis/DominatorInternals.h >/home/sloopy/llvm/include/llvm/Analysis/DominatorInternals.h
RUN mkdir -p /home/sloopy/llvm/tools/clang
RUN curl http://releases.llvm.org/3.4.2/cfe-3.4.2.src.tar.gz | tar xz -C /home/sloopy/llvm/tools/clang/ --strip-components 1
RUN mkdir -p /home/sloopy/llvm/tools/clang/tools
WORKDIR /home/sloopy/llvm/tools/clang/tools
RUN git clone https://github.com/thpani/clang-tools-extra.git extra

# install z3
WORKDIR /home/sloopy
RUN apt-get install -y unzip
RUN curl -L -o z3.zip https://github.com/Z3Prover/z3/releases/download/z3-4.5.0/z3-4.5.0-x64-ubuntu-14.04.zip && unzip z3.zip
RUN ln -s /home/sloopy/z3-4.5.0-x64-ubuntu-14.04/bin/libz3.so /usr/lib/ && \
    ln -s /home/sloopy/z3-4.5.0-x64-ubuntu-14.04/bin/libz3.a /usr/lib/ && \
    ldconfig -n -v /usr/lib

# setup build env
RUN apt-get install -y clang ninja-build cmake libc++-dev libboost-all-dev
RUN mkdir /home/sloopy/llvm-build

# build sloopy
WORKDIR /home/sloopy/llvm-build
RUN CC=clang CXX=clang++ cmake -GNinja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-std=c++11 -stdlib=libc++ -Wno-c++11-extensions -I/home/sloopy/z3-4.5.0-x64-ubuntu-14.04/include/" -DLLVM_TARGETS_TO_BUILD=CppBackend\;X86 ../llvm/
RUN ninja sloopy
