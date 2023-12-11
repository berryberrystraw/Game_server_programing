#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <setjmp.h>

#define SHARED_MEMORY_KEY 5522
#define MEMORY_SIZE 200

int signal_opponent = 1;
int mypid, enemypid;

int turn = 0;
char map[10][10]; //맵 

typedef struct { //공유메모리에 저장할 구조체 
	int act[30];
	int actCount;
	int shmid;
	int* Player;
}actInfo;

typedef struct {
	int score;
	int bp;
	int money;
	int posi; //현재 위치 
	int posj;
}player;

player p1, p2; //p2가 상대

void init_map(){
	for(int i=0; i<10; i++){
		for(int j=0; j<10; j++){
			map[i][j] = ' ';
		}
	}
}

//시그널 받기 함수
void receive_signal()
{
	signal_opponent = 0;
	return;
}

//처음에 호스트랑 클라이언트 매치메이킹 시 실행되는 코드
int matchmaking() {
	struct stat sbuf;
	FILE* fp;
	long iter;
	int fd;
	mypid = getpid();
	char *buffer;


	printf("연결을 시도합니다.\n");

	fp = fopen("host.txt", "r");
	if (fp == NULL)
	{
		printf("호스트 세션 시작\n");

		fp = fopen("host.txt", "w");
		if (fp == NULL)
		{
			printf("host.txt 파일 열기 오류 오픈 실패 \n");
			exit(0);
		}
		fprintf(fp, "%d\n", mypid);
		fclose(fp);

		printf("클라이언트와 연결 중...\n");
		signal(SIGUSR1, receive_signal);

		while (signal_opponent)
		{
			sleep(1);
		}
		signal_opponent = 1;

		fd = open("client.txt", O_RDONLY);
		if(fd == -1){
			perror("open");
			exit(1);
		}
		if(fstat(fd, &sbuf) == -1){
			perror("fstat");
			exit(1);
		}
		
		buffer = mmap(0, sbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
		if(buffer == MAP_FAILED){
			perror("mmap");
			exit(1);
		}
		
		enemypid = 0;
		for (iter = 0; buffer[iter] != '\n'; iter++)
		{
			enemypid *= 10;
			enemypid += (buffer[iter] - '0');
		}
		close(fd);
		munmap(buffer, sbuf.st_size);
		
		printf("클라이언트와 연결 성공\n");
		
		//printf("클라이언트 측 \nenemypid : %d\n", enemypid);
		//printf("mypid : %d\n", mypid);
		return 1;
	}
	else
	{
		printf("클라이언트 세션 시작\n");
		fd = open("host.txt", O_RDONLY);
		if(fd == -1){
			perror("open");
			exit(1);
		}
		if(fstat(fd, &sbuf) == -1){
			perror("fstat");
			exit(1);
		}
		
		buffer = mmap(0, sbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
		if(buffer == MAP_FAILED){
			perror("mmap");
			exit(1);
		}
		
		enemypid = 0;
		for (iter = 0; buffer[iter] != '\n'; iter++)
		{
			enemypid *= 10;
			enemypid += (buffer[iter] - '0');
		}
		close(fd);
		munmap(buffer, sbuf.st_size);
		
		//printf("클라이언트 측 \nenemypid : %d\n", enemypid);
		//printf("mypid : %d\n", mypid);
		
		fp = fopen("client.txt", "w");
		if (fp == NULL)
		{
			printf("파일 열기 오류 : client.txt 오픈 실패\n");
			exit(0);
		}
		fprintf(fp, "%d\n", mypid);
		fclose(fp);
		kill(enemypid, 10);
		printf("호스트와 연결 성공\n");
		signal(SIGUSR1, receive_signal);
		return 2;
	}
	return 0;
}

//시그널을 보내고 시그널이 올 때까지 대기
void send_signal()
{
	kill(enemypid, 10);
	//시그널 종료
	if(turn > 15)
	{
		signal_opponent = 0;
	}
	while (signal_opponent)
	{
		sleep(1);
	}
	signal_opponent = 1;
	return;
}

//게임 종료할때 호출
void exit_game()
{
	system("rm -rf host.txt");
	system("rm -rf client.txt");
}

void display_clear(){
	system("clear");
}

void print_map() //맵 출력 
{
	for (int i = 0; i < 10; i++)
	{
		for (int j = 0; j < 10; j++)
		{
			printf("%c", map[i][j]);
		}
		printf("\n");
	}
}

void print_playerPos() //플레이어 위치 출력 
{
	for (int i = 0; i < 10; i++)
	{
		for (int j = 0; j < 10; j++)
		{
			if (i == p1.posi && j == p1.posj)
				printf("i");
			else if (i == p2.posi && j == p2.posj)
				printf("o");
			else
				printf("%c", map[i][j]);
		}
		printf("\n");
	}
}

void p2_act(actInfo* info) //상대 행동 처리, 자신의 행동은 입력과 동시에 처리 
{
	for (int i = 0; i < info->actCount; i++)
	{
		switch (info->Player[i])
		{
			//상하좌우 반전해서 처리 
		case 1: //하 
			p2.posi++;
			if (!(p2.posi == p1.posi && p2.posj == p1.posj)) //자신이 있는 곳이 아니고 블럭이 없는 곳이라면 자기 땅으로 
				if (!(map[p2.posi][p2.posj] == 'm' || map[p2.posi][p2.posj] == 'b' || map[p2.posi][p2.posj] == 'c' || map[p2.posi][p2.posj] == 's' || map[p2.posi][p2.posj] == 'a'
					|| map[p2.posi][p2.posj] == 'M' || map[p2.posi][p2.posj] == 'B' || map[p2.posi][p2.posj] == 'C' || map[p2.posi][p2.posj] == 'S' || map[p2.posi][p2.posj] == 'A'))
					map[p2.posi][p2.posj] = '#';

			break;
		case 2: //상 
			p2.posi--;
			if (!(p2.posi == p1.posi && p2.posj == p1.posj))
				if (!(map[p2.posi][p2.posj] == 'm' || map[p2.posi][p2.posj] == 'b' || map[p2.posi][p2.posj] == 'c' || map[p2.posi][p2.posj] == 's' || map[p2.posi][p2.posj] == 'a'
					|| map[p2.posi][p2.posj] == 'M' || map[p2.posi][p2.posj] == 'B' || map[p2.posi][p2.posj] == 'C' || map[p2.posi][p2.posj] == 'S' || map[p2.posi][p2.posj] == 'A'))
					map[p2.posi][p2.posj] = '#';
			break;
		case 3: //우
			p2.posj++;
			if (!(p2.posi == p1.posi && p2.posj == p1.posj))
				if (!(map[p2.posi][p2.posj] == 'm' || map[p2.posi][p2.posj] == 'b' || map[p2.posi][p2.posj] == 'c' || map[p2.posi][p2.posj] == 's' || map[p2.posi][p2.posj] == 'a'
					|| map[p2.posi][p2.posj] == 'M' || map[p2.posi][p2.posj] == 'B' || map[p2.posi][p2.posj] == 'C' || map[p2.posi][p2.posj] == 'S' || map[p2.posi][p2.posj] == 'A'))
					map[p2.posi][p2.posj] = '#';
			break;
		case 4: //좌
			p2.posj--;
			if (!(p2.posi == p1.posi && p2.posj == p1.posj))
				if (!(map[p2.posi][p2.posj] == 'm' || map[p2.posi][p2.posj] == 'b' || map[p2.posi][p2.posj] == 'c' || map[p2.posi][p2.posj] == 's' || map[p2.posi][p2.posj] == 'a'
					|| map[p2.posi][p2.posj] == 'M' || map[p2.posi][p2.posj] == 'B' || map[p2.posi][p2.posj] == 'C' || map[p2.posi][p2.posj] == 'S' || map[p2.posi][p2.posj] == 'A'))
					map[p2.posi][p2.posj] = '#';
			break;
		case 5: //머니블럭 설치
			p2.money -= 3;
			map[p2.posi][p2.posj] = 'm';
			break;
		case 6: //행동블럭 설치 
			p2.money -= 4;
			map[p2.posi][p2.posj] = 'b';
			break;
		case 7: //통제블럭 설치
			p2.money -= 2;
			map[p2.posi][p2.posj] = 'c';
			break;
		case 8: //스코어블럭 설치
			p2.money -= 2;
			map[p2.posi][p2.posj] = 's';
			break;
		case 9: //공격블럭 설치
			p2.money -= 3;
			map[p2.posi][p2.posj] = 'a';
			break;

		}
	}
}

void scoreBlock(int pnum, int i, int j) //스코어블럭 처리, pnum이 1이면 자신 2면 상대 
{
	int left = j - 1;
	int right = j + 1;
	int up = i - 1;
	int down = i + 1;

	int count = 1;

	if (pnum == 1) //자신 
	{
		if (left >= 0)
		{
			if (map[i][left] == '@' || map[i][left] == 'M' || map[i][left] == 'B' || map[i][left] == 'C' || map[i][left] == 'S' || map[i][left] == 'A')
				count++;
		}

		if (right <= 9)
		{
			if (map[i][right] == '@' || map[i][right] == 'M' || map[i][right] == 'B' || map[i][right] == 'C' || map[i][right] == 'S' || map[i][right] == 'A')
				count++;
		}

		if (up >= 0)
		{
			if (map[up][j] == '@' || map[up][j] == 'M' || map[up][j] == 'B' || map[up][j] == 'C' || map[up][j] == 'S' || map[up][j] == 'A')
				count++;
		}

		if (down <= 9)
		{
			if (map[down][j] == '@' || map[down][j] == 'M' || map[down][j] == 'B' || map[down][j] == 'C' || map[down][j] == 'S' || map[down][j] == 'A')
				count++;
		}
		p1.score += count;
	}

	if (pnum == 2) //상대 
	{
		if (left >= 0)
		{
			if (map[i][left] == '#' || map[i][left] == 'm' || map[i][left] == 'b' || map[i][left] == 'c' || map[i][left] == 's' || map[i][left] == 'a')
				count++;
		}

		if (right <= 9)
		{
			if (map[i][right] == '#' || map[i][right] == 'm' || map[i][right] == 'b' || map[i][right] == 'c' || map[i][right] == 's' || map[i][right] == 'a')
				count++;
		}

		if (up >= 0)
		{
			if (map[up][j] == '#' || map[up][j] == 'm' || map[up][j] == 'b' || map[up][j] == 'c' || map[up][j] == 's' || map[up][j] == 'a')
				count++;
		}

		if (down <= 9)
		{
			if (map[down][j] == '#' || map[down][j] == 'm' || map[down][j] == 'b' || map[down][j] == 'c' || map[down][j] == 's' || map[down][j] == 'a')
				count++;
		}
		p2.score += count;
	}

}

void attackBlock(int pnum, int i, int j) //공격블럭 처리, pnum이 1이면 자신 2면 상대
{
	int left = j - 1;
	int right = j + 1;
	int up = i - 1;
	int down = i + 1;

	int count = 0;

	if (pnum == 1) //자신 
	{
		if (left >= 0)
		{
			if (map[i][left] == '#' || map[i][left] == 'm' || map[i][left] == 'b' || map[i][left] == 'c' || map[i][left] == 's' || map[i][left] == 'a')
				count++;
		}

		if (right <= 9)
		{
			if (map[i][right] == '#' || map[i][right] == 'm' || map[i][right] == 'b' || map[i][right] == 'c' || map[i][right] == 's' || map[i][right] == 'a')
				count++;
		}

		if (up >= 0)
		{
			if (map[up][j] == '#' || map[up][j] == 'm' || map[up][j] == 'b' || map[up][j] == 'c' || map[up][j] == 's' || map[up][j] == 'a')
				count++;
		}

		if (down <= 9)
		{
			if (map[down][j] == '#' || map[down][j] == 'm' || map[down][j] == 'b' || map[down][j] == 'c' || map[down][j] == 's' || map[down][j] == 'a')
				count++;
		}
		p2.score -= count;
	}

	if (pnum == 2) //상대 
	{
		if (left >= 0)
		{
			if (map[i][left] == '@' || map[i][left] == 'M' || map[i][left] == 'B' || map[i][left] == 'C' || map[i][left] == 'S' || map[i][left] == 'A')
				count++;
		}

		if (right <= 9)
		{
			if (map[i][right] == '@' || map[i][right] == 'M' || map[i][right] == 'B' || map[i][right] == 'C' || map[i][right] == 'S' || map[i][right] == 'A')
				count++;
		}

		if (up >= 0)
		{
			if (map[up][j] == '@' || map[up][j] == 'M' || map[up][j] == 'B' || map[up][j] == 'C' || map[up][j] == 'S' || map[up][j] == 'A')
				count++;
		}

		if (down <= 9)
		{
			if (map[down][j] == '@' || map[down][j] == 'M' || map[down][j] == 'B' || map[down][j] == 'C' || map[down][j] == 'S' || map[down][j] == 'A')
				count++;
		}
		p1.score -= count;
	}

}


void update() //한 턴이 끝날 때마다 호출 
{
	for (int i = 0; i < 10; i++)
		for (int j = 0; j < 10; j++)
		{
			switch (map[i][j])
			{
			case 'M': //자신  
				p1.money++;
				break;
			case 'B':
				p1.bp++;
				break;
			case 'S':
				scoreBlock(1, i, j);
				break;
			case 'A':
				attackBlock(1, i, j);
				break;
			case 'm': //상대 
				p2.money++;
				break;
			case 'b':
				p2.bp++;
				break;
			case 's':
				scoreBlock(2, i, j);
				break;
			case 'a':
				attackBlock(2, i, j);
				break;

			}
		}
}

void initMemory(actInfo* info, int n)
{
	if(n == 1){
		info->shmid = shmget((key_t)SHARED_MEMORY_KEY, (size_t)MEMORY_SIZE, 0666 | IPC_CREAT);
		if (info->shmid == -1) {
			printf("shmget failed\n");
			exit(0);
		}
	}

	if(n == 2){
		info->shmid = shmget((key_t)SHARED_MEMORY_KEY, (size_t)MEMORY_SIZE, 0);
		if (info->shmid == -1) {
			printf("shmget failed\n");
			exit(0);
		}
	}

	// 공유 메모리에 대한 접근 설정
	info->Player = (int*)shmat(info->shmid, NULL, 0);
	if (info->Player == (int*)-1) {
		printf("shmat failed\n");
		exit(0);
	}

	//메모리 초기화
	memset(info->Player, 0, MEMORY_SIZE);
}

void print(actInfo* info) 
{
	printf("\n");
	info->actCount = 6;
	for (int i = 0; i < info->actCount; i++) { //공유메모리에 저장된 행동정보를 출력
		printf("act %d : %d", i, info->Player[i]);

		if (info->Player[i] == 0)
		{
			printf("-> 아무행동을 하지 않았습니다.\n");
		}

		if (info->Player[i] == 1)
		{
			printf("-> 위로 이동하였습니다.\n");
		}

		if (info->Player[i] == 2)
		{
			printf("-> 아래로 이동하였습니다.\n");
		}

		if (info->Player[i] == 3)
		{
			printf("-> 왼쪽으로 이동하였습니다.\n");
		}

		if (info->Player[i] == 4)
		{
			printf("-> 오른쪽으로 이동하였습니다.\n");
		}

		if (info->Player[i] == 5)
		{
			printf("-> 머니블록을 설치했습니다.\n");
		}

		if (info->Player[i] == 6)
		{
			printf("-> 행동블록을 설치했습니다.\n");
		}

		if (info->Player[i] == 7)
		{
			printf("-> 통제블록을 설치했습니다.\n");
		}

		if (info->Player[i] == 8)
		{
			printf("-> 스코어블록을 설치했습니다.\n");
		}

		if (info->Player[i] == 9)
		{
			printf("-> 공격블록을 설치했습니다.\n");
		}
	}
	p2_act(info); //맵 업데이트

	for (int i = 0; i < info->actCount; i++) { //공유메모리에서 플레이어의 행동정보를 초기화
		info->Player[i] = 0;
	}
}

void SaveInfo(actInfo* info) {

	//공유 메모리에 행동정보 저장
	for (int i = 0; i < info->actCount; i++) {
		info->Player[i] = info->act[i];
	}

	//자신의 행동 정보 출력
	for (int i = 0; i < info->actCount; i++) {
		printf("Player act = ");
		printf("%d\n", info->Player[i]);
	}
}

void actInput(actInfo* info)
{
	int in;
	print_map();
	while (p1.bp > 0)
	{
		in = 0;
		printf("\n");
		printf("무엇을 하시겠습니까?\n점수 : %d, 돈 : %d, 행동포인트 : %d\n상대 점수 : %d\n", p1.score, p1.money, p1.bp, p2.score);
		printf("1. 이동 2. 블럭설치 3. 맵 출력 4. 플레이어 위치 출력: ");
		scanf("%d", &in);
		getchar();

		if (in == 1)
		{
			in = 0;
			printf("1. 상 2. 하 3. 좌 4. 우 (back : 그 외의 키) : ");
			scanf("%d", &in);
			getchar();
			if (in == 1)
			{
				if (p1.posi <= 0)
				{
					printf("이동할 수 없습니다\n");
					continue;
				}
				if (map[p1.posi - 1][p1.posj] == 'c')
				{
					printf("이동할 수 없습니다\n");
					continue;
				}

				p1.posi -= 1;
				p1.bp -= 1;
				if (!(p2.posi == p1.posi && p2.posj == p1.posj)) //자신이 있는 곳이 아니고 블럭이 없는 곳이라면 자기 땅으로 
					if (!(map[p1.posi][p1.posj] == 'm' || map[p1.posi][p1.posj] == 'b' || map[p1.posi][p1.posj] == 'c' || map[p1.posi][p1.posj] == 's' || map[p1.posi][p1.posj] == 'a'
						|| map[p1.posi][p1.posj] == 'M' || map[p1.posi][p1.posj] == 'B' || map[p1.posi][p1.posj] == 'C' || map[p1.posi][p1.posj] == 'S' || map[p1.posi][p1.posj] == 'A'))
						map[p1.posi][p1.posj] = '@';
				info->act[info->actCount] = 1;
				info->actCount++;
				display_clear();
				print_playerPos();
			}
			else if (in == 2)
			{
				if (p1.posi >= 9)
				{
					printf("이동할 수 없습니다\n");
					continue;
				}
				if (map[p1.posi + 1][p1.posj] == 'c')
				{
					printf("이동할 수 없습니다\n");
					continue;
				}
				p1.posi += 1;
				p1.bp -= 1;
				if (!(p2.posi == p1.posi && p2.posj == p1.posj)) //자신이 있는 곳이 아니고 블럭이 없는 곳이라면 자기 땅으로 
					if (!(map[p1.posi][p1.posj] == 'm' || map[p1.posi][p1.posj] == 'b' || map[p1.posi][p1.posj] == 'c' || map[p1.posi][p1.posj] == 's' || map[p1.posi][p1.posj] == 'a'
						|| map[p1.posi][p1.posj] == 'M' || map[p1.posi][p1.posj] == 'B' || map[p1.posi][p1.posj] == 'C' || map[p1.posi][p1.posj] == 'S' || map[p1.posi][p1.posj] == 'A'))
						map[p1.posi][p1.posj] = '@';
				info->act[info->actCount] = 2;
				info->actCount++;
				display_clear();
				print_playerPos();
			}
			else if (in == 3)
			{
				if (p1.posj <= 0)
				{
					printf("이동할 수 없습니다\n");
					continue;
				}
				if (map[p1.posi][p1.posj - 1] == 'c')
				{
					printf("이동할 수 없습니다\n");
					continue;
				}
				p1.posj -= 1;
				p1.bp -= 1;
				if (!(p2.posi == p1.posi && p2.posj == p1.posj)) //자신이 있는 곳이 아니고 블럭이 없는 곳이라면 자기 땅으로 
					if (!(map[p1.posi][p1.posj] == 'm' || map[p1.posi][p1.posj] == 'b' || map[p1.posi][p1.posj] == 'c' || map[p1.posi][p1.posj] == 's' || map[p1.posi][p1.posj] == 'a'
						|| map[p1.posi][p1.posj] == 'M' || map[p1.posi][p1.posj] == 'B' || map[p1.posi][p1.posj] == 'C' || map[p1.posi][p1.posj] == 'S' || map[p1.posi][p1.posj] == 'A'))
						map[p1.posi][p1.posj] = '@';
				info->act[info->actCount] = 3;
				info->actCount++;
				display_clear();
				print_playerPos();
			}
			else if (in == 4)
			{
				if (p1.posj >= 9)
				{
					printf("이동할 수 없습니다\n");
					continue;
				}
				if (map[p1.posi][p1.posj + 1] == 'c')
				{
					printf("이동할 수 없습니다\n");
					continue;
				}
				p1.posj += 1;
				p1.bp -= 1;
				if (!(p2.posi == p1.posi && p2.posj == p1.posj)) //자신이 있는 곳이 아니고 블럭이 없는 곳이라면 자기 땅으로 
					if (!(map[p1.posi][p1.posj] == 'm' || map[p1.posi][p1.posj] == 'b' || map[p1.posi][p1.posj] == 'c' || map[p1.posi][p1.posj] == 's' || map[p1.posi][p1.posj] == 'a'
						|| map[p1.posi][p1.posj] == 'M' || map[p1.posi][p1.posj] == 'B' || map[p1.posi][p1.posj] == 'C' || map[p1.posi][p1.posj] == 'S' || map[p1.posi][p1.posj] == 'A'))
						map[p1.posi][p1.posj] = '@';
				info->act[info->actCount] = 4;
				info->actCount++;
				display_clear();
				print_playerPos();
			}
			else
				continue;
		}

		else if (in == 2)
		{
			in = 0;
			if (map[p1.posi][p1.posj] == 'M' || map[p1.posi][p1.posj] == 'B' || map[p1.posi][p1.posj] == 'C' || map[p1.posi][p1.posj] == 'S' || map[p1.posi][p1.posj] == 'A'
				|| map[p1.posi][p1.posj] == 'm' || map[p1.posi][p1.posj] == 'b' || map[p1.posi][p1.posj] == 'c' || map[p1.posi][p1.posj] == 's' || map[p1.posi][p1.posj] == 'a')
			{
				printf("이미 설치된 블럭이 있습니다\n");
				continue;
			}

			printf("1. 머니블럭(3원) 2. 행동블럭(4원) 3. 통제블럭(2원) 4. 스코어블럭(2원) 5.공격블럭(3원) (back : 그 외의 키) : ");
			scanf("%d", &in);
			getchar();
			switch (in)
			{
			case 1:
				if (p1.money >= 3)
				{
					p1.money -= 3;
					map[p1.posi][p1.posj] = 'M';
					info->act[info->actCount] = 5;
					info->actCount++;
				}
				else
					printf("돈이 부족합니다\n");
				break;
			case 2:
				if (p1.money >= 4)
				{
					p1.money -= 4;
					map[p1.posi][p1.posj] = 'B';
					info->act[info->actCount] = 6;
					info->actCount++;
				}
				else
					printf("돈이 부족합니다\n");
				break;
			case 3:
				if (p1.money >= 2)
				{
					p1.money -= 2;
					map[p1.posi][p1.posj] = 'C';
					info->act[info->actCount] = 7;
					info->actCount++;
				}
				else
					printf("돈이 부족합니다\n");
				break;
			case 4:
				if (p1.money >= 2)
				{
					p1.money -= 2;
					map[p1.posi][p1.posj] = 'S';
					info->act[info->actCount] = 8;
					info->actCount++;
				}
				else
					printf("돈이 부족합니다\n");
				break;
			case 5:
				if (p1.money >= 3)
				{
					p1.money -= 3;
					map[p1.posi][p1.posj] = 'A';
					info->act[info->actCount] = 9;
					info->actCount++;
				}
				else
					printf("돈이 부족합니다\n");
				break;
			}

		}
		else if (in == 3)
		{
			print_map();
		}
		else if (in == 4)
		{

			print_playerPos();
		}
	}
}

void print_act()
{
	int M_count = 0;
	int B_count = 0;
	int C_count = 0;
	int S_count = 0;
	int A_count = 0;
	int m_count = 0;
	int b_count = 0;
	int c_count = 0;
	int s_count = 0;
	int a_count = 0;

	for(int i=0; i<10; i++){
		for(int j=0; j<10; j++){
				if(map[i][j] == 'M') M_count++;
				if(map[i][j] == 'B') B_count++;
				if(map[i][j] == 'C') C_count++;
				if(map[i][j] == 'S') S_count++;
				if(map[i][j] == 'A') A_count++;
				if(map[i][j] == 'm') m_count++;
				if(map[i][j] == 'b') b_count++;
				if(map[i][j] == 'c') c_count++;
				if(map[i][j] == 's') s_count++;
				if(map[i][j] == 'a') a_count++;
		}
	}

		printf("\n-내가 설치한 블럭 통계\n");
		printf("1. 머니블럭: %d\n", M_count);
		printf("2. 행동블럭: %d\n", B_count);
		printf("3. 통제블럭: %d\n", C_count);
		printf("4. 스코어블럭: %d\n", S_count);
		printf("5. 공격블럭: %d\n", A_count);
		
		printf("\n-상대가 설치한 블럭 통계\n");
		printf("1. 머니블럭: %d\n", m_count);
		printf("2. 행동블럭: %d\n", b_count);
		printf("3. 통제블럭: %d\n", c_count);
		printf("4. 스코어블럭: %d\n", s_count);
		printf("5. 공격블럭: %d\n", a_count);
}

int main() {
	int n=0;
	int status;
	
	p1.score = 0; //플레이어 초기화 
	p1.bp = 3;
	p1.money = 10;
	p1.posi = 9;
	p1.posj = 9;

	p2.score = 0;
	p2.bp = 3;
	p2.money = 10;
	p2.posi = 0;
	p2.posj = 0;
	
	init_map();
	
	map[0][0] = '#';
	map[9][9] = '@';

	actInfo info;
	
	if(fork() == 0){
		n = matchmaking();
		initMemory(&info, n);
		// 호스트(선턴) 
		if(n==1){
			while (1)
			{
				info.actCount = 0;
				actInput(&info);
				SaveInfo(&info); //공유 메모리에 행동 저장
				send_signal(); //시그널을 보낸 뒤 시그널이 올 때까지 기다림 
				print(&info);//공유메모리에서 상대 행동 읽어와서 p2_act 호출(상대 행동 처리)
				turn++;
				update();
				if (turn > 15)
				{
					display_clear();
					if (p1.score > p2.score)
					{
						printf("나의 점수 : %d\n",p1.score);
						printf("상대방 점수 : %d\n",p2.score);
						printf("승리");
					}
					else if (p1.score < p2.score)
					{
						printf("나의 점수 : %d\n",p1.score);
						printf("상대방 점수 : %d\n",p2.score);
						printf("패배");
					}
					else
					{
						printf("나의 점수 : %d\n",p1.score);
						printf("상대방 점수 : %d\n",p2.score);
						printf("무승부");
					}	

					print_act();
					exit_game();
					break;
				}
				p1.money += 2;
				p2.money += 2;

				p1.bp += 2;
				p2.bp += 2;
			}
		}

		if(n==2){
			while (1)
			{
				if(turn == 0){
					while (signal_opponent){
						sleep(1);
					}
					signal_opponent = 1;
				}
				print(&info);//공유메모리에서 상대 행동 읽어와서 p2_act 호출(상대 행동 처리)
				info.actCount = 0;
				actInput(&info);
				SaveInfo(&info); //공유 메모리에 행동 저장
				turn++;
				update();
				send_signal(); //시그널을 보낸 뒤 시그널이 올 때까지 기다림 
				if (turn > 15)
				{
					display_clear();
					if (p1.score > p2.score)
					{
						printf("나의 점수 : %d\n",p1.score);
						printf("상대방 점수 : %d\n",p2.score);
						printf("승리");
					}
					else if (p1.score < p2.score)
					{
						printf("나의 점수 : %d\n",p1.score);
						printf("상대방 점수 : %d\n",p2.score);
						printf("패배");
					}
					else
					{
						printf("나의 점수 : %d\n",p1.score);
						printf("상대방 점수 : %d\n",p2.score);
						printf("무승부");
					}	

					print_act();
					break;
				}
				p1.money += 2;
				p2.money += 2;

				p1.bp += 2;
				p2.bp += 2;
			}
		}
	}

	else{
		wait(&status);
		
		//공유 메모리 종료
		if (shmdt(info.Player) == -1) {
			perror("shmdt");
			exit(1);
		}

		// 공유 메모리 삭제
		if (shmctl(info.shmid, IPC_RMID, NULL) == -1) {
			perror("shmctl");
			exit(1);
		}

		return 0;
	}
}
