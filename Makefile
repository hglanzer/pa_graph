OUT = pa_graph

all:	pa_graph.c clean
	gcc -Wall pa_graph.c -D_REENTRANT -I/usr/include/glib-2.0 \
	-I/usr/lib/x86_64-linux-gnu/glib-2.0/include -lpulse-mainloop-glib -lpulse -lglib-2.0 `pkg-config --cflags --libs glib-2.0` \
	-lpthread \
	`pkg-config --cflags --libs  libgvc` \
	-ggdb \
	-o ${OUT}

clean:
	rm -f *.o ${OUT}

run:	all
	./${OUT}
