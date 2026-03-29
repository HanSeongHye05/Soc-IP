#include <stdint.h>
#include <stdio.h>
#include <string.h>        // strlen
#include <xil_exception.h>
#include <xintc_l.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "sleep.h"
#include "xgpio.h"
#include "xintc.h"
#include "xuartlite.h"
#include "xil_types.h"

// 주소 정의
#define stopwatch_addr XPAR_MYIP_STOPWATCH_0_BASEADDR
#define cooktimer_addr XPAR_MYIP_COOKTIMER_0_BASEADDR
#define watch_addr     XPAR_MYIP_WATCH_0_BASEADDR
#define gpio_addr      XPAR_AXI_GPIO_0_BASEADDR
#define fnd_addr       XPAR_MYIP_FND_CNTR_0_BASEADDR
#define intc_addr      XPAR_MICROBLAZE_RISCV_0_AXI_INTC_BASEADDR
#define uart_addr      XPAR_XUARTLITE_0_BASEADDR

#define GPIO_VEC_ID XPAR_FABRIC_XGPIO_0_INTR
#define UART_VEC_ID XPAR_FABRIC_AXI_UARTLITE_0_INTR

#define btn_channel 1
#define sw_channel  2

#define watch         0
#define speed_skating 1
#define cooktimer     2

// LCD 관련 정의
#define iic0_addr XPAR_MYIP_IIC_CNTR_0_BASEADDR
#define iic1_addr XPAR_MYIP_IIC_CNTR_1_BASEADDR

#define REG_DATA_OFFSET   0
#define REG_CTRL_OFFSET   1
#define REG_BUSY_OFFSET   2

#define LCD_CMD_CLEAR           0x01
#define LCD_CMD_RETURN_HOME     0x02
#define LCD_CMD_ENTRY_MODE      0x06
#define LCD_CMD_DISPLAY_ON      0x0C
#define LCD_CMD_FUNCTION_SET    0x28

// 라인 이동(HD44780)
#define LCD_CMD_LINE1           0x80
#define LCD_CMD_LINE2           0xC0

volatile unsigned int *lcd0_ip_base = (unsigned int *)iic0_addr;
volatile unsigned int *lcd1_ip_base = (unsigned int *)iic1_addr;

// ------------------- LCD 함수들 ---------------------------
void LCD0_WaitBusy() { while (lcd0_ip_base[REG_BUSY_OFFSET] & 0x01); }
void LCD1_WaitBusy() { while (lcd1_ip_base[REG_BUSY_OFFSET] & 0x01); }

void LCD0_Write(u8 value, u8 is_data) {
    LCD0_WaitBusy();
    lcd0_ip_base[REG_DATA_OFFSET] = value;
    u32 ctrl_rs = (is_data) ? 0x02 : 0x00;
    lcd0_ip_base[REG_CTRL_OFFSET] = ctrl_rs | 0x00;
    lcd0_ip_base[REG_CTRL_OFFSET] = ctrl_rs | 0x01;
    for(volatile int k=0; k<100; k++);
    lcd0_ip_base[REG_CTRL_OFFSET] = ctrl_rs | 0x00;
}

void LCD1_Write(u8 value, u8 is_data) {
    LCD1_WaitBusy();
    lcd1_ip_base[REG_DATA_OFFSET] = value;
    u32 ctrl_rs = (is_data) ? 0x02 : 0x00;
    lcd1_ip_base[REG_CTRL_OFFSET] = ctrl_rs | 0x00;
    lcd1_ip_base[REG_CTRL_OFFSET] = ctrl_rs | 0x01;
    for(volatile int k=0; k<100; k++);
    lcd1_ip_base[REG_CTRL_OFFSET] = ctrl_rs | 0x00;
}

void LCD0_SendCommand(u8 cmd) { LCD0_Write(cmd, 0); }
void LCD1_SendCommand(u8 cmd) { LCD1_Write(cmd, 0); }
void LCD0_SendData(u8 data)   { LCD0_Write(data, 1); }
void LCD1_SendData(u8 data)   { LCD1_Write(data, 1); }

