#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>

#include <errno.h>

#define ALL_SYSTEMS "224.0.0.1"
#define ALL_ROUTERS "224.0.0.2"

#define TIMER_INTERVAL_SECONDS 10   // Интервал Max Response Time
#define MAX_THREADS 10              // Максимальное количество групп / потоков

// Пакет IGMP
;
#pragma pack (push, 1)
typedef struct {
    uint8_t type;
    uint8_t maxTime;
    uint16_t checksum;
    uint32_t gAddr;
} IGMPPack;
#pragma pack (pop)


//typedef struct {

//} StatusFlags;


// Передаваемые потоку обработки одной группы данные
typedef struct {
    int No;                 // Порядковый номер потока
    int flag;               // Флаг из протокола
    time_t timer;           // Таймер протокола
    pthread_t thDesc;       // Дескриптор потока
    char mcGroupAddr[16];   // Мультикаст группа
    char ifaceName[16];     // Имя интерфейса
    char ifaceIP[16];       // IP- адрес на интерфейсе
} ExchangeParameters;


// Состояния
enum states
{
    ST_NON_MEMBER = 0,
    ST_IDLE_MEMBER,
    ST_DELAYING_MEMBER,
    ST_LAST
};

// События
enum messages
{
    MES_NONE = 0,
    MES_LEAVE_GROUP,
    MES_JOIN_GROUP,
    MES_QUERY_RECEIVED,
    MES_REPORT_RECEIVED,
    MES_TIMER_EXPIRED,
    MES_LAST
};

// Таблица переходов (смены состояний)
enum states FSM_new_state[ST_LAST][MES_LAST] = {
    [ST_NON_MEMBER][MES_NONE] = ST_NON_MEMBER,
    [ST_NON_MEMBER][MES_LEAVE_GROUP] = ST_NON_MEMBER,
    [ST_NON_MEMBER][MES_JOIN_GROUP] = ST_DELAYING_MEMBER,
    [ST_NON_MEMBER][MES_QUERY_RECEIVED] = ST_NON_MEMBER,
    [ST_NON_MEMBER][MES_REPORT_RECEIVED] = ST_NON_MEMBER,
    [ST_NON_MEMBER][MES_TIMER_EXPIRED] = ST_NON_MEMBER,

    [ST_IDLE_MEMBER][MES_NONE] = ST_IDLE_MEMBER,
    [ST_IDLE_MEMBER][MES_LEAVE_GROUP] = ST_NON_MEMBER,
    [ST_IDLE_MEMBER][MES_JOIN_GROUP] = ST_IDLE_MEMBER,
    [ST_IDLE_MEMBER][MES_QUERY_RECEIVED] = ST_DELAYING_MEMBER,
    [ST_IDLE_MEMBER][MES_REPORT_RECEIVED] = ST_IDLE_MEMBER,
    [ST_IDLE_MEMBER][MES_TIMER_EXPIRED] = ST_IDLE_MEMBER,

    [ST_DELAYING_MEMBER][MES_NONE] = ST_DELAYING_MEMBER,
    [ST_DELAYING_MEMBER][MES_LEAVE_GROUP] = ST_NON_MEMBER,
    [ST_DELAYING_MEMBER][MES_JOIN_GROUP] = ST_DELAYING_MEMBER,
    [ST_DELAYING_MEMBER][MES_QUERY_RECEIVED] = ST_DELAYING_MEMBER,
    [ST_DELAYING_MEMBER][MES_REPORT_RECEIVED] = ST_IDLE_MEMBER,
    [ST_DELAYING_MEMBER][MES_TIMER_EXPIRED] = ST_IDLE_MEMBER
};

volatile int g_workMode = 1;            // Состояние работы программы: 1-работать, 0-завершить
volatile int g_stopFlags[MAX_THREADS];  // Флаги остановки конкретного потока при удалении группы
//time_t g_timer[MAX_THREADS];            // Таймеры

// Передача пакета на заданный адрес
int igmpSend(int sock, int err, char* ipAddr, IGMPPack* mes) {
    // Удалённый хост
    struct sockaddr_in destHost;
    //memset(&destHost, 0, sizeof(destHost));
    destHost.sin_family = AF_INET;
    destHost.sin_port = htons(3000);
    inet_aton(ipAddr, &destHost.sin_addr);
    return sendto(sock, (void*)mes, sizeof(mes), 0, (struct sockaddr*) &destHost, sizeof(destHost));
}

