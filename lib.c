#include <reg51.h>
#include <intrins.h>
#include "./rhythm_sheet.c"
#define DATAOUT P1 // 指定P1口做为输出-- 16X16板上标示

sbit DATA  = DATAOUT ^ 0; // 行数据输出位------     RLI------------>	P1.0
sbit SCLH  = DATAOUT ^ 1; // 行扫描时钟位------     SCK------------>	P1.1
sbit SCLT  = DATAOUT ^ 2; // 行数据锁存位------     RCK -----------> P1.2
sbit AB    = DATAOUT ^ 3; // 列数据输出位-----      AB ------------>	P1.3
sbit SCK   = DATAOUT ^ 4; // 列扫描时钟位-----      CLK ----------->	P1.4
sbit reden = P0 ^ 1;      // 红色数据线             EN1------P01
sbit green = P0 ^ 0;      // 绿色数据线             EN------P00

sbit btn_a = P2 ^ 1;
sbit btn_b = P2 ^ 2;
sbit btn_c = P2 ^ 3;
sbit btn_d = P2 ^ 4;

unsigned char lhj[32];                      // 32字节RAM做为16*16点阵屏显示缓存
void          display();                    // 做为点阵扫描函数，将显示缓存的数据输出到点阵屏

/** 16 updates means a seconds */
unsigned int timerCount = 0;
unsigned int timerInSecCount = 0;

const int n_update_per_note_move = 4;
const int n_update_before_start = 32;
const int x_grid_of_note_length = 2;
const int x_grid_of_gap = 2;
int heading_note_to_edge = 0; 
int heading_note_index = -1; // -1 means not started, greater than 0 means already start
int heading_note_at_line = 1;
int game_end = -1; // If 1 then means already end.

code unsigned char dg[32] = { //
    // 全亮数据
    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,

    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,

    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,

    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,
};

code unsigned char vertical_line[32] = {
    0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 
    0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 
    0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 
    0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 
};

/** Turn LED cache to all off. */
// void flush()
// {
// 		int i;
// 		for (i = 0; i < 32; i++) { lhj[i] = 0x00; }
// }

/**
 * Show a verticle line on the LED, by writting to LED cache `lhj`.
 * 
 * This line is used for indicate the line-of-judgement.
 */
void flushToVerticalLine()
{
    int i;
    for (i = 0; i < 32; i++) { lhj[i] = vertical_line[i]; }
}

/**
 * Find and change the value of the heading note and distance to edge.
 */
void changeToNextNote()
{
    int new_heading_note_index;
    if (game_end > 0) { return; }

    new_heading_note_index = heading_note_index + 1;

    if (new_heading_note_index >= line_size) { game_end = 1; heading_note_at_line = -1; return ; }

    // Calculate the new value of heading note to edge.
    // The head to head (excluded one edge) distance of next note will be:
    heading_note_to_edge += (x_grid_of_gap + x_grid_of_note_length) * (new_heading_note_index - heading_note_index);
    heading_note_index = new_heading_note_index;
    if (line_1[heading_note_index] > 0) { heading_note_at_line = 1; }
    else if (line_2[heading_note_index] > 0) { heading_note_at_line = 2; }
    else if (line_3[heading_note_index] > 0) { heading_note_at_line = 3; }
    else if (line_4[heading_note_index] > 0) { heading_note_at_line = 4; }
}

int calculateDistanceToEdge(int index)
{
    return (index - heading_note_index) * (x_grid_of_gap + x_grid_of_note_length) + heading_note_to_edge;
}

/**
 * Draw a 2x2 block to LED screen cache `lhj`.
 */
void drawOn(int line, int dist_to_edge)
{
    int i = 0;
    int one_dot_line = 0;

    // Small blocks of given length should be draw.

    // Parts that should be drawn in the left side:
    for (i = 1; i <= x_grid_of_note_length && i + dist_to_edge <= 5; i++)
    {
        one_dot_line = 0x80 >> (2 + i + dist_to_edge);
        lhj[(line * 4 - 3) * 2] |= one_dot_line;
        lhj[(line * 4 - 2) * 2] |= one_dot_line;
    }

    // Parts that should be drawn in the right side:
    if (dist_to_edge + x_grid_of_note_length > 5)
    {
        for (; i <= x_grid_of_note_length && i + dist_to_edge <= 13; i++)
        {
            one_dot_line = 0x80 >> (2 + i + dist_to_edge - 8);
            lhj[(line * 4 - 3) * 2 + 1] |= one_dot_line;
            lhj[(line * 4 - 2) * 2 + 1] |= one_dot_line;
        }
    }
}

