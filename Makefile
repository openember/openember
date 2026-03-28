SHELL := /usr/bin/env bash

BUILD_DIR ?= build
JOBS ?= $(shell nproc)

.PHONY: help menuconfig genconfig update configure build clean all

help:
	@echo "OpenEmber helper targets"
	@echo "  make menuconfig [BUILD_DIR=build]  (also runs genconfig)"
	@echo "  make genconfig  [BUILD_DIR=build]"
	@echo "  make update     [BUILD_DIR=build]  (cmake -S . -B; was: configure)"
	@echo "  make build      [BUILD_DIR=build] [JOBS=nproc]"
	@echo "  make all        [BUILD_DIR=build] [JOBS=nproc]"
	@echo "  make clean      [BUILD_DIR=build]"

menuconfig:
	bash scripts/kconfig/menuconfig.sh "$(BUILD_DIR)"
	bash scripts/kconfig/genconfig.sh "$(BUILD_DIR)"

genconfig:
	bash scripts/kconfig/genconfig.sh "$(BUILD_DIR)"

update configure:
	cmake -S . -B "$(BUILD_DIR)"

build:
	cmake --build "$(BUILD_DIR)" -j"$(JOBS)"

all: menuconfig update build

clean:
	rm -rf "$(BUILD_DIR)"

