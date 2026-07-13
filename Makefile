export PATH := /home/shay/a/ece270/bin:$(PATH)
export LD_LIBRARY_PATH := /home/shay/a/ece270/lib:$(LD_LIBRARY_PATH)

YOSYS=yosys
NEXTPNR=nextpnr-ice40
SHELL=bash

PROJ	= riscvmove
PINMAP 	= pinmap.pcf
TCLPREF = addwave.gtkw
PROGRAMS = programs
C_SRC   = $(PROGRAMS)/sample.c
BUILD   = ./build
HEX	    = $(BUILD)/test_program.hex
SRC	    = top.sv
ICE   	= ice40hx8k.sv
CHK 	= check.bin
DEM 	= demo.bin
JSON    = ll.json
SUP     = support/cells_*.v
UART	= uart/uart.v uart/uart_tx.v uart/uart_rx.v
FILES   = $(ICE) $(SRC) $(UART) src/riscvmove_defs.sv src/alu.sv src/regfile.sv src/mem.sv src/riscvmove.sv
TRACE	= $(PROJ).vcd
VOBJ    = $(BUILD)/obj_dir
MAP = mapped
TB	=  testbench
VERILOG := /usr/share/yosys/ice40/cells_sim.v /usr/share/yosys/ice40/cells_map.v
YOSYS_SIMLIB := /usr/share/yosys/simlib.v /usr/share/yosys/simcells.v

DEVICE  = 8k
TIMEDEV = hx8k
FOOTPRINT = ct256

RUNC_HEX          = $(BUILD)/run_c.hex
RUNC_DATA_HEX     = $(BUILD)/run_c_data.hex
EMBEDDED_HEX      = $(BUILD)/.embedded.hex
EMBEDDED_DATA_HEX = $(BUILD)/.embedded_data.hex
RUNC_HW_DEPS  = $(FILES) $(PINMAP) Makefile

# Without this, a recipe that fails partway (nextpnr crashing, icepack
# erroring out, a killed build) leaves whatever partial/empty file it had
# started writing sitting there with a fresh mtime -- Make then sees that
# file as an up-to-date target on the next run and happily hands the
# corrupt result downstream instead of rebuilding it. This is exactly how
# build/riscvmove.asc once ended up a 0-byte file that run_c's icebram path
# silently trusted. Standard GNU Make idiom: delete a rule's target if its
# recipe exits non-zero.
.DELETE_ON_ERROR:

all: cram

.PHONY: check_env
check_env:
	@echo -e "\nEnvironment setup correctly!\n"

#########################
# Default/canonical program hexes (intermediate build outputs -- never
# committed). cpu_test.hex is verify_riscvmove's own dedicated copy, derived
# solely from programs/cpu_test.s, so other flows writing to
# build/test_program.hex (compile_c, run_c, ...) can never make the
# regression test see the wrong program. test_program.hex is what actually
# gets embedded by synth (top.sv's INIT_FILE) and falls back to the same
# cpu_test.s program if nothing's been compiled into it yet.
#
# Every program produces a *pair* of hex files: <name>.hex for imem (.text
# only) and <name>_data.hex for dmem (.rodata + .data's initial values --
# there's no lw/lb/sw path from dmem back into imem's BRAM, so string
# literals/const arrays and initialized globals alike need their own
# preloaded copy in dmem (src/mem.sv); see link.ld).
$(BUILD)/cpu_test.hex: $(PROGRAMS)/cpu_test.s
	mkdir -p $(BUILD)
	riscv64-unknown-elf-gcc -Og -march=rv32i -mabi=ilp32 -nostdlib -T link.ld $(PROGRAMS)/cpu_test.s -o $(BUILD)/cpu_test.elf
	riscv64-unknown-elf-objcopy -O binary -j .text $(BUILD)/cpu_test.elf $(BUILD)/cpu_test.bin
	riscv64-unknown-elf-objcopy -O binary -j .rodata -j .data $(BUILD)/cpu_test.elf $(BUILD)/cpu_test_data.bin
	python3 bin2hex.py $(BUILD)/cpu_test.bin $(BUILD)/cpu_test.hex 512 unique
	python3 bin2hex.py $(BUILD)/cpu_test_data.bin $(BUILD)/cpu_test_data.hex 256

$(BUILD)/test_program.hex: $(BUILD)/cpu_test.hex
	cp $(BUILD)/cpu_test.hex $(BUILD)/test_program.hex
	cp $(BUILD)/cpu_test_data.hex $(BUILD)/test_program_data.hex

