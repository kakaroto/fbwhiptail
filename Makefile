
all : test-menu-gtk test-menu-fb fbwhiptail test-dri

test-menu-gtk: test-menu-gtk.c cairo_menu.c cairo_utils.c
	gcc -g -O0 -o $@ $^ \
		`pkg-config --cflags --libs cairo`                \
		`pkg-config --cflags --libs gtk+-2.0` -lm

test-menu-fb: test-menu-fb.c cairo_menu.c cairo_utils.c cairo_linuxfb.c \
		libcairo.a libpixman-1.a libpng16.a libz.a
	gcc -g -O0 -o $@ $^ -lm
	strip $@


fbwhiptail: fbwhiptail.c fbwhiptail_menu.c cairo_menu.c cairo_utils.c cairo_dri.c cairo_linuxfb.c \
		libcairo.a libpixman-1.a libpng16.a libz.a
	gcc -g -O0 -o $@ $^ -lm

test-dri: test-dri.c
	gcc -g -O0 -o $@ $^ 
