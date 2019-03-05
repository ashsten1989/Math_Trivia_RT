/*----------------------------------------------------------------------------
 *
 *  CLIENT
 *---------------------------------------------------------------------------- */


#include "bsp.h"
#include "mrfi.h"
#include "nwk_types.h"
#include "nwk_api.h"
#include "bsp_leds.h"
#include "bsp_buttons.h"
#include "app_remap_led.h"
#include <stdlib.h>

void MCU_Init();
void TXString( char* string, int length );
void InitTimerA(int);
void SinglePlayer();
void TwoPlayer();
void resetGame();
void NewRound();

static uint8_t sCB(linkID_t);

// OP-codes
enum Status{DEF, MENU, SINGLE_PLAYER, MULTIPLAYER, CHOOSE_DIFF};
enum RxOpCode{DEFAULT,WAIT_FOR_OPPONENT,ROUND_START,ROUND_END,GAME_END};
enum TxOpCode{NAME,RESULTS};
enum Difficulty{NON, EASY, MEDIUM, HARD};

// game vars
int timeOutLatency=5;
static uint8_t gameGlobaldiff;
static char sUserAns[3]={0}, myScore=5, ctr=0, timeOut=0, playAgain=0, isWaitingForAnswer=0, timerCnt=0;
enum Status gameStatus=MENU;
enum RxOpCode rx_operation;

bspIState_t intState;

// user name vars
static char usrName[5]={0}, gotName=0, otherusrName[5]={0};

// work loop semaphores
static volatile uint8_t sPeerFrameSem = 0;
static linkID_t linkID = 0;

// belong to RXCode()
static uint8_t rxlen;
static uint8_t rxmsg[7];

void main (void)
{
    WDTCTL = WDTPW | WDTHOLD;
    srand(123);
    BSP_Init();
    MCU_Init();

#ifdef I_WANT_TO_CHANGE_DEFAULT_ROM_DEVICE_ADDRESS_PSEUDO_CODE
    {
        addr_t lAddr;

        createRandomAddress(&lAddr);
        SMPL_Ioctl(IOCTL_OBJ_ADDR, IOCTL_ACT_SET, &lAddr);
    }
#endif /* I_WANT_TO_CHANGE_DEFAULT_ROM_DEVICE_ADDRESS_PSEUDO_CODE */


    TXString("\r\nPlease enter your name (up to 5 chars): ",42);          // get player's name
    __bis_SR_register(LPM3_bits + GIE);

  while (1){                                                            // main program loop
    TXString("\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n",20);
    P1OUT = 0x00;
    gameStatus=MENU;
    TXString("\r\nWelcome ",10);
    TXString(usrName,5);
    TXString("\r\n\r\n\r\nPlease choose one of the options:\r\n1) Single Player\r\n2) Two Player\r\nNote: you may press 'Esc' button to restart, anytime.\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n",147);
    __bis_SR_register(LPM3_bits + GIE);
  }

}


/* Timer A0 interrupt service routine
*/
#pragma vector=TIMERA0_VECTOR
__interrupt void Timer_A (void)
{
  switch (gameStatus){
    case DEF:
      TACTL = MC_0 + TACLR;
      InitTimerA(1);
      __bic_SR_register_on_exit(LPM3_bits);
      break;
    case SINGLE_PLAYER:
      if(TACCR0==48000 && !timerCnt){                   // first 4 seconds out of 8 -> update timerCtr++
        TACTL = MC_0 + TACLR;
        InitTimerA(4);
        timerCnt++;
      }
      else{                                             // last 4 seconds out of8 -> update timeout flag
        TACTL = MC_0 + TACLR;
        timeOut = 1;
        timerCnt--;
        __bic_SR_register_on_exit(LPM3_bits);
      }
      break;
    case MULTIPLAYER:
        if(timerCnt != 20){
          TACTL = MC_0 + TACLR;
          InitTimerA(4);
          timerCnt++;
        }
        else{
        TACTL = MC_0 + TACLR;
        timeOut = 1;
        timerCnt=0;
        __bic_SR_register_on_exit(LPM3_bits);
        }

      break;
    case MENU:
      __bic_SR_register_on_exit(LPM3_bits);             // Clear LPM3 bit from 0(SR)
      break;
  }
}

