/*
 * NanoEvery_PCM
 * 
 * based on code by david mellis / michael smith
 * https://github.com/damellis/PCM
 * 
 * Clemens Wegener <clemens@chair.audio>
 * 
 * The audio data needs to be unsigned, 8-bit, 8000 Hz, and small enough
 * to fit in flash. 10000-13000 samples is about the limit.
 *
 * sounddata.h should look like this:
 *     const int sounddata_length=10000;
 *     const unsigned char sounddata_data[] PROGMEM = { ..... };
 *
 * You can use wav2c from GBA CSS:
 *     http://thieumsweb.free.fr/english/gbacss.html
 * Then add "PROGMEM" in the right place. I hacked it up to dump the samples
 * as unsigned rather than signed, but it shouldn't matter.
 *
 * http://musicthing.blogspot.com/2005/05/tiny-music-makers-pt-4-mac-startup.html
 * mplayer -ao pcm macstartup.mp3
 * sox audiodump.wav -v 1.32 -c 1 -r 8000 -u -1 macstartup-8000.wav
 * sox macstartup-8000.wav macstartup-cut.wav trim 0 10000s
 * wav2c macstartup-cut.wav sounddata.h sounddata
 *
 * (starfox) nb. under sox 12.18 (distributed in CentOS 5), i needed to run
 * the following command to convert my wav file to the appropriate format:
 * sox audiodump.wav -c 1 -r 8000 -u -b macstartup-8000.wav
 */

#include "NanoEvery_PCM.h"
#include "Arduino.h"

#ifdef MILLIS_USE_TIMERA0
  #error "This sketch takes over TCA0 - please use a different timer for millis"
#endif

#define PERIOD_EXAMPLE_VALUE    (1000)

unsigned char const *sounddata_data=0;
int sounddata_length=0;

volatile uint16_t sample = 0;
uint8_t speakerPin = 3;

void CLOCK_init (void)
{
    /* Enable writing to protected register */
    //CPU_CCP = CCP_IOREG_gc;
    /* Enable Prescaler and set Prescaler Division to 64 */
    //CLKCTRL.MCLKCTRLB = CLKCTRL_PDIV_64X_gc | CLKCTRL_PEN_bm;
    
    /* Enable writing to protected register */
    CPU_CCP = CCP_IOREG_gc;
    /* Select 32KHz Internal Ultra Low Power Oscillator (OSCULP32K) */
    CLKCTRL.MCLKCTRLA = CLKCTRL_CLKSEL_OSC20M_gc;
    
    /* Wait for system oscillator changing to finish */
    while (CLKCTRL.MCLKSTATUS & CLKCTRL_SOSC_bm)
    {
        ;
    }
}

void PWM_init (void)
{
  pinMode(speakerPin, OUTPUT);   //Port F, Pin 5 = Arduino ~D3       
  /* set the alternate pin mux */
  PORTMUX.TCBROUTEA |= PORTMUX_TCB1_bm;
  TCB1.CCMPL = 255; /* PWM Period*/
  TCB1.CCMPH = 128;   /* PWM Compare*/
  TCB1.CTRLB = 1 << TCB_ASYNC_bp      /* Asynchronous Enable: enabled */
               | 1 << TCB_CCMPEN_bp   /* Pin Output Enable: enabled */
               | 0 << TCB_CCMPINIT_bp /* Pin Initial State: disabled */
               | TCB_CNTMODE_PWM8_gc; /* 8-bit PWM */

  TCB1.CTRLA = TCB_CLKSEL_CLKDIV2_gc  /* CLK_PER (No Prescaling) */
               | 1 << TCB_ENABLE_bp   /* Enable: enabled */
               | 0 << TCB_RUNSTDBY_bp /* Run Standby: disabled */
               | 1 << TCB_SYNCUPD_bp; /* Synchronize Update: enabled */     
  TCB1.CTRLA |= TCB_ENABLE_bm;

}

/*Using default clock 3.33MHz */
void TCA0_init(void);
void PORT_init(void);

void TCA0_init(void)
{
  /* enable overflow interrupt */
  TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm;
  /* set Normal mode */
  TCA0.SINGLE.CTRLB = TCA_SINGLE_WGMODE_NORMAL_gc;
  /* disable event counting */
  TCA0.SINGLE.EVCTRL &= ~(TCA_SINGLE_CNTEI_bm);
  /* set the period */
  TCA0.SINGLE.PER = PERIOD_EXAMPLE_VALUE;

  TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV2_gc /* set clock source (sys_clk/256) */
                    | TCA_SINGLE_ENABLE_bm;       /* start timer */
}
void PORT_init(void)
{
  /* set pin 0 of PORT A as output */
  PORTA.DIR |= PIN0_bm; //D2
}
ISR(TCA0_OVF_vect)
{
  /* Toggle PIN 0 of PORT A */
  PORTA.OUTTGL = PIN0_bm;
  TCB1.CCMPH = pgm_read_byte(&sounddata_data[sample]);
  ++sample;
  /* The interrupt flag has to be cleared manually */
  TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
}

void startPlayback(unsigned char const *data, int length)
{
  sounddata_data = data;
  sounddata_length = length;

  CLOCK_init();
  PWM_init ();
  PORT_init();
  TCA0_init();
    /* enable global interrupts */
  sei();
}

void stopPlayback()
{
  // Disable playback per-sample interrupt.
  TCA0.SINGLE.INTCTRL &= ~TCA_SINGLE_OVF_bm;

   /* stop timer */
  //TCA0.SINGLE.CTRLA &= ~TCA_SINGLE_ENABLE_bm;      
  
  // Disable the PWM timer.
  TCB1.CTRLA &= ~TCB_ENABLE_bm;
  
  digitalWrite(speakerPin, LOW);
  // reset sample pointer
  sample = 0;
}
