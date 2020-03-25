#include <asf.h>

#include "gfx_mono_ug_2832hsweg04.h"
#include "gfx_mono_text.h"
#include "sysfont.h"


#define YEAR        2018
#define MOUNT       3
#define DAY         19
#define WEEK        12
#define HOUR        15
#define MINUTE      45
#define SECOND      0

/**
* LEDs
*/

#define LED_PIO_ID	   ID_PIOC
#define LED_PIO        PIOC
#define LED_PIN		   8
#define LED_PIN_MASK   (1<<LED_PIN)

#define LED1_PIO_ID	   ID_PIOA
#define LED1_PIO        PIOA
#define LED1_PIN		   0
#define LED1_PIN_MASK   (1<<LED1_PIN)

#define LED2_PIO_ID        ID_PIOC
#define LED2_PIO           PIOC
#define LED2_PIN       30
#define LED2_PIN_MASK  (1 << LED2_PIN)

volatile uint8_t flag_led1 = 1;
volatile uint8_t flag_led0 = 1;
volatile Bool f_rtt_alarme = false;


void TC_init(Tc * TC, int ID_TC, int TC_CHANNEL, int freq);
void RTC_init(void);
void pin_toggle(Pio *pio, uint32_t mask);

void TC1_Handler(void){
	volatile uint32_t ul_dummy;

	/****************************************************************
	* Devemos indicar ao TC que a interrup��o foi satisfeita.
	******************************************************************/
	ul_dummy = tc_get_status(TC0, 1);

	/* Avoid compiler warning */
	UNUSED(ul_dummy);

	/** Muda o estado do LED */
	if(flag_led1)
		pin_toggle(LED1_PIO, LED1_PIN_MASK);
}

void pin_toggle(Pio *pio, uint32_t mask){
	if(pio_get_output_data_status(pio, mask))
	pio_clear(pio, mask);
	else
	pio_set(pio,mask);
}

void LED_init(int estado){
	pmc_enable_periph_clk(LED1_PIO_ID);
	pio_set_output(LED1_PIO, LED1_PIN_MASK, estado, 0, 0 );
	pmc_enable_periph_clk(LED2_PIO_ID);
	pio_set_output(LED2_PIO, LED2_PIN_MASK, estado, 0, 0 );
	pmc_enable_periph_clk(LED_PIO_ID);
	pio_set_output(LED_PIO, LED_PIN_MASK, estado, 0, 0 );
};
void TC_init(Tc * TC, int ID_TC, int TC_CHANNEL, int freq){
	uint32_t ul_div;
	uint32_t ul_tcclks;
	uint32_t ul_sysclk = sysclk_get_cpu_hz();

	uint32_t channel = 1;

	/* Configura o PMC */
	/* O TimerCounter � meio confuso
	o uC possui 3 TCs, cada TC possui 3 canais
	TC0 : ID_TC0, ID_TC1, ID_TC2
	TC1 : ID_TC3, ID_TC4, ID_TC5
	TC2 : ID_TC6, ID_TC7, ID_TC8
	*/
	pmc_enable_periph_clk(ID_TC);

	/** Configura o TC para operar em  4Mhz e interrup�c�o no RC compare */
	tc_find_mck_divisor(freq, ul_sysclk, &ul_div, &ul_tcclks, ul_sysclk);
	tc_init(TC, TC_CHANNEL, ul_tcclks | TC_CMR_CPCTRG);
	tc_write_rc(TC, TC_CHANNEL, (ul_sysclk / ul_div) / freq);

	/* Configura e ativa interrup�c�o no TC canal 0 */
	/* Interrup��o no C */
	NVIC_EnableIRQ((IRQn_Type) ID_TC);
	tc_enable_interrupt(TC, TC_CHANNEL, TC_IER_CPCS);

	/* Inicializa o canal 0 do TC */
	tc_start(TC, TC_CHANNEL);
}

