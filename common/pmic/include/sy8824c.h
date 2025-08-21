#ifndef __LINUX_REGULATOR_SY8824C_H
#define __LINUX_REGULATOR_SY8824C_H

#define SY8824C_REG_VSEL0               (0x00)

/* register definition */
#define SY8824C_BUCK_EN0_MASK           (0x80)
#define SY8824C_BUCK_EN0_SHIFT          (7)
#define SY8824C_BUCK_EN0_WIDTH          (1)
#define SY8824C_MODE0_MASK              (0x40)
#define SY8824C_MODE0_SHIFT             (6)
#define SY8824C_MODE0_WIDTH             (1)
#define SY8824C_NSEL0_MASK              (0x3F)
#define SY8824C_NSEL0_SHIFT             (0)
#define SY8824C_NSEL0_WIDTH             (6)

#endif
