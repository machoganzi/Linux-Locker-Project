#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <signal.h>
#include <sys/time.h>
#include "student.h"

#define PORT 8080
#define MAX_PASSWORD_LENGTH 12  //비밀번호 최대 자릿수 설정 - 최대 12자리
#define LINE "----------------------------------------\n"
#define MAX_LOCKERS 10

int password_length = 4; // 초기 비밀번호 길이 4
volatile sig_atomic_t input_timeout = 0; // 타이머 플래그
volatile sig_atomic_t warning_issued_10s = 0; // 경고 메시지 플래그 (10초)
volatile sig_atomic_t warning_issued_20s = 0; // 경고 메시지 플래그 (20초)

// 함수 선언
void handle_server_response(int sock, char* buffer, int buffer_size);
void get_info(int sock);
void get_locker_status(int sock, int locker_num, char* buffer, int buffer_size);
int is_valid_password(const char* password);
void set_pwd(int sock, int locker_num, int student_id);
void ch_pwd(int sock, int locker_num, int student_id);
int verify(int sock, int locker_num);
void store_content(int sock, int locker_num);
void remove_content(int sock, int locker_num);
void return_locker(int sock, int locker_num, int student_id);
void display_menu(int sock, int locker_num);
int check_user_lockers(int sock, int student_id, int *locker_nums);
void print_kiosk_header(const char* title);
void clear_screen();
void display_locker_contents(int sock, int locker_num);
void extend_time(int sock, int locker_num);
void show_remaining_time(int sock, int locker_num);
void get_student_lockers(int sock, int student_id);
void alarm_handler(int signum); // 시그널 핸들러 선언
void set_timer(int seconds); // 타이머 설정 함수 선언
void reset_timer(); // 타이머 리셋 함수 선언

// 서버로부터 응답을 처리
void handle_server_response(int sock, char* buffer, int buffer_size) {
    int n = read(sock, buffer, buffer_size - 1);
    if (n > 0) {
        buffer[n] = '\0';
    }
}

// 서버에서 정보를 요청
void get_info(int sock) {
    char message[100];
    sprintf(message, "GET_INFO");
    write(sock, message, strlen(message));
    char response[1024];
    handle_server_response(sock, response, sizeof(response));
    printf("%s\n", response);
}

// 사물함의 상태를 확인
void get_locker_status(int sock, int locker_num, char* buffer, int buffer_size) {
    char message[100];
    sprintf(message, "STATUS %d", locker_num);
    write(sock, message, strlen(message));
    handle_server_response(sock, buffer, buffer_size);
}

// 비밀번호가 유효한지 확인
int is_valid_password(const char* password) {
    if (strlen(password) != password_length) {
        return 0;
    }
    for (int i = 0; i < password_length; i++) {
        if (!isdigit(password[i])) {
            return 0;
        }
    }
    return 1;
}

// 사물함 비밀번호를 설정
void set_pwd(int sock, int locker_num, int student_id) {
    char password[MAX_PASSWORD_LENGTH + 1];
    char confirm_password[MAX_PASSWORD_LENGTH + 1];
    int attempts = 3;  // 비밀번호 확인 시도 횟수
    clear_screen();
    print_kiosk_header("비밀번호 설정");
    while (1) {
        printf("%d자리 숫자로 비밀번호를 입력하세요: ", password_length);
        reset_timer(); // 타이머 리셋
        set_timer(10); // 10초 타이머 설정
        if (scanf("%s", password) != 1 || input_timeout) {
            printf("입력 시간 초과!\n");
            close(sock);
            return;
        }
        reset_timer(); // 타이머 해제
        if (is_valid_password(password)) {
            while (attempts > 0) {
                printf("비밀번호를 다시 한번 입력하세요. : ");
                reset_timer(); // 타이머 리셋
                set_timer(10); // 10초 타이머 설정
                if (scanf("%s", confirm_password) != 1 || input_timeout) {
                    printf("입력 시간 초과!\n");
                    close(sock);
                    return;
                }
                reset_timer(); // 타이머 해제
                if (strcmp(password, confirm_password) == 0) {
                    clear_screen();
                    break;
                } else {
                    attempts--;
                    if (attempts == 0) {
                        printf("입력 횟수가 초과되었습니다. 프로그램을 종료합니다.\n");
                        char message[100];
    			sprintf(message, "RETURN %d %d", locker_num, student_id);
    			write(sock, message, strlen(message));
                        close(sock);
                        exit(0);
                    }
                    clear_screen();
                    printf("비밀번호가 일치하지 않습니다. 다시 시도해주세요. (%d/3)\n", attempts);
                    print_kiosk_header("비밀번호 설정");
                }
            }
            if (attempts > 0) {
                break;
            }
        } else {
            clear_screen();
            printf("유효하지 않은 비밀번호입니다. %d자리 숫자로 입력해주세요.\n", password_length);
            print_kiosk_header("비밀번호 설정");
        }
    }
    char message[150];
    sprintf(message, "SET_PWD %d %d %s", locker_num, student_id, password);
    write(sock, message, strlen(message));
    char response[1024];
    handle_server_response(sock, response, sizeof(response));
    printf("%s\n", response);
}

