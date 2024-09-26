#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include "locker.h"
#include "student.h"

#define PORT 8080
#define MAX_CLIENTS 10
#define MAX_CONTENT_LENGTH 256
#define MAX_ITEMS 10
#define ITEM_LENGTH 50
#define STUDENT_FILE "students.dat"
#define LOCKER_FILE "lockers.dat"
#define LOCKER_LOCK_TIME 10  // 비밀번호 3회 이상 틀릴 경우 잠김 시간 설정 - 10초

struct locker *lockers;
int num_lockers;
int password_length;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

// 사물함 데이터를 파일에 저장
void save_locker_data() {
    FILE *file = fopen(LOCKER_FILE, "wb");
    if (file == NULL) {
        perror("사물함 파일 저장 실패");
        return;
    }
    fwrite(lockers, sizeof(struct locker), num_lockers, file);
    fclose(file);
}

// 사물함 데이터를 파일에서 로드
void load_locker_data() {
    FILE *file = fopen(LOCKER_FILE, "rb");
    if (file == NULL) {
        perror("사물함 파일 읽기 실패");
        return;
    }
    fread(lockers, sizeof(struct locker), num_lockers, file);
    fclose(file);
}

// 사물함 초기화
void initialize_lockers(int n) {
    lockers = (struct locker *)malloc(n * sizeof(struct locker));
    for (int i = 0; i < n; i++) {
        lockers[i].use = 0;
        lockers[i].connect = 0;
        strcpy(lockers[i].pwd, "");
        lockers[i].remaining_time = 0;
        lockers[i].item_count = 0;
        lockers[i].lock_count = 0;
        lockers[i].extend_count = 0;
        lockers[i].is_locked = 0;
        lockers[i].lock_end_time = 0;
        for (int j = 0; j < MAX_ITEMS; j++) {
            strcpy(lockers[i].content[j], "");
        }
    }
    save_locker_data();
}

// 사물함 상태 출력
void get_locker_status(int locker_num, char *response) {
    response[0] = '\0'; // Clear the response buffer
    if (lockers[locker_num].item_count == 0) {
        strcat(response, "[비어있음]");
    } else {
        for (int i = 0; i < lockers[locker_num].item_count; i++) {
            strcat(response, "[");
            strcat(response, lockers[locker_num].content[i]);
            strcat(response, "] ");
        }
    }
}

// 학생 데이터를 파일에 저장
void save_student_data(struct student *students, int num_students) {
    FILE *file = fopen(STUDENT_FILE, "wb");
    if (file == NULL) {
        perror("파일 저장 실패");
        return;
    }
    fwrite(students, sizeof(struct student), num_students, file);
    fclose(file);
}

// 학생 데이터를 파일에서 로드
void load_student_data(struct student **students, int *num_students) {
    FILE *file = fopen(STUDENT_FILE, "rb");
    if (file == NULL) {
        *students = NULL;
        *num_students = 0;
        return;
    }
    fseek(file, 0, SEEK_END);
    int size = ftell(file);
    fseek(file, 0, SEEK_SET);
    *num_students = size / sizeof(struct student);
    *students = (struct student *)malloc(size);
    fread(*students, sizeof(struct student), *num_students, file);
    fclose(file);
}

struct student *students;
int num_students;

// 학생의 사물함 정보 업데이트
void update_student_lockers(int student_id, int locker_num, int add) {
    for (int i = 0; i < num_students; i++) {
        if (students[i].id == student_id) {
            if (add) {
                for (int j = 0; j < 3; j++) {
                    if (students[i].locker_num[j] == 0) {
                        students[i].locker_num[j] = locker_num;
                        save_student_data(students, num_students);
                        return;
                    }
                }
            } else {
                for (int j = 0; j < 3; j++) {
                    if (students[i].locker_num[j] == locker_num) {
                        students[i].locker_num[j] = 0;
                        save_student_data(students, num_students);
                        return;
                    }
                }
            }
        }
    }
    if (add) {
        num_students++;
        students = (struct student *)realloc(students, num_students * sizeof(struct student));
        students[num_students - 1].id = student_id;
        students[num_students - 1].locker_num[0] = locker_num;
        students[num_students - 1].locker_num[1] = 0;
        students[num_students - 1].locker_num[2] = 0;
        save_student_data(students, num_students);
    }
}

