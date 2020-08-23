ifeq (,$(platform))
    ARCH = $(shell uname -m)
    ifeq ($(ARCH), $(filter $(ARCH), i386 i686))
        platform = x86
    else ifeq ($(ARCH), $(filter $(ARCH), x86_64 AMD64 amd64))
        platform = x64
    else ifneq (,$(findstring aarch64,$(ARCH)))
        platform = arm64
        HARDWARE = $(shell grep Hardware /proc/cpuinfo)
        ifneq (,$(findstring Vero4K,$(HARDWARE)))
            platform = vero4k
            FEATURES = $(shell grep Features /proc/cpuinfo)
            ifneq (,$(findstring neon,$(FEATURES)))
                platform += neon
            endif
        endif
    else ifneq (,$(findstring arm,$(ARCH)))
        HARDWARE = $(shell grep Hardware /proc/cpuinfo)
        ifneq (,$(findstring BCM2709,$(HARDWARE)))
            platform = rpi2
        else ifneq (,$(findstring BCM2835,$(HARDWARE)))
            platform = rpi4
        else ifneq (,$(findstring BCM2711,$(HARDWARE)))
            platform = rpi4
        else ifneq (,$(findstring AM33XX,$(HARDWARE)))
            platform = beagle
        else ifneq (,$(findstring Pandora,$(HARDWARE)))
            platform = pandora
        else ifneq (,$(findstring ODROIDC,$(HARDWARE)))
            platform = odroidc1
        else ifneq (,$(findstring ODROID-XU3,$(HARDWARE)))
            platform = odroidxu3
	else ifneq (,$(findstring ODROID-XU4,$(HARDWARE)))
            platform = odroidxu3
        else ifneq (,$(findstring ODROIDXU,$(HARDWARE)))
            platform = odroidxu
        else ifneq (,$(findstring ODROIDX2,$(HARDWARE)))
            platform = odroidx2
        else ifneq (,$(findstring ODROIDX,$(HARDWARE)))
            platform = odroidx
        else ifneq (,$(findstring ODROID-U2/U3,$(HARDWARE)))
            platform = odroidu2
        else ifneq (,$(findstring ODROIDU2,$(HARDWARE)))
            platform = odroidu2
        else
            platform = armv7h
        endif
    else ifneq (,$(findstring mips,$(ARCH)))
        platform = gcwz
    else
        $(warning Unsupported CPU architecture, using lincpp)
        platform = lincpp
    endif

    FLAGS = $(shell grep flags /proc/cpuinfo)
    ifneq (,$(findstring sse4_1,$(FLAGS)))
        platform += sse4_1
    endif
endif
