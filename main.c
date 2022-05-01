#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

// Пакет IGMP
;
#pragma pack (push, 1)
typedef struct {
    uint8_t compl;
    uint8_t maxTime;
    uint16_t checksum;
    uint32_t gAddr;
} IGMPPack;
#pragma pack (pop)

// Состояния
enum states
{
    ST_NON_MEMBER = 0,
    ST_IDLE_MEMBER,
    ST_DELAYING_MEMBER,
    ST_LAST
};

// Воздействия
enum messages
{
    MES_LEAVE_GROUP,
    MES_JOIN_GROUP,
    MES_QUERY_RECEIVED,
    MES_REPORT_RECEIVED,
    MES_TIMER_EXPIRED,
    MES_LAST
};

//// Таблица переходов (смены состояний)
//enum states FSM_new_state[ST_LAST][MES_LAST] = {
//    [ST_NON_MEMBER][MES_JOIN_GROUP] = ST_DELAYING_MEMBER,
//    [ST_IDLE_MEMBER][MES_QUERY_RECEIVED] = ST_DELAYING_MEMBER,
//    [ST_IDLE_MEMBER][MES_LEAVE_GROUP] = ST_NON_MEMBER,
//    [ST_DELAYING_MEMBER][MES_LEAVE_GROUP] = ST_NON_MEMBER,
//    [ST_DELAYING_MEMBER][MES_QUERY_RECEIVED] = ST_DELAYING_MEMBER,
//    [ST_DELAYING_MEMBER][MES_REPORT_RECEIVED] = ST_IDLE_MEMBER,
//    [ST_DELAYING_MEMBER][MES_TIMER_EXPIRED] = ST_IDLE_MEMBER
//};

int g_workMode = 1; // Состояние работы программы: 1-работать, 0-завершить

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


int membershipReport(int sock, int exchangeErr, char* mcGroupAddr) {
    IGMPPack mes;
    mes.compl = 0x16;
    mes.maxTime = 0;
    mes.checksum = 0;
    mes.gAddr = inet_addr(mcGroupAddr);
    return igmpSend(sock, exchangeErr, "127.0.0.1", &mes);
}


//Вхождение в группу (join group), когда хост решил присоединиться к группе на данном интерфейсе. Это
//событие может происходить только в состоянии Non-Member.
int joinGroup(int sock, int exchangeErr, char* mcGroupAddr) {
    IGMPPack mes;
    mes.compl = 0x11;
    mes.maxTime = 0;
    mes.checksum = 0;
    mes.gAddr = inet_addr(mcGroupAddr);
    return igmpSend(sock, exchangeErr, "127.0.0.1", &mes);
}

//Выход из группы (leave group), когда хост решил покинуть группу на данном интерфейсе. Это событие может
//происходить только в состоянии Delaying Member или Idle Member.
int leaveGroup(int sock, int exchangeErr, char* mcGroupAddr) {
    IGMPPack mes;
    mes.compl = 0x17;
    mes.maxTime = 0;
    mes.checksum = 0;
    mes.gAddr = inet_addr(mcGroupAddr);
    return igmpSend(sock, exchangeErr, "224.0.0.2", &mes);
}


// Функции перехода
enum states f1(int sock, int exchangeErr, char* mcGroupAddr) {
    joinGroup(sock, exchangeErr, mcGroupAddr);
    return ST_DELAYING_MEMBER;
}
enum states f2(int sock, int exchangeErr, char* mcGroupAddr) {
    return ST_DELAYING_MEMBER;
}
enum states f3(int sock, int exchangeErr, char* mcGroupAddr) {
    return ST_NON_MEMBER;
}
enum states f4(int sock, int exchangeErr, char* mcGroupAddr) {
    return ST_NON_MEMBER;
}
enum states f5(int sock, int exchangeErr, char* mcGroupAddr) {
    return ST_DELAYING_MEMBER;
}
enum states f6(int sock, int exchangeErr, char* mcGroupAddr) {
    return ST_IDLE_MEMBER;
}
enum states f7(int sock, int exchangeErr, char* mcGroupAddr) {
    return ST_IDLE_MEMBER;
}

