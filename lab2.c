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
volatile int state = OFF; // ������ �Ҹ� ������ ����
volatile int st = ON; // Ÿ�̸Ӱ� ������ ���� ������ on,off�ϱ� ���� ���� ����

OS_STK       TaskStk[N_TASKS][TASK_STK_SIZE]; // task ����
OS_EVENT *Mbox; // ���� �ڽ� �̺�Ʈ
OS_EVENT *Sem; // ��������
OS_EVENT *Sem2; // ��������
OS_FLAG_GRP *r_grp; // �̺�Ʈ �÷��� ����
OS_EVENT *MsgQ; // �޽��� ť �̺�Ʈ
void *MsgQarr[MSG_QUEUE_SIZE]; // �޽��� ť

void  BuzzerTask(void *data); // �뷡�� ���踦 �����ϴ� task
void  FndTask(void *data); // �뷡 ��ȣ�� ��� �ð��� ��Ÿ���� task
void LedTask(void*); // ������ ��Ÿ���� task

volatile int count; // �ܰ�
volatile int songnum; // �뷡 ��ȣ
volatile int num; // �뷡 ��� �ð�
volatile int arr[12] = { 17, 43, 66, 77, 97, 114, 129, 137, 150, 161, 166, -1 }; // 32���� ����
// 1�� �뷡 : �б� ���� ������, 2�� �뷡 : �����, 3�� �뷡 : ���䳢
volatile int song[3][270] = { { 11,11,4,4,4,11,4,4,4,11,5,5,5,11,5,5,5,11,4,4,4,11,4,4,4,11,2,2,2,2,2,11,11,11,4,4,4,11,4,4,4,11,2,2,2,11,2,2,2,11,1,1,1,1,1,11,11,11,4,4,4,11,4,4,4,11,5,5,5,11,5,5,5,11,4,4,4,11,4,4,4,11,2,2,2,2,2,11,4,4,4,11,2,2,2,11,1,1,1,11,2,2,2,11,0,0,0,0,0, 100 },
							{11,11,4,4,4,11,2,2,2,11,2,2,2,11,11,11,3,3,3,11,1,1,1,11,1,1,1,11,11,11,0,0,0,1,1,1,2,2,2,3,3,3,4,4,11,4,4,11,4,4,4,4,4,11,4,4,4,11,2,2,2,11,2,2,2,11,2,2,2,11,3,3,3,11,1,1,1,11,1,1,1,1,1,11,0,0,0,11,2,2,2,11,4,4,4,11,4,4,4,11,2,2,2,11,2,2,2,11,2,2,2,2,2, 100},
							{11,11,4,4,4,4,4,11,2,2,2,11,2,2,2,11,4,4,4,11,2,2,2,11,0,0,0,0,0,11,11,1,1,1,1,1,11,2,2,2,11,1,1,1,11,11,0,0,0,11,2,2,2,11,4,4,4,4,4,11,11,11,7,7,7,11,4,4,4,11,7,7,7,11,4,4,4,11,7,7,7,11,4,4,4,11,2,2,2,2,11,11,4,4,4,4,4,11,1,1,1,11,3,3,3,11,2,2,2,11,1,1,1,11,0,0,0,0, 100} };

ISR(TIMER2_OVF_vect) {
	if (state == ON && arr[song[songnum][count]] >= 0) { // ������°� �ǰ�, ���谪�� ������ �ƴϸ� ���� ����
		if (st == ON) { // ����ON���¸� ������ off��Ŵ
			PORTB = 0x00;
			st = OFF;
		}
		else { // ����OFF���¸� ������ ON��Ŵ
			PORTB = 0x10;
			st = ON;
		}
		TCNT2 = arr[song[songnum][count]]; // song[songnum][count]�� �ش�Ǵ� ���踦 ��Ÿ���� ���� Ŭ���� ���� ������ Ÿ�̸� �ֱ� ����
	}
}

ISR(INT4_vect) {
	if (state == ON) { // sw1�� ������ ������ Running���¸� Stop
		state = OFF;
	}
	else { // sw1�� ������ ������ Stop���¸� Running
		state = ON;
	}
}

ISR(INT5_vect) { // ���� �뷡�� �ٲ� 
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
	EICRB = 0x0A;  // INT 4,5 �ϰ� ���� ���.
	EIMSK = 0x30;  // 4,5�� ���ͷ�Ʈ ���
	SREG |= 1 << 7; //���� ���ͷ�Ʈ ����

	DDRB = 0x10; // ���� ��� ���� PB4
	TCCR2 = 0x03; // Ÿ�̸� Ŭ���� ���� Ŭ���� 32���ַ� ����
	TIMSK = 0x40; // timer 2����  overflow interrupt ����
	TCNT2 = arr[song[songnum][count]];

	int err;
	Sem = OSSemCreate(1); // �������� ����
	Sem2 = OSSemCreate(1); // �������� ����
	Mbox = OSMboxCreate((void *)0); // ���Ϲڽ� ����
	r_grp = OSFlagCreate(0x00, &err); // �̺�Ʈ �÷��� ����
	MsgQ = OSQCreate(&MsgQarr[0], MSG_QUEUE_SIZE); // �޽��� ť ����

	// task�� ����
	OSTaskCreate(BuzzerTask, (void *)0, (void *)&TaskStk[0][TASK_STK_SIZE - 1], 1);
	OSTaskCreate(FndTask, (void *)0, (void *)&TaskStk[1][TASK_STK_SIZE - 1], 2);
	OSTaskCreate(LedTask, (void*)0, (void*)&TaskStk[2][TASK_STK_SIZE - 1], 0);

	OSStart();

	return 0;
}