/* UART RX interrupt service routine:
  * Take care of all user interface activities.
*/
#pragma vector=USCIAB0RX_VECTOR
__interrupt void USCI0RX_ISR(void)
{
  char rx = UCA0RXBUF;
  TXString(&rx,1);

  if (rx==27){                                          // reboot command - "Esc"
    TACTL = MC_0 + TACLR;
    TXString("\r\nRebooting...",13);
    WDTCTL = WDTPW + ~WDTHOLD;
  }
  switch(gameStatus){
    case SINGLE_PLAYER:
      if(isWaitingForAnswer){
        if((rx==89) || (rx==121)){                      // get player decision for playing another single mode game
          playAgain=1;
          __bic_SR_register_on_exit(LPM3_bits);         // Clear LPM3 bit from 0(SR)
        }
        if((rx==78) || (rx==110)){
          playAgain=0;
          __bic_SR_register_on_exit(LPM3_bits);         // Clear LPM3 bit from 0(SR)
        }
      }
      else{                                             // get first 3 digits of player answer as long as timeOut didn't occur
        if(!timeOut){
          if(rx>47 && rx<59 && ctr<3){
            sUserAns[ctr]=rx;
            ctr++;
         }
         if(rx==13){
            TACTL = MC_0 + TACLR;
            __bic_SR_register_on_exit(LPM3_bits);
         }
       }
      }
      break;
    case MULTIPLAYER:
        if(isWaitingForAnswer){
                if((rx==89) || (rx==121)){                      // get player decision for playing another game
                  playAgain=1;
                  __bic_SR_register_on_exit(LPM3_bits);         // Clear LPM3 bit from 0(SR)
                }
                if((rx==78) || (rx==110)){
                  playAgain=0;
                  __bic_SR_register_on_exit(LPM3_bits);         // Clear LPM3 bit from 0(SR)
                }
                isWaitingForAnswer=0;
                break;
         }


        if(rx>47 && rx<59 && ctr<3){                    // get first 3 digits of player answer
          sUserAns[ctr]=rx;
          ctr++;
        }
        if(rx==13){
          TACTL = MC_0 + TACLR;
          __bic_SR_register_on_exit(LPM3_bits);
        }
        timeOutLatency = 5;
      break;
    case MENU:
      if(gotName){                                      // get player decision for single/multiplier game mode
        if(rx == '1') {
          __bic_SR_register_on_exit(LPM3_bits);
          SinglePlayer();
          TACTL = MC_0 + TACLR;
          __bic_SR_register_on_exit(LPM3_bits);
          return;
        }
        if(rx == '2') {
          __bic_SR_register_on_exit(LPM3_bits);
          TwoPlayer();
          TACTL = MC_0 + TACLR;
          __bic_SR_register_on_exit(LPM3_bits);
          WDTCTL = WDTPW + ~WDTHOLD;
          return;
        }
      }
      else{                                             // get player name
        if(rx == 13){
          for (; ctr < 5; ctr++)
            usrName[ctr]=' ';
          gotName=1;
          ctr=0;
          __bic_SR_register_on_exit(LPM3_bits);
          return;
        }
        if(ctr < 5){
          usrName[ctr]=rx;
          ctr++;
        }
       else{
         gotName=1;
         __bic_SR_register_on_exit(LPM3_bits);
         return;
       }
     }
     break;
   case CHOOSE_DIFF:
     if(isWaitingForAnswer){
       switch(rx-48){                                   // get player decision for game difficulty level
            case EASY:
                timeOutLatency=4;
                gameGlobaldiff=1;
                __bic_SR_register_on_exit(LPM3_bits);
                break;
            case MEDIUM:
                gameGlobaldiff=2;
                timeOutLatency=5;
                __bic_SR_register_on_exit(LPM3_bits);
                break;
            case HARD:
                gameGlobaldiff=3;
                timeOutLatency=4;
                __bic_SR_register_on_exit(LPM3_bits);
                break;
       }
     }
     break;
   }
}