void LCD0_Init_Soft() {
    usleep(100000);
    LCD0_SendCommand(LCD_CMD_FUNCTION_SET);
    LCD0_SendCommand(LCD_CMD_DISPLAY_ON);
    LCD0_SendCommand(LCD_CMD_ENTRY_MODE);
    LCD0_SendCommand(LCD_CMD_CLEAR);
}

void LCD1_Init_Soft() {
    usleep(100000);
    LCD1_SendCommand(LCD_CMD_FUNCTION_SET);
    LCD1_SendCommand(LCD_CMD_DISPLAY_ON);
    LCD1_SendCommand(LCD_CMD_ENTRY_MODE);
    LCD1_SendCommand(LCD_CMD_CLEAR);
}

void LCD0_PrintString(char *str) { while(*str) { LCD0_SendData(*str++); } }
void LCD1_PrintString(char *str) { while(*str) { LCD1_SendData(*str++); } }

// 16칸 고정 출력(남는 칸 공백)
static void LCD0_Print16(u8 line_cmd, const char *s)
{
    char buf[17];
    int n = (int)strlen(s);
    if (n > 16) n = 16;
    for (int i=0;i<16;i++) buf[i] = ' ';
    for (int i=0;i<n;i++)  buf[i] = s[i];
    buf[16] = '\0';
    LCD0_SendCommand(line_cmd);
    LCD0_PrintString(buf);
}

static void LCD1_PrintN(const char *s, int n) // n글자만 출력(부족하면 공백)
{
    for (int i=0;i<n;i++) {
        char c = (s && s[i]) ? s[i] : ' ';
        LCD1_SendData((u8)c);
    }
}
// -----------------------------------------------------------------------

XUartLite uart_device;
XIntc intc;
XGpio gpio_device;

unsigned int *stopwatch_device = (unsigned int*) stopwatch_addr;
unsigned int *watch_device     = (unsigned int*) watch_addr;
unsigned int *cooktimer_device = (unsigned int*) cooktimer_addr;
unsigned int *fnd_device       = (unsigned int*) fnd_addr;

void Gpio_Handler(void *CallBackRef);
void RecvHandler(void *CallBackRef, unsigned int EventData);
void SendHandler(void *CallBackRef, unsigned int EventData);

volatile char btn_flag = 0;
volatile char sw_flag  = 0;

volatile char uart_rx_data = 0;
volatile int  uart_rx_flag = 0;

volatile unsigned int shared_btn_value = 0;

// ============================================================
// Speed Skating(Stopwatch) : 1명 4바퀴 (간단 버전)
//
// [목표]
// - 4바퀴 스플릿(split) 기록을 SW에서 계산/저장해서 LCD에 표시
// - BEST(가장 빠른 스플릿, PB)는 최소 split로 갱신해서 LCD0에 표시
// - Start/Stop 버튼(1)을 눌러도 "기록은 유지" (초기화는 Clear 버튼(4)에서만)
//
// [스플릿 계산 방식]
// - Stopwatch IP가 제공하는 누적 시간(Abs Time)을 읽음:
//     stopwatch_device[1] = sec
//     stopwatch_device[2] = csec(0~99)
// - Lap 버튼(2) 누르는 순간의 누적시간(now) - 직전 Lap 누적시간(last) = 이번 바퀴 split
//
// [LCD 배치(HD44780 16x2 x 2개)]
// - LCD0 (16x2):
//     L1: "SPEED SKATING MODE" (고정)
//     L2: "BEST:SS.CC"         (PB 표시, 없으면 --.--)
// - LCD1 (16x2): 8칸 + 8칸으로 4랩 표시
//     L1: [0~7]  "1:SS.CC"   [8~15] "2:SS.CC"
//     L2: [0~7]  "3:SS.CC"   [8~15] "4:SS.CC"
//
// [중요 포인트]
// - LCD1은 매번 16칸 전체를 다시 쓰면 옆 칸이 같이 깨질 수 있어서,
//   Lap 기록 시 "해당 랩 칸(8칸)만" 커서 이동 후 갱신한다.
//
// - Lap 펄스(0b010)는 IP에 주지 않는다.
//   (SW에서 split을 관리하므로 IP lap 동작과 섞이면 반복값/기준 꼬임 가능)
// ============================================================
#define TOTAL_LAPS   4
#define CSEC_PER_SEC 100