// ���� task
void BuzzerTask(void *data)
{
	data = data;

	int err;
	while (1) {
		OSFlagPend(r_grp, 0x01, OS_FLAG_WAIT_SET_ANY + OS_FLAG_CONSUME, 0, &err); // 100ms���� fnd���� �̺�Ʈ �÷��׸� Ȱ��ȭ�� ������ wait

		OSSemPend(Sem, 0, &err); // lock
		count++;
		if (song[songnum][count] == 100) { // �뷡�� �������� �˷��ִ� ��ȣ�� ������ ��
			songnum++; // ���� �뷡�� �̵�
			
			// ����ð�, �ܰ� �ʱ�ȭ
			count = 0;
			num = 0;
			if (songnum == SONGNUM) { // ������ �뷡���� ù �뷡�� �Ѿ
				songnum = 0;
			}
		}
		OSSemPost(Sem); // unlock

		OSMboxPost(Mbox, (void*)&song[songnum][count]); // LED task�� ���Ϲڽ��� ���� ���� ���踦 ����
		
		OSTimeDlyHMSM(0, 0, 0, 1);
	}
}

void FndTask(void *data)
{
	int err;
	unsigned char FND[10] = { 0x3f, 0x06, 0x5b, 0x4f,
		0x66, 0x6d, 0x7d, 0x27, 0x7f, 0x6f }; // fnd�� 0~9���� ����ð��� ǥ���ϱ� ���� ���Ǵ� �迭��
	unsigned char songfnd[10] = { 0xbf, 0x86, 0xdb, 0xcf,
		0xe6, 0xed, 0xfd, 0xa7, 0xff, 0xdf }; // fnd�� �뷡 ��ȣ�� ǥ���ϱ� ���� dp�� ������ fnd �迭��
	unsigned char stop[4] = { 0x6d, 0x78, 0x3f, 0x73}; // stop ǥ�� fnd �迭��
	unsigned char play[4] = { 0x73, 0x38, 0x77, 0x6e }; // play ǥ�� fnd �迭��
	unsigned char idx[4] = { 0x01, 0x02, 0x04, 0x08 }; // fnd ��ġ�� ����Ű�� ��
	
	DDRC = 0xff;
	DDRG = 0x0f;
	int id[4] = { 0, };
	int i, cnt = 0, start = 0;
	unsigned char value = FND[0];
	for (; ; ) {
		if (state == ON) {
			start = 1; // start �÷��� 1�� ����
			if (cnt == 10) { // 100ms ����
				OSFlagPost(r_grp, 0x01, OS_FLAG_SET, &err); // r_grp �̺�Ʈ �÷��� Ȱ��ȭ => ���� task�� ready ���·� ����
				OSQPost(MsgQ, (void *)&cnt); // LED task���� �޽����� �־ LED task�� ready ���·� ����
				cnt = 0;
			}
			for (i = 0; i < 3; i++) { // ��� �ð� ���
				PORTC = FND[id[i]];
				PORTG = idx[i];
				_delay_ms(2);
			}
			// �뷡 ��ȣ ���
			PORTC = songfnd[songnum + 1];
			PORTG = idx[3];
			_delay_ms(2);

			// 1/100�� �� ++
			OSSemPend(Sem2, 0, &err);

			num++; // ��� �ð� ����
			// ��� �ð��� �� �ڸ��� ����
			id[0] = num % 100 / 10;
			id[1] = num % 1000 / 100;
			id[2] = num % 10000 / 1000;
			
			OSSemPost(Sem2);

			cnt++;
		}
		else { // ������ �������� ��
			if (start == 0) { // ó�� �����϶��� fnd�� play ǥ��
				for (i = 0; i < 4; i++) {
					PORTC = play[i];
					PORTG = idx[3 - i];
					_delay_ms(1);
				}
				_delay_ms(1);
			}
			else { // �ѹ� ������ ���� �뷡�� �Ͻ����� ��ų �� stop ǥ��
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
		OSQPend(MsgQ, 0, &err); // fnd �½�ũ���� �޼���ť�� �޽����� ���������� wait
		sound = *((INT8U *)OSMboxPend(Mbox, 0, &err)); // �������� 100ms ���� ���谪�� ���� �ڽ��� ���������� wait
		if (sound != 11) { // ���� �ڽ��� ���� sound ���� ������ �� value �� ����
			value = 0;
			for (i = 0; i <= sound; i++) {
				value |= (1 << i);
				PORTA = value;
			}
		}
		else { // �� �� ���谪 ���
			PORTA = value;
		}
		
		OSTimeDlyHMSM(0, 0, 0, 1);
	}
}