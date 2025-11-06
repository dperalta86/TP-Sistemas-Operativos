SUBDIRS := aging deadlock

all:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir || exit 1; \
	done

clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean || exit 1; \
	done

test: all
	@for dir in $(SUBDIRS); do \
		echo "\n== Ejecutando tests en $$dir =="; \
		(cd $${dir} && ./test_$${dir}); \
	done