# test_program.hex's recipe above produces this as a side effect; this rule
# just lets other targets depend on it directly.
$(BUILD)/test_program_data.hex: $(BUILD)/test_program.hex

#########################
# Flash to FPGA
$(BUILD)/$(PROJ).json : $(ICE) $(SRC) $(PINMAP) Makefile $(BUILD)/test_program.hex $(BUILD)/test_program_data.hex
	# lint with Verilator
	verilator -Isrc --lint-only --top-module top $(SRC)
	# if build folder doesn't exist, create it
	mkdir -p $(BUILD)
	# synthesize using Yosys
	$(YOSYS) -p "read_verilog -Isrc -sv -noblackbox $(FILES); synth_ice40 -top ice40hx8k -json $(BUILD)/$(PROJ).json"

$(BUILD)/$(PROJ).asc : $(BUILD)/$(PROJ).json
	# Place and route using nextpnr
	$(NEXTPNR) --hx8k --package ct256 --pcf $(PINMAP) --asc $(BUILD)/$(PROJ).asc --json $(BUILD)/$(PROJ).json 2> >(sed -e 's/^.* 0 errors$$//' -e '/^Info:/d' -e '/^[ ]*$$/d' 1>&2)

$(BUILD)/$(PROJ).bin : $(BUILD)/$(PROJ).asc
	# Convert to bitstream using IcePack
	icepack $(BUILD)/$(PROJ).asc $(BUILD)/$(PROJ).bin
	
#########################
# ice40 Specific Targets
check: $(CHK)
	iceprog -S $(CHK)
	
demo:  $(DEM)
	iceprog -S $(DEM)

flash: $(BUILD)/$(PROJ).bin
	iceprog $(BUILD)/$(PROJ).bin

cram: $(BUILD)/$(PROJ).bin
	iceprog -S $(BUILD)/$(PROJ).bin

time: $(BUILD)/$(PROJ).asc
	icetime -p $(PINMAP) -P $(FOOTPRINT) -d $(TIMEDEV) $<

# *******************************************************************************
# COMPILATION & SIMULATION TARGETS
# *******************************************************************************