// Отправка пакета membershipReport
int membershipReport(int sock, int exchangeErr, ExchangeParameters* params) {
    IGMPPack mes;
    mes.type = 0x16;
    mes.maxTime = 0;
    mes.checksum = 0;
    mes.gAddr = inet_addr(params->mcGroupAddr);
    return igmpSend(sock, exchangeErr, params->mcGroupAddr, &mes);
    //9. Получатели сообщений
    //В таблице приведена классификация описанных в документе сообщений по группам адресатов.
    //Тип сообщенияГруппа адресатов
    //Membership ReportГруппа, к которой относится запрос
}


//Выход из группы (leave group), когда хост решил покинуть группу на данном интерфейсе. Это событие может
//происходить только в состоянии Delaying Member или Idle Member.
int leaveGroup(int sock, int exchangeErr, ExchangeParameters* params) {
    IGMPPack mes;
    mes.type = 0x17;
    mes.maxTime = 0;
    mes.checksum = 0;
    mes.gAddr = inet_addr(params->mcGroupAddr);
    return igmpSend(sock, exchangeErr, ALL_ROUTERS, &mes);
    //9. Получатели сообщений
    //В таблице приведена классификация описанных в документе сообщений по группам адресатов.
    //Тип сообщенияГруппа адресатов
    //Leave MessageALL-ROUTERS (224.0.0.2)

    //Когда из группы выходит последний хост, который в ответ на Query передавал сообщение Membership Report для
    //данной группы, этому хосту следует передать сообщение Leave Group по групповому адресу all-routers (224.0.0.2).
    //Если выходящий из группы хост не является последним, кто отвечал на Query, этот хост может не передавать никакого
    //сообщения, поскольку в группе остаются другие хосты.
}


// Функции перехода
void f0(int sock, int exchangeErr, ExchangeParameters* params) {
    //printf("f0\n");
}
void f1(int sock, int exchangeErr, ExchangeParameters* params) {
    printf("f1\n");
    //joinGroup(sock, exchangeErr, params);
    //    Когда хост присоединяется к группе, ему следует незамедлительно передать сообщение Version 2 Membership Report
    //    для этой группы, если данный хост является первым членом этой группы в своей сети. Учитывая возможность потери
    //    или повреждения начального сообщения Membership Report рекомендуется передать 1 или 2 дополнительных копии
    //    этого сообщения с короткой задержкой [Unsolicited Report Interval].
    if (exchangeErr >= 0) {
        for (int i = 0; i < 3; i++) {
            //exchangeErr = membershipReport(sock, exchangeErr, params);
            membershipReport(sock, exchangeErr, params);
            // TODO  Возвращать ошибку обмена
            if (i < 2) sleep(1);
        }
    }
    params->flag = 1;
    params->timer = time(NULL);             // Старт таймера
    //return ST_DELAYING_MEMBER;
}
void f2(int sock, int exchangeErr, ExchangeParameters* params) {
    printf("f2\n");
    params->timer = time(NULL);  // Старт таймера
    //return ST_DELAYING_MEMBER;
}
void f3(int sock, int exchangeErr, ExchangeParameters* params) {
    // Фактически не срабатывает, выход по ctrl+C
    printf("f3\n");
    leaveGroup(sock, exchangeErr, params);
    //return ST_NON_MEMBER;
}
void f4(int sock, int exchangeErr, ExchangeParameters* params) {
    // Фактически не срабатывает, выход по ctrl+C
    printf("f4\n");
    //return ST_NON_MEMBER;
}
void f5(int sock, int exchangeErr, ExchangeParameters* params) {
    printf("f5\n");
    if (TIMER_INTERVAL_SECONDS < (time(NULL) - params->timer)) { params->flag = 1; }
    //return ST_DELAYING_MEMBER;
}
void f6(int sock, int exchangeErr, ExchangeParameters* params) {
    printf("f6\n");
    params->flag = 0;
    //return ST_IDLE_MEMBER;
}
void f7(int sock, int exchangeErr, ExchangeParameters* params) {
    printf("f7 - switching idle member\n");
    membershipReport(sock, exchangeErr, params);
    params->flag = 1;
    // TODO  Возвращать ошибку обмена
    //return ST_IDLE_MEMBER;
}

