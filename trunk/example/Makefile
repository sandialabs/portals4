CFLAGS := -O3
LDFLAGS := -lportals
LDPATHS := -L/usr/local/lib
CPPFLAGS := -I/usr/local/include

hello: hello.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LDPATHS) $(LDFLAGS) -o $@

clean:
	$(RM) hello
