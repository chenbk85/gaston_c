#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "./atomic.h"

#include <event2/event.h>
#include <event2/bufferevent.h>

#define LISTEN_PORT 9995
#define LISTEN_BACKLOG 32

void do_accept(evutil_socket_t listener, short event, void *arg);
void read_cb(struct bufferevent *bev, void *arg);
void error_cb(struct bufferevent *bev, short event, void *arg);
void write_cb(struct bufferevent *bev, void *arg);

void server_writefifo(int fd);

int net_process() {
    int ret;
    evutil_socket_t listener;
    listener = socket(AF_INET, SOCK_STREAM, 0);
    assert(listener > 0);
    evutil_make_listen_socket_reuseable(listener);

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = 0;
    sin.sin_port = htons(LISTEN_PORT);

    if (bind(listener, (struct sockaddr *) &sin, sizeof (sin)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(listener, LISTEN_BACKLOG) < 0) {
        perror("listen");
        return 1;
    }

    printf("Listening...\n");

    evutil_make_socket_nonblocking(listener);

    struct event_base *base = event_base_new();
    assert(base != NULL);
    struct event *listen_event;
    listen_event = event_new(base, listener, EV_READ | EV_PERSIST, do_accept, (void*) base);
    event_add(listen_event, NULL);
    event_base_dispatch(base);

    printf("The End.");
    return 0;
}

void do_accept(evutil_socket_t listener, short event, void *arg) {
    struct event_base *base = (struct event_base *) arg;
    evutil_socket_t fd;
    struct sockaddr_in sin;
    socklen_t slen;
    fd = accept(listener, (struct sockaddr *) &sin, &slen);
    if (fd < 0) {
        perror("accept");
        return;
    }
    if (fd > FD_SETSIZE) {
        perror("fd > FD_SETSIZE\n");
        return;
    }

    printf("ACCEPT: fd = %u\n", fd);

    struct bufferevent *bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(bev, read_cb, NULL, error_cb, arg);
    bufferevent_enable(bev, EV_READ | EV_WRITE | EV_PERSIST);
}

void worker(int fd, char *input, char *output) {
    server_writefifo(fd); //逻辑处理
    sprintf(output, "%s%d\0", input, fd);
}

void read_cb(struct bufferevent *bev, void *arg) {
#define MAX_LINE    2560
    char line[MAX_LINE + 1];
    char output[1000];
    int n;
    evutil_socket_t fd = bufferevent_getfd(bev);

    while (n = bufferevent_read(bev, line, MAX_LINE), n > 0) {
        line[n] = '\0';
        printf("fd=%u, read line: %s\n", fd, line);
        worker((int) fd, line, output);

        bufferevent_write(bev, output, strlen(output));
    }
    //close(fd);
}

void write_cb(struct bufferevent *bev, void *arg) {
}

void error_cb(struct bufferevent *bev, short event, void *arg) {
    evutil_socket_t fd = bufferevent_getfd(bev);
    printf("fd = %u, ", fd);
    if (event & BEV_EVENT_TIMEOUT) {
        printf("Timed out\n"); //if bufferevent_set_timeouts() called
    } else if (event & BEV_EVENT_EOF) {
        printf("connection closed\n");
    } else if (event & BEV_EVENT_ERROR) {
        printf("some other error\n");
    }
    bufferevent_free(bev);
}




#define ForkNum 1
#define SERVER_PORT 7000
#define FifoServer "/tmp/fifo.server."
#define FifoClient "/tmp/fifo.client."
//#define FifoNum 2000

//FIFO FD
int FIFO_CLIENT_R = -1;
int FIFO_CLIENT_W = -1;
int FIFO_SERVER_R = -1;
int FIFO_SERVER_W = -1;
//FIFO FD
int ForkPos = 0;
atomic_t ActiveConnectionAtomic;
static int ActiveConnection = 0;

void fd2client_fifopath(int fd, char *path) {
    int pos = fd % ForkNum;
    sprintf(path, "%s%d", FifoClient, pos);
}

void fd2server_fifopath(int fd, char *path) {
    int pos = fd % ForkNum;
    sprintf(path, "%s%d", FifoServer, pos);
}

void init_fifo() {
    char ClientFifoPath[100];
    char ServerFifoPath[100];

    for (int i = 0; i < ForkNum; i++) {
        fd2client_fifopath(i, ClientFifoPath);
        fd2server_fifopath(i, ServerFifoPath);
        //write 2 
        if (mkfifo(ClientFifoPath, O_CREAT | O_EXCL)) {
            //printf("cannot create fifoserver\n");
        }
        if (mkfifo(ServerFifoPath, O_CREAT | O_EXCL)) {
            //printf("cannot create fifoserver\n");
        }
    }
}

void client_writefifo(int fd) {
    int len = 100;
    int nwrite = 0;
    char sfd[len];
    char ClientFifoPath[100];
    sprintf(sfd, "%d", fd);
    fd2client_fifopath(ForkPos, ClientFifoPath);
    printf("%s\n", ClientFifoPath);

    if (FIFO_CLIENT_W == -1) {
        FIFO_CLIENT_W = open(ClientFifoPath, O_WRONLY, 0);
    }

    //int fifo_fd=open(ClientFifoPath,O_WRONLY,0);
    if ((nwrite = write(FIFO_CLIENT_W, sfd, len)) == -1) {
        if (errno == EAGAIN) {
            printf("The FIFO has not been read yet.Please try later\n");
        } else {
            printf("fifo.error:%d\n", errno);
        }
    } else {
        printf("client.fifo.write:%s\n", sfd);
    }

}

void client_readfifo() {
    int len = 100;
    int nread = 0;
    char sfd[len];
    char ClientFifoPath[100];
    char ServerFifoPath[100];
    char buf_r[len];
    //sprintf( sfd, "%d", fd);
    //sprintf( ClientFifoPath, "%s%d", FifoClient,fd);
    fd2server_fifopath(ForkPos, ServerFifoPath);

    if (FIFO_SERVER_R == -1) {
        FIFO_SERVER_R = open(ServerFifoPath, O_RDONLY, 0);
        //printf("FIFO_SERVER_R::%d - %s\n", FIFO_SERVER_R, ServerFifoPath);
    }
    if ((nread = read(FIFO_SERVER_R, buf_r, len)) == -1) {
        if (errno == EAGAIN) {
            printf("no data yet\n");
        }
    } else {
        //printf("server.fifo.read:%s\n", buf_r);
        int client_fd = atoi(buf_r);
        //client_fd = client_fd * 10;
        client_writefifo(client_fd);
    }
}

void server_readfifo(int fd) {
    int len = 100;
    int nread = 0;
    char sfd[len];
    char ClientFifoPath[100];
    char buf_r[len];
    //sprintf( sfd, "%d", fd);
    fd2client_fifopath(fd, ClientFifoPath);
    if (FIFO_CLIENT_R == -1) {
        FIFO_CLIENT_R = open(ClientFifoPath, O_RDONLY, 0);
    }

    if ((nread = read(FIFO_CLIENT_R, buf_r, len)) == -1) {
        if (errno == EAGAIN) {
            //printf("no data yet\n");
        }
    } else {
        printf("client.fifo.read:%s\n", buf_r);
    }
    //close(fifo_fd);

}

void server_writefifo(int fd) {
    int len = 100;
    int nwrite = 0;
    char sfd[len];
    char ClientFifoPath[100];
    char ServerFifoPath[100];
    sprintf(sfd, "%d", fd);
    fd2server_fifopath(fd, ServerFifoPath);

    if (FIFO_SERVER_W == -1) {
        FIFO_SERVER_W = open(ServerFifoPath, O_WRONLY, 0);
    }

    if ((nwrite = write(FIFO_SERVER_W, sfd, len)) == -1) {
        if (errno == EAGAIN) {
            printf("The FIFO has not been read yet.Please try later\n");
        }
    } else {
        //printf("server.fifo.write:%s\n", sfd);
    }
    server_readfifo(fd);
}

int child_process() {
    int pid = getpid();
    int filePos = pid % ForkNum;

    while (1) {
        client_readfifo();
        //usleep(1000000);
        usleep(10);
        //printf("%s-%d-pos:%d\n", "child", pid, ForkPos);
    }
    printf("%s-%d\n", "child_process", pid);
}

int fork_process(int num) {
    int i, k;
    int pid = -1;
    for (i = 0; i < num; i++) {
        if (pid != 0) {
            ForkPos++;
            pid = fork();
            printf(":::%d\n", pid);
            if (pid == 0) {
                child_process();
                break;
            } else {
                //waitpid(pid,NULL,0);
            }
        } else {

            //printf("This is parent process[%i]\n",pid);
        }
    }
    return pid;
}

int main() {
    //signal(SIGCLD, SIG_IGN);

    atomic_set(&ActiveConnectionAtomic, 0);
    init_fifo();
    fork_process(ForkNum);
    net_process();

    return 1;
}