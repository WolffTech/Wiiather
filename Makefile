#---------------------------------------------------------------------------------
# Wiiather: based on the devkitPro Wii application template.
#---------------------------------------------------------------------------------
.SUFFIXES:

export DEVKITPRO ?= /opt/devkitpro
export DEVKITPPC ?= $(DEVKITPRO)/devkitPPC

include $(DEVKITPPC)/wii_rules

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# DATA is a list of directories containing binary files embedded with bin2o
# INCLUDES is a list of directories containing extra header files
#---------------------------------------------------------------------------------
TARGET		:=	wiiather
BUILD		:=	build
SOURCES		:=	source source/audio source/gen source/util source/net source/weather source/render source/screens
DATA		:=	data
INCLUDES	:=	source source/gen
DIST		:=	dist/apps/wiiather

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
CFLAGS		=	-g -O2 -std=gnu11 -Wall -Wextra -Wno-unused-parameter $(MACHDEP) $(INCLUDE)
CXXFLAGS	=	$(CFLAGS)
LDFLAGS		=	-g $(MACHDEP) -Wl,-Map,$(notdir $@).map

#---------------------------------------------------------------------------------
# any extra libraries we wish to link with the project
#---------------------------------------------------------------------------------
LIBS	:=	-lgrrlib -lpngu -lfreetype -lbz2 -lbrotlidec -lbrotlicommon -lpng16 -ljpeg -lz \
			-lmbedtls -lmbedx509 -lmbedcrypto -ljansson \
			-lfat -lwiiuse -lbte -lasnd -lmad -logc -lm

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:=	$(PORTLIBS)

#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
					$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

export LD	:=	$(CC)

export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES))
export OFILES_SOURCES := $(CFILES:.c=.o)
export OFILES := $(OFILES_BIN) $(OFILES_SOURCES)

export HFILES := $(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE	:=	$(foreach dir,$(INCLUDES), -iquote $(CURDIR)/$(dir)) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					-I$(CURDIR)/$(BUILD) \
					-I$(LIBOGC_INC)

export LIBPATHS	:= -L$(LIBOGC_LIB) $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean run dist test

#---------------------------------------------------------------------------------
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).dol dist

#---------------------------------------------------------------------------------
# Host-side tests for the platform-independent logic.
HOST_CC		?=	cc
TEST_LIB	:=	source/weather/obs.c source/weather/forecast.c source/util/utc.c
TEST_LIB	+=	source/weather/latest_obs.c source/weather/sun_moon.c source/weather/alerts.c
TESTS		:=	$(patsubst tests/%.c,$(BUILD)/host/%,$(wildcard tests/test_*.c))

test: $(TESTS)
	@for t in $(TESTS); do echo $$t; $$t || exit 1; done

$(BUILD)/host/%: tests/%.c $(TEST_LIB) $(wildcard source/weather/*.h source/util/*.h)
	@mkdir -p $(dir $@)
	$(HOST_CC) -std=gnu11 -Wall -Wextra -Werror -iquote source -o $@ $< $(TEST_LIB) -lm

#---------------------------------------------------------------------------------
run: $(BUILD)
	wiiload $(TARGET).dol

#---------------------------------------------------------------------------------
# Homebrew Channel layout: copy dist/apps to the root of the SD card.
dist: $(BUILD)
	@mkdir -p $(DIST)
	@cp $(OUTPUT).dol $(DIST)/boot.dol
	@cp meta/meta.xml meta/icon.png $(DIST)/
	@echo dist ... $(DIST)

#---------------------------------------------------------------------------------
else

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

$(OFILES_SOURCES) : $(HFILES)

#---------------------------------------------------------------------------------
%.png.o	%_png.h :	%.png
	@echo $(notdir $<)
	$(bin2o)

%.pem.o	%_pem.h :	%.pem
	@echo $(notdir $<)
	$(bin2o)

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
