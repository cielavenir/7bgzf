BUILD		:= build

ifneq ($(BUILD),$(notdir $(CURDIR)))

TARGET ?= cielbox

SOURCES		+= applet lib lib/zlib lib/zopfli lib/libdeflate lib/popt .
INCLUDES	+= 
DATA		+= data 

#--- set toolchain name
ifneq ($(CLANG),)
export CC	:=	$(PREFIX)clang
#export CXX	:=	$(PREFIX)clang++ -stdlib=libc++
export CXX	:=	$(PREFIX)clang++
else ifneq ($(OPEN64),)
export CC       :=      $(PREFIX)opencc
export CXX      :=      $(PREFIX)openCC
else
export CC	:=	$(PREFIX)gcc
export CXX	:=	$(PREFIX)g++
endif

#export AS	:=	$(PREFIX)as
#export AR	:=	$(PREFIX)ar
#export OBJCOPY	:=	$(PREFIX)objcopy
export STRIP	:=	$(PREFIX)strip

#--- set path
### relative path from build
#LIBS	+=
export LIBS

LIBDIRS	:=	
export LIBDIRS

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

#--- flags
#ARCH	:=	
CFLAGS	:=	-Wall -Wno-parentheses \
			-Wno-sign-compare -Wno-unused-parameter -Wno-unused-value \
			-O2 -fomit-frame-pointer -ffast-math \
			-DNO_VIZ -DHAVE_INTERNAL -D_LIBCPP_NO_EXCEPTIONS \
			$(ARCH)

ifeq ($(WIN32),)
	#enable 7ZIP_ST in non-Win32
	CFLAGS += -D_7ZIP_ST
endif
ifneq ($(CRYPTOPP),)
        SOURCES += lib/cryptopp
        CFLAGS += -DCRYPTOPP=1
	LIBS += -lcryptopp
endif


#-DNO_VIZ for zlib.

CFLAGS	+=	$(INCLUDE)
CXXFLAGS	:=	$(CFLAGS) -fno-exceptions
#lovely hack...
#bah -Wno-pointer-sign must be stripped for iPodLinux
#-std=c99 breaks fileno() on Linux...
CFLAGS	+= -std=gnu99
CFLAGS	+= -Wno-pointer-sign -Wno-unused-result

ASFLAGS	:=	$(ARCH)
LDFLAGS	=	$(ARCH) $(LDF) -O2
#-Wl,-Map,$(notdir $*.map)

ifneq ($(USTL),)
#you need to give libustl path. It must be linked statically.
	CXXFLAGS += -DUSTL
	LIBS += -lsupc++
	ifneq ($(WIN32),)
		LDFLAGS += -static-libgcc
	endif
else
	CXXFLAGS += -fno-rtti
	ifneq ($(WIN32),)
		LDFLAGS += -static-libgcc -static-libstdc++
	endif
endif

ifneq ($(NOZLIBNG),)
	CFLAGS += -DNOZLIBNG
else
	SOURCES += lib/zlib-ng lib/zlib-ng/arch/arm
endif

ifneq ($(ZLIBNG_X86),)
	### some very old gcc does not have immintrin.h; need to add flag...
	SOURCES += lib/zlib-ng/arch/x86
	CFLAGS += -DX86_CPUID -DX86_QUICK_STRATEGY
endif

ifneq ($(NOIGZIP),)
	CFLAGS += -DNOIGZIP
else
	LIBS += ../lib/isa-l/bin/isa-l.a
endif

export CFLAGS
export CXXFLAGS
export LDFLAGS
 
export BIN	:=	$(CURDIR)/$(TARGET)$(SUFF)
export DEPSDIR := $(CURDIR)/$(BUILD)

#export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
#					$(foreach dir,$(DATA),$(CURDIR)/$(dir))
export VPATH	:=	$(CURDIR)

CFILES		:=	$(foreach dir,$(SOURCES),$(wildcard $(dir)/*.c))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(wildcard $(dir)/*.cpp))
SFILES		:=	$(foreach dir,$(SOURCES),$(wildcard $(dir)/*.s))
#BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))
 
ifeq ($(strip $(CPPFILES)),)
	export LD	:=	$(CC)
else
	ifneq ($(USTL),)
		#Oh dear: undefined reference to std::__throw_out_of_range_fmt
		#No proper fix yet...
		#export LD	:=	$(CXX)
		export LD	:=	$(CC)
	else
		#gcc doesn't understand -static-libstdc++...
		export LD	:=	$(CXX)
	endif
endif

export OFILES	:=	$(addsuffix .o,$(BINFILES)) \
					$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

.PHONY: $(BUILD) clean
 
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@[ "$(NOIGZIP)" != "" ] || CC=$(PREFIX)gcc CFLAGS="$(subst -flto,,$(CFLAGS))" make -C lib/isa-l -f Makefile.unx lib programs/igzip
	@make --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile -j4

clean:
	@#echo clean ...
	@make -C lib/isa-l -f Makefile.unx clean
	@rm -fr $(BUILD) 

#---------------------------------------------------------------------------------
else

#%.a:
#	@#echo $(notdir $@)
#	@rm -f $@
#	@$(AR) -rc $@ $^

lib/cryptopp/%.o: RTTI:=-frtti -fexceptions

%.o: %.cpp
	@#echo $(notdir $<)
	@[ -d $@ ] || mkdir -p $(dir $@)
	@$(CXX) -MMD -MP -MF $(DEPSDIR)/$*.d $(CXXFLAGS) $(RTTI) -c $< -o $@ $(ERROR_FILTER)

%.o: %.c
	@#echo $(notdir $<)
	@[ -d $@ ] || mkdir -p $(dir $@)
	@$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(CFLAGS) -c $< -o $@ $(ERROR_FILTER)

%.o: %.s
	@#echo $(notdir $<)
	@[ -d $@ ] || mkdir -p $(dir $@)
	@$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d -x assembler-with-cpp $(ASFLAGS) -c $< -o $@ $(ERROR_FILTER)

DEPENDS	:=	$(OFILES:.o=.d)

$(BIN): $(OFILES)
	@#echo linking $(notdir $@)
	@$(LD)  $(LDFLAGS) $(OFILES) $(LIBPATHS) $(LIBS) -o $@
	@if ! echo "$(LDFLAGS)" | grep -- -shared; then $(STRIP) $@; fi

-include $(DEPENDS)

endif
