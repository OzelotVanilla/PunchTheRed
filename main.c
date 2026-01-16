#include "./lib.c"

void main(void) // 主入口函数
{
    unsigned char i = 0, j = 0;
    int user_input;
    TMOD = 0x02; /* timer 0, mode 2 */
    TH0 = 0x00;   
    TL0 = 0x00;
    TR0 = 1; /* 启动定时器 */
    IE = 0x82; /* Enable timer 0 interrupt */
		
    while (1)
    {
        reden = 0; green = 1;
        flushToVerticalLine();
        
        user_input = readFromButton();
        updateNoteDisplay();
        processUserInput(user_input);

        display();
    }
}