// 사물함 비밀번호를 변경
void ch_pwd(int sock, int locker_num, int student_id) {
    char password[MAX_PASSWORD_LENGTH + 1];
    char confirm_password[MAX_PASSWORD_LENGTH + 1];
    int attempts = 3;  // 비밀번호 확인 시도 횟수
    clear_screen();
    print_kiosk_header("비밀번호 변경");
    while (1) {
        printf("%d자리 숫자로 새로운 비밀번호를 입력하세요: ", password_length);
        reset_timer(); // 타이머 리셋
        set_timer(10); // 10초 타이머 설정
        if (scanf("%s", password) != 1 || input_timeout) {
            printf("입력 시간 초과!\n");
            close(sock);
            return;
        }
        reset_timer(); // 타이머 해제
        if (is_valid_password(password)) {
            while (attempts > 0) {
                printf("비밀번호를 다시 한번 입력하세요. : ");
                reset_timer(); // 타이머 리셋
                set_timer(10); // 10초 타이머 설정
                if (scanf("%s", confirm_password) != 1 || input_timeout) {
                    printf("입력 시간 초과!\n");
                    close(sock);
                    return;
                }
                reset_timer(); // 타이머 해제
                if (strcmp(password, confirm_password) == 0) {
                    clear_screen();
                    break;
                } else {
                    attempts--;
                    if (attempts == 0) {
                        printf("입력 횟수가 초과되었습니다. 프로그램을 종료합니다.\n");
                        close(sock);
                        exit(0);
                    }
                    clear_screen();
                    printf("비밀번호가 일치하지 않습니다. 다시 시도해주세요. (%d/3)\n", attempts);
                    print_kiosk_header("비밀번호 변경");
                }
            }
            if (attempts > 0) {
                break;
            }
        } else {
            clear_screen();
            printf("유효하지 않은 비밀번호입니다. %d자리 숫자로 입력해주세요.\n", password_length);
            print_kiosk_header("비밀번호 변경");
        }
    }
    char message[150];
    sprintf(message, "CH_PWD %d %d %s", locker_num, student_id, password);
    write(sock, message, strlen(message));
    char response[1024];
    handle_server_response(sock, response, sizeof(response));
    printf("%s\n", response);
}


// 사물함 비밀번호를 확인
int verify(int sock, int locker_num) {
    char password[MAX_PASSWORD_LENGTH + 1], response[1024];
    int attempts = 3;
    clear_screen();
    
    while (attempts > 0) {
        print_kiosk_header("비밀번호 확인");
        printf("%d자리 숫자로 비밀번호를 입력하세요: ", password_length);
        reset_timer(); // 타이머 리셋
        set_timer(10); // 10초 타이머 설정
        if (scanf("%s", password) != 1 || input_timeout) {
            printf("입력 시간 초과!\n");
            close(sock);
            return 0;
        }
        reset_timer(); // 타이머 해제
        if (!is_valid_password(password)) {
            clear_screen();
            printf("유효하지 않은 비밀번호입니다. %d자리 숫자로 입력해주세요.\n", password_length);
            continue;
        }
        char message[100];
        sprintf(message, "VERIFY %d %s", locker_num, password);
        write(sock, message, strlen(message));
        handle_server_response(sock, response, sizeof(response));
        clear_screen();
        if (strstr(response, "[비어있음]") || response[0] != '<') {
            return 1;
        } else if (strstr(response, "비밀번호를 3회 이상 틀렸습니다")) {
            printf("%s\n", response); // 서버 응답 출력
            return 0;
        } else if(strstr(response, "잠겨 있습니다.")){
          printf("%s\n", response); // 서버 응답 출력
          return 0;  
        }
         else {
            printf("%s\n", response); // 서버 응답 출력
        }
        attempts--;
    }
    return 0;
}