void MCU_Init()
{
  BCSCTL1 = CALBC1_8MHZ;                    // Set DCO to 8Mhz
  DCOCTL = CALDCO_8MHZ;
  P1DIR |= 0x03;                            // set direction of LEDs
  P1OUT = 0x00;
  P3SEL |= 0x30;                            // P3.4,5 = USCI_A0 TXD/RXD -  Port Select  - universal serial communication interface  UART MODE
  UCA0CTL1 = UCSSEL_2;                      // select the SMCLK for Low power mode
  UCA0BR0 = 0x41;                           // 9600 from 8Mhz //USCI_A0 Baud rate control register 0
  UCA0BR1 = 0x3;
  UCA0MCTL = UCBRS_2;
  UCA0CTL1 &= ~UCSWRST;                     // **Initialize USCI state machine**

  IE2 |= UCA0RXIE;                          // Enable USCI_A0 RX interrupt
  __enable_interrupt();
}

/* TXString:
  * Print a string to screen
*/
void TXString( char* string, int length ){
  int pointer;
  for( pointer = 0; pointer < length; pointer++){
    volatile int i;
    UCA0TXBUF = string[pointer];
    while (!(IFG2&UCA0TXIFG));              // USCI_A0 TX buffer ready?
  }
}

// wireless message handler
static uint8_t sCB(linkID_t lid)
{
  if (lid == linkID)
  {
    sPeerFrameSem++;
    return 0;
  }

  return 1;
}

/* InitTimerA
*/
void InitTimerA(int seconds){
  BCSCTL3 |= LFXT1S_2;                      // LFXT1 = VLO
  TACCTL0 = CCIE;                           // TACCR0 interrupt enabled
  TACCR0 = 12000*seconds;                   // ~1 second
  TACTL |= TASSEL_1 + MC_1;                 // ACLK, upmode
}

/* SinglePlayer:
  * Running the game in single player mode.
  * There is an option to continue playing in this mode as long as the player choose to play another game.
  * Doesn't connect to another player and there is no WI-FI use
*/
void SinglePlayer(){
  char gameOver=1,score=0;
  gameStatus=CHOOSE_DIFF;

  while (1){
    if(gameOver){                                       // uses for new game initializations and for difficulty selection
      gameStatus=CHOOSE_DIFF;
      resetGame();
      gameStatus=SINGLE_PLAYER;
      gameOver=0;
    }
    NewRound();                                         // running one round iteration
    if(myScore==10){                                    // check if game reached to his end
      gameOver=1;
      isWaitingForAnswer=1;
      TXString("\r\n\r\n\r\n\r\n\r\n\r\n\r\nYou earned 10 points!\r\n\r\nYou Won!\r\n\r\nWould you like to try again? [Y/N]\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n",105);
      __bis_SR_register(LPM3_bits+GIE);
      if(!playAgain)return;
    }
    else if(myScore==0){
      gameOver=1;
      isWaitingForAnswer=1;
      TXString("\r\n\r\n\r\n\r\n\r\n\r\nToo bad...\r\n\r\nYou lose the game!\r\n\r\nWould you like to try again? [Y/N]\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n",103);
      __bis_SR_register(LPM3_bits+GIE);
      if(!playAgain)return;
    }
    else{                                               // in a case the game is not over
      TXString("\r\nYour score is: ",17);
      score=myScore+48;
      TXString(&score,1);
      TXString("\r\n- - - - - - - - - - - - - - - - -\r\n",37);
    }
  }
}