# Source Compilation and simulation of Design
.PHONY: sim_%_src
sim_%_src: 
	@echo -e "Creating executable for source simulation...\n"
	@mkdir -p $(BUILD) && rm -rf $(BUILD)/*
	@iverilog -g2012 -o $(BUILD)/$*_tb -Y .sv -y src/ $(TB)/$*_tb.sv
	@echo -e "\nSource Compilation complete!\n"
	@echo -e "Simulating source...\n"
	@vvp -l vvp_sim.log $(BUILD)/$*_tb
	@echo -e "\nSimulation complete!\n"
	@echo -e "\nOpening waveforms...\n"
	@if [ -f waves/$*.gtkw ]; then \
		gtkwave waves/$*.gtkw; \
	else \
		gtkwave waves/$*.vcd; \
	fi


# Run synthesis on Design
.PHONY: syn_%
syn_%: check_env
	@echo -e "Synthesizing design...\n"
	@mkdir -p $(MAP)
	@if [ "$*" = "imem" ]; then \
		$(YOSYS) -d -p "read_verilog -sv -noblackbox src/mem.sv; chparam -set DEPTH 4 -set INIT_FILE \"test/imem_fixture.hex\" imem; synth -top imem; write_verilog -noattr -noexpr -nohex -nodec -defparam $(MAP)/imem.v" > $*.log; \
	else \
		$(YOSYS) -d -p "read_verilog -sv -noblackbox src/*; synth_ice40 -top $*; write_verilog -noattr -noexpr -nohex -nodec -defparam $(MAP)/$*.v" > $*.log; \
	fi
	@echo -e "\nSynthesis complete!\n"


# Compile and simulate synthesized design
.PHONY: sim_%_syn
sim_%_syn: syn_%
	@echo -e "Compiling synthesized design...\n"
	@mkdir -p $(BUILD) && rm -rf $(BUILD)/*
	@if [ "$*" = "imem" ]; then \
		iverilog -g2012 -o $(BUILD)/$*_tb $(TB)/$*_tb.sv src/mem.sv; \
	else \
		iverilog -g2012 -o $(BUILD)/$*_tb -DFUNCTIONAL -DUNIT_DELAY=#1 $(TB)/$*_tb.sv $(MAP)/$*.v $(VERILOG); \
	fi
	@echo -e "\nCompilation complete!\n"
	@echo -e "Simulating synthesized design...\n\n"
	@vvp -l vvp_sim.log $(BUILD)/$*_tb
	@echo -e "\nSimulation complete!\n"
	@echo -e "\nOpening waveforms...\n"
	@if [ -f waves/$*.gtkw ]; then \
		gtkwave waves/$*.gtkw; \
	else \
		gtkwave waves/$*.vcd; \
	fi

#########################
# Verilator Tests (Quiet compilation unless it fails)
verify_alu:
	@mkdir -p $(BUILD)
	@echo Compiling alu...
	@if ! verilator -Isrc --cc src/alu.sv --exe $(CURDIR)/test/tb_alu.cpp --Mdir $(VOBJ) --build -j -o alu_test > $(BUILD)/verilator_compile.log 2>&1; then \
		cat $(BUILD)/verilator_compile.log; \
		rm -f $(BUILD)/verilator_compile.log; \
		exit 1; \
	fi
	@rm -f $(BUILD)/verilator_compile.log
	@echo Simulating alu...
	@$(VOBJ)/alu_test
	@echo

verify_regfile:
	@mkdir -p $(BUILD)
	@echo Compiling regfile...
	@if ! verilator -Isrc --cc src/regfile.sv --exe $(CURDIR)/test/tb_regfile.cpp --Mdir $(VOBJ) --build -j -o regfile_test > $(BUILD)/verilator_compile.log 2>&1; then \
		cat $(BUILD)/verilator_compile.log; \
		rm -f $(BUILD)/verilator_compile.log; \
		exit 1; \
	fi
	@rm -f $(BUILD)/verilator_compile.log
	@echo Simulating regfile...
	@$(VOBJ)/regfile_test
	@echo

verify_imem:
	@mkdir -p $(BUILD)
	@echo Compiling imem...
	@if ! verilator -Isrc --cc src/mem.sv --top-module imem -GDEPTH=4 -GINIT_FILE="\"$(CURDIR)/test/imem_fixture.hex\"" --exe $(CURDIR)/test/tb_imem.cpp --Mdir $(VOBJ) --build -j -o imem_test > $(BUILD)/verilator_compile.log 2>&1; then \
		cat $(BUILD)/verilator_compile.log; \
		rm -f $(BUILD)/verilator_compile.log; \
		exit 1; \
	fi
	@rm -f $(BUILD)/verilator_compile.log
	@echo Simulating imem...
	@$(VOBJ)/imem_test
	@echo

verify_dmem:
	@mkdir -p $(BUILD)
	@echo Compiling dmem...
	@if ! verilator -Isrc --cc src/mem.sv --top-module dmem --exe $(CURDIR)/test/tb_dmem.cpp --Mdir $(VOBJ) --build -j -o dmem_test > $(BUILD)/verilator_compile.log 2>&1; then \
		cat $(BUILD)/verilator_compile.log; \
		rm -f $(BUILD)/verilator_compile.log; \
		exit 1; \
	fi
	@rm -f $(BUILD)/verilator_compile.log
	@echo Simulating dmem...
	@$(VOBJ)/dmem_test
	@echo

verify_riscvmove: $(BUILD)/cpu_test.hex
	@echo Compiling riscvmove...
	@if ! verilator -Isrc --cc src/riscvmove.sv -GINIT_FILE="\"$(BUILD)/cpu_test.hex\"" -GDATA_INIT_FILE="\"$(BUILD)/cpu_test_data.hex\"" --exe $(CURDIR)/test/tb_riscvmove.cpp --Mdir $(VOBJ) --build -j -o riscvmove_test > $(BUILD)/verilator_compile.log 2>&1; then \
		cat $(BUILD)/verilator_compile.log; \
		rm -f $(BUILD)/verilator_compile.log; \
		exit 1; \
	fi
	@rm -f $(BUILD)/verilator_compile.log
	@echo Simulating riscvmove...
	@$(VOBJ)/riscvmove_test
	@echo

verify_led_test:
	@mkdir -p $(BUILD)
	@echo Compiling led_test program...
	@$(MAKE) compile_c C_SRC=$(PROGRAMS)/led_test.c HEX=$(BUILD)/led_test.hex > $(BUILD)/led_test_compile.log 2>&1 || (cat $(BUILD)/led_test_compile.log; rm -f $(BUILD)/led_test_compile.log; exit 1)
	@rm -f $(BUILD)/led_test_compile.log
	@echo Compiling led_test simulation...
	@if ! verilator -Isrc --cc src/riscvmove.sv -GINIT_FILE="\"$(BUILD)/led_test.hex\"" -GDATA_INIT_FILE="\"$(BUILD)/led_test_data.hex\"" --exe $(CURDIR)/test/tb_riscvmove_led.cpp --Mdir $(VOBJ) --build -j -o riscvmove_led_test > $(BUILD)/verilator_compile.log 2>&1; then \
		cat $(BUILD)/verilator_compile.log; \
		rm -f $(BUILD)/verilator_compile.log; \
		exit 1; \
	fi
	@rm -f $(BUILD)/verilator_compile.log
	@echo Simulating led_test...
	@$(VOBJ)/riscvmove_led_test
	@echo

verify_all: verify_alu verify_regfile verify_imem verify_dmem verify_riscvmove verify_led_test
	@echo "=================================================="
	@echo "ALL TESTS PASSED SUCCESSFULLY!"
	@echo "=================================================="

# Compilation Flow for C files to hex
# PAD_MODE=unique fills unused imem words with a non-repeating pattern
# (required by icebram -- see run_c below, which can't disambiguate two
# identical 256-word-aligned padding columns, a matching granularity tied to
# the physical SB_RAM40_4K tile size regardless of imem's logical DEPTH).
# This is also the default for a second, easy-to-miss reason: imem has no
# write port, so Yosys sees it as pure ROM and is free to prove any output
# bit that happens to be constant across the *entire* synthesized image --
# real instructions plus padding -- and hardwire it in the fabric instead of
# storing it in reconfigurable BRAM. A small reference program with all-zero
# NOP padding (bit 31 is 0 for every non-MMIO, non-negative-immediate
# instruction) can leave instr[31] provably constant across all 512 words,
# permanently dead in the fabric -- silently corrupting the *next* icebram
# swap the moment it embeds a program that needs bit 31 set (e.g. any `lui
# rd,0x80000` building an MMIO address). Unique's pseudorandom padding makes
# every bit toggle somewhere in the image, so Yosys can never prove one
# constant. PAD_MODE=nop remains available (pass it explicitly) if you need
# a stray PC to land on guaranteed-harmless NOPs instead, but only use it for
# a build that will never later be an icebram base for another program.
# dmem's hex always uses PAD_MODE=nop regardless -- it has a real write port
# (Yosys can't fold a bit "constant" when runtime stores can change it), and
# icebram never touches it anyway, since run_c only ever swaps a program's
# *code*; if a new program's .rodata/.data differ from what's currently in
# dmem, use run_from_scratch_c instead (see run_c's own comment below).
# Usage: make compile_c C_SRC=programs/sample.c HEX=build/test_program.hex
PAD_MODE = unique

compile_c:
	mkdir -p $(BUILD)
	riscv64-unknown-elf-gcc -Og -march=rv32i -mabi=ilp32 -nostdlib -T link.ld crt0.s $(C_SRC) -o $(BUILD)/program.elf
	riscv64-unknown-elf-objcopy -O binary -j .text $(BUILD)/program.elf $(BUILD)/program.bin
	riscv64-unknown-elf-objcopy -O binary -j .rodata -j .data $(BUILD)/program.elf $(BUILD)/program_data.bin
	python3 bin2hex.py $(BUILD)/program.bin $(HEX) 512 $(PAD_MODE)
	python3 bin2hex.py $(BUILD)/program_data.bin $(HEX:.hex=_data.hex) 256 nop
	@echo "=================================================="
	@echo "SUCCESSFULLY COMPILED $(C_SRC) TO $(HEX) (+ $(HEX:.hex=_data.hex))!"
	@echo "=================================================="

# Compile -> hex -> flash to FPGA
run_from_scratch_c:
	# Compile C code to hex (if C_SRC is set by user do not change it)
	make compile_c C_SRC=$(C_SRC) HEX=$(HEX)
	# Build and flash to FPGA
	make cram

# Swap a new C program into an already placed-and-routed bitstream via
# icebram, instead of re-running synthesis/place-and-route, then flash it.
# Usage: make run_c C_SRC=path/to/file.c
#
# icebram matches purely on hex-file content (see icebram.cc): it slices the
# "from" pattern into 256-word-aligned columns per bit position and errors
# out if any two columns collide. NOP padding makes every all-NOP 256-word
# chunk identical to every other one, which always collides -- so anything
# this target embeds must use PAD_MODE=unique instead.
#
# EMBEDDED_HEX/EMBEDDED_DATA_HEX track the exact (unique-padded) imem/dmem
# content currently believed to be in build/riscvmove.asc. If either is
# missing, or the .asc is newer than our imem record (built externally by
# `make cram`/`run_from_scratch_c`, or this is the very first run), we can't
# trust icebram to find anything reliably, so we force one fresh synth+P&R
# with C_SRC embedded directly instead. The same is true if any hardware
# source changed after the current .asc was built: icebram can only swap
# code bytes, not rebuild logic.
#
# icebram itself only ever gets handed imem's before/after hex pair
# (EMBEDDED_HEX/RUNC_HEX) -- it has no equivalent way to patch dmem's real
# BRAM content, so C_SRC is always compiled first (both code and data) and
# its *data* image is compared against EMBEDDED_DATA_HEX before deciding
# which path to take. A mismatch there used to be silently ignored -- the
# icebram path would swap in new code that assumes different .rodata/.data
# while dmem still physically held whatever the previous program's initial
# values were (e.g. a lookup table read would return an unrelated program's
# leftover string bytes instead of its own constants) -- so it now forces
# the same fresh synth+P&R path as an imem mismatch would.
run_c:
	@$(MAKE) compile_c C_SRC=$(C_SRC) HEX=$(RUNC_HEX) PAD_MODE=unique; \
	need_rebuild=0; \
	if [ ! -s $(BUILD)/$(PROJ).asc ] || [ ! -s $(EMBEDDED_HEX) ] || [ ! -s $(EMBEDDED_DATA_HEX) ] || [ $(BUILD)/$(PROJ).asc -nt $(EMBEDDED_HEX) ]; then \
		need_rebuild=1; \
	fi; \
	if ! cmp -s $(RUNC_DATA_HEX) $(EMBEDDED_DATA_HEX); then \
		need_rebuild=1; \
	fi; \
	for dep in $(RUNC_HW_DEPS); do \
		if [ "$$dep" -nt $(BUILD)/$(PROJ).asc ]; then \
			need_rebuild=1; \
			break; \
		fi; \
	done; \
	if [ $$need_rebuild -eq 1 ]; then \
		echo "No icebram-compatible bitstream on hand (or dmem/.data would differ) -- synthesizing fresh with $(C_SRC) embedded directly."; \
		cp $(RUNC_HEX) $(BUILD)/test_program.hex; \
		cp $(RUNC_DATA_HEX) $(BUILD)/test_program_data.hex; \
		rm -f $(BUILD)/$(PROJ).json $(BUILD)/$(PROJ).asc $(BUILD)/$(PROJ).bin; \
		$(MAKE) $(BUILD)/$(PROJ).asc; \
		cp $(BUILD)/test_program.hex $(EMBEDDED_HEX); \
		cp $(BUILD)/test_program_data.hex $(EMBEDDED_DATA_HEX); \
	else \
		echo "Swapping $(C_SRC)'s code into the existing bitstream via icebram (dmem/.data already matches)."; \
		icebram $(EMBEDDED_HEX) $(RUNC_HEX) < $(BUILD)/$(PROJ).asc > $(BUILD)/$(PROJ)_runc.asc; \
		mv $(BUILD)/$(PROJ)_runc.asc $(BUILD)/$(PROJ).asc; \
		cp $(RUNC_HEX) $(EMBEDDED_HEX); \
	fi
	$(MAKE) cram

run_uart:
	# Compile C code to a hex
	$(MAKE) compile_c C_SRC=$(PROGRAMS)/uart_sample.c HEX=$(BUILD)/uart_sample.hex
	# Build and run Verilator UART simulation
	verilator -Isrc --cc src/riscvmove.sv -GINIT_FILE="\"$(BUILD)/uart_sample.hex\"" -GDATA_INIT_FILE="\"$(BUILD)/uart_sample_data.hex\"" --exe $(CURDIR)/test/tb_riscvmove_uart.cpp --Mdir $(VOBJ) --build -j -o riscvmove_uart
	$(VOBJ)/riscvmove_uart

#########################
# Clean Up
clean:
	rm -rf build/ obj_dir/ *.fst *.vcd verilog.log abc.history mapped/
