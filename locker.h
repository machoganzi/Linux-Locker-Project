#define LOCKER_H

struct locker {
    int use;
    int connect;
    char pwd[13];
    int remaining_time;
    int item_count;
    char content[10][256];
    int lock_count;      // 비밀번호 틀린 횟수
    int extend_count;    // 시간 연장 횟수
    int is_locked;       // 잠금 상태를 나타내는 변수
    time_t lock_end_time; // 잠금 해제 시간을 나타내는 변수
};

