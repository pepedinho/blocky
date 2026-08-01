compiler := "g++"
flags := "-std=c++20 -Wall -Wextra -Werror -O2 -g -Isrc"
sanitizers := "-fsanitize=address,undefined"
build_dir := "build"
target := build_dir + "/kv_server"

sources := "src/main.cpp src/io_uring.cpp src/socket.cpp"

GREEN := '\033[1;32m'
CYAN := '\033[1;36m'
RED := '\033[1;31m'
RESET := '\033[0m'

default: run

prepare:
    @mkdir -p {{ build_dir }}

build: prepare
    @printf "{{ CYAN }}Building project...{{ RESET }}\n"
    {{ compiler }} {{ flags }} {{ sources }} -o {{ target }}
    @printf "{{ GREEN }}Build completed successfully! -> {{ target }}{{ RESET }}\n"

debug: prepare
    @printf "{{ CYAN }}Building debug binary with sanitizers...{{ RESET }}\n"
    {{ compiler }} {{ flags }} {{ sanitizers }} {{ sources }} -o {{ target }}_debug
    @printf "{{ GREEN }}Debug build ready! -> {{ target }}_debug{{ RESET }}\n"

run: build
    @printf "{{ CYAN }}Starting KV Server...{{ RESET }}\n"
    ./{{ target }}

clean:
    @printf "{{ RED }}Cleaning build directory...{{ RESET }}\n"
    rm -rf {{ build_dir }}