/* resetGame:
  * Reset global variables and stop timer activity.
*/
void resetGame(){
  TACTL = MC_0 + TACLR;  // Stop timer + reset
  isWaitingForAnswer=0;
  myScore=5;
  ctr=0;
  timeOut=0;
  playAgain=0;
  rxmsg[0]=0;
  rxmsg[1]=0;
  rxmsg[2]=0;
  rxmsg[3]=0;
  rxmsg[4]=0;
  rxmsg[5]=0;
  rxmsg[6]=0;
  sUserAns[0]=0;
  sUserAns[1]=0;
  sUserAns[2]=0;
  P1OUT = 0x00;
  if(gameStatus == CHOOSE_DIFF){
      TXString("\r\nPlease choose your game difficulty:\r\n1) Easy - 8 seconds\r\n2) Medium - 6 seconds\r\n3) Hard - 4 seconds\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n",122);
      isWaitingForAnswer=1;
      __bis_SR_register(LPM3_bits+GIE);
      isWaitingForAnswer=0;
  }
}

/* NewRound:
  * Running one round of game and update score.
  * Set timer according to the chosen difficulty in the beginning of the game.
*/
void NewRound(){
  char num1, num2,num3,num4, userAns = 0;
  char toPrint[9], toPrintHard[20], power10[3] = {1,10,100};
  volatile char mathOperation = 0;

  num1 = (rand()%10)+1 , num2 = (rand()%10)+1;
  num3 = (rand()%10)+1 , num4 = (rand()%10)+1;

  if(gameGlobaldiff==1){
    mathOperation = num1+num2;
  }
  else if(gameGlobaldiff == 2){
    mathOperation = num1* num2;
  }
  else if(gameGlobaldiff == 3){
    mathOperation = (num1 * num2) + (num3 * num4);
  }

  // print question to screen
  TXString("\r\n\r\nRound has started\r\n\r\nEnter your answer and press 'Enter':\r\n- - - - - - - - - - - - - - -\r\n", 94);
  if(gameGlobaldiff == 3){
    toPrintHard[0] = num1/10 + 48;
    toPrintHard[1] = num1%10 + 48;
    toPrintHard[2] = ' ';
    toPrintHard[3] = '*';
    toPrintHard[4] = ' ';
    toPrintHard[5] = num2/10 + 48;
    toPrintHard[6] = num2%10 + 48;
    toPrintHard[7] = ' ';
    toPrintHard[8] = '+';
    toPrintHard[9] = ' ';
    toPrintHard[10] = num3/10 + 48;
    toPrintHard[11] = num3%10 + 48;
    toPrintHard[12] = ' ';
    toPrintHard[13] = '*';
    toPrintHard[14] = ' ';
    toPrintHard[15] = num4/10 + 48;
    toPrintHard[16] = num4%10 + 48;
    toPrintHard[17] = ' ';
    toPrintHard[18] = '=';
    toPrintHard[19] = '?';
    TXString(toPrintHard, 20);
  }
  else{
    toPrint[0] = num1/10 + 48;
    toPrint[1] = num1%10 + 48;
    toPrint[2] = ' ';
    if(gameGlobaldiff==1){
        toPrint[3] = '+';
    }
    else if(gameGlobaldiff == 2){
     toPrint[3] = '*';
    }
    toPrint[4] = ' ';
    toPrint[5] = num2/10 + 48;
    toPrint[6] = num2%10 + 48;
    toPrint[7] = '=';
    toPrint[8] = '?';
    TXString(toPrint, 9);
  }
  InitTimerA(timeOutLatency);                           // set timer
  __bis_SR_register(LPM3_bits+GIE);                     // get player answer or wait for time to expire

  if(!timeOut){                                         // time didn't expired -> update player score
    userAns += (sUserAns[0]-48)*power10[ctr-1];         // convert ASCII answer to integer
    ctr--;
    if(ctr>0){
      userAns += (sUserAns[1]-48)*power10[ctr-1];
      ctr--;
    }
    if(ctr>0){
      userAns += (sUserAns[2]-48)*power10[ctr-1];
      ctr--;
    }
    if(userAns == mathOperation){                             // update score
      myScore++;
      TXString("\r\n\r\nGood work!!!\r\n",18);
    }
    else{
      myScore--;
      TXString("\r\n\r\nWrong answer!!!\r\n",21);
    }
  }
  else                                                  // time expire
    TXString("\r\n\r\nTime expired\r\n",18);

  sUserAns[0] = 0;
  sUserAns[1] = 0;
  sUserAns[2] = 0;
  ctr = 0;
  timeOut=0;
  P1OUT |= 0x01;
}