// 사물함에 내용을 저장
void store_content(int sock, int locker_num) {
    char content[256];
    clear_screen();
    print_kiosk_header("내용 저장");
    printf("저장할 내용을 입력하세요: ");
    reset_timer(); // 타이머 리셋
    set_timer(10); // 10초 타이머 설정
    if (fgets(content, sizeof(content), stdin) == NULL || input_timeout) {
        printf("입력 시간 초과!\n");
        close(sock);
        return;
    }
    reset_timer(); // 타이머 해제
    content[strcspn(content, "\n")] = '\0';
    char message[512];
    sprintf(message, "STORE %d %s", locker_num, content);
    write(sock, message, strlen(message));
    char response[1024];
    clear_screen();
    handle_server_response(sock, response, sizeof(response));
    printf("%s\n", response);
}

// 사물함에서 내용을 제거
void remove_content(int sock, int locker_num) {
    char content[256];
    clear_screen();
    print_kiosk_header("내용 제거");
    printf("제거할 내용을 입력하세요: ");
    reset_timer(); // 타이머 리셋
    set_timer(10); // 10초 타이머 설정
    if (fgets(content, sizeof(content), stdin) == NULL || input_timeout) {
        printf("입력 시간 초과!\n");
        close(sock);
        return;
    }
    reset_timer(); // 타이머 해제
    content[strcspn(content, "\n")] = '\0';
    char message[512];
    sprintf(message, "REMOVE %d %s", locker_num, content);
    write(sock, message, strlen(message));
    char response[1024];
    clear_screen();
    handle_server_response(sock, response, sizeof(response));
    printf("%s\n", response);
}

// 사물함을 반납
void return_locker(int sock, int locker_num, int student_id) {
    char message[100];
    sprintf(message, "RETURN %d %d", locker_num, student_id);
    write(sock, message, strlen(message));
    char response[1024];
    handle_server_response(sock, response, sizeof(response));
    printf("%s\n", response);
}

// 사물함 메뉴를 출력
void display_menu(int sock, int locker_num) {
    print_kiosk_header("메뉴");
    char response[1024];
    get_locker_status(sock, locker_num, response, sizeof(response));
    printf("현재 사물함 내용물: %s\n", response);
    printf("1. 물건 넣기\n");
    printf("2. 물건 빼기\n");
    printf("3. 비밀번호 재설정\n");
    printf("4. 사물함 반납\n");
    printf("5. 시간연장\n");
    printf("6. 현재 사물함의 남은 사용시간\n");
    printf("7. 종료\n");
    printf("선택: ");
}

// 사물함의 내용을 출력
void display_locker_contents(int sock, int locker_num) {
    char response[1024];
    get_locker_status(sock, locker_num, response, sizeof(response));
    printf("현재 사물함 내용물: %s\n", response);
}

// 사물함 사용 시간을 연장
void extend_time(int sock, int locker_num) {
    char message[100];
    sprintf(message, "EXTEND %d", locker_num);
    write(sock, message, strlen(message));
    char response[1024];
    handle_server_response(sock, response, sizeof(response));
    printf("%s\n", response);
}

// 사물함의 남은 시간을 출력
void show_remaining_time(int sock, int locker_num) {
    char message[100];
    char response[1024]; // response 변수 선언
    sprintf(message, "REMAINING_TIME %d", locker_num);
    write(sock, message, strlen(message));
    handle_server_response(sock, response, sizeof(response));
    printf("%s\n", response);
}

// 학생이 사용하는 사물함 목록을 가져옴
void get_student_lockers(int sock, int student_id) {
    char message[100];
    sprintf(message, "GET_USER_LOCKERS %d", student_id); 
    write(sock, message, strlen(message));
    char response[1024];
    handle_server_response(sock, response, sizeof(response));
    printf("%s\n", response);
}

