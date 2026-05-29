KER_DIR ?= /lib/modules/$(shell uname -r)/build
DRV_DIR := $(shell pwd)

obj-m += edge.o
edge10-objs := edge.o

INC := -I $(DRV_DIR)
CFLAGS_edge.o := -Wall $(INC)

all:
	@git_repo_name=$$(git rev-parse --show-toplevel 2>/dev/null | xargs basename 2>/dev/null); \
	if [ "$$git_repo_name" = "edge-driver" ]; then \
		echo "#define EDGE_VERSION  \"`git describe --tags --always --dirty 2>/dev/null` (`date "+%Y-%m-%d %H:%M:%S"`)\"" > version.h; \
	else \
	    if [ ! -e version.h ]; then \
		    echo "#define EDGE_VERSION \" \"" > version.h; \
	    fi \
	fi
	$(MAKE) -C $(KER_DIR) M=$(DRV_DIR) modules

clean:
	rm -rf *.o .*.o.d *~ core .depend .*.cmd *.ko *.ko.unsigned *.mod.c .tmp_versions *.symvers .#* *.save *.bak Modules.* modules.order Module.markers *.bin *.ur-safe .cache.mk *.mod
