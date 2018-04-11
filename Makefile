
all : fbwhiptail gtkwhiptail

test-menu-gtk: test-menu-gtk.c cairo_menu.c cairo_utils.c fbwhiptail_menu.c
	$(CC) -g -O0 -o $@ $^ \
		`pkg-config --cflags --libs cairo`                \
		`pkg-config --cflags --libs gtk+-2.0` -lm

test-menu-fb: test-menu-fb.c cairo_menu.c cairo_utils.c cairo_linuxfb.c \
		libcairo.a libpixman-1.a libpng16.a libz.a
	$(CC) -g -O0 -o $@ $^ -lm


fbwhiptail: fbwhiptail.c fbwhiptail_menu.c cairo_menu.c cairo_utils.c cairo_dri.c cairo_linuxfb.c
	$(CC) -g -O0 -o $@ $^ -lm -lcairo -lpixman-1 -lpng16 -lz

gtkwhiptail: fbwhiptail.c fbwhiptail_menu.c fbwhiptail_menu.h cairo_menu.c cairo_utils.c
	$(CC) -g -O0 -DGTKWHIPTAIL -o $@ $^ -lm 		\
		`pkg-config --cflags --libs cairo`              \
		`pkg-config --cflags --libs gtk+-2.0`

test-dri: test-dri.c
	$(CC) -g -O0 -o $@ $^
