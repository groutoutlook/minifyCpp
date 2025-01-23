FROM ubuntu:24.04 AS build

WORKDIR /work
RUN apt-get update && \
    apt-get install build-essential libclang-17-dev clang-17 llvm-17-dev cmake -y

COPY src /work/src/
COPY include /work/include/
COPY CMakeLists.txt /work/

# build
RUN mkdir build && \
    cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=MinSizeRel -DLLVMVersion=17 && \
    cmake --build .

# output stage
FROM scratch
COPY --from=build /work/build/minifier /
ENTRYPOINT [ "minifier" ]