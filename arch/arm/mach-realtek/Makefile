# SPDX-License-Identifier:	GPL-2.0

obj-$(CONFIG_SPL_BUILD)	+= spl.o

obj-y	+= clock.o
#obj-	y+= timer.o
obj-y	+= dram.o
obj-y	+= hwinit.o
obj-y	+= mem.o
obj-y	+= ddr.o
obj-$(CONFIG_$(SPL_)USB_STORAGE) += usb.o
obj-y	+= mcp.o

ifndef CONFIG_SPL_BUILD
obj-y	+= board.o
obj-y	+= md.o
endif

obj-$(CONFIG_TARGET_RTD1319) += rtd1319/
obj-$(CONFIG_TARGET_RTD1619B) += rtd1619b/
