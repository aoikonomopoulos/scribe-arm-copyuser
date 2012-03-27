Throwaway repo to easily test and verify the scribe callbacks from arm asm

Gotchas:

* Need to set KERNEL_OBJDIR appropriately in the Makefile
* Need to create two symlinks to within the kernel source tree
    * asm -> $(KERNEL_SRCDIR)/arch/arm/include/asm
    * linux -> $(KERNEL_SRCDIR)/include/linux
* also needs a modification in linux/scribe_uaccess.h to make it includable
  from asm code
