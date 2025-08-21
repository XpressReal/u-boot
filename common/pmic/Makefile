ccflags-y += -I$(srctree)/common/pmic/include -I$(srctree)/drivers/i2c/

obj-y += cmd_pmic.o
obj-y += pmic.o
obj-y += pmic-i2c.o

obj-$(CONFIG_RTK_PMIC_G2227)   += pmic-g2227.o
obj-$(CONFIG_RTK_PMIC_G2237)   += pmic-g2237.o
obj-$(CONFIG_RTK_PMIC_SY8827E) += pmic-sy8827e.o
obj-$(CONFIG_RTK_PMIC_SY8824C) += pmic-sy8824c.o
obj-$(CONFIG_RTK_PMIC_APW8889) += pmic-apw8889.o
obj-$(CONFIG_RTK_PMIC_APW8886) += pmic-apw8886.o
obj-$(CONFIG_RTK_PMIC_PWM_DT)  += pmic-pwm-dt.o