volatile int lap_count = 0;                 // 저장된 랩 수(0~4)
volatile unsigned int last_abs_sec  = 0;    // 직전 Lap 누적 sec
volatile unsigned int last_abs_csec = 0;    // 직전 Lap 누적 csec

volatile unsigned int split_sec[TOTAL_LAPS];   // 각 랩 split sec
volatile unsigned int split_csec[TOTAL_LAPS];  // 각 랩 split csec

volatile unsigned int best_split_csec = 0;  // PB(최소 split), 0이면 아직 없음

// 누적(sec,csec) -> total_csec 변환
static inline unsigned int to_csec(unsigned int sec, unsigned int csec)
{
    return sec * CSEC_PER_SEC + (csec % CSEC_PER_SEC);
}

// total_csec -> (sec,csec) 변환
static inline void from_csec(unsigned int total_csec, unsigned int *sec, unsigned int *csec)
{
    *sec = total_csec / CSEC_PER_SEC;
    *csec = total_csec % CSEC_PER_SEC;
}

// 기록 전체 초기화 (Clear 버튼에서 사용)
static void Speed_Reset(void)
{
    lap_count = 0;
    last_abs_sec = 0;
    last_abs_csec = 0;
    best_split_csec = 0;

    for (int i=0;i<TOTAL_LAPS;i++) {
        split_sec[i] = 0;
        split_csec[i] = 0;
    }
}

// LCD0 2줄(BEST)만 갱신
static void Speed_UpdateBestLine(void)
{
    char best_line[17];
    if (best_split_csec == 0) {
        snprintf(best_line, sizeof(best_line), "BEST:--.--");
    } else {
        unsigned int bs, bc;
        from_csec(best_split_csec, &bs, &bc);
        snprintf(best_line, sizeof(best_line), "BEST:%02u.%02u", bs, bc);
    }
    LCD0_Print16(LCD_CMD_LINE2, best_line);
}

// idx(0~3) 해당 랩 "8칸"만 LCD1의 정확한 위치에 출력
static void Speed_UpdateOneLapCell(int idx)
{
    // L1(1~2랩): base=LINE1, col=0 or 8
    // L2(3~4랩): base=LINE2, col=0 or 8
    u8 base = (idx < 2) ? LCD_CMD_LINE1 : LCD_CMD_LINE2;
    u8 col  = (idx % 2) * 8;
    LCD1_SendCommand((u8)(base + col));

    char cell[9];
    int lap_no = idx + 1;

    if (lap_count >= lap_no) {
        // 8칸 안에 들어가는 포맷: "n:SS.CC" (최대 7글자) + 공백 padding
        snprintf(cell, sizeof(cell), "%d:%02u.%02u", lap_no, split_sec[idx], split_csec[idx]);
    } else {
        snprintf(cell, sizeof(cell), "%d:--.--", lap_no);
    }

    // 8칸 고정 출력(부족한 글자는 LCD1_PrintN에서 공백으로 채움)
    LCD1_PrintN(cell, 8);
}

// speed skating 화면 프레임(초기 화면) 출력
static void Speed_ShowFrame(void)
{
    // LCD0 1줄: 모드 고정 문구
    LCD0_Print16(LCD_CMD_LINE1, "SPEED SKATING MODE");
    // LCD0 2줄: BEST 표시
    Speed_UpdateBestLine();

    // LCD1: 4개 칸 모두 placeholder 출력
    Speed_UpdateOneLapCell(0);
    Speed_UpdateOneLapCell(1);
    Speed_UpdateOneLapCell(2);
    Speed_UpdateOneLapCell(3);
}