// Таблица переходов - выполняемых на переходах функций
void (*func[ST_LAST][MES_LAST])() = {
        [ST_NON_MEMBER][MES_NONE]               = f0,
        [ST_NON_MEMBER][MES_LEAVE_GROUP]        = f0,
        [ST_NON_MEMBER][MES_JOIN_GROUP]         = f1,
        [ST_NON_MEMBER][MES_QUERY_RECEIVED]     = f0,
        [ST_NON_MEMBER][MES_REPORT_RECEIVED]    = f0,
        [ST_NON_MEMBER][MES_TIMER_EXPIRED]      = f0,

        [ST_IDLE_MEMBER][MES_NONE]              = f0,
        [ST_IDLE_MEMBER][MES_LEAVE_GROUP]       = f3,   // Фактически не срабатывает, выход по ctrl+C
        [ST_IDLE_MEMBER][MES_JOIN_GROUP]        = f0,
        [ST_IDLE_MEMBER][MES_QUERY_RECEIVED]    = f2,
        [ST_IDLE_MEMBER][MES_REPORT_RECEIVED]   = f0,
        [ST_IDLE_MEMBER][MES_TIMER_EXPIRED]     = f0,

        [ST_DELAYING_MEMBER][MES_NONE]          = f0,
        [ST_DELAYING_MEMBER][MES_LEAVE_GROUP]   = f4,   // Фактически не срабатывает, выход по ctrl+C
        [ST_DELAYING_MEMBER][MES_JOIN_GROUP]    = f0,
        [ST_DELAYING_MEMBER][MES_QUERY_RECEIVED] = f5,
        [ST_DELAYING_MEMBER][MES_REPORT_RECEIVED] = f6,
        [ST_DELAYING_MEMBER][MES_TIMER_EXPIRED] = f7
};


/*!
 * \brief Функция организации обмена по сети
 */
//int exchange(char* mcGroupAddr) {
int exchange(ExchangeParameters* params) {
    printf("Starting with iface %s ip %s group %s\n",
           params->ifaceName,
           params->ifaceIP,
           params->mcGroupAddr);
    int exchangeErr = 0;
    enum states state = ST_NON_MEMBER;
    //enum states state = ST_IDLE_MEMBER;
    params->timer = time(NULL);
    params->flag = 0;

    // Локальный интерфейс
    struct sockaddr_in localFace;
    //memset(&localFace, 0, sizeof(localFace));
    //inet_aton("192.168.88.72", &localFace.sin_addr);
    inet_aton(params->ifaceIP, &localFace.sin_addr);
    //localFace.sin_addr.s_addr = htonl(INADDR_ANY);
    localFace.sin_port = htons(3000);
    localFace.sin_family = AF_INET;

    int mainSock = socket(AF_INET, SOCK_RAW, IPPROTO_IGMP);
    exchangeErr = mainSock;
    if (exchangeErr > 0)
        printf("Socket #%d opened\n", mainSock);
    else
        printf("Socket opening ERROR code #%d\n", mainSock);

    if (exchangeErr >= 0)
        exchangeErr = bind(mainSock, (struct sockaddr*) &localFace, sizeof(localFace));



    // Автомат состояний
    enum messages chng = MES_JOIN_GROUP;    // Хост решил присоединиться к группе
    int bufferSize = 1024;
    char* buffer = (char*)malloc(bufferSize);
    int mesSize;
    while ((g_workMode == 1) && (g_stopFlags[params->No] == 1) && (exchangeErr >= 0)) {

        // Изменение состояния
        func[state][chng](mainSock, exchangeErr, params);
        state = FSM_new_state[state][chng];

        // Сброс сигнала перехода
        chng = MES_NONE;

        // Чтение сокета
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(mainSock, &read_fds);
        // Таймаут на получение
        struct timeval waitTime;
        waitTime.tv_sec = 0;
        waitTime.tv_usec = 50000;
        int recvSize = select(mainSock + 1, &read_fds, NULL, NULL, &waitTime);
        if (recvSize > 0) {
            mesSize = recv(mainSock, buffer, bufferSize, 0);
            // Проверить тип протокола и обработать, если IGMP
            struct iphdr *ip = (struct iphdr*) buffer;
            if (ip->protocol == IPPROTO_IGMP) {

                // Отладочное сообщение
                char* ipAddr1 = inet_ntoa(*(struct in_addr*) &ip->saddr);
                char* ipAddr2 = inet_ntoa(*(struct in_addr*) &ip->daddr);
                printf("Recieved IGMP packet from %s to %s lenght %d\n", ipAddr1, ipAddr2, mesSize);

                // Парсинг сообщений
                IGMPPack* mes = (IGMPPack*)(buffer + sizeof(struct iphdr));
                printf("IGMP message code 0x%x\n", mes->type);

                // Генерация событий по пришедшим пакетам
                if (mes->type == 0x11) { chng = MES_QUERY_RECEIVED; }
                if (mes->type == 0x16) { chng = MES_REPORT_RECEIVED; }
            }
        }

        // Генерация события по таймеру
        if (state == ST_DELAYING_MEMBER) {
            if ((time(NULL) - params->timer) > TIMER_INTERVAL_SECONDS) { chng = MES_TIMER_EXPIRED; }
        }

        //sleep(1);
    }

    //    Когда из группы выходит последний хост, который в ответ на Query передавал сообщение Membership Report для
    //    данной группы, этому хосту следует передать сообщение Leave Group по групповому адресу all-routers (224.0.0.2
    // Функция f3
    if (exchangeErr >= 0) {
        if (state != ST_NON_MEMBER) {
            exchangeErr = leaveGroup(mainSock, exchangeErr, params);
        }
    }

    free(buffer);
    close(mainSock);
    return exchangeErr;
}


