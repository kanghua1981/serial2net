

CFLAGS += -O2 -DLINUX -D_GNU_SOURCE -Wall -DDEBUG -lpthread

APP_BINARY =  sertcpproxy

all: $(APP_BINARY)

$(APP_BINARY): sertcpproxy.o dev-serial.o
	$(CC) $(CFLAGS) -lpthread $^ $(CLDFLAGS) -o $@
clean:
	rm -rf *.o
	rm -rf $(APP_BINARY)