// 학생이 사용하는 사물함 정보를 로드
void get_student_lockers(int student_id, char *response) {
    response[0] = '\0'; 
    for (int i = 0; i < num_students; i++) {
        if (students[i].id == student_id) {
            sprintf(response, "사용자 %d의 사용 중인 사물함:\n", student_id);
            for (int j = 0; j < 3; j++) {
                if (students[i].locker_num[j] != 0) {
                    char locker_info[100];
                    int locker_num = students[i].locker_num[j] - 1;
                    sprintf(locker_info, "- 사물함 번호: %d ", locker_num + 1);
                    strcat(response, locker_info);
                    if (lockers[locker_num].is_locked) {
                        strcat(response, "[잠김 상태]\n");
                    } else {
                        strcat(response, "[사용 가능]\n");
                    }
                }
            }
            return;
        }
    }
    sprintf(response, "사용 중인 사물함이 없습니다.");
}

// 사물함 잠금 타이머 처리
void *timeout_handler(void *arg) {
    int locker_num = *(int *)arg;
    free(arg);

    sleep(LOCKER_LOCK_TIME);

    pthread_mutex_lock(&lock);
    lockers[locker_num].is_locked = 0;
    pthread_mutex_unlock(&lock);

    pthread_exit(NULL);
}

// 사물함 사용 시간을 관리
void *manage_locker_times(void *arg) {
    while (1) {
        sleep(1);
        pthread_mutex_lock(&lock);
        for (int i = 0; i < num_lockers; i++) {
            if (lockers[i].use && lockers[i].remaining_time > 0) {
                lockers[i].remaining_time--;
                if (lockers[i].remaining_time == 0) {
                    lockers[i].use = 0;
                    lockers[i].connect = 0;
                    strcpy(lockers[i].pwd, "");
                    lockers[i].item_count = 0;
                    lockers[i].lock_count = 0;
                    lockers[i].extend_count = 0;
                    lockers[i].is_locked = 0;
                    lockers[i].lock_end_time = 0;
                    for (int j = 0; j < MAX_ITEMS; j++) {
                        strcpy(lockers[i].content[j], "");
                    }
                    // 반납된 사물함을 학생 데이터에서 삭제
                    for (int j = 0; j < num_students; j++) {
                        for (int k = 0; k < 3; k++) {
                            if (students[j].locker_num[k] == i + 1) {
                                students[j].locker_num[k] = 0;
                                save_student_data(students, num_students);
                                break;
                            }
                        }
                    }
                }
            }
        }
        save_locker_data();
        pthread_mutex_unlock(&lock);
    }
    pthread_exit(NULL);
}