// 사용자 사물함 목록을 확인
int check_user_lockers(int sock, int student_id, int *locker_nums) {
    char message[100];
    sprintf(message, "GET_USER_LOCKERS %d", student_id);
    write(sock, message, strlen(message));
    char response[1024];
    handle_server_response(sock, response, sizeof(response));
    printf("%s\n", response);

    char *ptr = response;
    int count = 0;
    while ((ptr = strstr(ptr, "사물함 번호: ")) != NULL) {
        ptr += strlen("사물함 번호: ");
        locker_nums[count++] = atoi(ptr);
    }
    return count;
}

// 메뉴바 출력
void print_kiosk_header(const char* title) {
    printf(LINE);
    printf("%s\n", title);
    printf(LINE);
}

// 시그널 핸들러
void alarm_handler(int signum) {
    if (signum == SIGALRM) {
        if (!warning_issued_10s) {
            printf("\n<<자동 종료 시스템>> 종료까지 20초 남았습니다. \n 입력 : ");
            fflush(stdout);
            warning_issued_10s = 1;
            set_timer(10); // 10초 타이머 재설정
        } else if (!warning_issued_20s) {
            printf("\n<<자동 종료 시스템>> 종료까지 10초 남았습니다.\n입력 : ");
            fflush(stdout);
            warning_issued_20s = 1;
            set_timer(10); // 10초 타이머 재설정
        } else {
            printf("\n<<자동 종료 시스템>> 종료되었습니다.\n");
            fflush(stdout);
            input_timeout = 1;
            exit(0);
        }
    }
}

// 타이머 설정 
void set_timer(int seconds) {
    struct itimerval timer;
    timer.it_value.tv_sec = seconds;
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;
    setitimer(ITIMER_REAL, &timer, NULL);
}

// 타이머 리셋
void reset_timer() {
    set_timer(0); // 타이머 해제
    input_timeout = 0;
    warning_issued_10s = 0;
    warning_issued_20s = 0;
}