// Таблица переходов - выполняемых на переходах функций
enum states (*func[ST_LAST][MES_LAST])() = {
    [ST_NON_MEMBER][MES_JOIN_GROUP] = f1,
    [ST_IDLE_MEMBER][MES_QUERY_RECEIVED] = f2,
    [ST_IDLE_MEMBER][MES_LEAVE_GROUP] = f3,
    [ST_DELAYING_MEMBER][MES_LEAVE_GROUP] = f4,
    [ST_DELAYING_MEMBER][MES_QUERY_RECEIVED] = f5,
    [ST_DELAYING_MEMBER][MES_REPORT_RECEIVED] = f6,
    [ST_DELAYING_MEMBER][MES_TIMER_EXPIRED] = f7
};


/*!
 * \brief Функция организации обмена по сети
 */
int exchange(char* mcGroupAddr) {
    int exchangeErr = 0;
    enum states state = ST_NON_MEMBER;

    // IGMP пакет
    IGMPPack mes;
    //char mcGroupAddr[16] = "224.0.0.88";
    mes.gAddr = inet_addr(mcGroupAddr);

    // Локальный интерфейс
    struct sockaddr_in localFace;
    //memset(&localFace, 0, sizeof(localFace));
    //inet_aton("192.168.88.72", &localFace.sin_addr);
    inet_aton("127.0.0.1", &localFace.sin_addr);
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

    //    Когда хост присоединяется к группе, ему следует незамедлительно передать сообщение Version 2 Membership Report
    //    для этой группы, если данный хост является первым членом этой группы в своей сети. Учитывая возможность потери
    //    или повреждения начального сообщения Membership Report рекомендуется передать 1 или 2 дополнительных копии
    //    этого сообщения с короткой задержкой [Unsolicited Report Interval].
    if (exchangeErr >= 0) {
        for (int i = 0; i < 3; i++) {
            exchangeErr = membershipReport(mainSock, exchangeErr, mcGroupAddr);
            if (i < 2) sleep(1);
        }
    }

    // Автомат состояний
    enum messages chng = MES_JOIN_GROUP;
    while ((g_workMode == 1) && (exchangeErr >= 0)) {


        state = func[state][chng](mainSock, exchangeErr, mcGroupAddr);
    }

    //    Когда из группы выходит последний хост, который в ответ на Query передавал сообщение Membership Report для
    //    данной группы, этому хосту следует передать сообщение Leave Group по групповому адресу all-routers (224.0.0.2
    if (exchangeErr >= 0) {
        //mes.compl = htonl(0x17000000);
        if (state != ST_NON_MEMBER) {
            exchangeErr = leaveGroup(mainSock, exchangeErr, mcGroupAddr);
        }
    }

    close(mainSock);
    return exchangeErr;
}


int main(int argc,char *argv[])
{
    printf("Multicast checking\n");

    // Обмен по сети выделяется в отдельный поток для того, чтобы обеспечить отклик
    // интерфейса пользователя
    pthread_t thDesc;
    char mcGroupAddr[16] = "224.0.0.88";
    if (pthread_create(&thDesc, NULL, (void*)&exchange, &mcGroupAddr) == 0) {
        printf("Exchange started\n");
    }

    //while (g_workMode == 1) {

//    // Обмен по сети выделяется в отдельный поток для того, чтобы обеспечить отклик
//    // интерфейса пользователя
//    pthread_t thDesc;
//    if (pthread_create(&thDesc, NULL, (void*)&exchange, NULL) == 0) {
//        printf("Exchange started\n");
//    }

    // g_workMode = 0;
    //}

    pthread_join(thDesc, NULL);
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
