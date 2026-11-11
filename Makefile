CC ?= gcc
MPICC ?= mpicc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Wpedantic
OMPFLAGS ?= -fopenmp
CPPFLAGS ?= -Isrc
LDFLAGS ?=

BIN := build/bin

.PHONY: all clean test data

all: $(BIN)/ac_bench $(BIN)/ac_worker_diagnostic $(BIN)/system_probe

$(BIN):
	mkdir -p $@

$(BIN)/ac_bench: src/ac_bench.c src/native_platform.h | $(BIN)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(OMPFLAGS) $< $(LDFLAGS) -o $@

$(BIN)/ac_worker_diagnostic: src/ac_worker_diagnostic.c src/native_platform.h | $(BIN)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(OMPFLAGS) $< $(LDFLAGS) -o $@

ifeq ($(OS),Windows_NT)
$(BIN)/system_probe: src/system_probe.c | $(BIN)
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LDFLAGS) -o $@
else
$(BIN)/system_probe: src/system_probe_posix.c | $(BIN)
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LDFLAGS) -o $@
endif

mpi: $(BIN)/ac_bench_mpi $(BIN)/ac_mpi_layout

$(BIN)/ac_bench_mpi: src/ac_bench.c src/native_platform.h | $(BIN)
	$(MPICC) $(CFLAGS) $(CPPFLAGS) $(OMPFLAGS) -DWITH_MPI $< $(LDFLAGS) -o $@

$(BIN)/ac_mpi_layout: src/ac_mpi_layout.c src/native_platform.h | $(BIN)
	$(MPICC) $(CFLAGS) $(CPPFLAGS) $(OMPFLAGS) -DWITH_MPI $< $(LDFLAGS) -o $@

data:
	python3 scripts/generate.py --out data/demo --bytes 1048576 --patterns 1000 --seed 101

test: all
	python3 tests/test_smoke.py --bin-dir $(BIN)

clean:
	rm -rf build data/demo