// 화면 지우기 기능 
void clear_screen() {
    printf("\033[H\033[J");
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    struct student user;

    signal(SIGALRM, alarm_handler); // 시그널 핸들러 설정

    clear_screen();
    print_kiosk_header("사물함 시스템");

    printf("학번을 입력하세요: ");
    reset_timer();
    set_timer(10); 
    if (scanf("%d", &user.id) != 1 || input_timeout) {
        printf("잘못된 입력입니다!\n");
        close(sock);
        return 0;
    }
    reset_timer(); 
    getchar();

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("소켓 생성 실패\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("IP 주소 변환 실패\n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("서버 연결 실패\n");
        return -1;
    }

    char password_length_msg[100];
    sprintf(password_length_msg, "GET_PASSWORD_LENGTH");
    write(sock, password_length_msg, strlen(password_length_msg));
    char response[1024];
    handle_server_response(sock, response, sizeof(response));
    reset_timer(); 
    set_timer(10);
    if (sscanf(response, "%d", &password_length) != 1 || input_timeout) {
        printf("잘못된 입력입니다!\n");
        close(sock);
        return 0;
    }
    reset_timer(); 

    int locker_nums[3];
    int num_lockers = check_user_lockers(sock, user.id, locker_nums);

    if (num_lockers > 0) {
        int selected_locker = -1;
        clear_screen();
        while (selected_locker == -1) {
            print_kiosk_header("사물함 선택");
            get_student_lockers(sock, user.id);
            printf("사용할 사물함 번호를 입력해 주십시오. (0 - 새로운 사물함 사용): ");
            int choice;
            reset_timer(); 
            set_timer(10); 
            if (scanf("%d", &choice) != 1 || input_timeout) {
                printf("잘못된 입력입니다!\n");
                close(sock);
                return 0;
            }
            reset_timer(); 
            getchar();
            
            if (choice == 0) {
                if (num_lockers >= 3) {
                    clear_screen();
                    printf("최대로 사용 가능한 사물함 수를 초과하였습니다. 다시 사용할 사물함 번호를 입력해 주십시오.\n");
                } else {
                    selected_locker = 0;
                }
            } else if (choice < 1 || choice > MAX_LOCKERS) {  
                clear_screen();
                printf("유효하지 않은 사물함 번호입니다. 다시 입력해 주세요.\n");
            } else {
                for (int i = 0; i < num_lockers; i++) {
                    if (choice == locker_nums[i]) {
                        char access_message[100];
                        sprintf(access_message, "ACCESS %d %d", choice, user.id);
                        write(sock, access_message, strlen(access_message));
                        char access_response[1024];
                        handle_server_response(sock, access_response, sizeof(access_response));
                        if (strstr(access_response, "IN_USE")) {
                            clear_screen();
                            printf("이 사물함은 이미 사용 중입니다. 다른 사물함을 선택해 주십시오.\n");
                        } else {
                            clear_screen();
                            selected_locker = choice;
                            break;
                        }
                    }
                }
                if (selected_locker == -1) {
                    clear_screen();
                    printf("현재 사용 중인 사물함 번호가 아닙니다!\n");
                }
            }
        }

        if (selected_locker != 0) {
            int locker_num = selected_locker;
            if (!verify(sock, locker_num)) {
                return 0; 
            }
            while (1) {
                display_menu(sock, locker_num);
                int action;
                reset_timer(); 
                set_timer(10); 
                if (scanf("%d", &action) != 1 || input_timeout) {
                    printf("잘못된 입력입니다!\n");
                    close(sock);
                    return 0;
                }
                reset_timer(); 
                getchar();

                if (action == 1) {
                    clear_screen();
                    store_content(sock, locker_num);
                } else if (action == 2) {
                    clear_screen();
                    remove_content(sock, locker_num);
                } else if (action == 3) {
                    clear_screen();
                    if (verify(sock, locker_num)) {
                        ch_pwd(sock, locker_num, user.id);
                    } else return 0;
                } else if (action == 4) {
                    clear_screen();
                    return_locker(sock, locker_num, user.id);
                    break;
                } else if (action == 5) {
                    clear_screen();
                    extend_time(sock, locker_num);
                } else if (action == 6) {
                    clear_screen();
                    show_remaining_time(sock, locker_num);
                } else if (action == 7) {
                    clear_screen();
                    close(sock);
                    return 0;
                } else {
                    clear_screen();
                }
            }
        } else {
            int action, locker_num;
            while (1) {
                clear_screen();
                print_kiosk_header("메뉴");
                printf("1. 전체 사물함 정보 조회\n");
                printf("2. 종료\n");
                printf("선택: ");
                reset_timer(); 
                set_timer(10); 
                if (scanf("%d", &action) != 1 || input_timeout) {
                    printf("잘못된 입력입니다!\n");
                    close(sock);
                    return 0;
                }
                reset_timer(); 
                getchar();

                if (action == 1) {
                    get_info(sock);
                    printf("어느 사물함을 사용하시겠습니까? ");
                    reset_timer();
                    set_timer(10); 
                    if (scanf("%d", &locker_num) != 1 || input_timeout) {
                        printf("잘못된 입력입니다!\n");
                        close(sock);
                        return 0;
                    }
                    reset_timer(); 
                    getchar();
                    
                    if (locker_num < 1 || locker_num > MAX_LOCKERS) {  
                        clear_screen();
                        printf("유효하지 않은 사물함 번호입니다. 다시 입력해 주세요.\n");
                        continue;
                    }

                    char access_message[100];
                    sprintf(access_message, "ACCESS %d %d", locker_num, user.id);
                    write(sock, access_message, strlen(access_message));
                    char access_response[1024];
                    handle_server_response(sock, access_response, sizeof(access_response));
                    if (strstr(access_response, "IN_USE")) {
                        clear_screen();
                        printf("이 사물함은 이미 사용 중입니다. 다른 사물함을 선택해 주십시오.\n");
                        continue;
                    }
                    
                    // 사물함을 사용 상태로 설정
                    char message[100];
                    sprintf(message, "USE_LOCKER %d %d", locker_num, user.id);
                    write(sock, message, strlen(message));
                    handle_server_response(sock, response, sizeof(response));
                    
                    set_pwd(sock, locker_num, user.id);

                    while (1) {
                        display_menu(sock, locker_num);

                        reset_timer(); 
                        set_timer(10); 
                        if (scanf("%d", &action) != 1 || input_timeout) {
                            printf("잘못된 입력입니다!\n");
                            close(sock);
                            return 0;
                        }
                        reset_timer(); 
                        getchar();

                        if (action == 1) {
                            clear_screen();
                            store_content(sock, locker_num);
                        } else if (action == 2) {
                            clear_screen();
                            remove_content(sock, locker_num);
                        } else if (action == 3) {
                            if (verify(sock, locker_num)) {
                                clear_screen();
                                ch_pwd(sock, locker_num, user.id);
                            } else return 0;
                        } else if (action == 4) {
                            clear_screen();
                            return_locker(sock, locker_num, user.id);
                            break;
                        } else if (action == 5) {
                            clear_screen();
                            extend_time(sock, locker_num);
                        } else if (action == 6) {
                            clear_screen();
                            show_remaining_time(sock, locker_num);
                        } else if (action == 7) {
                            close(sock);
                            return 0;
                        } else {
                            clear_screen();
                        }
                    }
                } else if (action == 2) {
                    close(sock);
                    return 0;
                } else {
                    clear_screen();
                }
            }
        }
    } else {
        clear_screen();
        print_kiosk_header("새로운 사물함 사용");
        printf("새로운 사물함을 사용하시겠습니까? (Y/N): ");
        char choice;
        reset_timer(); 
        set_timer(10); 
        if (scanf("%c", &choice) != 1 || input_timeout) {
            printf("잘못된 입력입니다!\n");
            close(sock);
            return 0;
        }
        reset_timer(); 
        getchar();

        if (choice == 'N' || choice == 'n') {
            close(sock);
            return 0;
        }

        int action, locker_num;
        clear_screen();
        while (1) {
            print_kiosk_header("메뉴");
            printf("1. 전체 사물함 정보 조회\n");
            printf("2. 종료\n");
            printf("선택: ");
            reset_timer(); 
            set_timer(10); 
            if (scanf("%d", &action) != 1 || input_timeout) {
                printf("잘못된 입력입니다!\n");
                close(sock);
                return 0;
            }
            reset_timer(); 
            getchar();

            if (action == 1) {
                get_info(sock);

                printf("어느 사물함을 사용하시겠습니까? ");
                reset_timer(); 
                set_timer(10); 
                if (scanf("%d", &locker_num) != 1 || input_timeout) {
                    printf("잘못된 입력입니다!\n");
                    close(sock);
                    return 0;
                }
                reset_timer(); 
                getchar();

                if (locker_num < 1 || locker_num > MAX_LOCKERS) {  
                    clear_screen();
                    printf("유효하지 않은 사물함 번호입니다. 다시 입력해 주세요.\n");
                    continue;
                }

                char access_message[100];
                sprintf(access_message, "ACCESS %d %d", locker_num, user.id);
                write(sock, access_message, strlen(access_message));
                char access_response[1024];
                handle_server_response(sock, access_response, sizeof(access_response));
                if (strstr(access_response, "IN_USE")) {
                    clear_screen();
                    printf("이 사물함은 이미 사용 중입니다. 다른 사물함을 선택해 주십시오.\n");
                    continue;
                }
                
                // 사물함을 사용 상태로 설정
                char message[100];
                sprintf(message, "USE_LOCKER %d %d", locker_num, user.id);
                write(sock, message, strlen(message));
                handle_server_response(sock, response, sizeof(response));

                set_pwd(sock, locker_num, user.id);

                while (1) {
                    display_menu(sock, locker_num);

                    reset_timer(); 
                    set_timer(10); 
                    if (scanf("%d", &action) != 1 || input_timeout) {
                        printf("잘못된 입력입니다!\n");
                        close(sock);
                        return 0;
                    }
                    reset_timer();
                    getchar();

                    if (action == 1) {
                        clear_screen();
                        store_content(sock, locker_num);
                    } else if (action == 2) {
                        clear_screen();
                        remove_content(sock, locker_num);
                    } else if (action == 3) {
                        if (verify(sock, locker_num)) {
                            clear_screen();
                            ch_pwd(sock, locker_num, user.id);
                        } else return 0;
                    } else if (action == 4) {
                        clear_screen();
                        return_locker(sock, locker_num, user.id);
                        break;
                    } else if (action == 5) {
                        clear_screen();
                        extend_time(sock, locker_num);
                    } else if (action == 6) {
                        clear_screen();
                        show_remaining_time(sock, locker_num);
                    } else if (action == 7) {
                        close(sock);
                        return 0;
                    } else {
                        clear_screen();
                    }
                }
            } else if (action == 2) {
                close(sock);
                return 0;
            } else {
                clear_screen();
            }
        }
    }

    close(sock);
    return 0;
}

