ICARUS_INC_DIR ?= /usr/local/include/iverilog

SIM ?= icarus
TOPLEVEL_LANG ?= verilog
CWD=.
export COCOTB_REDUCED_LOG_FMT=1
export COCOTB_RESOLVE_X=RANDOM

DUT=rpu
MODULE=test_$(DUT)
TOPLEVEL=$(MODULE)

RTL_DIR = $(CWD)/../../rtl
ACC_DIR = $(CWD)/../../accel/pigasus_sme/rtl
ROSEBUD = $(CWD)/../../lib/Rosebud/rtl
AXIS_LIB = $(CWD)/../../lib/axis/rtl

COMPILE_ARGS += -P $(TOPLEVEL).SLOT_COUNT=32
COMPILE_ARGS += -P $(TOPLEVEL).INIT_ROMS=1
COMPILE_ARGS += -I $(ROSEBUD)
COMPILE_ARGS += -I $(ACC_DIR)

VERILOG_SOURCES  = $(ROSEBUD)/../tb/common/test_rpu.v
VERILOG_SOURCES += $(RTL_DIR)/rpu_PR_pig.v

VERILOG_SOURCES += $(ROSEBUD)/basic_fifo.v
VERILOG_SOURCES += $(ROSEBUD)/axis_register.v
VERILOG_SOURCES += $(ROSEBUD)/mem_modules.v
VERILOG_SOURCES += $(ROSEBUD)/axis_dma.v
VERILOG_SOURCES += $(ROSEBUD)/VexRiscv.v
VERILOG_SOURCES += $(ROSEBUD)/riscvcore.v
VERILOG_SOURCES += $(ROSEBUD)/rpu.v
VERILOG_SOURCES += $(ROSEBUD)/rpu_controller.v
VERILOG_SOURCES += $(ROSEBUD)/rpu_intercon.v
VERILOG_SOURCES += $(ROSEBUD)/mem_sys.v
VERILOG_SOURCES += $(ROSEBUD)/simple_arbiter.v
VERILOG_SOURCES += $(ROSEBUD)/simple_sync_sig.v
VERILOG_SOURCES += $(ROSEBUD)/axis_stat.v
VERILOG_SOURCES += $(ROSEBUD)/header.v
VERILOG_SOURCES += $(ROSEBUD)/single_accel_rd_dma.v
VERILOG_SOURCES += $(ROSEBUD)/spaced_2lvl_penc.v

VERILOG_SOURCES += $(ACC_DIR)/backend.sv
VERILOG_SOURCES += $(ACC_DIR)/first_filter.sv
VERILOG_SOURCES += $(ACC_DIR)/frontend.sv
VERILOG_SOURCES += $(ACC_DIR)/hashtable.sv
VERILOG_SOURCES += $(ACC_DIR)/hashtable_top.sv
VERILOG_SOURCES += $(ACC_DIR)/ips.sv
VERILOG_SOURCES += $(ACC_DIR)/acc_hash.sv
VERILOG_SOURCES += $(ACC_DIR)/mul_hash.sv
VERILOG_SOURCES += $(ACC_DIR)/rr_arbiter.sv
VERILOG_SOURCES += $(ACC_DIR)/rr_arbiter_4.sv
VERILOG_SOURCES += $(ACC_DIR)/string_matcher.sv
VERILOG_SOURCES += $(ACC_DIR)/hyper_pipe.sv
VERILOG_SOURCES += $(ACC_DIR)/hyper_pipe_rst.sv
VERILOG_SOURCES += $(ACC_DIR)/port_unit.sv
VERILOG_SOURCES += $(ACC_DIR)/port_group.sv
VERILOG_SOURCES += $(ACC_DIR)/rule_unit.sv
VERILOG_SOURCES += $(ACC_DIR)/pkt_almost_full.sv
VERILOG_SOURCES += $(ACC_DIR)/reduction_2t1_2.sv
VERILOG_SOURCES += $(ACC_DIR)/unified_fifo.sv
VERILOG_SOURCES += $(ACC_DIR)/unified_pkt_fifo.sv
VERILOG_SOURCES += $(ACC_DIR)/rule_depacker_128_64.sv
VERILOG_SOURCES += $(ACC_DIR)/rule_depacker_64_32.sv
VERILOG_SOURCES += $(ACC_DIR)/SME_wrapper.sv
VERILOG_SOURCES += $(ACC_DIR)/accel_wrap_pigasus.v
VERILOG_SOURCES += $(ACC_DIR)/ip_match.v

VERILOG_SOURCES += $(AXIS_LIB)/axis_fifo.v
VERILOG_SOURCES += $(AXIS_LIB)/arbiter.v
VERILOG_SOURCES += $(AXIS_LIB)/priority_encoder.v
VERILOG_SOURCES += $(AXIS_LIB)/sync_reset.v
VERILOG_SOURCES += $(AXIS_LIB)/axis_pipeline_register.v

PLUSARGS+=-fst
INC_DIR=$(ICARUS_INC_DIR)

include $(shell cocotb-config --makefiles)/Makefile.sim

compile:
	make -C ../../accel/pigasus/c/ NAME=pigasus
	make -C ../../accel/pigasus/c/ NAME=pigasus2

clean::
	@rm -rf *.xml __pycache__
