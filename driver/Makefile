PWD = $(shell pwd)  
KERNEL_SRC = ../../../../T56/android/kernel/

obj-m := char_dev.o
cmbs_audio-objs := char_dev.o
   
default:  
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) modules  
  
         
clean:  
	-rm *.ko  
	-rm *.o
	-rm .*.cmd
	-rm *.mod.c
	-rm modules.order
	-rm Module.symvers
	-rm -r .tmp_versions
