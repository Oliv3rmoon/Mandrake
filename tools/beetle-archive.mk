# Mandrake - included after Beetle PSX's Makefile by tools/build-beetle.sh: packs the objects of the build into an
# archive instead of linking a shared library. The shared-library source list is the one used: it carries Beetle's
# own libretro-common (file streams, threads, strings, hashes), which the archive keeps to itself.
# SPDX-License-Identifier: GPL-3.0-or-later

Mandrake-archive: $(OBJECTS)
	$(AR) rcs $(MANDRAKE_ARCHIVE) $(OBJECTS)

# The PS5: rthreads.c and retro_eventcount.c ask for _POSIX_C_SOURCE 199309 when it is not set, and the SDK's
# headers then hide CLOCK_REALTIME (it needs 200112). Setting it first makes their own #ifndef skip theirs.
ifeq ($(MANDRAKE_PS5),1)
$(CORE_DIR)/libretro-common/rthreads/rthreads.o $(CORE_DIR)/libretro-common/rthreads/retro_eventcount.o: CFLAGS += -D_POSIX_C_SOURCE=200112L
endif
