/**
 * 5 semestre - Eng. da Computa��o - Insper
 * Rafael Corsi - rafael.corsi@insper.edu.br
 *
 * Projeto 0 para a placa SAME70-XPLD
 *
 * Objetivo :
 *  - Introduzir ASF e HAL
 *  - Configuracao de clock
 *  - Configuracao pino In/Out
 *
 * Material :
 *  - Kit: ATMEL SAME70-XPLD - ARM CORTEX M7
 */

/************************************************************************/
/* includes                                                             */
/************************************************************************/

#include "asf.h"

/************************************************************************/
/* defines                                                              */
/************************************************************************/



#define LED_PIO           PIOC                 
#define LED_PIO_ID        ID_PIOC              
#define LED_PIO_IDX       8                    
#define LED_PIO_IDX_MASK  (1 << LED_PIO_IDX)   

#define LED1_PIO           PIOC
#define LED1_PIO_ID        ID_PIOC
#define LED1_PIO_IDX       19
#define LED1_PIO_IDX_MASK  (1 << LED1_PIO_IDX)

#define LED2_PIO           PIOD
#define LED2_PIO_ID        ID_PIOD
#define LED2_PIO_IDX       26
#define LED2_PIO_IDX_MASK  (1 << LED2_PIO_IDX)

#define LED3_PIO           PIOD
#define LED3_PIO_ID        ID_PIOD
#define LED3_PIO_IDX       11
#define LED3_PIO_IDX_MASK  (1 << LED3_PIO_IDX)

#define BUT_PIO				PIOA
#define BUT_PIO_ID			ID_PIOA
#define BUT_PIO_IDX			11
#define BUT_PIO_IDX_MASK (1u << BUT_PIO_IDX)


#define BUT1_PIO				PIOA
#define BUT1_PIO_ID				ID_PIOA
#define BUT1_PIO_IDX			2
#define BUT1_PIO_IDX_MASK (1u << BUT1_PIO_IDX)

#define BUT3_PIO				PIOC
#define BUT3_PIO_ID				ID_PIOC
#define BUT3_PIO_IDX			13
#define BUT3_PIO_IDX_MASK (1u << BUT3_PIO_IDX)

#define BUT2_PIO				PIOD
#define BUT2_PIO_ID				ID_PIOD
#define BUT2_PIO_IDX			30
#define BUT2_PIO_IDX_MASK (1u << BUT2_PIO_IDX)


/************************************************************************/
/* constants                                                            */
/************************************************************************/

/************************************************************************/
/* variaveis globais                                                    */
/************************************************************************/
volatile Bool f_rtt_alarme = false;

/************************************************************************/
/* prototypes                                                           */
/************************************************************************/
void pin_toggle(Pio *pio, uint32_t mask);
void io_init(void);
static void RTT_init(uint16_t pllPreScale, uint32_t IrqNPulses);

/************************************************************************/
/* interrupcoes                                                         */
/************************************************************************/

void RTT_Handler(void)
{
  uint32_t ul_status;

  /* Get RTT status */
  ul_status = rtt_get_status(RTT);

  /* IRQ due to Time has changed */
  if ((ul_status & RTT_SR_RTTINC) == RTT_SR_RTTINC) {  }

  /* IRQ due to Alarm */
  if ((ul_status & RTT_SR_ALMS) == RTT_SR_ALMS) {
      pin_toggle(LED_PIO, LED_IDX_MASK);    // BLINK Led
      f_rtt_alarme = true;                  // flag RTT alarme
   }  
}

/************************************************************************/
/* funcoes                                                              */
/************************************************************************/

void pin_toggle(Pio *pio, uint32_t mask){
  if(pio_get_output_data_status(pio, mask))
    pio_clear(pio, mask);
  else
    pio_set(pio,mask);
}

void io_init(void){
  /* led */
  pmc_enable_periph_clk(LED_PIO_ID);
  pio_configure(LED_PIO, PIO_OUTPUT_0, LED_IDX_MASK, PIO_DEFAULT);
}

static float get_time_rtt(){
  uint ul_previous_time = rtt_read_timer_value(RTT); 
}

static void RTT_init(uint16_t pllPreScale, uint32_t IrqNPulses)
{
  uint32_t ul_previous_time;

  /* Configure RTT for a 1 second tick interrupt */
  rtt_sel_source(RTT, false);
  rtt_init(RTT, pllPreScale);
  
  ul_previous_time = rtt_read_timer_value(RTT);
  while (ul_previous_time == rtt_read_timer_value(RTT));
  
  rtt_write_alarm_time(RTT, IrqNPulses+ul_previous_time);

  /* Enable RTT interrupt */
  NVIC_DisableIRQ(RTT_IRQn);
  NVIC_ClearPendingIRQ(RTT_IRQn);
  NVIC_SetPriority(RTT_IRQn, 0);
  NVIC_EnableIRQ(RTT_IRQn);
  rtt_enable_interrupt(RTT, RTT_MR_ALMIEN);
}

/************************************************************************/
/* Main                                                                 */
/************************************************************************/

// Funcao principal chamada na inicalizacao do uC.
int main(void){
  
	// Desliga watchdog
	WDT->WDT_MR = WDT_MR_WDDIS;
  
  sysclk_init();
  io_init();
  
  // Inicializa RTT com IRQ no alarme.
  f_rtt_alarme = true;
    
  // super loop
  // aplicacoes embarcadas n�o devem sair do while(1).
  while (1){
    if (f_rtt_alarme){
      
      /*
       * O clock base do RTT � 32678Hz
       * Para gerar outra base de tempo � necess�rio
       * usar o PLL pre scale, que divide o clock base.
       *
       * Nesse exemplo, estamos operando com um clock base
       * de pllPreScale = 32768/32768/2 = 2Hz
       *
       * Quanto maior a frequ�ncia maior a resolu��o, por�m
       * menor o tempo m�ximo que conseguimos contar.
       *
       * Podemos configurar uma IRQ para acontecer quando 
       * o contador do RTT atingir um determinado valor
       * aqui usamos o irqRTTvalue para isso.
       * 
       * Nesse exemplo o irqRTTvalue = 8, causando uma
       * interrup��o a cada 2 segundos (lembre que usamos o 
       * pllPreScale, cada incremento do RTT leva 500ms (2Hz).
       */
      uint16_t pllPreScale = (int) (((float) 32768) / 2.0);
      uint32_t irqRTTvalue  = 4;
      
      // reinicia RTT para gerar um novo IRQ
      RTT_init(pllPreScale, irqRTTvalue);         
      
     /*
      * caso queira ler o valor atual do RTT, basta usar a funcao
      *   rtt_read_timer_value()
      */
      
      /*
       * CLEAR FLAG
       */
      f_rtt_alarme = false;
    }
  }  
  return 0;
}