// 클라이언트 요청을 처리
void *client_handler(void *client_socket) {
    int sock = *(int *)client_socket;
    char buffer[1024];
    char response[1024];
    int n;
    char command[50], param1[50], param2[50], param3[256];
    int locker_num, student_id;

    while ((n = read(sock, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[n] = '\0';
        sscanf(buffer, "%s %s %s %[^\n]", command, param1, param2, param3);

        if (strcmp(command, "GET_INFO") == 0) {
            pthread_mutex_lock(&lock);
            load_locker_data();  // 파일에서 최신 사물함 데이터를 읽어옴
            strcpy(response, "");
            for (int i = 0; i < num_lockers; i++) {
                char locker_info[10];
                if (lockers[i].use == 0) {
                    sprintf(locker_info, "[ %d ] ", i + 1);
                } else {
                    sprintf(locker_info, "[ X ] ");
                }
                strcat(response, locker_info);
                if ((i + 1) % 5 == 0) {
                    strcat(response, "\n");
                }
            }
            pthread_mutex_unlock(&lock);
            write(sock, response, strlen(response));
        } 
        else if (strcmp(command, "SET_PWD") == 0) {
            locker_num = atoi(param1) - 1;
            student_id = atoi(param2);
            pthread_mutex_lock(&lock);
            if (lockers[locker_num].use == 1 && lockers[locker_num].connect != student_id) {
                sprintf(response, "사물함 %d는 이미 사용 중입니다.", locker_num + 1);
            } else {
                lockers[locker_num].use = 1;
                lockers[locker_num].connect = student_id;
                if (strlen(param3) != password_length) {
                    sprintf(response, "<<비밀번호는 %d자리 숫자로 입력해주세요.>>", password_length);
                } else {
                    strcpy(lockers[locker_num].pwd, param3);
                    lockers[locker_num].remaining_time = 180;  // 사물함 사용 시간 설정 - 180초(3분)
                    lockers[locker_num].extend_count = 0;      // 연장 횟수 초기화
                    sprintf(response, "<<사물함 %d의 비밀번호가 설정되었습니다.>>\n 현재 비밀번호 : %s", locker_num + 1, lockers[locker_num].pwd);
                    update_student_lockers(student_id, locker_num + 1, 1);
                    save_locker_data();  // 변경 사항을 파일에 저장
                }
            }
            pthread_mutex_unlock(&lock);
            write(sock, response, strlen(response));
        }
        else if (strcmp(command, "CH_PWD") == 0) {
            locker_num = atoi(param1) - 1;
            student_id = atoi(param2);
            pthread_mutex_lock(&lock);
            if (lockers[locker_num].use == 1 && lockers[locker_num].connect != student_id) {
                sprintf(response, "사물함 %d는 이미 사용 중입니다.", locker_num + 1);
            } else {
                lockers[locker_num].use = 1;
                lockers[locker_num].connect = student_id;
                if (strlen(param3) != password_length) {
                    sprintf(response, "<<비밀번호는 %d자리 숫자로 입력해주세요.>>", password_length);
                } else {
                    strcpy(lockers[locker_num].pwd, param3);
                    sprintf(response, "<<사물함 %d의 비밀번호가 변경되었습니다.>>\n 현재 비밀번호 : %s", locker_num + 1, lockers[locker_num].pwd);
                    save_locker_data();  // 변경 사항을 파일에 저장
                }
            }
            pthread_mutex_unlock(&lock);
            write(sock, response, strlen(response));
        } 
        else if (strcmp(command, "VERIFY") == 0) {
    locker_num = atoi(param1) - 1;
    pthread_mutex_lock(&lock);
    if (lockers[locker_num].is_locked) {
        time_t now = time(NULL);
        int remaining_time = lockers[locker_num].lock_end_time - now;
        if (remaining_time <= 0) {
            lockers[locker_num].is_locked = 0;
            sprintf(response, "<<사물함 %d의 잠금이 해제되었습니다.\n>>", locker_num + 1);
        } else {
            sprintf(response, "<<사물함 %d는 잠겨 있습니다. 잠금 해제까지 %d초 남았습니다.>>", locker_num + 1, remaining_time);
        }
    } else if (strcmp(lockers[locker_num].pwd, param2) == 0) {
        lockers[locker_num].lock_count = 0;  // 비밀번호 틀린 횟수 초기화
        if (lockers[locker_num].item_count > 0) {
            response[0] = '\0';
            for (int i = 0; i < lockers[locker_num].item_count; i++) {
                strcat(response, lockers[locker_num].content[i]);
                strcat(response, " ");
            }
        } else {
            strcpy(response, "[비어있음]");
        }
    } else {
        lockers[locker_num].lock_count++;
        if (lockers[locker_num].lock_count >= 3) {
            lockers[locker_num].is_locked = 1;
            lockers[locker_num].lock_end_time = time(NULL) + LOCKER_LOCK_TIME; // 설정한 시간 뒤에 잠금 해제
            lockers[locker_num].lock_count = 0;  // 틀린 횟수 초기화
            sprintf(response, "<<비밀번호를 3회 이상 틀렸습니다. 사물함이 %d초 동안 잠깁니다.>>", LOCKER_LOCK_TIME);
            int *arg = malloc(sizeof(int));
            *arg = locker_num;
            pthread_t tid;
            pthread_create(&tid, NULL, timeout_handler, arg);
        } else {
            sprintf(response, "<<사물함 %d의 비밀번호가 틀렸습니다. (%d/3)>>", locker_num + 1, lockers[locker_num].lock_count);
        }
    }
    pthread_mutex_unlock(&lock);
    write(sock, response, strlen(response));
}

        else if (strcmp(command, "STORE") == 0) {
            locker_num = atoi(param1) - 1;
            pthread_mutex_lock(&lock);
            if (lockers[locker_num].use == 0) {
                sprintf(response, "<<사물함 %d는 비어 있습니다. 비밀번호를 먼저 설정해주세요.>>", locker_num + 1);
            } else {
                if (lockers[locker_num].item_count < MAX_ITEMS) {
                    strcpy(lockers[locker_num].content[lockers[locker_num].item_count], param2);
                    lockers[locker_num].item_count++;
                    sprintf(response, "<<사물함 %d에 내용이 저장되었습니다.>>", locker_num + 1);
                    save_locker_data();  // 변경 사항을 파일에 저장
                } else {
                    sprintf(response, "<<사물함 %d가 가득 찼습니다. 더 이상 저장할 수 없습니다.>>", locker_num + 1);
                }
            }
            pthread_mutex_unlock(&lock);
            write(sock, response, strlen(response));
        }
        else if (strcmp(command, "REMOVE") == 0) {
            locker_num = atoi(param1) - 1;
            pthread_mutex_lock(&lock);
            if (lockers[locker_num].use == 0) {
                sprintf(response, "사물함 %d는 비어 있습니다. 비밀번호를 먼저 설정해주세요.", locker_num + 1);
            } else {
                int found = 0;
                for (int i = 0; i < lockers[locker_num].item_count; i++) {
                    if (strcmp(lockers[locker_num].content[i], param2) == 0) {
                        found = 1;
                        for (int j = i; j < lockers[locker_num].item_count - 1; j++) {
                            strcpy(lockers[locker_num].content[j], lockers[locker_num].content[j + 1]);
                        }
                        lockers[locker_num].item_count--;
                        strcpy(lockers[locker_num].content[lockers[locker_num].item_count], "");
                        sprintf(response, "<<사물함 %d에서 %s를 제거했습니다.>>", locker_num + 1, param2);
                        save_locker_data();  // 변경 사항을 파일에 저장
                        break;
                    }
                }
                if (!found) {
                    sprintf(response, "<<사물함 %d에 %s가 없습니다.>>", locker_num + 1, param2);
                }
            }
            pthread_mutex_unlock(&lock);
            write(sock, response, strlen(response));
        }
        else if (strcmp(command, "RETURN") == 0) {
            locker_num = atoi(param1) - 1;
            student_id = atoi(param2);
            pthread_mutex_lock(&lock);
            lockers[locker_num].use = 0;
            lockers[locker_num].connect = 0;
            strcpy(lockers[locker_num].pwd, "");
            lockers[locker_num].remaining_time = 0;
            lockers[locker_num].item_count = 0;
            lockers[locker_num].lock_count = 0;
            lockers[locker_num].extend_count = 0;
            lockers[locker_num].is_locked = 0;
            lockers[locker_num].lock_end_time = 0;
            for (int i = 0; i < MAX_ITEMS; i++) {
                strcpy(lockers[locker_num].content[i], "");
            }
            update_student_lockers(student_id, locker_num + 1, 0); // 사물함 반납 시 학생 데이터에서도 제거하는 기능
            sprintf(response, "<<사물함 %d가 반납되었습니다.>>\n", locker_num + 1);
            save_locker_data();  // 변경 사항을 파일에 저장
            pthread_mutex_unlock(&lock);
            write(sock, response, strlen(response));
        }
        else if (strcmp(command, "GET_USER_LOCKERS") == 0) {
            student_id = atoi(param1);
            pthread_mutex_lock(&lock);
            get_student_lockers(student_id, response);
            pthread_mutex_unlock(&lock);
            write(sock, response, strlen(response));
        }
        else if (strcmp(command, "STATUS") == 0) {
            locker_num = atoi(param1) - 1;
            pthread_mutex_lock(&lock);
            get_locker_status(locker_num, response);
            pthread_mutex_unlock(&lock);
            write(sock, response, strlen(response));
        }
        else if (strcmp(command, "ACCESS") == 0) {
            locker_num = atoi(param1) - 1;
            student_id = atoi(param2);
            pthread_mutex_lock(&lock);
            if (lockers[locker_num].use == 1 && lockers[locker_num].connect != student_id) {
                sprintf(response, "IN_USE");
            } else {
                lockers[locker_num].use = 1;
                lockers[locker_num].connect = student_id;
                sprintf(response, "LOCKER_ACCESSED");
                save_locker_data();  // 변경 사항을 파일에 저장
            }
            pthread_mutex_unlock(&lock);
            write(sock, response, strlen(response));
        }
        else if (strcmp(command, "EXTEND") == 0) {
            locker_num = atoi(param1) - 1;
            pthread_mutex_lock(&lock);
            if (lockers[locker_num].extend_count < 3) {
                lockers[locker_num].remaining_time += 60; // 시간 연장기능에서 연장되는 시간 설정 - 60초(1분)
                lockers[locker_num].extend_count++;
                sprintf(response, "<<사물함 %d의 시간 연장이 완료되었습니다. (%d/3)>>", locker_num + 1, lockers[locker_num].extend_count);
            } else {
                sprintf(response, "<<사물함 %d는 최대 연장 횟수를 초과했습니다.>>", locker_num + 1);
            }
            pthread_mutex_unlock(&lock);
            write(sock, response, strlen(response));
        }
        else if (strcmp(command, "REMAINING_TIME") == 0) {
            locker_num = atoi(param1) - 1;
            pthread_mutex_lock(&lock);
            sprintf(response, "<<%d번 사물함의 남은 시간: %d초>>", locker_num + 1, lockers[locker_num].remaining_time);
            pthread_mutex_unlock(&lock);
            write(sock, response, strlen(response));
        }
        else if (strcmp(command, "GET_PASSWORD_LENGTH") == 0) {
            sprintf(response, "%d", password_length);
            write(sock, response, strlen(response));
        }
        else if (strcmp(command, "USE_LOCKER") == 0) { 
            locker_num = atoi(param1) - 1;
            student_id = atoi(param2);
            pthread_mutex_lock(&lock);
            if (lockers[locker_num].use == 1 && lockers[locker_num].connect != student_id) {
                sprintf(response, "사물함 %d는 이미 사용 중입니다.", locker_num + 1);
            } else {
                lockers[locker_num].use = 1;
                lockers[locker_num].connect = student_id;
                lockers[locker_num].remaining_time = 180;  // 3분 설정
                lockers[locker_num].extend_count = 0;      // 연장 횟수 초기화
                save_locker_data();  // 변경 사항을 파일에 저장
            }
            pthread_mutex_unlock(&lock);
            write(sock, response, strlen(response));
        }
        else {
            sprintf(response, "알 수 없는 명령입니다.");
            write(sock, response, strlen(response));
        }
    }
    
    close(sock);
    free(client_socket);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "사용 방법: %s <사물함 개수> <비밀번호 길이>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    num_lockers = atoi(argv[1]);
    password_length = atoi(argv[2]);

    if (num_lockers <= 0) {
        fprintf(stderr, "사물함 개수는 양수여야 합니다.\n");
        exit(EXIT_FAILURE);
    }

    if (password_length < 4 || password_length > 12) {
        fprintf(stderr, "비밀번호 길이는 4 이상 12 이하의 숫자여야 합니다.\n");
        exit(EXIT_FAILURE);
    }

    load_student_data(&students, &num_students);

    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    pthread_t tid;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("소켓 생성 실패");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("바인드 실패");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("리슨 실패");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("서버가 포트 %d에서 수신 대기 중.\n", PORT);

    initialize_lockers(num_lockers);

    pthread_create(&tid, NULL, manage_locker_times, NULL); 

    while ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) >= 0) {
        int *client_sock = malloc(sizeof(int));
        *client_sock = new_socket;
        printf("새로운 사용자 접근");
        if (pthread_create(&tid, NULL, client_handler, (void *)client_sock) != 0) {
            perror("pthread_create");
            close(new_socket);
            free(client_sock);
        }
    }

    if (new_socket < 0) {
        perror("accept");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    close(server_fd);
    free(lockers);
    free(students);
    return 0;
}

