# SPDX-License-Identifier: CC0-1.0
#
# SPDX-FileContributor: Antonio Niño Díaz, 2025

BLOCKSDS	?= /opt/blocksds/core

# User config

NAME		:= datelTool
GAME_TITLE	:= Datel Flash Tool
GAME_SUBTITLE	:= Flash Dumper & Writer
GAME_AUTHOR := by Apache Thunder & edo9300
GAME_ICON := icon.png
GFXDIRS := graphics
CXXFLAGS = -std=c++26

# Libraries

include $(BLOCKSDS)/sys/default_makefiles/rom_arm9/Makefile


$(ROM): $(ELF)
	@echo "  NDSTOOL $@"
	$(V)$(BLOCKSDS)/tools/ndstool/ndstool -c $@ \
		-7 $(ARM7ELF) -9 $(ELF) \
		-b $(GAME_ICON) "$(GAME_FULL_TITLE)" \
		-g DATT 01 "DATEL TOOL" -z 80040000 -a 00000138 -p 0001 $(NDSTOOL_ARGS)
