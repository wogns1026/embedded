#include "includes.h"

#define F_CPU	16000000UL	// CPU frequency = 16 Mhz
#include <avr/io.h>	
#include <util/delay.h>

#define  TASK_STK_SIZE  OS_TASK_DEF_STK_SIZE            
#define  N_TASKS        3
#define MSG_QUEUE_SIZE 4
#define ON 1
#define OFF 0
#define SONGNUM 3
volatile int state = OFF; // 부저의 소리 유무를 조절
volatile int st = ON; // 타이머가 지날때 마다 부저를 on,off하기 위한 상태 변수

OS_STK       TaskStk[N_TASKS][TASK_STK_SIZE]; // task 스택
OS_EVENT *Mbox; // 메일 박스 이벤트
OS_EVENT *Sem; // 세마포어
OS_EVENT *Sem2; // 세마포어
OS_FLAG_GRP *r_grp; // 이벤트 플래그 설정
OS_EVENT *MsgQ; // 메시지 큐 이벤트
void *MsgQarr[MSG_QUEUE_SIZE]; // 메시지 큐

void  BuzzerTask(void *data); // 노래를 음계를 설정하는 task
void  FndTask(void *data); // 노래 번호와 재생 시간을 나타내는 task
void LedTask(void*); // 음정을 나타내는 task

volatile int count; // 단계
volatile int songnum; // 노래 번호
volatile int num; // 노래 재생 시간
volatile int arr[12] = { 17, 43, 66, 77, 97, 114, 129, 137, 150, 161, 166, -1 }; // 32분주 음계
// 1번 노래 : 학교 종이 땡땡땡, 2번 노래 : 나비야, 3번 노래 : 산토끼
volatile int song[3][270] = { { 11,11,4,4,4,11,4,4,4,11,5,5,5,11,5,5,5,11,4,4,4,11,4,4,4,11,2,2,2,2,2,11,11,11,4,4,4,11,4,4,4,11,2,2,2,11,2,2,2,11,1,1,1,1,1,11,11,11,4,4,4,11,4,4,4,11,5,5,5,11,5,5,5,11,4,4,4,11,4,4,4,11,2,2,2,2,2,11,4,4,4,11,2,2,2,11,1,1,1,11,2,2,2,11,0,0,0,0,0, 100 },
							{11,11,4,4,4,11,2,2,2,11,2,2,2,11,11,11,3,3,3,11,1,1,1,11,1,1,1,11,11,11,0,0,0,1,1,1,2,2,2,3,3,3,4,4,11,4,4,11,4,4,4,4,4,11,4,4,4,11,2,2,2,11,2,2,2,11,2,2,2,11,3,3,3,11,1,1,1,11,1,1,1,1,1,11,0,0,0,11,2,2,2,11,4,4,4,11,4,4,4,11,2,2,2,11,2,2,2,11,2,2,2,2,2, 100},
							{11,11,4,4,4,4,4,11,2,2,2,11,2,2,2,11,4,4,4,11,2,2,2,11,0,0,0,0,0,11,11,1,1,1,1,1,11,2,2,2,11,1,1,1,11,11,0,0,0,11,2,2,2,11,4,4,4,4,4,11,11,11,7,7,7,11,4,4,4,11,7,7,7,11,4,4,4,11,7,7,7,11,4,4,4,11,2,2,2,2,11,11,4,4,4,4,4,11,1,1,1,11,3,3,3,11,2,2,2,11,1,1,1,11,0,0,0,0, 100} };

ISR(TIMER2_OVF_vect) {
	if (state == ON && arr[song[songnum][count]] >= 0) { // 재생상태가 되고, 음계값이 음수가 아니면 부저 실행
		if (st == ON) { // 부저ON상태면 부저를 off시킴
			PORTB = 0x00;
			st = OFF;
		}
		else { // 부저OFF상태면 부저를 ON시킴
			PORTB = 0x10;
			st = ON;
		}
		TCNT2 = arr[song[songnum][count]]; // song[songnum][count]에 해당되는 음계를 나타내기 위해 클럭의 수를 지정해 타이머 주기 설정
	}
}

ISR(INT4_vect) {
	if (state == ON) { // sw1을 누를때 부저가 Running상태면 Stop
		state = OFF;
	}
	else { // sw1을 누를때 부저가 Stop상태면 Running
		state = ON;
	}
}

ISR(INT5_vect) { // 다음 노래로 바꿈 
	songnum++;
	count = 0;
	num = 0;
	if (songnum == SONGNUM) {
		songnum = 0;
	}
}

int main(void)
{
	OSInit();
	OS_ENTER_CRITICAL();
	TCCR0 = 0x07;
	TIMSK = _BV(TOIE0);
	TCNT0 = 256 - (CPU_CLOCK_HZ / OS_TICKS_PER_SEC / 1024);
	OS_EXIT_CRITICAL();

	DDRA = 0xff;
	EICRB = 0x0A;  // INT 4,5 하강 엣지 사용.
	EIMSK = 0x30;  // 4,5번 인터럽트 사용
	SREG |= 1 << 7; //전역 인터럽트 설정

	DDRB = 0x10; // 버저 출력 설정 PB4
	TCCR2 = 0x03; // 타이머 클럭을 본래 클럭의 32분주로 설정
	TIMSK = 0x40; // timer 2번의  overflow interrupt 설정
	TCNT2 = arr[song[songnum][count]];

	int err;
	Sem = OSSemCreate(1); // 세마포어 생성
	Sem2 = OSSemCreate(1); // 세마포어 생성
	Mbox = OSMboxCreate((void *)0); // 메일박스 생성
	r_grp = OSFlagCreate(0x00, &err); // 이벤트 플래그 생성
	MsgQ = OSQCreate(&MsgQarr[0], MSG_QUEUE_SIZE); // 메시지 큐 생성

	// task들 생성
	OSTaskCreate(BuzzerTask, (void *)0, (void *)&TaskStk[0][TASK_STK_SIZE - 1], 1);
	OSTaskCreate(FndTask, (void *)0, (void *)&TaskStk[1][TASK_STK_SIZE - 1], 2);
	OSTaskCreate(LedTask, (void*)0, (void*)&TaskStk[2][TASK_STK_SIZE - 1], 0);

	OSStart();

	return 0;
}

