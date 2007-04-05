// config.c
// Scott Bronson
// 10 Mar 2007
//
// Handles configuration listeners.


#include "env.h"
#include "io/socket.h"
#include "config.h"


#define DEFAULT_CONFIG_PORT 6667


struct {
	io_atom io;
	char c;
	int chars_processed;
} config;


static void read_proc(io_poller *poller, io_atom *ioa, int flags)
{
	char readbuf[1024];
	connection *conn = (connection*)ioa;
    int fd = conn->io.fd;
    int len;
        
    if(flags & IO_READ) { 
        do {
            len = read(fd, readbuf, sizeof(readbuf));
        } while (errno == EINTR);   // stupid posix
    
        if(len > 0) {
			write(fd, readbuf, len);
            conn->chars_processed += len;
        } else if(len == 0) {
            // A 0-length read means remote has closed normally
			printf("connection closed by remote on fd %d\n", conn->io.fd);
			connection_close(conn);
			return;
        } else {
            // handle an error on the socket
            if(errno == EAGAIN) {
                // nothing to read?  weird.
            } else if(errno == EWOULDBLOCK) {
                // with glibc EAGAIN==EWOULDBLOCK so this is probably dead code
            } else {
				printf("read error %s on fd %d\n", strerror(errno), conn->io.fd);
				connection_close(conn);
                return;
            }
        }
    }
    
    if(flags & IO_WRITE) {
		// there's more space in the write buffer
		// so continue writing.
    }   
            
    if(flags & IO_EXCEPT) {
        // I think this is also used for OOB.
        // recv (fd1, &c, 1, MSG_OOB);
		printf("exception on fd %d\n", conn->io.fd);
		connection_close(conn);
		return;
    }
}


static void accept_proc(io_poller *poller, io_atom *ioa, int flags)
{
	connection *conn;
	socket_addr remote;

	// since the accepter only has IO_READ anyway, there's no need to
	// check the flags param.

    conn = malloc(sizeof(connection));
	if(!conn) {
		perror("allocating connection");
		return;
	}

	if(io_accept(&poller, &conn->io, connection_proc, IO_READ, &accepter, &remote) < 0) {
		perror("connecting to remote");
		return;
	}

	printf("connection opened from %s port %d given fd %d\n",
		inet_ntoa(remote.addr), remote.port, conn->io.fd);
}


struct config* config_socket_add(io_poller *poller, const char *confstr)
{
	const socket_addr addr = { { htonl(INADDR_ANY) }, DEFAULT_CONFIG_PORT };
	const char errstr;
	
	errstr = io_parse_address(confstr, &addr);
	if(errstr) {
		return NULL;
	}
	
	if(io_listen(poller, &conf->io, config_proc, addr) < 0) {
		perror("listen");
		exit(1);
	}

	printf("Listening for config on port %d, fd %d.\n", addr.port, accepter.fd);
}


struct config* config_add(io_poller *poller, const char *confstr)
{
	// if confstr is a dash then attach to stdin
	if(confstr[0] == '-' && confstr[1] == '\0') {
		io_init(&config->io, STDIN_FILENO, config_proc);
		return config;
	}
	
	// if the confstr contains a colon, it must be an addr:port spec
	if(strchr(confstr, ':')) {
		return config_socket_add(poller, config, confstr);
	}
	
	// TODO: add support for bidirectional communication over unix domain sockets.
	config->io.fd = open(confstr,O_RDONLY);
	if(fd < 0) {
		log_error(env, )
	}
}


int config_dispose(struct config *conf)
{
	io_remove(&poller, &accepter);

	if(conf->io.fd >= 0) {
		close(conf->io.fd);
		conf->io.fd = -1;
	}

	free(conf);
}


