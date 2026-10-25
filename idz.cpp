#include <stdio.h>      // printf, perror, fprintf
#include <stdlib.h>     // exit, EXIT_FAILURE, EXIT_SUCCESS
#include <string.h>     // memset
#include <unistd.h>     // close
#include <errno.h>      // errno
#include <sys/socket.h> // socket, bind
#include <netinet/in.h> // sockaddr_in, in_addr, INADDR_ANY
#include <arpa/inet.h>  // htons, htonl, inet_pton, inet_ntop
#include <sys/types.h>   // pid_t

/* Константы для настройки сокета */ 
// define Заменяет все вхождения имени в коде на значение до компиляции
#define SERVER_IP   "127.0.0.1"  /* IP-адрес для привязки */
#define SERVER_PORT 9090         /* Порт для привязки */

void print_open_fds(const char *label)
{
  printf("\n--- Открытые FD (%s) ---\n", label);

  pid_t pid = getpid(); //pid_t-у каждого запущенного процесса (программы) есть номер — PID (Process ID). Ядро по этому номеру понимает, о каком процессе речь. getpid() - возвращает уникальный номер текущего процесса
  char cmd[256];
  snprintf(cmd, sizeof(cmd), "lsof -p %d 2>/dev/null", (int)pid); // snprintf - записывает результат в масив, 1-куда писать,2-сколько места есть, шаблон, что подставляем
  system(cmd); // запускает шел и передает строку в виде команды
}

int main(void)
{
    /* ============================================================
     * ШАГ 1: Создание сокета
     * ============================================================
     */
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0); // создаем сокет и кладем в переменную  (домен(IPV4), тип, протокол(0 автовыбор))
    if (sockfd < 0) // >= 0(дескриптор) или -1(ошибка)
    {
        perror("socket"); //печатает в терминал сообщение об ошибке: сначала текст "socket", потом двоеточие, потом расшифровку последней ошибки из переменной errno.
        return EXIT_FAILURE; //return EXIT_FAILURE; — немедленно завершает программу (выходит из main) и сообщает операционной системе, что программа завершилась с ошибкой. EXIT_FAILURE по умолчанию равна 1 (провал)
    }
    printf("Сокет создан успешно. Файловый дескриптор: %d\n", sockfd); //sockfd то что подставляется вместо d

    /* ============================================================
     * ШАГ 2: Заполнение структуры адреса
     * ============================================================
     */
    struct sockaddr_in server_addr; // создаем структуру. Создай переменную с именем server_addr типа sockaddr_in(шаблон для хранения IPv4-адреса (IP + порт + семейство).)
    memset(&server_addr, 0, sizeof(server_addr)); // memset-заполняет память структуры. server_addr-куда писать, 0-какими значениями заполнить, 3 аргумент - ск байт заполнить нулями. эта строка делает структуру 'чистой'
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(SERVER_PORT); // htons - преобразует из пордка хоста в сетевой порядок 16 бит 

    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) //inet_pton-«интернет-представление в число». Преобразует IP из текста в бинарный формат. 3 аргумент - этот куда записать результат
    {                                                              // 1-успех 0-неверный формат IP -1 - Системная ошибка
        if (errno == 0) // erno— это глобальная переменная (число), в которую ядро записывает код последней ошибки, когда системный вызов падает.
        {
          fprintf(stderr, "Ошибка: неверный формат IP-адреса '%s'\n", SERVER_IP); // fprintf-печать с форматирование в поток, stderr-поток ошибок
        }
        else
        {
          perror("inet_pton");
        }
        close(sockfd); // закрываем сокет о свобождаем FD
        return EXIT_FAILURE;
    }
    printf("Адрес заполнен: %s:%d\n", SERVER_IP, SERVER_PORT); //s-строка, d-целое число

    /* ============================================================
     * ШАГ 3: Вывод адреса в текстовом виде
     * ============================================================
     */
    char ip_str[INET_ADDRSTRLEN]; // создаем массив символов, INET_ADDRSTRLEN = 16 — константа из <arpa/inet.h>
    if (inet_ntop(AF_INET, &server_addr.sin_addr, ip_str, sizeof(ip_str)) == NULL) // inet_ntop-Преобразует IP из бинарного в текст. 2аргумент -откуда читать бинарный ip, 3аргумент куда записать строку, 4агумент-сколько байт доступно(защита от переполнения)
    {                                                                              // возвращает null при ошибке
      perror("inet_ntop");
      close(sockfd);
      return EXIT_FAILURE;
    }
    printf("Проверка адреса: %s:%d\n", ip_str, ntohs(server_addr.sin_port));

    /* ============================================================
     * ШАГ 4: Привязка сокета к адресу (bind)
     * ============================================================
     */
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) //1аргумент-наш сокет, 2какой адрес привязать(«Возьми адрес server_addr и считай его указателем на sockaddr».)sockaddr-это универсальная структура для хранения любого сетевого адреса: IPv4, IPv6, UNIX-сокетов
    {
      if (errno == EADDRINUSE) { // EADDRINUSE=48 порт занят
        fprintf(stderr, "Ошибка: порт %d уже используется (EADDRINUSE, errno=%d)\n", SERVER_PORT, errno);
      } else if (errno == EACCES) { // EACCES = 13 нет прав на порт 
        fprintf(stderr, "Ошибка: нет прав на порт %d (EACCES, errno=%d)\n", SERVER_PORT, errno);
      } else {
        perror("bind");
      }
      close(sockfd);
      return EXIT_FAILURE;
    }
    printf("Сокет успешно привязан к %s:%d\n", SERVER_IP, SERVER_PORT);

    /* ============================================================
     * ШАГ 5: Вывод информации о сокете
     * ============================================================
     */
    printf("\n--- Информация о сокете ---\n");
    printf("Файловый дескриптор: %d\n", sockfd);
    printf("Семейство адресов: AF_INET (%d)\n", server_addr.sin_family);
    printf("Порт (сетевой): 0x%04X\n", ntohs(server_addr.sin_port));
    printf("IP-адрес: %s\n", ip_str);


    print_open_fds("ДО close() — сокет должен быть в списке");
    getchar(); // останавливает программу и ждет ввода символа

    /* ============================================================
     * ШАГ 6: Закрытие сокета
     * ============================================================
     */
    if (close(sockfd) < 0) //0-успех -1 ошибка
    {
      perror("close");
      return EXIT_FAILURE;
    }
    printf("\nСокет закрыт. Дескриптор %d освобождён.\n", sockfd);

    /*
    * ШАГ 8 (средний уровень): FD после close()
    * Сокета в списке быть не должно — FD 3 освобождён.
    */
    print_open_fds("ПОСЛЕ close() — сокета быть не должно");
    return EXIT_SUCCESS;
}