// Преобразование имени интерфейса в IP-адрес на нём
void ifToIP(char* ip, char* name) {
    int fd;
    struct ifreq ifr;
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    /* I want to get an IPv4 IP address */
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, name, IFNAMSIZ-1);
    ioctl(fd, SIOCGIFADDR, &ifr);
    close(fd);
    strcpy(ip, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
    //printf("%s\n", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
}


// Обработка сигнала остановки TERM
void sigStop(int sig) {
    g_workMode = 0;
}




int main(int argc,char *argv[])
{
    printf("Multicast checking\n");

    int noErr = 1; //  Флаг отсутствия ошибок

    signal(SIGINT, sigStop);    //  Вешаемся на сигнал остановки TERM

    // Параметры, передаваемые в потоки
    int threadCounter = 0;
    ExchangeParameters params[MAX_THREADS];

    for (int i = 0; i < MAX_THREADS; ++i) {
        g_stopFlags[i] = 1; // Инициализация флагов остановки потоков
        params[i].thDesc = 0;
    }

    // Наполнение первого потока сканирования
    strcpy(params[0].mcGroupAddr, "224.0.0.88");
    strcpy(params[0].ifaceName, "eno1");
    strcpy(params[0].ifaceIP, "192.168.88.216");

    // Разбор опций командной строки, настройка первого потока
    //char iface[16];
    int opchar = 0;
    //int optindex = 0;
    extern char* optarg;
//    struct option opts[] = {
//        {"iface", required_argument, &iface, 'i'},
//        {"iface", required_argument, &iface, 'g'},
//        {0, 0, 0, 0}
//    };
    while (-1 != (opchar = getopt(argc, argv, "i:g:"))) {
        switch (opchar) {
        case 'i':
            strcpy(params[0].ifaceName, optarg);
            // Проверку корректности введённого интерфейса можно реализовать
            // попыткой открыть такой файл в /sys/class/net/
            //if (ifToIP(params[0].ifaceIP, params[0].ifaceName) != 0) noErr = 0;
            ifToIP(params[0].ifaceIP, params[0].ifaceName);
            break;
        case 'g':
            strcpy(params[0].mcGroupAddr, optarg);
            break;
        }
    }


    // Обмен по сети выделяется в отдельный поток для того, чтобы обеспечить отклик
    // интерфейса пользователя
    if (noErr) {
        params[0].No = threadCounter++;
        if (pthread_create(&(params[0].thDesc), NULL, (void*)&exchange, &params) == 0) {
            printf("Exchange started\n");
        }
    }

    // Цикл ввода команд пользователем
    int commMaxSize = 10;
    char comm[commMaxSize];
    const char* commQuit = "q";
    while ((g_workMode == 1) && (noErr)) {
        //fgets(comm, commMaxSize, stdin);
        //puts(comm);


//    // По команде на добавление группы создаётся новый поток, описание в массиве params
//    // Обмен по сети выделяется в отдельный поток для того, чтобы обеспечить отклик
//    // интерфейса пользователя
//    // Одна группа - один поток. 1. Групп не предполагается много 2. Так удобно 3. Отдельный обмен по каждой группе
//    pthread_t thDesc;
//    if (pthread_create(&thDesc, NULL, (void*)&exchange, NULL) == 0) {
//        printf("Exchange started\n");
//    }


        //if (strcmp(comm, commQuit) == 0) sigStop(1); //else puts("wrong"); //g_workMode = 0;
        sleep(1);
    }

    //pthread_join(params[0].thDesc, NULL);
    for (int i = 0; i < MAX_THREADS; ++i) {
        if (params[i].thDesc != 0) pthread_join(params[i].thDesc, NULL);
    }
    return 0;
}


