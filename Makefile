.ONESHELL:
SHELL := /bin/bash
.DEFAULT_GOAL := example-all
.PHONY: build-lib fmt lint lint-verible lint-slang example-core example-uvm example-all clean

ROOT := $(abspath .)
BUILD_DIR := $(ROOT)/build
DSIM_HOME ?= $(HOME)/.local/lib/altairdsim/current
DSIM_LICENSE ?= $(HOME)/.config/altairdsim/dsim-license.json
DSIM_LIB := $(BUILD_DIR)/libhdl_probe_dpi.so
DSIM_UVM_OPTS := -uvm 2020.3.1 -sv_lib $(DSIM_LIB) +acc+rwcbf +UVM_NO_RELNOTES
CC := gcc
CFLAGS := -DDSIM -fPIC -g -I$(DSIM_HOME)/include

CORE_SV := $(ROOT)/sv/hdl_probe_pkg.sv
SV_FILES := $(sort $(wildcard $(ROOT)/sv/*.sv) $(wildcard $(ROOT)/sv/*.svh) $(wildcard $(ROOT)/tb/*.sv))
SLANG_FILES := $(CORE_SV) $(ROOT)/tb/example_core.sv

build-lib: $(DSIM_LIB)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(DSIM_LIB): $(ROOT)/c/hdl_probe.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -shared -o $(DSIM_LIB) $(ROOT)/c/hdl_probe.c

lint:
	$(MAKE) lint-verible
	$(MAKE) lint-slang

lint-verible:
	verible-verilog-lint --rules_config=$(ROOT)/.rules.verible_lint $(SV_FILES)

lint-slang:
	slang -Weverything -Werror -Wno-unused-but-set-variable $(SLANG_FILES)

fmt:
	verible-verilog-format --inplace $(SV_FILES)

example-core: build-lib
	source $(DSIM_HOME)/shell_activate.bash
	export DSIM_LICENSE=$(DSIM_LICENSE)
	dsim -sv_lib $(DSIM_LIB) +acc+rwcbf -top example_core $(CORE_SV) $(ROOT)/tb/example_core.sv

example-uvm: build-lib
	source $(DSIM_HOME)/shell_activate.bash
	export DSIM_LICENSE=$(DSIM_LICENSE)
	dsim $(DSIM_UVM_OPTS) -top example_uvm $(CORE_SV) $(ROOT)/tb/example_uvm.sv

example-all:
	$(MAKE) example-core
	$(MAKE) example-uvm

clean:
	rm -rf $(BUILD_DIR) dsim.log dsim.env dsim_work metrics.db tr_db.log
