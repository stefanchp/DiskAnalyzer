#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include "common.h"

void send_request_and_print_response(request_t *req) {
    int sockfd;
    struct sockaddr_un addr;
    response_t res;

    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Eroare la creare socket");
        return;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "Eroare: Nu m-am putut conecta la daemon (%s).\n", SOCKET_PATH);
        fprintf(stderr, "Asigura-te ca daemonul ruleaza!\n");
        close(sockfd);
        return;
    }

    if (write(sockfd, req, sizeof(request_t)) != sizeof(request_t)) {
        perror("Eroare la trimitere date");
        close(sockfd);
        return;
    }

    if (read(sockfd, &res, sizeof(response_t)) <= 0) {
        perror("Eroare la primire raspuns");
    } else {
        if (res.status_code == 0) {
            printf("%s\n", res.message);
        } else {
            fprintf(stderr, "Eroare Server: %s\n", res.message);
        }
    }

    close(sockfd);
}

void print_usage(char *prog_name) {
    printf("Usage: %s [OPTIUNI]...\n", prog_name);
    printf("Analizor de spatiu pe disc - Client CLI\n\n");
    printf("  -a, --add <dir>       Adauga un job nou de analiza pentru directorul <dir>\n");
    printf("  -p, --priority <1-3>  Seteaza prioritatea (doar impreuna cu -a)\n");
    printf("                        SAU afiseaza raportul (fara -a) pentru un ID job\n");
    printf("  -S, --suspend <id>    Suspenda jobul cu ID-ul specificat\n");
    printf("  -R, --resume <id>     Reia jobul cu ID-ul specificat\n");
    printf("  -r, --remove <id>     Sterge/Anuleaza jobul cu ID-ul specificat\n");
    printf("  -i, --info <id>       Afiseaza statusul curent pentru un job\n");
    printf("  -l, --list            Listeaza toate joburile active\n");
    printf("\nExemple:\n");
    printf("  %s -a /home/user/docs -p 3\n", prog_name);
    printf("  %s -l\n", prog_name);
    printf("  %s -p 0\n", prog_name);
}

int main(int argc, char *argv[]) {
    int opt;
    int option_index = 0;

    request_t req;
    memset(&req, 0, sizeof(req));
    
    int is_add_action = 0;
    char *add_path_arg = NULL;
    int p_arg_value = -1;
    int has_p_arg = 0;

    req.type = -1; 
    req.priority = 2;

    static struct option long_options[] = {
        {"add",      required_argument, 0, 'a'},
        {"priority", required_argument, 0, 'p'},
        {"suspend",  required_argument, 0, 'S'},
        {"resume",   required_argument, 0, 'R'},
        {"remove",   required_argument, 0, 'r'},
        {"info",     required_argument, 0, 'i'},
        {"list",     no_argument,       0, 'l'},
        {"help",     no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "a:p:S:R:r:i:lh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'a':
                req.type = CMD_ADD;
                is_add_action = 1;
                add_path_arg = optarg;
                break;
            
            case 'p':
                p_arg_value = atoi(optarg);
                has_p_arg = 1;
                break;

            case 'S':
                req.type = CMD_SUSPEND;
                req.id = atoi(optarg);
                break;

            case 'R':
                req.type = CMD_RESUME;
                req.id = atoi(optarg);
                break;

            case 'r':
                req.type = CMD_REMOVE;
                req.id = atoi(optarg);
                break;

            case 'i':
                req.type = CMD_INFO;
                req.id = atoi(optarg);
                break;

            case 'l':
                req.type = CMD_LIST;
                break;

            case 'h':
                print_usage(argv[0]);
                exit(0);

            default:
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }


    if (optind < argc) {
        fprintf(stderr, "Eroare: Argument nerecunoscut: '%s'.\n", argv[optind]);
        fprintf(stderr, "Sugestie: Pentru a analiza un director, foloseste '-a %s'\n", argv[optind]);
        exit(EXIT_FAILURE);
    }

    
    if (is_add_action) {
        strncpy(req.path, add_path_arg, sizeof(req.path) - 1);
        
        if (has_p_arg) {
            if (p_arg_value < 1) p_arg_value = 1;
            if (p_arg_value > 3) p_arg_value = 3;
            req.priority = p_arg_value;
        } else {
            req.priority = 2;
        }
    } 
    else {
        if (has_p_arg && req.type == -1) {
            req.type = CMD_PRINT;
            req.id = p_arg_value;
        }
    }

    if (req.type == -1) {
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    send_request_and_print_response(&req);

    return 0;
}