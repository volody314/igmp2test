# igmp2test
Эмулятор поведения клиента IGMP2

## Запускать от рута ! Raw sockets !

### Ключи:

-i      - задание рабочего интерфейса. По умолчанию eno1
          Пример:
          -i wlo1

-g      - задание мультикаст- группы. По умолчанию "224.0.0.99"
          Пример:
          -g "224.0.0.88"

### Пример запуска:

igmp2test -i eno1 -g "224.0.0.100"


### Реализовано

Из основного функционала:

+ задание интерфейса
+ задание группы
+ отправка Report при запуске
+ отправка запроса на вхождение в группу
+ отправка сообщения о покидании группы
+ приём и парсинг входящих IGMP2 пакетов
+ протокол со стороны хоста реализован в мере его осознания

Из раздела "Пожелания к реализации" реализовано всё


### TODO

- не отлажены скрипты тестирования программы на Scapy (не оттестированы ответы программы)
- нет интерактивного добавления и удаления новых групп (архитектура программы подготовлена,
  сделать можно за час-два, но без решения проблемы тестирования введение данных функций бессмысленно)


### Приложены:

1. wireshark.pcapng

   Жёлтые пакеты - отправляет утиллита

   Красные пакеты отправляют скрипты Scapy


2. igmp-query-send.py и igmp-report-send.py  - Питоновые скрипты под Scapy с попыткой запуска
соответствующих пакетов

Пока не работают как задуманно:
- гонят пакеты с на 2 байта обрезанной нагрузкой
- не считают контрольную сумму
- Wireshark видит пакеты, отправленные sr1, но не sendp


[root@fedora igmp2test]# python3 igmp-query-send.py
###[ IP ]###
  version   = 4
  ihl       = 5
  tos       = 0x0
  len       = 29
  id        = 1
  flags     =
  frag      = 0
  ttl       = 64
  proto     = igmp
  chksum    = 0x815c
  src       = 192.168.88.216
  dst       = 224.0.0.1
  \options   \
###[ Raw ]###
     load      = '\x11\x00\x00\x00à\x00\x00c'

Begin emission:
Finished sending 1 packets.
.^C
Received 1 packets, got 0 answers, remaining 1 packets
[root@fedora igmp2test]# python3 igmp-report-send.py
###[ IP ]###
  version   = 4
  ihl       = 5
  tos       = 0x0
  len       = 29
  id        = 1
  flags     =
  frag      = 0
  ttl       = 64
  proto     = igmp
  chksum    = 0x815c
  src       = 192.168.88.216
  dst       = 224.0.0.1
  \options   \
###[ Raw ]###
     load      = '\x16\x00\x00\x00à\x00\x00c'

Begin emission:
Finished sending 1 packets.
................................................^C
Received 48 packets, got 0 answers, remaining 1 packets

