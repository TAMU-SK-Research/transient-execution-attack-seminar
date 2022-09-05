

CFLAGS=-std=c99 --static -I${PWD}/../include

.PHONY: all
all: l1d-latency spectre_v1 spectre_v2 spectre_v3 spectre_v4

l1d-latency: l1d-latency.c libm5eradicate.a
	gcc $^ ${CFLAGS} -o $@

spectre_v1: spectre_v1.c libm5eradicate.a
	gcc $^ ${CFLAGS} -o $@

spectre_v2: spectre_v2.c libm5eradicate.a indirect_call.o
	gcc $^ ${CFLAGS} -o $@

spectre_v3: spectre_v3.c libm5eradicate.a
	gcc $^ ${CFLAGS} -o $@

spectre_v4: spectre_v4.c libm5eradicate.a
	gcc $^ ${CFLAGS} -o $@

clean:
	rm l1d-latency spectre_v1 spectre_v2 spectre_v3 spectre_v4