/* TwoPlayer:
  * Running one game in multiplayer mode.
  * Client function
  * The program will return to it's start and ask for player's name.
*/
void TwoPlayer()
{

    char score=0;
    uint8_t oterusr[5];
    uint8_t len;
    uint8_t num[5];

    gameStatus = MULTIPLAYER;

    TXString("\r\nConnecting to Host...",23);

    SMPL_Init(sCB);

    /* turn on RX. default is RX off. */
    SMPL_Ioctl( IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_RXON, 0);

    while (SMPL_SUCCESS != SMPL_Link(&linkID)); // connect to host

    while(!sPeerFrameSem);
    SMPL_Receive(linkID, otherusrName, &len);  // get host player name

    BSP_ENTER_CRITICAL_SECTION(intState);
    sPeerFrameSem--;
    BSP_EXIT_CRITICAL_SECTION(intState);

    SMPL_Send(linkID, (uint8_t*)usrName, 5);  // send guest player name

    while(!sPeerFrameSem);
    SMPL_Receive(linkID, num, &len);  // get difficulty from host player

    BSP_ENTER_CRITICAL_SECTION(intState);
    sPeerFrameSem--;
    BSP_EXIT_CRITICAL_SECTION(intState);

    gameGlobaldiff=num[0]-48;

    gameStatus = MULTIPLAYER;
    resetGame();

  while(1)
  {
    TACTL = MC_0 + TACLR;
    NewRound();

    while(!sPeerFrameSem);
    SMPL_Receive(linkID, rxmsg, &len); // get host player score
    BSP_ENTER_CRITICAL_SECTION(intState);
    sPeerFrameSem--;
    BSP_EXIT_CRITICAL_SECTION(intState);

    num[0]= myScore;
    SMPL_Send(linkID, num, 1);  // send guest player score

    if(myScore == 10 || rxmsg[0]== 10 || myScore == 0 || rxmsg[0]== 0)  // game over
    {
        TXString("\r\nGame Over!",12);
        TXString("\r\nThe winner is:",16);

        if((myScore == 10 && rxmsg[0] != 10) || (rxmsg[0]== 0 && myScore != 0))
            TXString((char *)(usrName),5);
        else if((myScore != 10 && rxmsg[0] == 10) || (rxmsg[0]!= 0 && myScore == 0) )
            TXString((char *)(otherusrName),5);
        else if((myScore == 10 && rxmsg[0] == 10) || (myScore == 0 && rxmsg[0] == 0)){
            TXString("\r\nTie!!",7);
        }


        TXString("\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n",20);

        TXString("\r\nPlay again? Y/N",17);
        isWaitingForAnswer=1;
        myScore = 5;
        __bis_SR_register(LPM3_bits+GIE);
        if(!playAgain)return;

    }
    else    // game not over - round over
    {
        TXString("\r\n\r\nRound Results:\r\n\r\n",22);
        TXString(usrName,5);
        TXString(" score - ",9);
        score=myScore+48;
        TXString(&score,1);
        TXString(" | ",3);
        TXString(otherusrName,5);
        TXString(" score - ",9);
        score=rxmsg[0]+48;
        TXString(&score,1);
        TXString("   ",3);
    }

  }




}