// Lap 버튼 처리: 이번 split 계산/저장 + BEST 갱신 + "해당 칸만" LCD 업데이트
static void Speed_OnLap(void)
{
    if (lap_count >= TOTAL_LAPS) {
        xil_printf("Already finished 4 laps.\r\n");
        return;
    }

    // Stopwatch IP 누적 시간 읽기
    unsigned int abs_sec  = stopwatch_device[1];
    unsigned int abs_csec = stopwatch_device[2] % CSEC_PER_SEC;

    // now_total - last_total = 이번 Lap split
    unsigned int now_total  = to_csec(abs_sec, abs_csec);
    unsigned int last_total = to_csec(last_abs_sec, last_abs_csec);
    if (now_total < last_total) now_total = last_total; // 안전장치

    unsigned int split_total = now_total - last_total;

    // split_total을 sec/csec로 변환해서 저장
    unsigned int sp_s, sp_c;
    from_csec(split_total, &sp_s, &sp_c);

    int idx = lap_count; // 이번에 저장할 랩 인덱스(0~3)

    split_sec[idx]  = sp_s;
    split_csec[idx] = sp_c;

    // PB 갱신(최초 또는 더 작은 split이면 업데이트)
    if (best_split_csec == 0 || split_total < best_split_csec) {
        best_split_csec = split_total;
    }

    lap_count++;

    // 다음 split 계산을 위한 기준점(직전 누적시간) 갱신
    last_abs_sec  = abs_sec;
    last_abs_csec = abs_csec;

    xil_printf("Lap %d split=%u.%02u  (BEST=%u.%02u)\r\n",
               lap_count, sp_s, sp_c,
               best_split_csec / CSEC_PER_SEC, best_split_csec % CSEC_PER_SEC);

    // ★ LCD 갱신은 "이번 랩 칸(8칸)"만 업데이트해서 옆 칸 영향 방지
    Speed_UpdateOneLapCell(idx);

    // BEST 라인만 업데이트
    Speed_UpdateBestLine();
}
// ============================================================