void RTT_Handler(void){
	uint32_t ul_status;

	/* Get RTT status */
	ul_status = rtt_get_status(RTT);

	/* IRQ due to Time has changed */
	if ((ul_status & RTT_SR_RTTINC) == RTT_SR_RTTINC) {  }

	/* IRQ due to Alarm */
	if ((ul_status & RTT_SR_ALMS) == RTT_SR_ALMS) {
		pin_toggle(LED2_PIO, LED2_PIN_MASK);    // BLINK Led
		f_rtt_alarme = true;                  // flag RTT alarme
	}
}

static void RTT_init(uint16_t pllPreScale, uint32_t IrqNPulses){
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

void RTC_Handler(void){
	uint32_t ul_status = rtc_get_status(RTC);

	/*
	*  Verifica por qual motivo entrou
	*  na interrupcao, se foi por segundo
	*  ou Alarm
	*/
	if ((ul_status & RTC_SR_SEC) == RTC_SR_SEC) {
		rtc_clear_status(RTC, RTC_SCCR_SECCLR);
	}
	
	/* Time or date alarm */
	if ((ul_status & RTC_SR_ALARM) == RTC_SR_ALARM) {
			rtc_clear_status(RTC, RTC_SCCR_ALRCLR);

			flag_led0 = 1;
	}
	
	rtc_clear_status(RTC, RTC_SCCR_ACKCLR);
	rtc_clear_status(RTC, RTC_SCCR_TIMCLR);
	rtc_clear_status(RTC, RTC_SCCR_CALCLR);
	rtc_clear_status(RTC, RTC_SCCR_TDERRCLR);
	
}

void RTC_init(){
	/* Configura o PMC */
	pmc_enable_periph_clk(ID_RTC);

	/* Default RTC configuration, 24-hour mode */
	rtc_set_hour_mode(RTC, 0);

	/* Configura data e hora manualmente */
	rtc_set_date(RTC, YEAR, MOUNT, DAY, WEEK);
	rtc_set_time(RTC, HOUR, MINUTE, SECOND);

	/* Configure RTC interrupts */
	NVIC_DisableIRQ(RTC_IRQn);
	NVIC_ClearPendingIRQ(RTC_IRQn);
	NVIC_SetPriority(RTC_IRQn, 0);
	NVIC_EnableIRQ(RTC_IRQn);

	/* Ativa interrupcao via alarme */
	rtc_enable_interrupt(RTC,  RTC_IER_ALREN);

}

void pisca_led(int n, int t){
	for (int i=0;i<n;i++){
		pio_clear(LED_PIO, LED_PIN);
		delay_ms(t);
		pio_set(LED_PIO, LED_PIN);
		delay_ms(t);
	}
}


int main (void){
	board_init();
	sysclk_init();
	delay_init();
	
	/* Configura Leds */
	LED_init(1);
	
	/** Configura timer TC0, canal 1 */
	TC_init(TC0, ID_TC1, 1, 4);

  // Init OLED
	gfx_mono_ssd1306_init();
  
  // Escreve na tela um circulo e um texto
	gfx_mono_draw_filled_circle(20, 16, 16, GFX_PIXEL_SET, GFX_WHOLE);
  gfx_mono_draw_string("eita", 50,16, &sysfont);
  
  	RTC_init();

  	/* configura alarme do RTC */
  	rtc_set_date_alarm(RTC, 1, MOUNT, 1, DAY);
  	rtc_set_time_alarm(RTC, 1, HOUR, 1, MINUTE, 1, SECOND+10);
  	
  
  f_rtt_alarme = true;

  /* Insert application code here, after the board has been initialized. */
	while(1) {
		
		if(flag_led0){
			pisca_led(5, 200);
			flag_led0 = 0;
		}
		
		
	    if (f_rtt_alarme){
				
		  uint16_t pllPreScale = (int) (((float) 32768) / 2.0);
		  uint32_t irqRTTvalue  = 4;
		  
		  RTT_init(pllPreScale, irqRTTvalue);  
      
			f_rtt_alarme = false;
		}

	}
}
