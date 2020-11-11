TARGET = pa_graph

all:	pa_graph.c clean
	gcc -Wall pa_graph.c \
	-lpthread \
	`pkg-config --cflags --libs libpulse` \
	`pkg-config --cflags --libs glib-2.0` \
	`pkg-config --cflags --libs libgvc` \
	-ggdb \
	-o ${TARGET}

clean:
	rm -f *.o ${TARGET}

run:	all
	./${TARGET}
