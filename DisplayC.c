/*
 * Por: Wilton Lacerda Silva
 *    Comunicação serial com I2C
 *  
 * Uso da interface I2C para comunicação com o Display OLED
 * 
 * Estudo da biblioteca ssd1306 com PicoW na Placa BitDogLab.
 *  
 * Este programa escreve uma mensagem no display OLED.
 * 
 * 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "inc/ssd1306.h"
#include "inc/font.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "ws2812.pio.h"
#include "hardware/timer.h"

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

#define IS_RGBW false
#define NUM_PIXELS 25
#define WS2812_PIN 7
#define tempo 100
#define GREEN 11
#define BLUE 12
#define RED 13
#define BUTTON_A 5
#define BUTTON_B 6
#define FRAME_NUMBER 10 //De 0 a 9

char c,mensagem[16]={0};
unsigned int alarmes_ativos=0;//Quando a interrupção for acionada múltiplas vezes, só
//irá retirar a mensagem a partir da última

// Variável global para armazenar a cor (Entre 0 e 255 para intensidade)
uint8_t led_r = 5; // Intensidade do vermelho
uint8_t led_g = 0; // Intensidade do verde
uint8_t led_b = 0; // Intensidade do azul

static volatile uint32_t last_time = 0;//Usado para o debounce
unsigned short int escolhido=0;//Determina o numero que sera exibido na matriz de leds
ssd1306_t ssd; // Inicializa a estrutura do display

// Buffer para armazenar quais LEDs estão ligados matriz 5x5
 bool led_buffer[FRAME_NUMBER][NUM_PIXELS] ={
    {0,1,1,1,0,
     0,1,0,1,0,
     0,1,0,1,0,
     0,1,0,1,0,
     0,1,1,1,0
    },
    {0,0,1,0,0,
     0,1,1,0,0,
     0,0,1,0,0,
     0,0,1,0,0,
     0,0,1,0,0
    },
    {0,1,1,1,0,
     0,0,0,1,0,
     0,1,1,1,0,
     0,1,0,0,0,
     0,1,1,1,0
    },
    {0,1,1,1,0,
     0,0,0,1,0,
     0,1,1,1,0,
     0,0,0,1,0,
     0,1,1,1,0
    },
    {0,1,0,1,0,
     0,1,0,1,0,
     0,1,1,1,0,
     0,0,0,1,0,
     0,0,0,1,0
    },
    {0,1,1,1,0,
     0,1,0,0,0,
     0,1,1,1,0,
     0,0,0,1,0,
     0,1,1,1,0
    },
    {0,1,1,1,0,
     0,1,0,0,0,
     0,1,1,1,0,
     0,1,0,1,0,
     0,1,1,1,0
    },
    {0,1,1,1,0,
     0,0,0,1,0,
     0,0,0,1,0,
     0,0,0,1,0,
     0,0,0,1,0
    },
    {0,1,1,1,0,
     0,1,0,1,0,
     0,1,1,1,0,
     0,1,0,1,0,
     0,1,1,1,0
    },
    {0,1,1,1,0,
     0,1,0,1,0,
     0,1,1,1,0,
     0,0,0,1,0,
     0,1,1,1,0
    }
};

//Corrige o índice para que o LED certo seja acendido
uint correcao_index(int index){
     //Caso esteja numa linha ímpar
     if((index>=5 && index<10) || (index>=15 && index<20))
     return index<10 ? index+10:index-10;
     else
     return NUM_PIXELS-index-1;
    }

//Aparentemente essa função só funciona com vetor de 1 dimensão
static inline void put_pixel(uint32_t pixel_grb)
{
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

//Apaga toda a matriz de LEDs
void limpar_matriz(){
  uint32_t color = urgb_u32(0, 0, 0);
  for(int i=0;i<NUM_PIXELS;i++)
    put_pixel(color);
}

//Liga os LEDs na matriz que estiverem com '1'
void set_one_led(uint8_t r, uint8_t g, uint8_t b)
{
    // Define a cor com base nos parâmetros fornecidos
    uint32_t color = urgb_u32(r, g, b);

    // Define todos os LEDs com a cor especificada
    for (int i = 0; i < NUM_PIXELS; i++)
    {
        if (led_buffer[escolhido][correcao_index(i)])
        {
            put_pixel(color); // Liga o LED com um no buffer
        }
        else
        {
            put_pixel(0);  // Desliga os LEDs com zero no buffer
        }
    }
}

int64_t retirar_mensagem(){
  if(alarmes_ativos == 1)//Garante que só retira a mensagem 2 segundos após o último pressionamento
  {                      //do botão
  ssd1306_fill(&ssd, 0);
  ssd1306_draw_char(&ssd,c,58,32);
  ssd1306_rect(&ssd, 3, 3, 122, 58, 1, 0);
  ssd1306_send_data(&ssd);
  mensagem[0]='\0';
  }
  alarmes_ativos--;
  return 0;
}

//Rotina de interrupção, o botão A decrementa e o botão B incrementa
void interrupt(uint gpio, uint32_t events)
{
    // Obtem o tempo atual em microssegundos
    uint32_t current_time = to_us_since_boot(get_absolute_time());
    // Verifica se passou tempo suficiente desde o último evento
    if (current_time - last_time > 300000) // 300 ms de debouncing
    {
      bool ligou;
      last_time=current_time;
      ssd1306_fill(&ssd, 0);
      ssd1306_draw_char(&ssd,c,58,32);
      if(gpio==BUTTON_B){//Verifica se foi o botao B (incremento) ou o botao A (decremento)
        ligou=gpio_get(BLUE);
        gpio_put(BLUE,!gpio_get(BLUE));
        if(!ligou){
         puts("LED azul ligado!");
         strcpy(mensagem,"azul LIGADO");
        }
        else{
         puts("LED azul desligado!");
         strcpy(mensagem,"azul DESLIGADO");
        }
      }else{
        ligou=gpio_get(GREEN);
        gpio_put(GREEN,!gpio_get(GREEN));
        if(!ligou){
         puts("LED verde ligado!");
         strcpy(mensagem,"verde LIGADO");
        }
        else{
         puts("LED verde desligado!");
         strcpy(mensagem,"verde DESLIGADO");
        }
      }
      ssd1306_draw_string(&ssd, mensagem, 5, 10);
      ssd1306_rect(&ssd, 3, 3, 122, 58, 1, 0);
      ssd1306_send_data(&ssd);
      if(alarmes_ativos <= 10)//Limite de 10 alarmes ativos ao mesmo tempo
      {
      alarmes_ativos++;
      add_alarm_in_ms(2000, retirar_mensagem, NULL, false);
      }
    }
}

int main()
{
  // I2C Initialisation. Using it at 400Khz.
  i2c_init(I2C_PORT, 400 * 1000);

  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // Set the GPIO pin function to I2C
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // Set the GPIO pin function to I2C
  gpio_pull_up(I2C_SDA); // Pull up the data line
  gpio_pull_up(I2C_SCL); // Pull up the clock line
  ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
  ssd1306_config(&ssd); // Configura o display
  ssd1306_send_data(&ssd); // Envia os dados para o display

  // Limpa o display. O display inicia com todos os pixels apagados.
  ssd1306_fill(&ssd, false);
  ssd1306_send_data(&ssd);

  PIO pio = pio0;
    int sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);

    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);

    stdio_init_all();

    gpio_init(RED);              // Inicializa o pino do LED
    gpio_set_dir(RED, GPIO_OUT); // Configura o pino como saida
    gpio_put(RED,0);

    gpio_init(BLUE);              
    gpio_set_dir(BLUE, GPIO_OUT);
    gpio_put(BLUE,0);

    gpio_init(GREEN);              
    gpio_set_dir(GREEN, GPIO_OUT);
    gpio_put(GREEN,0);
    
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN); // Configura o pino como entrada
    gpio_pull_up(BUTTON_A);          // Habilita o pull-up interno

    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN); // Configura o pino como entrada
    gpio_pull_up(BUTTON_B);

    //set_one_led(led_r, led_g, led_b);

    //Só é possivel ter uma unica função de interrupção
    gpio_set_irq_enabled_with_callback(BUTTON_B, GPIO_IRQ_EDGE_FALL, true, &interrupt);
    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &interrupt);

    limpar_matriz();//Apaga todos os LEDs da matriz
  //bool cor = true;
  while (true)
  {
    //cor = !cor;
   printf("Digite algum caractere\n\n");
   c=getchar();
   ssd1306_fill(&ssd, 1); // Limpa o display
   ssd1306_rect(&ssd, 3, 3, 122, 58, 0, 1); // Desenha um retângulo
   ssd1306_draw_char(&ssd,c,58,32);
   if(c >= '0' && c <= '9'){
    escolhido = c-'0';
    set_one_led(led_r,led_g,led_b);
   }
   else
   limpar_matriz();
   //Garante que a mensagem continue sendo exibida mesmo que um caractere seja digitado
   if(mensagem[0]) ssd1306_draw_string(&ssd, mensagem, 5, 10);
   ssd1306_send_data(&ssd); // Atualiza o display n
    sleep_ms(1000);
  }
}