/**
 * Update the notes position to LED cache `lhj`.
 * 
 * Should call after flush to vertical line.
 */
void updateNoteDisplay()
{
    int draw_index = heading_note_index;
    int dist = 0;

    // Game already end.
    if (game_end > 0) { return; }

    // If still not start
    else if (heading_note_index < 0)
    {
        // If should start
        if (timerCount >= n_update_before_start) { heading_note_index = 0; heading_note_to_edge = 13; }
        else { return; }
    }

    // Game started
    // Should decrease the count of heading_note_to_edge if need.
    if (timerCount % n_update_per_note_move == 0) { heading_note_to_edge--; }
    // If that makes one note already out of the edge, move to next one.
    if (heading_note_to_edge < -3) { changeToNextNote(); }

    // Draw it.
    do
    {
        dist = calculateDistanceToEdge(draw_index);
        if (calculateDistanceToEdge(draw_index) > 13) { break; }
        if (line_1[draw_index] != 0) drawOn(1, dist);
        if (line_2[draw_index] != 0) drawOn(2, dist);
        if (line_3[draw_index] != 0) drawOn(3, dist);
        if (line_4[draw_index] != 0) drawOn(4, dist);
        draw_index++;
    }
    while(draw_index < line_size);
}

/**
 * Read raw button input, change to the button number printed on the button board.
 */
int readFromButton()
{
	if (btn_d == 1) { return 0; }
	return (btn_a == 1 ? 1 : 0) 
			 + (btn_b == 1 ? 2 : 0)
			 + (btn_c == 1 ? 4 : 0)
			 + 1;
}

/**
 * Process user input.
 * 
 * If user inputs correct button at correct time,
 * turn the LED screen to green (in one cycle).
 */
void processUserInput(int input)
{
    if (game_end > 0) { return; }

    // If there is a note that near that line
    if (heading_note_to_edge <= 0 && heading_note_at_line == input)
    {
        reden = 1; green = 0; // Feedback to user.
        changeToNextNote();
    }
}

void T0INT(void) interrupt 1
{
    timerInSecCount = ((++timerInSecCount) * 16) % 225;
    if (timerInSecCount == 0) {timerCount++;}
    // TH0 = 0x00;   
    // TL0 = 0x00;
}

/**
 * Display function based on the sample code provided by MCU vendor.
 * 
 * This function writes the cache `lhj` to the LED screen.
 * The comment written in Chinese explains the technical detail of writing.
 */
void display() // 显示
{
    unsigned char i, ia, j, tmp; // 定义变量
    DATAOUT = 0XFF;              // 置位高电平做准备
    AB      = 0;                 // 将列数据位清0，准备移位
    for (i = 0; i < 16; i++)
    {             // 循环输出16行数据
        SCK  = 0; // 为列移位做准备
        SCLT = 0; // 为行锁存做准备
        for (ia = 2; ia > 0;)
        {                           // 每行16个点，循环位移两个字节
            ia--;                   // 循环两次
            tmp = ~lhj[i * 2 + ia]; // 读取点阵数据做输出，这里用到ia目的是先读取点阵数据的第二位字节，因一行16个点由两个字节组成，
                                    // 电路中的移位寄存器最后一位对应最后一列，所以要先输出一行中的第二个字节数据
            for (j = 0; j < 8; j++)
            {                      // 循环两次，每次移一个字节，
                SCLH = 0;          // 为列移位做准备
                DATA = tmp & 0x01; // 将数据低位做输出，由电路图可知，移位寄存器的最后一位对应最后一列，因此先移最后一位
                tmp >>= 1;         // 将数据缓冲右移一位，为下次输出做准备
                SCLH = 1;          // 将DATA上的数据移入寄存器
            }                      // 移入单字节结束
        }                          // 移入两个字节结束
        SCK  = 1;                  // SCK拉高，列数据移位，相应行拉低，三极管导通输出电量到相应行点阵管阳极（共阳）
        SCLT = 1;                  // SCLT拉高，将数据锁存输出到相应行的点阵发光管显示，显示一行后将保持到下一行显示开始
        AB   = 1;                  // 列数据位只在第一行时为0，其它时候都为1，当将这个0移入寄存器后，从第一位开始一直移位最后一位，
                                   // 移位的过程，AB就必需是1，这是因为不能同时有两个及两个以上0的出现，否则显示出乱
    }
    j = 64;
    while (j--)
        ;    // 每一行的显示，保持16个移位时间，因此，最后一行的显示，也要加入保持时间，补尝显示的亮度
    SCK = 0; //
    SCK = 1; // 将最后一行数据移出
}