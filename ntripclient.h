#ifndef NTRIPCLIENT
#define NTRIPCLIENT
//#define MAXDATASIZE 1000 /* max number of bytes we can get at once */

//Установка параметров для подключения
enum MODE { HTTP = 1, NTRIP1 = 3, AUTO = 4, UDP = 5, END };

//char buf[MAXDATASIZE];

typedef struct _Buffer {
    char *buf;
    int size_msg;                                     //Размер сообщения
    int buyte_count;                                  //кол-во принятых байт
} Buffer;

class NtCl{                                           //Объявление класса NtCl
public:
    char *server,                                     //Параметры сервера(IP)
         *port,                                       //Порт
         *user,                                       //Имя пользователя
         *password,                                   //Пароль
         *mnt;                                        //Точка монтирования
    int   mode,                                       //Выбор режима NTRIP1,AUTO
          reconect_time;                              //вр. перодкл


    NtCl(char *s, char *pr, char *u, char *p, char *m, int md, int recon_time);

//Установка параметров подключения
    void SetParam(char *s, char *pr, char *u, char *p, char *m, int md, int recon_time);
//Запуск подключения NtripClient
    void Run(void);
//Получаемые данные, блокировка и разблокировка потока
    int RECV(int __fd, void *__buf, size_t __n, int __flags);
//Принятый размер сообщения в буфер
    Buffer GetBuf();
//Функция блокировки потока
    void lock();
//Функция разблокировки потока
    void unlock();
    void Stop();

int numbytes;
int totalbytes;
int start_last_msg;

private:
    int stop;
    int sleeptime;
    int error;
    char *proxyserver;
    char proxyport[6];
    char *b;
    long i;

    pthread_attr_t attr;

    Buffer msg_buf;
    pthread_mutex_t mutex;                                //базовый тип мьютекса
    void RunTh(void);
    void RunTh1(void);
    static void *StartRunTh(void*);
    static int encode(char *buf, int size, char *user, char *pwd);
};                                                        //Конец объявления класса

#endif // NTRIPCLIENT

