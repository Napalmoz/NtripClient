#include <QThread>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <cstdio>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <netdb.h>
#include <ctype.h>
#include <ntripclient.h>

#define BUFFERSIZE 80100
#define DEBUG 1

typedef int sockettype;
#define MAXDATASIZE 1000 /* max number of bytes we can get at once */
#define myperror perror
#define AGENTSTRING "NTRIP NtripClientPOSIX"
static char revisionstr[] = "$Revision: 1.50 $";

char buf[MAXDATASIZE];

NtCl::NtCl(char *s, char *pr, char *u, char *p, char *m, int md, int recon_time)
{
    SetParam(s, pr, u, p, m, md, recon_time);
    //инициализация атрибутов создания нового потока, новый поток должен быть "detached"
    if (pthread_attr_init(&attr)) {
        exit(EXIT_FAILURE);
    }
    if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)){
        exit(EXIT_FAILURE);
    }
}

//Установка параметров подключения
void NtCl::SetParam(char *s, char *pr, char *u, char *p, char *m, int md, int recon_time)
{
    server = s;
    port = pr;
    user = u;
    password = p;
    mnt = m;
    mode = md;

    totalbytes = 0;
    start_last_msg = 0;
    stop = 0;
    sleeptime = 0;
    error = 0;
    proxyserver = 0;
    reconect_time = recon_time;
}


void *NtCl::StartRunTh(void*ptr) {
    ((NtCl*) ptr)->RunTh();
    return 0;
}

void NtCl::Stop()
{
    stop = 1;
}

void NtCl::Run()
{
    stop = 0;
    pthread_t p1;
    int stat = pthread_create(&p1, &attr, NtCl::StartRunTh, this);
    if (stat) {
                pthread_exit(NULL);
               }
    pthread_join(p1, NULL);
}


void NtCl::RunTh1(void)
{
    while(true)
    {
        sleep(1);
    }
}

//Проверка значений и подключения
void NtCl::RunTh(void)
{
    pthread_mutex_init(&mutex, NULL);
    setbuf(stdout, 0);
    setbuf(stdin, 0);
    setbuf(stderr, 0);
    do
    {
        error = 0;
        sockettype sockfd = 0;
        struct sockaddr_in their_addr;
        struct hostent *he;
        struct servent *se;
        if(sleeptime)
        {
          sleeptime += 2;
        }
        else
        {
          sleeptime = 1;
        }

        if(!stop && !error)
        {
          memset(&their_addr, 0, sizeof(struct sockaddr_in));
          if((i = strtol(port, &b, 10)) && (!b || !*b))
            their_addr.sin_port = htons(i);
          else if(!(se = getservbyname(port, 0)))
          {
            fprintf(stderr, "Can't resolve port %s.", port);
            stop = 1;
          }
          else
          {
            their_addr.sin_port = se->s_port;
          }
          if(!stop && !error)
          {
            if(!(he=gethostbyname(server)))
            {
              fprintf(stderr, "Server name lookup failed for '%s'.\n", server);
              error = 1;
            }
            else if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
            {
              myperror("socket");
              error = 1;
            }
            else
            {
              their_addr.sin_family = AF_INET;
              their_addr.sin_addr = *((struct in_addr *)he->h_addr);
            }
          }
        }
        if(!stop && !error)
        {
            if(connect(sockfd, (struct sockaddr *)&their_addr,
            sizeof(struct sockaddr)) == -1)
            {
              myperror("connect");
              error = 1;
            }
            if(!stop && !error)
            {
              if(!mnt)
              {
                i = snprintf(buf, MAXDATASIZE,
                "GET %s%s%s%s/ HTTP/1.1\r\n"
                "Host: %s\r\n%s"
                "User-Agent: %s/%s\r\n"
                "Connection: close\r\n"
                "\r\n"
                , proxyserver ? "http://" : "", proxyserver ? proxyserver : "",
                proxyserver ? ":" : "", proxyserver ? proxyport : "",
                server, mode == NTRIP1 ? "" : "Ntrip-Version: Ntrip/2.0\r\n",
                AGENTSTRING, revisionstr);
              }
              else
              {
                i=snprintf(buf, MAXDATASIZE-40, /* leave some space for login */
                "GET %s%s%s%s/%s HTTP/1.1\r\n"
                "Host: %s\r\n%s"
                "User-Agent: %s/%s\r\n"
                "Connection: close%s"
                , proxyserver ? "http://" : "", proxyserver ? proxyserver : "",
                proxyserver ? ":" : "", proxyserver ? proxyport : "",
                mnt,server,
                mode == NTRIP1 ? "" : "Ntrip-Version: Ntrip/2.0\r\n",
                AGENTSTRING, revisionstr,
                (*user || *password) ? "\r\nAuthorization: Basic " : "");

                if(i > MAXDATASIZE-40 || i < 0) /* second check for old glibc */
                {
                  fprintf(stderr, "Requested data too long\n");
                  stop = 1;
                }
                else
                {
                  i += NtCl::encode(buf+i, MAXDATASIZE-i-4, user, password);
                  if(i > MAXDATASIZE-4)
                  {
                    fprintf(stderr, "Username and/or password too long\n");
                    stop = 1;
                  }
                  else
                  {
                    buf[i++] = '\r';
                    buf[i++] = '\n';
                    buf[i++] = '\r';
                    buf[i++] = '\n';
                  }
                }
              }
            }
        }
        if(!stop && !error)
        {
          if(send(sockfd, buf, (size_t)i, 0) != i)
          {
            myperror("send");
            error = 1;
          }
          else if(mnt && *mnt != '%')
          {
            int k = 0;
            int chunkymode = 0;
            int starttime = time(0);
            int lastout = starttime;

            while(!stop && !error &&
            (numbytes=RECV(sockfd, buf, MAXDATASIZE-1, 0)) > 0)
            {

              if(!k)
              {
                if( numbytes > 17 &&
                  !strstr(buf, "ICY 200 OK")  &&  /* case 'proxy & ntrip 1.0 caster' */
                  (!strncmp(buf, "HTTP/1.1 200 OK\r\n", 17) ||
                  !strncmp(buf, "HTTP/1.0 200 OK\r\n", 17)) )
                {
                  char *datacheck = "Content-Type: gnss/data\r\n";
                  char *chunkycheck = "Transfer-Encoding: chunked\r\n";
                  int l = strlen(datacheck)-1;
                  int j=0;
                  for(i = 0; j != l && i < numbytes-l; ++i)
                  {
                    for(j = 0; j < l && buf[i+j] == datacheck[j]; ++j)
                      ;
                  }
                  if(i == numbytes-l)
                  {
                    fprintf(stderr, "No 'Content-Type: gnss/data' found\n");
                    error = 1;
                  }
                  l = strlen(chunkycheck)-1;
                  j=0;
                  for(i = 0; j != l && i < numbytes-l; ++i)
                  {
                    for(j = 0; j < l && buf[i+j] == chunkycheck[j]; ++j)
                      ;
                  }
                  if(i < numbytes-l)
                    chunkymode = 1;
                }
                else if(mode != NTRIP1)
                {
                  fprintf(stderr, "NTRIP version 2 HTTP connection failed%s.\n",
                  mode == AUTO ? ", falling back to NTRIP1" : "");
                  if(mode == HTTP)
                    stop = 1;
                }
                ++k;
              }
              else
              {
                sleeptime = 0;
                totalbytes += numbytes;

                if(totalbytes < 0) /* overflow */
                {
                  totalbytes = 0;
                  starttime = time(0);
                  lastout = starttime;
                }
              }
            }
          }
        }
        if(sockfd)
            close(sockfd);
//ожидание переподключения
        sleep(reconect_time);
      }
    while(mnt && *mnt != '%' && !stop);

    pthread_exit(NULL);
}