// 부저 task
void BuzzerTask(void *data)
{
	data = data;

	int err;
	while (1) {
		OSFlagPend(r_grp, 0x01, OS_FLAG_WAIT_SET_ANY + OS_FLAG_CONSUME, 0, &err); // 100ms마다 fnd에서 이벤트 플래그를 활성화할 때까지 wait

		OSSemPend(Sem, 0, &err); // lock
		count++;
		if (song[songnum][count] == 100) { // 노래가 끝났음을 알려주는 신호가 들어왔을 때
			songnum++; // 다음 노래로 이동
			
			// 재생시간, 단계 초기화
			count = 0;
			num = 0;
			if (songnum == SONGNUM) { // 마지막 노래에서 첫 노래로 넘어감
				songnum = 0;
			}
		}
		OSSemPost(Sem); // unlock

		OSMboxPost(Mbox, (void*)&song[songnum][count]); // LED task로 메일박스를 통해 현재 음계를 보냄
		
		OSTimeDlyHMSM(0, 0, 0, 1);
	}
}

void FndTask(void *data)
{
	int err;
	unsigned char FND[10] = { 0x3f, 0x06, 0x5b, 0x4f,
		0x66, 0x6d, 0x7d, 0x27, 0x7f, 0x6f }; // fnd로 0~9까지 재생시간을 표현하기 위해 사용되는 배열값
	unsigned char songfnd[10] = { 0xbf, 0x86, 0xdb, 0xcf,
		0xe6, 0xed, 0xfd, 0xa7, 0xff, 0xdf }; // fnd로 노래 번호를 표현하기 위해 dp를 포함한 fnd 배열값
	unsigned char stop[4] = { 0x6d, 0x78, 0x3f, 0x73}; // stop 표시 fnd 배열값
	unsigned char play[4] = { 0x73, 0x38, 0x77, 0x6e }; // play 표시 fnd 배열값
	unsigned char idx[4] = { 0x01, 0x02, 0x04, 0x08 }; // fnd 위치를 가르키는 값
	
	DDRC = 0xff;
	DDRG = 0x0f;
	int id[4] = { 0, };
	int i, cnt = 0, start = 0;
	unsigned char value = FND[0];
	for (; ; ) {
		if (state == ON) {
			start = 1; // start 플래그 1로 설정
			if (cnt == 10) { // 100ms 마다
				OSFlagPost(r_grp, 0x01, OS_FLAG_SET, &err); // r_grp 이벤트 플래그 활성화 => 부저 task를 ready 상태로 만듬
				OSQPost(MsgQ, (void *)&cnt); // LED task에게 메시지를 주어서 LED task를 ready 상태로 만듬
				cnt = 0;
			}
			for (i = 0; i < 3; i++) { // 재생 시간 출력
				PORTC = FND[id[i]];
				PORTG = idx[i];
				_delay_ms(2);
			}
			// 노래 번호 출력
			PORTC = songfnd[songnum + 1];
			PORTG = idx[3];
			_delay_ms(2);

			// 1/100초 당 ++
			OSSemPend(Sem2, 0, &err);

			num++; // 재생 시간 증가
			// 재생 시간을 각 자리로 나눔
			id[0] = num % 100 / 10;
			id[1] = num % 1000 / 100;
			id[2] = num % 10000 / 1000;
			
			OSSemPost(Sem2);

			cnt++;
		}
		else { // 부저가 꺼져있을 때
			if (start == 0) { // 처음 시작일때는 fnd에 play 표시
				for (i = 0; i < 4; i++) {
					PORTC = play[i];
					PORTG = idx[3 - i];
					_delay_ms(1);
				}
				_delay_ms(1);
			}
			else { // 한번 시작한 이후 노래를 일시정지 시킬 때 stop 표시
				for (i = 0; i < 4; i++) {
					PORTC = stop[i];
					PORTG = idx[3 - i];
					_delay_ms(1);
				}
				_delay_ms(1);
			}
		}
		
		OSTimeDlyHMSM(0, 0, 0, 1);
	}
}

void LedTask(void *data) {
	int value, i, sound, err;

	while (1) {
		OSQPend(MsgQ, 0, &err); // fnd 태스크에서 메세지큐로 메시지를 보낼때까지 wait
		sound = *((INT8U *)OSMboxPend(Mbox, 0, &err)); // 부저에서 100ms 마다 음계값을 메일 박스로 보낼때까지 wait
		if (sound != 11) { // 메일 박스로 받은 sound 값이 무음일 때 value 값 설정
			value = 0;
			for (i = 0; i <= sound; i++) {
				value |= (1 << i);
				PORTA = value;
			}
		}
		else { // 그 전 음계값 출력
			PORTA = value;
		}
		
		OSTimeDlyHMSM(0, 0, 0, 1);
	}
}