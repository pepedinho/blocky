build_dir := "build"
target := build_dir + "/kv_server"

default: run

build:
    @make -j$(nproc)

debug:
    @make debug

run: build
    @./{{ target }}

client:
    python3 ./tools/client.py 8080

nc-client:
    nc 127.0.0.1 8080

clean:
    @make clean