//Получаемые данные, блокировка и разблокировка потока
    int NtCl::RECV(int __fd, void *__buf, size_t __n, int __flags)
{
    NtCl::lock();
    int numbytes=recv(__fd, __buf, __n, __flags);
    NtCl::unlock();
    usleep(50000);
    return numbytes;
}

//Блокирует остальные потоки
void NtCl::lock() {
    pthread_mutex_lock(&mutex);
}
//Разблокирует остальные потоки
void NtCl::unlock() {
    pthread_mutex_unlock(&mutex);
}

//GetBuffer
Buffer NtCl::GetBuf(){
    Buffer buf_struct;

    buf_struct.buf = buf;
    buf_struct.size_msg = numbytes;
    buf_struct.buyte_count = totalbytes;
    return buf_struct;

}

static char encodingTable [64] = {
      'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
      'Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f',
      'g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v',
      'w','x','y','z','0','1','2','3','4','5','6','7','8','9','+','/'
};

int NtCl::encode(char *buf, int size, char *user, char *pwd)
{
  unsigned char inbuf[3];
  char *out = buf;
  int i, sep = 0, fill = 0, bytes = 0;

  while(*user || *pwd)
  {
    i = 0;
    while(i < 3 && *user) inbuf[i++] = *(user++);
    if(i < 3 && !sep)    {inbuf[i++] = ':'; ++sep; }
    while(i < 3 && *pwd)  inbuf[i++] = *(pwd++);
    while(i < 3)         {inbuf[i++] = 0; ++fill; }
    if(out-buf < size-1)
      *(out++) = encodingTable[(inbuf [0] & 0xFC) >> 2];
    if(out-buf < size-1)
      *(out++) = encodingTable[((inbuf [0] & 0x03) << 4)
               | ((inbuf [1] & 0xF0) >> 4)];
    if(out-buf < size-1)
    {
      if(fill == 2)
        *(out++) = '=';
      else
        *(out++) = encodingTable[((inbuf [1] & 0x0F) << 2)
                 | ((inbuf [2] & 0xC0) >> 6)];
    }
    if(out-buf < size-1)
    {
      if(fill >= 1)
        *(out++) = '=';
      else
        *(out++) = encodingTable[inbuf [2] & 0x3F];
    }
    bytes += 4;
  }
  if(out-buf < size)
    *out = 0;
  return bytes;
}