int main(){
    init_platform();

    XUartLite_Initialize(&uart_device, uart_addr);
    XGpio_Initialize(&gpio_device, gpio_addr);

    XGpio_SetDataDirection(&gpio_device, btn_channel, 0xf);
    XGpio_SetDataDirection(&gpio_device, sw_channel, 0xffff);

    LCD0_Init_Soft();
    LCD1_Init_Soft();

    XIntc_Initialize(&intc, intc_addr);
    XIntc_Connect(&intc, GPIO_VEC_ID, Gpio_Handler, (void *)&gpio_device);
    XIntc_Connect(&intc, UART_VEC_ID, (XInterruptHandler) XUartLite_InterruptHandler, (void *)&uart_device);

    XIntc_Enable(&intc, GPIO_VEC_ID);
    XIntc_Enable(&intc, UART_VEC_ID);
    XIntc_Start(&intc, XIN_REAL_MODE);

    XGpio_InterruptEnable(&gpio_device, btn_channel);
    XGpio_InterruptEnable(&gpio_device, sw_channel);
    XGpio_InterruptGlobalEnable(&gpio_device);

    XUartLite_SetRecvHandler(&uart_device, RecvHandler, (void *)&uart_device);
    XUartLite_SetSendHandler(&uart_device, SendHandler, (void *)&uart_device);
    XUartLite_EnableInterrupt(&uart_device);

    Xil_ExceptionInit();
    Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT, (Xil_ExceptionHandler)XIntc_InterruptHandler, (void *)&intc);
    Xil_ExceptionEnable();

    print("System Started\n");
    fnd_device[1] = 1;

    int mode = 0;
    int prev_mode = -1;

    // speed skating 초기 화면
    Speed_Reset();
    LCD0_SendCommand(LCD_CMD_CLEAR);
    LCD1_SendCommand(LCD_CMD_CLEAR);
    usleep(2000);
    Speed_ShowFrame();

    // 첫 수신 예약
    XUartLite_Recv(&uart_device, (u8*)&uart_rx_data, 1);

    while(1)
    {
        if(uart_rx_flag) {
            XUartLite_Recv(&uart_device, (u8*)&uart_rx_data, 1);
            xil_printf("Data received: %c\r\n", uart_rx_data);
            uart_rx_flag = 0;
        }

        // 모드 선택(원본 유지)
        unsigned int sw_val = XGpio_DiscreteRead(&gpio_device, sw_channel);
        switch (sw_val & 0x07) {
        case 0x01: mode = watch ; break;
        case 0x02: mode = speed_skating; break;
        case 0x04: mode = cooktimer; break;
        }

        // 모드 변경(원본 유지)
        if (mode != prev_mode) {
            LCD0_SendCommand(LCD_CMD_CLEAR);
            LCD1_SendCommand(LCD_CMD_CLEAR);
            usleep(2000);

            switch (mode) {
                case speed_skating:
                    Speed_Reset();
                    Speed_ShowFrame();
                    break;
                case cooktimer:
                    LCD0_PrintString("MODE: COOKTIMER");
                    LCD1_PrintString("MODE: COOKTIMER");
                    break;
                case watch:
                    LCD0_PrintString("MODE: WATCH");
                    LCD1_PrintString("MODE: WATCH");
                    break;
            }
            prev_mode = mode;
        }

        // 버튼 처리(원본 유지: speed_skating만 동작 구현)
        if (btn_flag) {
            btn_flag = 0;
            unsigned int btn_value = shared_btn_value;

            switch (mode) {
                case speed_skating:
                    switch (btn_value) {
                        case 1: // Start/Stop
                            // [주의] Start/Stop은 "기록 유지"가 요구사항이므로
                            // Speed_Reset()/Speed_ShowFrame() 같은 초기화 호출을 하지 않는다.
                            // Stopwatch IP에 start/stop 펄스만 전달.
                            stopwatch_device[0] |= 0b001;
                            stopwatch_device[0] &= ~0b001;
                            xil_printf("start/stop\n");

                            while (XGpio_DiscreteRead(&gpio_device, btn_channel) != 0) { }
                            usleep(30000);
                            break;

                        case 2: // Lap
                            Speed_OnLap();

                            // IP lap 펄스는 주지 않음(SW에서 split 관리)
                            // stopwatch_device[0] |= 0b010;
                            // stopwatch_device[0] &= ~0b010;

                            while (XGpio_DiscreteRead(&gpio_device, btn_channel) != 0) { }
                            usleep(30000);
                            break;

                        case 4: // Clear
                            // Clear에서만 기록 초기화
                            Speed_Reset();
                            Speed_ShowFrame();

                            stopwatch_device[0] |= 0b100;
                            stopwatch_device[0] &= ~0b100;
                            xil_printf("clear\n");

                            while (XGpio_DiscreteRead(&gpio_device, btn_channel) != 0) { }
                            usleep(30000);
                            break;
                    }
                    break;

                case cooktimer:
                    break;
                case watch:
                    break;
            }
        }

        // FND 출력(원본 유지: 누적시간 표시)
        switch(mode){
            case speed_skating:
                fnd_device[0] = ((stopwatch_device[1]/10)<<12)
                              + ((stopwatch_device[1]%10)<<8)
                              + ((stopwatch_device[2]/10)<<4)
                              +  (stopwatch_device[2]%10);
                break;
            case cooktimer:
                break;
            case watch:
                break;
        }
    }

    cleanup_platform();
    return 0;
}

// ------------------- 인터럽트 핸들러 -------------------
void Gpio_Handler(void *CallBackRef){
    XGpio *gpio_ptr = (XGpio *)CallBackRef;
    unsigned int isr = XGpio_InterruptGetStatus(gpio_ptr);

    if (isr & 1) { // 버튼
        unsigned int current_btn = XGpio_DiscreteRead(gpio_ptr, btn_channel);
        if (current_btn != 0) {
            shared_btn_value = current_btn;
            btn_flag = 1;
            xil_printf("Button Pressed: %d\r\n", shared_btn_value);
        }
        XGpio_InterruptClear(gpio_ptr, btn_channel);
    }

    if (isr & 2) { // 스위치
        sw_flag = 1;
        print("switch interrupt\r\n");
        XGpio_InterruptClear(gpio_ptr, sw_channel);
    }
}

void RecvHandler(void *CallBackRef, unsigned int EventData){
    uart_rx_flag = 1;
}

void SendHandler(void *CallBackRef, unsigned int EventData){
    return;
}
