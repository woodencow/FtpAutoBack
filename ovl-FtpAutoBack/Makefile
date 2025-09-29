#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/libnx/switch_rules

#---------------------------------------------------------------------------------
# TARGET 是输出文件的名称
# BUILD 是存放目标文件和中间文件的目录
# SOURCES 是包含源代码的目录列表
# DATA 是包含数据文件的目录列表
# INCLUDES 是包含头文件的目录列表
# ROMFS 是包含要添加到 RomFS 的数据的目录，相对于 Makefile（可选）
#
# NO_ICON: 如果设置为任何值，则不使用图标。
# NO_NACP: 如果设置为任何值，则不生成 .nacp 文件。
# APP_TITLE 是存储在 .nacp 文件中的应用程序名称（可选）
# APP_AUTHOR 是存储在 .nacp 文件中的应用程序作者（可选）
# APP_VERSION 是存储在 .nacp 文件中的应用程序版本（可选）
# APP_TITLEID 是存储在 .nacp 文件中的应用程序标题ID（可选）
# ICON 是图标文件名（.jpg），相对于项目文件夹。
#   如果未设置，它会尝试按以下顺序使用其中一个：
#     - <项目名称>.jpg
#     - icon.jpg
#     - <libnx 文件夹>/default_icon.jpg
#
# CONFIG_JSON 是 NPDM 配置文件（.json）的文件名，相对于项目文件夹。
#   如果未设置，它会尝试按以下顺序使用其中一个：
#     - <项目名称>.json
#     - config.json
#   如果提供或自动检测到 JSON 文件，则构建 ExeFS PFS0（.nsp）而不是
#   自制可执行文件（.nro）。这用于系统模块。
#   NACP 构建也会被跳过。
#---------------------------------------------------------------------------------
# FtpAutoBack      4674704175746F4261636B
# 即传即备          E58DB3E4BCA0E58DB3E5A487
# BBBBBBBBBBBB     424242424242424242424242


APP_TITLE	:=	FtpAutoBack
APP_VERSION :=	0.1.0

TARGET		:=	FtpAutoBack
BUILD		:=	build
SOURCES		:=	source libs/minIni-nx/source
DATA		:=	data
INCLUDES	:=	include libs/libtesla/include libs/minIni-nx/include

NO_ICON		:=  1

#---------------------------------------------------------------------------------
# 代码生成选项
#---------------------------------------------------------------------------------
ARCH	:=	-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE

CFLAGS	:=	-g -Wall -O2 -ffunction-sections \
			$(ARCH) $(DEFINES)

CFLAGS	+=	$(INCLUDE) -D__SWITCH__ -DAPP_VERSION=$(APP_VERSION) \
			-DMININI_USE_NX=1 -DMININI_USE_STDIO=1 -DMININI_USE_FLOAT=1

CXXFLAGS	:= $(CFLAGS) -fno-exceptions -std=c++20

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS	:= -lnx

#---------------------------------------------------------------------------------
# 包含库的目录列表，这必须是包含 include 和 lib 的顶级目录
#---------------------------------------------------------------------------------
LIBDIRS	:= $(PORTLIBS) $(LIBNX)


#---------------------------------------------------------------------------------
# 除非需要为不同的文件扩展名添加额外规则，否则无需编辑此处以下的内容
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#---------------------------------------------------------------------------------
# 对于 C++ 项目使用 CXX 链接，对于标准 C 使用 CC
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#---------------------------------------------------------------------------------
	export LD	:=	$(CC)
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
	export LD	:=	$(CXX)
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES))
export OFILES_SRC	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES 	:=	$(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN	:=	$(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

ifeq ($(strip $(CONFIG_JSON)),)
	jsons := $(wildcard *.json)
	ifneq (,$(findstring $(TARGET).json,$(jsons)))
		export APP_JSON := $(TOPDIR)/$(TARGET).json
	else
		ifneq (,$(findstring config.json,$(jsons)))
			export APP_JSON := $(TOPDIR)/config.json
		endif
	endif
else
	export APP_JSON := $(TOPDIR)/$(CONFIG_JSON)
endif

ifeq ($(strip $(ICON)),)
	icons := $(wildcard *.jpg)
	ifneq (,$(findstring $(TARGET).jpg,$(icons)))
		export APP_ICON := $(TOPDIR)/$(TARGET).jpg
	else
		ifneq (,$(findstring icon.jpg,$(icons)))
			export APP_ICON := $(TOPDIR)/icon.jpg
		endif
	endif
else
	export APP_ICON := $(TOPDIR)/$(ICON)
endif

ifeq ($(strip $(NO_ICON)),)
	export NROFLAGS += --icon=$(APP_ICON)
endif

ifeq ($(strip $(NO_NACP)),)
	export NROFLAGS += --nacp=$(CURDIR)/$(TARGET).nacp
endif

ifneq ($(APP_TITLEID),)
	export NACPFLAGS += --titleid=$(APP_TITLEID)
endif

ifneq ($(ROMFS),)
	export NROFLAGS += --romfsdir=$(CURDIR)/$(ROMFS)
endif

.PHONY: $(BUILD) clean all

#---------------------------------------------------------------------------------
all: $(BUILD)


$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
clean:
	@rm -fr $(BUILD) $(TARGET).ovl $(TARGET).nro $(TARGET).nacp $(TARGET).elf


#---------------------------------------------------------------------------------
else
.PHONY:	all

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# 主要目标
#---------------------------------------------------------------------------------
.PHONY: force all clean
all	:	 $(OUTPUT).ovl

$(OUTPUT).ovl		:	$(OUTPUT).elf $(OUTPUT).nacp force
	@elf2nro $< $@ $(NROFLAGS)
	@echo "built ... $(notdir $(OUTPUT).ovl)"

$(OUTPUT).elf	:	$(OFILES) force

force:

$(OFILES_SRC)	: $(HFILES_BIN) force

#---------------------------------------------------------------------------------
# 对于用作二进制数据的每个扩展名，都需要这样的规则
#---------------------------------------------------------------------------------
%.bin.o	%_bin.h :	%.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