/*

TODO:
Написать консольное приложение, которое будет эмулировать поведение IGMPv2-клиента:
отправлять в сетевые интерфейсы ПК запросы на принятие рассылок мультикастного трафика,
прекращать подписку на них, и отвечать на приходящие из сети запросы.
Работа протокола IGMP версии 2 в приложении должна соответствовать стандарту IETF,
описанному в документе RFC 2236 - https://tools.ietf.org/html/rfc2236.
Управление приложением производится через интерфейс командной строки.

Реализовать на чистом Си. Сборка должна проходить с флагом -Werror без ошибок.

Описание требуемого поведения:
Параметрами командной строки утилите передаётся диапазон IP-адресов мультикастных групп
и имя сетевого интерфейса (ethN), через который должны отправляться IGMP-пакеты.
При запуске утилита посылает IGMP Report на все указанные в командной строке группы и
переходит в режим ожидания.
Утилита должна отвечать пакетами IGMP Report на входящие с ethN сообщения IGMP General Query и
IGMP Group Specific Query (если GSQ пришёл на одну из прослушиваемых в данный момент групп).
Ответ должен быть реализован с задержкой согласно RFC 2236.
Также в режиме ожидания должна быть реализована возможность корректировки списка
прослушиваемых групп с клавиатуры: команда "add <IP>" добавляет группу к списку с
последующей отправкой IGMP Report, команда "del <IP>" удаляет группу из списка
с последующей отправкой IGMP Leave. Команда "print" распечатывает список мультикастных IP-адресов,
на которые в данный момент подписан клиент, и название сетевого интерфейса, на котором работает утилита.
Генерацию IGMP Query и мультикастного трафика для проверки работы программы осуществлять генератором Scapy
(при наличии навыков использования других генераторов трафика допускаются и они).
К тестовому заданию приложить скрипты для генератора трафика,
использовавшиеся для проверки программы и файлы Wireshark с захваченным трафиком,
демонстрирующие её работу (ввод команд, обработка трафика).

Описание стандарта
RFC 2236 (Internet Group Management Protocol, Version 2) - https://tools.ietf.org/html/rfc2236
Описание всех состояний, событий и производимых действий, которые требуется реализовать в рамках задания,
содержится в разделе 6. Host State Diagram.
Обработку IGMPv1 Query, описанную в разделе 6, реализовывать не требуется (часть,
начинающаяся со слов "In addition, a host may be in one of two possible states with respect
to any single network interface:" и блок-схема на стр. 10).

Пожелания к реализации (будет плюсом):
- Архитектура приложения, выбор необходимого и достаточного количества потоков определяется исполнителем задания.
Нужно уметь объяснить, почему из множества возможных вариантов было выбранно именно этот, каковы его недостатки и
приемущества по сравнению с другими вариантами.
- Реализовать машину состояний через таблицу переходов, представить состояния и события из RFC в виде enum'ов.
Почитать про "Finite State Machine Transition table C implementation", посмотреть на примеры реализации.
- Реализовать парсинг аргументов (опций), задаваемых при запуске приложения, через функцию getopt.
- Выбрать единый code convention и соблюдать его во всём коде.
- Поясняющие комментарии, особенно в местах, реализующих конкретные положения RFC (можно с выдержками оттуда).
- Наличие и корректность обработка ошибок, кодов возврата функций также будет оцениваться.
- Выложить проект на GitHub для Code Review.

datatracker.ietf.org (https://tools.ietf.org/html/rfc2236)
RFC 2236 - Internet Group Management Protocol, Version 2
Internet Group Management Protocol, Version 2 (RFC 2236)

*/
